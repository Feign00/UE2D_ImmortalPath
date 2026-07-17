// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPillSlotWidget.h"

#include "ImmortalAlchemyWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

namespace
{
	FSlateBrush MakePillBrush(const TCHAR* AssetPath, const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = FVector2D(92.0f);
		Brush.TintColor = FSlateColor(Tint);
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, AssetPath)) Brush.SetResourceObject(Texture);
		return Brush;
	}
}

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
	QuantityFont.Size = 13;
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
	const TCHAR* Path = bPillSelected
		? TEXT("/Game/GAME/Asset/ui/inventory/slots/selected.selected")
		: TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal");
	const FLinearColor QualityColor = UImmortalAlchemyLibrary::GetQualityColor(Stack.Quality);
	const FSlateBrush Brush = MakePillBrush(Path, Stack.IsValid() ? QualityColor.CopyWithNewOpacity(0.95f) : FLinearColor::White);
	FButtonStyle Style;
	Style.SetNormal(Brush);
	Style.SetHovered(Brush);
	Style.SetPressed(Brush);
	Style.SetDisabled(Brush);
	Button->SetStyle(Style);
	Button->SetIsEnabled(Stack.IsValid());

	FImmortalPillDefinition Definition;
	if (Stack.IsValid() && UImmortalAlchemyLibrary::GetPillDefinition(Stack.PillId, Definition))
	{
		GlyphText->SetText(Definition.IconGlyph);
		GlyphText->SetColorAndOpacity(FSlateColor(QualityColor));
		GlyphText->SetShadowColorAndOpacity(QualityColor.CopyWithNewOpacity(0.65f));
		GlyphText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		QuantityText->SetText(FText::FromString(FString::Printf(TEXT("×%d"), Stack.Quantity)));
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

