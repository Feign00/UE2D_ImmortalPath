// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalInventorySlotWidget.h"

#include "ImmortalInventoryWidget.h"
#include "ImmortalInventoryPresentation.h"
#include "ImmortalCraftingArt.h"
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

	const TCHAR* GetSlotGlyph(const EImmortalEquipmentSlot Slot)
	{
		switch (Slot)
		{
		case EImmortalEquipmentSlot::Weapon: return TEXT("剑");
		case EImmortalEquipmentSlot::Head: return TEXT("盔");
		case EImmortalEquipmentSlot::Chest: return TEXT("甲");
		case EImmortalEquipmentSlot::Bracers: return TEXT("腕");
		case EImmortalEquipmentSlot::Belt: return TEXT("带");
		case EImmortalEquipmentSlot::Boots: return TEXT("靴");
		case EImmortalEquipmentSlot::RingLeft: return TEXT("戒1");
		case EImmortalEquipmentSlot::RingRight: return TEXT("戒2");
		case EImmortalEquipmentSlot::Accessory: return TEXT("链");
		default: return TEXT("");
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
	UOverlaySlot* FrameSlot = Layers->AddChildToOverlay(QualityFrame);
	FrameSlot->SetHorizontalAlignment(HAlign_Fill);
	FrameSlot->SetVerticalAlignment(VAlign_Fill);

	SlotLabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventorySlotLabel"));
	SlotLabelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.89f, 0.72f)));
	SlotLabelText->SetShadowOffset(FVector2D(1.0f));
	SlotLabelText->SetShadowColorAndOpacity(FLinearColor::Black);
	FSlateFontInfo LabelFont = SlotLabelText->GetFont();
	LabelFont.Size = 11;
	SlotLabelText->SetFont(LabelFont);
	if (UOverlaySlot* LabelSlot = Layers->AddChildToOverlay(SlotLabelText))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Right);
		LabelSlot->SetVerticalAlignment(VAlign_Top);
		LabelSlot->SetPadding(FMargin(0.0f, 4.0f, 6.0f, 0.0f));
	}

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
	if (!SlotButton || !ItemIcon || !QualityFrame || !LevelText || !MaterialGlyphText || !SlotLabelText || !LockGlyphText)
	{
		return;
	}

	SlotButton->SetStyle(ImmortalUITheme::ButtonStyle(bSelected));
	SlotButton->SetToolTipText(FText::GetEmpty());
	SlotButton->SetIsEnabled(false);
	ItemIcon->SetBrush(FSlateBrush());
	ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
	QualityFrame->SetVisibility(ESlateVisibility::Collapsed);
	MaterialGlyphText->SetText(FText::GetEmpty());
	MaterialGlyphText->SetVisibility(ESlateVisibility::Collapsed);
	SlotLabelText->SetText(FText::GetEmpty());
	SlotLabelText->SetVisibility(ESlateVisibility::Collapsed);
	LevelText->SetText(FText::GetEmpty());
	LevelText->SetVisibility(ESlateVisibility::Collapsed);
	LockGlyphText->SetVisibility(ESlateVisibility::Collapsed);
	FSlateFontInfo GlyphFont = MaterialGlyphText->GetFont();
	GlyphFont.Size = 34;
	MaterialGlyphText->SetFont(GlyphFont);
	FSlateFontInfo LevelFont = LevelText->GetFont();
	LevelFont.Size = bArtifactItem ? 11 : 14;
	LevelText->SetFont(LevelFont);

	// Atlas art replaces the central fallback glyph rather than obscuring it.
	const auto ShowItemArt = [this](FSlateBrush Brush, const FText& FallbackGlyph, const FLinearColor Color)
	{
		const bool bHasArt = Brush.DrawAs == ESlateBrushDrawType::Image && Brush.GetResourceObject();
		if (bHasArt)
		{
			Brush.ImageSize = FVector2D(52.0f);
			ItemIcon->SetBrush(Brush);
			ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			FSlateFontInfo FallbackFont = MaterialGlyphText->GetFont();
			FallbackFont.Size = FallbackGlyph.ToString().Len() > 1 ? 22 : 34;
			MaterialGlyphText->SetFont(FallbackFont);
			MaterialGlyphText->SetText(FallbackGlyph);
			MaterialGlyphText->SetColorAndOpacity(FSlateColor(Color));
			MaterialGlyphText->SetShadowColorAndOpacity(FLinearColor::Black);
			MaterialGlyphText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	};
	const auto ShowQualityFrame = [this](const FLinearColor Color)
	{
		QualityFrame->SetBrush(ImmortalUITheme::PanelBrush(FLinearColor::Transparent, Color));
		QualityFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	};

	if (bQuestItem)
	{
		FImmortalQuestItemDefinition Definition;
		if (bHasItem && UImmortalInventoryLibrary::GetQuestItemDefinition(QuestItemStack.QuestItemId, Definition))
		{
			ShowItemArt(FSlateBrush(), Definition.IconGlyph.IsEmpty() ? FText::FromString(TEXT("任")) : Definition.IconGlyph,
				Definition.DisplayColor);
			SlotButton->SetToolTipText(Definition.DisplayName);
			LevelText->SetText(FText::FromString(FString::Printf(TEXT("×%d"), QuestItemStack.Quantity)));
			LevelText->SetVisibility(ESlateVisibility::HitTestInvisible);
			SlotButton->SetIsEnabled(true);
		}
		return;
	}

	if (bArtifactItem)
	{
		FImmortalArtifactDefinition Definition;
		if (bHasItem && UImmortalArtifactLibrary::GetArtifactDefinition(ArtifactItem.ArtifactId, Definition))
		{
			const FLinearColor Color = UImmortalArtifactLibrary::GetQualityColor(Definition.Quality);
			// Artifact illustrations are a separate pending art stage; keep the explicit catalog glyph.
			ShowItemArt(FSlateBrush(), Definition.IconGlyph.IsEmpty() ? FText::FromString(TEXT("宝")) : Definition.IconGlyph, Color);
			ShowQualityFrame(Color);
			SlotButton->SetToolTipText(FText::FromString(FString::Printf(TEXT("%s · %s%s"),
				*Definition.DisplayName.ToString(), *UImmortalArtifactLibrary::GetQualityText(Definition.Quality).ToString(),
				bEquipped ? TEXT(" · 已穿戴") : TEXT(""))));
			LevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv%d ★%d"), ArtifactItem.Level, ArtifactItem.Stars)));
			LevelText->SetVisibility(ESlateVisibility::HitTestInvisible);
			LockGlyphText->SetVisibility(ArtifactItem.bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			SlotButton->SetIsEnabled(true);
		}
		else if (!bHasItem)
		{
			ShowItemArt(FSlateBrush(), FText::FromString(TEXT("法")), FLinearColor(0.48f, 0.38f, 0.58f, 0.55f));
			SlotButton->SetToolTipText(FText::FromString(TEXT("空法宝栏 · 查看法宝")));
			// The empty paper-doll artifact cell remains an entry to the artifact category.
			SlotButton->SetIsEnabled(true);
		}
		return;
	}

	if (bPillItem)
	{
		FImmortalPillDefinition Definition;
		if (bHasItem && UImmortalAlchemyLibrary::GetPillDefinition(PillStack.PillId, Definition))
		{
			const FLinearColor QualityColor = UImmortalAlchemyLibrary::GetQualityColor(PillStack.Quality);
			ShowItemArt(ImmortalAlchemyArt::Brush(AlchemyAtlas.LoadSynchronous(), PillStack.PillId),
				Definition.IconGlyph.IsEmpty() ? FText::FromString(TEXT("丹")) : Definition.IconGlyph, QualityColor);
			ShowQualityFrame(QualityColor);
			SlotButton->SetToolTipText(FText::FromString(FString::Printf(TEXT("%s · %s"),
				*Definition.DisplayName.ToString(), *UImmortalAlchemyLibrary::GetQualityText(PillStack.Quality).ToString())));
			LevelText->SetText(FText::FromString(FString::Printf(TEXT("×%d"), PillStack.Quantity)));
			LevelText->SetVisibility(ESlateVisibility::HitTestInvisible);
			SlotButton->SetIsEnabled(true);
		}
		return;
	}

	if (bMaterialItem)
	{
		FImmortalMaterialDefinition Definition;
		if (bHasItem && UImmortalMaterialLibrary::GetMaterialDefinition(MaterialStack.MaterialId, Definition))
		{
			ShowItemArt(ImmortalCraftingArt::MaterialBrush(ForgeAtlas.LoadSynchronous(), MaterialAtlas.LoadSynchronous(), MaterialStack.MaterialId),
				Definition.IconGlyph.IsEmpty() ? FText::FromString(TEXT("◆")) : Definition.IconGlyph, Definition.DisplayColor);
			SlotButton->SetToolTipText(Definition.DisplayName);
			LevelText->SetText(FText::FromString(FString::Printf(TEXT("×%d"), MaterialStack.Quantity)));
			LevelText->SetVisibility(ESlateVisibility::HitTestInvisible);
			SlotButton->SetIsEnabled(true);
		}
		return;
	}

	const EImmortalEquipmentSlot VisibleSlot = bHasItem ? Item.Slot : PlaceholderSlot;
	if (VisibleSlot != EImmortalEquipmentSlot::MAX)
	{
		SlotButton->SetToolTipText(FText::FromString(bHasItem
			? FString::Printf(TEXT("%s · %s%s"), *Item.DisplayName.ToString(),
				*UImmortalEquipmentLibrary::GetSlotText(VisibleSlot).ToString(), bEquipped ? TEXT(" · 已穿戴") : TEXT(""))
			: UImmortalEquipmentLibrary::GetSlotText(VisibleSlot).ToString()));
		const float Alpha = bHasItem ? 1.0f : 0.32f;
		FSlateBrush Brush = ImmortalCraftingArt::EquipmentBrush(EquipmentAtlas.LoadSynchronous(), VisibleSlot);
		Brush.TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, Alpha));
		ShowItemArt(Brush, FText::FromString(GetSlotGlyph(VisibleSlot)), FLinearColor(0.92f, 0.86f, 0.72f, Alpha));
		if (VisibleSlot == EImmortalEquipmentSlot::Bracers || VisibleSlot == EImmortalEquipmentSlot::Belt
			|| VisibleSlot == EImmortalEquipmentSlot::RingLeft || VisibleSlot == EImmortalEquipmentSlot::RingRight)
		{
			SlotLabelText->SetText(FText::FromString(GetSlotGlyph(VisibleSlot)));
			SlotLabelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.89f, 0.72f, bHasItem ? 1.0f : 0.65f)));
			SlotLabelText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	if (bHasItem)
	{
		ShowQualityFrame(UImmortalEquipmentLibrary::GetQualityColor(Item.Quality));
		LevelText->SetText(FText::AsNumber(Item.ItemLevel));
		LevelText->SetVisibility(ESlateVisibility::HitTestInvisible);
		LockGlyphText->SetVisibility(Item.bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
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
