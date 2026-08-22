// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalCaveTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../Save/ImmortalPathSaveGame.h"
#include "Misc/AutomationTest.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"

namespace
{
	bool AreCaveStatesEqual(const FImmortalCaveState& Left, const FImmortalCaveState& Right)
	{
		if (Left.bInitialized != Right.bInitialized
			|| Left.LastSettlementUtcTicks != Right.LastSettlementUtcTicks
			|| Left.StoredSpiritStones != Right.StoredSpiritStones
			|| Left.StoredSpiritGrass != Right.StoredSpiritGrass
			|| Left.StoredOre != Right.StoredOre
			|| Left.SpiritStoneFraction != Right.SpiritStoneFraction
			|| Left.SpiritGrassFraction != Right.SpiritGrassFraction
			|| Left.OreFraction != Right.OreFraction
			|| Left.TotalSpiritStonesProduced != Right.TotalSpiritStonesProduced
			|| Left.TotalSpiritGrassProduced != Right.TotalSpiritGrassProduced
			|| Left.TotalOreProduced != Right.TotalOreProduced
			|| Left.TotalSpiritStonesCollected != Right.TotalSpiritStonesCollected
			|| Left.TotalSpiritGrassCollected != Right.TotalSpiritGrassCollected
			|| Left.TotalOreCollected != Right.TotalOreCollected
			|| Left.CollectionCount != Right.CollectionCount
			|| Left.Revision != Right.Revision
			|| Left.Buildings.Num() != Right.Buildings.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Buildings.Num(); ++Index)
		{
			if (Left.Buildings[Index].Type != Right.Buildings[Index].Type
				|| Left.Buildings[Index].Level != Right.Buildings[Index].Level)
			{
				return false;
			}
		}
		return true;
	}

