// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "ImmortalEndlessDungeonTypes.h"

#include "../Save/ImmortalPathSaveGame.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalEndlessDungeonRulesTest,
	"ImmortalPath.EndlessDungeon.RulesScalingAndFloorKinds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalEndlessDungeonProgressionTest,
	"ImmortalPath.EndlessDungeon.ProgressionCheckpointAndNormalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalEndlessDungeonRewardTest,
	"ImmortalPath.EndlessDungeon.RewardBundleSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalEndlessDungeonRulesTest::RunTest(const FString& Parameters)
{
	const FImmortalEndlessDungeonRules Rules =
		UImmortalEndlessDungeonLibrary::GetRules();
	TestTrue(TEXT("Endless Dungeon rules are complete"), Rules.IsValid());
	TestEqual(TEXT("Fallback supports floors 1 through 9999"),
		Rules.MaximumFloor, 9999);
	TestEqual(TEXT("Fallback checkpoint interval is ten floors"),
		Rules.CheckpointInterval, 10);
	TestEqual(TEXT("Fallback elite interval is five floors"),
		Rules.EliteFloorInterval, 5);
	TestEqual(TEXT("Fallback Boss interval is ten floors"),
		Rules.BossFloorInterval, 10);
	TestEqual(TEXT("Fallback normal floor contains three enemies"),
		Rules.NormalEnemyCount, 3);
	TestEqual(TEXT("Fallback elite floor contains two enemies"),
		Rules.EliteEnemyCount, 2);
	TestEqual(TEXT("Fallback Boss floor contains one enemy"),
		Rules.BossEnemyCount, 1);
	TestEqual(TEXT("Boss phase two summons one minion"),
		Rules.PhaseTwoSummonCount, 1);
	TestEqual(TEXT("Boss phase three summons two minions"),
		Rules.PhaseThreeSummonCount, 2);

	FImmortalEndlessFloorDescriptor Normal;
	FImmortalEndlessFloorDescriptor Elite;
	FImmortalEndlessFloorDescriptor Boss;
	TestTrue(TEXT("Floor one resolves"),
		UImmortalEndlessDungeonLibrary::GetFloorDescriptor(1, Normal));
	TestTrue(TEXT("Floor five resolves"),
		UImmortalEndlessDungeonLibrary::GetFloorDescriptor(5, Elite));
	TestTrue(TEXT("Floor ten resolves"),
		UImmortalEndlessDungeonLibrary::GetFloorDescriptor(10, Boss));
	TestTrue(TEXT("Normal descriptor is valid"), Normal.IsValid());
	TestTrue(TEXT("Elite descriptor is valid"), Elite.IsValid());
	TestTrue(TEXT("Boss descriptor is valid"), Boss.IsValid());
	TestFalse(TEXT("Floor one is neither elite nor Boss"),
		Normal.bElite || Normal.bBoss);
	TestEqual(TEXT("Normal floor requires three kills"),
		Normal.RequiredKills, 3);
	TestTrue(TEXT("Floor five is an elite floor"), Elite.bElite);
	TestFalse(TEXT("Floor five is not a Boss floor"), Elite.bBoss);
	TestEqual(TEXT("Elite floor requires two kills"),
		Elite.RequiredKills, 2);
	TestTrue(TEXT("Floor ten gives Boss precedence over elite cadence"),
		Boss.bBoss && !Boss.bElite);
	TestEqual(TEXT("Boss floor requires one kill"),
		Boss.RequiredKills, 1);
	TestEqual(TEXT("Normal floor grants no equipment"),
		Normal.EquipmentCount, 0);
	TestEqual(TEXT("Five-floor milestone grants one equipment item"),
		Elite.EquipmentCount, 1);
	TestEqual(TEXT("Ten-floor milestone grants two equipment items"),
		Boss.EquipmentCount, 2);
	TestTrue(TEXT("Ten-floor equipment quality improves over floor five"),
		static_cast<int32>(Boss.MinimumEquipmentQuality)
			> static_cast<int32>(Elite.MinimumEquipmentQuality));
	TestTrue(TEXT("Every floor grants spirit stones"),
		Normal.SpiritStones > 0 && Elite.SpiritStones > 0
			&& Boss.SpiritStones > 0);
	TestTrue(TEXT("Every floor grants material"),
		!Normal.Materials.IsEmpty()
			&& !Elite.Materials.IsEmpty()
			&& !Boss.Materials.IsEmpty());

	FImmortalEndlessFloorDescriptor Invalid;
	TestFalse(TEXT("Floor zero is outside the dungeon"),
		UImmortalEndlessDungeonLibrary::GetFloorDescriptor(0, Invalid));
	TestFalse(TEXT("Floor 10000 is outside the dungeon"),
		UImmortalEndlessDungeonLibrary::GetFloorDescriptor(10000, Invalid));

	bool bEveryDescriptorValid = true;
	bool bDifficultyNeverRegresses = true;
	bool bEquipmentNeverFloodsBackpack = true;
	bool bMilestoneQualityNeverRegresses = true;
	float PreviousHealth = 0.0f;
	float PreviousAttack = 0.0f;
	float PreviousDefense = 0.0f;
	int32 PreviousMilestoneQuality = -1;
	for (int32 Floor = 1; Floor <= Rules.MaximumFloor; ++Floor)
	{
		FImmortalEndlessFloorDescriptor Descriptor;
		if (!UImmortalEndlessDungeonLibrary::GetFloorDescriptor(
			Floor, Descriptor)
			|| !Descriptor.IsValid())
		{
			bEveryDescriptorValid = false;
			break;
		}
		bDifficultyNeverRegresses = bDifficultyNeverRegresses
			&& Descriptor.HealthMultiplier + KINDA_SMALL_NUMBER
				>= PreviousHealth
			&& Descriptor.AttackMultiplier + KINDA_SMALL_NUMBER
				>= PreviousAttack
			&& Descriptor.DefenseBonus + KINDA_SMALL_NUMBER
				>= PreviousDefense;
		bEquipmentNeverFloodsBackpack =
			bEquipmentNeverFloodsBackpack
				&& Descriptor.EquipmentCount >= 0
				&& Descriptor.EquipmentCount <= 2;
		if (Descriptor.EquipmentCount > 0)
		{
			const int32 Quality =
				static_cast<int32>(Descriptor.MinimumEquipmentQuality);
			bMilestoneQualityNeverRegresses =
				bMilestoneQualityNeverRegresses
					&& Quality >= PreviousMilestoneQuality;
			PreviousMilestoneQuality = Quality;
		}
		PreviousHealth = Descriptor.HealthMultiplier;
		PreviousAttack = Descriptor.AttackMultiplier;
		PreviousDefense = Descriptor.DefenseBonus;
	}
	TestTrue(TEXT("All 9999 floor descriptors are valid"),
		bEveryDescriptorValid);
	TestTrue(TEXT("Health, attack and defense never regress"),
		bDifficultyNeverRegresses);
	TestTrue(TEXT("No floor can grant more than two equipment items"),
		bEquipmentNeverFloodsBackpack);
	TestTrue(TEXT("Milestone minimum quality is non-decreasing"),
		bMilestoneQualityNeverRegresses);
	return true;
}

