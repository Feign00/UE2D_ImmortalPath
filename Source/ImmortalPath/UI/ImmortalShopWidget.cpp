// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalShopWidget.h"

#include "ImmortalShopEntryWidget.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "../Shop/ImmortalShopTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

namespace
{
	FSlateBrush MakeShopBrush(const TCHAR* AssetPath, const FVector2D Size, const FLinearColor Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(Tint);
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, AssetPath))
		{
			Brush.SetResourceObject(Texture);
		}
		return Brush;
	}

	void SetShopLayout(UCanvasPanelSlot* Slot, const FVector2D Position, const FVector2D Size)
	{
		if (!Slot)
		{
			return;
		}
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetAutoSize(false);
	}

	void StyleShopText(UTextBlock* Text, const int32 Size, const FLinearColor& Color)
	{
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor::Black);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}

	FButtonStyle MakeShopButtonStyle(const FVector2D Size, const FLinearColor& Tint)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeShopBrush(TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal"), Size, Tint));
		Style.SetHovered(MakeShopBrush(TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal"), Size,
			Tint + FLinearColor(0.12f, 0.1f, 0.04f, 0.0f)));
		Style.SetPressed(MakeShopBrush(TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal"), Size,
			Tint * FLinearColor(0.78f, 0.78f, 0.78f, 1.0f)));
		Style.SetDisabled(MakeShopBrush(TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal"), Size,
			FLinearColor(0.2f, 0.2f, 0.2f, 0.55f)));
		return Style;
	}

	UTextBlock* AddShopButtonLabel(UWidgetTree* Tree, UButton* Button, const TCHAR* Label, const int32 FontSize = 15)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		Text->SetJustification(ETextJustify::Center);
		StyleShopText(Text, FontSize, FLinearColor::White);
		Button->AddChild(Text);
		return Text;
	}

	FString FormatCountdown(const int64 TotalSeconds)
	{
		const int64 SafeSeconds = FMath::Max<int64>(TotalSeconds, 0);
		const int64 Hours = SafeSeconds / 3600;
		const int64 Minutes = (SafeSeconds % 3600) / 60;
		const int64 Seconds = SafeSeconds % 60;
		return FString::Printf(TEXT("%02lld:%02lld:%02lld"), Hours, Minutes, Seconds);
	}
}