	bool AreMaterialInventoriesEqual(
		const TArray<FImmortalMaterialStack>& Left,
		const TArray<FImmortalMaterialStack>& Right)
	{
		if (Left.Num() != Right.Num()) return false;
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index].MaterialId != Right[Index].MaterialId
				|| Left[Index].Quantity != Right[Index].Quantity)
			{
				return false;
			}
		}
		return true;
	}

	bool AreProductionValuesNearlyEqual(
		const FImmortalCaveState& Left,
		const FImmortalCaveState& Right)
	{
		return Left.LastSettlementUtcTicks == Right.LastSettlementUtcTicks
			&& Left.StoredSpiritStones == Right.StoredSpiritStones
			&& Left.StoredSpiritGrass == Right.StoredSpiritGrass
			&& Left.StoredOre == Right.StoredOre
			&& FMath::IsNearlyEqual(Left.SpiritStoneFraction, Right.SpiritStoneFraction, 1.0e-9)
			&& FMath::IsNearlyEqual(Left.SpiritGrassFraction, Right.SpiritGrassFraction, 1.0e-9)
			&& FMath::IsNearlyEqual(Left.OreFraction, Right.OreFraction, 1.0e-9)
			&& Left.TotalSpiritStonesProduced == Right.TotalSpiritStonesProduced
			&& Left.TotalSpiritGrassProduced == Right.TotalSpiritGrassProduced
			&& Left.TotalOreProduced == Right.TotalOreProduced;
	}

	void SetBuildingLevel(
		FImmortalCaveState& State,
		const EImmortalCaveBuildingType Type,
		const int32 Level)
	{
		if (FImmortalCaveBuildingProgress* Progress = State.Buildings.FindByPredicate(
			[Type](const FImmortalCaveBuildingProgress& Entry)
			{
				return Entry.Type == Type;
			}))
		{
			Progress->Level = Level;
		}
	}

	int32 GetTotalMaterialCost(const FImmortalCraftingCost& Cost)
	{
		int32 Total = 0;
		for (const FImmortalCraftingMaterialCost& Entry : Cost.Materials) Total += Entry.Quantity;
		return Total;
	}

	bool HasUniqueValidMaterialIds(const FImmortalCraftingCost& Cost)
	{
		TSet<FName> Seen;
		for (const FImmortalCraftingMaterialCost& Entry : Cost.Materials)
		{
			if (Entry.MaterialId.IsNone() || Entry.Quantity <= 0 || Seen.Contains(Entry.MaterialId)) return false;
			Seen.Add(Entry.MaterialId);
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalCaveCatalogUpgradeProductionTest,
	"ImmortalPath.Cave.CatalogUpgradeProductionAndSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalCaveCatalogUpgradeProductionTest::RunTest(const FString& Parameters)
{
	const int64 StartTicks = FDateTime(2026, 7, 18, 0, 0, 0).GetTicks();
	const TArray<EImmortalCaveBuildingType> BuildingTypes = UImmortalCaveLibrary::GetKnownBuildingTypes();
	TestEqual(TEXT("SaveGame version is v23 after death recovery persistence was added"),
		UImmortalPathSaveGame::CurrentSaveVersion, 23);
	TestEqual(TEXT("The cave catalog contains seven buildings"), BuildingTypes.Num(), 7);
	TSet<EImmortalCaveBuildingType> UniqueBuildingTypes;
	for (const EImmortalCaveBuildingType Type : BuildingTypes)
	{
		FImmortalCaveBuildingDefinition Definition;
		TestTrue(TEXT("Every cave building resolves a definition"),
			UImmortalCaveLibrary::GetBuildingDefinition(Type, Definition));
		TestEqual(TEXT("A definition preserves its stable building type"), Definition.Type, Type);
		TestFalse(TEXT("A cave building display name is present"), Definition.DisplayName.IsEmpty());
		TestFalse(TEXT("A cave building description is present"), Definition.Description.IsEmpty());
		TestEqual(TEXT("Every cave building has twenty levels"), Definition.MaximumLevel, 20);
		TestFalse(TEXT("Cave building identifiers are unique"), UniqueBuildingTypes.Contains(Type));
		UniqueBuildingTypes.Add(Type);
	}
	TestEqual(TEXT("All seven cave building identifiers are unique"), UniqueBuildingTypes.Num(), 7);
	FImmortalCaveBuildingDefinition UnknownDefinition;
	TestFalse(TEXT("An unknown cave building has no definition"),
		UImmortalCaveLibrary::GetBuildingDefinition(
			static_cast<EImmortalCaveBuildingType>(255), UnknownDefinition));

	const FImmortalCaveState DefaultState = UImmortalCaveLibrary::CreateDefaultState(StartTicks);
	TestTrue(TEXT("A default cave is initialized"), DefaultState.bInitialized);
	TestEqual(TEXT("Default settlement time is preserved exactly"), DefaultState.LastSettlementUtcTicks, StartTicks);
	TestEqual(TEXT("A default cave has one record per building"), DefaultState.Buildings.Num(), 7);
	for (const EImmortalCaveBuildingType Type : BuildingTypes)
	{
		TestEqual(TEXT("Every cave building starts at level one"),
			UImmortalCaveLibrary::GetBuildingLevel(DefaultState, Type), 1);
	}
	const FImmortalCaveProductionSnapshot DefaultProduction =
		UImmortalCaveLibrary::GetProductionSnapshot(DefaultState);
	TestTrue(TEXT("Level-one spirit vein produces exactly 30 stones per hour"),
		FMath::IsNearlyEqual(DefaultProduction.SpiritStonesPerHour, 30.0));
	TestTrue(TEXT("Level-one spirit field produces exactly 0.75 grass per hour"),
		FMath::IsNearlyEqual(DefaultProduction.SpiritGrassPerHour, 0.75));
	TestTrue(TEXT("Level-one spirit vein produces exactly 0.5 ore per hour"),
		FMath::IsNearlyEqual(DefaultProduction.OrePerHour, 0.5));
	TestTrue(TEXT("Level-one storage holds eight hours"),
		FMath::IsNearlyEqual(DefaultProduction.StorageHours, 8.0));
	TestEqual(TEXT("Default stone capacity is eight hours of output"),
		DefaultProduction.SpiritStoneCapacity, 240);
	TestEqual(TEXT("Default grass capacity rounds up to six whole items"),
		DefaultProduction.SpiritGrassCapacity, 6);
	TestEqual(TEXT("Default ore capacity is four whole items"),
		DefaultProduction.OreCapacity, 4);
	TestTrue(TEXT("Level-one meditation room gives a 1.05 cultivation multiplier"),
		FMath::IsNearlyEqual(DefaultProduction.CultivationRateMultiplier, 1.05f));

	// Duplicate, unknown, missing and out-of-range records are repaired into a
	// deterministic seven-entry representation, then a second pass is a no-op.
	FImmortalCaveState DirtyState;
	DirtyState.bInitialized = true;
	DirtyState.LastSettlementUtcTicks = StartTicks;
	DirtyState.Revision = 5;
	DirtyState.Buildings = {
		{EImmortalCaveBuildingType::CaveHeart, 2},
		{EImmortalCaveBuildingType::CaveHeart, 3},
		{EImmortalCaveBuildingType::MeditationRoom, 99},
		{EImmortalCaveBuildingType::SpiritVein, -5},
		{static_cast<EImmortalCaveBuildingType>(255), 10}
	};
	DirtyState.StoredSpiritStones = MAX_int32;
	DirtyState.SpiritStoneFraction = 0.75;
	DirtyState.StoredSpiritGrass = -3;
	DirtyState.SpiritGrassFraction = 2.25;
	DirtyState.StoredOre = 1;
	DirtyState.OreFraction = -0.5;
	DirtyState.TotalOreProduced = -1;
	DirtyState.CollectionCount = -2;
	TestTrue(TEXT("Dirty cave normalization reports a state change"),
		UImmortalCaveLibrary::NormalizeState(DirtyState, StartTicks));
	TestEqual(TEXT("Normalization restores exactly seven buildings"), DirtyState.Buildings.Num(), 7);
	TestEqual(TEXT("Duplicate core records keep the highest valid level"),
		UImmortalCaveLibrary::GetBuildingLevel(DirtyState, EImmortalCaveBuildingType::CaveHeart), 3);
	TestEqual(TEXT("A non-core building is capped by the cave heart"),
		UImmortalCaveLibrary::GetBuildingLevel(DirtyState, EImmortalCaveBuildingType::MeditationRoom), 3);
	TestEqual(TEXT("A negative loaded building level becomes one"),
		UImmortalCaveLibrary::GetBuildingLevel(DirtyState, EImmortalCaveBuildingType::SpiritVein), 1);
	TestEqual(TEXT("A missing building is restored at level one"),
		UImmortalCaveLibrary::GetBuildingLevel(DirtyState, EImmortalCaveBuildingType::ForgeRoom), 1);
	const FImmortalCaveProductionSnapshot DirtyProduction = UImmortalCaveLibrary::GetProductionSnapshot(DirtyState);
	TestEqual(TEXT("Over-cap stone storage is clamped to the current capacity"),
		DirtyState.StoredSpiritStones, DirtyProduction.SpiritStoneCapacity);
	TestTrue(TEXT("A full normalized store drops its fractional progress"),
		FMath::IsNearlyZero(DirtyState.SpiritStoneFraction));
	TestEqual(TEXT("Whole grass fractional units are moved into storage"), DirtyState.StoredSpiritGrass, 2);
	TestTrue(TEXT("The remaining grass fraction is preserved"),
		FMath::IsNearlyEqual(DirtyState.SpiritGrassFraction, 0.25));
	TestTrue(TEXT("A negative ore fraction becomes zero"), FMath::IsNearlyZero(DirtyState.OreFraction));
	TestEqual(TEXT("A negative total counter becomes zero"), DirtyState.TotalOreProduced, int64(0));
	TestEqual(TEXT("A negative collection count becomes zero"), DirtyState.CollectionCount, 0);
	const FImmortalCaveState NormalizedOnce = DirtyState;
	TestFalse(TEXT("Canonical cave normalization is idempotent"),
		UImmortalCaveLibrary::NormalizeState(DirtyState, StartTicks));
	TestTrue(TEXT("A second normalization changes no cave fields"),
		AreCaveStatesEqual(DirtyState, NormalizedOnce));

	// At level one, every non-core upgrade is gated until the heart advances.
	TArray<FImmortalMaterialStack> RichMaterials;
	for (const FName MaterialId : UImmortalMaterialLibrary::GetKnownMaterialIds())
	{
		UImmortalMaterialLibrary::AddMaterialStack(RichMaterials, MaterialId, 999999);
	}
	const FImmortalCaveUpgradeResult GatedUpgrade = UImmortalCaveLibrary::EvaluateUpgrade(
		DefaultState,
		EImmortalCaveBuildingType::MeditationRoom,
		RichMaterials,
		MAX_int32);
	TestTrue(TEXT("A level-two meditation room is blocked by a level-one heart"),
		GatedUpgrade.bBlockedByCaveHeart);
	TestFalse(TEXT("A heart-gated upgrade cannot proceed despite abundant resources"),
		GatedUpgrade.bCanUpgrade);

	FImmortalCaveState MaximumState = DefaultState;
	for (const EImmortalCaveBuildingType Type : BuildingTypes) SetBuildingLevel(MaximumState, Type, 20);
	for (const EImmortalCaveBuildingType Type : BuildingTypes)
	{
		const FImmortalCaveUpgradeResult MaximumResult = UImmortalCaveLibrary::EvaluateUpgrade(
			MaximumState, Type, RichMaterials, MAX_int32);
		TestTrue(TEXT("Every level-twenty cave building reports maximum level"),
			MaximumResult.bAtMaximumLevel);
		TestFalse(TEXT("A maximum-level cave building cannot upgrade"), MaximumResult.bCanUpgrade);
		TestEqual(TEXT("A maximum-level cave building has no stone upgrade cost"),
			MaximumResult.Cost.SpiritStones, 0);
	}

	// Every building's level-two-to-three cost is strictly above its
	// level-one-to-two cost, and generated material IDs never duplicate.
	for (const EImmortalCaveBuildingType Type : BuildingTypes)
	{
		FImmortalCaveState CostState = DefaultState;
		if (Type != EImmortalCaveBuildingType::CaveHeart)
		{
			SetBuildingLevel(CostState, EImmortalCaveBuildingType::CaveHeart, 20);
		}
		const FImmortalCraftingCost FirstCost = UImmortalCaveLibrary::GetUpgradeCost(CostState, Type);
		SetBuildingLevel(CostState, Type, 2);
		const FImmortalCraftingCost SecondCost = UImmortalCaveLibrary::GetUpgradeCost(CostState, Type);
		TestTrue(TEXT("Cave upgrade stone costs increase with level"),
			SecondCost.SpiritStones > FirstCost.SpiritStones);
		TestTrue(TEXT("Cave upgrade material quantities increase with level"),
			GetTotalMaterialCost(SecondCost) > GetTotalMaterialCost(FirstCost));
		TestTrue(TEXT("First cave cost contains unique valid material IDs"),
			HasUniqueValidMaterialIds(FirstCost));
		TestTrue(TEXT("Second cave cost contains unique valid material IDs"),
			HasUniqueValidMaterialIds(SecondCost));
	}

	// Insufficient upgrades preserve all three transaction authorities exactly.
	FImmortalCaveState RejectedState = DefaultState;
	TArray<FImmortalMaterialStack> RejectedMaterials;
	RejectedMaterials.Add({TEXT("Ore"), 3});
	int32 RejectedStones = 199;
	const FImmortalCaveState BeforeRejectedState = RejectedState;
	const TArray<FImmortalMaterialStack> BeforeRejectedMaterials = RejectedMaterials;
	const FImmortalCaveUpgradeResult RejectedResult = UImmortalCaveLibrary::TryUpgradeBuilding(
		RejectedState, EImmortalCaveBuildingType::CaveHeart, RejectedMaterials, RejectedStones);
	TestFalse(TEXT("An incomplete cave upgrade transaction fails"), RejectedResult.bSucceeded);
	TestTrue(TEXT("A rejected upgrade preserves the complete cave state"),
		AreCaveStatesEqual(RejectedState, BeforeRejectedState));
	TestTrue(TEXT("A rejected upgrade preserves the material inventory"),
		AreMaterialInventoriesEqual(RejectedMaterials, BeforeRejectedMaterials));
	TestEqual(TEXT("A rejected upgrade preserves spirit stones"), RejectedStones, 199);

	// A successful heart upgrade consumes exactly its documented 200 stones and four ore.
	FImmortalCaveState UpgradedState = DefaultState;
	TArray<FImmortalMaterialStack> UpgradeMaterials;
	UpgradeMaterials.Add({TEXT("Ore"), 2});
	UpgradeMaterials.Add({TEXT("Ore"), 3});
	UpgradeMaterials.Add({TEXT("SpiritGrass"), 7});
	int32 UpgradeStones = 1000;
	const int32 UpgradeRevision = UpgradedState.Revision;
	const FImmortalCaveUpgradeResult UpgradeResult = UImmortalCaveLibrary::TryUpgradeBuilding(
		UpgradedState, EImmortalCaveBuildingType::CaveHeart, UpgradeMaterials, UpgradeStones);
	TestTrue(TEXT("An affordable cave-heart upgrade succeeds"), UpgradeResult.bSucceeded);
	TestEqual(TEXT("A successful result retains the previous level"), UpgradeResult.CurrentLevel, 1);
	TestEqual(TEXT("A successful result reports the target level"), UpgradeResult.TargetLevel, 2);
	TestEqual(TEXT("The cave heart reaches level two"),
		UImmortalCaveLibrary::GetBuildingLevel(UpgradedState, EImmortalCaveBuildingType::CaveHeart), 2);
	TestEqual(TEXT("A heart upgrade consumes exactly 200 spirit stones"), UpgradeStones, 800);
	TestEqual(TEXT("A heart upgrade consumes exactly four ore across duplicate stacks"),
		UImmortalMaterialLibrary::GetMaterialQuantity(UpgradeMaterials, TEXT("Ore")), 1);
	TestEqual(TEXT("A heart upgrade preserves unrelated materials"),
		UImmortalMaterialLibrary::GetMaterialQuantity(UpgradeMaterials, TEXT("SpiritGrass")), 7);
	TestEqual(TEXT("A successful upgrade advances the cave revision once"),
		UpgradedState.Revision, UpgradeRevision + 1);

	// One 90-minute settlement and two segmented settlements must produce the
	// same whole resources and fractions (revision counts intentionally differ).
	FImmortalCaveState OneShotState = DefaultState;
	FImmortalCaveState SegmentedState = DefaultState;
	const int64 ThirtyMinutes = FTimespan::FromMinutes(30.0).GetTicks();
	const int64 NinetyMinutes = FTimespan::FromMinutes(90.0).GetTicks();
	UImmortalCaveLibrary::SettleProduction(OneShotState, StartTicks + NinetyMinutes);
	UImmortalCaveLibrary::SettleProduction(SegmentedState, StartTicks + ThirtyMinutes);
	UImmortalCaveLibrary::SettleProduction(SegmentedState, StartTicks + NinetyMinutes);
	TestTrue(TEXT("Segmented cave settlement equals one-shot production"),
		AreProductionValuesNearlyEqual(OneShotState, SegmentedState));
	TestEqual(TEXT("Ninety minutes produces 45 spirit stones"), OneShotState.StoredSpiritStones, 45);
	TestEqual(TEXT("Ninety minutes produces one whole spirit grass"), OneShotState.StoredSpiritGrass, 1);
	TestEqual(TEXT("Ninety minutes has not yet produced one whole ore"), OneShotState.StoredOre, 0);
	TestTrue(TEXT("Grass keeps its exact one-eighth remainder"),
		FMath::IsNearlyEqual(OneShotState.SpiritGrassFraction, 0.125, 1.0e-9));
	TestTrue(TEXT("Ore keeps its exact three-quarter remainder"),
		FMath::IsNearlyEqual(OneShotState.OreFraction, 0.75, 1.0e-9));

	// Long settlement clamps every store, then full stores still advance their
	// high-water time and discard both overflow and fractions.
	FImmortalCaveState CapacityState = DefaultState;
	const int64 OneHundredHours = FTimespan::FromHours(100.0).GetTicks();
	UImmortalCaveLibrary::SettleProduction(CapacityState, StartTicks + OneHundredHours);
	TestEqual(TEXT("Long settlement clamps stone storage"),
		CapacityState.StoredSpiritStones, DefaultProduction.SpiritStoneCapacity);
	TestEqual(TEXT("Long settlement clamps grass storage"),
		CapacityState.StoredSpiritGrass, DefaultProduction.SpiritGrassCapacity);
	TestEqual(TEXT("Long settlement clamps ore storage"),
		CapacityState.StoredOre, DefaultProduction.OreCapacity);
	TestTrue(TEXT("A full stone store has no fractional backlog"),
		FMath::IsNearlyZero(CapacityState.SpiritStoneFraction));
	const int64 FullStoreNextTicks = StartTicks + FTimespan::FromHours(101.0).GetTicks();
	const FImmortalCaveSettlementResult FullStoreResult =
		UImmortalCaveLibrary::SettleProduction(CapacityState, FullStoreNextTicks);
	TestEqual(TEXT("A full cave still advances the settlement high-water mark"),
		CapacityState.LastSettlementUtcTicks, FullStoreNextTicks);
	TestEqual(TEXT("A full cave accepts no additional stones"), FullStoreResult.AddedSpiritStones, 0);
	TestTrue(TEXT("A full cave records discarded stone overflow"), FullStoreResult.DiscardedSpiritStones > 0);
	const FImmortalCaveState SameTimestampState = CapacityState;
	const FImmortalCaveSettlementResult SameTimestampResult =
		UImmortalCaveLibrary::SettleProduction(CapacityState, FullStoreNextTicks);
	TestFalse(TEXT("Settling the same timestamp is a no-op"), SameTimestampResult.bStateChanged);
	TestTrue(TEXT("Same-timestamp settlement changes no fields"),
		AreCaveStatesEqual(CapacityState, SameTimestampState));

	// Rolling the clock backwards preserves the old high-water mark. Recovery
	// therefore pays only time after that mark, never the rolled-back interval.
	FImmortalCaveState RollbackState = DefaultState;
	const int64 OneHour = FTimespan::FromHours(1.0).GetTicks();
	UImmortalCaveLibrary::SettleProduction(RollbackState, StartTicks + 2 * OneHour);
	const FImmortalCaveState BeforeRollback = RollbackState;
	const FImmortalCaveSettlementResult RollbackResult =
		UImmortalCaveLibrary::SettleProduction(RollbackState, StartTicks + OneHour);
	TestTrue(TEXT("A backwards clock is detected"), RollbackResult.bClockRollbackDetected);
	TestEqual(TEXT("Clock rollback does not lower the settlement high-water mark"),
		RollbackState.LastSettlementUtcTicks, BeforeRollback.LastSettlementUtcTicks);
	TestEqual(TEXT("Clock rollback grants no spirit stones"),
		RollbackState.StoredSpiritStones, BeforeRollback.StoredSpiritStones);
	UImmortalCaveLibrary::SettleProduction(RollbackState, StartTicks + 3 * OneHour);
	TestEqual(TEXT("Clock recovery pays only one new hour after the old mark"),
		RollbackState.StoredSpiritStones, 90);

	// Collection is intentionally partial when player currency/material stacks
	// are nearly full; uncollected resources stay safely inside the cave.
	FImmortalCaveState CollectionState = DefaultState;
	CollectionState.StoredSpiritStones = 10;
	CollectionState.StoredSpiritGrass = 5;
	CollectionState.StoredOre = 3;
	TArray<FImmortalMaterialStack> CollectionMaterials;
	CollectionMaterials.Add({TEXT("SpiritGrass"), 999998});
	CollectionMaterials.Add({TEXT("Ore"), 999999});
	int32 CollectionStones = MAX_int32 - 4;
	const FImmortalCaveCollectionResult CollectionResult = UImmortalCaveLibrary::CollectStoredResources(
		CollectionState, CollectionMaterials, CollectionStones);
	TestTrue(TEXT("A partial cave collection reports success"), CollectionResult.bCollectedAnything);
	TestEqual(TEXT("Only four stones fit in the player wallet"), CollectionResult.SpiritStonesCollected, 4);
	TestEqual(TEXT("Only one spirit grass fits in the player stack"), CollectionResult.SpiritGrassCollected, 1);
	TestEqual(TEXT("No ore fits in a full player stack"), CollectionResult.OreCollected, 0);
	TestEqual(TEXT("Uncollected stones remain stored"), CollectionState.StoredSpiritStones, 6);
	TestEqual(TEXT("Uncollected grass remains stored"), CollectionState.StoredSpiritGrass, 4);
	TestEqual(TEXT("Uncollected ore remains stored"), CollectionState.StoredOre, 3);
	TestEqual(TEXT("The player wallet reaches its exact integer capacity"), CollectionStones, MAX_int32);
	TestEqual(TEXT("The player grass stack reaches its exact material capacity"),
		UImmortalMaterialLibrary::GetMaterialQuantity(CollectionMaterials, TEXT("SpiritGrass")), 999999);

	FImmortalCraftingCost ForgeCost;
	ForgeCost.SpiritStones = 100;
	ForgeCost.Materials.Add({TEXT("Ore"), 4});
	const FImmortalCraftingCost DefaultForgeCost =
		UImmortalCaveLibrary::ApplyForgeDiscount(ForgeCost, DefaultState);
	TestEqual(TEXT("A level-one forge gives no spirit-stone discount"), DefaultForgeCost.SpiritStones, 100);
	const FImmortalCraftingCost MaximumForgeCost =
		UImmortalCaveLibrary::ApplyForgeDiscount(ForgeCost, MaximumState);
	TestEqual(TEXT("Forge discount is capped at exactly twenty-five percent"),
		MaximumForgeCost.SpiritStones, 75);
	TestEqual(TEXT("Forge discount never changes material quantities"),
		MaximumForgeCost.Materials[0].Quantity, 4);
	FImmortalCraftingCost ExtremeForgeCost;
	ExtremeForgeCost.SpiritStones = MAX_int32;
	TestEqual(TEXT("Zero discount preserves an extreme int32 spirit-stone cost exactly"),
		UImmortalCaveLibrary::ApplyForgeDiscount(ExtremeForgeCost, DefaultState).SpiritStones,
		MAX_int32);
	TestEqual(TEXT("Extreme spirit-stone discounts use precise saturating arithmetic"),
		UImmortalCaveLibrary::ApplyForgeDiscount(ExtremeForgeCost, MaximumState).SpiritStones,
		1610612736);
	return true;
}

#endif
