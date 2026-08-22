// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalEndlessDungeonTypes.generated.h"

/**
 * Data-driven rules for the Endless Dungeon.
 *
 * The native fallback uses three normal enemies, two elite enemies every five
 * floors and one Boss every ten floors. A valid row in
 * /Game/GAME/Data/DT_EndlessDungeon can override these values.
 */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalEndlessDungeonRules : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon", meta = (ClampMin = "1", ClampMax = "9999"))
	int32 MaximumFloor = 9999;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon", meta = (ClampMin = "1"))
	int32 CheckpointInterval = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon", meta = (ClampMin = "2"))
	int32 EliteFloorInterval = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon", meta = (ClampMin = "2"))
	int32 BossFloorInterval = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Enemies", meta = (ClampMin = "1"))
	int32 NormalEnemyCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Enemies", meta = (ClampMin = "1"))
	int32 EliteEnemyCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Enemies", meta = (ClampMin = "1"))
	int32 BossEnemyCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Boss", meta = (ClampMin = "0"))
	int32 PhaseTwoSummonCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Boss", meta = (ClampMin = "0"))
	int32 PhaseThreeSummonCount = 2;

	/** Base per-enemy scaling. Threshold bonuses remain active on later floors so difficulty never regresses. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Scaling", meta = (ClampMin = "0.1"))
	float BaseHealthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Scaling", meta = (ClampMin = "0.0"))
	float HealthMultiplierPerFloor = 0.07f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Scaling", meta = (ClampMin = "0.0"))
	float HealthMultiplierPerEliteThreshold = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Scaling", meta = (ClampMin = "0.0"))
	float HealthMultiplierPerBossThreshold = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Scaling", meta = (ClampMin = "0.1"))
	float BaseAttackMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Scaling", meta = (ClampMin = "0.0"))
	float AttackMultiplierPerFloor = 0.035f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Scaling", meta = (ClampMin = "0.0"))
	float AttackMultiplierPerEliteThreshold = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Scaling", meta = (ClampMin = "0.0"))
	float AttackMultiplierPerBossThreshold = 0.24f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Scaling", meta = (ClampMin = "0.0"))
	float BaseDefenseBonus = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Scaling", meta = (ClampMin = "0.0"))
	float DefenseBonusPerFloor = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Scaling", meta = (ClampMin = "0.0"))
	float DefenseBonusPerEliteThreshold = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Scaling", meta = (ClampMin = "0.0"))
	float DefenseBonusPerBossThreshold = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Rewards", meta = (ClampMin = "1"))
	int32 BaseSpiritStones = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Rewards", meta = (ClampMin = "0"))
	int32 SpiritStonesPerFloor = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Rewards")
	FName BaseMaterialId = TEXT("Ore");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Rewards", meta = (ClampMin = "1"))
	int32 BaseMaterialQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Rewards", meta = (ClampMin = "1"))
	int32 MaterialQuantityFloorInterval = 25;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Rewards", meta = (ClampMin = "1"))
	int32 EquipmentLevelFloorInterval = 3;

	/** The library always clamps these counts to two, preventing milestone rewards from flooding the 30-slot backpack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Rewards", meta = (ClampMin = "1", ClampMax = "2"))
	int32 EliteEquipmentCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Rewards", meta = (ClampMin = "2", ClampMax = "2"))
	int32 BossEquipmentCount = 2;

	bool IsValid() const;
};

/** Fully evaluated combat and reward data for one floor. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalEndlessFloorDescriptor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon")
	int32 Floor = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon")
	bool bElite = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon")
	bool bBoss = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon", meta = (ClampMin = "1"))
	int32 RequiredKills = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Combat")
	float HealthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Combat")
	float AttackMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Combat")
	float DefenseBonus = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Rewards")
	int32 EquipmentItemLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Rewards")
	EImmortalEquipmentQuality MinimumEquipmentQuality = EImmortalEquipmentQuality::Common;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Rewards")
	int32 EquipmentCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Rewards")
	int32 SpiritStones = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Endless Dungeon|Rewards")
	TArray<FImmortalMaterialStack> Materials;

	bool IsValid() const;
};

/**
 * A pre-rolled reward transaction. It deliberately contains no live DataTable
 * reference, so a committed reward keeps its exact GUIDs and values across a
 * restart or a later rules-table edit.
 */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalEndlessRewardBundle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Endless Dungeon")
	FGuid RewardId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Endless Dungeon")
	int32 ClearedFloor = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Endless Dungeon")
	int64 CreatedUtcTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Endless Dungeon")
	int32 SpiritStones = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Endless Dungeon")
	TArray<FImmortalEquipmentItem> EquipmentItems;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Endless Dungeon")
	TArray<FImmortalMaterialStack> Materials;

	bool IsValid() const;
};

