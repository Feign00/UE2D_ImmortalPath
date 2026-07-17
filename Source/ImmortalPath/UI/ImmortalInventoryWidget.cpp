// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalInventoryWidget.h"

#include "ImmortalInventorySlotWidget.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
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

	EquipmentTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EquipmentTitle"));
	EquipmentTitleText->SetText(FText::FromString(TEXT("当前装备")));
	SetTextAppearance(EquipmentTitleText, 20, FLinearColor::White);
	SetCanvasLayout(Canvas->AddChildToCanvas(EquipmentTitleText), FVector2D(42.0f, 83.0f), FVector2D(200.0f, 32.0f));

	EquipmentGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("EquipmentGrid"));
	EquipmentGrid->SetSlotPadding(FMargin(7.0f));
	SetCanvasLayout(Canvas->AddChildToCanvas(EquipmentGrid), FVector2D(34.0f, 116.0f), FVector2D(318.0f, 318.0f));

	MaterialOverviewText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MaterialOverview"));
	MaterialOverviewText->SetAutoWrapText(true);
	SetTextAppearance(MaterialOverviewText, 16, FLinearColor(0.82f, 0.86f, 0.9f, 1.0f));
	SetCanvasLayout(Canvas->AddChildToCanvas(MaterialOverviewText), FVector2D(40.0f, 120.0f), FVector2D(305.0f, 290.0f));
	MaterialOverviewText->SetVisibility(ESlateVisibility::Collapsed);

	UButton* EquipmentTabButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EquipmentTabButton"));
	EquipmentTabButton->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleEquipmentTabClicked);
	SetCanvasLayout(Canvas->AddChildToCanvas(EquipmentTabButton), FVector2D(372.0f, 76.0f), FVector2D(112.0f, 38.0f));
	EquipmentTabText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EquipmentTabText"));
	EquipmentTabText->SetText(FText::FromString(TEXT("装备")));
	EquipmentTabText->SetJustification(ETextJustify::Center);
	SetTextAppearance(EquipmentTabText, 18, FLinearColor::White);
	EquipmentTabButton->AddChild(EquipmentTabText);

	UButton* MaterialTabButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MaterialTabButton"));
	MaterialTabButton->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleMaterialTabClicked);
	SetCanvasLayout(Canvas->AddChildToCanvas(MaterialTabButton), FVector2D(490.0f, 76.0f), FVector2D(112.0f, 38.0f));
	MaterialTabText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MaterialTabText"));
	MaterialTabText->SetText(FText::FromString(TEXT("材料")));
	MaterialTabText->SetJustification(ETextJustify::Center);
	SetTextAppearance(MaterialTabText, 18, FLinearColor::White);
	MaterialTabButton->AddChild(MaterialTabText);

	UButton* PillTabButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PillTabButton"));
	PillTabButton->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandlePillTabClicked);
	SetCanvasLayout(Canvas->AddChildToCanvas(PillTabButton), FVector2D(608.0f, 76.0f), FVector2D(112.0f, 38.0f));
	PillTabText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PillTabText"));
	PillTabText->SetText(FText::FromString(TEXT("丹药")));
	PillTabText->SetJustification(ETextJustify::Center);
	SetTextAppearance(PillTabText, 18, FLinearColor::White);
	PillTabButton->AddChild(PillTabText);

	BackpackCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BackpackCount"));
	SetTextAppearance(BackpackCountText, 16, FLinearColor(0.75f, 0.78f, 0.82f, 1.0f));
	SetCanvasLayout(Canvas->AddChildToCanvas(BackpackCountText), FVector2D(730.0f, 87.0f), FVector2D(110.0f, 28.0f));

	UScrollBox* BackpackScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BackpackScroll"));
	BackpackScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	SetCanvasLayout(Canvas->AddChildToCanvas(BackpackScroll), FVector2D(372.0f, 120.0f), FVector2D(482.0f, 452.0f));

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
			|| LastMaterialRevision != Player->GetMaterialInventoryRevision()
			|| LastPillRevision != Player->GetPillInventoryRevision()
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
	LastMaterialRevision = Player->GetMaterialInventoryRevision();
	LastPillRevision = Player->GetPillInventoryRevision();
	LastCombatPower = Player->GetCombatPower();
	CombatPowerText->SetText(FText::FromString(FString::Printf(TEXT("战斗力  %.1f"), LastCombatPower)));
	if (bShowingPills)
	{
		const TArray<FImmortalPillStack>& Pills = Player->GetPillInventory();
		int32 TotalQuantity = 0;
		for (const FImmortalPillStack& Stack : Pills)
		{
			TotalQuantity = FMath::Min(TotalQuantity + FMath::Max(Stack.Quantity, 0), MAX_int32);
		}
		BackpackCountText->SetText(FText::FromString(FString::Printf(TEXT("%d 种 / %d 枚"), Pills.Num(), TotalQuantity)));
	}
	else if (bShowingMaterials)
	{
		const TArray<FImmortalMaterialStack> Materials = Player->GetMaterialInventory();
		int64 TotalQuantity = 0;
		for (const FImmortalMaterialStack& Stack : Materials)
		{
			TotalQuantity = FMath::Min<int64>(TotalQuantity + FMath::Max(Stack.Quantity, 0), MAX_int32);
		}
		BackpackCountText->SetText(FText::FromString(FString::Printf(
			TEXT("%d 种 / %lld 份"), Materials.Num(), TotalQuantity)));
	}
	else
	{
		BackpackCountText->SetText(FText::FromString(FString::Printf(
			TEXT("%d / %d"), LastInventoryCount, Player->GetInventoryCapacity())));
	}

	const TArray<FImmortalEquipmentItem> Equipped = Player->GetEquippedItems();
	const TArray<FImmortalEquipmentItem> Inventory = Player->GetInventoryItems();
	if (!bShowingMaterials && !bShowingPills && SelectedItemId.IsValid() && !FindItemById(Equipped, Inventory, SelectedItemId))
	{
		SelectedItemId.Invalidate();
	}
	if (!bShowingMaterials && !bShowingPills && !SelectedItemId.IsValid())
	{
		if (!Inventory.IsEmpty()) SelectedItemId = Inventory[0].ItemId;
		else if (!Equipped.IsEmpty()) SelectedItemId = Equipped[0].ItemId;
	}
	if (bShowingMaterials)
	{
		const TArray<FImmortalMaterialStack> Materials = Player->GetMaterialInventory();
		if (!SelectedMaterialId.IsNone()
			&& UImmortalMaterialLibrary::GetMaterialQuantity(Materials, SelectedMaterialId) <= 0)
		{
			SelectedMaterialId = NAME_None;
		}
		if (SelectedMaterialId.IsNone() && !Materials.IsEmpty())
		{
			SelectedMaterialId = Materials[0].MaterialId;
		}
	}
	if (bShowingPills)
	{
		const TArray<FImmortalPillStack>& Pills = Player->GetPillInventory();
		const bool bSelectionExists = Pills.ContainsByPredicate([this](const FImmortalPillStack& Stack)
		{
			return Stack.IsValid() && Stack.PillId == SelectedPillId && Stack.Quality == SelectedPillQuality;
		});
		if (!bSelectionExists)
		{
			SelectedPillId = Pills.IsEmpty() ? NAME_None : Pills[0].PillId;
			SelectedPillQuality = Pills.IsEmpty() ? EImmortalPillQuality::Ordinary : Pills[0].Quality;
		}
	}

	RefreshTabAppearance();
	RebuildEquipmentSlots();
	RebuildBackpackSlots();
	RefreshMaterialOverview();
	RefreshDetails();
}

