// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Alchemy/ImmortalAlchemyTypes.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalAlchemyWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;

/** Native 900x600 furnace: data-driven recipes, result feedback and pill inventory/use. */
UCLASS()
class IMMORTALPATH_API UImmortalAlchemyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();
	void SelectRecipe(FName RecipeId);
	void SelectPill(FName PillId, EImmortalPillQuality Quality);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RebuildRecipes();
	void RebuildPills();
	void RefreshRecipeDetails();
	void RefreshPillDetails();

	UFUNCTION()
	void HandleCraftClicked();

	UFUNCTION()
	void HandleUseClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RecipeList;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> PillGrid;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RecipeNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RecipeDescriptionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> IngredientText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ChanceText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RecipeEffectText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CraftButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CraftButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PillNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PillEffectText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BoostText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> UseButton;

	FName SelectedRecipeId = NAME_None;
	FName SelectedPillId = NAME_None;
	EImmortalPillQuality SelectedPillQuality = EImmortalPillQuality::Ordinary;
	int32 LastMaterialRevision = INDEX_NONE;
	int32 LastPillRevision = INDEX_NONE;
	int32 LastRealmIndex = INDEX_NONE;
	int32 LastMinorStage = INDEX_NONE;
};

