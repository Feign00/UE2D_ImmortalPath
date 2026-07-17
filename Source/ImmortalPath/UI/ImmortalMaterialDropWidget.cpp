// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalMaterialDropWidget.h"

#include "../Items/ImmortalMaterialTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

void UImmortalMaterialDropWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MaterialDropSize"));
	Root->SetWidthOverride(112.0f);
	Root->SetHeightOverride(88.0f);
	WidgetTree->RootWidget = Root;

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MaterialDropLayers"));
	Root->AddChild(Overlay);

	GlyphText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MaterialGlyph"));
	GlyphText->SetText(FText::FromString(TEXT("◆")));
	GlyphText->SetJustification(ETextJustify::Center);
	GlyphText->SetShadowOffset(FVector2D::ZeroVector);
	GlyphText->SetShadowColorAndOpacity(FLinearColor(0.25f, 1.0f, 0.65f, 0.9f));
	FSlateFontInfo GlyphFont = GlyphText->GetFont();
	GlyphFont.Size = 48;
	GlyphText->SetFont(GlyphFont);
	if (UOverlaySlot* GlyphOverlaySlot = Overlay->AddChildToOverlay(GlyphText))
	{
		GlyphOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		GlyphOverlaySlot->SetVerticalAlignment(VAlign_Top);
	}

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MaterialName"));
	NameText->SetText(FText::FromString(TEXT("材料")));
	NameText->SetJustification(ETextJustify::Center);
	NameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	NameText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	NameText->SetShadowColorAndOpacity(FLinearColor::Black);
	FSlateFontInfo NameFont = NameText->GetFont();
	NameFont.Size = 15;
	NameText->SetFont(NameFont);
	if (UOverlaySlot* NameOverlaySlot = Overlay->AddChildToOverlay(NameText))
	{
		NameOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		NameOverlaySlot->SetVerticalAlignment(VAlign_Bottom);
	}
}

void UImmortalMaterialDropWidget::SetMaterial(
	const FImmortalMaterialDefinition& Definition,
	const int32 Quantity)
{
	if (GlyphText)
	{
		GlyphText->SetText(Definition.IconGlyph.IsEmpty() ? FText::FromString(TEXT("◆")) : Definition.IconGlyph);
		GlyphText->SetColorAndOpacity(FSlateColor(Definition.DisplayColor));
		GlyphText->SetShadowColorAndOpacity(Definition.DisplayColor.CopyWithNewOpacity(0.75f));
	}
	if (NameText)
	{
		NameText->SetText(FText::FromString(FString::Printf(
			TEXT("%s ×%d"), *Definition.DisplayName.ToString(), FMath::Max(Quantity, 1))));
	}
}
