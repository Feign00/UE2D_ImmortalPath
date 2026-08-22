// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalMapTypes.generated.h"

/** Data-driven rules for one persistent adventure map. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalMapDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map") FName MapId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map", meta = (MultiLine = "true")) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map") FText NormalMonsterName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map") FText BossName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unlock", meta = (ClampMin = "0", ClampMax = "9")) int32 RequiredRealmIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "0")) int32 OrderIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress", meta = (ClampMin = "1", ClampMax = "999")) int32 MaximumStage = 999;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress", meta = (ClampMin = "2")) int32 BossStageInterval = 10;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.01")) float HealthMultiplier = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.01")) float AttackMultiplier = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0")) float DefenseMultiplier = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "0")) int32 EquipmentLevelBonus = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards") EImmortalEquipmentQuality MinimumEquipmentQuality = EImmortalEquipmentQuality::Common;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards") EImmortalEquipmentQuality BossMinimumEquipmentQuality = EImmortalEquipmentQuality::Rare;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "0.0", ClampMax = "1.0")) float EquipmentDropChanceBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "0.0", ClampMax = "1.0")) float MaterialDropChanceBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "0.1")) float SpiritStoneMultiplier = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards") TArray<FName> MaterialPoolIds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation") FLinearColor SceneTint = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation") FLinearColor MonsterTint = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation") FLinearColor BossColor = FLinearColor(1.0f, 0.25f, 0.12f, 1.0f);

	bool IsValid() const { return !MapId.IsNone() && MaximumStage > 0 && BossStageInterval >= 2; }
};

/** Independent stage progress for one map. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalMapProgress
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Map") FName MapId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Map", meta = (ClampMin = "1", ClampMax = "999")) int32 Stage = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Map", meta = (ClampMin = "0")) int32 StageKills = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Map") bool bCompleted = false;

	bool IsValid() const { return !MapId.IsNone() && Stage > 0 && StageKills >= 0; }
};

/** Versioned active-map selection and all independent map progress. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalMapSystemState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Map") bool bInitialized = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Map") FName ActiveMapId = TEXT("QingyunMountain");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Map") TArray<FImmortalMapProgress> MapProgress;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalMapTravelResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Map") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Map") bool bUnlocked = false;
	UPROPERTY(BlueprintReadOnly, Category = "Map") bool bAlreadyActive = false;
	UPROPERTY(BlueprintReadOnly, Category = "Map") FName PreviousMapId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Map") FName DestinationMapId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Map") FText Message;
};

/** Fallback map catalog and pure migration/normalization rules. */
UCLASS()
class IMMORTALPATH_API UImmortalMapLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static FName GetQingyunMountainId();
	static FName GetImmortalPalaceRuinsId();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Maps")
	static TArray<FName> GetKnownMapIds();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Maps")
	static bool GetMapDefinition(FName MapId, FImmortalMapDefinition& OutDefinition);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Maps")
	static bool IsMapUnlocked(FName MapId, int32 RealmIndex);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Maps")
	static FText GetRealmRequirementText(int32 RealmIndex);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Maps")
	static bool GetMapProgress(const FImmortalMapSystemState& State, FName MapId, FImmortalMapProgress& OutProgress);

	/** Converts the legacy Qingyun-only fields into a complete eight-map state. */
	static FImmortalMapSystemState CreateMigratedState(int32 QingyunStage, int32 QingyunKills, bool bQingyunCompleted);

	/** Removes unknown/duplicate progress and restores one valid entry for every catalog map. */
	static void NormalizeState(FImmortalMapSystemState& State);

	/** Writes one clamped progress snapshot into an already normalized state. */
	static bool SetMapProgress(FImmortalMapSystemState& State, const FImmortalMapProgress& Progress);

	/** Compatibility progression used by existing crafting/shop unlock rules. */
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Maps")
	static int32 GetEffectiveAdventureStage(FName MapId, int32 LocalStage);
};
