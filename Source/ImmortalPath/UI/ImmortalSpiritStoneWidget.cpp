// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalSpiritStoneWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

void UImmortalSpiritStoneWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SpiritStoneSize"));
	Root->SetWidthOverride(80.0f);
	Root->SetHeightOverride(80.0f);
	WidgetTree->RootWidget = Root;

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("SpiritStoneLayers"));
	Root->AddChild(Overlay);

	UTextBlock* Gem = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpiritStoneGem"));
	Gem->SetText(FText::FromString(TEXT("◆")));
	Gem->SetColorAndOpacity(FSlateColor(FLinearColor(0.25f, 1.0f, 0.78f, 1.0f)));
	Gem->SetShadowOffset(FVector2D(0.0f, 0.0f));
	Gem->SetShadowColorAndOpacity(FLinearColor(0.1f, 0.9f, 1.0f, 0.9f));
	FSlateFontInfo GemFont = Gem->GetFont();
	GemFont.Size = 48;
	Gem->SetFont(GemFont);
	if (UOverlaySlot* GemSlot = Overlay->AddChildToOverlay(Gem))
	{
		GemSlot->SetHorizontalAlignment(HAlign_Center);
		GemSlot->SetVerticalAlignment(VAlign_Top);
	}

	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpiritStoneLabel"));
	Label->SetText(FText::FromString(TEXT("灵石")));
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Label->SetShadowOffset(FVector2D(1.0f, 1.0f));
	FSlateFontInfo LabelFont = Label->GetFont();
	LabelFont.Size = 15;
	Label->SetFont(LabelFont);
	if (UOverlaySlot* LabelSlot = Overlay->AddChildToOverlay(Label))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Bottom);
	}
}
