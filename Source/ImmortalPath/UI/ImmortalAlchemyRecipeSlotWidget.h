// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalAlchemyRecipeSlotWidget.generated.h"

class UButton;
class UImmortalAlchemyWidget;
class UTextBlock;

UCLASS()
class IMMORTALPATH_API UImmortalAlchemyRecipeSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeRecipe(UImmortalAlchemyWidget* InOwner, FName InRecipeId, bool bUnlocked, bool bSelected);

protected:
	virtual void NativeOnInitialized() override;

private:
	void RefreshAppearance();

	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<UImmortalAlchemyWidget> OwnerAlchemy;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;

	FName RecipeId = NAME_None;
	bool bRecipeUnlocked = false;
	bool bRecipeSelected = false;
};

