// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalFarmingTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../Save/ImmortalPathSaveGame.h"
#include "Misc/AutomationTest.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"

namespace
{
	bool ArePlotStatesEqual(
		const FImmortalFarmingPlotState& Left,
		const FImmortalFarmingPlotState& Right)
	{
		return Left.CropId == Right.CropId
			&& Left.TotalGrowthTicks == Right.TotalGrowthTicks
			&& Left.RemainingGrowthTicks == Right.RemainingGrowthTicks
			&& Left.GrowthProgressPermilleRemainder == Right.GrowthProgressPermilleRemainder
			&& Left.PendingYield == Right.PendingYield
			&& Left.PlantedSpiritFieldLevel == Right.PlantedSpiritFieldLevel;
	}

	bool AreFarmingStatesEqual(
		const FImmortalFarmingState& Left,
		const FImmortalFarmingState& Right,
		const bool bIgnoreRevision = false)
	{
		if (Left.bInitialized != Right.bInitialized
			|| Left.LastSettlementUtcTicks != Right.LastSettlementUtcTicks
			|| Left.TotalCropsPlanted != Right.TotalCropsPlanted
			|| Left.TotalCropsHarvested != Right.TotalCropsHarvested
			|| Left.TotalItemsHarvested != Right.TotalItemsHarvested
			|| (!bIgnoreRevision && Left.Revision != Right.Revision)
			|| Left.Plots.Num() != Right.Plots.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Plots.Num(); ++Index)
		{
			if (!ArePlotStatesEqual(Left.Plots[Index], Right.Plots[Index])) return false;
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

	int32 GetMaterialCostQuantity(
		const FImmortalCraftingCost& Cost,
		const FName MaterialId)
	{
		int32 Total = 0;
		for (const FImmortalCraftingMaterialCost& Entry : Cost.Materials)
		{
			if (Entry.MaterialId == MaterialId) Total += Entry.Quantity;
		}
		return Total;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalFarmingCatalogStateTransactionTest,
	"ImmortalPath.Farming.CatalogStateAndPlantTransactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalFarmingCatalogStateTransactionTest::RunTest(const FString& Parameters)
{
	const int64 StartTicks = FDateTime(2026, 7, 18, 0, 0, 0).GetTicks();
	TestEqual(TEXT("SaveGame version is v17 after equipment expansion was added"),
		UImmortalPathSaveGame::CurrentSaveVersion, 17);

	const TArray<FName> CropIds = UImmortalFarmingLibrary::GetKnownCropIds();
	TestEqual(TEXT("The farming catalog contains exactly three stable crops"), CropIds.Num(), 3);
	TestEqual(TEXT("The first stable crop is spirit grass"), CropIds[0], FName(TEXT("SpiritGrassCrop")));
	TestEqual(TEXT("The second stable crop is immortal fruit"), CropIds[1], FName(TEXT("ImmortalFruitCrop")));
	TestEqual(TEXT("The third stable crop is spirit wood"), CropIds[2], FName(TEXT("SpiritWoodCrop")));
	TSet<FName> UniqueCropIds;
	for (const FName CropId : CropIds)
	{
		FImmortalFarmingCropDefinition Definition;
		TestTrue(*FString::Printf(TEXT("Definition exists for %s"), *CropId.ToString()),
			UImmortalFarmingLibrary::GetCropDefinition(CropId, Definition));
		TestFalse(TEXT("Every crop has a display name"), Definition.DisplayName.IsEmpty());
		TestFalse(TEXT("Every crop has a description"), Definition.Description.IsEmpty());
		TestFalse(TEXT("Every crop has an output material"), Definition.OutputMaterialId.IsNone());
		FImmortalMaterialDefinition MaterialDefinition;
		TestTrue(TEXT("Every crop output resolves through the material catalog"),
			UImmortalMaterialLibrary::GetMaterialDefinition(Definition.OutputMaterialId, MaterialDefinition));
		TestTrue(TEXT("Every crop duration is positive"), Definition.BaseGrowthSeconds > 0);
		TestTrue(TEXT("Every crop yield range is valid"),
			Definition.MinimumBaseYield > 0
			&& Definition.MaximumBaseYield >= Definition.MinimumBaseYield);
		TestFalse(TEXT("Crop identifiers are unique"), UniqueCropIds.Contains(CropId));
		UniqueCropIds.Add(CropId);
	}

	FImmortalFarmingCropDefinition GrassDefinition;
	FImmortalFarmingCropDefinition FruitDefinition;
	FImmortalFarmingCropDefinition WoodDefinition;
	UImmortalFarmingLibrary::GetCropDefinition(TEXT("SpiritGrassCrop"), GrassDefinition);
	UImmortalFarmingLibrary::GetCropDefinition(TEXT("ImmortalFruitCrop"), FruitDefinition);
	UImmortalFarmingLibrary::GetCropDefinition(TEXT("SpiritWoodCrop"), WoodDefinition);
	TestEqual(TEXT("Spirit grass grows in five minutes"), GrassDefinition.BaseGrowthSeconds, 300);
	TestEqual(TEXT("Immortal fruit grows in fifteen minutes"), FruitDefinition.BaseGrowthSeconds, 900);
	TestEqual(TEXT("Spirit wood grows in thirty minutes"), WoodDefinition.BaseGrowthSeconds, 1800);
	TestEqual(TEXT("Spirit grass costs three stones"), GrassDefinition.PlantingCost.SpiritStones, 3);
	TestEqual(TEXT("Immortal fruit costs eight stones"), FruitDefinition.PlantingCost.SpiritStones, 8);
	TestEqual(TEXT("Immortal fruit costs one grass"),
		GetMaterialCostQuantity(FruitDefinition.PlantingCost, TEXT("SpiritGrass")), 1);
	TestEqual(TEXT("Spirit wood costs twelve stones"), WoodDefinition.PlantingCost.SpiritStones, 12);
	TestEqual(TEXT("Spirit wood costs one grass"),
		GetMaterialCostQuantity(WoodDefinition.PlantingCost, TEXT("SpiritGrass")), 1);
	TestEqual(TEXT("Spirit wood costs one ore"),
		GetMaterialCostQuantity(WoodDefinition.PlantingCost, TEXT("Ore")), 1);
	TestEqual(TEXT("Unknown crop IDs are rejected"),
		UImmortalFarmingLibrary::GetCropDefinition(TEXT("UnknownCrop"), GrassDefinition), false);

	TestEqual(TEXT("There are always six persistent plots"),
		UImmortalFarmingLibrary::GetMaximumPlotCount(), 6);
	TestEqual(TEXT("An invalid field level unlocks no plots"),
		UImmortalFarmingLibrary::GetUnlockedPlotCount(0), 0);
	for (const TPair<int32, int32>& Pair : TArray<TPair<int32, int32>>{
		{1, 2}, {4, 2}, {5, 3}, {8, 3}, {9, 4}, {12, 4},
		{13, 5}, {16, 5}, {17, 6}, {20, 6}})
	{
		TestEqual(
			*FString::Printf(TEXT("Field level %d unlocks the documented plot count"), Pair.Key),
			UImmortalFarmingLibrary::GetUnlockedPlotCount(Pair.Key),
			Pair.Value);
	}
	const TArray<int32> RequiredLevels = {1, 1, 5, 9, 13, 17};
	for (int32 PlotIndex = 0; PlotIndex < RequiredLevels.Num(); ++PlotIndex)
	{
		TestEqual(TEXT("Each stable plot has the documented field-level gate"),
			UImmortalFarmingLibrary::GetRequiredFieldLevelForPlot(PlotIndex), RequiredLevels[PlotIndex]);
	}
	TestEqual(TEXT("An invalid plot has no field-level gate"),
		UImmortalFarmingLibrary::GetRequiredFieldLevelForPlot(6), 0);
	TestTrue(TEXT("Level-one field speed is exactly 1.0"),
		FMath::IsNearlyEqual(UImmortalFarmingLibrary::GetGrowthSpeedMultiplier(1), 1.0f));
	TestTrue(TEXT("Level-twenty field speed is exactly 1.57"),
		FMath::IsNearlyEqual(UImmortalFarmingLibrary::GetGrowthSpeedMultiplier(20), 1.57f));

	const FImmortalFarmingState DefaultState = UImmortalFarmingLibrary::CreateDefaultState(StartTicks);
	TestTrue(TEXT("A default farm is initialized"), DefaultState.bInitialized);
	TestEqual(TEXT("A default farm preserves its supplied high-water mark"),
		DefaultState.LastSettlementUtcTicks, StartTicks);
	TestEqual(TEXT("A default farm has six records"), DefaultState.Plots.Num(), 6);
	TestEqual(TEXT("A default farm begins at revision one"), DefaultState.Revision, 1);
	for (const FImmortalFarmingPlotState& Plot : DefaultState.Plots)
	{
		TestTrue(TEXT("Every default plot is empty"), Plot.IsEmpty());
	}

	FImmortalFarmingState LegacyState;
	TestTrue(TEXT("An uninitialized legacy state migrates to a default farm"),
		UImmortalFarmingLibrary::NormalizeState(LegacyState, StartTicks));
	TestTrue(TEXT("A migrated legacy farm is initialized"), LegacyState.bInitialized);
	TestEqual(TEXT("Migration starts at current UTC rather than granting historical growth"),
		LegacyState.LastSettlementUtcTicks, StartTicks);
	TestEqual(TEXT("Migration creates six empty plots"), LegacyState.Plots.Num(), 6);

	FImmortalFarmingState DirtyState;
	DirtyState.bInitialized = true;
	DirtyState.LastSettlementUtcTicks = 0;
	DirtyState.TotalCropsPlanted = -1;
	DirtyState.TotalCropsHarvested = -2;
	DirtyState.TotalItemsHarvested = -3;
	DirtyState.Revision = -4;
	DirtyState.Plots.SetNum(8);
	DirtyState.Plots[0].CropId = TEXT("UnknownCrop");
	DirtyState.Plots[0].PendingYield = 99;
	DirtyState.Plots[1].CropId = TEXT("SpiritGrassCrop");
	DirtyState.Plots[1].TotalGrowthTicks = -5;
	DirtyState.Plots[1].RemainingGrowthTicks = MAX_int64;
	DirtyState.Plots[1].GrowthProgressPermilleRemainder = 5000;
	DirtyState.Plots[1].PendingYield = MAX_int32;
	DirtyState.Plots[1].PlantedSpiritFieldLevel = 99;
	DirtyState.Plots[2].CropId = NAME_None;
	DirtyState.Plots[2].PendingYield = 5;
	DirtyState.Plots[2].RemainingGrowthTicks = 7;
	TestTrue(TEXT("Dirty farming state reports normalization"),
		UImmortalFarmingLibrary::NormalizeState(DirtyState, StartTicks));
	TestEqual(TEXT("Dirty farming arrays are repaired to six records"), DirtyState.Plots.Num(), 6);
	TestTrue(TEXT("Unknown crops are removed"), DirtyState.Plots[0].IsEmpty());
	TestTrue(TEXT("Empty plots lose stray growth and yield fields"),
		DirtyState.Plots[2].IsEmpty()
		&& DirtyState.Plots[2].PendingYield == 0
		&& DirtyState.Plots[2].RemainingGrowthTicks == 0);
	TestEqual(TEXT("Known crop duration is repaired"),
		DirtyState.Plots[1].TotalGrowthTicks,
		static_cast<int64>(300) * ETimespan::TicksPerSecond);
	TestEqual(TEXT("Known crop remaining time is clamped"),
		DirtyState.Plots[1].RemainingGrowthTicks, DirtyState.Plots[1].TotalGrowthTicks);
	TestEqual(TEXT("Loaded field level is clamped to twenty"),
		DirtyState.Plots[1].PlantedSpiritFieldLevel, 20);
	TestEqual(TEXT("Tampered yield is clamped to the maximum level-twenty yield"),
		DirtyState.Plots[1].PendingYield, 8);
	TestEqual(TEXT("Permille carry is clamped to 999"),
		DirtyState.Plots[1].GrowthProgressPermilleRemainder, 999);
	TestEqual(TEXT("Negative audit counters are repaired"), DirtyState.TotalItemsHarvested, int64(0));
	TestEqual(TEXT("Missing high-water time is repaired"), DirtyState.LastSettlementUtcTicks, StartTicks);
	const FImmortalFarmingState NormalizedOnce = DirtyState;
	TestFalse(TEXT("Farming normalization is idempotent"),
		UImmortalFarmingLibrary::NormalizeState(DirtyState, StartTicks));
	TestTrue(TEXT("A second normalization changes no fields"),
		AreFarmingStatesEqual(DirtyState, NormalizedOnce));

	FImmortalFarmingState PlantState = DefaultState;
	TArray<FImmortalMaterialStack> PlantMaterials;
	int32 PlantStones = 3;
	const FImmortalFarmingPlantResult GrassPlant = UImmortalFarmingLibrary::TryPlantCrop(
		PlantState, 0, TEXT("SpiritGrassCrop"), 1, PlantMaterials, PlantStones, StartTicks);
	TestTrue(TEXT("An affordable spirit-grass planting succeeds"), GrassPlant.bSucceeded);
	TestFalse(TEXT("Core planting never claims a persistence failure"), GrassPlant.bPersistenceFailed);
	TestEqual(TEXT("Spirit-grass planting consumes exactly three stones"), PlantStones, 0);
	TestEqual(TEXT("The crop is stored on the selected plot"),
		PlantState.Plots[0].CropId, FName(TEXT("SpiritGrassCrop")));
	TestEqual(TEXT("The formal five-minute duration is frozen"),
		PlantState.Plots[0].RemainingGrowthTicks,
		static_cast<int64>(300) * ETimespan::TicksPerSecond);
	TestTrue(TEXT("Level-one grass yield is frozen inside its 3-4 range"),
		PlantState.Plots[0].PendingYield >= 3 && PlantState.Plots[0].PendingYield <= 4);

	const FImmortalFarmingState BeforeOccupiedPlant = PlantState;
	const TArray<FImmortalMaterialStack> BeforeOccupiedMaterials = PlantMaterials;
	const int32 BeforeOccupiedStones = PlantStones;
	const FImmortalFarmingPlantResult OccupiedPlant = UImmortalFarmingLibrary::TryPlantCrop(
		PlantState, 0, TEXT("SpiritGrassCrop"), 1, PlantMaterials, PlantStones, StartTicks);
	TestFalse(TEXT("An occupied plot rejects another crop"), OccupiedPlant.bSucceeded);
	TestTrue(TEXT("Rejected occupied planting preserves farming state"),
		AreFarmingStatesEqual(PlantState, BeforeOccupiedPlant));
	TestTrue(TEXT("Rejected occupied planting preserves materials"),
		AreMaterialInventoriesEqual(PlantMaterials, BeforeOccupiedMaterials));
	TestEqual(TEXT("Rejected occupied planting preserves stones"), PlantStones, BeforeOccupiedStones);

	FImmortalFarmingState LockedState = DefaultState;
	TArray<FImmortalMaterialStack> LockedMaterials;
	int32 LockedStones = 100;
	const FImmortalFarmingPlantResult LockedPlot = UImmortalFarmingLibrary::TryPlantCrop(
		LockedState, 2, TEXT("SpiritGrassCrop"), 1, LockedMaterials, LockedStones, StartTicks);
	TestFalse(TEXT("Plot three is locked below field level five"), LockedPlot.bSucceeded);
	TestFalse(TEXT("Locked result reports that the plot is unavailable"), LockedPlot.bPlotUnlocked);
	const FImmortalFarmingPlantResult LockedCrop = UImmortalFarmingLibrary::TryPlantCrop(
		LockedState, 0, TEXT("ImmortalFruitCrop"), 1, LockedMaterials, LockedStones, StartTicks);
	TestFalse(TEXT("Immortal fruit is locked below field level five"), LockedCrop.bSucceeded);
	TestFalse(TEXT("Locked result reports that the crop is unavailable"), LockedCrop.bCropUnlocked);

	FImmortalFarmingState InsufficientState = DefaultState;
	TArray<FImmortalMaterialStack> InsufficientMaterials;
	int32 InsufficientStones = 8;
	const FImmortalFarmingState BeforeInsufficientState = InsufficientState;
	const FImmortalFarmingPlantResult InsufficientFruit = UImmortalFarmingLibrary::TryPlantCrop(
		InsufficientState, 0, TEXT("ImmortalFruitCrop"), 5,
		InsufficientMaterials, InsufficientStones, StartTicks);
	TestFalse(TEXT("Fruit planting fails without its grass cost"), InsufficientFruit.bSucceeded);
	TestTrue(TEXT("Insufficient planting preserves farming state exactly"),
		AreFarmingStatesEqual(InsufficientState, BeforeInsufficientState));
	TestEqual(TEXT("Insufficient planting preserves stones"), InsufficientStones, 8);

	FImmortalFarmingState FruitState = DefaultState;
	TArray<FImmortalMaterialStack> FruitMaterials = {
		{TEXT("SpiritGrass"), 1}, {TEXT("SpiritGrass"), 1}, {TEXT("Ore"), 4}};
	int32 FruitStones = 20;
	const FImmortalFarmingPlantResult FruitPlant = UImmortalFarmingLibrary::TryPlantCrop(
		FruitState, 0, TEXT("ImmortalFruitCrop"), 5,
		FruitMaterials, FruitStones, StartTicks);
	TestTrue(TEXT("Fruit planting consumes across duplicate inventory stacks"), FruitPlant.bSucceeded);
	TestEqual(TEXT("Fruit planting consumes exactly eight stones"), FruitStones, 12);
	TestEqual(TEXT("Fruit planting consumes exactly one grass"),
		UImmortalMaterialLibrary::GetMaterialQuantity(FruitMaterials, TEXT("SpiritGrass")), 1);
	TestEqual(TEXT("Fruit planting preserves unrelated ore"),
		UImmortalMaterialLibrary::GetMaterialQuantity(FruitMaterials, TEXT("Ore")), 4);
	TestTrue(TEXT("Level-five fruit freezes base yield plus one level bonus"),
		FruitState.Plots[0].PendingYield >= 3 && FruitState.Plots[0].PendingYield <= 4);

	FImmortalFarmingState RollbackPlantState = UImmortalFarmingLibrary::CreateDefaultState(
		StartTicks + FTimespan::FromHours(1.0).GetTicks());
	TArray<FImmortalMaterialStack> RollbackPlantMaterials;
	int32 RollbackPlantStones = 3;
	const FImmortalFarmingState BeforeRollbackPlant = RollbackPlantState;
	const FImmortalFarmingPlantResult RollbackPlant = UImmortalFarmingLibrary::TryPlantCrop(
		RollbackPlantState, 0, TEXT("SpiritGrassCrop"), 1,
		RollbackPlantMaterials, RollbackPlantStones, StartTicks);
	TestFalse(TEXT("Clock rollback blocks planting"), RollbackPlant.bSucceeded);
	TestTrue(TEXT("Clock rollback is exposed to the caller"), RollbackPlant.bClockRollbackDetected);
	TestTrue(TEXT("Blocked rollback planting changes no farming fields"),
		AreFarmingStatesEqual(RollbackPlantState, BeforeRollbackPlant));
	TestEqual(TEXT("Blocked rollback planting charges no stones"), RollbackPlantStones, 3);

	FImmortalFarmingState BatchState = DefaultState;
	TArray<FImmortalMaterialStack> BatchMaterials;
	UImmortalMaterialLibrary::AddMaterialStack(BatchMaterials, TEXT("SpiritGrass"), 2);
	int32 BatchStones = 16;
	const FImmortalFarmingBatchPlantResult BatchPlant = UImmortalFarmingLibrary::TryPlantAllEmpty(
		BatchState, TEXT("ImmortalFruitCrop"), 5, BatchMaterials, BatchStones, StartTicks);
	TestTrue(TEXT("Batch planting succeeds when at least one plot is affordable"), BatchPlant.bSucceeded);
	TestFalse(TEXT("A partially affordable batch is not reported as complete"),
		BatchPlant.bAllEligiblePlotsPlanted);
	TestEqual(TEXT("Field level five exposes three eligible empty plots"), BatchPlant.EligiblePlotCount, 3);
	TestEqual(TEXT("Two affordable plots are planted"), BatchPlant.PlantedPlotCount, 2);
	TestEqual(TEXT("Batch result preserves one result per eligible plot"), BatchPlant.PlantResults.Num(), 3);
	TestEqual(TEXT("Batch planting consumes all supplied stones"), BatchStones, 0);
	TestEqual(TEXT("Batch planting consumes both supplied grass"),
		UImmortalMaterialLibrary::GetMaterialQuantity(BatchMaterials, TEXT("SpiritGrass")), 0);

	FImmortalFarmingState HighLevelState = DefaultState;
	TArray<FImmortalMaterialStack> HighLevelMaterials;
	int32 HighLevelStones = 3;
	const FImmortalFarmingPlantResult HighLevelPlant = UImmortalFarmingLibrary::TryPlantCrop(
		HighLevelState, 5, TEXT("SpiritGrassCrop"), 17,
		HighLevelMaterials, HighLevelStones, StartTicks);
	TestTrue(TEXT("Field level seventeen unlocks plot six"), HighLevelPlant.bSucceeded);
	TestTrue(TEXT("Every four field levels add one frozen yield"),
		HighLevelPlant.FrozenYield >= 7 && HighLevelPlant.FrozenYield <= 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalFarmingGrowthHarvestBatchTest,
	"ImmortalPath.Farming.GrowthHarvestAndBatchSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalFarmingGrowthHarvestBatchTest::RunTest(const FString& Parameters)
{
	const int64 StartTicks = FDateTime(2026, 7, 18, 0, 0, 0).GetTicks();
	const int64 OneSecond = ETimespan::TicksPerSecond;

	FImmortalFarmingState OneShot = UImmortalFarmingLibrary::CreateDefaultState(StartTicks);
	FImmortalFarmingState Segmented = OneShot;
	TArray<FImmortalMaterialStack> OneShotMaterials;
	TArray<FImmortalMaterialStack> SegmentedMaterials;
	int32 OneShotStones = 3;
	int32 SegmentedStones = 3;
	TestTrue(TEXT("One-shot growth setup plants successfully"),
		UImmortalFarmingLibrary::TryPlantCrop(
			OneShot, 0, TEXT("SpiritGrassCrop"), 2,
			OneShotMaterials, OneShotStones, StartTicks).bSucceeded);
	TestTrue(TEXT("Segmented growth setup plants successfully"),
		UImmortalFarmingLibrary::TryPlantCrop(
			Segmented, 0, TEXT("SpiritGrassCrop"), 2,
			SegmentedMaterials, SegmentedStones, StartTicks).bSucceeded);
	const int64 ElapsedTicks = 123456789;
	UImmortalFarmingLibrary::SettleGrowth(OneShot, 2, StartTicks + ElapsedTicks);
	for (const int64 Offset : TArray<int64>{1, 17, 999, 123456, 10000000, ElapsedTicks})
	{
		UImmortalFarmingLibrary::SettleGrowth(Segmented, 2, StartTicks + Offset);
	}
	TestTrue(TEXT("Integer permille carry makes segmented growth equal one-shot growth"),
		AreFarmingStatesEqual(OneShot, Segmented, true));
	TestEqual(TEXT("Segmented and one-shot remainder are identical"),
		OneShot.Plots[0].GrowthProgressPermilleRemainder,
		Segmented.Plots[0].GrowthProgressPermilleRemainder);

	FImmortalFarmingState BoundaryState = UImmortalFarmingLibrary::CreateDefaultState(StartTicks);
	TArray<FImmortalMaterialStack> BoundaryMaterials;
	int32 BoundaryStones = 3;
	UImmortalFarmingLibrary::TryPlantCrop(
		BoundaryState, 0, TEXT("SpiritGrassCrop"), 1,
		BoundaryMaterials, BoundaryStones, StartTicks);
	const int64 GrassDuration = static_cast<int64>(300) * OneSecond;
	UImmortalFarmingLibrary::SettleGrowth(BoundaryState, 1, StartTicks + GrassDuration - 1);
	TestFalse(TEXT("A crop is not mature one tick before its exact boundary"),
		BoundaryState.Plots[0].IsMature());
	TestEqual(TEXT("Exactly one base-growth tick remains at the pre-boundary"),
		BoundaryState.Plots[0].RemainingGrowthTicks, int64(1));
	const FImmortalFarmingSettlementResult BoundarySettlement =
		UImmortalFarmingLibrary::SettleGrowth(BoundaryState, 1, StartTicks + GrassDuration);
	TestTrue(TEXT("A crop matures on its exact boundary"), BoundaryState.Plots[0].IsMature());
	TestEqual(TEXT("Boundary settlement reports one newly mature plot"),
		BoundarySettlement.MaturedPlotCount, 1);
	const FImmortalFarmingState SameTimestampState = BoundaryState;
	const FImmortalFarmingSettlementResult SameTimestamp =
		UImmortalFarmingLibrary::SettleGrowth(BoundaryState, 1, StartTicks + GrassDuration);
	TestFalse(TEXT("Repeated settlement at the same timestamp is a no-op"), SameTimestamp.bStateChanged);
	TestTrue(TEXT("Repeated same-timestamp settlement changes no farming fields"),
		AreFarmingStatesEqual(BoundaryState, SameTimestampState));

	FImmortalFarmingState EmptyHighWater = UImmortalFarmingLibrary::CreateDefaultState(StartTicks);
	const int64 OneHour = FTimespan::FromHours(1.0).GetTicks();
	UImmortalFarmingLibrary::SettleGrowth(EmptyHighWater, 1, StartTicks + OneHour);
	TArray<FImmortalMaterialStack> EmptyHighWaterMaterials;
	int32 EmptyHighWaterStones = 3;
	UImmortalFarmingLibrary::TryPlantCrop(
		EmptyHighWater, 0, TEXT("SpiritGrassCrop"), 1,
		EmptyHighWaterMaterials, EmptyHighWaterStones, StartTicks + OneHour);
	UImmortalFarmingLibrary::SettleGrowth(
		EmptyHighWater, 1, StartTicks + OneHour + 299 * OneSecond);
	TestFalse(TEXT("Historical empty-field time never accelerates a newly planted crop"),
		EmptyHighWater.Plots[0].IsMature());
	TestEqual(TEXT("The newly planted crop has exactly one second remaining"),
		EmptyHighWater.Plots[0].RemainingGrowthTicks, OneSecond);

	FImmortalFarmingState PreviewState = UImmortalFarmingLibrary::CreateDefaultState(StartTicks);
	TArray<FImmortalMaterialStack> PreviewMaterials;
	int32 PreviewStones = 3;
	UImmortalFarmingLibrary::TryPlantCrop(
		PreviewState, 0, TEXT("SpiritGrassCrop"), 1,
		PreviewMaterials, PreviewStones, StartTicks);
	const FImmortalFarmingState BeforePreview = PreviewState;
	const FImmortalFarmingPlotView HalfwayView = UImmortalFarmingLibrary::GetPlotView(
		PreviewState, 0, 1, StartTicks + 150 * OneSecond);
	TestTrue(TEXT("Projected plot view never mutates persistent state"),
		AreFarmingStatesEqual(PreviewState, BeforePreview));
	TestEqual(TEXT("Halfway preview shows 150 seconds remaining"), HalfwayView.RemainingSeconds, int64(150));
	TestEqual(TEXT("Halfway preview reports exactly 500 permille"), HalfwayView.ProgressPermille, 500);
	TestEqual(TEXT("Halfway preview reports the growing phase"),
		HalfwayView.GrowthStage, EImmortalFarmingGrowthStage::Growing);

	FImmortalFarmingState RollbackState = UImmortalFarmingLibrary::CreateDefaultState(StartTicks);
	TArray<FImmortalMaterialStack> RollbackMaterials;
	int32 RollbackStones = 3;
	UImmortalFarmingLibrary::TryPlantCrop(
		RollbackState, 0, TEXT("SpiritGrassCrop"), 1,
		RollbackMaterials, RollbackStones, StartTicks);
	UImmortalFarmingLibrary::SettleGrowth(RollbackState, 1, StartTicks + 120 * OneSecond);
	const FImmortalFarmingState BeforeRollback = RollbackState;
	const FImmortalFarmingSettlementResult RollbackResult =
		UImmortalFarmingLibrary::SettleGrowth(RollbackState, 1, StartTicks + 60 * OneSecond);
	TestTrue(TEXT("A backwards clock is detected"), RollbackResult.bClockRollbackDetected);
	TestTrue(TEXT("Clock rollback changes no state and never lowers the high-water mark"),
		AreFarmingStatesEqual(RollbackState, BeforeRollback));
	UImmortalFarmingLibrary::SettleGrowth(RollbackState, 1, StartTicks + 180 * OneSecond);
	TestEqual(TEXT("Clock recovery pays only sixty seconds beyond the old mark"),
		RollbackState.Plots[0].RemainingGrowthTicks, 120 * OneSecond);

	FImmortalMaterialDefinition GrassMaterialDefinition;
	TestTrue(TEXT("Spirit grass material definition exists for harvest tests"),
		UImmortalMaterialLibrary::GetMaterialDefinition(TEXT("SpiritGrass"), GrassMaterialDefinition));
	FImmortalFarmingState PartialHarvestState = UImmortalFarmingLibrary::CreateDefaultState(StartTicks);
	TArray<FImmortalMaterialStack> PartialHarvestMaterials;
	int32 PartialHarvestStones = 3;
	UImmortalFarmingLibrary::TryPlantCrop(
		PartialHarvestState, 0, TEXT("SpiritGrassCrop"), 1,
		PartialHarvestMaterials, PartialHarvestStones, StartTicks);
	UImmortalFarmingLibrary::SettleGrowth(
		PartialHarvestState, 1, StartTicks + GrassDuration);
	const int32 FrozenYield = PartialHarvestState.Plots[0].PendingYield;
	UImmortalMaterialLibrary::AddMaterialStack(
		PartialHarvestMaterials, TEXT("SpiritGrass"), GrassMaterialDefinition.MaximumStack - 1);
	const FImmortalFarmingHarvestResult PartialHarvest = UImmortalFarmingLibrary::TryHarvestPlot(
		PartialHarvestState, 0, 1, PartialHarvestMaterials, StartTicks + GrassDuration);
	TestTrue(TEXT("Near-full material stacks accept a partial harvest"), PartialHarvest.bSucceeded);
	TestTrue(TEXT("A partial harvest is reported explicitly"), PartialHarvest.bPartiallyHarvested);
	TestEqual(TEXT("Only one item fits in the near-full stack"), PartialHarvest.HarvestedQuantity, 1);
	TestEqual(TEXT("Uncollected mature yield remains on the same plot"),
		PartialHarvestState.Plots[0].PendingYield, FrozenYield - 1);
	TestTrue(TEXT("A partially harvested plot remains mature"), PartialHarvestState.Plots[0].IsMature());
	const int32 RemainingYield = PartialHarvestState.Plots[0].PendingYield;
	TestTrue(TEXT("Space can be made in the material stack"),
		UImmortalMaterialLibrary::RemoveMaterialStack(
			PartialHarvestMaterials, TEXT("SpiritGrass"), RemainingYield));
	const FImmortalFarmingHarvestResult FinishedHarvest = UImmortalFarmingLibrary::TryHarvestPlot(
		PartialHarvestState, 0, 1, PartialHarvestMaterials, StartTicks + GrassDuration);
	TestTrue(TEXT("The remaining mature yield can be harvested later"), FinishedHarvest.bSucceeded);
	TestTrue(TEXT("The later harvest fully clears the plot"), FinishedHarvest.bFullyHarvested);
	TestTrue(TEXT("A fully harvested plot becomes empty"), PartialHarvestState.Plots[0].IsEmpty());
	const FImmortalFarmingState BeforeDuplicateHarvest = PartialHarvestState;
	const TArray<FImmortalMaterialStack> BeforeDuplicateMaterials = PartialHarvestMaterials;
	const FImmortalFarmingHarvestResult DuplicateHarvest = UImmortalFarmingLibrary::TryHarvestPlot(
		PartialHarvestState, 0, 1, PartialHarvestMaterials, StartTicks + GrassDuration);
	TestFalse(TEXT("A second harvest cannot duplicate the crop"), DuplicateHarvest.bSucceeded);
	TestTrue(TEXT("Rejected duplicate harvest preserves state"),
		AreFarmingStatesEqual(PartialHarvestState, BeforeDuplicateHarvest));
	TestTrue(TEXT("Rejected duplicate harvest preserves materials"),
		AreMaterialInventoriesEqual(PartialHarvestMaterials, BeforeDuplicateMaterials));

	FImmortalFarmingState BatchState = UImmortalFarmingLibrary::CreateDefaultState(StartTicks);
	TArray<FImmortalMaterialStack> BatchMaterials;
	int32 BatchStones = 6;
	const FImmortalFarmingBatchPlantResult BatchPlant = UImmortalFarmingLibrary::TryPlantAllEmpty(
		BatchState, TEXT("SpiritGrassCrop"), 1, BatchMaterials, BatchStones, StartTicks);
	TestTrue(TEXT("Both default unlocked plots can be batch planted"), BatchPlant.bSucceeded);
	TestTrue(TEXT("An affordable two-plot batch reports complete"), BatchPlant.bAllEligiblePlotsPlanted);
	TestEqual(TEXT("Two plots were batch planted"), BatchPlant.PlantedPlotCount, 2);
	UImmortalFarmingLibrary::SettleGrowth(BatchState, 1, StartTicks + GrassDuration);
	const int32 ExpectedBatchYield = BatchState.Plots[0].PendingYield + BatchState.Plots[1].PendingYield;
	const FImmortalFarmingBatchHarvestResult BatchHarvest =
		UImmortalFarmingLibrary::TryHarvestAllReady(
			BatchState, 1, BatchMaterials, StartTicks + GrassDuration);
	TestTrue(TEXT("Ready plots can be harvested as one batch"), BatchHarvest.bSucceeded);
	TestFalse(TEXT("Core batch harvest never claims persistence failure"), BatchHarvest.bPersistenceFailed);
	TestEqual(TEXT("Both mature plots were found"), BatchHarvest.ReadyPlotCount, 2);
	TestEqual(TEXT("Both mature plots were fully cleared"), BatchHarvest.FullyHarvestedPlotCount, 2);
	TestEqual(TEXT("Batch harvest transfers the exact sum of frozen yields"),
		BatchHarvest.HarvestedItemCount, ExpectedBatchYield);
	TestTrue(TEXT("Batch harvest clears plot one"), BatchState.Plots[0].IsEmpty());
	TestTrue(TEXT("Batch harvest clears plot two"), BatchState.Plots[1].IsEmpty());
	const FImmortalFarmingState BeforeRepeatedBatchHarvest = BatchState;
	const TArray<FImmortalMaterialStack> BeforeRepeatedBatchMaterials = BatchMaterials;
	const FImmortalFarmingBatchHarvestResult RepeatedBatchHarvest =
		UImmortalFarmingLibrary::TryHarvestAllReady(
			BatchState, 1, BatchMaterials, StartTicks + GrassDuration);
	TestFalse(TEXT("Repeated batch harvest grants nothing"), RepeatedBatchHarvest.bSucceeded);
	TestEqual(TEXT("Repeated batch harvest finds no ready plots"), RepeatedBatchHarvest.ReadyPlotCount, 0);
	TestTrue(TEXT("Repeated batch harvest preserves state"),
		AreFarmingStatesEqual(BatchState, BeforeRepeatedBatchHarvest));
	TestTrue(TEXT("Repeated batch harvest preserves materials"),
		AreMaterialInventoriesEqual(BatchMaterials, BeforeRepeatedBatchMaterials));

	TestEqual(TEXT("Zero duration formats as mature"),
		UImmortalFarmingLibrary::FormatDuration(0).ToString(), FString(TEXT("已成熟")));
	TestEqual(TEXT("Sixty-five seconds format as 01:05"),
		UImmortalFarmingLibrary::FormatDuration(65 * OneSecond).ToString(), FString(TEXT("01:05")));
	TestEqual(TEXT("One hour, one minute and one second formats with hours"),
		UImmortalFarmingLibrary::FormatDuration(3661 * OneSecond).ToString(), FString(TEXT("01:01:01")));
	return true;
}

#endif
