// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalQuestEntryWidget.h"

#include "ImmortalQuestWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/SlateTypes.h"

namespace
{
	FSlateBrush MakeQuestRowBrush(const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.ImageSize = FVector2D(1210.0f, 43.0f);
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.CornerRadii = FVector4(4.0f);
		return Brush;
	}
}

void UImmortalQuestEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("QuestEntrySize"));
	Root->SetWidthOverride(1210.0f);
	Root->SetHeightOverride(45.0f);
	WidgetTree->RootWidget = Root;

	ClaimButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("QuestClaimButton"));
	ClaimButton->OnClicked.AddDynamic(
		this, &UImmortalQuestEntryWidget::HandleClaimClicked);
	Root->AddChild(ClaimButton);

	RowText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("QuestRowText"));
	RowText->SetAutoWrapText(false);
	RowText->SetJustification(ETextJustify::Left);
	RowText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	RowText->SetShadowColorAndOpacity(FLinearColor::Black);
	FSlateFontInfo Font = RowText->GetFont();
	Font.Size = 12;
	RowText->SetFont(Font);
	ClaimButton->AddChild(RowText);
	RefreshAppearance();
}

void UImmortalQuestEntryWidget::Configure(
	UImmortalQuestWidget* InOwner,
	const FImmortalQuestDefinition& InDefinition,
	const FImmortalQuestProgressView& InProgress)
{
	OwnerQuestWidget = InOwner;
	Definition = InDefinition;
	Progress = InProgress;
	RefreshAppearance();
}

void UImmortalQuestEntryWidget::RefreshAppearance()
{
	if (!ClaimButton || !RowText) return;
	FString StateText;
	FLinearColor Color;
	if (Progress.bClaimed)
	{
		StateText = TEXT("已领取");
		Color = FLinearColor(0.08f, 0.15f, 0.12f, 0.90f);
	}
	else if (!Progress.bUnlocked)
	{
		StateText = TEXT("未解锁");
		Color = FLinearColor(0.10f, 0.10f, 0.12f, 0.90f);
	}
	else if (Progress.bCanClaim)
	{
		StateText = TEXT("点击领取");
		Color = FLinearColor(0.38f, 0.25f, 0.07f, 0.98f);
	}
	else
	{
		StateText = TEXT("进行中");
		Color = FLinearColor(0.06f, 0.13f, 0.15f, 0.94f);
	}

	FButtonStyle Style;
	Style.SetNormal(MakeQuestRowBrush(Color));
	Style.SetHovered(MakeQuestRowBrush((Color * 1.20f).GetClamped()));
	Style.SetPressed(MakeQuestRowBrush((Color * 0.76f).GetClamped()));
	Style.SetDisabled(MakeQuestRowBrush(Color));
	ClaimButton->SetStyle(Style);
	ClaimButton->SetIsEnabled(Progress.bCanClaim);

	const FString Requirement = FString::Printf(
		TEXT("%s %lld/%lld"),
		*UImmortalQuestLibrary::GetMetricText(Definition.Metric).ToString(),
		Progress.Progress,
		Progress.Target);
	RowText->SetText(FText::FromString(FString::Printf(
		TEXT("  %-12s  %s  |  %s  |  奖励：%s  |  %s"),
		*Definition.DisplayName.ToString(),
		*Definition.Description.ToString(),
		*Requirement,
		*UImmortalQuestLibrary::FormatReward(Definition.Reward).ToString(),
		*StateText)));
	RowText->SetColorAndOpacity(FSlateColor(
		Progress.bClaimed || !Progress.bUnlocked
			? FLinearColor(0.56f, 0.60f, 0.60f, 1.0f)
			: FLinearColor(0.94f, 0.90f, 0.74f, 1.0f)));
}

void UImmortalQuestEntryWidget::HandleClaimClicked()
{
	if (OwnerQuestWidget.IsValid() && Progress.bCanClaim)
	{
		OwnerQuestWidget->ClaimQuest(Definition.QuestId);
	}
}

