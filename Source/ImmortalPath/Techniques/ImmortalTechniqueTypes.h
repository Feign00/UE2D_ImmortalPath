// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Crafting/ImmortalCraftingTypes.h"
#include "../Progression/ImmortalCharacterPathTypes.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalTechniqueTypes.generated.h"

UENUM(BlueprintType)
enum class EImmortalTechniqueQuality : uint8
{
	Mortal UMETA(DisplayName = "Mortal"),
	Spirit UMETA(DisplayName = "Spirit"),
	Mystic UMETA(DisplayName = "Mystic"),
	Earth UMETA(DisplayName = "Earth"),
	Heaven UMETA(DisplayName = "Heaven"),
	Immortal UMETA(DisplayName = "Immortal")
};

UENUM(BlueprintType)
enum class EImmortalTechniqueActiveEffect : uint8
{
	BreathRecovery UMETA(DisplayName = "Breath Recovery"),
	SwordWave UMETA(DisplayName = "Sword Wave"),
	FlameNova UMETA(DisplayName = "Flame Nova"),
	ChainLightning UMETA(DisplayName = "Chain Lightning")
};

UENUM(BlueprintType)
enum class EImmortalTechniquePointBranch : uint8
{
	Active UMETA(DisplayName = "Active"),
	Passive UMETA(DisplayName = "Passive"),
	Special UMETA(DisplayName = "Special")
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalTechniqueDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique", meta = (MultiLine = "true")) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique") FText IconGlyph;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique") EImmortalTechniqueQuality Quality = EImmortalTechniqueQuality::Mortal;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique") EImmortalTechniqueActiveEffect ActiveEffect = EImmortalTechniqueActiveEffect::BreathRecovery;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique") EImmortalElementType Element = EImmortalElementType::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique", meta = (ClampMin = "3", ClampMax = "20")) int32 AttacksPerTrigger = 8;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique", meta = (ClampMin = "2", ClampMax = "10")) int32 ActivesPerUltimate = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique", meta = (ClampMin = "0.0")) float ActiveMagnitude = 0.08f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique", meta = (ClampMin = "0.0")) float UltimateMagnitude = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique|Passive", meta = (ClampMin = "0.0")) float AttackPercent = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique|Passive", meta = (ClampMin = "0.0")) float DefensePercent = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique|Passive", meta = (ClampMin = "0.0")) float HealthPercent = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique|Passive", meta = (ClampMin = "0.0")) float AttackSpeedBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique|Passive", meta = (ClampMin = "0.0")) float CriticalChanceBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique|Passive", meta = (ClampMin = "0.0")) float CultivationRateBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique|Unlock", meta = (ClampMin = "1", ClampMax = "999")) int32 MinimumQingyunStage = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique|Unlock", meta = (ClampMin = "0", ClampMax = "9")) int32 MinimumRealmIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Technique|Learning") FImmortalCraftingCost LearningCost;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalTechniqueProgress
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Technique") FName TechniqueId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Technique", meta = (ClampMin = "1", ClampMax = "50")) int32 Level = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Technique", meta = (ClampMin = "0", ClampMax = "4")) int32 BreakthroughRank = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Technique", meta = (ClampMin = "0", ClampMax = "10")) int32 ActivePoints = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Technique", meta = (ClampMin = "0", ClampMax = "10")) int32 PassivePoints = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Technique", meta = (ClampMin = "0", ClampMax = "5")) int32 SpecialPoints = 0;

	bool IsValid() const { return !TechniqueId.IsNone(); }
	int32 GetAllocatedPointCount() const { return ActivePoints + PassivePoints + SpecialPoints; }
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalTechniqueBonuses
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Technique") float AttackMultiplier = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Technique") float DefenseMultiplier = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Technique") float HealthMultiplier = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Technique") float AttackSpeedBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Technique") float CriticalChanceBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Technique") float CultivationRateMultiplier = 1.0f;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalTechniqueOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Technique") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Technique") bool bUnlocked = false;
	UPROPERTY(BlueprintReadOnly, Category = "Technique") bool bAffordable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Technique") FName TechniqueId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Technique") int32 InsightPointsAfter = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Technique") FText Message;
};

UCLASS()
class IMMORTALPATH_API UImmortalTechniqueLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static TArray<FName> GetKnownTechniqueIds();
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static bool GetTechniqueDefinition(FName TechniqueId, FImmortalTechniqueDefinition& OutDefinition);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static FImmortalTechniqueProgress CreateTechnique(FName TechniqueId);
	static void NormalizeProgress(FImmortalTechniqueProgress& Progress);
	static void NormalizeLibrary(TArray<FImmortalTechniqueProgress>& Library, TArray<FName>& EquippedTechniqueIds, int32& InsightPoints);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static int32 GetLevelCap(const FImmortalTechniqueProgress& Progress);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static int32 GetUpgradeCultivationCost(const FImmortalTechniqueProgress& Progress);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static FImmortalCraftingCost GetBreakthroughCost(const FImmortalTechniqueProgress& Progress);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static FImmortalTechniqueBonuses CalculateBonuses(const FImmortalTechniqueProgress& Progress);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static float CalculateActiveMagnitude(const FImmortalTechniqueProgress& Progress);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static float CalculateUltimateMagnitude(const FImmortalTechniqueProgress& Progress);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static int32 CalculateTriggerAttackCount(const FImmortalTechniqueProgress& Progress);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static int32 CalculateUltimateActiveCount(const FImmortalTechniqueProgress& Progress);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static int32 GetBranchPointCap(EImmortalTechniquePointBranch Branch);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static FText GetQualityText(EImmortalTechniqueQuality Quality);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static FLinearColor GetQualityColor(EImmortalTechniqueQuality Quality);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static FText GetActiveEffectText(const FImmortalTechniqueProgress& Progress);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static FText GetUltimateEffectText(const FImmortalTechniqueProgress& Progress);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static FText GetPassiveEffectText(const FImmortalTechniqueProgress& Progress);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique") static FText GetSpecialEffectText(const FImmortalTechniqueProgress& Progress);
};