void UImmortalInventoryWidget::RebuildEquipmentSlots()
{
	EquipmentGrid->ClearChildren();
	if (bShowingMaterials || bShowingPills)
	{
		EquipmentGrid->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	EquipmentGrid->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
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
	if (bShowingPills)
	{
		const TArray<FImmortalPillStack>& Pills = Player->GetPillInventory();
		const int32 VisibleSlots = FMath::Max(Pills.Num(), 10);
		for (int32 Index = 0; Index < VisibleSlots; ++Index)
		{
			const FImmortalPillStack Stack = Pills.IsValidIndex(Index) ? Pills[Index] : FImmortalPillStack();
			UImmortalInventorySlotWidget* SlotWidget = CreateWidget<UImmortalInventorySlotWidget>(
				GetOwningPlayer(), UImmortalInventorySlotWidget::StaticClass());
			SlotWidget->InitializePillSlot(this, Stack,
				Stack.IsValid() && Stack.PillId == SelectedPillId && Stack.Quality == SelectedPillQuality);
			BackpackGrid->AddChildToUniformGrid(SlotWidget, Index / 5, Index % 5);
		}
		return;
	}
	if (bShowingMaterials)
	{
		TArray<FName> MaterialIds = UImmortalMaterialLibrary::GetKnownMaterialIds();
		MaterialIds.Sort([](const FName Left, const FName Right)
		{
			FImmortalMaterialDefinition LeftDefinition;
			FImmortalMaterialDefinition RightDefinition;
			UImmortalMaterialLibrary::GetMaterialDefinition(Left, LeftDefinition);
			UImmortalMaterialLibrary::GetMaterialDefinition(Right, RightDefinition);
			if (LeftDefinition.Category != RightDefinition.Category)
			{
				return static_cast<uint8>(LeftDefinition.Category) < static_cast<uint8>(RightDefinition.Category);
			}
			return LeftDefinition.DisplayName.ToString() < RightDefinition.DisplayName.ToString();
		});
		for (int32 Index = 0; Index < MaterialIds.Num(); ++Index)
		{
			FImmortalMaterialStack Stack;
			Stack.MaterialId = MaterialIds[Index];
			Stack.Quantity = Player->GetMaterialQuantity(Stack.MaterialId);
			UImmortalInventorySlotWidget* SlotWidget = CreateWidget<UImmortalInventorySlotWidget>(
				GetOwningPlayer(), UImmortalInventorySlotWidget::StaticClass());
			SlotWidget->InitializeMaterialSlot(this, Stack, Stack.IsValid() && SelectedMaterialId == Stack.MaterialId);
			BackpackGrid->AddChildToUniformGrid(SlotWidget, Index / 5, Index % 5);
		}
		return;
	}

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

void UImmortalInventoryWidget::HandleMaterialSelected(const FName MaterialId)
{
	SelectedMaterialId = MaterialId;
	RefreshFromPlayer();
}

void UImmortalInventoryWidget::HandlePillSelected(const FName PillId, const EImmortalPillQuality Quality)
{
	SelectedPillId = PillId;
	SelectedPillQuality = Quality;
	RefreshFromPlayer();
}

void UImmortalInventoryWidget::ShowMaterialTab()
{
	bShowingMaterials = true;
	bShowingPills = false;
	RefreshFromPlayer();
}

void UImmortalInventoryWidget::ShowPillTab()
{
	bShowingPills = true;
	bShowingMaterials = false;
	RefreshFromPlayer();
}

void UImmortalInventoryWidget::RefreshDetails()
{
	if (bShowingPills)
	{
		FImmortalPillDefinition Definition;
		const int32 Quantity = Player->GetPillQuantity(SelectedPillId, SelectedPillQuality);
		if (Quantity <= 0 || !UImmortalAlchemyLibrary::GetPillDefinition(SelectedPillId, Definition))
		{
			ItemNameText->SetText(FText::FromString(TEXT("尚未炼得丹药")));
			ItemNameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			ItemDetailsText->SetText(FText::FromString(TEXT("按 L 打开炼丹炉，选择丹方并消耗材料炼制。")));
			ComparisonText->SetText(FText::GetEmpty());
			return;
		}

		const FLinearColor QualityColor = UImmortalAlchemyLibrary::GetQualityColor(SelectedPillQuality);
		ItemNameText->SetText(FText::FromString(FString::Printf(TEXT("%s · %s"),
			*Definition.DisplayName.ToString(),
			*UImmortalAlchemyLibrary::GetQualityText(SelectedPillQuality).ToString())));
		ItemNameText->SetColorAndOpacity(FSlateColor(QualityColor));
		ItemDetailsText->SetText(FText::FromString(FString::Printf(TEXT("当前持有 %d 枚\n%s\n药效：%s"),
			Quantity,
			*Definition.Description.ToString(),
			*UImmortalAlchemyLibrary::GetEffectText(Definition, SelectedPillQuality).ToString())));
		ComparisonText->SetText(FText::FromString(TEXT("可在炼丹炉 [L] 中服用")));
		ComparisonText->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.95f, 0.62f, 1.0f)));
		return;
	}
	if (bShowingMaterials)
	{
		FImmortalMaterialDefinition Definition;
		const int32 Quantity = Player->GetMaterialQuantity(SelectedMaterialId);
		if (Quantity <= 0 || !UImmortalMaterialLibrary::GetMaterialDefinition(SelectedMaterialId, Definition))
		{
			ItemNameText->SetText(FText::FromString(TEXT("请选择已获得的材料")));
			ItemNameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			ItemDetailsText->SetText(FText::FromString(TEXT("材料会自动拾取并按种类堆叠，不占用 30 格装备背包。")));
			ComparisonText->SetText(FText::GetEmpty());
			return;
		}

		ItemNameText->SetText(Definition.DisplayName);
		ItemNameText->SetColorAndOpacity(FSlateColor(Definition.DisplayColor));
		FString MaterialDetails = UImmortalMaterialLibrary::GetCategoryText(Definition.Category).ToString();
		MaterialDetails += FString::Printf(TEXT("  x%d\n"), Quantity);
		MaterialDetails += Definition.Description.ToString();
		ItemDetailsText->SetText(FText::FromString(MaterialDetails));
		ComparisonText->SetText(FText::FromString(TEXT("后续用途：炼丹 / 炼器 / 百宝阁")));
		ComparisonText->SetColorAndOpacity(FSlateColor(FLinearColor(0.42f, 0.92f, 0.76f, 1.0f)));
		return;
	}

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
		TEXT("%s 等级%d · 强化+%d · 洗炼%d\n攻击 %.1f  防御 %.1f  生命 %.1f\n攻速 %.1f%%  暴击 %.1f%%  词条%d  战力 %.1f"),
		*UImmortalEquipmentLibrary::GetSlotText(Item->Slot).ToString(), Item->ItemLevel,
		Item->EnhancementLevel, Item->RefinementCount,
		Item->AttackBonus, Item->DefenseBonus, Item->HealthBonus,
		Item->AttackSpeedBonus * 100.0f, Item->CriticalChanceBonus * 100.0f, Item->Affixes.Num(), ItemPower)));

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

