// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalEquipmentTypes.generated.h"

UENUM(BlueprintType)
enum class EImmortalEquipmentSlot : uint8
{
	Weapon UMETA(DisplayName = "Weapon"),
	Head UMETA(DisplayName = "Head"),
	Chest UMETA(DisplayName = "Chest"),
	Boots UMETA(DisplayName = "Boots"),
	/** Kept at value 4 for old saves; displayed as Necklace from save version 17 onward. */
	Accessory UMETA(DisplayName = "Necklace"),
	Bracers UMETA(DisplayName = "Bracers"),
	Belt UMETA(DisplayName = "Belt"),
	RingLeft UMETA(DisplayName = "Left Ring"),
	RingRight UMETA(DisplayName = "Right Ring"),
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EImmortalEquipmentQuality : uint8
{
	Common UMETA(DisplayName = "Common"),
	Uncommon UMETA(DisplayName = "Uncommon"),
	Rare UMETA(DisplayName = "Rare"),
	Epic UMETA(DisplayName = "Epic"),
	Legendary UMETA(DisplayName = "Legendary"),
	Immortal UMETA(DisplayName = "Immortal"),
	Divine UMETA(DisplayName = "Divine")
};

UENUM(BlueprintType)
enum class EImmortalEquipmentAffixType : uint8
{
	Attack UMETA(DisplayName = "Attack"),
	Defense UMETA(DisplayName = "Defense"),
	Health UMETA(DisplayName = "Health"),
	AttackSpeed UMETA(DisplayName = "Attack Speed"),
	CriticalChance UMETA(DisplayName = "Critical Chance"),
	CriticalDamage UMETA(DisplayName = "Critical Damage"),
	FireDamage UMETA(DisplayName = "Fire Damage"),
	ThunderDamage UMETA(DisplayName = "Thunder Damage"),
	IceDamage UMETA(DisplayName = "Ice Damage"),
	LifeSteal UMETA(DisplayName = "Life Steal"),
	CultivationGain UMETA(DisplayName = "Cultivation Gain"),
	LootFind UMETA(DisplayName = "Loot Find"),
	BossDamage UMETA(DisplayName = "Boss Damage"),
	MAX UMETA(Hidden)
};

/** Cultivation discipline required to activate/equip an item. Universal items work for every path. */
UENUM(BlueprintType)
enum class EImmortalEquipmentDiscipline : uint8
{
	Universal UMETA(DisplayName = "Universal"),
	Body UMETA(DisplayName = "Body Cultivation"),
	Dharma UMETA(DisplayName = "Dharma Cultivation"),
	Sword UMETA(DisplayName = "Sword Cultivation"),
	Poison UMETA(DisplayName = "Poison Cultivation"),
	Thunder UMETA(DisplayName = "Thunder Cultivation"),
	MAX UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalEquipmentAffix
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Affix")
	EImmortalEquipmentAffixType Type = EImmortalEquipmentAffixType::Attack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Affix")
	float Value = 0.0f;
};

/** One active threshold in a named equipment set. Percentage fields are additive multipliers. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalEquipmentSetTier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set", meta = (ClampMin = "2", ClampMax = "6"))
	int32 RequiredPieces = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set") FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set|Stats") float AttackMultiplierBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set|Stats") float DefenseMultiplierBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set|Stats") float HealthMultiplierBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set|Stats") float AttackSpeedBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set|Stats") float CriticalChanceBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set|Stats") float CriticalDamageBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set|Stats") float ThunderDamageBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set|Stats") float CultivationGainBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set|Stats") float BossDamageBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set|Stats") float FinalDamageBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set|Stats") float DamageReductionBonus = 0.0f;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalEquipmentSetDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set") FName SetId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set") FLinearColor DisplayColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set") TArray<FImmortalEquipmentSetTier> Tiers;
};

/** Aggregated bonuses from every active 2/4/6-piece threshold. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalEquipmentSetBonuses
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Set") TMap<FName, int32> PieceCounts;
	UPROPERTY(BlueprintReadOnly, Category = "Set|Stats") float AttackMultiplierBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Set|Stats") float DefenseMultiplierBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Set|Stats") float HealthMultiplierBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Set|Stats") float AttackSpeedBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Set|Stats") float CriticalChanceBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Set|Stats") float CriticalDamageBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Set|Stats") float ThunderDamageBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Set|Stats") float CultivationGainBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Set|Stats") float BossDamageBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Set|Stats") float FinalDamageBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Set|Stats") float DamageReductionBonus = 0.0f;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalEquipmentItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment") FGuid ItemId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment") FName DisplayName = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment") EImmortalEquipmentSlot Slot = EImmortalEquipmentSlot::Weapon;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment") EImmortalEquipmentQuality Quality = EImmortalEquipmentQuality::Common;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment") EImmortalEquipmentDiscipline Discipline = EImmortalEquipmentDiscipline::Universal;
	/** None means a normal item. Named set pieces activate cumulative 2/4/6-piece bonuses. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment") FName SetId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment", meta = (ClampMin = "1")) int32 ItemLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment", meta = (ClampMin = "0", ClampMax = "15")) int32 EnhancementLevel = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment", meta = (ClampMin = "0")) int32 RefinementCount = 0;
	/** Protected equipment is excluded from sale, dismantle and full-backpack auto replacement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment") bool bLocked = false;

	/** Unenhanced core stats. Enhancement only scales these fields; refinement never changes them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Base Stats") float BaseAttackBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Base Stats") float BaseDefenseBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Base Stats") float BaseHealthBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Base Stats") float BaseAttackSpeedBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Base Stats") float BaseCriticalChanceBonus = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Affixes")
	TArray<FImmortalEquipmentAffix> Affixes;

	/** Cached totals consumed by combat and retained for compatibility with version 1-6 saves. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Stats") float AttackBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Stats") float DefenseBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Stats") float HealthBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Stats") float AttackSpeedBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Stats") float CriticalChanceBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Stats") float CriticalDamageBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Stats") float FireDamageBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Stats") float ThunderDamageBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Stats") float IceDamageBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Stats") float LifeStealBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Stats") float CultivationGainBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Stats") float LootFindBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment|Stats") float BossDamageBonus = 0.0f;

	bool IsValid() const { return ItemId.IsValid(); }
};

UCLASS()
class IMMORTALPATH_API UImmortalEquipmentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Equipment")
	static FImmortalEquipmentItem GenerateRandomEquipment(int32 ItemLevel = 1);

	/** Boss and other guaranteed loot rolls can enforce a minimum quality. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Equipment")
	static FImmortalEquipmentItem GenerateRandomEquipmentWithMinimumQuality(
		int32 ItemLevel,
		EImmortalEquipmentQuality MinimumQuality);

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Equipment")
	static FImmortalEquipmentItem GenerateCraftedEquipment(
		int32 ItemLevel,
		EImmortalEquipmentSlot Slot,
		EImmortalEquipmentQuality Quality,
		EImmortalEquipmentDiscipline Discipline = EImmortalEquipmentDiscipline::Universal,
		FName SetId = NAME_None);

	/** Migrates legacy totals into base stats, validates affixes, then rebuilds cached totals. */
	static void NormalizeForgingState(FImmortalEquipmentItem& Item);