void UImmortalShopWidget::InitializeForPlayer(AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalShopWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShopPanelSize"));
	Root->SetWidthOverride(900.0f);
	Root->SetHeightOverride(600.0f);
	WidgetTree->RootWidget = Root;

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShopCanvas"));
	Root->AddChild(Canvas);

	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ShopBackground"));
	Background->SetBrush(MakeShopBrush(
		TEXT("/Game/GAME/Asset/ui/inventory/panel_background.panel_background"),
		FVector2D(900.0f, 600.0f),
		FLinearColor(0.92f, 0.84f, 0.64f, 0.98f)));
	SetShopLayout(Canvas->AddChildToCanvas(Background), FVector2D::ZeroVector, FVector2D(900.0f, 600.0f));

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopTitle"));
	Title->SetText(FText::FromString(TEXT("百宝阁 · 每日珍品")));
	StyleShopText(Title, 28, FLinearColor(1.0f, 0.78f, 0.28f, 1.0f));
	SetShopLayout(Canvas->AddChildToCanvas(Title), FVector2D(24.0f, 14.0f), FVector2D(350.0f, 42.0f));

	CurrencyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopCurrency"));
	CurrencyText->SetJustification(ETextJustify::Right);
	StyleShopText(CurrencyText, 15, FLinearColor(0.72f, 0.95f, 1.0f, 1.0f));
	SetShopLayout(Canvas->AddChildToCanvas(CurrencyText), FVector2D(390.0f, 20.0f), FVector2D(408.0f, 34.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShopClose"));
	CloseButton->OnClicked.AddDynamic(this, &UImmortalShopWidget::HandleCloseClicked);
	CloseButton->SetStyle(MakeShopButtonStyle(FVector2D(64.0f), FLinearColor(0.44f, 0.22f, 0.12f, 1.0f)));
	SetShopLayout(Canvas->AddChildToCanvas(CloseButton), FVector2D(816.0f, 8.0f), FVector2D(64.0f));
	AddShopButtonLabel(WidgetTree, CloseButton, TEXT("×"), 22);

	UTextBlock* OfferTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopOfferTitle"));
	OfferTitle->SetText(FText::FromString(TEXT("今日商品")));
	StyleShopText(OfferTitle, 20, FLinearColor(1.0f, 0.88f, 0.55f, 1.0f));
	SetShopLayout(Canvas->AddChildToCanvas(OfferTitle), FVector2D(20.0f, 68.0f), FVector2D(270.0f, 30.0f));
	UScrollBox* OfferScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ShopOfferScroll"));
	SetShopLayout(Canvas->AddChildToCanvas(OfferScroll), FVector2D(16.0f, 102.0f), FVector2D(278.0f, 470.0f));
	OfferList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopOfferList"));
	OfferScroll->AddChild(OfferList);

	UTextBlock* DetailTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopDetailTitle"));
	DetailTitle->SetText(FText::FromString(TEXT("商品详情")));
	StyleShopText(DetailTitle, 20, FLinearColor(1.0f, 0.88f, 0.55f, 1.0f));
	SetShopLayout(Canvas->AddChildToCanvas(DetailTitle), FVector2D(314.0f, 68.0f), FVector2D(270.0f, 30.0f));

	OfferNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopOfferName"));
	OfferNameText->SetAutoWrapText(true);
	StyleShopText(OfferNameText, 22, FLinearColor::White);
	SetShopLayout(Canvas->AddChildToCanvas(OfferNameText), FVector2D(310.0f, 106.0f), FVector2D(276.0f, 52.0f));
	OfferMetaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopOfferMeta"));
	StyleShopText(OfferMetaText, 14, FLinearColor(0.66f, 0.9f, 1.0f, 1.0f));
	SetShopLayout(Canvas->AddChildToCanvas(OfferMetaText), FVector2D(310.0f, 160.0f), FVector2D(276.0f, 28.0f));
	OfferDetailText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopOfferDetail"));
	OfferDetailText->SetAutoWrapText(true);
	StyleShopText(OfferDetailText, 14, FLinearColor(0.9f, 0.9f, 0.86f, 1.0f));
	SetShopLayout(Canvas->AddChildToCanvas(OfferDetailText), FVector2D(310.0f, 194.0f), FVector2D(276.0f, 164.0f));
	OfferPriceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopOfferPrice"));
	OfferPriceText->SetJustification(ETextJustify::Center);
	StyleShopText(OfferPriceText, 17, FLinearColor(1.0f, 0.8f, 0.32f, 1.0f));
	SetShopLayout(Canvas->AddChildToCanvas(OfferPriceText), FVector2D(310.0f, 360.0f), FVector2D(276.0f, 32.0f));

	BuyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShopBuyButton"));
	BuyButton->OnClicked.AddDynamic(this, &UImmortalShopWidget::HandleBuyClicked);
	BuyButton->SetStyle(MakeShopButtonStyle(FVector2D(170.0f, 46.0f), FLinearColor(0.55f, 0.37f, 0.08f, 1.0f)));
	SetShopLayout(Canvas->AddChildToCanvas(BuyButton), FVector2D(363.0f, 398.0f), FVector2D(170.0f, 46.0f));
	AddShopButtonLabel(WidgetTree, BuyButton, TEXT("购买"), 17);

	RefreshCostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopRefreshCost"));
	RefreshCostText->SetJustification(ETextJustify::Center);
	StyleShopText(RefreshCostText, 13, FLinearColor(0.76f, 0.82f, 0.88f, 1.0f));
	SetShopLayout(Canvas->AddChildToCanvas(RefreshCostText), FVector2D(310.0f, 454.0f), FVector2D(276.0f, 28.0f));
	RefreshButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShopRefreshButton"));
	RefreshButton->OnClicked.AddDynamic(this, &UImmortalShopWidget::HandleRefreshClicked);
	RefreshButton->SetStyle(MakeShopButtonStyle(FVector2D(170.0f, 42.0f), FLinearColor(0.2f, 0.38f, 0.5f, 1.0f)));
	SetShopLayout(Canvas->AddChildToCanvas(RefreshButton), FVector2D(363.0f, 484.0f), FVector2D(170.0f, 42.0f));
	AddShopButtonLabel(WidgetTree, RefreshButton, TEXT("手动刷新"), 15);

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopResult"));
	ResultText->SetAutoWrapText(true);
	ResultText->SetJustification(ETextJustify::Center);
	StyleShopText(ResultText, 14, FLinearColor(1.0f, 0.78f, 0.35f, 1.0f));
	SetShopLayout(Canvas->AddChildToCanvas(ResultText), FVector2D(306.0f, 536.0f), FVector2D(286.0f, 54.0f));

	UTextBlock* SaleTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopSaleTitle"));
	SaleTitle->SetText(FText::FromString(TEXT("出售背包物品")));
	StyleShopText(SaleTitle, 20, FLinearColor(0.55f, 1.0f, 0.72f, 1.0f));
	SetShopLayout(Canvas->AddChildToCanvas(SaleTitle), FVector2D(610.0f, 68.0f), FVector2D(270.0f, 30.0f));
	UScrollBox* SaleScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ShopSaleScroll"));
	SetShopLayout(Canvas->AddChildToCanvas(SaleScroll), FVector2D(606.0f, 102.0f), FVector2D(278.0f, 306.0f));
	SaleList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopSaleList"));
	SaleScroll->AddChild(SaleList);

	SaleNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopSaleName"));
	SaleNameText->SetAutoWrapText(true);
	StyleShopText(SaleNameText, 18, FLinearColor(0.65f, 1.0f, 0.78f, 1.0f));
	SetShopLayout(Canvas->AddChildToCanvas(SaleNameText), FVector2D(610.0f, 416.0f), FVector2D(270.0f, 38.0f));
	SaleDetailText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopSaleDetail"));
	SaleDetailText->SetAutoWrapText(true);
	StyleShopText(SaleDetailText, 14, FLinearColor(0.88f, 0.9f, 0.92f, 1.0f));
	SetShopLayout(Canvas->AddChildToCanvas(SaleDetailText), FVector2D(610.0f, 456.0f), FVector2D(270.0f, 70.0f));

	SellOneButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShopSellOneButton"));
	SellOneButton->OnClicked.AddDynamic(this, &UImmortalShopWidget::HandleSellOneClicked);
	SellOneButton->SetStyle(MakeShopButtonStyle(FVector2D(126.0f, 44.0f), FLinearColor(0.18f, 0.5f, 0.3f, 1.0f)));
	SetShopLayout(Canvas->AddChildToCanvas(SellOneButton), FVector2D(610.0f, 536.0f), FVector2D(126.0f, 44.0f));
	AddShopButtonLabel(WidgetTree, SellOneButton, TEXT("出售 1 个"), 14);
	SellAllButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShopSellAllButton"));
	SellAllButton->OnClicked.AddDynamic(this, &UImmortalShopWidget::HandleSellAllClicked);
	SellAllButton->SetStyle(MakeShopButtonStyle(FVector2D(134.0f, 44.0f), FLinearColor(0.15f, 0.42f, 0.27f, 1.0f)));
	SetShopLayout(Canvas->AddChildToCanvas(SellAllButton), FVector2D(746.0f, 536.0f), FVector2D(134.0f, 44.0f));
	AddShopButtonLabel(WidgetTree, SellAllButton, TEXT("材料全部出售"), 13);

	RefreshFromPlayer();
}

void UImmortalShopWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Player.IsValid())
	{
		return;
	}

	const bool bInventoryChanged = LastShopRevision != Player->GetShopRevision()
		|| LastEquipmentRevision != Player->GetEquipmentInventoryRevision()
		|| LastMaterialRevision != Player->GetMaterialInventoryRevision()
		|| LastSpiritStones != Player->GetGold();
	if (bInventoryChanged)
	{
		RefreshFromPlayer();
		return;
	}

	const int64 Seconds = Player->GetShopSecondsUntilRefresh();
	if (Seconds != LastRefreshSeconds)
	{
		LastRefreshSeconds = Seconds;
		RefreshHeader();
	}
}

