// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Maps/ImmortalMapTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalAscensionTypes.generated.h"

/** Permanent paths powered by Immortal Seals earned from repeatable ascension. */
UENUM(BlueprintType)
enum class EImmortalAscensionPath : uint8
{
	Battle UMETA(DisplayName = "Battle Dao"),
	Enlightenment UMETA(DisplayName = "Enlightenment Dao"),
	Fortune UMETA(DisplayName = "Fortune Dao"),
	MAX UMETA(Hidden)
};

/** Permanent achievement record retained when a new adventure cycle starts. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalAscensionMapLegacy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ascension")
	FName MapId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ascension", meta = (ClampMin = "1", ClampMax = "999"))
	int32 HighestStage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ascension", meta = (ClampMin = "0", ClampMax = "999"))
	int32 TimesCompleted = 0;
};

/**
 * Permanent prestige state. Ascension resets cultivation and the replayable
 * adventure cycle while retaining lifetime map achievements here.
 */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalAscensionState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ascension")
	bool bInitialized = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ascension")
	int32 Revision = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ascension", meta = (ClampMin = "0", ClampMax = "999"))
	int32 AscensionCount = 0;

	/** Unspent permanent currency. Path ranks use the documented escalating cost. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ascension", meta = (ClampMin = "0"))
	int32 ImmortalSeals = 0;

	/** Audit high-water mark; never decreases when seals are invested. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ascension", meta = (ClampMin = "0"))
	int64 TotalImmortalSealsEarned = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ascension", meta = (ClampMin = "0", ClampMax = "50"))
	int32 BattlePathRank = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ascension", meta = (ClampMin = "0", ClampMax = "50"))
	int32 EnlightenmentPathRank = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ascension", meta = (ClampMin = "0", ClampMax = "50"))
	int32 FortunePathRank = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ascension")
	int64 LastAscensionUtcTicks = 0;

	/** Highest stage and completed-cycle count for every known adventure map. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ascension")
	TArray<FImmortalAscensionMapLegacy> LifetimeMapRecords;
};

/** Complete gate report used by both native UI and the authoritative transaction. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalAscensionEligibility
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	bool bEligible = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	bool bReachedAscensionRealm = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	bool bImmortalPalaceCompleted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	bool bAtQingyunMountain = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	bool bIndependentEncountersIdle = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	bool bBelowAscensionLimit = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	int32 RewardImmortalSeals = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	FText Message;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalAscensionOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	int32 ImmortalSealsGranted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	int32 AscensionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	int32 ImmortalSeals = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	FText Message;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalAscensionPathResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	EImmortalAscensionPath Path = EImmortalAscensionPath::Battle;

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	int32 PreviousRank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	int32 CurrentRank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	int32 ImmortalSeals = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	int32 ImmortalSealsSpent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ascension")
	FText Message;
};

UCLASS()
class IMMORTALPATH_API UImmortalAscensionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static constexpr int32 MaximumAscensionCount = 999;
	static constexpr int32 MaximumPathRank = 50;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Ascension")
	static FImmortalAscensionState CreateDefaultState();

	/** Repairs malformed values and returns whether any serialized value changed. */
	static bool NormalizeState(FImmortalAscensionState& State);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Ascension")
	static int32 CalculateAscensionReward(int32 CompletedAscensions);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Ascension")
	static int32 GetPathRank(
		const FImmortalAscensionState& State,
		EImmortalAscensionPath Path);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Ascension")
	static int32 CalculatePathUpgradeCost(int32 CurrentRank);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Ascension")
	static int64 CalculatePathCumulativeCost(int32 Rank);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Ascension")
	static FText GetPathDisplayName(EImmortalAscensionPath Path);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Ascension")
	static FText GetPathDescription(EImmortalAscensionPath Path);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Ascension")
	static float CalculateBattleDamageMultiplier(
		const FImmortalAscensionState& State);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Ascension")
	static float CalculateCultivationRateMultiplier(
		const FImmortalAscensionState& State);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Ascension")
	static float CalculateEquipmentDropMultiplier(
		const FImmortalAscensionState& State);

	/** Canonical stage-one state used by every newly ascended adventure cycle. */
	static FImmortalMapSystemState CreateNewCycleMapState();

	/** Returns one normalized lifetime record, or false for an unknown map. */
	static bool GetLifetimeMapRecord(
		const FImmortalAscensionState& State,
		FName MapId,
		FImmortalAscensionMapLegacy& OutRecord);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Ascension")
	static int32 GetLifetimeCompletedMapCount(
		const FImmortalAscensionState& State);

	/** Pure external-gate evaluation; the player supplies authoritative runtime facts. */
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Ascension")
	static FImmortalAscensionEligibility EvaluateEligibility(
		const FImmortalAscensionState& State,
		bool bReachedAscensionRealm,
		bool bImmortalPalaceCompleted,
		bool bAtQingyunMountain,
		bool bWorldBossActive,
		bool bEndlessDungeonActive);

	/** Mutates only permanent ascension state after external gates have passed. */
	static FImmortalAscensionOperationResult GrantAscension(
		FImmortalAscensionState& State,
		const FImmortalMapSystemState& CompletedCycleMapState,
		int64 CurrentUtcTicks);

	/** Consumes exactly one seal and raises one path rank atomically in memory. */
	static FImmortalAscensionPathResult InvestPath(
		FImmortalAscensionState& State,
		EImmortalAscensionPath Path);
};
