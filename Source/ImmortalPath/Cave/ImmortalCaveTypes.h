// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Crafting/ImmortalCraftingTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalCaveTypes.generated.h"

/** Stable building identifiers. Never reorder these values after saves have shipped. */
UENUM(BlueprintType)
enum class EImmortalCaveBuildingType : uint8
{
	CaveHeart UMETA(DisplayName = "Cave Heart"),
	MeditationRoom UMETA(DisplayName = "Meditation Room"),
	SpiritVein UMETA(DisplayName = "Spirit Vein"),
	StoragePavilion UMETA(DisplayName = "Storage Pavilion"),
	AlchemyRoom UMETA(DisplayName = "Alchemy Room"),
	ForgeRoom UMETA(DisplayName = "Forge Room"),
	SpiritField UMETA(DisplayName = "Spirit Field")
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalCaveBuildingDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cave")
	EImmortalCaveBuildingType Type = EImmortalCaveBuildingType::CaveHeart;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cave")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cave", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cave")
	FLinearColor DisplayColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cave", meta = (ClampMin = "1"))
	int32 MaximumLevel = 20;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalCaveBuildingProgress
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave")
	EImmortalCaveBuildingType Type = EImmortalCaveBuildingType::CaveHeart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave", meta = (ClampMin = "1", ClampMax = "20"))
	int32 Level = 1;
};

/** Complete persistent cave authority. Produced resources remain here until explicitly collected. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalCaveState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave")
	bool bInitialized = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave")
	TArray<FImmortalCaveBuildingProgress> Buildings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave")
	int64 LastSettlementUtcTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave", meta = (ClampMin = "0"))
	int32 StoredSpiritStones = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave", meta = (ClampMin = "0"))
	int32 StoredSpiritGrass = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave", meta = (ClampMin = "0"))
	int32 StoredOre = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave")
	double SpiritStoneFraction = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave")
	double SpiritGrassFraction = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave")
	double OreFraction = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave", meta = (ClampMin = "0"))
	int64 TotalSpiritStonesProduced = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave", meta = (ClampMin = "0"))
	int64 TotalSpiritGrassProduced = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave", meta = (ClampMin = "0"))
	int64 TotalOreProduced = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave", meta = (ClampMin = "0"))
	int64 TotalSpiritStonesCollected = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave", meta = (ClampMin = "0"))
	int64 TotalSpiritGrassCollected = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave", meta = (ClampMin = "0"))
	int64 TotalOreCollected = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave", meta = (ClampMin = "0"))
	int32 CollectionCount = 0;

	/** Incremented only when canonical cave state changes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Cave", meta = (ClampMin = "0"))
	int32 Revision = 0;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalCaveProductionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cave") double SpiritStonesPerHour = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") double SpiritGrassPerHour = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") double OrePerHour = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") double StorageHours = 8.0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") int32 SpiritStoneCapacity = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") int32 SpiritGrassCapacity = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") int32 OreCapacity = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") float GlobalProductionMultiplier = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") float CultivationRateMultiplier = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") float AlchemySuccessChanceBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") float AlchemyExceptionalChanceBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") float ForgeSpiritStoneDiscount = 0.0f;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalCaveSettlementResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cave") bool bStateChanged = false;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") bool bClockRollbackDetected = false;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") double ElapsedSeconds = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") int32 AddedSpiritStones = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") int32 AddedSpiritGrass = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") int32 AddedOre = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") int64 DiscardedSpiritStones = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") int64 DiscardedSpiritGrass = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") int64 DiscardedOre = 0;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalCaveUpgradeResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cave") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") bool bKnownBuilding = false;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") bool bAtMaximumLevel = false;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") bool bBlockedByCaveHeart = false;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") bool bAffordable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") bool bCanUpgrade = false;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") int32 CurrentLevel = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") int32 TargetLevel = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") FImmortalCraftingCost Cost;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") FText Message;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalCaveCollectionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cave") bool bCollectedAnything = false;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") bool bPersistenceFailed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") int32 SpiritStonesCollected = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") int32 SpiritGrassCollected = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") int32 OreCollected = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cave") FText Message;
};

/** Deterministic cave catalog, economy, settlement and transaction helpers. */
UCLASS()
class IMMORTALPATH_API UImmortalCaveLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cave")
	static TArray<EImmortalCaveBuildingType> GetKnownBuildingTypes();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cave")
	static bool GetBuildingDefinition(EImmortalCaveBuildingType Type, FImmortalCaveBuildingDefinition& OutDefinition);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cave")
	static FImmortalCaveState CreateDefaultState(int64 InitialUtcTicks = 0);

	/** Repairs legacy/tampered state into one canonical entry per building. Returns true when state changed. */
	static bool NormalizeState(FImmortalCaveState& State, int64 DefaultUtcTicks = 0);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cave")
	static int32 GetBuildingLevel(const FImmortalCaveState& State, EImmortalCaveBuildingType Type);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cave")
	static FImmortalCraftingCost GetUpgradeCost(const FImmortalCaveState& State, EImmortalCaveBuildingType Type);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cave")
	static FImmortalCaveUpgradeResult EvaluateUpgrade(
		const FImmortalCaveState& State,
		EImmortalCaveBuildingType Type,
		const TArray<FImmortalMaterialStack>& Materials,
		int32 SpiritStones);

	/** Transactional: cave, inventory and stones are unchanged unless the complete upgrade succeeds. */
	static FImmortalCaveUpgradeResult TryUpgradeBuilding(
		FImmortalCaveState& State,
		EImmortalCaveBuildingType Type,
		TArray<FImmortalMaterialStack>& Materials,
		int32& SpiritStones);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cave")
	static FImmortalCaveProductionSnapshot GetProductionSnapshot(const FImmortalCaveState& State);

	/** Settles elapsed production. A backwards clock never moves the saved high-water mark backwards. */
	static FImmortalCaveSettlementResult SettleProduction(FImmortalCaveState& State, int64 CurrentUtcTicks);

	/** Moves every resource that fits into the supplied player inventories, leaving any remainder stored. */
	static FImmortalCaveCollectionResult CollectStoredResources(
		FImmortalCaveState& State,
		TArray<FImmortalMaterialStack>& Materials,
		int32& SpiritStones);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cave")
	static FImmortalCraftingCost ApplyForgeDiscount(
		const FImmortalCraftingCost& Cost,
		const FImmortalCaveState& State);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cave")
	static FText GetCurrentEffectText(const FImmortalCaveState& State, EImmortalCaveBuildingType Type);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cave")
	static FText GetNextEffectText(const FImmortalCaveState& State, EImmortalCaveBuildingType Type);
};