	static void RebuildEquipmentStats(FImmortalEquipmentItem& Item);

	/** Returns false at the enhancement cap. Cost consumption belongs to the crafting transaction layer. */
	static bool EnhanceEquipment(FImmortalEquipmentItem& Item, int32 MaximumEnhancementLevel = 15);

	/** Keeps slot, level, quality, base stats, ID and enhancement while rerolling all random affixes. */
	static bool RerollEquipmentAffixes(FImmortalEquipmentItem& Item);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	static FText GetAffixText(const FImmortalEquipmentAffix& Affix);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	static float CalculateEquipmentPower(const FImmortalEquipmentItem& Item);

	/** Build-aware comparison score including cumulative 2/4/6-piece effects. */
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	static float CalculateLoadoutPower(const TArray<FImmortalEquipmentItem>& EquippedItems);

	/** Character-aware comparison so set percentages also value base progression stats. */
	static float CalculateLoadoutPowerWithBaseStats(
		const TArray<FImmortalEquipmentItem>& EquippedItems,
		float BaseAttack,
		float BaseDefense,
		float BaseHealth);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	static FLinearColor GetQualityColor(EImmortalEquipmentQuality Quality);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	static FText GetQualityText(EImmortalEquipmentQuality Quality);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	static FText GetSlotText(EImmortalEquipmentSlot Slot);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	static FText GetDisciplineText(EImmortalEquipmentDiscipline Discipline);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment|Set")
	static TArray<FName> GetKnownSetIds();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment|Set")
	static bool GetSetDefinition(FName SetId, FImmortalEquipmentSetDefinition& OutDefinition);

	static FImmortalEquipmentSetBonuses CalculateSetBonuses(const TArray<FImmortalEquipmentItem>& EquippedItems);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment|Set")
	static FText GetSetSummaryText(const TArray<FImmortalEquipmentItem>& EquippedItems);
};
