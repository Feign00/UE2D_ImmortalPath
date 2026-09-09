// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalInventorySlotWidget.h"

#include "ImmortalInventoryWidget.h"
#include "ImmortalInventoryPresentation.h"
#include "ImmortalUITheme.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

namespace
{
	// Shared cell size for paper-doll equipment and the seven-column backpack.
	constexpr float SlotSize = ImmortalInventoryPresentation::SlotSize;

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
		case EImmortalEquipmentSlot::Bracers:
		case EImmortalEquipmentSlot::Belt: return TEXT("/Game/GAME/Asset/ui/inventory/equipment_icons/clothes.clothes");
		case EImmortalEquipmentSlot::Boots: return TEXT("/Game/GAME/Asset/ui/inventory/equipment_icons/boots.boots");
		case EImmortalEquipmentSlot::RingLeft:
		case EImmortalEquipmentSlot::RingRight:
		case EImmortalEquipmentSlot::Accessory: return TEXT("/Game/GAME/Asset/ui/inventory/equipment_icons/accessory.accessory");
		default: return TEXT("/Game/GAME/Asset/ui/inventory/equipment_icons/weapon.weapon");
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
	UButtonSlot* ContentSlot = CastChecked<UButtonSlot>(Layers->Slot);
	ContentSlot->SetHorizontalAlignment(HAlign_Fill);
	ContentSlot->SetVerticalAlignment(VAlign_Fill);
	SlotButton->SetClipping(EWidgetClipping::ClipToBounds);

	ItemIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("InventoryItemIcon"));
	if (UOverlaySlot* IconSlot = Layers->AddChildToOverlay(ItemIcon))
	{
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(4.0f, 2.0f, 4.0f, 14.0f));
	}

	MaterialGlyphText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventoryMaterialGlyph"));
	MaterialGlyphText->SetJustification(ETextJustify::Center);
	MaterialGlyphText->SetShadowOffset(FVector2D::ZeroVector);
	FSlateFontInfo MaterialFont = MaterialGlyphText->GetFont();
	MaterialFont.Size = 34;
	MaterialGlyphText->SetFont(MaterialFont);
	if (UOverlaySlot* MaterialSlot = Layers->AddChildToOverlay(MaterialGlyphText))
	{
		MaterialSlot->SetHorizontalAlignment(HAlign_Fill);
		MaterialSlot->SetVerticalAlignment(VAlign_Center);
		MaterialSlot->SetPadding(FMargin(12.0f, 5.0f, 12.0f, 22.0f));
	}

	QualityFrame = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("InventoryQualityFrame"));
	MaterialGlyphText->RemoveFromParent();
	SymbolIcon = CreateWidget<UImmortalIconWidget>(this);
	UOverlaySlot* SymbolSlot = Layers->AddChildToOverlay(SymbolIcon);
	SymbolSlot->SetHorizontalAlignment(HAlign_Fill);
	SymbolSlot->SetVerticalAlignment(VAlign_Fill);
	SymbolSlot->SetPadding(FMargin(8.0f, 4.0f, 8.0f, 16.0f));
	UOverlaySlot* FrameSlot = Layers->AddChildToOverlay(QualityFrame);
	FrameSlot->SetHorizontalAlignment(HAlign_Fill);
	FrameSlot->SetVerticalAlignment(VAlign_Fill);

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

	LockGlyphText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventoryLockGlyph"));
	LockGlyphText->SetText(FText::FromString(TEXT("锁")));
	LockGlyphText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.78f, 0.2f, 1.0f)));
	LockGlyphText->SetShadowOffset(FVector2D(1.0f));
	FSlateFontInfo LockFont = LockGlyphText->GetFont();
	LockFont.Size = 12;
	LockGlyphText->SetFont(LockFont);
	if (UOverlaySlot* LockSlot = Layers->AddChildToOverlay(LockGlyphText))
	{
		LockSlot->SetHorizontalAlignment(HAlign_Left);
		LockSlot->SetVerticalAlignment(VAlign_Top);
		LockSlot->SetPadding(FMargin(6.0f, 4.0f, 0.0f, 0.0f));
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
	bMaterialItem = false;
	bPillItem = false;
	bArtifactItem = false;
	bQuestItem = false;
	bHasItem = bInHasItem;
	bEquipped = bInEquipped;
	bSelected = bInSelected;
	PlaceholderSlot = InPlaceholderSlot;
	RefreshAppearance();
}

