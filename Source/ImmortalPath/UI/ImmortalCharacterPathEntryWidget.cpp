// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalCharacterPathEntryWidget.h"

#include "ImmortalCharacterBuildWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

namespace
{
	FSlateBrush MakeBuildEntryBrush(const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = FVector2D(230.0f, 62.0f);
		Brush.TintColor = FSlateColor(Tint);
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal"))) Brush.SetResourceObject(Texture);
		return Brush;
	}
}

void UImmortalCharacterPathEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BuildPathEntrySize"));
	Root->SetWidthOverride(230.0f);
	Root->SetHeightOverride(62.0f);
	WidgetTree->RootWidget = Root;
	EntryButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BuildPathEntryButton"));
	EntryButton->OnClicked.AddDynamic(this, &UImmortalCharacterPathEntryWidget::HandleClicked);
	Root->AddChild(EntryButton);
	EntryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BuildPathEntryText"));
	EntryText->SetJustification(ETextJustify::Center);
	EntryText->SetShadowOffset(FVector2D(1.0f));
	EntryText->SetShadowColorAndOpacity(FLinearColor::Black);
	FSlateFontInfo Font = EntryText->GetFont();
	Font.Size = 16;
	EntryText->SetFont(Font);
	EntryButton->AddChild(EntryText);
	RefreshAppearance();
}

void UImmortalCharacterPathEntryWidget::InitializeEntry(
	UImmortalCharacterBuildWidget* InOwner,
	const EImmortalCultivationPath InPath,
	const bool bCurrent,
	const bool bSelected)
{
	OwnerBuild = InOwner;
	Path = InPath;
	bCurrentPath = bCurrent;
	bSelectedEntry = bSelected;
	FImmortalCultivationPathDefinition Definition;
	if (UImmortalCharacterPathLibrary::GetCultivationPathDefinition(Path, Definition))
	{
		DisplayText = FText::FromString(FString::Printf(TEXT("%s  %s%s\n秘技·%s"),
			*Definition.IconGlyph.ToString(), *Definition.DisplayName.ToString(),
			bCurrentPath ? TEXT(" [当前]") : TEXT(""), *Definition.SkillName.ToString()));
		DisplayColor = UImmortalCharacterPathLibrary::GetCultivationPathColor(Path);
	}
	RefreshAppearance();
}

void UImmortalCharacterPathEntryWidget::RefreshAppearance()
{
	if (!EntryButton || !EntryText) return;
	const FLinearColor Tint = bSelectedEntry
		? FLinearColor(0.24f, 0.46f, 0.62f, 1.0f)
		: (bCurrentPath ? FLinearColor(0.16f, 0.48f, 0.34f, 1.0f) : FLinearColor(0.14f, 0.22f, 0.30f, 0.92f));
	const FSlateBrush Brush = MakeBuildEntryBrush(Tint);
	FButtonStyle Style;
	Style.SetNormal(Brush);
	Style.SetHovered(Brush);
	Style.SetPressed(Brush);
	Style.SetDisabled(Brush);
	EntryButton->SetStyle(Style);
	EntryText->SetText(DisplayText);
	EntryText->SetColorAndOpacity(FSlateColor(DisplayColor));
}

void UImmortalCharacterPathEntryWidget::HandleClicked()
{
	if (OwnerBuild.IsValid()) OwnerBuild->SelectPath(Path);
}
