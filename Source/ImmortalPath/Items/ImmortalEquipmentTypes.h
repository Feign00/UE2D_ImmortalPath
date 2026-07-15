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

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalEquipmentItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment") FGuid ItemId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment") FName DisplayName = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment") EImmortalEquipmentSlot Slot = EImmortalEquipmentSlot::Weapon;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment") EImmortalEquipmentQuality Quality = EImmortalEquipmentQuality::Common;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (ClampMin = "1")) int32 ItemLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Stats") float AttackBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Stats") float DefenseBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Stats") float HealthBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Stats") float AttackSpeedBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Stats") float CriticalChanceBonus = 0.0f;

	bool IsValid() const { return ItemId.IsValid(); }
};

UCLASS()
class IMMORTALPATH_API UImmortalEquipmentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Equipment")
	static FImmortalEquipmentItem GenerateRandomEquipment(int32 ItemLevel = 1);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	static float CalculateEquipmentPower(const FImmortalEquipmentItem& Item);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	static FLinearColor GetQualityColor(EImmortalEquipmentQuality Quality);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	static FText GetQualityText(EImmortalEquipmentQuality Quality);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	static FText GetSlotText(EImmortalEquipmentSlot Slot);
};
