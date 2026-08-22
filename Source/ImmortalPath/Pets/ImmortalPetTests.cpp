// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPetTypes.h"
#include "../Save/ImmortalPathSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "PaperFlipbook.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalPetCatalogTest,
	"ImmortalPath.Pets.CatalogAndPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalPetCatalogTest::RunTest(const FString& Parameters)
{
	const TArray<FName> PetIds = UImmortalPetLibrary::GetKnownPetIds();
	TestTrue(TEXT("At least two pets are available"), PetIds.Num() >= 2);
	TestTrue(TEXT("Fox remains in the stable catalog"),
		PetIds.Contains(TEXT("SpiritFox")));
	TestTrue(TEXT("Hound remains in the stable catalog"),
		PetIds.Contains(TEXT("SpiritHound")));

	TSet<uint8> AttackStyles;
	for (const FName PetId : PetIds)
	{
		FImmortalPetDefinition Definition;
		TestTrue(TEXT("Pet definition resolves"),
			UImmortalPetLibrary::GetPetDefinition(PetId, Definition));
		TestTrue(TEXT("Pet definition is valid"), Definition.IsValid());
		TestFalse(TEXT("Pet has a move flipbook path"),
			Definition.MoveFlipbook.IsNull());
		TestFalse(TEXT("Pet has an attack flipbook path"),
			Definition.AttackFlipbook.IsNull());
		TestFalse(TEXT("Pet has a hurt flipbook path"),
			Definition.HurtFlipbook.IsNull());
		TestFalse(TEXT("Pet has a death flipbook path"),
			Definition.DeathFlipbook.IsNull());
		TestNotNull(TEXT("Move asset loads as a PaperFlipbook"),
			Definition.MoveFlipbook.LoadSynchronous());
		TestNotNull(TEXT("Attack asset loads as a PaperFlipbook"),
			Definition.AttackFlipbook.LoadSynchronous());
		TestNotNull(TEXT("Hurt asset loads as a PaperFlipbook"),
			Definition.HurtFlipbook.LoadSynchronous());
		TestNotNull(TEXT("Death asset loads as a PaperFlipbook"),
			Definition.DeathFlipbook.LoadSynchronous());
		TestTrue(TEXT("TBH pet rests on the visible side of the player"),
			Definition.FollowOffsetX > 0.0f);
		TestTrue(TEXT("Supplied pet sheets use taskbar-scale presentation"),
			Definition.VisualScale >= 0.25f
				&& Definition.VisualScale <= 0.75f);
		AttackStyles.Add(static_cast<uint8>(Definition.AttackStyle));
	}
	TestEqual(TEXT("Initial pets cover melee and ranged roles"),
		AttackStyles.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalPetStateTest,
	"ImmortalPath.Pets.DefaultStateNormalizationAndSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalPetStateTest::RunTest(const FString& Parameters)
{
	FImmortalPetState State = UImmortalPetLibrary::CreateDefaultState();
	TestTrue(TEXT("Default state is initialized"), State.bInitialized);
	TestEqual(TEXT("Default state contains catalog entries"), State.Pets.Num(), 2);
	TestEqual(TEXT("Fox is the default active pet"),
		State.ActivePetId, FName(TEXT("SpiritFox")));

	FImmortalPetProgress Fox;
	FImmortalPetProgress Hound;
	TestTrue(TEXT("Fox progress exists"),
		UImmortalPetLibrary::GetPetProgress(State, TEXT("SpiritFox"), Fox));
	TestTrue(TEXT("Fox starts owned"), Fox.bOwned);
	TestTrue(TEXT("Hound progress exists"),
		UImmortalPetLibrary::GetPetProgress(State, TEXT("SpiritHound"), Hound));
	TestFalse(TEXT("Hound starts locked"), Hound.bOwned);
	TestFalse(TEXT("Locked pet cannot be selected"),
		UImmortalPetLibrary::SetActivePet(State, TEXT("SpiritHound")));
	TestTrue(TEXT("Hound unlock mutates state"),
		UImmortalPetLibrary::UnlockPet(State, TEXT("SpiritHound")));
	TestTrue(TEXT("Owned hound can be selected"),
		UImmortalPetLibrary::SetActivePet(State, TEXT("SpiritHound")));
	TestEqual(TEXT("Selection changed"), State.ActivePetId,
		FName(TEXT("SpiritHound")));

	FImmortalPetProgress Corrupt = Hound;
	Corrupt.bOwned = true;
	Corrupt.Level = 500;
	Corrupt.Experience = MAX_int32;
	Corrupt.Stars = 50;
	Corrupt.TotalCombatKills = -8;
	State.Pets.Add(Corrupt);
	State.ActivePetId = TEXT("MissingPet");
	State.TotalCombatKills = -1;
	const int32 RevisionBeforeRepair = State.Revision;
	TestTrue(TEXT("Malformed state is repaired"),
		UImmortalPetLibrary::NormalizeState(State));
	TestTrue(TEXT("Repair advances revision"),
		State.Revision > RevisionBeforeRepair);
	TestEqual(TEXT("Duplicates are merged"), State.Pets.Num(), 2);
	TestTrue(TEXT("Owned active fallback survives"),
		!State.ActivePetId.IsNone());
	TestEqual(TEXT("Total kills clamp to zero"), State.TotalCombatKills,
		static_cast<int64>(0));
	TestTrue(TEXT("Hound still resolves"),
		UImmortalPetLibrary::GetPetProgress(State, TEXT("SpiritHound"), Hound));
	TestEqual(TEXT("Level clamps to cap"), Hound.Level,
		UImmortalPetLibrary::MaximumPetLevel);
	TestEqual(TEXT("Max-level experience is zero"), Hound.Experience, 0);
	TestEqual(TEXT("Stars clamp to cap"), Hound.Stars,
		UImmortalPetLibrary::MaximumPetStars);
	const int32 RepairedRevision = State.Revision;
	TestFalse(TEXT("Normalization is idempotent"),
		UImmortalPetLibrary::NormalizeState(State));
	TestEqual(TEXT("Idempotent normalization keeps revision"),
		State.Revision, RepairedRevision);

	FImmortalPetProgress Unknown;
	Unknown.PetId = TEXT("FuturePet");
	Unknown.bOwned = true;
	Unknown.Level = 7;
	State.Pets.Add(Unknown);
	State.ActivePetId = Unknown.PetId;
	TestTrue(TEXT("Unknown active pet is repaired"),
		UImmortalPetLibrary::NormalizeState(State));
	FImmortalPetProgress PreservedUnknown;
	TestTrue(TEXT("Unknown forward-compatible progress is retained"),
		UImmortalPetLibrary::GetPetProgress(
			State, Unknown.PetId, PreservedUnknown));
	TestTrue(TEXT("Unknown owned flag is retained"),
		PreservedUnknown.bOwned);
	TestTrue(TEXT("Unknown pet cannot remain active"),
		State.ActivePetId != Unknown.PetId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalPetGrowthTest,
	"ImmortalPath.Pets.ExperienceStarsAndCombatScaling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalPetGrowthTest::RunTest(const FString& Parameters)
{
	FImmortalPetState State = UImmortalPetLibrary::CreateDefaultState();
	FImmortalPetProgress Before;
	TestTrue(TEXT("Default active progress resolves"),
		UImmortalPetLibrary::GetPetProgress(
			State, State.ActivePetId, Before));
	const int32 Required =
		UImmortalPetLibrary::GetExperienceRequiredForLevel(Before.Level);
	const FImmortalPetExperienceResult Growth =
		UImmortalPetLibrary::GrantActivePetExperience(
			State, Required + 25, 3);
	TestTrue(TEXT("Experience grant succeeds"), Growth.bSucceeded);
	TestEqual(TEXT("One level gained"), Growth.LevelsGained, 1);
	TestEqual(TEXT("Overflow experience carries"), Growth.CurrentExperience, 25);
	TestEqual(TEXT("Kill audit advances"), State.TotalCombatKills,
		static_cast<int64>(3));

	FImmortalPetProgress After;
	TestTrue(TEXT("Grown progress resolves"),
		UImmortalPetLibrary::GetPetProgress(
			State, State.ActivePetId, After));
	FImmortalPetDefinition Definition;
	TestTrue(TEXT("Definition resolves"),
		UImmortalPetLibrary::GetPetDefinition(
			State.ActivePetId, Definition));
	const float LevelDamage =
		UImmortalPetLibrary::CalculateDamageRatio(Definition, After);
	TestTrue(TEXT("Level growth raises damage"),
		LevelDamage > UImmortalPetLibrary::CalculateDamageRatio(
			Definition, Before));

	const FImmortalCraftingCost FirstStarCost =
		UImmortalPetLibrary::GetStarUpCost(After);
	TestTrue(TEXT("Star growth costs stones"),
		FirstStarCost.SpiritStones > 0);
	TestFalse(TEXT("Star growth consumes monster material"),
		FirstStarCost.Materials.IsEmpty());
	TestTrue(TEXT("Star state mutation succeeds"),
		UImmortalPetLibrary::RaiseStar(State, State.ActivePetId));
	FImmortalPetProgress Starred;
	UImmortalPetLibrary::GetPetProgress(
		State, State.ActivePetId, Starred);
	TestTrue(TEXT("Star growth raises damage"),
		UImmortalPetLibrary::CalculateDamageRatio(Definition, Starred)
			> LevelDamage);
	TestTrue(TEXT("Later star cost increases"),
		UImmortalPetLibrary::GetStarUpCost(Starred).SpiritStones
			> FirstStarCost.SpiritStones);
	TestTrue(TEXT("World Boss grants more pet experience than normal map kill"),
		UImmortalPetLibrary::CalculateCombatExperienceReward(
			100, false, true, true)
			> UImmortalPetLibrary::CalculateCombatExperienceReward(
				100, false, false, false));

	FImmortalPetState CorruptState =
		UImmortalPetLibrary::CreateDefaultState();
	FImmortalPetProgress* Corrupt =
		UImmortalPetLibrary::FindMutablePetProgress(
			CorruptState, CorruptState.ActivePetId);
	TestNotNull(TEXT("Corrupt-growth fixture resolves"), Corrupt);
	if (Corrupt)
	{
		Corrupt->Level = MIN_int32;
		Corrupt->Experience = MIN_int32;
		Corrupt->TotalCombatKills = MAX_int64 - 1;
		CorruptState.TotalCombatKills = MAX_int64 - 1;
		const FImmortalPetExperienceResult SafeGrowth =
			UImmortalPetLibrary::GrantActivePetExperience(
				CorruptState, MAX_int32, 5);
		TestTrue(TEXT("Corrupt XP input is repaired without a long loop"),
			SafeGrowth.bSucceeded);
		TestTrue(TEXT("Large XP stays within the level cap"),
			SafeGrowth.CurrentLevel
				<= UImmortalPetLibrary::MaximumPetLevel);
		TestEqual(TEXT("Per-pet kill count saturates"),
			Corrupt->TotalCombatKills, MAX_int64);
		TestEqual(TEXT("Global kill count saturates"),
			CorruptState.TotalCombatKills, MAX_int64);
	}

	FImmortalPetState ZeroState =
		UImmortalPetLibrary::CreateDefaultState();
	const int32 ZeroRevision = ZeroState.Revision;
	TestFalse(TEXT("Zero XP is rejected"),
		UImmortalPetLibrary::GrantActivePetExperience(
			ZeroState, 0, 0).bSucceeded);
	TestEqual(TEXT("Rejected XP does not change revision"),
		ZeroState.Revision, ZeroRevision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalPetSaveSchemaTest,
	"ImmortalPath.Pets.SaveSchemaV23",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalPetSaveSchemaTest::RunTest(const FString& Parameters)
{
	UImmortalPathSaveGame* SaveGame =
		NewObject<UImmortalPathSaveGame>();
	TestNotNull(TEXT("SaveGame fixture exists"), SaveGame);
	if (!SaveGame)
	{
		return false;
	}
	TestEqual(TEXT("Death recovery persistence advances schema to v23"),
		UImmortalPathSaveGame::CurrentSaveVersion, 23);
	TestEqual(TEXT("New SaveGame uses v23"),
		SaveGame->SaveVersion, 23);
	TestFalse(TEXT("Raw new SaveGame has no pet snapshot marker"),
		SaveGame->bPetSystemInitialized);

	SaveGame->bPetSystemInitialized = true;
	SaveGame->PetState =
		UImmortalPetLibrary::CreateDefaultState();
	TestTrue(TEXT("Pet state payload remains initialized"),
		SaveGame->PetState.bInitialized);
	TestEqual(TEXT("Pet selection survives assignment"),
		SaveGame->PetState.ActivePetId,
		FName(TEXT("SpiritFox")));
	return true;
}

#endif
