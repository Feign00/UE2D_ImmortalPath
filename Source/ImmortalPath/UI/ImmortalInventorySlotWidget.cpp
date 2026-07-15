// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalInventorySlotWidget.h"

#include "ImmortalInventoryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

namespace
{
	constexpr float SlotSize = 96.0f;

	FSlateBrush MakeInventoryBrush(const TCHAR* AssetPath, const FVector2D Size, const FLinearColor Tint = FLinearColor::White)
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

	const TCHAR* GetSlotTexturePath(const EImmortalEquipmentSlot Slot)
	{
		switch (Slot)
		{
		case EImmortalEquipmentSlot::Head: return TEXT("/Game/GAME/Asset/ui/inventory/equipment_icons/head.head");
		case EImmortalEquipmentSlot::Chest: return TEXT("/Game/GAME/Asset/ui/inventory/equipment_icons/clothes.clothes");
		case EImmortalEquipmentSlot::Boots: return TEXT("/Game/GAME/Asset/ui/inventory/equipment_icons/boots.boots");
		case EImmortalEquipmentSlot::Accessory: return TEXT("/Game/GAME/Asset/ui/inventory/equipment_icons/accessory.accessory");
		default: return TEXT("/Game/GAME/Asset/ui/inventory/equipment_icons/weapon.weapon");
		}
	}

	const TCHAR* GetQualityFramePath(const EImmortalEquipmentQuality Quality)
	{
		switch (Quality)
		{
		case EImmortalEquipmentQuality::Uncommon: return TEXT("/Game/GAME/Asset/ui/inventory/quality_frames/green.green");
		case EImmortalEquipmentQuality::Rare: return TEXT("/Game/GAME/Asset/ui/inventory/quality_frames/blue.blue");
		case EImmortalEquipmentQuality::Epic: return TEXT("/Game/GAME/Asset/ui/inventory/quality_frames/purple.purple");
		case EImmortalEquipmentQuality::Legendary: return TEXT("/Game/GAME/Asset/ui/inventory/quality_frames/gold.gold");
		default: return TEXT("/Game/GAME/Asset/ui/inventory/quality_frames/white.white");
		}
	}
}

void UImmortalInventorySlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("InventorySlotSize"));
	RootBox->SetWidthOverride(SlotSize);
	RootBox->SetHeightOverride(SlotSize);
	WidgetTree->RootWidget = RootBox;

	SlotButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("InventorySlotButton"));
	SlotButton->OnClicked.AddDynamic(this, &UImmortalInventorySlotWidget::HandleClicked);
	RootBox->AddChild(SlotButton);

	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("InventorySlotLayers"));
	Layers->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SlotButton->AddChild(Layers);

	ItemIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("InventoryItemIcon"));
	if (UOverlaySlot* IconSlot = Layers->AddChildToOverlay(ItemIcon))
	{
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(16.0f));
	}

	QualityFrame = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("InventoryQualityFrame"));
	Layers->AddChildToOverlay(QualityFrame);

	LevelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventoryItemLevel"));
	LevelText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	LevelText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	LevelText->SetShadowColorAndOpacity(FLinearColor::Black);
	FSlateFontInfo LevelFont = LevelText->GetFont();
	LevelFont.Size = 14;
	LevelText->SetFont(LevelFont);
	if (UOverlaySlot* TextSlot = Layers->AddChildToOverlay(LevelText))
	{
		TextSlot->SetHorizontalAlignment(HAlign_Right);
		TextSlot->SetVerticalAlignment(VAlign_Bottom);
		TextSlot->SetPadding(FMargin(0.0f, 0.0f, 9.0f, 7.0f));
	}

	RefreshAppearance();
}

void UImmortalInventorySlotWidget::InitializeSlot(
	UImmortalInventoryWidget* InOwner,
	const FImmortalEquipmentItem& InItem,
	const bool bInHasItem,
	const bool bInEquipped,
	const bool bInSelected,
	const EImmortalEquipmentSlot InPlaceholderSlot)
{
	OwnerInventory = InOwner;
	Item = InItem;
	bHasItem = bInHasItem;
	bEquipped = bInEquipped;
	bSelected = bInSelected;
	PlaceholderSlot = InPlaceholderSlot;
	RefreshAppearance();
}

void UImmortalInventorySlotWidget::RefreshAppearance()
{
	if (!SlotButton || !ItemIcon || !QualityFrame || !LevelText)
	{
		return;
	}

	const TCHAR* StatePath = bSelected
		? TEXT("/Game/GAME/Asset/ui/inventory/slots/selected.selected")
		: (bEquipped
			? TEXT("/Game/GAME/Asset/ui/inventory/slots/equipped.equipped")
			: TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal"));
	const FSlateBrush StateBrush = MakeInventoryBrush(StatePath, FVector2D(SlotSize));
	FButtonStyle ButtonStyle;
	ButtonStyle.SetNormal(StateBrush);
	ButtonStyle.SetHovered(StateBrush);
	ButtonStyle.SetPressed(StateBrush);
	ButtonStyle.SetDisabled(StateBrush);
	SlotButton->SetStyle(ButtonStyle);

	const EImmortalEquipmentSlot VisibleSlot = bHasItem ? Item.Slot : PlaceholderSlot;
	if (VisibleSlot != EImmortalEquipmentSlot::MAX)
	{
		const float Alpha = bHasItem ? 1.0f : 0.32f;
		ItemIcon->SetBrush(MakeInventoryBrush(GetSlotTexturePath(VisibleSlot), FVector2D(64.0f), FLinearColor(1.0f, 1.0f, 1.0f, Alpha)));
		ItemIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (bHasItem)
	{
		QualityFrame->SetBrush(MakeInventoryBrush(GetQualityFramePath(Item.Quality), FVector2D(SlotSize)));
		QualityFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		LevelText->SetText(FText::AsNumber(Item.ItemLevel));
		LevelText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		QualityFrame->SetVisibility(ESlateVisibility::Collapsed);
		LevelText->SetVisibility(ESlateVisibility::Collapsed);
	}

	SlotButton->SetIsEnabled(bHasItem);
}

void UImmortalInventorySlotWidget::HandleClicked()
{
	if (bHasItem && OwnerInventory.IsValid())
	{
		OwnerInventory->HandleSlotSelected(Item.ItemId);
	}
}
