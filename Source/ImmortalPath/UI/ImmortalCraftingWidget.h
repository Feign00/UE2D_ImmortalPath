// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalCraftingWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;
class UVerticalBox;
class UImage;
class UTexture2D;

UCLASS()
class IMMORTALPATH_API UImmortalCraftingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();
	void SelectRecipe(FName RecipeId);
	void SelectEquipment(FGuid ItemId);
	UTexture2D* GetEquipmentAtlas() const;
	UTexture2D* GetForgeAtlas() const;
	UTexture2D* GetMaterialAtlas() const;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Art")
	TSoftObjectPtr<UTexture2D> EquipmentAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_EquipmentAtlas.T_EquipmentAtlas")));
	UPROPERTY(EditDefaultsOnly, Category="Art")
	TSoftObjectPtr<UTexture2D> ForgeAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_ForgeAtlas.T_ForgeAtlas")));
	UPROPERTY(EditDefaultsOnly, Category="Art")
	TSoftObjectPtr<UTexture2D> MaterialAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_MaterialAtlas.T_MaterialAtlas")));
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RebuildRecipeEntries();
	void RebuildEquipmentEntries();
	void RefreshRecipeDetails();
	void RefreshEquipmentDetails();
	void RefreshCostList(UVerticalBox* List, const struct FImmortalCraftingCost& Cost);

	UFUNCTION() void HandleCraftClicked();
	UFUNCTION() void HandleEnhanceClicked();
	UFUNCTION() void HandleRefineClicked();
	UFUNCTION() void HandleArtifactFurnaceClicked();
	UFUNCTION() void HandleCloseClicked();

	UPROPERTY(Transient) TWeakObjectPtr<AImmortalPlayerCharacter> Player;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> RecipeList;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> EquipmentList;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> RecipeCostList;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> EnhancementCostList;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> RefinementCostList;
	UPROPERTY(Transient) TObjectPtr<UImage> RecipeIcon;
	UPROPERTY(Transient) TObjectPtr<UImage> ItemIcon;
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
	FName LastRenderedRecipeId = NAME_None;
	FGuid LastRenderedItemId;
	int32 LastEquipmentRevision = INDEX_NONE;
	int32 LastMaterialRevision = INDEX_NONE;
	int32 LastSpiritStones = INDEX_NONE;
	int32 LastStage = INDEX_NONE;
	int32 LastCaveRevision = INDEX_NONE;
};
