// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalSaveExitWidget.h"

#include "../Characters/ImmortalPlayerCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "Styling/SlateTypes.h"

namespace
{
	void StyleText(UTextBlock* Text, const int32 Size, const FLinearColor& Color)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
	}

	FButtonStyle MakeButtonStyle(const FLinearColor& Color)
	{
		auto MakeBrush = [](const FLinearColor& Tint)
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.TintColor = FSlateColor(Tint);
			Brush.OutlineSettings.CornerRadii =
				FVector4(10.0f, 10.0f, 10.0f, 10.0f);
			return Brush;
		};

		FButtonStyle Style;
		Style.SetNormal(MakeBrush(Color));
		Style.SetHovered(MakeBrush(Color * 1.18f));
		Style.SetPressed(MakeBrush(Color * 0.78f));
		Style.SetDisabled(MakeBrush(FLinearColor(0.09f, 0.10f, 0.12f, 1.0f)));
		return Style;
	}

	UButton* MakeAction(
		UWidgetTree* Tree,
		const TCHAR* Name,
		const TCHAR* Label,
		const float Width,
		const FLinearColor& Color,
		UTextBlock*& OutLabel)
	{
		USizeBox* Size = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(Width);
		Size->SetHeightOverride(86.0f);

		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetStyle(MakeButtonStyle(Color));
		Size->AddChild(Button);

		OutLabel = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		OutLabel->SetText(FText::FromString(FString(Label)));
		OutLabel->SetJustification(ETextJustify::Center);
		StyleText(OutLabel, 30, FLinearColor(0.98f, 0.93f, 0.79f, 1.0f));
		Button->AddChild(OutLabel);
		return Button;
	}
}

void UImmortalSaveExitWidget::InitializeForPlayer(AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	ShowFailureMessage();
}

void UImmortalSaveExitWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UImmortalSaveExitWidget::BuildWidgetTree()
{
	// The root is transparent and does not capture hits outside the small panel.
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("SaveExitCanvas"));
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Canvas;

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("SaveExitFrame"));
	Frame->SetBrushColor(FLinearColor(0.52f, 0.41f, 0.24f, 0.98f));
	Frame->SetPadding(FMargin(4.0f));
	UCanvasPanelSlot* FrameSlot = Canvas->AddChildToCanvas(Frame);
	FrameSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	FrameSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	FrameSlot->SetAutoSize(true);

	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("SaveExitWidth"));
	// Author at desktop-strip DPI rather than enlarging a small rasterized
	// widget afterward. This keeps Chinese glyphs and button labels sharp.
	Width->SetWidthOverride(1800.0f);
	Frame->AddChild(Width);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("SaveExitPanel"));
	Panel->SetBrushColor(FLinearColor(0.025f, 0.037f, 0.052f, 0.98f));
	Panel->SetPadding(FMargin(44.0f, 24.0f));
	Width->AddChild(Panel);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("SaveExitContent"));
	Panel->AddChild(Content);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SaveExitTitle"));
	Title->SetText(FText::FromString(TEXT("退出前保存失败")));
	StyleText(Title, 42, FLinearColor(0.96f, 0.78f, 0.46f, 1.0f));
	Content->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

	UTextBlock* Explanation = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SaveExitExplanation"));
	Explanation->SetText(FText::FromString(TEXT(
		"刚才的退出保存未成功，游戏仍在运行。请检查磁盘空间或写入权限后重试。")));
	Explanation->SetAutoWrapText(true);
	StyleText(Explanation, 30, FLinearColor(0.88f, 0.91f, 0.92f, 1.0f));
	Content->AddChildToVerticalBox(Explanation)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SaveExitStatus"));
	StatusText->SetAutoWrapText(true);
	StyleText(StatusText, 28, FLinearColor(0.76f, 0.82f, 0.83f, 1.0f));
	Content->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));

	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("SaveExitActions"));
	Content->AddChildToVerticalBox(Actions)->SetHorizontalAlignment(HAlign_Right);

	UTextBlock* RetryLabel = nullptr;
	RetryButton = MakeAction(
		WidgetTree, TEXT("SaveExitRetryButton"), TEXT("重试保存并退出"),
		410.0f, FLinearColor(0.12f, 0.27f, 0.23f, 1.0f), RetryLabel);
	RetryButton->OnClicked.AddDynamic(this, &UImmortalSaveExitWidget::HandleRetryClicked);
	RetryButton->SetToolTipText(FText::FromString(TEXT("重新写入存档；只有成功后才退出。")));
	Actions->AddChildToHorizontalBox(RetryButton->GetParent())->SetPadding(
		FMargin(0.0f, 0.0f, 20.0f, 0.0f));

	UTextBlock* ContinueLabel = nullptr;
	ContinueButton = MakeAction(
		WidgetTree, TEXT("SaveExitContinueButton"), TEXT("继续游戏"),
		290.0f, FLinearColor(0.15f, 0.24f, 0.35f, 1.0f), ContinueLabel);
	ContinueButton->OnClicked.AddDynamic(this, &UImmortalSaveExitWidget::HandleContinueClicked);
	ContinueButton->SetToolTipText(FText::FromString(TEXT("关闭提示并继续游戏；后台自动保存可能继续发生。")));
	Actions->AddChildToHorizontalBox(ContinueButton->GetParent())->SetPadding(
		FMargin(0.0f, 0.0f, 20.0f, 0.0f));

	UTextBlock* ExitLabel = nullptr;
	ExitWithoutSavingButton = MakeAction(
		WidgetTree, TEXT("SaveExitWithoutSavingButton"), TEXT("跳过保存并退出"),
		480.0f, FLinearColor(0.42f, 0.17f, 0.13f, 1.0f), ExitLabel);
	ExitWithoutSavingLabel = ExitLabel;
	ExitWithoutSavingButton->OnClicked.AddDynamic(
		this, &UImmortalSaveExitWidget::HandleExitWithoutSavingClicked);
	ExitWithoutSavingButton->SetToolTipText(FText::FromString(
		TEXT("退出时不再尝试保存；最近未保存的进度可能丢失，已有自动存档仍保留。需要再次点击确认。")));
	Actions->AddChildToHorizontalBox(ExitWithoutSavingButton->GetParent());

	ShowFailureMessage();
}

