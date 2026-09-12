// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPillSlotWidget.h"

#include "ImmortalAlchemyWidget.h"
#include "ImmortalAlchemyArt.h"
#include "ImmortalUITheme.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

void UImmortalPillSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PillSlotSize"));
	Root->SetWidthOverride(92.0f);
	Root->SetHeightOverride(92.0f);
	WidgetTree->RootWidget = Root;
	Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PillButton"));
	Button->OnClicked.AddDynamic(this, &UImmortalPillSlotWidget::HandleClicked);
	Root->AddChild(Button);
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PillLayers"));
	Layers->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Button->AddChild(Layers);
	PillArt = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PillArt"));
	USizeBox* ArtSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PillArtSize"));
	ArtSize->SetWidthOverride(64);
	ArtSize->SetHeightOverride(64);
	ArtSize->AddChild(PillArt);
	if (UOverlaySlot* ArtSlot = Layers->AddChildToOverlay(ArtSize))
	{
		ArtSlot->SetHorizontalAlignment(HAlign_Center);
		ArtSlot->SetVerticalAlignment(VAlign_Center);
		ArtSlot->SetPadding(FMargin(0, 0, 0, 14));
	}
	PillArt->SetVisibility(ESlateVisibility::HitTestInvisible);

	GlyphText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PillGlyph"));
	GlyphText->SetJustification(ETextJustify::Center);
	GlyphText->SetShadowOffset(FVector2D::ZeroVector);
	FSlateFontInfo GlyphFont = GlyphText->GetFont();
	GlyphFont.Size = 36;
	GlyphText->SetFont(GlyphFont);
	if (UOverlaySlot* GlyphSlot = Layers->AddChildToOverlay(GlyphText))
	{
		GlyphSlot->SetHorizontalAlignment(HAlign_Fill);
		GlyphSlot->SetVerticalAlignment(VAlign_Center);
		GlyphSlot->SetPadding(FMargin(8.0f, 4.0f, 8.0f, 18.0f));
	}

	QuantityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PillQuantity"));
	QuantityText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	QuantityText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	FSlateFontInfo QuantityFont = QuantityText->GetFont();
	QuantityFont.Size = 15;
	QuantityText->SetFont(QuantityFont);
	if (UOverlaySlot* QuantitySlot = Layers->AddChildToOverlay(QuantityText))
	{
		QuantitySlot->SetHorizontalAlignment(HAlign_Right);
		QuantitySlot->SetVerticalAlignment(VAlign_Bottom);
		QuantitySlot->SetPadding(FMargin(0.0f, 0.0f, 7.0f, 5.0f));
	}
	RefreshAppearance();
}

void UImmortalPillSlotWidget::InitializePill(
	UImmortalAlchemyWidget* InOwner,
	const FImmortalPillStack& InStack,
	const bool bSelected)
{
	OwnerAlchemy = InOwner;
	Stack = InStack;
	bPillSelected = bSelected;
	RefreshAppearance();
}

void UImmortalPillSlotWidget::RefreshAppearance()
{
	if (!Button || !GlyphText || !QuantityText) return;
	const FLinearColor QualityColor = UImmortalAlchemyLibrary::GetQualityColor(Stack.Quality);
	Button->SetStyle(ImmortalUITheme::ButtonStyle(bPillSelected));
	Button->SetIsEnabled(Stack.IsValid());
	const FSlateBrush ArtBrush = ImmortalAlchemyArt::Brush(OwnerAlchemy.IsValid() ? OwnerAlchemy->GetAlchemyAtlas() : nullptr, Stack.IsValid() ? Stack.PillId : NAME_None);
	PillArt->SetBrush(ArtBrush);

	FImmortalPillDefinition Definition;
	if (Stack.IsValid() && UImmortalAlchemyLibrary::GetPillDefinition(Stack.PillId, Definition))
	{
		GlyphText->SetText(Definition.IconGlyph);
		GlyphText->SetColorAndOpacity(FSlateColor(QualityColor));
		GlyphText->SetShadowColorAndOpacity(QualityColor.CopyWithNewOpacity(0.65f));
		GlyphText->SetVisibility(ArtBrush.DrawAs == ESlateBrushDrawType::Image ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		Button->SetToolTipText(FText::FromString(FString::Printf(TEXT("%s · %s"), *Definition.DisplayName.ToString(), *UImmortalAlchemyLibrary::GetQualityText(Stack.Quality).ToString())));
		QuantityText->SetColorAndOpacity(FSlateColor(QualityColor));
		QuantityText->SetText(FText::FromString(FString::Printf(TEXT("%s ×%d"),
			Stack.Quality == EImmortalPillQuality::Exceptional ? TEXT("极") : TEXT("普"), Stack.Quantity)));
		QuantityText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		GlyphText->SetVisibility(ESlateVisibility::Collapsed);
		QuantityText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UImmortalPillSlotWidget::HandleClicked()
{
	if (Stack.IsValid() && OwnerAlchemy.IsValid()) OwnerAlchemy->SelectPill(Stack.PillId, Stack.Quality);
}
