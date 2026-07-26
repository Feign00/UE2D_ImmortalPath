// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalMapEntryWidget.h"

#include "ImmortalMapWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

namespace
{
	FSlateBrush MakeMapEntryBrush(const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = FVector2D(330.0f, 60.0f);
		Brush.TintColor = FSlateColor(Tint);
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal")))
		{
			Brush.SetResourceObject(Texture);
		}
		return Brush;
	}
}

void UImmortalMapEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MapEntrySize"));
	Root->SetWidthOverride(330.0f);
	Root->SetHeightOverride(60.0f);
	WidgetTree->RootWidget = Root;

	EntryButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MapEntryButton"));
	EntryButton->OnClicked.AddDynamic(this, &UImmortalMapEntryWidget::HandleClicked);
	Root->AddChild(EntryButton);

	EntryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MapEntryText"));
	EntryText->SetJustification(ETextJustify::Center);
	EntryText->SetAutoWrapText(true);
	EntryText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	EntryText->SetShadowColorAndOpacity(FLinearColor::Black);
	FSlateFontInfo Font = EntryText->GetFont();
	Font.Size = 17;
	EntryText->SetFont(Font);
	EntryButton->AddChild(EntryText);

	RefreshAppearance();
}

void UImmortalMapEntryWidget::InitializeMapEntry(
	UImmortalMapWidget* InOwner,
	const FImmortalMapDefinition& InDefinition,
	const FImmortalMapProgress& InProgress,
	const bool bInUnlocked,
	const bool bInActive,
	const bool bInSelected)
{
	OwnerMapWidget = InOwner;
	MapId = InDefinition.MapId;
	MapName = InDefinition.DisplayName;
	RealmRequirement = UImmortalMapLibrary::GetRealmRequirementText(InDefinition.RequiredRealmIndex);
	Progress = InProgress;
	AccentColor = InDefinition.SceneTint;
	MaximumStage = FMath::Max(InDefinition.MaximumStage, 1);
	bUnlocked = bInUnlocked;
	bActive = bInActive;
	bSelected = bInSelected;
	RefreshAppearance();
}

void UImmortalMapEntryWidget::RefreshAppearance()
{
	if (!EntryButton || !EntryText)
	{
		return;
	}

	const FString Prefix = bActive ? TEXT("[当前] ") : (bUnlocked ? TEXT("") : TEXT("[锁定] "));
	const FString ProgressLine = bUnlocked
		? (Progress.bCompleted
			? FString::Printf(TEXT("已通关 · %d / %d 关"), MaximumStage, MaximumStage)
			: FString::Printf(TEXT("第 %d / %d 关"), FMath::Clamp(Progress.Stage, 1, MaximumStage), MaximumStage))
		: FString::Printf(TEXT("需达到 %s境"), *RealmRequirement.ToString());
	EntryText->SetText(FText::FromString(FString::Printf(
		TEXT("%s%s\n%s"), *Prefix, *MapName.ToString(), *ProgressLine)));

	const FLinearColor NormalTint = !bUnlocked
		? FLinearColor(0.12f, 0.13f, 0.16f, 0.86f)
		: (bSelected
			? FLinearColor(0.18f, 0.38f, 0.48f, 0.98f)
			: (bActive
				? FLinearColor(0.48f, 0.34f, 0.10f, 0.98f)
				: FLinearColor(0.20f, 0.22f, 0.25f, 0.92f)));
	const FLinearColor HoverTint = bUnlocked
		? FLinearColor(0.25f, 0.50f, 0.58f, 1.0f)
		: FLinearColor(0.18f, 0.19f, 0.22f, 0.92f);
	FButtonStyle Style;
	Style.SetNormal(MakeMapEntryBrush(NormalTint));
	Style.SetHovered(MakeMapEntryBrush(HoverTint));
	Style.SetPressed(MakeMapEntryBrush(FLinearColor(0.60f, 0.44f, 0.15f, 1.0f)));
	EntryButton->SetStyle(Style);

	const FLinearColor TextColor = !bUnlocked
		? FLinearColor(0.48f, 0.50f, 0.55f, 1.0f)
		: (bActive ? FLinearColor(1.0f, 0.83f, 0.36f, 1.0f) : AccentColor.GetClamped(0.42f, 1.0f));
	EntryText->SetColorAndOpacity(FSlateColor(TextColor));
}

void UImmortalMapEntryWidget::HandleClicked()
{
	if (OwnerMapWidget.IsValid() && !MapId.IsNone())
	{
		OwnerMapWidget->SelectMap(MapId);
	}
}

