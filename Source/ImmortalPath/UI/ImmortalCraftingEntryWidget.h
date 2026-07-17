// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalCraftingEntryWidget.generated.h"

class UButton;
class UImmortalCraftingWidget;
class UTextBlock;

UCLASS()
class IMMORTALPATH_API UImmortalCraftingEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeRecipeEntry(UImmortalCraftingWidget* InOwner, FName InRecipeId, bool bUnlocked, bool bSelected);
	void InitializeEquipmentEntry(UImmortalCraftingWidget* InOwner, const struct FImmortalEquipmentItem& Item, bool bEquipped, bool bSelected);

protected:
	virtual void NativeOnInitialized() override;

private:
	void RefreshAppearance();

	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient) TWeakObjectPtr<UImmortalCraftingWidget> OwnerCrafting;
	UPROPERTY(Transient) TObjectPtr<UButton> EntryButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EntryText;
	FName RecipeId = NAME_None;
	FGuid ItemId;
	FText DisplayText;
	FLinearColor DisplayColor = FLinearColor::White;
	bool bRecipeEntry = true;
	bool bEnabled = true;
	bool bSelected = false;
};

