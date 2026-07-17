// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalEquipmentTypes.generated.h"

UENUM(BlueprintType)
enum class EImmortalEquipmentSlot : uint8
{
	Weapon UMETA(DisplayName = "Weapon"),
	Head UMETA(DisplayName = "Head"),
	Chest UMETA(DisplayName = "Chest"),
	Boots UMETA(DisplayName = "Boots"),
	Accessory UMETA(DisplayName = "Accessory"),
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EImmortalEquipmentQuality : uint8
{
	Common UMETA(DisplayName = "Common"),
	Uncommon UMETA(DisplayName = "Uncommon"),
	Rare UMETA(DisplayName = "Rare"),
	Epic UMETA(DisplayName = "Epic"),
	Legendary UMETA(DisplayName = "Legendary")
};

UENUM(BlueprintType)
enum class EImmortalEquipmentAffixType : uint8
{
	Attack UMETA(DisplayName = "Attack"),
	Defense UMETA(DisplayName = "Defense"),
	Health UMETA(DisplayName = "Health"),
	AttackSpeed UMETA(DisplayName = "Attack Speed"),
	CriticalChance UMETA(DisplayName = "Critical Chance")
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

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalEquipmentItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment") FGuid ItemId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment") FName DisplayName = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment") EImmortalEquipmentSlot Slot = EImmortalEquipmentSlot::Weapon;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment") EImmortalEquipmentQuality Quality = EImmortalEquipmentQuality::Common;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment", meta = (ClampMin = "1")) int32 ItemLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment", meta = (ClampMin = "0", ClampMax = "15")) int32 EnhancementLevel = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Equipment", meta = (ClampMin = "0")) int32 RefinementCount = 0;

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
		EImmortalEquipmentQuality Quality);

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

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	static FLinearColor GetQualityColor(EImmortalEquipmentQuality Quality);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	static FText GetQualityText(EImmortalEquipmentQuality Quality);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	static FText GetSlotText(EImmortalEquipmentSlot Slot);
};
