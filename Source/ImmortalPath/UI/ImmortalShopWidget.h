// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalShopWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;
class UVerticalBox;

/** Native three-column Treasure Pavilion: buy daily stock and sell backpack equipment/materials. */
UCLASS()
class IMMORTALPATH_API UImmortalShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();
	void SelectOffer(FGuid ListingId);
	void SelectEquipmentForSale(FGuid ItemId);
	void SelectMaterialForSale(FName MaterialId);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	enum class ESaleSelection : uint8
	{
		None,
		Equipment,
		Material
	};

	void RebuildOfferEntries();
	void RebuildSaleEntries();
	void RefreshOfferDetails();
	void RefreshSaleDetails();
	void RefreshHeader();
	void SetResultMessage(const FText& Message, bool bSucceeded);

	UFUNCTION() void HandleBuyClicked();
	UFUNCTION() void HandleRefreshClicked();
	UFUNCTION() void HandleSellOneClicked();
	UFUNCTION() void HandleSellAllClicked();
	UFUNCTION() void HandleCloseClicked();

	UPROPERTY(Transient) TWeakObjectPtr<AImmortalPlayerCharacter> Player;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> OfferList;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> SaleList;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CurrencyText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OfferNameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OfferMetaText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OfferDetailText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OfferPriceText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RefreshCostText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SaleNameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SaleDetailText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultText;
	UPROPERTY(Transient) TObjectPtr<UButton> BuyButton;
	UPROPERTY(Transient) TObjectPtr<UButton> RefreshButton;
	UPROPERTY(Transient) TObjectPtr<UButton> SellOneButton;
	UPROPERTY(Transient) TObjectPtr<UButton> SellAllButton;

	FGuid SelectedListingId;
	FGuid SelectedEquipmentId;
	FName SelectedMaterialId = NAME_None;
	ESaleSelection SaleSelection = ESaleSelection::None;
	int32 LastShopRevision = INDEX_NONE;
	int32 LastEquipmentRevision = INDEX_NONE;
	int32 LastMaterialRevision = INDEX_NONE;
	int32 LastSpiritStones = INDEX_NONE;
	int64 LastRefreshSeconds = INDEX_NONE;
};