void UImmortalInventoryWidget::RefreshTabAppearance()
{
	if (EquipmentTabText)
	{
		EquipmentTabText->SetColorAndOpacity(FSlateColor(bShowingMaterials || bShowingPills
			? FLinearColor(0.68f, 0.7f, 0.74f, 1.0f)
			: FLinearColor(1.0f, 0.78f, 0.28f, 1.0f)));
	}
	if (MaterialTabText)
	{
		MaterialTabText->SetColorAndOpacity(FSlateColor(bShowingMaterials
			? FLinearColor(0.32f, 1.0f, 0.68f, 1.0f)
			: FLinearColor(0.68f, 0.7f, 0.74f, 1.0f)));
	}
	if (PillTabText)
	{
		PillTabText->SetColorAndOpacity(FSlateColor(bShowingPills
			? FLinearColor(0.92f, 0.5f, 1.0f, 1.0f)
			: FLinearColor(0.68f, 0.7f, 0.74f, 1.0f)));
	}
	if (EquipmentTitleText)
	{
		EquipmentTitleText->SetText(FText::FromString(bShowingMaterials ? TEXT("材料统计") : TEXT("当前装备")));
	}
	if (EquipmentTitleText && bShowingPills)
	{
		EquipmentTitleText->SetText(FText::FromString(TEXT("丹药统计")));
	}
	if (MaterialOverviewText)
	{
		MaterialOverviewText->SetVisibility(bShowingMaterials || bShowingPills
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

void UImmortalInventoryWidget::RefreshMaterialOverview()
{
	if (!MaterialOverviewText || (!bShowingMaterials && !bShowingPills) || !Player.IsValid())
	{
		return;
	}

	if (bShowingPills)
	{
		int32 OrdinaryCount = 0;
		int32 ExceptionalCount = 0;
		for (const FImmortalPillStack& Stack : Player->GetPillInventory())
		{
			if (!Stack.IsValid()) continue;
			if (Stack.Quality == EImmortalPillQuality::Exceptional) ExceptionalCount += Stack.Quantity;
			else OrdinaryCount += Stack.Quantity;
		}
		MaterialOverviewText->SetText(FText::FromString(FString::Printf(
			TEXT("丹药独立堆叠，不占装备背包容量。\n\n普通丹药  %d\n极品丹药  %d\n\n按 L 打开炼丹炉，可查看丹方、炼制丹药并服用。悟道丹的修炼增益只计算在线时间。"),
			OrdinaryCount, ExceptionalCount)));
		return;
	}

	TMap<EImmortalMaterialCategory, int32> CategoryQuantities;
	for (const FImmortalMaterialStack& Stack : Player->GetMaterialInventory())
	{
		FImmortalMaterialDefinition Definition;
		if (Stack.IsValid() && UImmortalMaterialLibrary::GetMaterialDefinition(Stack.MaterialId, Definition))
		{
			CategoryQuantities.FindOrAdd(Definition.Category) += Stack.Quantity;
		}
	}

	FString Summary = TEXT("材料独立堆叠，不占装备背包容量。\n\n");
	for (int32 CategoryIndex = 0; CategoryIndex <= static_cast<int32>(EImmortalMaterialCategory::Artifact); ++CategoryIndex)
	{
		const EImmortalMaterialCategory Category = static_cast<EImmortalMaterialCategory>(CategoryIndex);
		Summary += FString::Printf(TEXT("%s  %d\n"),
			*UImmortalMaterialLibrary::GetCategoryText(Category).ToString(),
			CategoryQuantities.FindRef(Category));
	}
	Summary += TEXT("\n普通妖兽随机掉落材料；守关 Boss 保底妖丹与法宝碎片。离线挂机同样会积累材料。");
	MaterialOverviewText->SetText(FText::FromString(Summary));
}

void UImmortalInventoryWidget::HandleEquipmentTabClicked()
{
	if (bShowingMaterials || bShowingPills)
	{
		bShowingMaterials = false;
		bShowingPills = false;
		RefreshFromPlayer();
	}
}

void UImmortalInventoryWidget::HandleMaterialTabClicked()
{
	if (!bShowingMaterials || bShowingPills)
	{
		bShowingMaterials = true;
		bShowingPills = false;
		RefreshFromPlayer();
	}
}

void UImmortalInventoryWidget::HandlePillTabClicked()
{
	if (!bShowingPills)
	{
		bShowingPills = true;
		bShowingMaterials = false;
		RefreshFromPlayer();
	}
}

void UImmortalInventoryWidget::HandleCloseClicked()
{
	if (Player.IsValid())
	{
		Player->ToggleInventory();
	}
}