void UImmortalShopWidget::RefreshFromPlayer()
{
	if (!Player.IsValid() || !OfferList || !SaleList)
	{
		return;
	}

	LastShopRevision = Player->GetShopRevision();
	LastEquipmentRevision = Player->GetEquipmentInventoryRevision();
	LastMaterialRevision = Player->GetMaterialInventoryRevision();
	LastSpiritStones = Player->GetGold();
	LastRefreshSeconds = Player->GetShopSecondsUntilRefresh();

	const FImmortalShopState State = Player->GetShopState();
	if (!State.Listings.ContainsByPredicate([this](const FImmortalShopListing& Listing)
		{ return Listing.ListingId == SelectedListingId; }))
	{
		SelectedListingId = State.Listings.IsEmpty() ? FGuid() : State.Listings[0].ListingId;
	}

	const TArray<FImmortalEquipmentItem> Equipment = Player->GetInventoryItems();
	const TArray<FImmortalMaterialStack> Materials = Player->GetMaterialInventory();
	const bool bEquipmentStillExists = Equipment.ContainsByPredicate([this](const FImmortalEquipmentItem& Item)
		{ return Item.ItemId == SelectedEquipmentId; });
	const bool bMaterialStillExists = Materials.ContainsByPredicate([this](const FImmortalMaterialStack& Stack)
		{ return Stack.MaterialId == SelectedMaterialId && Stack.Quantity > 0; });
	if (SaleSelection == ESaleSelection::Equipment && !bEquipmentStillExists)
	{
		SaleSelection = ESaleSelection::None;
		SelectedEquipmentId.Invalidate();
	}
	if (SaleSelection == ESaleSelection::Material && !bMaterialStillExists)
	{
		SaleSelection = ESaleSelection::None;
		SelectedMaterialId = NAME_None;
	}
	if (SaleSelection == ESaleSelection::None)
	{
		if (!Equipment.IsEmpty())
		{
			SaleSelection = ESaleSelection::Equipment;
			SelectedEquipmentId = Equipment[0].ItemId;
		}
		else if (!Materials.IsEmpty())
		{
			SaleSelection = ESaleSelection::Material;
			SelectedMaterialId = Materials[0].MaterialId;
		}
	}

	RefreshHeader();
	RebuildOfferEntries();
	RebuildSaleEntries();
	RefreshOfferDetails();
	RefreshSaleDetails();
}