void UImmortalInventorySlotWidget::InitializeMaterialSlot(
	UImmortalInventoryWidget* InOwner,
	const FImmortalMaterialStack& InStack,
	const bool bInSelected)
{
	OwnerInventory = InOwner;
	MaterialStack = InStack;
	bMaterialItem = true;
	bPillItem = false;
	bArtifactItem = false;
	bQuestItem = false;
	bHasItem = InStack.IsValid();
	bEquipped = false;
	bSelected = bInSelected;
	PlaceholderSlot = EImmortalEquipmentSlot::MAX;
	RefreshAppearance();
}

void UImmortalInventorySlotWidget::InitializePillSlot(
	UImmortalInventoryWidget* InOwner,
	const FImmortalPillStack& InStack,
	const bool bInSelected)
{
	OwnerInventory = InOwner;
	PillStack = InStack;
	bPillItem = true;
	bMaterialItem = false;
	bArtifactItem = false;
	bQuestItem = false;
	bHasItem = InStack.IsValid();
	bEquipped = false;
	bSelected = bInSelected;
	PlaceholderSlot = EImmortalEquipmentSlot::MAX;
	RefreshAppearance();
}

void UImmortalInventorySlotWidget::InitializeArtifactSlot(
	UImmortalInventoryWidget* InOwner,
	const FImmortalArtifactItem& InItem,
	const bool bInEquipped,
	const bool bInSelected)
{
	OwnerInventory = InOwner;
	ArtifactItem = InItem;
	bArtifactItem = true;
	bQuestItem = false;
	bPillItem = false;
	bMaterialItem = false;
	bHasItem = InItem.IsValid();
	bEquipped = bInEquipped;
	bSelected = bInSelected;
	PlaceholderSlot = EImmortalEquipmentSlot::MAX;
	RefreshAppearance();
}

void UImmortalInventorySlotWidget::InitializeQuestItemSlot(
	UImmortalInventoryWidget* InOwner,
	const FImmortalQuestItemStack& InStack,
	const bool bInSelected)
{
	OwnerInventory = InOwner;
	QuestItemStack = InStack;
	bQuestItem = true;
	bArtifactItem = false;
	bPillItem = false;
	bMaterialItem = false;
	bHasItem = InStack.IsValid();
	bEquipped = false;
	bSelected = bInSelected;
	PlaceholderSlot = EImmortalEquipmentSlot::MAX;
	RefreshAppearance();
}

