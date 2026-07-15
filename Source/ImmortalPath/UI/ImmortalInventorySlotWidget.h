// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalInventorySlotWidget.generated.h"

class UButton;
class UImage;
class UImmortalInventoryWidget;
class UTextBlock;

/** One 96x96 equipment/backpack cell assembled from the supplied UI textures. */
UCLASS()
class IMMORTALPATH_API UImmortalInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeSlot(
		UImmortalInventoryWidget* InOwner,
		const FImmortalEquipmentItem& InItem,
		bool bInHasItem,
		bool bInEquipped,
		bool bInSelected,
		EImmortalEquipmentSlot InPlaceholderSlot = EImmortalEquipmentSlot::MAX);

	const FImmortalEquipmentItem& GetItem() const { return Item; }
	bool HasItem() const { return bHasItem; }

protected:
	virtual void NativeOnInitialized() override;

private:
	void RefreshAppearance();

	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<UImmortalInventoryWidget> OwnerInventory;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SlotButton;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(Transient)
	TObjectPtr<UImage> QualityFrame;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LevelText;

	FImmortalEquipmentItem Item;
	EImmortalEquipmentSlot PlaceholderSlot = EImmortalEquipmentSlot::MAX;
	bool bHasItem = false;
	bool bEquipped = false;
	bool bSelected = false;
};
