// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalSaveRecoveryWidget.h"

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
	void StyleRecoveryText(
		UTextBlock* Text,
		const int32 FontSize,
		const FLinearColor& Color)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
	}

	FButtonStyle MakeRecoveryButtonStyle(const FLinearColor& Color)
	{
		auto MakeBrush = [](const FLinearColor& Tint)
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.TintColor = FSlateColor(Tint);
			Brush.OutlineSettings.CornerRadii =
				FVector4(5.0f, 5.0f, 5.0f, 5.0f);
			return Brush;
		};

		FButtonStyle Style;
		Style.SetNormal(MakeBrush(Color));
		Style.SetHovered(MakeBrush(Color * 1.18f));
		Style.SetPressed(MakeBrush(Color * 0.78f));
		Style.SetDisabled(MakeBrush(FLinearColor(0.09f, 0.10f, 0.12f, 1.0f)));
		return Style;
	}

	UButton* MakeRecoveryButton(
		UWidgetTree* Tree,
		const TCHAR* Name,
		const TCHAR* Label,
		const float Width,
		const FLinearColor& Color)
	{
		USizeBox* Size = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetVisibility(ESlateVisibility::Visible);
		Size->SetWidthOverride(Width);
		Size->SetHeightOverride(48.0f);

		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetStyle(MakeRecoveryButtonStyle(Color));
		Size->AddChild(Button);

		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(FString(Label)));
		Text->SetJustification(ETextJustify::Center);
		StyleRecoveryText(Text, 17, FLinearColor(0.98f, 0.93f, 0.79f, 1.0f));
		Button->AddChild(Text);
		return Button;
	}
}

void UImmortalSaveRecoveryWidget::InitializeForPlayer(
	AImmortalPlayerCharacter* InPlayer,
	const bool bHasBackup,
	const bool bMainMissing,
	const bool bIncompatibleVersion)
{
	Player = InPlayer;
	bBackupAvailable = bHasBackup && !bIncompatibleVersion;
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(bIncompatibleVersion
			? TEXT("存档版本不兼容")
			: bMainMissing ? TEXT("主存档缺失") : TEXT("存档读取失败")));
	}
	if (ExplanationText)
	{
		ExplanationText->SetText(FText::FromString(bIncompatibleVersion
			? TEXT("存档或备份由更新版本的游戏创建。当前版本不会继续游玩或修改它；请升级游戏后再打开。")
			: bMainMissing
				? (bBackupAvailable
					? TEXT("主存档缺失，但发现上一份有效备份。为保护进度，本次不会建立新档；请先选择恢复或退出。")
					: TEXT("主存档缺失，检测到的备份文件也无法在当前版本恢复。为保护现有数据，本次不会建立新档；请退出并保留存档文件。"))
				: TEXT("检测到已有主存档，但文件无法读取。为保护现有数据，本次不会继续游戏，也不会自动保存或覆盖主存档。")));
	}

	if (RestoreButton)
	{
		// Collapse the size box too, so a missing backup leaves no empty action.
		RestoreButton->GetParent()->SetVisibility(
			bBackupAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (BackupExplanationText)
	{
		BackupExplanationText->SetText(FText::FromString(bIncompatibleVersion
			? TEXT("当前版本不会用旧备份替换新版本存档，也不会覆盖新版本备份。请退出并保留原文件。")
			: bBackupAvailable
				? TEXT("可选择“恢复上一份备份”来替换主存档并重新进入游戏；也可以直接退出。")
				: TEXT("没有找到可用备份。请退出游戏并保留存档文件，以便检查或手动恢复。")));
	}
	FocusRecoveryAction();
}

void UImmortalSaveRecoveryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildRecoveryWidgetTree();
}

