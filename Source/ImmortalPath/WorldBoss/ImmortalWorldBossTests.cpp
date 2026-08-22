// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "ImmortalWorldBossTypes.h"

#include "../Artifacts/ImmortalArtifactTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "../Save/ImmortalPathSaveGame.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalWorldBossCatalogAndProgressionTest,
	"ImmortalPath.WorldBoss.CatalogAndProgression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalWorldBossRewardPersistenceSafetyTest,
	"ImmortalPath.WorldBoss.RewardPersistenceSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalWorldBossCatalogAndProgressionTest::RunTest(const FString& Parameters)
{
	const TArray<FName> BossIds = UImmortalWorldBossLibrary::GetKnownWorldBossIds();
	const FName FirstBossId(TEXT("AzureScaleDragon"));
	const FName SecondBossId(TEXT("SevenStarDemonLord"));
	const TArray<FName> BuiltInBossIds = {
		FirstBossId,
		SecondBossId,
		FName(TEXT("CloudAbyssLeviathan")),
		FName(TEXT("ChaosHeavenBeast"))};
	TestTrue(TEXT("World Boss catalog contains at least the four built-in encounters"),
		BossIds.Num() >= 4);
	for (const FName BuiltInId : BuiltInBossIds)
	{
		TestTrue(TEXT("Every built-in World Boss remains present"),
			BossIds.Contains(BuiltInId));
	}

	TSet<FName> SeenIds;
	for (const FName BossId : BossIds)
	{
		FImmortalWorldBossDefinition Definition;
		TestTrue(TEXT("Every catalog ID resolves"), UImmortalWorldBossLibrary::GetWorldBossDefinition(BossId, Definition));
		TestTrue(TEXT("Every World Boss definition is complete"), Definition.IsValid());
		TestFalse(TEXT("World Boss IDs are unique"), SeenIds.Contains(BossId));
		SeenIds.Add(BossId);
		TestTrue(TEXT("A World Boss guarantees multiple high-quality equipment drops"),
			Definition.GuaranteedEquipmentDrops >= 4);
		TestTrue(TEXT("World Boss has a long-range skill"), Definition.SkillBonusRange >= 300.0f);
		TestTrue(TEXT("World Boss has phase summons"),
			Definition.PhaseTwoSummonCount > 0 && Definition.PhaseThreeSummonCount > 0);

		FImmortalMaterialDefinition Material;
		FImmortalArtifactDefinition Artifact;
		TestTrue(TEXT("Independent rare material resolves"),
			UImmortalMaterialLibrary::GetMaterialDefinition(Definition.RareMaterialId, Material));
		TestTrue(TEXT("Fixed first-clear artifact resolves"),
			UImmortalArtifactLibrary::GetArtifactDefinition(Definition.FirstClearArtifactId, Artifact));
		TestTrue(TEXT("Boss is locked one realm below its requirement"),
			Definition.RequiredRealmIndex == 0
				|| !UImmortalWorldBossLibrary::IsUnlocked(Definition, Definition.RequiredRealmIndex - 1));
		TestTrue(TEXT("Boss unlocks at its exact realm requirement"),
			UImmortalWorldBossLibrary::IsUnlocked(Definition, Definition.RequiredRealmIndex));
	}

	int32 PreviousRealm = -1;
	int32 PreviousStage = 0;
	int32 PreviousQuality = -1;
	for (const FName BuiltInId : BuiltInBossIds)
	{
		FImmortalWorldBossDefinition Definition;
		TestTrue(TEXT("Built-in progression definition resolves"),
			UImmortalWorldBossLibrary::GetWorldBossDefinition(BuiltInId, Definition));
		TestTrue(TEXT("Built-in realm requirements never regress"),
			Definition.RequiredRealmIndex >= PreviousRealm);
		TestTrue(TEXT("Built-in recommended stages increase"),
			Definition.RecommendedStage > PreviousStage);
		TestTrue(TEXT("Built-in equipment quality never regresses"),
			static_cast<int32>(Definition.MinimumEquipmentQuality) >= PreviousQuality);
		PreviousRealm = Definition.RequiredRealmIndex;
		PreviousStage = Definition.RecommendedStage;
		PreviousQuality = static_cast<int32>(Definition.MinimumEquipmentQuality);
	}

	FImmortalWorldBossState State = UImmortalWorldBossLibrary::CreateDefaultState();
	TestTrue(TEXT("Default World Boss state is initialized"), State.bInitialized);
	TestEqual(TEXT("Default state has one row per boss"), State.BossProgress.Num(), BossIds.Num());

	FImmortalWorldBossProgress Duplicate;
	TestTrue(TEXT("First built-in progress resolves before duplicate test"),
		UImmortalWorldBossLibrary::GetProgress(State, FirstBossId, Duplicate));
	Duplicate.DefeatCount = 2;
	Duplicate.BestClearSeconds = 54.0f;
	Duplicate.LastDefeatedUtcTicks = 1234;
	State.BossProgress.Add(Duplicate);
	FImmortalWorldBossProgress Unknown;
	Unknown.BossId = TEXT("RemovedBoss");
	Unknown.DefeatCount = 99;
	State.BossProgress.Add(Unknown);
	TestTrue(TEXT("Normalization repairs duplicate and unknown rows"),
		UImmortalWorldBossLibrary::NormalizeState(State));
	TestEqual(TEXT("Normalized state returns to catalog cardinality"), State.BossProgress.Num(), BossIds.Num());

	FImmortalWorldBossProgress Progress;
	TestTrue(TEXT("Merged progress resolves"),
		UImmortalWorldBossLibrary::GetProgress(State, FirstBossId, Progress));
	TestEqual(TEXT("Duplicate progress keeps greatest defeat count"), Progress.DefeatCount, 2);
	TestEqual(TEXT("Duplicate progress keeps valid best time"), Progress.BestClearSeconds, 54.0f);

	// Reset one entry to exercise a true first clear.
	FImmortalWorldBossProgress* Mutable = State.BossProgress.FindByPredicate(
		[SecondBossId](const FImmortalWorldBossProgress& Entry)
		{
			return Entry.BossId == SecondBossId;
		});
	TestNotNull(TEXT("Second boss progress is present"), Mutable);
	if (Mutable)
	{
		*Mutable = FImmortalWorldBossProgress();
		Mutable->BossId = SecondBossId;
	}
	const int32 RevisionBefore = State.Revision;
	const FImmortalWorldBossRecordResult First =
		UImmortalWorldBossLibrary::RecordDefeat(State, SecondBossId, 61.0f, 5000);
	TestTrue(TEXT("First defeat records"), First.bSucceeded);
	TestTrue(TEXT("First defeat grants first-clear status"), First.bFirstClear);
	TestTrue(TEXT("First defeat is a new best"), First.bNewBestTime);
	TestTrue(TEXT("First-clear artifact flag is persisted"), First.Progress.bFirstClearArtifactClaimed);
	TestEqual(TEXT("Victory increments revision once after normalization"), State.Revision, RevisionBefore + 1);

	const FImmortalWorldBossRecordResult Slower =
		UImmortalWorldBossLibrary::RecordDefeat(State, SecondBossId, 80.0f, 6000);
	TestTrue(TEXT("Repeat defeat records"), Slower.bSucceeded);
	TestFalse(TEXT("Repeat defeat is not first clear"), Slower.bFirstClear);
	TestFalse(TEXT("Slower clear does not replace record"), Slower.bNewBestTime);
	TestEqual(TEXT("Slower clear preserves best time"), Slower.Progress.BestClearSeconds, 61.0f);
		TestEqual(TEXT("Repeat defeat increments count"), Slower.Progress.DefeatCount, 2);

	const FImmortalWorldBossRecordResult Faster =
		UImmortalWorldBossLibrary::RecordDefeat(State, SecondBossId, 42.5f, 7000);
	TestTrue(TEXT("Faster clear becomes the new record"), Faster.bNewBestTime);
	TestEqual(TEXT("Faster clear is persisted"), Faster.Progress.BestClearSeconds, 42.5f);

	FImmortalWorldBossDefinition RewardDefinition;
	TestTrue(TEXT("Reward source boss resolves"),
		UImmortalWorldBossLibrary::GetWorldBossDefinition(FirstBossId, RewardDefinition));
	const FImmortalWorldBossRewardBundle FirstClearReward =
		UImmortalWorldBossLibrary::CreateRewardBundle(
			RewardDefinition, 12, true, 8000);
	TestTrue(TEXT("First-clear reward bundle is durable and valid"), FirstClearReward.IsValid());
	TestEqual(TEXT("Reward has the configured equipment count"),
		FirstClearReward.EquipmentItems.Num(), RewardDefinition.GuaranteedEquipmentDrops);
	TestEqual(TEXT("Reward carries the fixed first-clear artifact"),
		FirstClearReward.ArtifactId, RewardDefinition.FirstClearArtifactId);
	for (const FImmortalEquipmentItem& Item : FirstClearReward.EquipmentItems)
	{
		TestTrue(TEXT("Reward equipment meets minimum quality"),
			static_cast<int32>(Item.Quality)
				>= static_cast<int32>(RewardDefinition.MinimumEquipmentQuality));
	}
	const FImmortalWorldBossRewardBundle RepeatReward =
		UImmortalWorldBossLibrary::CreateRewardBundle(
			RewardDefinition, 12, false, 9000);
	TestTrue(TEXT("Repeat reward bundle is valid"), RepeatReward.IsValid());
	TestTrue(TEXT("Repeat reward has no additional fixed artifact"), RepeatReward.ArtifactId.IsNone());

	State.PendingRewards.Add(FirstClearReward);
	State.PendingRewards.Add(FirstClearReward);
	TestTrue(TEXT("Normalization removes duplicate pending reward IDs"),
		UImmortalWorldBossLibrary::NormalizeState(State));
	TestEqual(TEXT("Exactly one durable pending reward remains"), State.PendingRewards.Num(), 1);
	TestEqual(TEXT("Death recovery schema v23 preserves World Boss state"),
		UImmortalPathSaveGame::CurrentSaveVersion, 23);
	return true;
}