void UImmortalSaveExitWidget::NativeConstruct()
{
	Super::NativeConstruct();
	FocusRetryAction();
}

void UImmortalSaveExitWidget::ActivateInput()
{
	FocusRetryAction();
}

void UImmortalSaveExitWidget::FocusRetryAction()
{
	APlayerController* Controller = GetOwningPlayer();
	if (!Controller && Player.IsValid())
	{
		Controller = Cast<APlayerController>(Player->GetController());
	}
	if (!Controller || !RetryButton) return;

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(RetryButton->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Controller->SetInputMode(InputMode);
	Controller->bShowMouseCursor = true;
	RetryButton->SetKeyboardFocus();
}

void UImmortalSaveExitWidget::SetStatusMessage(const FText& Message, const bool bIsError)
{
	if (!StatusText) return;
	StatusText->SetText(Message);
	StatusText->SetColorAndOpacity(FSlateColor(bIsError
		? FLinearColor(1.0f, 0.44f, 0.35f, 1.0f)
		: FLinearColor(0.76f, 0.82f, 0.83f, 1.0f)));
}

void UImmortalSaveExitWidget::DisarmExitWithoutSaving()
{
	bExitWithoutSavingArmed = false;
	if (ExitWithoutSavingLabel)
	{
		ExitWithoutSavingLabel->SetText(FText::FromString(TEXT("跳过保存并退出")));
	}
}

void UImmortalSaveExitWidget::ShowFailureMessage()
{
	DisarmExitWithoutSaving();
	SetStatusMessage(FText::FromString(TEXT(
		"窗口仍在；后台养成可能继续自动保存。可重试保存并退出，或继续游戏。")), true);
}

void UImmortalSaveExitWidget::HandleRetryClicked()
{
	DisarmExitWithoutSaving();
	if (!Player.IsValid())
	{
		SetStatusMessage(FText::FromString(TEXT("无法找到玩家，未执行保存或退出。")), true);
		return;
	}

	RetryButton->SetIsEnabled(false);
	SetStatusMessage(FText::FromString(TEXT("正在保存当前进度…")), false);
	const bool bSavedAndQuitting = Player->SaveAndQuitDesktop();
	if (!bSavedAndQuitting)
	{
		RetryButton->SetIsEnabled(true);
		ShowFailureMessage();
		RetryButton->SetKeyboardFocus();
	}
}

void UImmortalSaveExitWidget::HandleContinueClicked()
{
	DisarmExitWithoutSaving();
	if (Player.IsValid())
	{
		Player->DismissFailedSaveExitPrompt();
	}
	else
	{
		SetStatusMessage(FText::FromString(TEXT("无法恢复游戏操作；请保持游戏运行并检查存档。")), true);
	}
}

void UImmortalSaveExitWidget::HandleExitWithoutSavingClicked()
{
	if (!bExitWithoutSavingArmed)
	{
		bExitWithoutSavingArmed = true;
		ExitWithoutSavingLabel->SetText(FText::FromString(TEXT("确认跳过保存并退出")));
		SetStatusMessage(FText::FromString(TEXT(
			"退出时不再尝试保存，最近未保存的进度可能丢失；已有自动存档仍保留。请再次点击红色按钮确认。")), true);
		return;
	}

	if (!Player.IsValid())
	{
		DisarmExitWithoutSaving();
		SetStatusMessage(FText::FromString(TEXT("无法找到玩家，未执行退出。")), true);
		return;
	}

	Player->ExitWithoutSavingAfterFailure();
}
