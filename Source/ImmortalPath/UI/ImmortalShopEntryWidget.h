// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalShopEntryWidget.generated.h"

class UButton;
class UImmortalShopWidget;
class UTextBlock;
struct FImmortalEquipmentItem;
struct FImmortalMaterialStack;
struct FImmortalShopListing;

/** Compact selectable row shared by the shop catalog and the two sale inventories. */
UCLASS()
class IMMORTALPATH_API UImmortalShopEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeOfferEntry(
		UImmortalShopWidget* InOwner,
		const FImmortalShopListing& InListing,
		bool bInSelected);
	void InitializeEquipmentSaleEntry(
		UImmortalShopWidget* InOwner,
		const FImmortalEquipmentItem& InItem,
		int32 SellPrice,
		bool bInSelected);
	void InitializeMaterialSaleEntry(
		UImmortalShopWidget* InOwner,
		const FImmortalMaterialStack& InStack,
		int32 UnitSellPrice,
		bool bInSelected);

protected:
	virtual void NativeOnInitialized() override;

private:
	enum class EEntryMode : uint8
	{
		Offer,
		EquipmentSale,
		MaterialSale
	};

	void RefreshAppearance();

	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<UImmortalShopWidget> OwnerShop;

	UPROPERTY(Transient)
	TObjectPtr<UButton> EntryButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EntryText;

	EEntryMode EntryMode = EEntryMode::Offer;
	FGuid EntryId;
	FName MaterialId = NAME_None;
	FText DisplayText;
	FLinearColor DisplayColor = FLinearColor::White;
	bool bSelected = false;
	bool bSoldOut = false;
};