bool FImmortalWorldBossRewardPersistenceSafetyTest::RunTest(
	const FString& Parameters)
{
	FImmortalWorldBossDefinition Definition;
	TestTrue(TEXT("Reward safety boss resolves"),
		UImmortalWorldBossLibrary::GetWorldBossDefinition(
			TEXT("AzureScaleDragon"), Definition));
	const FImmortalWorldBossRewardBundle First =
		UImmortalWorldBossLibrary::CreateRewardBundle(Definition, 20, true, 12345);
	const FImmortalWorldBossRewardBundle Second =
		UImmortalWorldBossLibrary::CreateRewardBundle(Definition, 20, false, 12346);
	TestTrue(TEXT("First reward is valid"), First.IsValid());
	TestTrue(TEXT("Second reward is valid"), Second.IsValid());
	TestNotEqual(TEXT("Separately rolled rewards use unique transaction IDs"),
		First.RewardId, Second.RewardId);
	TestEqual(TEXT("First clear carries the fixed artifact"),
		First.ArtifactId, Definition.FirstClearArtifactId);
	TestTrue(TEXT("Repeat clear does not duplicate the fixed artifact"),
		Second.ArtifactId.IsNone());

	FImmortalWorldBossRewardBundle Repairable = First;
	Repairable.CreatedUtcTicks = -50;
	const FImmortalMaterialStack DuplicateMaterial = Repairable.Materials[0];
	Repairable.Materials.Add(DuplicateMaterial);
	FImmortalWorldBossRewardBundle Invalid = Second;
	Invalid.SpiritStones = 0;
	FImmortalWorldBossRewardBundle LegacyCatalogReward = Second;
	LegacyCatalogReward.BossId = TEXT("RetiredWorldBoss");
	LegacyCatalogReward.ArtifactId = TEXT("RetiredArtifact");

	FImmortalWorldBossState State = UImmortalWorldBossLibrary::CreateDefaultState();
	const int32 InitialRevision = State.Revision;
	State.PendingRewards = {
		Repairable, Repairable, Invalid, LegacyCatalogReward};
	TestTrue(TEXT("Normalization reports repaired, duplicate and invalid pending rewards"),
		UImmortalWorldBossLibrary::NormalizeState(State));
	TestEqual(TEXT("Valid and legacy catalog-independent transactions survive"),
		State.PendingRewards.Num(), 2);
	TestEqual(TEXT("Negative pending timestamp is repaired"),
		State.PendingRewards[0].CreatedUtcTicks, static_cast<int64>(0));
	TestEqual(TEXT("Duplicate material stacks are merged"),
		State.PendingRewards[0].Materials.Num(), 2);
	TestEqual(TEXT("Retired catalog transaction remains durable"),
		State.PendingRewards[1].BossId, FName(TEXT("RetiredWorldBoss")));
	TestEqual(TEXT("Normalization advances the state revision once"),
		State.Revision, InitialRevision + 1);

	const int32 StableRevision = State.Revision;
	TestFalse(TEXT("Normalized pending state is idempotent"),
		UImmortalWorldBossLibrary::NormalizeState(State));
	TestEqual(TEXT("Idempotent normalization does not churn revisions"),
		State.Revision, StableRevision);

	UImmortalPathSaveGame* SaveGame = NewObject<UImmortalPathSaveGame>();
	TestNotNull(TEXT("SaveGame can hold World Boss state"), SaveGame);
	if (SaveGame)
	{
		TestEqual(TEXT("New SaveGame uses v23"), SaveGame->SaveVersion, 23);
		TestFalse(TEXT("Unmigrated SaveGame leaves World Boss initialization explicit"),
			SaveGame->bWorldBossInitialized);
		SaveGame->bWorldBossInitialized = true;
		SaveGame->WorldBossState = State;
		TestEqual(TEXT("Pending transaction survives SaveGame assignment"),
			SaveGame->WorldBossState.PendingRewards.Num(), 2);
	}
	return true;
}

#endif