void UImmortalShopWidget::RebuildOfferEntries()
{
	OfferList->ClearChildren();
	const FImmortalShopState State = Player->GetShopState();
	for (const FImmortalShopListing& Listing : State.Listings)
	{
		UImmortalShopEntryWidget* Entry = CreateWidget<UImmortalShopEntryWidget>(this, UImmortalShopEntryWidget::StaticClass());
		if (!Entry)
		{
			continue;
		}
		Entry->InitializeOfferEntry(this, Listing, Listing.ListingId == SelectedListingId);
		OfferList->AddChildToVerticalBox(Entry);
	}
	if (State.Listings.IsEmpty())
	{
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Empty->SetText(FText::FromString(TEXT("今日暂无商品")));
		Empty->SetJustification(ETextJustify::Center);
		StyleShopText(Empty, 15, FLinearColor(0.58f, 0.58f, 0.6f, 1.0f));
		OfferList->AddChildToVerticalBox(Empty);
	}
}

void UImmortalShopWidget::RebuildSaleEntries()
{
	SaleList->ClearChildren();
	const TArray<FImmortalEquipmentItem> EquipmentItems = Player->GetInventoryItems();
	const TArray<FImmortalMaterialStack> MaterialStacks = Player->GetMaterialInventory();
	if (!EquipmentItems.IsEmpty())
	{
		UTextBlock* EquipmentHeader = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		EquipmentHeader->SetText(FText::FromString(TEXT("— 背包装备 —")));
		EquipmentHeader->SetJustification(ETextJustify::Center);
		StyleShopText(EquipmentHeader, 13, FLinearColor(0.8f, 0.7f, 0.42f, 1.0f));
		SaleList->AddChildToVerticalBox(EquipmentHeader);
	}
	for (const FImmortalEquipmentItem& Item : EquipmentItems)
	{
		UImmortalShopEntryWidget* Entry = CreateWidget<UImmortalShopEntryWidget>(this, UImmortalShopEntryWidget::StaticClass());
		if (!Entry)
		{
			continue;
		}
		Entry->InitializeEquipmentSaleEntry(
			this,
			Item,
			Player->GetEquipmentShopSellPrice(Item.ItemId),
			SaleSelection == ESaleSelection::Equipment && Item.ItemId == SelectedEquipmentId);
		SaleList->AddChildToVerticalBox(Entry);
	}
	if (!MaterialStacks.IsEmpty())
	{
		UTextBlock* MaterialHeader = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		MaterialHeader->SetText(FText::FromString(TEXT("— 材料 —")));
		MaterialHeader->SetJustification(ETextJustify::Center);
		StyleShopText(MaterialHeader, 13, FLinearColor(0.48f, 0.9f, 0.68f, 1.0f));
		SaleList->AddChildToVerticalBox(MaterialHeader);
	}
	for (const FImmortalMaterialStack& Stack : MaterialStacks)
	{
		if (!Stack.IsValid())
		{
			continue;
		}
		UImmortalShopEntryWidget* Entry = CreateWidget<UImmortalShopEntryWidget>(this, UImmortalShopEntryWidget::StaticClass());
		if (!Entry)
		{
			continue;
		}
		Entry->InitializeMaterialSaleEntry(
			this,
			Stack,
			Player->GetMaterialShopSellPrice(Stack.MaterialId, 1),
			SaleSelection == ESaleSelection::Material && Stack.MaterialId == SelectedMaterialId);
		SaleList->AddChildToVerticalBox(Entry);
	}
	if (EquipmentItems.IsEmpty() && MaterialStacks.IsEmpty())
	{
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Empty->SetText(FText::FromString(TEXT("背包中没有可出售物品")));
		Empty->SetJustification(ETextJustify::Center);
		Empty->SetAutoWrapText(true);
		StyleShopText(Empty, 15, FLinearColor(0.58f, 0.6f, 0.62f, 1.0f));
		SaleList->AddChildToVerticalBox(Empty);
	}
}

