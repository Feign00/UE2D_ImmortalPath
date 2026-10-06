// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPetTypes.h"
#include "../Save/ImmortalPathSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "PaperFlipbook.h"

#include <limits>

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
		const float MaximumPresentationScale =
			Definition.FootAnchorMode == EImmortalPetFootAnchorMode::PlayerBattleLane
				? 1.25f : 0.75f;
		TestTrue(TEXT("Supplied pet sheets use taskbar-scale presentation"),
			Definition.VisualScale >= 0.25f
				&& Definition.VisualScale <= MaximumPresentationScale);
		AttackStyles.Add(static_cast<uint8>(Definition.AttackStyle));
	}
	TestEqual(TEXT("Initial pets cover melee and ranged roles"),
		AttackStyles.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalPetDesktopFoxPresentationTest,
	"ImmortalPath.Pets.DesktopFoxPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalPetDesktopFoxPresentationTest::RunTest(const FString& Parameters)
{
	FImmortalPetDefinition Fox;
	if (!TestTrue(TEXT("SpiritFox definition resolves"),
		UImmortalPetLibrary::GetPetDefinition(TEXT("SpiritFox"), Fox)))
	{
		return false;
	}
	TestTrue(TEXT("New fox keeps a valid catalog definition"), Fox.IsValid());
	TestTrue(TEXT("New fox follows the owner's visual battle foot plane"),
		Fox.FootAnchorMode == EImmortalPetFootAnchorMode::PlayerBattleLane);
	TestEqual(TEXT("New fox uses the readable battle-lane scale"), Fox.VisualScale, 1.25f);
	TestTrue(TEXT("New fox preserves authored colors"), Fox.DisplayColor == FLinearColor::White);
	TestEqual(TEXT("New fox contact is frame index 3 at 12 FPS"),
		Fox.AttackContactSeconds, 3.0f / 12.0f);
	TestEqual(TEXT("Fox damage ratio is unchanged"), Fox.BaseDamageRatio, 0.18f);
	TestEqual(TEXT("Fox attack interval is unchanged"), Fox.AttackInterval, 1.15f);
	TestEqual(TEXT("Fox falls back to the capsule bottom without an owner foot plane"),
		Fox.GetSpriteRelativeZ(17.0f, 34.0f), -34.0f);

	const TCHAR* States[] = { TEXT("Idle"), TEXT("Move"), TEXT("Attack"), TEXT("Hurt"), TEXT("Death") };
	const float ExpectedFPS[] = { 8.0f, 8.0f, 12.0f, 12.0f, 10.0f };
	const TSoftObjectPtr<UPaperFlipbook> Clips[] = {
		Fox.IdleFlipbook, Fox.MoveFlipbook, Fox.AttackFlipbook, Fox.HurtFlipbook, Fox.DeathFlipbook
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(States); ++Index)
	{
		const FString ExpectedPath = FString::Printf(
			TEXT("/Game/GAME/Asset/Monsters/desktop_pixel_v2/fox/FB_Fox_Pixel_%s.FB_Fox_Pixel_%s"),
			States[Index], States[Index]);
		TestEqual(FString::Printf(TEXT("Fox %s uses the new shared actor art"), States[Index]),
			Clips[Index].ToSoftObjectPath().ToString(), ExpectedPath);
		UPaperFlipbook* Clip = Clips[Index].LoadSynchronous();
		if (TestNotNull(FString::Printf(TEXT("Fox %s clip loads"), States[Index]), Clip))
		{
			TestEqual(FString::Printf(TEXT("Fox %s has eight authored frames"), States[Index]),
				Clip->GetNumFrames(), 8);
			TestEqual(FString::Printf(TEXT("Fox %s keeps authored FPS"), States[Index]),
				Clip->GetFramesPerSecond(), ExpectedFPS[Index]);
		}
	}
	if (UPaperFlipbook* Attack = Fox.AttackFlipbook.LoadSynchronous())
	{
		TestEqual(TEXT("Authored contact time is used by pet attacks"),
			Fox.GetAttackContactDelay(Attack->GetTotalDuration()), 0.25f);
	}

	FImmortalPetDefinition Hound;
	if (!TestTrue(TEXT("SpiritHound definition resolves"),
		UImmortalPetLibrary::GetPetDefinition(TEXT("SpiritHound"), Hound)))
	{
		return false;
	}
	TestTrue(TEXT("Hound remains valid"), Hound.IsValid());
	TestTrue(TEXT("Hound retains legacy sprite anchoring"),
		Hound.FootAnchorMode == EImmortalPetFootAnchorMode::LegacyOffset);
	TestEqual(TEXT("Hound keeps its legacy anchor offset"),
		Hound.GetSpriteRelativeZ(17.0f, 34.0f), 51.0f);
	TestEqual(TEXT("Hound keeps legacy attack timing selection"), Hound.AttackContactSeconds, -1.0f);
	TestTrue(TEXT("Hound keeps the existing stationary-pose fallback"), Hound.IdleFlipbook.IsNull());
	TestEqual(TEXT("Hound keeps its old scale"), Hound.VisualScale, 0.55f);
	TestTrue(TEXT("Hound keeps its old tint"),
		Hound.DisplayColor == FLinearColor(0.86f, 0.70f, 0.42f, 1.0f));
	TestEqual(TEXT("Hound damage ratio is unchanged"), Hound.BaseDamageRatio, 0.26f);
	TestEqual(TEXT("Hound attack interval is unchanged"), Hound.AttackInterval, 1.45f);
	TestEqual(TEXT("Hound movement is unchanged"), Hound.MovementSpeed, 620.0f);
	TestEqual(TEXT("Hound attack range is unchanged"), Hound.AttackRange, 205.0f);
	TestEqual(TEXT("Hound move art remains unchanged"),
		Hound.MoveFlipbook.ToSoftObjectPath().ToString(),
		FString(TEXT("/Game/GAME/Asset/Player/dog_move_left/dog_moveleft.dog_moveleft")));
	TestEqual(TEXT("Hound attack art remains unchanged"),
		Hound.AttackFlipbook.ToSoftObjectPath().ToString(),
		FString(TEXT("/Game/GAME/Asset/Player/dog_attcak/dogattack.dogattack")));
	TestEqual(TEXT("Hound hurt art remains unchanged"),
		Hound.HurtFlipbook.ToSoftObjectPath().ToString(),
		FString(TEXT("/Game/GAME/Asset/Player/doghurt/doghurt.doghurt")));
	TestEqual(TEXT("Hound death art remains unchanged"),
		Hound.DeathFlipbook.ToSoftObjectPath().ToString(),
		FString(TEXT("/Game/GAME/Asset/Player/dog_death/death_dog.death_dog")));
	const float LegacyDurations[] = { 0.01f, 0.03f, 0.10f, 0.60f, 1.0f, 2.0f };
	for (const float Duration : LegacyDurations)
	{
		const float OldMaximumWindup = FMath::Max(FMath::Min(Duration * 0.80f, 0.45f), 0.05f);
		const float OldContact = FMath::Clamp(Duration * 0.45f, 0.05f, OldMaximumWindup);
		TestEqual(TEXT("Hound attack timing preserves the exact legacy formula"),
			Hound.GetAttackContactDelay(Duration), OldContact);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalPetPresentationPolicyValidationTest,
	"ImmortalPath.Pets.PresentationPolicyValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalPetPresentationPolicyValidationTest::RunTest(const FString& Parameters)
{
	FImmortalPetDefinition Definition;
	if (!TestTrue(TEXT("Valid presentation fixture resolves"),
		UImmortalPetLibrary::GetPetDefinition(TEXT("SpiritFox"), Definition)))
	{
		return false;
	}
	FImmortalPetDefinition Corrupt = Definition;
	Corrupt.FootAnchorMode = static_cast<EImmortalPetFootAnchorMode>(255);
	TestFalse(TEXT("Unknown foot-anchor modes are rejected"), Corrupt.IsValid());
	Corrupt = Definition;
	Corrupt.AttackContactSeconds = -0.5f;
	TestFalse(TEXT("Unknown negative contact sentinels are rejected"), Corrupt.IsValid());
	Corrupt.AttackContactSeconds = -2.0f;
	TestFalse(TEXT("Contact values below the legacy sentinel are rejected"), Corrupt.IsValid());
	Corrupt.AttackContactSeconds = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN contact values are rejected"), Corrupt.IsValid());
	Corrupt.AttackContactSeconds = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Infinite contact values are rejected"), Corrupt.IsValid());
	Corrupt.AttackContactSeconds = -1.0f;
	TestTrue(TEXT("The legacy contact sentinel remains valid"), Corrupt.IsValid());
	Corrupt.AttackContactSeconds = 0.0f;
	TestTrue(TEXT("An explicitly immediate contact remains valid"), Corrupt.IsValid());
	TestEqual(TEXT("Immediate contact is resolved without a zero-rate timer"),
		Corrupt.GetAttackContactDelay(0.67f), 0.0f);
	TestEqual(TEXT("Fox contact remains independent of a longer authored clip"),
		Definition.GetAttackContactDelay(2.0f), 0.25f);
	TestTrue(TEXT("Contact cannot occur after an unexpectedly short clip"),
		FMath::IsNearlyEqual(Definition.GetAttackContactDelay(0.10f), 0.09f));
	TestEqual(TEXT("A minimum-duration clip cannot schedule a late contact"),
		Definition.GetAttackContactDelay(0.01f), 0.0f);
	TestEqual(TEXT("An invalid duration cannot schedule a late contact"),
		Definition.GetAttackContactDelay(std::numeric_limits<float>::quiet_NaN()), 0.0f);
	TestEqual(TEXT("Foot anchoring does not inherit the old sprite-height offset"),
		Definition.GetSpriteRelativeZ(-70.0f, 42.0f), -42.0f);
	Definition.VisualScale = 0.5f;
	TestEqual(TEXT("Visual scale does not change the foot contact point"),
		Definition.GetSpriteRelativeZ(-70.0f, 42.0f), -42.0f);

	Definition.FootAnchorMode = EImmortalPetFootAnchorMode::PlayerBattleLane;
	TestTrue(TEXT("Owner battle-lane foot anchoring is valid"), Definition.IsValid());
	TestEqual(TEXT("Owner battle lane starts at the capsule bottom until a reference is available"),
		Definition.GetSpriteRelativeZ(17.0f, 34.0f), -34.0f);
	TestEqual(TEXT("Owner battle-lane fallback clamps an invalid negative capsule height"),
		Definition.GetSpriteRelativeZ(17.0f, -34.0f), 0.0f);
	Definition.VisualScale = 1.25f;
	TestEqual(TEXT("Owner battle-lane fallback is independent of the new larger scale"),
		Definition.GetSpriteRelativeZ(-70.0f, 42.0f), -42.0f);

	Definition.FootAnchorMode = EImmortalPetFootAnchorMode::CapsuleBottom;
	TestTrue(TEXT("Direct capsule-bottom anchoring remains valid"), Definition.IsValid());
	TestEqual(TEXT("Direct capsule-bottom anchoring retains its old pure strategy"),
		Definition.GetSpriteRelativeZ(17.0f, 34.0f), -34.0f);
	Definition.FootAnchorMode = EImmortalPetFootAnchorMode::LegacyOffset;
	TestTrue(TEXT("Legacy offset anchoring remains valid"), Definition.IsValid());
	TestEqual(TEXT("Legacy offset anchoring retains its old pure strategy"),
		Definition.GetSpriteRelativeZ(17.0f, 34.0f), 51.0f);
	TestEqual(TEXT("Legacy mode ordinal is preserved for existing definitions"),
		static_cast<uint8>(EImmortalPetFootAnchorMode::LegacyOffset), static_cast<uint8>(0));
	TestEqual(TEXT("Capsule-bottom mode ordinal is preserved for existing definitions"),
		static_cast<uint8>(EImmortalPetFootAnchorMode::CapsuleBottom), static_cast<uint8>(1));
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
