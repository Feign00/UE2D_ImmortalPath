// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalShopEntryWidget.h"

#include "ImmortalShopWidget.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "../Shop/ImmortalShopTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

namespace
{
	FSlateBrush MakeShopEntryBrush(const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = FVector2D(270.0f, 56.0f);
		Brush.TintColor = FSlateColor(Tint);
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal")))
		{
			Brush.SetResourceObject(Texture);
		}
		return Brush;
	}
}

void UImmortalShopEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShopEntrySize"));
	Root->SetWidthOverride(270.0f);
	Root->SetHeightOverride(56.0f);
	WidgetTree->RootWidget = Root;

	EntryButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShopEntryButton"));
	EntryButton->OnClicked.AddDynamic(this, &UImmortalShopEntryWidget::HandleClicked);
	Root->AddChild(EntryButton);

	EntryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopEntryText"));
	EntryText->SetJustification(ETextJustify::Center);
	EntryText->SetAutoWrapText(true);
	EntryText->SetShadowOffset(FVector2D(1.0f));
	EntryText->SetShadowColorAndOpacity(FLinearColor::Black);
	FSlateFontInfo Font = EntryText->GetFont();
	Font.Size = 14;
	EntryText->SetFont(Font);
	EntryButton->AddChild(EntryText);

	RefreshAppearance();
}

void UImmortalShopEntryWidget::InitializeOfferEntry(
	UImmortalShopWidget* InOwner,
	const FImmortalShopListing& InListing,
	const bool bInSelected)
{
	OwnerShop = InOwner;
	EntryMode = EEntryMode::Offer;
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
	EntryId = InItem.ItemId;
	MaterialId = NAME_None;
	bSelected = bInSelected;
	bSoldOut = false;
	DisplayColor = UImmortalEquipmentLibrary::GetQualityColor(InItem.Quality);
	const FString ItemName = InItem.DisplayName.IsNone()
		? UImmortalEquipmentLibrary::GetSlotText(InItem.Slot).ToString()
		: InItem.DisplayName.ToString();
	DisplayText = FText::FromString(FString::Printf(
		TEXT("%s  +%d\n%s  ·  售 %d"),
		*ItemName,
		InItem.EnhancementLevel,
		*UImmortalEquipmentLibrary::GetQualityText(InItem.Quality).ToString(),
		SellPrice));
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

	const FLinearColor NormalTint = bSoldOut
		? FLinearColor(0.2f, 0.2f, 0.22f, 0.68f)
		: (bSelected
			? FLinearColor(0.52f, 0.36f, 0.12f, 1.0f)
			: FLinearColor(0.31f, 0.28f, 0.24f, 0.9f));
	const FLinearColor HoverTint = bSoldOut
		? FLinearColor(0.28f, 0.27f, 0.28f, 0.78f)
		: FLinearColor(0.64f, 0.45f, 0.15f, 1.0f);
	FButtonStyle Style;
	Style.SetNormal(MakeShopEntryBrush(NormalTint));
	Style.SetHovered(MakeShopEntryBrush(HoverTint));
	Style.SetPressed(MakeShopEntryBrush(FLinearColor(0.75f, 0.54f, 0.2f, 1.0f)));
	EntryButton->SetStyle(Style);
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

