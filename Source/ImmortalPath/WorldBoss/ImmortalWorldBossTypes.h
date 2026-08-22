// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalWorldBossTypes.generated.h"

/**
 * Data-driven definition for one optional World Boss challenge.
 *
 * World Bosses deliberately live outside the 1-999 map-stage progression. Their
 * defeat record and first-clear artifact are persisted independently.
 */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalWorldBossDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss")
	FName BossId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss", meta = (MultiLine = "true"))
	FText Description;

	/** Serialized EImmortalCultivationRealm index to avoid coupling data rows to the component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss", meta = (ClampMin = "0", ClampMax = "9"))
	int32 RequiredRealmIndex = 0;

	/** Baseline monster scaling level, independent of the active map's current stage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss", meta = (ClampMin = "1", ClampMax = "999"))
	int32 RecommendedStage = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss", meta = (ClampMin = "15.0", Units = "s"))
	float TimeLimitSeconds = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Combat", meta = (ClampMin = "1.0"))
	float HealthMultiplier = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Combat", meta = (ClampMin = "0.1"))
	float AttackMultiplier = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Combat", meta = (ClampMin = "0.0"))
	float DefenseBonus = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Combat", meta = (ClampMin = "0.1"))
	float AttackSpeedMultiplier = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Combat", meta = (ClampMin = "1.0"))
	float VisualScaleMultiplier = 1.55f;

	/** Every Nth strike is a long-range skill attack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Skills", meta = (ClampMin = "2"))
	int32 SkillEveryAttacks = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Skills", meta = (ClampMin = "1.0"))
	float SkillDamageMultiplier = 2.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Skills", meta = (ClampMin = "0.0", Units = "cm"))
	float SkillBonusRange = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Skills", meta = (ClampMin = "0"))
	int32 PhaseTwoSummonCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Skills", meta = (ClampMin = "0"))
	int32 PhaseThreeSummonCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Rewards", meta = (ClampMin = "1"))
	int32 GuaranteedEquipmentDrops = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Rewards")
	EImmortalEquipmentQuality MinimumEquipmentQuality = EImmortalEquipmentQuality::Rare;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Rewards", meta = (ClampMin = "0"))
	int32 EquipmentLevelBonus = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Rewards", meta = (ClampMin = "1"))
	int32 SpiritStoneReward = 360;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Rewards")
	FName RareMaterialId = TEXT("DemonCore");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Rewards", meta = (ClampMin = "1"))
	int32 RareMaterialQuantity = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Rewards", meta = (ClampMin = "1"))
	int32 ArtifactFragmentQuantity = 3;

	/** Granted once per save on the first successfully persisted defeat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Rewards")
	FName FirstClearArtifactId = TEXT("XuanGuangSword");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Boss|Presentation")
	FLinearColor DisplayColor = FLinearColor(0.15f, 0.85f, 1.0f, 1.0f);

	bool IsValid() const;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalWorldBossProgress
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "World Boss")
	FName BossId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "World Boss", meta = (ClampMin = "0"))
	int32 DefeatCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "World Boss", meta = (ClampMin = "0.0", Units = "s"))
	float BestClearSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "World Boss")
	int64 LastDefeatedUtcTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "World Boss")
	bool bFirstClearArtifactClaimed = false;

	bool IsValid() const { return !BossId.IsNone(); }
};

/**
 * A pre-rolled, durable reward transaction.
 *
 * It is saved before delivery so a full locked backpack or an interrupted
 * second disk write cannot lose or duplicate a World Boss victory reward.
 */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalWorldBossRewardBundle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "World Boss")
	FGuid RewardId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "World Boss")
	FName BossId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "World Boss")
	int64 CreatedUtcTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "World Boss")
	int32 SpiritStones = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "World Boss")
	TArray<FImmortalEquipmentItem> EquipmentItems;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "World Boss")
	TArray<FImmortalMaterialStack> Materials;

	/** None on repeat clears; otherwise the fixed first-clear artifact. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "World Boss")
	FName ArtifactId = NAME_None;

	bool IsValid() const;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalWorldBossState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "World Boss")
	bool bInitialized = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "World Boss")
	int32 Revision = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "World Boss")
	TArray<FImmortalWorldBossProgress> BossProgress;

	/** Usually delivered immediately; retained only when delivery or persistence cannot complete. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "World Boss")
	TArray<FImmortalWorldBossRewardBundle> PendingRewards;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalWorldBossRecordResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	bool bFirstClear = false;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	bool bNewBestTime = false;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	FImmortalWorldBossProgress Progress;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalWorldBossChallengeResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	bool bUnlocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	bool bAlreadyActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	FName BossId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	FText Message;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalWorldBossVictoryResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	bool bRewardDelivered = false;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	bool bRewardPending = false;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	bool bFirstClear = false;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	bool bNewBestTime = false;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	FGuid RewardId;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	FText Message;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalWorldBossRuntimeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	FName BossId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	int32 Phase = 0;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	float CurrentHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	float MaximumHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	float RemainingSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "World Boss")
	float TimeLimitSeconds = 0.0f;
};

UCLASS()
class IMMORTALPATH_API UImmortalWorldBossLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Immortal Path|World Boss")
	static TArray<FName> GetKnownWorldBossIds();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|World Boss")
	static bool GetWorldBossDefinition(FName BossId, FImmortalWorldBossDefinition& OutDefinition);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|World Boss")
	static FImmortalWorldBossState CreateDefaultState();

	/** Returns true when malformed, duplicate, removed or newly introduced rows changed the state. */
	static bool NormalizeState(FImmortalWorldBossState& State);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|World Boss")
	static bool GetProgress(
		const FImmortalWorldBossState& State,
		FName BossId,
		FImmortalWorldBossProgress& OutProgress);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|World Boss")
	static bool IsUnlocked(const FImmortalWorldBossDefinition& Definition, int32 RealmIndex);

	/** Rolls every reward exactly once before the bundle is persisted. */
	static FImmortalWorldBossRewardBundle CreateRewardBundle(
		const FImmortalWorldBossDefinition& Definition,
		int32 EquipmentItemLevel,
		bool bIncludeFirstClearArtifact,
		int64 CurrentUtcTicks);

	/** Records exactly one persisted victory and resolves first-clear/new-best flags. */
	static FImmortalWorldBossRecordResult RecordDefeat(
		FImmortalWorldBossState& State,
		FName BossId,
		float ClearSeconds,
		int64 CurrentUtcTicks);
};
