// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalInventoryWidget.h"

#include "ImmortalInventorySlotWidget.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Styling/SlateTypes.h"

namespace
{
	FSlateBrush MakePanelBrush(const TCHAR* AssetPath, const FVector2D Size, const FLinearColor Tint = FLinearColor::White)
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

	void SetCanvasLayout(UCanvasPanelSlot* Slot, const FVector2D Position, const FVector2D Size)
	{
		if (Slot)
		{
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
			Slot->SetAutoSize(false);
		}
	}

	void SetTextAppearance(UTextBlock* Text, const int32 Size, const FLinearColor Color)
	{
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor::Black);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}

	const FImmortalEquipmentItem* FindItemById(
		const TArray<FImmortalEquipmentItem>& Equipped,
		const TArray<FImmortalEquipmentItem>& Inventory,
		const FGuid& ItemId)
	{
		if (!ItemId.IsValid()) return nullptr;
		if (const FImmortalEquipmentItem* Found = Equipped.FindByPredicate([&ItemId](const FImmortalEquipmentItem& Item) { return Item.ItemId == ItemId; }))
		{
			return Found;
		}
		return Inventory.FindByPredicate([&ItemId](const FImmortalEquipmentItem& Item) { return Item.ItemId == ItemId; });
	}
}

void UImmortalInventoryWidget::InitializeForPlayer(AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("InventoryPanelSize"));
	RootBox->SetWidthOverride(900.0f);
	RootBox->SetHeightOverride(600.0f);
	WidgetTree->RootWidget = RootBox;

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InventoryCanvas"));
	RootBox->AddChild(Canvas);

	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("InventoryBackground"));
	Background->SetBrush(MakePanelBrush(TEXT("/Game/GAME/Asset/ui/inventory/panel_background.panel_background"), FVector2D(900.0f, 600.0f), FLinearColor(1.0f, 1.0f, 1.0f, 0.97f)));
	SetCanvasLayout(Canvas->AddChildToCanvas(Background), FVector2D::ZeroVector, FVector2D(900.0f, 600.0f));

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventoryTitle"));
	Title->SetText(FText::FromString(TEXT("装备与背包")));
	SetTextAppearance(Title, 28, FLinearColor(0.98f, 0.82f, 0.42f, 1.0f));
	SetCanvasLayout(Canvas->AddChildToCanvas(Title), FVector2D(38.0f, 22.0f), FVector2D(240.0f, 42.0f));

	CombatPowerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CombatPowerText"));
	SetTextAppearance(CombatPowerText, 20, FLinearColor(1.0f, 0.72f, 0.22f, 1.0f));
	SetCanvasLayout(Canvas->AddChildToCanvas(CombatPowerText), FVector2D(575.0f, 29.0f), FVector2D(230.0f, 32.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("InventoryCloseButton"));
	CloseButton->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleCloseClicked);
	const FSlateBrush CloseBrush = MakePanelBrush(TEXT("/Game/GAME/Asset/ui/inventory/close_button.close_button"), FVector2D(64.0f));
	FButtonStyle CloseStyle;
	CloseStyle.SetNormal(CloseBrush);
	CloseStyle.SetHovered(CloseBrush);
	CloseStyle.SetPressed(CloseBrush);
	CloseStyle.SetDisabled(CloseBrush);
	CloseButton->SetStyle(CloseStyle);
	SetCanvasLayout(Canvas->AddChildToCanvas(CloseButton), FVector2D(816.0f, 12.0f), FVector2D(64.0f));

	UTextBlock* EquipmentTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EquipmentTitle"));
	EquipmentTitle->SetText(FText::FromString(TEXT("当前装备")));
	SetTextAppearance(EquipmentTitle, 20, FLinearColor::White);
	SetCanvasLayout(Canvas->AddChildToCanvas(EquipmentTitle), FVector2D(42.0f, 83.0f), FVector2D(200.0f, 32.0f));

	EquipmentGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("EquipmentGrid"));
	EquipmentGrid->SetSlotPadding(FMargin(7.0f));
	SetCanvasLayout(Canvas->AddChildToCanvas(EquipmentGrid), FVector2D(34.0f, 116.0f), FVector2D(318.0f, 318.0f));

	UTextBlock* BackpackTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BackpackTitle"));
	BackpackTitle->SetText(FText::FromString(TEXT("背包")));
	SetTextAppearance(BackpackTitle, 20, FLinearColor::White);
	SetCanvasLayout(Canvas->AddChildToCanvas(BackpackTitle), FVector2D(379.0f, 83.0f), FVector2D(100.0f, 32.0f));

	BackpackCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BackpackCount"));
	SetTextAppearance(BackpackCountText, 16, FLinearColor(0.75f, 0.78f, 0.82f, 1.0f));
	SetCanvasLayout(Canvas->AddChildToCanvas(BackpackCountText), FVector2D(730.0f, 87.0f), FVector2D(110.0f, 28.0f));

	UScrollBox* BackpackScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BackpackScroll"));
	BackpackScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	SetCanvasLayout(Canvas->AddChildToCanvas(BackpackScroll), FVector2D(372.0f, 116.0f), FVector2D(482.0f, 456.0f));

	BackpackGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("BackpackGrid"));
	BackpackGrid->SetSlotPadding(FMargin(2.0f));
	BackpackScroll->AddChild(BackpackGrid);

	ItemNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SelectedItemName"));
	SetTextAppearance(ItemNameText, 19, FLinearColor::White);
	SetCanvasLayout(Canvas->AddChildToCanvas(ItemNameText), FVector2D(40.0f, 435.0f), FVector2D(310.0f, 30.0f));

	ItemDetailsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SelectedItemDetails"));
	ItemDetailsText->SetAutoWrapText(true);
	SetTextAppearance(ItemDetailsText, 15, FLinearColor(0.88f, 0.88f, 0.9f, 1.0f));
	SetCanvasLayout(Canvas->AddChildToCanvas(ItemDetailsText), FVector2D(40.0f, 467.0f), FVector2D(310.0f, 90.0f));

	ComparisonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SelectedItemComparison"));
	SetTextAppearance(ComparisonText, 15, FLinearColor(0.45f, 1.0f, 0.45f, 1.0f));
	SetCanvasLayout(Canvas->AddChildToCanvas(ComparisonText), FVector2D(40.0f, 557.0f), FVector2D(310.0f, 28.0f));

	RefreshFromPlayer();
}

void UImmortalInventoryWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Player.IsValid()
		&& (LastEquipmentDropCount != Player->GetEquipmentDropCount()
			|| LastInventoryCount != Player->GetInventoryItems().Num()
			|| !FMath::IsNearlyEqual(LastCombatPower, Player->GetCombatPower())))
	{
		RefreshFromPlayer();
	}
}

void UImmortalInventoryWidget::RefreshFromPlayer()
{
	if (!Player.IsValid() || !EquipmentGrid || !BackpackGrid)
	{
		return;
	}

	LastEquipmentDropCount = Player->GetEquipmentDropCount();
	LastInventoryCount = Player->GetInventoryItems().Num();
	LastCombatPower = Player->GetCombatPower();
	CombatPowerText->SetText(FText::FromString(FString::Printf(TEXT("战斗力  %.1f"), LastCombatPower)));
	BackpackCountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), LastInventoryCount, Player->GetInventoryCapacity())));

	const TArray<FImmortalEquipmentItem> Equipped = Player->GetEquippedItems();
	const TArray<FImmortalEquipmentItem> Inventory = Player->GetInventoryItems();
	if (SelectedItemId.IsValid() && !FindItemById(Equipped, Inventory, SelectedItemId))
	{
		SelectedItemId.Invalidate();
	}
	if (!SelectedItemId.IsValid())
	{
		if (!Inventory.IsEmpty()) SelectedItemId = Inventory[0].ItemId;
		else if (!Equipped.IsEmpty()) SelectedItemId = Equipped[0].ItemId;
	}

	RebuildEquipmentSlots();
	RebuildBackpackSlots();
	RefreshDetails();
}

void UImmortalInventoryWidget::RebuildEquipmentSlots()
{
	EquipmentGrid->ClearChildren();
	AddEquipmentSlot(EImmortalEquipmentSlot::Weapon, 0, 0);
	AddEquipmentSlot(EImmortalEquipmentSlot::Head, 1, 0);
	AddEquipmentSlot(EImmortalEquipmentSlot::Chest, 0, 1);
	AddEquipmentSlot(EImmortalEquipmentSlot::Boots, 1, 1);
	AddEquipmentSlot(EImmortalEquipmentSlot::Accessory, 0, 2);
}

