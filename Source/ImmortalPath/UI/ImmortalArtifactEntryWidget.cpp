// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalArtifactEntryWidget.h"

#include "ImmortalArtifactWidget.h"
#include "../Artifacts/ImmortalArtifactTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

namespace
{
	FSlateBrush MakeArtifactEntryBrush(const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = FVector2D(245.0f, 48.0f);
		Brush.TintColor = FSlateColor(Tint);
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal")))
		{
			Brush.SetResourceObject(Texture);
		}
		return Brush;
	}
}

void UImmortalArtifactEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ArtifactEntrySize"));
	Root->SetWidthOverride(245.0f);
	Root->SetHeightOverride(48.0f);
	WidgetTree->RootWidget = Root;
	EntryButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ArtifactEntryButton"));
	EntryButton->OnClicked.AddDynamic(this, &UImmortalArtifactEntryWidget::HandleClicked);
	Root->AddChild(EntryButton);
	EntryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ArtifactEntryText"));
	EntryText->SetJustification(ETextJustify::Center);
	EntryText->SetShadowOffset(FVector2D(1.0f));
	EntryText->SetShadowColorAndOpacity(FLinearColor::Black);
	FSlateFontInfo Font = EntryText->GetFont();
	Font.Size = 16;
	EntryText->SetFont(Font);
	EntryButton->AddChild(EntryText);
	RefreshAppearance();
}

void UImmortalArtifactEntryWidget::InitializeDefinitionEntry(
	UImmortalArtifactWidget* InOwner,
	const FName InArtifactId,
	const bool bUnlocked,
	const bool bInSelected)
{
	OwnerArtifact = InOwner;
	ArtifactId = InArtifactId;
	InstanceId.Invalidate();
	bDefinitionEntry = true;
	bEnabled = true;
	bSelected = bInSelected;
	FImmortalArtifactDefinition Definition;
	if (UImmortalArtifactLibrary::GetArtifactDefinition(ArtifactId, Definition))
	{
		DisplayText = bUnlocked
			? FText::FromString(FString::Printf(TEXT("%s  %s"), *Definition.IconGlyph.ToString(), *Definition.DisplayName.ToString()))
			: FText::FromString(FString::Printf(TEXT("未解锁 · 第 %d 关"), Definition.MinimumQingyunStage));
		DisplayColor = bUnlocked
			? UImmortalArtifactLibrary::GetQualityColor(Definition.Quality)
			: FLinearColor(0.5f, 0.52f, 0.56f, 1.0f);
	}
	RefreshAppearance();
}

void UImmortalArtifactEntryWidget::InitializeArtifactEntry(
	UImmortalArtifactWidget* InOwner,
	const FImmortalArtifactItem& Item,
	const bool bEquipped,
	const bool bInSelected)
{
	OwnerArtifact = InOwner;
	ArtifactId = Item.ArtifactId;
	InstanceId = Item.InstanceId;
	bDefinitionEntry = false;
	bEnabled = Item.IsValid();
	bSelected = bInSelected;
	FImmortalArtifactDefinition Definition;
	if (UImmortalArtifactLibrary::GetArtifactDefinition(Item.ArtifactId, Definition))
	{
		DisplayText = FText::FromString(FString::Printf(TEXT("%s%s · %d级 · %d星"),
			bEquipped ? TEXT("[已装备] ") : TEXT(""), *Definition.DisplayName.ToString(), Item.Level, Item.Stars));
		DisplayColor = UImmortalArtifactLibrary::GetQualityColor(Definition.Quality);
	}
	RefreshAppearance();
}

void UImmortalArtifactEntryWidget::RefreshAppearance()
{
	if (!EntryButton || !EntryText) return;
	const FLinearColor Tint = bSelected
		? FLinearColor(0.48f, 0.28f, 0.72f, 1.0f)
		: FLinearColor(0.28f, 0.32f, 0.42f, bEnabled ? 0.9f : 0.45f);
	const FSlateBrush Brush = MakeArtifactEntryBrush(Tint);
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

void UImmortalArtifactEntryWidget::HandleClicked()
{
	if (!OwnerArtifact.IsValid()) return;
	if (bDefinitionEntry) OwnerArtifact->SelectDefinition(ArtifactId);
	else OwnerArtifact->SelectArtifact(InstanceId);
}

