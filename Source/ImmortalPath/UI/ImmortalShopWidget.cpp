// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalShopWidget.h"
#include "ImmortalFeaturePageLayout.h"
#include "ImmortalCraftingArt.h"
#include "ImmortalUITheme.h"
#include "ImmortalIconWidget.h"

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

	UTextBlock* AddShopButtonLabel(UWidgetTree* Tree, UButton* Button, const TCHAR* Label, const int32 FontSize = 15)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		Text->SetJustification(ETextJustify::Center);
		StyleShopText(Text, FontSize, FLinearColor::White);
		Button->AddChild(Text);
		ImmortalFeaturePageLayout::StabilizeButtonLabel(Button, Text);
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

FSlateBrush UImmortalShopWidget::GetEquipmentArt(EImmortalEquipmentSlot EquipmentSlot)
{
	return ImmortalCraftingArt::EquipmentBrush(EquipmentAtlas.LoadSynchronous(), EquipmentSlot);
}

FSlateBrush UImmortalShopWidget::GetMaterialArt(FName Id)
{
	return ImmortalCraftingArt::MaterialBrush(ForgeAtlas.LoadSynchronous(), MaterialAtlas.LoadSynchronous(), Id);
}

FSlateBrush UImmortalShopWidget::GetOfferArt(const FImmortalShopListing& Listing)
{
	switch (Listing.ProductType)
	{
	case EImmortalShopProductType::Equipment: return GetEquipmentArt(Listing.EquipmentItem.Slot);
	case EImmortalShopProductType::Material: return GetMaterialArt(Listing.ProductId);
	case EImmortalShopProductType::Pill: return ImmortalAlchemyArt::Brush(AlchemyAtlas.LoadSynchronous(), Listing.ProductId);
	default:
		// Artifact-specific illustrations belong to the artifact art pass. Keep the existing
		// generic sword symbol visible rather than misrepresenting a material as an artifact.
		FSlateBrush Empty;
		Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
		return Empty;
	}
}

void UImmortalShopWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShopPanelSize"));
	Root->SetWidthOverride(1600);
	Root->SetHeightOverride(600);
	WidgetTree->RootWidget = Root;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShopCanvas"));
	Root->AddChild(Canvas);
	ImmortalFeaturePageLayout::AddReadabilityBackground(WidgetTree, Canvas);

	const auto Card = [&](const TCHAR* Name, float X, float Width)
	{
		UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		Background->SetBrush(ImmortalUITheme::PanelBrush(FLinearColor(0.055f, 0.095f, 0.09f, 0.98f)));
		Background->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetShopLayout(Canvas->AddChildToCanvas(Background), FVector2D(X, 52), FVector2D(Width, 484));
	};
	Card(TEXT("ShopCatalogCard"), 8, 332);
	Card(TEXT("ShopPurchaseCard"), 348, 440);
	Card(TEXT("ShopInventoryCard"), 796, 332);
	Card(TEXT("ShopSaleCard"), 1136, 456);
	const auto Text = [&](const TCHAR* Name, const TCHAR* Label, float X, float Y, float W, float H, int32 Font)
	{
		UTextBlock* Result = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Result->SetText(FText::FromString(Label));
		StyleShopText(Result, Font, FLinearColor(0.9f, 0.94f, 0.91f));
		SetShopLayout(Canvas->AddChildToCanvas(Result), FVector2D(X, Y), FVector2D(W, H));
		return Result;
	};
	const auto Button = [&](const TCHAR* Name, const TCHAR* Label, float X, float Y, float W, float H)
	{
		UButton* Result = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Result->SetStyle(ImmortalUITheme::ButtonStyle());
		SetShopLayout(Canvas->AddChildToCanvas(Result), FVector2D(X, Y), FVector2D(W, H));
		AddShopButtonLabel(WidgetTree, Result, Label, 18);
		return Result;
	};
	const auto List = [&](const TCHAR* ScrollName, const TCHAR* ListName, float X)
	{
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), ScrollName);
		SetShopLayout(Canvas->AddChildToCanvas(Scroll), FVector2D(X, 106), FVector2D(312, 416));
		ImmortalFeaturePageLayout::StyleScrollBox(Scroll);
		UVerticalBox* Items = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), ListName);
		Scroll->AddChild(Items);
		return Items;
	};
	const auto Art = [&](const TCHAR* Name, float X, float Y, float Size)
	{
		UImage* Result = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		Result->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetShopLayout(Canvas->AddChildToCanvas(Result), FVector2D(X, Y), FVector2D(Size));
		return Result;
	};

	Text(TEXT("ShopTitle"), TEXT("百宝阁 · 每日珍品"), 18, 4, 460, 38, 28);
	CurrencyText = Text(TEXT("ShopCurrency"), TEXT(""), 700, 9, 810, 30, 19);
	CurrencyText->SetJustification(ETextJustify::Right);
	UButton* Close = Button(TEXT("ShopClose"), TEXT("×"), 1540, 3, 44, 36);
	Close->OnClicked.AddDynamic(this, &UImmortalShopWidget::HandleCloseClicked);
	Text(TEXT("ShopOfferTitle"), TEXT("今日商品"), 22, 64, 300, 32, 22);
	Text(TEXT("ShopSaleTitle"), TEXT("背包 · 可售物品"), 810, 64, 300, 32, 22);
	OfferList = List(TEXT("ShopOfferScroll"), TEXT("ShopOfferList"), 18);
	SaleList = List(TEXT("ShopSaleScroll"), TEXT("ShopSaleList"), 806);

	OfferIcon = Art(TEXT("ShopOfferIcon"), 626, 68, 144);
	OfferFallback = CreateWidget<UImmortalIconWidget>(this);
	OfferFallback->SetIcon(4);
	SetShopLayout(Canvas->AddChildToCanvas(OfferFallback), FVector2D(650, 92), FVector2D(96));
	OfferFallback->SetVisibility(ESlateVisibility::Hidden);
	OfferNameText = Text(TEXT("ShopOfferName"), TEXT(""), 364, 68, 250, 92, 24);
	OfferMetaText = Text(TEXT("ShopOfferMeta"), TEXT(""), 364, 172, 250, 32, 18);
	OfferDetailText = Text(TEXT("ShopOfferDetail"), TEXT(""), 364, 218, 408, 134, 19);
	Art(TEXT("ShopPriceIcon"), 370, 358, 44)->SetBrush(GetMaterialArt(TEXT("SpiritStones")));
	OfferPriceText = Text(TEXT("ShopOfferPrice"), TEXT(""), 426, 365, 342, 32, 21);
	BuyButton = Button(TEXT("ShopBuyButton"), TEXT("购买整份"), 364, 408, 408, 48);
	BuyButton->OnClicked.AddDynamic(this, &UImmortalShopWidget::HandleBuyClicked);
	RefreshCostText = Text(TEXT("ShopRefreshCost"), TEXT(""), 364, 472, 244, 52, 16);
	RefreshCostText->SetAutoWrapText(true);
	RefreshButton = Button(TEXT("ShopRefreshButton"), TEXT("刷新商品"), 622, 474, 150, 48);
	RefreshButton->OnClicked.AddDynamic(this, &UImmortalShopWidget::HandleRefreshClicked);

	SaleIcon = Art(TEXT("ShopSaleIcon"), 1148, 70, 136);
	SaleNameText = Text(TEXT("ShopSaleName"), TEXT(""), 1296, 78, 278, 124, 23);
	SaleDetailText = Text(TEXT("ShopSaleDetail"), TEXT(""), 1154, 224, 420, 152, 20);
	Text(TEXT("ShopSaleHint"), TEXT("锁定装备不能出售\n已穿戴的装备不会列入此处"), 1154, 390, 420, 62, 17);
	SellOneButton = Button(TEXT("ShopSellOneButton"), TEXT("出售 1 个"), 1154, 474, 190, 48);
	SellOneButton->OnClicked.AddDynamic(this, &UImmortalShopWidget::HandleSellOneClicked);
	SellAllButton = Button(TEXT("ShopSellAllButton"), TEXT("出售此种全部"), 1356, 474, 218, 48);
	SellAllButton->OnClicked.AddDynamic(this, &UImmortalShopWidget::HandleSellAllClicked);
	SellAllButton->SetToolTipText(FText::FromString(TEXT("只出售当前选中的这一种材料，不会出售其他材料。")));
	ResultText = Text(TEXT("ShopResult"), TEXT("选择商品查看详情；购买和刷新不会暂停自动历练。"), 18, 546, 1560, 44, 19);
	for (UTextBlock* Detail : {OfferNameText.Get(), OfferDetailText.Get(), SaleNameText.Get(), SaleDetailText.Get(), ResultText.Get()})
		ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, Detail);
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
		|| LastMaterialRevision != Player->GetMaterialInventoryRevision();
	if (bInventoryChanged)
	{
		RefreshFromPlayer();
		return;
	}

	const int32 SpiritStones = Player->GetGold();
	const int64 Seconds = Player->GetShopSecondsUntilRefresh();
	if (SpiritStones != LastSpiritStones || Seconds != LastRefreshSeconds)
	{
		LastSpiritStones = SpiritStones;
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

	const bool bOffersChanged = LastShopRevision != Player->GetShopRevision() || OfferList->GetChildrenCount() == 0;
	const bool bSalesChanged = LastEquipmentRevision != Player->GetEquipmentInventoryRevision()
		|| LastMaterialRevision != Player->GetMaterialInventoryRevision() || SaleList->GetChildrenCount() == 0;
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
		if (const FImmortalEquipmentItem* FirstSellable = Equipment.FindByPredicate([](const FImmortalEquipmentItem& Item)
		{
			return !Item.bLocked;
		}))
		{
			SaleSelection = ESaleSelection::Equipment;
			SelectedEquipmentId = FirstSellable->ItemId;
		}
		else if (!Materials.IsEmpty())
		{
			SaleSelection = ESaleSelection::Material;
			SelectedMaterialId = Materials[0].MaterialId;
		}
	}

	RefreshHeader();
	if (bOffersChanged) RebuildOfferEntries();
	if (bSalesChanged) RebuildSaleEntries();
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
	OfferList->ForceLayoutPrepass();
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
		StyleShopText(EquipmentHeader, 18, FLinearColor(0.8f, 0.7f, 0.42f, 1.0f));
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
		StyleShopText(MaterialHeader, 18, FLinearColor(0.48f, 0.9f, 0.68f, 1.0f));
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
	SaleList->ForceLayoutPrepass();
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
		OfferIcon->SetVisibility(ESlateVisibility::Hidden);
		OfferFallback->SetVisibility(ESlateVisibility::Hidden);
		BuyButton->SetIsEnabled(false);
	}
	else
	{
		OfferIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		const FSlateBrush Brush = GetOfferArt(*Listing);
		OfferIcon->SetBrush(Brush);
		OfferFallback->SetVisibility(Brush.DrawAs == ESlateBrushDrawType::NoDrawType ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
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
			SaleIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
			SaleIcon->SetBrush(GetEquipmentArt(Item->Slot));
			SaleNameText->SetText(FText::FromString(Name));
			SaleNameText->SetColorAndOpacity(FSlateColor(UImmortalEquipmentLibrary::GetQualityColor(Item->Quality)));
			SaleDetailText->SetText(FText::FromString(FString::Printf(
				TEXT("%s · %s · %d级 · 强化 +%d%s\n%s"),
				*UImmortalEquipmentLibrary::GetQualityText(Item->Quality).ToString(),
				*UImmortalEquipmentLibrary::GetSlotText(Item->Slot).ToString(),
				Item->ItemLevel,
				Item->EnhancementLevel,
				Item->bLocked ? TEXT(" · 已锁定") : TEXT(""),
				Item->bLocked
					? TEXT("请先在储物戒中解锁")
					: *FString::Printf(TEXT("出售可得 %d 灵石"), Player->GetEquipmentShopSellPrice(Item->ItemId)))));
			SellOneButton->SetIsEnabled(!Item->bLocked);
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
			SaleIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
			SaleIcon->SetBrush(GetMaterialArt(Stack->MaterialId));
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

	SaleIcon->SetVisibility(ESlateVisibility::Hidden);
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
		// The next refresh selects the first new offer; keep that selection visible.
		if (UScrollBox* Catalog = Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("ShopOfferScroll"))))
			Catalog->ScrollToStart();
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
