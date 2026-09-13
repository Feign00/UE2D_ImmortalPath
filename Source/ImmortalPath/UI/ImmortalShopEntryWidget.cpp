// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalShopEntryWidget.h"

#include "ImmortalShopWidget.h"
#include "ImmortalUITheme.h"
#include "ImmortalFeaturePageLayout.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "../Shop/ImmortalShopTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

void UImmortalShopEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShopEntrySize"));
	Root->SetWidthOverride(292.0f);
	Root->SetHeightOverride(92.0f);
	WidgetTree->RootWidget = Root;

	EntryButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShopEntryButton"));
	EntryButton->OnClicked.AddDynamic(this, &UImmortalShopEntryWidget::HandleClicked);
	Root->AddChild(EntryButton);

	EntryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopEntryText"));
	EntryText->SetJustification(ETextJustify::Left);
	EntryText->SetAutoWrapText(false);
	EntryText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	EntryText->SetShadowOffset(FVector2D(1.0f));
	EntryText->SetShadowColorAndOpacity(FLinearColor::Black);
	FSlateFontInfo Font = EntryText->GetFont();
	Font.Size = 18;
	EntryText->SetFont(Font);
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Row->SetVisibility(ESlateVisibility::HitTestInvisible);
	UButtonSlot* ContentSlot = Cast<UButtonSlot>(EntryButton->AddChild(Row));
	ContentSlot->SetHorizontalAlignment(HAlign_Fill);
	ContentSlot->SetVerticalAlignment(VAlign_Fill);
	ContentSlot->SetPadding(FMargin(6));
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>();
	IconBox->SetWidthOverride(68); IconBox->SetHeightOverride(68);
	ProductIcon = CreateWidget<UImmortalIconWidget>(this);
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
	IconBox->AddChild(Layers);
	ProductArt = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ShopEntryArt"));
	UOverlaySlot* ArtSlot = Layers->AddChildToOverlay(ProductArt);
	ArtSlot->SetHorizontalAlignment(HAlign_Fill);
	ArtSlot->SetVerticalAlignment(VAlign_Fill);

	Layers->AddChildToOverlay(ProductIcon);
	Row->AddChildToHorizontalBox(IconBox)->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(EntryText);
	TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TextSlot->SetPadding(FMargin(5,0)); TextSlot->SetVerticalAlignment(VAlign_Center);

	RefreshAppearance();
}

void UImmortalShopEntryWidget::InitializeOfferEntry(
	UImmortalShopWidget* InOwner,
	const FImmortalShopListing& InListing,
	const bool bInSelected)
{
	OwnerShop = InOwner;
	EntryMode = EEntryMode::Offer;
	ArtBrush = InOwner->GetOfferArt(InListing);
	EntryId = InListing.ListingId;
	MaterialId = NAME_None;
	bSelected = bInSelected;
	bSoldOut = InListing.bSoldOut;
	DisplayColor = UImmortalShopLibrary::GetListingColor(InListing);
	DisplayText = FText::FromString(FString::Printf(
		TEXT("%s%s\n%s  ·  %d 灵石"),
		bSoldOut ? TEXT("[已售罄] ") : TEXT(""),
		*UImmortalShopLibrary::GetListingDisplayName(InListing).ToString(),
		*UImmortalShopLibrary::GetProductTypeText(InListing.ProductType).ToString(),
		InListing.BundlePrice));
	RefreshAppearance();
}

void UImmortalShopEntryWidget::InitializeEquipmentSaleEntry(
	UImmortalShopWidget* InOwner,
	const FImmortalEquipmentItem& InItem,
	const int32 SellPrice,
	const bool bInSelected)
{
	OwnerShop = InOwner;
	EntryMode = EEntryMode::EquipmentSale;
	ArtBrush = InOwner->GetEquipmentArt(InItem.Slot);
	EntryId = InItem.ItemId;
	MaterialId = NAME_None;
	bSelected = bInSelected;
	bSoldOut = InItem.bLocked;
	DisplayColor = UImmortalEquipmentLibrary::GetQualityColor(InItem.Quality);
	const FString ItemName = InItem.DisplayName.IsNone()
		? UImmortalEquipmentLibrary::GetSlotText(InItem.Slot).ToString()
		: InItem.DisplayName.ToString();
	DisplayText = FText::FromString(FString::Printf(
		TEXT("%s%s\n%s  ·  %s"),
		InItem.bLocked ? TEXT("[已锁定] ") : TEXT(""),
		*ItemName,
		*UImmortalEquipmentLibrary::GetQualityText(InItem.Quality).ToString(),
		InItem.bLocked ? TEXT("不可出售") : *FString::Printf(TEXT("售 %d"), SellPrice)));
	RefreshAppearance();
}

void UImmortalShopEntryWidget::InitializeMaterialSaleEntry(
	UImmortalShopWidget* InOwner,
	const FImmortalMaterialStack& InStack,
	const int32 UnitSellPrice,
	const bool bInSelected)
{
	OwnerShop = InOwner;
	EntryMode = EEntryMode::MaterialSale;
	ArtBrush = InOwner->GetMaterialArt(InStack.MaterialId);
	EntryId.Invalidate();
	MaterialId = InStack.MaterialId;
	bSelected = bInSelected;
	bSoldOut = false;
	FImmortalMaterialDefinition Definition;
	const bool bHasDefinition = UImmortalMaterialLibrary::GetMaterialDefinition(MaterialId, Definition);
	DisplayColor = bHasDefinition ? Definition.DisplayColor : FLinearColor(0.84f, 0.86f, 0.9f, 1.0f);
	const FString Name = bHasDefinition ? Definition.DisplayName.ToString() : MaterialId.ToString();
	DisplayText = FText::FromString(FString::Printf(
		TEXT("%s  ×%d\n单价 %d 灵石"),
		*Name,
		InStack.Quantity,
		UnitSellPrice));
	RefreshAppearance();
}

void UImmortalShopEntryWidget::RefreshAppearance()
{
	if (!EntryButton || !EntryText)
	{
		return;
	}

	EntryButton->SetStyle(ImmortalUITheme::ButtonStyle(bSelected));
	ProductArt->SetBrush(ArtBrush);
	ProductIcon->SetIcon(4);
	ProductIcon->SetVisibility(ArtBrush.GetResourceObject() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	EntryButton->SetToolTipText(DisplayText);
	// Unavailable entries remain inspectable; the transaction controls enforce availability.
	EntryButton->SetIsEnabled(true);
	EntryText->SetText(DisplayText);
	EntryText->SetColorAndOpacity(FSlateColor(bSoldOut
		? FLinearColor(0.55f, 0.55f, 0.58f, 1.0f)
		: DisplayColor));
}

void UImmortalShopEntryWidget::HandleClicked()
{
	if (!OwnerShop.IsValid())
	{
		return;
	}

	switch (EntryMode)
	{
	case EEntryMode::Offer:
		OwnerShop->SelectOffer(EntryId);
		break;
	case EEntryMode::EquipmentSale:
		OwnerShop->SelectEquipmentForSale(EntryId);
		break;
	case EEntryMode::MaterialSale:
		OwnerShop->SelectMaterialForSale(MaterialId);
		break;
	default:
		break;
	}
}