void UImmortalSaveRecoveryWidget::BuildRecoveryWidgetTree()
{
	// The opaque root covers the entire viewport, including the old gameplay.
	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("SaveRecoveryBackdrop"));
	Backdrop->SetBrushColor(FLinearColor(0.012f, 0.018f, 0.027f, 1.0f));
	Backdrop->SetPadding(FMargin(0.0f));
	WidgetTree->RootWidget = Backdrop;

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("SaveRecoveryCanvas"));
	Backdrop->AddChild(Canvas);

	UBorder* PanelFrame = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("SaveRecoveryPanelFrame"));
	PanelFrame->SetBrushColor(FLinearColor(0.52f, 0.41f, 0.24f, 1.0f));
	PanelFrame->SetPadding(FMargin(2.0f));
	UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(PanelFrame);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetAutoSize(true);

	USizeBox* PanelWidth = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("SaveRecoveryPanelWidth"));
	PanelWidth->SetWidthOverride(980.0f);
	PanelFrame->AddChild(PanelWidth);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("SaveRecoveryPanel"));
	Panel->SetBrushColor(FLinearColor(0.025f, 0.037f, 0.052f, 1.0f));
	Panel->SetPadding(FMargin(24.0f, 16.0f));
	PanelWidth->AddChild(Panel);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("SaveRecoveryContent"));
	Panel->AddChild(Content);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SaveRecoveryTitle"));
	TitleText->SetText(FText::FromString(TEXT("存档读取失败")));
	StyleRecoveryText(TitleText, 24, FLinearColor(0.96f, 0.78f, 0.46f, 1.0f));
	Content->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));

	ExplanationText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SaveRecoveryExplanation"));
	ExplanationText->SetText(FText::FromString(TEXT(
		"检测到已有主存档，但文件无法读取。为保护现有数据，本次不会继续游戏，也不会自动保存或覆盖主存档。")));
	ExplanationText->SetAutoWrapText(true);
	StyleRecoveryText(ExplanationText, 16, FLinearColor(0.88f, 0.91f, 0.92f, 1.0f));
	Content->AddChildToVerticalBox(ExplanationText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

	BackupExplanationText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SaveRecoveryBackupExplanation"));
	BackupExplanationText->SetAutoWrapText(true);
	StyleRecoveryText(BackupExplanationText, 14,
		FLinearColor(0.70f, 0.83f, 0.93f, 1.0f));
	Content->AddChildToVerticalBox(BackupExplanationText)->SetPadding(
		FMargin(0.0f, 0.0f, 0.0f, 10.0f));

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SaveRecoveryStatus"));
	StatusText->SetAutoWrapText(true);
	StyleRecoveryText(StatusText, 14, FLinearColor(0.76f, 0.82f, 0.83f, 1.0f));
	Content->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	SetStatusMessage(FText::FromString(TEXT("请选择下方操作。此界面不会自动继续。")), false);

	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("SaveRecoveryActions"));
	Content->AddChildToVerticalBox(Actions)->SetHorizontalAlignment(HAlign_Right);

	RestoreButton = MakeRecoveryButton(
		WidgetTree, TEXT("SaveRecoveryRestoreButton"), TEXT("恢复上一份备份"),
		240.0f, FLinearColor(0.12f, 0.27f, 0.23f, 1.0f));
	RestoreButton->OnClicked.AddDynamic(
		this, &UImmortalSaveRecoveryWidget::HandleRestoreBackupClicked);
	RestoreButton->SetToolTipText(FText::FromString(TEXT("用上一份备份替换无法读取的主存档，然后重新进入游戏。")));
	RestoreButton->GetParent()->SetVisibility(ESlateVisibility::Collapsed);
	Actions->AddChildToHorizontalBox(RestoreButton->GetParent())->SetPadding(
		FMargin(0.0f, 0.0f, 14.0f, 0.0f));

	ExitButton = MakeRecoveryButton(
		WidgetTree, TEXT("SaveRecoveryExitButton"), TEXT("退出游戏"),
		170.0f, FLinearColor(0.42f, 0.17f, 0.13f, 1.0f));
	ExitButton->OnClicked.AddDynamic(
		this, &UImmortalSaveRecoveryWidget::HandleExitClicked);
	ExitButton->SetToolTipText(FText::FromString(TEXT("不保存当前状态，直接退出游戏。")));
	Actions->AddChildToHorizontalBox(ExitButton->GetParent());
}

void UImmortalSaveRecoveryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	FocusRecoveryAction();
}

void UImmortalSaveRecoveryWidget::ActivateInput()
{
	FocusRecoveryAction();
}

void UImmortalSaveRecoveryWidget::SetStatusMessage(
	const FText& Message,
	const bool bIsError)
{
	if (!StatusText) return;
	StatusText->SetText(Message);
	StatusText->SetColorAndOpacity(FSlateColor(bIsError
		? FLinearColor(1.0f, 0.44f, 0.35f, 1.0f)
		: FLinearColor(0.76f, 0.82f, 0.83f, 1.0f)));
}

void UImmortalSaveRecoveryWidget::FocusRecoveryAction()
{
	APlayerController* Controller = GetOwningPlayer();
	if (!Controller && Player.IsValid())
	{
		Controller = Cast<APlayerController>(Player->GetController());
	}
	if (!Controller) return;

	UButton* FirstAction = bBackupAvailable ? RestoreButton.Get() : ExitButton.Get();
	if (!FirstAction) return;

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(FirstAction->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Controller->SetInputMode(InputMode);
	Controller->bShowMouseCursor = true;
	FirstAction->SetKeyboardFocus();
}

void UImmortalSaveRecoveryWidget::HandleRestoreBackupClicked()
{
	if (!bBackupAvailable || !Player.IsValid())
	{
		SetStatusMessage(FText::FromString(TEXT("无法开始恢复备份。请退出游戏并保留存档文件。")), true);
		return;
	}

	RestoreButton->SetIsEnabled(false);
	SetStatusMessage(FText::FromString(TEXT("正在恢复备份并重新进入游戏…")), false);
	if (!Player->RestoreSaveBackupAndRestart())
	{
		RestoreButton->SetIsEnabled(true);
		SetStatusMessage(FText::FromString(TEXT(
			"恢复备份失败。游戏仍停留在此界面；请检查存档文件与磁盘状态，或退出后手动处理。")), true);
		RestoreButton->SetKeyboardFocus();
	}
}

void UImmortalSaveRecoveryWidget::HandleExitClicked()
{
	if (Player.IsValid())
	{
		Player->ExitWithoutSavingForRecovery();
	}
	else
	{
		SetStatusMessage(FText::FromString(TEXT(
			"无法执行退出操作。请从系统关闭游戏；不要继续写入存档文件。")), true);
	}
}