bool FImmortalEndlessDungeonProgressionTest::RunTest(
	const FString& Parameters)
{
	FImmortalEndlessDungeonState State =
		UImmortalEndlessDungeonLibrary::CreateDefaultState();
	TestTrue(TEXT("Default Endless Dungeon state is initialized"),
		State.bInitialized);
	TestEqual(TEXT("New save has no cleared floor"),
		State.HighestClearedFloor, 0);
	TestEqual(TEXT("New save starts at checkpoint floor one"),
		UImmortalEndlessDungeonLibrary::GetCheckpointStartFloor(State), 1);

	FImmortalEndlessDungeonState Checkpoint = State;
	Checkpoint.HighestClearedFloor = 9;
	TestEqual(TEXT("Record nine replays the first ten-floor segment"),
		UImmortalEndlessDungeonLibrary::GetCheckpointStartFloor(Checkpoint), 1);
	Checkpoint.HighestClearedFloor = 10;
	TestEqual(TEXT("Record ten unlocks checkpoint floor eleven"),
		UImmortalEndlessDungeonLibrary::GetCheckpointStartFloor(Checkpoint), 11);
	Checkpoint.HighestClearedFloor = 31;
	TestEqual(TEXT("Record thirty-one restarts at current segment floor thirty-one"),
		UImmortalEndlessDungeonLibrary::GetCheckpointStartFloor(Checkpoint), 31);

	const int32 InitialRevision = State.Revision;
	const FImmortalEndlessRecordResult Skipped =
		UImmortalEndlessDungeonLibrary::RecordFloorClear(
			State, 2, 1000);
	TestFalse(TEXT("A skipped floor is rejected"), Skipped.bSucceeded);
	TestEqual(TEXT("Rejected skip preserves highest floor"),
		State.HighestClearedFloor, 0);
	TestEqual(TEXT("Rejected skip preserves revision"),
		State.Revision, InitialRevision);

	for (int32 Floor = 1; Floor <= 10; ++Floor)
	{
		const FImmortalEndlessRecordResult Clear =
			UImmortalEndlessDungeonLibrary::RecordFloorClear(
				State, Floor, 1000 + Floor);
		TestTrue(*FString::Printf(
			TEXT("Sequential floor %d records"), Floor),
			Clear.bSucceeded);
		TestTrue(TEXT("Every accepted sequential clear is a new highest"),
			Clear.bNewHighest);
		TestEqual(TEXT("Record returns previous highest"),
			Clear.PreviousHighestFloor, Floor - 1);
		TestEqual(TEXT("Record returns new highest"),
			Clear.HighestClearedFloor, Floor);
	}
	TestEqual(TEXT("Ten sequential clears persist highest floor ten"),
		State.HighestClearedFloor, 10);
	TestEqual(TEXT("Total cleared-floor audit advances exactly ten"),
		State.TotalFloorsCleared, static_cast<int64>(10));
	TestEqual(TEXT("Five- and ten-floor milestones are each claimed once"),
		State.ClaimedMilestoneIds.Num(), 2);
	TestEqual(TEXT("Checkpoint after floor ten is floor eleven"),
		UImmortalEndlessDungeonLibrary::GetCheckpointStartFloor(State), 11);

	const int32 RevisionBeforeDuplicate = State.Revision;
	const FImmortalEndlessRecordResult Duplicate =
		UImmortalEndlessDungeonLibrary::RecordFloorClear(
			State, 10, 5000);
	TestFalse(TEXT("A duplicate floor clear is rejected"),
		Duplicate.bSucceeded);
	TestEqual(TEXT("Duplicate clear cannot advance revision"),
		State.Revision, RevisionBeforeDuplicate);
	TestEqual(TEXT("Duplicate clear cannot duplicate milestone claims"),
		State.ClaimedMilestoneIds.Num(), 2);
	const FImmortalEndlessRecordResult SecondSkip =
		UImmortalEndlessDungeonLibrary::RecordFloorClear(
			State, 12, 5001);
	TestFalse(TEXT("A later skipped floor remains rejected"),
		SecondSkip.bSucceeded);

	FImmortalEndlessFloorDescriptor FloorOne;
	TestTrue(TEXT("Reward fixture floor resolves"),
		UImmortalEndlessDungeonLibrary::GetFloorDescriptor(
			1, FloorOne));
	FImmortalEndlessRewardBundle Pending =
		UImmortalEndlessDungeonLibrary::CreateRewardBundle(
			FloorOne, -50);
	Pending.Materials[0].MaterialId = TEXT("RetiredEndlessMaterial");
	FImmortalEndlessRewardBundle Invalid = Pending;
	Invalid.RewardId.Invalidate();

	FImmortalEndlessDungeonState Dirty;
	Dirty.bInitialized = false;
	Dirty.Revision = -5;
	Dirty.HighestClearedFloor = -9;
	Dirty.TotalFloorsCleared = -20;
	Dirty.TotalRuns = -3;
	Dirty.LastClearedUtcTicks = -100;
	Dirty.ClaimedMilestoneIds = {
		TEXT("RetiredMilestone"),
		NAME_None,
		TEXT("RetiredMilestone"),
		TEXT("EndlessFloor_0005")};
	Dirty.PendingRewards = {Pending, Pending, Invalid};
	TestTrue(TEXT("Normalization repairs malformed state"),
		UImmortalEndlessDungeonLibrary::NormalizeState(Dirty));
	TestTrue(TEXT("Normalization initializes state"), Dirty.bInitialized);
	TestEqual(TEXT("A durable pending floor repairs the high-water mark"),
		Dirty.HighestClearedFloor, 1);
	TestEqual(TEXT("Pending floor also repairs the lifetime clear audit"),
		Dirty.TotalFloorsCleared, static_cast<int64>(1));
	TestEqual(TEXT("A recovered clear implies at least one run"),
		Dirty.TotalRuns, 1);
	TestEqual(TEXT("Normalized zero reward time remains a valid timestamp"),
		Dirty.LastClearedUtcTicks, static_cast<int64>(0));
	TestEqual(TEXT("Duplicate and empty milestone IDs are removed"),
		Dirty.ClaimedMilestoneIds.Num(), 2);
	TestTrue(TEXT("Unknown milestone claim is deliberately retained"),
		Dirty.ClaimedMilestoneIds.Contains(TEXT("RetiredMilestone")));
	TestEqual(TEXT("Duplicate and invalid pending rewards are removed"),
		Dirty.PendingRewards.Num(), 1);
	TestEqual(TEXT("Negative pending timestamp is repaired"),
		Dirty.PendingRewards[0].CreatedUtcTicks,
		static_cast<int64>(0));
	TestEqual(TEXT("Unknown pending material survives without catalog lookup"),
		Dirty.PendingRewards[0].Materials[0].MaterialId,
		FName(TEXT("RetiredEndlessMaterial")));
	TestEqual(TEXT("Repair advances normalized revision once"),
		Dirty.Revision, 1);
	const int32 StableRevision = Dirty.Revision;
	TestFalse(TEXT("Normalized state is idempotent"),
		UImmortalEndlessDungeonLibrary::NormalizeState(Dirty));
	TestEqual(TEXT("Idempotent normalization does not churn revision"),
		Dirty.Revision, StableRevision);

	FImmortalEndlessDungeonState DurableHighWater =
		UImmortalEndlessDungeonLibrary::CreateDefaultState();
	DurableHighWater.HighestClearedFloor = 15000;
	DurableHighWater.TotalFloorsCleared = 15000;
	TestTrue(TEXT("Out-of-format high-water mark is normalized"),
		UImmortalEndlessDungeonLibrary::NormalizeState(DurableHighWater));
	TestEqual(TEXT("Durable high-water mark clamps only to the save-format ceiling"),
		DurableHighWater.HighestClearedFloor, 9999);
	TestEqual(TEXT("Lifetime clear audit is never reduced with the playable cap"),
		DurableHighWater.TotalFloorsCleared, static_cast<int64>(15000));
	return true;
}

