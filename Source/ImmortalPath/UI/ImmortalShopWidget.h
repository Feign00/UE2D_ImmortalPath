// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "ImmortalShopWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;
class UVerticalBox;
class UImage;
class UTexture2D;
class UImmortalIconWidget;
struct FImmortalShopListing;

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

	FSlateBrush GetOfferArt(const FImmortalShopListing& Listing);
	FSlateBrush GetEquipmentArt(EImmortalEquipmentSlot EquipmentSlot);
	FSlateBrush GetMaterialArt(FName Id);

protected:
	UPROPERTY(EditDefaultsOnly, Category="Art")
	TSoftObjectPtr<UTexture2D> EquipmentAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_EquipmentAtlas.T_EquipmentAtlas")));
	UPROPERTY(EditDefaultsOnly, Category="Art")
	TSoftObjectPtr<UTexture2D> ForgeAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_ForgeAtlas.T_ForgeAtlas")));
	UPROPERTY(EditDefaultsOnly, Category="Art")
	TSoftObjectPtr<UTexture2D> MaterialAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_MaterialAtlas.T_MaterialAtlas")));
	UPROPERTY(EditDefaultsOnly, Category="Art")
	TSoftObjectPtr<UTexture2D> AlchemyAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_AlchemyAtlas.T_AlchemyAtlas")));
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
	UPROPERTY(Transient) TObjectPtr<UImage> OfferIcon;
	UPROPERTY(Transient) TObjectPtr<UImage> SaleIcon;
	UPROPERTY(Transient) TObjectPtr<UImmortalIconWidget> OfferFallback;
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
