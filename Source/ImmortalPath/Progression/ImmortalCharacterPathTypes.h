// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Crafting/ImmortalCraftingTypes.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalCharacterPathTypes.generated.h"

UENUM(BlueprintType)
enum class EImmortalElementType : uint8
{
	None UMETA(DisplayName = "None"),
	Metal UMETA(DisplayName = "Metal"),
	Wood UMETA(DisplayName = "Wood"),
	Water UMETA(DisplayName = "Water"),
	Fire UMETA(DisplayName = "Fire"),
	Earth UMETA(DisplayName = "Earth"),
	Wind UMETA(DisplayName = "Wind"),
	Thunder UMETA(DisplayName = "Thunder"),
	Ice UMETA(DisplayName = "Ice"),
	Chaos UMETA(DisplayName = "Chaos"),
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EImmortalSpiritRoot : uint8
{
	Unawakened UMETA(DisplayName = "Unawakened"),
	Metal UMETA(DisplayName = "Metal Root"),
	Wood UMETA(DisplayName = "Wood Root"),
	Water UMETA(DisplayName = "Water Root"),
	Fire UMETA(DisplayName = "Fire Root"),
	Earth UMETA(DisplayName = "Earth Root"),
	Wind UMETA(DisplayName = "Wind Root"),
	Thunder UMETA(DisplayName = "Thunder Root"),
	Ice UMETA(DisplayName = "Ice Root"),
	Mutated UMETA(DisplayName = "Mutated Root"),
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EImmortalCultivationPath : uint8
{
	Unselected UMETA(DisplayName = "Unselected"),
	Body UMETA(DisplayName = "Body Cultivation"),
	Dharma UMETA(DisplayName = "Dharma Cultivation"),
	Sword UMETA(DisplayName = "Sword Cultivation"),
	Poison UMETA(DisplayName = "Poison Cultivation"),
	Thunder UMETA(DisplayName = "Thunder Cultivation"),
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EImmortalPathSkillEffect : uint8
{
	BodyQuake UMETA(DisplayName = "Body Quake"),
	FiveElementsSpell UMETA(DisplayName = "Five Elements Spell"),
	SwordArray UMETA(DisplayName = "Sword Array"),
	PoisonCloud UMETA(DisplayName = "Poison Cloud"),
	ThunderChain UMETA(DisplayName = "Thunder Chain")
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalSpiritRootDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spirit Root") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spirit Root", meta = (MultiLine = "true")) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spirit Root") FText IconGlyph;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spirit Root") EImmortalElementType Element = EImmortalElementType::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spirit Root", meta = (ClampMin = "0.0")) float RollWeight = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spirit Root", meta = (ClampMin = "0.0")) float CultivationRateBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spirit Root", meta = (ClampMin = "0.0")) float MatchingElementDamageBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spirit Root", meta = (ClampMin = "0.0")) float PillEffectBonus = 0.0f;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalSpiritRootState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Spirit Root") EImmortalSpiritRoot Root = EImmortalSpiritRoot::Unawakened;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Spirit Root", meta = (ClampMin = "0.6", ClampMax = "1.0")) float Purity = 0.6f;
	bool IsAwakened() const { return Root > EImmortalSpiritRoot::Unawakened && Root < EImmortalSpiritRoot::MAX; }
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalCultivationPathDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path", meta = (MultiLine = "true")) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path") FText IconGlyph;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path") FText SkillName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path", meta = (MultiLine = "true")) FText SkillDescription;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path") EImmortalPathSkillEffect SkillEffect = EImmortalPathSkillEffect::BodyQuake;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path") EImmortalEquipmentDiscipline EquipmentDiscipline = EImmortalEquipmentDiscipline::Universal;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path") EImmortalElementType SkillElement = EImmortalElementType::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path", meta = (ClampMin = "3", ClampMax = "20")) int32 AttacksPerSkill = 8;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path", meta = (ClampMin = "0.0")) float SkillMagnitude = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path|Attributes", meta = (ClampMin = "0.0")) float AttackPercent = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path|Attributes", meta = (ClampMin = "0.0")) float DefensePercent = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path|Attributes", meta = (ClampMin = "0.0")) float HealthPercent = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path|Attributes", meta = (ClampMin = "0.0")) float ManaPercent = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path|Attributes", meta = (ClampMin = "0.0")) float AttackSpeedBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path|Attributes", meta = (ClampMin = "0.0")) float CriticalChanceBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path|Attributes", meta = (ClampMin = "0.0", ClampMax = "0.75")) float DamageReduction = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path|Attributes", meta = (ClampMin = "0.0")) float CultivationRateBonus = 0.0f;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalCultivationPathState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Path") EImmortalCultivationPath Path = EImmortalCultivationPath::Unselected;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Path", meta = (ClampMin = "0")) int32 SwitchCount = 0;
	bool IsSelected() const { return Path > EImmortalCultivationPath::Unselected && Path < EImmortalCultivationPath::MAX; }
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalCharacterPathBonuses
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Path") float AttackMultiplier = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Path") float DefenseMultiplier = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Path") float HealthMultiplier = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Path") float ManaMultiplier = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Path") float AttackSpeedBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Path") float CriticalChanceBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Path") float DamageReduction = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Path") float CultivationRateMultiplier = 1.0f;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalCharacterPathOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Path") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Path") bool bAffordable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Path") EImmortalCultivationPath Path = EImmortalCultivationPath::Unselected;
	UPROPERTY(BlueprintReadOnly, Category = "Path") FText Message;
};

UCLASS()
class IMMORTALPATH_API UImmortalCharacterPathLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Spirit Root") static TArray<EImmortalSpiritRoot> GetKnownSpiritRoots();
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Spirit Root") static bool GetSpiritRootDefinition(EImmortalSpiritRoot Root, FImmortalSpiritRootDefinition& OutDefinition);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Spirit Root") static FImmortalSpiritRootState GenerateSpiritRootFromRoll(float TypeRoll, float PurityRoll);
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Spirit Root") static FImmortalSpiritRootState GenerateRandomSpiritRoot();
	static void NormalizeSpiritRoot(FImmortalSpiritRootState& State);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Spirit Root") static float CalculateCultivationRateMultiplier(const FImmortalSpiritRootState& State);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Spirit Root") static float CalculateElementDamageMultiplier(const FImmortalSpiritRootState& State, EImmortalElementType Element);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Spirit Root") static float CalculatePillEffectMultiplier(const FImmortalSpiritRootState& State);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Spirit Root") static float CalculateEffectivePillMagnitude(const FImmortalSpiritRootState& State, float BaseMagnitude, bool bBonusOnly);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Spirit Root") static FLinearColor GetSpiritRootColor(EImmortalSpiritRoot Root);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Element") static FText GetElementText(EImmortalElementType Element);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation Path") static TArray<EImmortalCultivationPath> GetKnownCultivationPaths();
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation Path") static bool GetCultivationPathDefinition(EImmortalCultivationPath Path, FImmortalCultivationPathDefinition& OutDefinition);
	static void NormalizeCultivationPath(FImmortalCultivationPathState& State);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation Path") static FImmortalCharacterPathBonuses CalculatePathBonuses(const FImmortalCultivationPathState& State);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation Path") static FImmortalCraftingCost GetPathSwitchCost(const FImmortalCultivationPathState& State, EImmortalCultivationPath NewPath);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation Path") static bool IsEquipmentCompatible(EImmortalCultivationPath Path, EImmortalEquipmentDiscipline Discipline);
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation Path") static FLinearColor GetCultivationPathColor(EImmortalCultivationPath Path);
};