bool FImmortalEndlessDungeonRewardTest::RunTest(
	const FString& Parameters)
{
	FImmortalEndlessFloorDescriptor Normal;
	FImmortalEndlessFloorDescriptor Elite;
	FImmortalEndlessFloorDescriptor Boss;
	FImmortalEndlessFloorDescriptor LaterBoss;
	TestTrue(TEXT("Normal reward descriptor resolves"),
		UImmortalEndlessDungeonLibrary::GetFloorDescriptor(
			1, Normal));
	TestTrue(TEXT("Elite reward descriptor resolves"),
		UImmortalEndlessDungeonLibrary::GetFloorDescriptor(
			5, Elite));
	TestTrue(TEXT("Boss reward descriptor resolves"),
		UImmortalEndlessDungeonLibrary::GetFloorDescriptor(
			10, Boss));
	TestTrue(TEXT("Later Boss reward descriptor resolves"),
		UImmortalEndlessDungeonLibrary::GetFloorDescriptor(
			20, LaterBoss));

	const FImmortalEndlessRewardBundle NormalReward =
		UImmortalEndlessDungeonLibrary::CreateRewardBundle(
			Normal, -25);
	const FImmortalEndlessRewardBundle EliteReward =
		UImmortalEndlessDungeonLibrary::CreateRewardBundle(
			Elite, 2000);
	const FImmortalEndlessRewardBundle BossReward =
		UImmortalEndlessDungeonLibrary::CreateRewardBundle(
			Boss, 3000);
	const FImmortalEndlessRewardBundle SecondBossReward =
		UImmortalEndlessDungeonLibrary::CreateRewardBundle(
			Boss, 3001);

	TestTrue(TEXT("Normal floor reward is valid"),
		NormalReward.IsValid());
	TestTrue(TEXT("Elite floor reward is valid"),
		EliteReward.IsValid());
	TestTrue(TEXT("Boss floor reward is valid"),
		BossReward.IsValid());
	TestEqual(TEXT("Negative creation time is normalized"),
		NormalReward.CreatedUtcTicks, static_cast<int64>(0));
	TestTrue(TEXT("Normal floor contains currency and material"),
		NormalReward.SpiritStones > 0
			&& !NormalReward.Materials.IsEmpty());
	TestEqual(TEXT("Normal floor adds no equipment pressure"),
		NormalReward.EquipmentItems.Num(), 0);
	TestEqual(TEXT("Elite floor pre-rolls exactly one equipment item"),
		EliteReward.EquipmentItems.Num(), 1);
	TestEqual(TEXT("Boss floor pre-rolls exactly two equipment items"),
		BossReward.EquipmentItems.Num(), 2);
	TestNotEqual(TEXT("Two separately committed rewards use unique IDs"),
		BossReward.RewardId, SecondBossReward.RewardId);

	TSet<FGuid> SeenEquipmentIds;
	for (const FImmortalEquipmentItem& Item : EliteReward.EquipmentItems)
	{
		TestTrue(TEXT("Elite reward equipment meets minimum quality"),
			static_cast<int32>(Item.Quality)
				>= static_cast<int32>(Elite.MinimumEquipmentQuality));
		TestFalse(TEXT("Elite reward equipment GUID is unique"),
			SeenEquipmentIds.Contains(Item.ItemId));
		SeenEquipmentIds.Add(Item.ItemId);
	}
	for (const FImmortalEquipmentItem& Item : BossReward.EquipmentItems)
	{
		TestTrue(TEXT("Boss reward equipment meets minimum quality"),
			static_cast<int32>(Item.Quality)
				>= static_cast<int32>(Boss.MinimumEquipmentQuality));
		TestFalse(TEXT("Boss reward equipment GUID is unique"),
			SeenEquipmentIds.Contains(Item.ItemId));
		SeenEquipmentIds.Add(Item.ItemId);
	}
	TestTrue(TEXT("Later Boss minimum quality never regresses"),
		static_cast<int32>(LaterBoss.MinimumEquipmentQuality)
			>= static_cast<int32>(Boss.MinimumEquipmentQuality));

	FImmortalEndlessRewardBundle RetiredCatalogReward = BossReward;
	RetiredCatalogReward.Materials[0].MaterialId =
		TEXT("RetiredMaterialDefinition");
	RetiredCatalogReward.EquipmentItems[0].SetId =
		TEXT("RetiredRewardSet");
	RetiredCatalogReward.EquipmentItems[0].DisplayName =
		TEXT("PreRolledRewardName");
	TestTrue(TEXT("Persisted reward validation is catalog-independent"),
		RetiredCatalogReward.IsValid());

	FImmortalEndlessRewardBundle DuplicateEquipment = BossReward;
	DuplicateEquipment.EquipmentItems[1].ItemId =
		DuplicateEquipment.EquipmentItems[0].ItemId;
	TestFalse(TEXT("A corrupt bundle cannot duplicate an equipment instance ID"),
		DuplicateEquipment.IsValid());

	FImmortalEndlessDungeonState PendingState =
		UImmortalEndlessDungeonLibrary::CreateDefaultState();
	FImmortalEndlessRewardBundle DuplicateFloorReward =
		RetiredCatalogReward;
	DuplicateFloorReward.RewardId = FGuid::NewGuid();
	PendingState.PendingRewards = {
		RetiredCatalogReward,
		RetiredCatalogReward,
		DuplicateFloorReward};
	TestTrue(TEXT("Pending reward ID and floor duplicates are detected"),
		UImmortalEndlessDungeonLibrary::NormalizeState(PendingState));
	TestEqual(TEXT("Exactly one pending transaction survives"),
		PendingState.PendingRewards.Num(), 1);
	TestEqual(TEXT("Pending transaction repairs permanent progress"),
		PendingState.HighestClearedFloor, 10);
	TestEqual(TEXT("Pending transaction repairs unique-clear audit"),
		PendingState.TotalFloorsCleared, static_cast<int64>(10));
	TestEqual(TEXT("Catalog-independent material survives normalization"),
		PendingState.PendingRewards[0].Materials[0].MaterialId,
		FName(TEXT("RetiredMaterialDefinition")));
	TestEqual(TEXT("Unknown pre-rolled set ID survives normalization"),
		PendingState.PendingRewards[0].EquipmentItems[0].SetId,
		FName(TEXT("RetiredRewardSet")));
	TestEqual(TEXT("Pre-rolled equipment name survives normalization"),
		PendingState.PendingRewards[0].EquipmentItems[0].DisplayName,
		FName(TEXT("PreRolledRewardName")));

	UImmortalPathSaveGame* SaveGame =
		NewObject<UImmortalPathSaveGame>();
	TestNotNull(TEXT("SaveGame can hold Endless Dungeon state"), SaveGame);
	if (SaveGame)
	{
		TestEqual(TEXT("Death recovery persistence raises the save schema to v23"),
			UImmortalPathSaveGame::CurrentSaveVersion, 23);
		TestEqual(TEXT("New SaveGame writes v23"),
			SaveGame->SaveVersion, 23);
		TestFalse(TEXT("Migration initialization remains explicit"),
			SaveGame->bEndlessDungeonInitialized);
		SaveGame->bEndlessDungeonInitialized = true;
		SaveGame->EndlessDungeonState = PendingState;
		TestEqual(TEXT("Pending floor reward survives SaveGame assignment"),
			SaveGame->EndlessDungeonState.PendingRewards.Num(), 1);
	}
	return true;
}

#endif
