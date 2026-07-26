// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Maps/ImmortalMapTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalSectTypes.generated.h"

UENUM(BlueprintType)
enum class EImmortalSectTaskMetric : uint8
{
	MonsterKills UMETA(DisplayName = "Monster Kills"),
	StageClears UMETA(DisplayName = "Stage Clears"),
	BossKills UMETA(DisplayName = "Boss Kills")
};

UENUM(BlueprintType)
enum class EImmortalSectRewardType : uint8
{
	Material UMETA(DisplayName = "Material"),
	SpiritStones UMETA(DisplayName = "Spirit Stones"),
	TechniqueInsight UMETA(DisplayName = "Technique Insight"),
	Technique UMETA(DisplayName = "Technique")
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalSectDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") FName SectId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect", meta = (MultiLine = "true")) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") int32 RequiredRealmIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") FName RequiredMapId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") int32 RequiredMapStage = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") FName TechniqueRewardId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") FLinearColor DisplayColor = FLinearColor::White;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalSectTaskDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") FName TaskId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") EImmortalSectTaskMetric Metric = EImmortalSectTaskMetric::MonsterKills;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") int32 TargetAmount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") int32 ContributionReward = 0;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalSectStoreOfferDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") FName OfferId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") EImmortalSectRewardType RewardType = EImmortalSectRewardType::Material;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") FName RewardId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") int32 RewardQuantity = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") int32 ContributionCost = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") int32 DailyLimit = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") bool bOneTime = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sect") int64 RequiredLifetimeContribution = 0;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalSectTaskProgress
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sect") FName TaskId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sect") int32 Progress = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sect") bool bClaimed = false;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalSectOfferProgress
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sect") FName OfferId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sect") int32 DailyPurchaseCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sect") int64 TotalPurchaseCount = 0;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalSectState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sect") bool bInitialized = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sect") FName SectId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sect") int32 Contribution = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sect") int64 TotalContributionEarned = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sect") int64 TotalContributionSpent = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sect") int32 TaskDayKey = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sect") int64 LastObservedUtcTicks = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sect") TArray<FImmortalSectTaskProgress> DailyTasks;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sect") TArray<FImmortalSectOfferProgress> OfferProgress;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sect") int64 TotalTasksClaimed = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Sect") int32 Revision = 0;

	bool HasJoined() const { return !SectId.IsNone(); }
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalSectDailyRefreshResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bStateChanged = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bClockRollbackDetected = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") int32 DayKey = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") FText Message;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalSectJoinResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bCanJoin = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bKnownSect = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bRequirementsMet = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bAlreadyJoined = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bLockedToOtherSect = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bClockRollbackDetected = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bPersistenceFailed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") FName SectId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") FText Message;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalSectTaskProgressResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bStateChanged = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bClockRollbackDetected = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") int32 MonsterKillsApplied = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") int32 StageClearsApplied = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") int32 BossKillsApplied = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") FText Message;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalSectTaskClaimResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bCanClaim = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bKnownTask = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bCompleted = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bAlreadyClaimed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bClockRollbackDetected = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bPersistenceFailed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") FName TaskId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") int32 ContributionAwarded = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") int32 ContributionAfter = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") FText Message;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalSectExchangeResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bCanExchange = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bKnownOffer = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bAffordable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bRequirementMet = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bDailyLimitReached = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bOneTimePurchased = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bClockRollbackDetected = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") bool bPersistenceFailed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") FName OfferId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") EImmortalSectRewardType RewardType = EImmortalSectRewardType::Material;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") FName RewardId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") int32 RewardQuantity = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") int32 ContributionSpent = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") int32 ContributionAfter = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sect") FText Message;
};

UCLASS()
class IMMORTALPATH_API UImmortalSectLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Sect") static TArray<FName> GetKnownSectIds();
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Sect") static bool GetSectDefinition(FName SectId, FImmortalSectDefinition& OutDefinition);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Sect") static TArray<FImmortalSectTaskDefinition> GetDailyTaskDefinitions();
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Sect") static TArray<FImmortalSectStoreOfferDefinition> GetStoreOfferDefinitions(FName SectId);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Sect") static int32 GetDayKeyFromUtcTicks(int64 UtcTicks, int32 UtcOffsetMinutes = 480);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Sect") static FText GetRankName(int64 TotalContributionEarned);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Sect") static bool GetTaskProgress(const FImmortalSectState& State, FName TaskId, FImmortalSectTaskProgress& OutProgress);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Sect") static bool GetOfferProgress(const FImmortalSectState& State, FName OfferId, FImmortalSectOfferProgress& OutProgress);

	static FImmortalSectState CreateDefaultState(int64 InitialUtcTicks = 0, int32 UtcOffsetMinutes = 480);
	static bool NormalizeState(FImmortalSectState& State, int64 DefaultUtcTicks = 0, int32 UtcOffsetMinutes = 480);
	static FImmortalSectDailyRefreshResult EnsureDailyState(FImmortalSectState& State, int64 CurrentUtcTicks, int32 UtcOffsetMinutes = 480);
	static FImmortalSectJoinResult EvaluateJoin(const FImmortalSectState& State, FName SectId, int32 RealmIndex,
		const FImmortalMapSystemState& MapState, int64 CurrentUtcTicks, int32 UtcOffsetMinutes = 480);
	static FImmortalSectJoinResult TryJoin(FImmortalSectState& State, FName SectId, int32 RealmIndex,
		const FImmortalMapSystemState& MapState, int64 CurrentUtcTicks, int32 UtcOffsetMinutes = 480);
	static FImmortalSectTaskProgressResult RecordCombatProgress(FImmortalSectState& State, int32 MonsterKills,
		int32 StageClears, int32 BossKills, int64 CurrentUtcTicks, int32 UtcOffsetMinutes = 480);
	static FImmortalSectTaskClaimResult EvaluateTaskClaim(const FImmortalSectState& State, FName TaskId,
		int64 CurrentUtcTicks, int32 UtcOffsetMinutes = 480);
	static FImmortalSectTaskClaimResult TryClaimTask(FImmortalSectState& State, FName TaskId,
		int64 CurrentUtcTicks, int32 UtcOffsetMinutes = 480);
	static FImmortalSectExchangeResult EvaluateExchange(const FImmortalSectState& State, FName OfferId,
		int64 CurrentUtcTicks, int32 UtcOffsetMinutes = 480);
	static FImmortalSectExchangeResult TryExchange(FImmortalSectState& State, FName OfferId,
		int64 CurrentUtcTicks, int32 UtcOffsetMinutes = 480);
};