/** Persistent records only; live actors, health and timers never enter SaveGame. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalEndlessDungeonState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Endless Dungeon")
	bool bInitialized = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Endless Dungeon")
	int32 Revision = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Endless Dungeon")
	int32 HighestClearedFloor = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Endless Dungeon")
	int64 TotalFloorsCleared = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Endless Dungeon")
	int32 TotalRuns = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Endless Dungeon")
	int64 LastClearedUtcTicks = 0;

	/**
	 * Stable audit IDs for five-floor and ten-floor milestones. Unknown IDs are
	 * retained during normalization so a temporarily removed row cannot be paid twice.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Endless Dungeon")
	TArray<FName> ClaimedMilestoneIds;

	/** Durable, pre-rolled floor rewards awaiting all-or-nothing inventory delivery. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Endless Dungeon")
	TArray<FImmortalEndlessRewardBundle> PendingRewards;
};

/** Pure progression result; inventory delivery is owned by the authoritative player transaction. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalEndlessRecordResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	bool bNewHighest = false;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	int32 PreviousHighestFloor = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	int32 HighestClearedFloor = 0;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalEndlessDungeonStartResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	bool bAlreadyActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	int32 StartFloor = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	FText Message;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalEndlessDungeonFloorClearResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	bool bRewardDelivered = false;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	bool bRewardPending = false;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	bool bNewRecord = false;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	FGuid RewardId;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	FText Message;
};

/** Online-only encounter snapshot consumed by the native TBH panel and combat HUD. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalEndlessDungeonRuntimeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	int32 Floor = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	int32 RequiredKills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	bool bElite = false;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	bool bBoss = false;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	int32 BossPhase = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	float CurrentHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	float MaximumHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	int32 RunStartFloor = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Endless Dungeon")
	float ElapsedSeconds = 0.0f;
};

UCLASS()
class IMMORTALPATH_API UImmortalEndlessDungeonLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Endless Dungeon")
	static FImmortalEndlessDungeonRules GetRules();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Endless Dungeon")
	static bool GetFloorDescriptor(int32 Floor, FImmortalEndlessFloorDescriptor& OutDescriptor);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Endless Dungeon")
	static FImmortalEndlessDungeonState CreateDefaultState();

	/** Repairs malformed state and returns true only when the serialized value changed. */
	static bool NormalizeState(FImmortalEndlessDungeonState& State);

	/**
	 * Returns the first floor of the current ten-floor checkpoint segment.
	 * Formula: max(1, (HighestClearedFloor / CheckpointInterval) * CheckpointInterval + 1).
	 * Thus records 0/9/10/31 restart at floors 1/1/11/31 respectively.
	 */
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Endless Dungeon")
	static int32 GetCheckpointStartFloor(const FImmortalEndlessDungeonState& State);

	/** Accepts only the next unbeaten floor: Floor must equal HighestClearedFloor + 1. */
	static FImmortalEndlessRecordResult RecordFloorClear(
		FImmortalEndlessDungeonState& State,
		int32 Floor,
		int64 CurrentUtcTicks);

	/** Rolls every inventory item exactly once before the player persists the pending bundle. */
	static FImmortalEndlessRewardBundle CreateRewardBundle(
		const FImmortalEndlessFloorDescriptor& Descriptor,
		int64 CurrentUtcTicks);
};
