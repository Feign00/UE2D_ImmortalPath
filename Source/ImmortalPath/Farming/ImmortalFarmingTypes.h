// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Crafting/ImmortalCraftingTypes.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalFarmingTypes.generated.h"

UENUM(BlueprintType)
enum class EImmortalFarmingGrowthStage : uint8
{
	Empty UMETA(DisplayName = "Empty"),
	Seedling UMETA(DisplayName = "Seedling"),
	Growing UMETA(DisplayName = "Growing"),
	Ripening UMETA(DisplayName = "Ripening"),
	Mature UMETA(DisplayName = "Mature")
};

/** One row in the optional /Game/GAME/Data/DT_FarmingCrops data table. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalFarmingCropDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Farming")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Farming", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Farming")
	FName OutputMaterialId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Farming")
	FImmortalCraftingCost PlantingCost;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Farming", meta = (ClampMin = "1"))
	int32 BaseGrowthSeconds = 300;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Farming", meta = (ClampMin = "1"))
	int32 MinimumBaseYield = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Farming", meta = (ClampMin = "1"))
	int32 MaximumBaseYield = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Farming", meta = (ClampMin = "1", ClampMax = "20"))
	int32 RequiredSpiritFieldLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Farming")
	FLinearColor DisplayColor = FLinearColor::White;
};

/** Persistent state for one stable plot index. Six records are always serialized. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalFarmingPlotState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Farming")
	FName CropId = NAME_None;

	/** Frozen base-time duration. Growth-speed multipliers consume this value faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Farming", meta = (ClampMin = "0"))
	int64 TotalGrowthTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Farming", meta = (ClampMin = "0"))
	int64 RemainingGrowthTicks = 0;

	/** Remainder of elapsed-ticks * speed-permille / 1000. Always in [0, 999]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Farming", meta = (ClampMin = "0", ClampMax = "999"))
	int32 GrowthProgressPermilleRemainder = 0;

	/** Harvest quantity is rolled and frozen when the crop is planted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Farming", meta = (ClampMin = "0"))
	int32 PendingYield = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Farming", meta = (ClampMin = "0", ClampMax = "20"))
	int32 PlantedSpiritFieldLevel = 0;

	bool IsEmpty() const { return CropId.IsNone(); }
	bool IsMature() const { return !CropId.IsNone() && RemainingGrowthTicks <= 0 && PendingYield > 0; }
};

/** Independent farming authority. It deliberately does not share the cave-production high-water mark. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalFarmingState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Farming")
	bool bInitialized = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Farming")
	TArray<FImmortalFarmingPlotState> Plots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Farming")
	int64 LastSettlementUtcTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Farming", meta = (ClampMin = "0"))
	int64 TotalCropsPlanted = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Farming", meta = (ClampMin = "0"))
	int64 TotalCropsHarvested = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Farming", meta = (ClampMin = "0"))
	int64 TotalItemsHarvested = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Farming", meta = (ClampMin = "0"))
	int32 Revision = 0;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalFarmingSettlementResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bStateChanged = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bPlotStateChanged = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bClockRollbackDetected = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") double ElapsedSeconds = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 GrowingPlotCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 MaturedPlotCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") FText Message;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalFarmingPlotView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 PlotIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bValidPlot = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bUnlocked = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bClockRollbackDetected = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 RequiredSpiritFieldLevel = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") FName CropId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") FName OutputMaterialId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") EImmortalFarmingGrowthStage GrowthStage = EImmortalFarmingGrowthStage::Empty;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int64 TotalGrowthTicks = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int64 RemainingGrowthTicks = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int64 RemainingSeconds = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 ProgressPermille = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 PendingYield = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") FText Message;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalFarmingPlantResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bPersistenceFailed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bKnownCrop = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bValidPlot = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bPlotUnlocked = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bCropUnlocked = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bPlotEmpty = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bAffordable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bClockRollbackDetected = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bCanPlant = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 PlotIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") FName CropId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") FImmortalCraftingCost Cost;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int64 GrowthDurationTicks = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 FrozenYield = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") FText Message;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalFarmingBatchPlantResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bPersistenceFailed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bAllEligiblePlotsPlanted = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 EligiblePlotCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 PlantedPlotCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") FName CropId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") TArray<FImmortalFarmingPlantResult> PlantResults;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") FText Message;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalFarmingHarvestResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bPersistenceFailed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bValidPlot = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bWasReady = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bFullyHarvested = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bPartiallyHarvested = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bClockRollbackDetected = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 PlotIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") FName CropId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") FName OutputMaterialId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 RequestedQuantity = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 HarvestedQuantity = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 RemainingQuantity = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") FText Message;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalFarmingBatchHarvestResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") bool bPersistenceFailed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 ReadyPlotCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 FullyHarvestedPlotCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 PartiallyHarvestedPlotCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") int32 HarvestedItemCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") TArray<FImmortalFarmingHarvestResult> HarvestResults;
	UPROPERTY(BlueprintReadOnly, Category = "Farming") FText Message;
};

/** Deterministic crop catalog, UTC growth simulation and inventory transactions. */
UCLASS()
class IMMORTALPATH_API UImmortalFarmingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Farming")
	static TArray<FName> GetKnownCropIds();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Farming")
	static bool GetCropDefinition(FName CropId, FImmortalFarmingCropDefinition& OutDefinition);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Farming")
	static int32 GetMaximumPlotCount();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Farming")
	static int32 GetUnlockedPlotCount(int32 SpiritFieldLevel);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Farming")
	static int32 GetRequiredFieldLevelForPlot(int32 PlotIndex);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Farming")
	static float GetGrowthSpeedMultiplier(int32 SpiritFieldLevel);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Farming")
	static FImmortalFarmingState CreateDefaultState(int64 InitialUtcTicks = 0);

	/** Repairs legacy/tampered data into exactly six canonical plot records. */
	static bool NormalizeState(FImmortalFarmingState& State, int64 DefaultUtcTicks = 0);

	/** Pure projected view. It never advances the supplied state's high-water mark. */
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Farming")
	static FImmortalFarmingPlotView GetPlotView(
		const FImmortalFarmingState& State,
		int32 PlotIndex,
		int32 SpiritFieldLevel,
		int64 CurrentUtcTicks);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Farming")
	static FImmortalFarmingPlantResult EvaluatePlant(
		const FImmortalFarmingState& State,
		int32 PlotIndex,
		FName CropId,
		int32 SpiritFieldLevel,
		const TArray<FImmortalMaterialStack>& Materials,
		int32 SpiritStones,
		int64 CurrentUtcTicks);

	/** Transactional: rejected planting changes none of state, materials or stones. */
	static FImmortalFarmingPlantResult TryPlantCrop(
		FImmortalFarmingState& State,
		int32 PlotIndex,
		FName CropId,
		int32 SpiritFieldLevel,
		TArray<FImmortalMaterialStack>& Materials,
		int32& SpiritStones,
		int64 CurrentUtcTicks);

	/** Plants every currently empty unlocked plot that remains affordable. */
	static FImmortalFarmingBatchPlantResult TryPlantAllEmpty(
		FImmortalFarmingState& State,
		FName CropId,
		int32 SpiritFieldLevel,
		TArray<FImmortalMaterialStack>& Materials,
		int32& SpiritStones,
		int64 CurrentUtcTicks);

	/** Applies elapsed UTC time using integer permille carry, preserving segmented equivalence. */
	static FImmortalFarmingSettlementResult SettleGrowth(
		FImmortalFarmingState& State,
		int32 SpiritFieldLevel,
		int64 CurrentUtcTicks);

	/** Transfers as much mature yield as fits; any remainder stays on the mature plot. */
	static FImmortalFarmingHarvestResult TryHarvestPlot(
		FImmortalFarmingState& State,
		int32 PlotIndex,
		int32 SpiritFieldLevel,
		TArray<FImmortalMaterialStack>& Materials,
		int64 CurrentUtcTicks);

	/** Harvests every mature plot independently, preserving blocked or partial plot remainders. */
	static FImmortalFarmingBatchHarvestResult TryHarvestAllReady(
		FImmortalFarmingState& State,
		int32 SpiritFieldLevel,
		TArray<FImmortalMaterialStack>& Materials,
		int64 CurrentUtcTicks);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Farming")
	static FText FormatDuration(int64 DurationTicks);
};
