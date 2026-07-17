// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Crafting/ImmortalCraftingTypes.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalArtifactTypes.generated.h"

UENUM(BlueprintType)
enum class EImmortalArtifactQuality : uint8
{
	Mortal UMETA(DisplayName = "Mortal"),
	Spirit UMETA(DisplayName = "Spirit"),
	Mystic UMETA(DisplayName = "Mystic"),
	Earth UMETA(DisplayName = "Earth"),
	Heaven UMETA(DisplayName = "Heaven"),
	Immortal UMETA(DisplayName = "Immortal")
};

UENUM(BlueprintType)
enum class EImmortalArtifactActiveEffect : uint8
{
	DirectDamage UMETA(DisplayName = "Direct Damage"),
	AreaDamage UMETA(DisplayName = "Area Damage"),
	HealAndShield UMETA(DisplayName = "Heal and Shield"),
	ChaosBurst UMETA(DisplayName = "Chaos Burst")
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalArtifactDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact", meta = (MultiLine = "true")) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact") FText IconGlyph;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact") EImmortalArtifactQuality Quality = EImmortalArtifactQuality::Spirit;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact") EImmortalArtifactActiveEffect ActiveEffect = EImmortalArtifactActiveEffect::DirectDamage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact", meta = (ClampMin = "2", ClampMax = "20")) int32 AttacksPerTrigger = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact", meta = (ClampMin = "0.0")) float ActiveMagnitude = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact|Passive", meta = (ClampMin = "0.0")) float AttackPercent = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact|Passive", meta = (ClampMin = "0.0")) float DefensePercent = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact|Passive", meta = (ClampMin = "0.0")) float HealthPercent = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact|Passive", meta = (ClampMin = "0.0")) float AttackSpeedBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact|Passive", meta = (ClampMin = "0.0")) float CriticalChanceBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact|Crafting", meta = (ClampMin = "1", ClampMax = "999")) int32 MinimumQingyunStage = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact|Crafting") FImmortalCraftingCost CraftingCost;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalArtifactItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Artifact") FGuid InstanceId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Artifact") FName ArtifactId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Artifact", meta = (ClampMin = "1", ClampMax = "50")) int32 Level = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Artifact", meta = (ClampMin = "0", ClampMax = "5")) int32 Stars = 0;

	bool IsValid() const { return InstanceId.IsValid() && !ArtifactId.IsNone(); }
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalArtifactBonuses
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Artifact") float AttackMultiplier = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Artifact") float DefenseMultiplier = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Artifact") float HealthMultiplier = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Artifact") float AttackSpeedBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Artifact") float CriticalChanceBonus = 0.0f;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalArtifactOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Artifact") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Artifact") bool bUnlocked = false;
	UPROPERTY(BlueprintReadOnly, Category = "Artifact") bool bAffordable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Artifact") FGuid InstanceId;
	UPROPERTY(BlueprintReadOnly, Category = "Artifact") FText Message;
};

UCLASS()
class IMMORTALPATH_API UImmortalArtifactLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact") static TArray<FName> GetKnownArtifactIds();
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact") static bool GetArtifactDefinition(FName ArtifactId, FImmortalArtifactDefinition& OutDefinition);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact") static FImmortalArtifactItem CreateArtifact(FName ArtifactId);
	static void NormalizeArtifact(FImmortalArtifactItem& Item);
	static void NormalizeInventory(TArray<FImmortalArtifactItem>& Inventory, FGuid& EquippedInstanceId);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact") static FImmortalArtifactBonuses CalculateBonuses(const FImmortalArtifactItem& Item);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact") static float CalculateActiveMagnitude(const FImmortalArtifactItem& Item);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact") static int32 CalculateTriggerAttackCount(const FImmortalArtifactItem& Item);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact") static FImmortalCraftingCost GetUpgradeCost(const FImmortalArtifactItem& Item);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact") static FImmortalCraftingCost GetStarUpCost(const FImmortalArtifactItem& Item);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact") static FText GetQualityText(EImmortalArtifactQuality Quality);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact") static FLinearColor GetQualityColor(EImmortalArtifactQuality Quality);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact") static FText GetActiveEffectText(const FImmortalArtifactItem& Item);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact") static FText GetPassiveEffectText(const FImmortalArtifactItem& Item);
};

