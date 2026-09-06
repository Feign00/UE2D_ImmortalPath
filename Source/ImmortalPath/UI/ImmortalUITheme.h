#pragma once

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "ImmortalIconWidget.h"

namespace ImmortalUITheme
{
	inline FSlateBrush PanelBrush(FLinearColor Color, FLinearColor Outline = FLinearColor(0.56f,0.44f,0.25f))
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = Color;
		Brush.OutlineSettings.CornerRadii = FVector4(2,2,2,2);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Width = 2.0f;
		Brush.OutlineSettings.Color = Outline;
		return Brush;
	}
	inline FButtonStyle ButtonStyle(bool bSelected = false)
	{
		FButtonStyle Style;
		Style.SetNormal(PanelBrush(bSelected ? FLinearColor(0.25f,0.22f,0.13f) : FLinearColor(0.045f,0.063f,0.084f)));
		Style.SetHovered(PanelBrush(FLinearColor(0.13f,0.21f,0.22f), FLinearColor(0.86f,0.70f,0.39f)));
		Style.SetPressed(PanelBrush(FLinearColor(0.24f,0.31f,0.23f)));
		Style.SetDisabled(PanelBrush(FLinearColor(0.035f,0.045f,0.055f),FLinearColor(0.15f,0.17f,0.19f)));
		Style.NormalPadding = FMargin(4);
		Style.PressedPadding = FMargin(4,5,4,3);
		return Style;
	}
	inline void IconButton(UUserWidget* Owner, UButton* Button, int32 Index, const FText& Label, bool bIconOnly = false)
	{
		Button->SetToolTipText(Label);
		Button->ClearChildren();
		UHorizontalBox* Content = Owner->WidgetTree->ConstructWidget<UHorizontalBox>();
		Button->AddChild(Content);
		USizeBox* IconSize = Owner->WidgetTree->ConstructWidget<USizeBox>();
		IconSize->SetWidthOverride(bIconOnly ? 32 : 23);
		IconSize->SetHeightOverride(bIconOnly ? 32 : 23);
		UImmortalIconWidget* Icon = CreateWidget<UImmortalIconWidget>(Owner);
		Icon->SetIcon(Index);
		IconSize->AddChild(Icon);
		Content->AddChildToHorizontalBox(IconSize)->SetVerticalAlignment(VAlign_Center);
		if (!bIconOnly)
		{
			UTextBlock* Text = Owner->WidgetTree->ConstructWidget<UTextBlock>();
			Text->SetText(Label);
			FSlateFontInfo Font = Text->GetFont(); Font.Size=11; Text->SetFont(Font);
			Text->SetColorAndOpacity(FLinearColor(0.95f,0.89f,0.72f));
			UHorizontalBoxSlot* Slot = Content->AddChildToHorizontalBox(Text);
			Slot->SetPadding(FMargin(4,0)); Slot->SetVerticalAlignment(VAlign_Center);
		}
	}
	inline void RestyleFeature(UUserWidget* Page)
	{
		if (!Page || !Page->WidgetTree) return;
		Page->WidgetTree->ForEachWidget([](UWidget* Widget)
		{
			if (UButton* Button = Cast<UButton>(Widget)) Button->SetStyle(ButtonStyle());
			// The old 900x600 frame was stretched over the 1600x270 page.
			if (UImage* Image = Cast<UImage>(Widget))
				if (Image->GetName().EndsWith(TEXT("Background"))) Image->SetVisibility(ESlateVisibility::Collapsed);
		});
	}
}