void UImmortalInventorySlotWidget::RefreshAppearance()
{
	if (!SlotButton || !ItemIcon || !QualityFrame || !LevelText || !MaterialGlyphText || !LockGlyphText)
	{
		return;
	}

	SlotButton->SetStyle(ImmortalUITheme::ButtonStyle(bSelected));
	SymbolIcon->SetVisibility((bQuestItem || bArtifactItem || bPillItem || bMaterialItem)
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SymbolIcon->SetIcon(bQuestItem ? 9 : bArtifactItem ? 4 : bPillItem ? 2
		: MaterialStack.MaterialId.ToString().Contains(TEXT("Herb")) ? 11 : 19);
	LockGlyphText->SetVisibility(ESlateVisibility::Collapsed);

	if (bQuestItem)
	{
		ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
		QualityFrame->SetVisibility(ESlateVisibility::Collapsed);
		FImmortalQuestItemDefinition Definition;
		if (bHasItem && UImmortalInventoryLibrary::GetQuestItemDefinition(QuestItemStack.QuestItemId, Definition))
		{
			MaterialGlyphText->SetText(Definition.IconGlyph.IsEmpty() ? FText::FromString(TEXT("任")) : Definition.IconGlyph);
			SlotButton->SetToolTipText(Definition.DisplayName);
			MaterialGlyphText->SetColorAndOpacity(FSlateColor(Definition.DisplayColor));
			MaterialGlyphText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			LevelText->SetText(FText::FromString(FString::Printf(TEXT("×%d"), QuestItemStack.Quantity)));
			LevelText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			MaterialGlyphText->SetVisibility(ESlateVisibility::Collapsed);
			LevelText->SetVisibility(ESlateVisibility::Collapsed);
		}
		SlotButton->SetIsEnabled(bHasItem);
		return;
	}

	if (bArtifactItem)
	{
		ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
		QualityFrame->SetVisibility(ESlateVisibility::Collapsed);
		FImmortalArtifactDefinition Definition;
		if (bHasItem && UImmortalArtifactLibrary::GetArtifactDefinition(ArtifactItem.ArtifactId, Definition))
		{
			const FLinearColor Color = UImmortalArtifactLibrary::GetQualityColor(Definition.Quality);
			SlotButton->SetToolTipText(Definition.DisplayName);
			MaterialGlyphText->SetText(Definition.IconGlyph.IsEmpty() ? FText::FromString(TEXT("宝")) : Definition.IconGlyph);
			MaterialGlyphText->SetColorAndOpacity(FSlateColor(Color));
			MaterialGlyphText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			LevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv%d ★%d"), ArtifactItem.Level, ArtifactItem.Stars)));
			LevelText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			LockGlyphText->SetVisibility(ArtifactItem.bLocked ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
		else
		{
			MaterialGlyphText->SetText(FText::FromString(TEXT("法")));
			MaterialGlyphText->SetColorAndOpacity(FSlateColor(FLinearColor(0.48f, 0.38f, 0.58f, 0.75f)));
			MaterialGlyphText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			LevelText->SetText(FText::FromString(TEXT("法宝")));
			LevelText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		SlotButton->SetIsEnabled(true);
		return;
	}

	if (bPillItem)
	{
		ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
		QualityFrame->SetVisibility(ESlateVisibility::Collapsed);
		FImmortalPillDefinition Definition;
		if (bHasItem && UImmortalAlchemyLibrary::GetPillDefinition(PillStack.PillId, Definition))
		{
			const FLinearColor QualityColor = UImmortalAlchemyLibrary::GetQualityColor(PillStack.Quality);
			SlotButton->SetToolTipText(Definition.DisplayName);
			MaterialGlyphText->SetText(Definition.IconGlyph);
			MaterialGlyphText->SetColorAndOpacity(FSlateColor(QualityColor));
			MaterialGlyphText->SetShadowColorAndOpacity(QualityColor.CopyWithNewOpacity(0.65f));
			MaterialGlyphText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			LevelText->SetText(FText::FromString(FString::Printf(TEXT("×%d"), PillStack.Quantity)));
			LevelText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			MaterialGlyphText->SetVisibility(ESlateVisibility::Collapsed);
			LevelText->SetVisibility(ESlateVisibility::Collapsed);
		}
		SlotButton->SetIsEnabled(bHasItem);
		return;
	}

	if (bMaterialItem)
	{
		ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
		QualityFrame->SetVisibility(ESlateVisibility::Collapsed);
		FImmortalMaterialDefinition Definition;
		if (bHasItem && UImmortalMaterialLibrary::GetMaterialDefinition(MaterialStack.MaterialId, Definition))
		{
			MaterialGlyphText->SetText(Definition.IconGlyph.IsEmpty() ? FText::FromString(TEXT("◆")) : Definition.IconGlyph);
			SlotButton->SetToolTipText(Definition.DisplayName);
			MaterialGlyphText->SetColorAndOpacity(FSlateColor(Definition.DisplayColor));
			MaterialGlyphText->SetShadowColorAndOpacity(Definition.DisplayColor.CopyWithNewOpacity(0.65f));
			MaterialGlyphText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			LevelText->SetText(FText::FromString(FString::Printf(TEXT("×%d"), MaterialStack.Quantity)));
			LevelText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			MaterialGlyphText->SetVisibility(ESlateVisibility::Collapsed);
			LevelText->SetVisibility(ESlateVisibility::Collapsed);
		}
		SlotButton->SetIsEnabled(bHasItem);
		return;
	}

	MaterialGlyphText->SetVisibility(ESlateVisibility::Collapsed);
	const EImmortalEquipmentSlot VisibleSlot = bHasItem ? Item.Slot : PlaceholderSlot;
	if (VisibleSlot != EImmortalEquipmentSlot::MAX)
	{
		SlotButton->SetToolTipText(FText::FromString(bHasItem
			? FString::Printf(TEXT("%s · %s%s"), *Item.DisplayName.ToString(),
				*UImmortalEquipmentLibrary::GetSlotText(VisibleSlot).ToString(), bEquipped ? TEXT(" · 已穿戴") : TEXT(""))
			: UImmortalEquipmentLibrary::GetSlotText(VisibleSlot).ToString()));
		const float Alpha = bHasItem ? 1.0f : 0.32f;
		ItemIcon->SetBrush(MakeInventoryBrush(GetSlotTexturePath(VisibleSlot), FVector2D(52.0f), FLinearColor(1.0f, 1.0f, 1.0f, Alpha)));
		ItemIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		FString SlotGlyph;
		switch (VisibleSlot)
		{
		case EImmortalEquipmentSlot::Bracers: SlotGlyph = TEXT("腕"); break;
		case EImmortalEquipmentSlot::Belt: SlotGlyph = TEXT("带"); break;
		case EImmortalEquipmentSlot::RingLeft: SlotGlyph = TEXT("戒1"); break;
		case EImmortalEquipmentSlot::RingRight: SlotGlyph = TEXT("戒2"); break;
		default: break;
		}
		if (!SlotGlyph.IsEmpty())
		{
			FSlateFontInfo GlyphFont = MaterialGlyphText->GetFont();
			GlyphFont.Size = 18;
			MaterialGlyphText->SetFont(GlyphFont);
			MaterialGlyphText->SetText(FText::FromString(SlotGlyph));
			MaterialGlyphText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.86f, 0.72f, Alpha)));
			MaterialGlyphText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}
	else
	{
		ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (bHasItem)
	{
		QualityFrame->SetBrush(ImmortalUITheme::PanelBrush(FLinearColor::Transparent,
			UImmortalEquipmentLibrary::GetQualityColor(Item.Quality)));
		QualityFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		LevelText->SetText(FText::AsNumber(Item.ItemLevel));
		LevelText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		LockGlyphText->SetVisibility(Item.bLocked ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
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
	if ((bHasItem || bArtifactItem) && OwnerInventory.IsValid())
	{
		if (bPillItem)
		{
			OwnerInventory->HandlePillSelected(PillStack.PillId, PillStack.Quality);
		}
		else if (bMaterialItem)
		{
			OwnerInventory->HandleMaterialSelected(MaterialStack.MaterialId);
		}
		else if (bArtifactItem)
		{
			OwnerInventory->HandleArtifactSelected(ArtifactItem.InstanceId);
		}
		else if (bQuestItem)
		{
			OwnerInventory->HandleQuestItemSelected(QuestItemStack.QuestItemId);
		}
		else
		{
			OwnerInventory->HandleSlotSelected(Item.ItemId);
		}
	}
}
