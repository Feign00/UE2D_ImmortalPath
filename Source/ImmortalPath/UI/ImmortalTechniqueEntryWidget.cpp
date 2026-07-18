// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalTechniqueEntryWidget.h"

#include "ImmortalTechniqueWidget.h"
#include "../Techniques/ImmortalTechniqueTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

namespace
{
	FSlateBrush MakeTechniqueEntryBrush(const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = FVector2D(250.0f, 58.0f);
		Brush.TintColor = FSlateColor(Tint);
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal")))
		{
			Brush.SetResourceObject(Texture);
		}
		return Brush;
	}
}

void UImmortalTechniqueEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TechniqueEntrySize"));
	Root->SetWidthOverride(250.0f);
	Root->SetHeightOverride(58.0f);
	WidgetTree->RootWidget = Root;
	EntryButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TechniqueEntryButton"));
	EntryButton->OnClicked.AddDynamic(this, &UImmortalTechniqueEntryWidget::HandleClicked);
	Root->AddChild(EntryButton);
	EntryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TechniqueEntryText"));
	EntryText->SetJustification(ETextJustify::Center);
	EntryText->SetAutoWrapText(true);
	EntryText->SetShadowOffset(FVector2D(1.0f));
	EntryText->SetShadowColorAndOpacity(FLinearColor::Black);
	FSlateFontInfo Font = EntryText->GetFont();
	Font.Size = 15;
	EntryText->SetFont(Font);
	EntryButton->AddChild(EntryText);
	RefreshAppearance();
}

void UImmortalTechniqueEntryWidget::InitializeEntry(
	UImmortalTechniqueWidget* InOwner,
	const FName InTechniqueId,
	const bool bLearned,
	const int32 Level,
	const int32 BreakthroughRank,
	const int32 EquippedSlot,
	const bool bUnlocked,
	const bool bInSelected)
{
	OwnerTechnique = InOwner;
	TechniqueId = InTechniqueId;
	bEnabled = true;
	bSelected = bInSelected;
	FImmortalTechniqueDefinition Definition;
	if (UImmortalTechniqueLibrary::GetTechniqueDefinition(TechniqueId, Definition))
	{
		if (bLearned)
		{
			const FString SlotText = EquippedSlot >= 0
				? FString::Printf(TEXT("[槽位%d] "), EquippedSlot + 1)
				: FString();
			DisplayText = FText::FromString(FString::Printf(TEXT("%s%s  %s\n%d级 · 突破%d"),
				*SlotText, *Definition.IconGlyph.ToString(), *Definition.DisplayName.ToString(), Level, BreakthroughRank));
			DisplayColor = UImmortalTechniqueLibrary::GetQualityColor(Definition.Quality);
		}
		else if (bUnlocked)
		{
			DisplayText = FText::FromString(FString::Printf(TEXT("%s  %s\n尚未习得"),
				*Definition.IconGlyph.ToString(), *Definition.DisplayName.ToString()));
			DisplayColor = UImmortalTechniqueLibrary::GetQualityColor(Definition.Quality);
		}
		else
		{
			DisplayText = FText::FromString(FString::Printf(TEXT("未解锁 · 第%d关 / 境界%d"),
				Definition.MinimumQingyunStage, Definition.MinimumRealmIndex + 1));
			DisplayColor = FLinearColor(0.48f, 0.5f, 0.55f, 1.0f);
		}
	}
	RefreshAppearance();
}

void UImmortalTechniqueEntryWidget::RefreshAppearance()
{
	if (!EntryButton || !EntryText) return;
	const FLinearColor Tint = bSelected
		? FLinearColor(0.18f, 0.55f, 0.72f, 1.0f)
		: FLinearColor(0.14f, 0.23f, 0.32f, 0.92f);
	const FSlateBrush Brush = MakeTechniqueEntryBrush(Tint);
	FButtonStyle Style;
	Style.SetNormal(Brush);
	Style.SetHovered(Brush);
	Style.SetPressed(Brush);
	Style.SetDisabled(Brush);
	EntryButton->SetStyle(Style);
	EntryButton->SetIsEnabled(bEnabled);
	EntryText->SetText(DisplayText);
	EntryText->SetColorAndOpacity(FSlateColor(DisplayColor));
}

void UImmortalTechniqueEntryWidget::HandleClicked()
{
	if (OwnerTechnique.IsValid()) OwnerTechnique->SelectTechnique(TechniqueId);
}