void UImmortalShopWidget::RefreshHeader()
{
	if (!Player.IsValid() || !CurrencyText)
	{
		return;
	}
	CurrencyText->SetText(FText::FromString(FString::Printf(
		TEXT("灵石 %d  ·  每日刷新 %s"),
		Player->GetGold(),
		*FormatCountdown(Player->GetShopSecondsUntilRefresh()))));
}

void UImmortalShopWidget::RefreshOfferDetails()
{
	if (!Player.IsValid())
	{
		return;
	}

	const FImmortalShopState State = Player->GetShopState();
	const FImmortalShopListing* Listing = State.Listings.FindByPredicate([this](const FImmortalShopListing& Candidate)
		{ return Candidate.ListingId == SelectedListingId; });
	if (!Listing)
	{
		OfferNameText->SetText(FText::FromString(TEXT("请选择商品")));
		OfferMetaText->SetText(FText::GetEmpty());
		OfferDetailText->SetText(FText::GetEmpty());
		OfferPriceText->SetText(FText::GetEmpty());
		BuyButton->SetIsEnabled(false);
	}
	else
	{
		OfferNameText->SetText(UImmortalShopLibrary::GetListingDisplayName(*Listing));
		OfferNameText->SetColorAndOpacity(FSlateColor(UImmortalShopLibrary::GetListingColor(*Listing)));
		OfferMetaText->SetText(FText::FromString(FString::Printf(TEXT("%s  ·  数量 %d"),
			*UImmortalShopLibrary::GetProductTypeText(Listing->ProductType).ToString(),
			Listing->BundleQuantity)));
		OfferDetailText->SetText(UImmortalShopLibrary::GetListingDetailText(*Listing));
		const FString PriceLine = Listing->bSoldOut
			? FString(TEXT("已售罄"))
			: FString::Printf(TEXT("售价：%d 灵石"), Listing->BundlePrice);
		OfferPriceText->SetText(FText::FromString(PriceLine));
		// Keep actionable offers clickable so the transaction's precise insufficient-currency/capacity
		// message can be shown instead of silently disabling the button.
		BuyButton->SetIsEnabled(!Listing->bSoldOut);
	}

	const int32 RefreshCost = UImmortalShopLibrary::GetManualRefreshCost(State);
	RefreshCostText->SetText(FText::FromString(FString::Printf(
		TEXT("本日已刷新 %d 次 · 下次 %d 灵石"),
		State.ManualRefreshCount,
		RefreshCost)));
	RefreshButton->SetIsEnabled(true);
}