void UImmortalInventoryWidget::AddEquipmentSlot(const EImmortalEquipmentSlot EquipmentSlot, const int32 Column, const int32 Row)
{
	FImmortalEquipmentItem Item;
	const bool bHasItem = Player->GetEquippedItemForSlot(EquipmentSlot, Item);
	UImmortalInventorySlotWidget* SlotWidget = CreateWidget<UImmortalInventorySlotWidget>(GetOwningPlayer(), UImmortalInventorySlotWidget::StaticClass());
	SlotWidget->InitializeSlot(this, Item, bHasItem, bHasItem, bHasItem && SelectedItemId == Item.ItemId, EquipmentSlot);
	if (UUniformGridSlot* GridSlot = EquipmentGrid->AddChildToUniformGrid(SlotWidget, Row, Column))
	{
		GridSlot->SetHorizontalAlignment(HAlign_Center);
		GridSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UImmortalInventoryWidget::RebuildBackpackSlots()
{
	BackpackGrid->ClearChildren();
	const TArray<FImmortalEquipmentItem> Inventory = Player->GetInventoryItems();
	const int32 Capacity = Player->GetInventoryCapacity();
	for (int32 Index = 0; Index < Capacity; ++Index)
	{
		const bool bHasItem = Inventory.IsValidIndex(Index);
		const FImmortalEquipmentItem Item = bHasItem ? Inventory[Index] : FImmortalEquipmentItem();
		UImmortalInventorySlotWidget* SlotWidget = CreateWidget<UImmortalInventorySlotWidget>(GetOwningPlayer(), UImmortalInventorySlotWidget::StaticClass());
		SlotWidget->InitializeSlot(this, Item, bHasItem, false, bHasItem && SelectedItemId == Item.ItemId);
		BackpackGrid->AddChildToUniformGrid(SlotWidget, Index / 5, Index % 5);
	}
}

void UImmortalInventoryWidget::HandleSlotSelected(const FGuid& ItemId)
{
	SelectedItemId = ItemId;
	RefreshFromPlayer();
}

void UImmortalInventoryWidget::RefreshDetails()
{
	const TArray<FImmortalEquipmentItem> Equipped = Player->GetEquippedItems();
	const TArray<FImmortalEquipmentItem> Inventory = Player->GetInventoryItems();
	const FImmortalEquipmentItem* Item = FindItemById(Equipped, Inventory, SelectedItemId);
	if (!Item)
	{
		ItemNameText->SetText(FText::FromString(TEXT("请选择装备")));
		ItemNameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		ItemDetailsText->SetText(FText::FromString(TEXT("装备掉落后会自动比较，并将更强的装备穿戴到对应部位。")));
		ComparisonText->SetText(FText::GetEmpty());
		return;
	}

	ItemNameText->SetText(FText::FromName(Item->DisplayName));
	ItemNameText->SetColorAndOpacity(FSlateColor(UImmortalEquipmentLibrary::GetQualityColor(Item->Quality)));
	const float ItemPower = UImmortalEquipmentLibrary::CalculateEquipmentPower(*Item);
	ItemDetailsText->SetText(FText::FromString(FString::Printf(
		TEXT("%s · 等级 %d\n攻击 +%.1f   防御 +%.1f   生命 +%.1f\n攻速 +%.1f%%   暴击 +%.1f%%   装备战力 %.1f"),
		*UImmortalEquipmentLibrary::GetSlotText(Item->Slot).ToString(), Item->ItemLevel,
		Item->AttackBonus, Item->DefenseBonus, Item->HealthBonus,
		Item->AttackSpeedBonus * 100.0f, Item->CriticalChanceBonus * 100.0f, ItemPower)));

	FImmortalEquipmentItem CurrentItem;
	const bool bHasCurrent = Player->GetEquippedItemForSlot(Item->Slot, CurrentItem);
	const bool bIsCurrent = bHasCurrent && CurrentItem.ItemId == Item->ItemId;
	const float CurrentPower = bHasCurrent ? UImmortalEquipmentLibrary::CalculateEquipmentPower(CurrentItem) : 0.0f;
	const float Difference = ItemPower - CurrentPower;
	if (bIsCurrent)
	{
		ComparisonText->SetText(FText::FromString(TEXT("已装备")));
		ComparisonText->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.85f, 1.0f, 1.0f)));
	}
	else
	{
		ComparisonText->SetText(FText::FromString(FString::Printf(TEXT("相对当前装备  %+.1f 战力"), Difference)));
		ComparisonText->SetColorAndOpacity(FSlateColor(Difference >= 0.0f
			? FLinearColor(0.4f, 1.0f, 0.4f, 1.0f)
			: FLinearColor(1.0f, 0.4f, 0.35f, 1.0f)));
	}
}

void UImmortalInventoryWidget::HandleCloseClicked()
{
	if (Player.IsValid())
	{
		Player->ToggleInventory();
	}
}
