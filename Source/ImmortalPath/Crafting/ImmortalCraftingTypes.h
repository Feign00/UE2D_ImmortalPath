// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalCraftingTypes.generated.h"

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalCraftingMaterialCost
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	FName MaterialId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "1"))
	int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalCraftingCost
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	TArray<FImmortalCraftingMaterialCost> Materials;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "0"))
	int32 SpiritStones = 0;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalCraftingRecipeDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	EImmortalEquipmentSlot OutputSlot = EImmortalEquipmentSlot::Weapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	EImmortalEquipmentQuality OutputQuality = EImmortalEquipmentQuality::Uncommon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = "1", ClampMax = "999"))
	int32 MinimumQingyunStage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FImmortalCraftingCost Cost;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalCraftingResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crafting") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Crafting") bool bUnlocked = false;
	UPROPERTY(BlueprintReadOnly, Category = "Crafting") bool bAffordable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Crafting") FName RecipeId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Crafting") FGuid ItemId;
	UPROPERTY(BlueprintReadOnly, Category = "Crafting") FText Message;
};

UCLASS()
class IMMORTALPATH_API UImmortalCraftingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Crafting")
	static TArray<FName> GetKnownRecipeIds();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Crafting")
	static bool GetRecipeDefinition(FName RecipeId, FImmortalCraftingRecipeDefinition& OutDefinition);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Crafting")
	static bool IsRecipeUnlocked(const FImmortalCraftingRecipeDefinition& Definition, int32 QingyunStage);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Crafting")
	static bool CanAfford(
		const TArray<FImmortalMaterialStack>& Materials,
		int32 SpiritStones,
		const FImmortalCraftingCost& Cost);

	/** Transactional: neither materials nor spirit stones change unless the complete cost is affordable. */
	static bool ConsumeCost(
		TArray<FImmortalMaterialStack>& Materials,
		int32& SpiritStones,
		const FImmortalCraftingCost& Cost);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Crafting")
	static FImmortalCraftingCost GetEnhancementCost(const FImmortalEquipmentItem& Item);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Crafting")
	static FImmortalCraftingCost GetRefinementCost(const FImmortalEquipmentItem& Item);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Crafting")
	static FText FormatCost(
		const FImmortalCraftingCost& Cost,
		const TArray<FImmortalMaterialStack>& Materials,
		int32 SpiritStones);
};
