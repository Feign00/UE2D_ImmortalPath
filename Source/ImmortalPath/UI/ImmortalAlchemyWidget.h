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
class UTexture2D;

/** Native 1600x600 furnace: illustrated recipes, result feedback and pill inventory/use. */
UCLASS()
class IMMORTALPATH_API UImmortalAlchemyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();
	void SelectRecipe(FName RecipeId);
	void SelectPill(FName PillId, EImmortalPillQuality Quality);
	UTexture2D* GetAlchemyAtlas() const;
	UTexture2D* GetMaterialAtlas() const;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Art")
	TSoftObjectPtr<UTexture2D> AlchemyAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_AlchemyAtlas.T_AlchemyAtlas")));
	UPROPERTY(EditDefaultsOnly, Category="Art")
	TSoftObjectPtr<UTexture2D> MaterialAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_MaterialAtlas.T_MaterialAtlas")));

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
	TObjectPtr<UVerticalBox> IngredientList;

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
	int32 LastCaveRevision = INDEX_NONE;
};