void UImmortalShopWidget::RefreshSaleDetails()
{
	if (!Player.IsValid())
	{
		return;
	}

	if (SaleSelection == ESaleSelection::Equipment)
	{
		const TArray<FImmortalEquipmentItem> Equipment = Player->GetInventoryItems();
		const FImmortalEquipmentItem* Item = Equipment.FindByPredicate([this](const FImmortalEquipmentItem& Candidate)
			{ return Candidate.ItemId == SelectedEquipmentId; });
		if (Item)
		{
			const FString Name = Item->DisplayName.IsNone()
				? UImmortalEquipmentLibrary::GetSlotText(Item->Slot).ToString()
				: Item->DisplayName.ToString();
			SaleNameText->SetText(FText::FromString(Name));
			SaleNameText->SetColorAndOpacity(FSlateColor(UImmortalEquipmentLibrary::GetQualityColor(Item->Quality)));
			SaleDetailText->SetText(FText::FromString(FString::Printf(
				TEXT("%s · %s · %d级 · 强化 +%d\n出售可得 %d 灵石"),
				*UImmortalEquipmentLibrary::GetQualityText(Item->Quality).ToString(),
				*UImmortalEquipmentLibrary::GetSlotText(Item->Slot).ToString(),
				Item->ItemLevel,
				Item->EnhancementLevel,
				Player->GetEquipmentShopSellPrice(Item->ItemId))));
			SellOneButton->SetIsEnabled(true);
			SellAllButton->SetIsEnabled(false);
			return;
		}
	}
	else if (SaleSelection == ESaleSelection::Material)
	{
		const TArray<FImmortalMaterialStack> Materials = Player->GetMaterialInventory();
		const FImmortalMaterialStack* Stack = Materials.FindByPredicate([this](const FImmortalMaterialStack& Candidate)
			{ return Candidate.MaterialId == SelectedMaterialId; });
		if (Stack && Stack->Quantity > 0)
		{
			FImmortalMaterialDefinition Definition;
			const bool bHasDefinition = UImmortalMaterialLibrary::GetMaterialDefinition(Stack->MaterialId, Definition);
			SaleNameText->SetText(bHasDefinition ? Definition.DisplayName : FText::FromName(Stack->MaterialId));
			SaleNameText->SetColorAndOpacity(FSlateColor(bHasDefinition
				? Definition.DisplayColor
				: FLinearColor::White));
			SaleDetailText->SetText(FText::FromString(FString::Printf(
				TEXT("持有 %d · 单价 %d 灵石\n全部出售可得 %d 灵石"),
				Stack->Quantity,
				Player->GetMaterialShopSellPrice(Stack->MaterialId, 1),
				Player->GetMaterialShopSellPrice(Stack->MaterialId, Stack->Quantity))));
			SellOneButton->SetIsEnabled(true);
			SellAllButton->SetIsEnabled(true);
			return;
		}
	}

	SaleNameText->SetText(FText::FromString(TEXT("请选择出售物品")));
	SaleNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.72f, 0.74f, 1.0f)));
	SaleDetailText->SetText(FText::FromString(TEXT("只会出售背包中的装备；已装备物品不会出现在列表中。")));
	SellOneButton->SetIsEnabled(false);
	SellAllButton->SetIsEnabled(false);
}

