// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalQuestTypes.generated.h"

UENUM(BlueprintType)
enum class EImmortalQuestCategory : uint8
{
	Main UMETA(DisplayName = "Main"),
	Daily UMETA(DisplayName = "Daily"),
	Achievement UMETA(DisplayName = "Achievement")
};

UENUM(BlueprintType)
enum class EImmortalQuestMetric : uint8
{
	MonsterKills UMETA(DisplayName = "Monster Kills"),
	StageClears UMETA(DisplayName = "Stage Clears"),
	BossKills UMETA(DisplayName = "Boss Kills"),
	EquipmentPickups UMETA(DisplayName = "Equipment Pickups"),
	SpiritStonesCollected UMETA(DisplayName = "Spirit Stones Collected"),
	CultivationBreakthroughs UMETA(DisplayName = "Cultivation Breakthroughs"),
	AlchemyCrafts UMETA(DisplayName = "Alchemy Crafts"),
	CraftingActions UMETA(DisplayName = "Crafting Actions"),
	SectTasksClaimed UMETA(DisplayName = "Sect Tasks Claimed"),
	MapCompletions UMETA(DisplayName = "Map Completions"),
	Ascensions UMETA(DisplayName = "Ascensions")
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalQuestReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") int32 SpiritStones = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") int32 TechniqueInsight = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") FName MaterialId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") int32 MaterialQuantity = 0;

	bool IsValid() const
	{
		return SpiritStones > 0 || TechniqueInsight > 0
			|| (!MaterialId.IsNone() && MaterialQuantity > 0);
	}
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalQuestDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") FName QuestId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") EImmortalQuestCategory Category = EImmortalQuestCategory::Main;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") EImmortalQuestMetric Metric = EImmortalQuestMetric::MonsterKills;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") int64 TargetAmount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") FName PrerequisiteQuestId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") FImmortalQuestReward Reward;

	bool IsValid() const
	{
		return !QuestId.IsNone() && TargetAmount > 0 && Reward.IsValid();
	}
};

/** Aggregate event counters keep catalog changes migration-safe and compact. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalQuestCounters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") int64 MonsterKills = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") int64 StageClears = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") int64 BossKills = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") int64 EquipmentPickups = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") int64 SpiritStonesCollected = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") int64 CultivationBreakthroughs = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") int64 AlchemyCrafts = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") int64 CraftingActions = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") int64 SectTasksClaimed = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") int64 MapCompletions = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") int64 Ascensions = 0;

	int64 GetValue(EImmortalQuestMetric Metric) const;
	bool AddValue(EImmortalQuestMetric Metric, int64 Amount);
	bool Normalize();
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalQuestState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") bool bInitialized = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") FImmortalQuestCounters LifetimeCounters;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") FImmortalQuestCounters DailyCounters;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") TArray<FName> ClaimedMainQuestIds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") TArray<FName> ClaimedDailyQuestIds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") TArray<FName> ClaimedAchievementIds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") int32 DailyDayKey = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") int64 LastObservedUtcTicks = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") int64 TotalClaims = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest") int32 Revision = 0;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalQuestDailyRefreshResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bStateChanged = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bClockRollbackDetected = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") int32 DayKey = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") FText Message;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalQuestRecordResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bStateChanged = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bClockRollbackDetected = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") EImmortalQuestMetric Metric = EImmortalQuestMetric::MonsterKills;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") int64 AmountApplied = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") FText Message;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalQuestProgressView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quest") FName QuestId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") int64 Progress = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") int64 Target = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bKnownQuest = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bUnlocked = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bCompleted = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bClaimed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bCanClaim = false;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalQuestClaimResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bCanClaim = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bKnownQuest = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bUnlocked = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bCompleted = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bAlreadyClaimed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bClockRollbackDetected = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") bool bPersistenceFailed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") FName QuestId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") FImmortalQuestReward Reward;
	UPROPERTY(BlueprintReadOnly, Category = "Quest") FText Message;
};

UCLASS()
class IMMORTALPATH_API UImmortalQuestLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Quest")
	static TArray<FImmortalQuestDefinition> GetQuestDefinitions(EImmortalQuestCategory Category);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Quest")
	static bool GetQuestDefinition(FName QuestId, FImmortalQuestDefinition& OutDefinition);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Quest")
	static int32 GetDayKeyFromUtcTicks(int64 UtcTicks, int32 UtcOffsetMinutes = 480);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Quest")
	static FText GetCategoryText(EImmortalQuestCategory Category);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Quest")
	static FText GetMetricText(EImmortalQuestMetric Metric);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Quest")
	static FText FormatReward(const FImmortalQuestReward& Reward);

	static FImmortalQuestState CreateDefaultState(int64 InitialUtcTicks = 0, int32 UtcOffsetMinutes = 480);
	static bool NormalizeState(FImmortalQuestState& State, int64 CurrentUtcTicks = 0, int32 UtcOffsetMinutes = 480);
	static FImmortalQuestDailyRefreshResult EnsureDailyState(FImmortalQuestState& State, int64 CurrentUtcTicks, int32 UtcOffsetMinutes = 480);
	static FImmortalQuestRecordResult RecordProgress(FImmortalQuestState& State, EImmortalQuestMetric Metric,
		int64 Amount, int64 CurrentUtcTicks, int32 UtcOffsetMinutes = 480);
	static FImmortalQuestProgressView GetProgress(const FImmortalQuestState& State, FName QuestId);
	static FImmortalQuestClaimResult EvaluateClaim(const FImmortalQuestState& State, FName QuestId,
		int64 CurrentUtcTicks, int32 UtcOffsetMinutes = 480);
	static FImmortalQuestClaimResult TryClaim(FImmortalQuestState& State, FName QuestId,
		int64 CurrentUtcTicks, int32 UtcOffsetMinutes = 480);
};

