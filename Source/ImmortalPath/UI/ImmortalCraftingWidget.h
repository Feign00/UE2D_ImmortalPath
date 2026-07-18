// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalCraftingWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;
class UVerticalBox;

UCLASS()
class IMMORTALPATH_API UImmortalCraftingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();
	void SelectRecipe(FName RecipeId);
	void SelectEquipment(FGuid ItemId);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RebuildRecipeEntries();
	void RebuildEquipmentEntries();
	void RefreshRecipeDetails();
	void RefreshEquipmentDetails();

	UFUNCTION() void HandleCraftClicked();
	UFUNCTION() void HandleEnhanceClicked();
	UFUNCTION() void HandleRefineClicked();
	UFUNCTION() void HandleArtifactFurnaceClicked();
	UFUNCTION() void HandleCloseClicked();

	UPROPERTY(Transient) TWeakObjectPtr<AImmortalPlayerCharacter> Player;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> RecipeList;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> EquipmentList;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CurrencyText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RecipeNameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RecipeDescriptionText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RecipeCostText;
	UPROPERTY(Transient) TObjectPtr<UButton> CraftButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CraftButtonText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ItemNameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ItemStatsText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AffixText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EnhancementCostText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RefinementCostText;
	UPROPERTY(Transient) TObjectPtr<UButton> EnhanceButton;
	UPROPERTY(Transient) TObjectPtr<UButton> RefineButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultText;

	FName SelectedRecipeId = NAME_None;
	FGuid SelectedItemId;
	int32 LastEquipmentRevision = INDEX_NONE;
	int32 LastMaterialRevision = INDEX_NONE;
	int32 LastSpiritStones = INDEX_NONE;
	int32 LastStage = INDEX_NONE;
};