void UImmortalShopWidget::SelectOffer(const FGuid ListingId)
{
	SelectedListingId = ListingId;
	if (ResultText)
	{
		ResultText->SetText(FText::GetEmpty());
	}
	RebuildOfferEntries();
	RefreshOfferDetails();
}

void UImmortalShopWidget::SelectEquipmentForSale(const FGuid ItemId)
{
	SaleSelection = ESaleSelection::Equipment;
	SelectedEquipmentId = ItemId;
	SelectedMaterialId = NAME_None;
	if (ResultText)
	{
		ResultText->SetText(FText::GetEmpty());
	}
	RebuildSaleEntries();
	RefreshSaleDetails();
}

void UImmortalShopWidget::SelectMaterialForSale(const FName MaterialId)
{
	SaleSelection = ESaleSelection::Material;
	SelectedMaterialId = MaterialId;
	SelectedEquipmentId.Invalidate();
	if (ResultText)
	{
		ResultText->SetText(FText::GetEmpty());
	}
	RebuildSaleEntries();
	RefreshSaleDetails();
}

void UImmortalShopWidget::HandleBuyClicked()
{
	if (!Player.IsValid() || !SelectedListingId.IsValid())
	{
		return;
	}
	const FImmortalShopTransactionResult Result = Player->BuyShopListing(SelectedListingId);
	SetResultMessage(Result.Message, Result.bSucceeded);
	RefreshFromPlayer();
}

void UImmortalShopWidget::HandleRefreshClicked()
{
	if (!Player.IsValid())
	{
		return;
	}
	const FImmortalShopTransactionResult Result = Player->RefreshShopInventory();
	if (Result.bSucceeded)
	{
		SelectedListingId.Invalidate();
	}
	SetResultMessage(Result.Message, Result.bSucceeded);
	RefreshFromPlayer();
}

void UImmortalShopWidget::HandleSellOneClicked()
{
	if (!Player.IsValid())
	{
		return;
	}
	FImmortalShopTransactionResult Result;
	if (SaleSelection == ESaleSelection::Equipment)
	{
		Result = Player->SellEquipmentToShop(SelectedEquipmentId);
	}
	else if (SaleSelection == ESaleSelection::Material)
	{
		Result = Player->SellMaterialToShop(SelectedMaterialId, 1);
	}
	else
	{
		return;
	}
	SetResultMessage(Result.Message, Result.bSucceeded);
	RefreshFromPlayer();
}

void UImmortalShopWidget::HandleSellAllClicked()
{
	if (!Player.IsValid() || SaleSelection != ESaleSelection::Material)
	{
		return;
	}
	const int32 Amount = Player->GetMaterialQuantity(SelectedMaterialId);
	const FImmortalShopTransactionResult Result = Player->SellMaterialToShop(SelectedMaterialId, Amount);
	SetResultMessage(Result.Message, Result.bSucceeded);
	RefreshFromPlayer();
}

void UImmortalShopWidget::SetResultMessage(const FText& Message, const bool bSucceeded)
{
	if (!ResultText)
	{
		return;
	}
	ResultText->SetText(Message);
	ResultText->SetColorAndOpacity(FSlateColor(bSucceeded
		? FLinearColor(0.45f, 1.0f, 0.6f, 1.0f)
		: FLinearColor(1.0f, 0.38f, 0.3f, 1.0f)));
}

void UImmortalShopWidget::HandleCloseClicked()
{
	if (Player.IsValid())
	{
		Player->ToggleShop();
	}
}
