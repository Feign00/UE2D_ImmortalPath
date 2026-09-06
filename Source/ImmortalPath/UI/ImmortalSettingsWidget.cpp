// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalSettingsWidget.h"

#include "../Characters/ImmortalPlayerCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/SlateTypes.h"

namespace
{
	void SetSettingsLayout(
		UCanvasPanelSlot* Slot,
		const FVector2D Position,
		const FVector2D Size)
	{
		if (!Slot) return;
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
	}

	void StyleSettingsText(
		UTextBlock* Text,
		const int32 Size,
		const FLinearColor& Color,
		const bool bCentered = false)
	{
		if (!Text) return;
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetJustification(
			bCentered ? ETextJustify::Center : ETextJustify::Left);
	}

	FButtonStyle MakeSettingsButtonStyle(
		const FVector2D Size,
		const FLinearColor& Color)
	{
		auto Brush = [Size](const FLinearColor& Tint)
		{
			FSlateBrush Result;
			Result.DrawAs = ESlateBrushDrawType::RoundedBox;
			Result.ImageSize = Size;
			Result.TintColor = FSlateColor(Tint);
			Result.OutlineSettings.CornerRadii =
				FVector4(5.0f, 5.0f, 5.0f, 5.0f);
			return Result;
		};
		FButtonStyle Style;
		Style.SetNormal(Brush(Color));
		Style.SetHovered(Brush(Color * 1.16f));
		Style.SetPressed(Brush(Color * 0.76f));
		Style.SetDisabled(Brush(
			FLinearColor(0.10f, 0.10f, 0.11f, 0.76f)));
		return Style;
	}

	UTextBlock* AddSettingsButtonLabel(
		UWidgetTree* Tree,
		UButton* Button,
		const FString& Label,
		const int32 FontSize = 16)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		Text->SetAutoWrapText(true);
		StyleSettingsText(
			Text,
			FontSize,
			FLinearColor(0.97f, 0.91f, 0.74f, 1.0f),
			true);
		Button->AddChild(Text);
		return Text;
	}

	UButton* AddSettingsButton(
		UWidgetTree* Tree,
		UCanvasPanel* Canvas,
		const TCHAR* Name,
		const FVector2D Position,
		const FVector2D Size,
		const FLinearColor& Color)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(
			UButton::StaticClass(), Name);
		Button->SetStyle(MakeSettingsButtonStyle(Size, Color));
		SetSettingsLayout(
			Canvas->AddChildToCanvas(Button),
			Position,
			Size);
		return Button;
	}
}

void UImmortalSettingsWidget::InitializeForPlayer(
	AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("DesktopSettingsSize"));
	RootSize->SetWidthOverride(1600.0f);
	RootSize->SetHeightOverride(300.0f);
	WidgetTree->RootWidget = RootSize;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("DesktopSettingsBackground"));
	Background->SetBrushColor(
		FLinearColor(0.016f, 0.022f, 0.032f, 0.995f));
	Background->SetPadding(FMargin(0.0f));
	RootSize->AddChild(Background);

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("DesktopSettingsCanvas"));
	Background->AddChild(Canvas);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("DesktopSettingsTitle"));
	Title->SetText(FText::FromString(TEXT("系统设置  [Esc]")));
	StyleSettingsText(
		Title,
		24,
		FLinearColor(0.92f, 0.78f, 0.48f, 1.0f));
	SetSettingsLayout(
		Canvas->AddChildToCanvas(Title),
		FVector2D(18.0f, 5.0f),
		FVector2D(230.0f, 34.0f));

	SummaryText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("DesktopSettingsSummary"));
	SummaryText->SetAutoWrapText(true);
	StyleSettingsText(
		SummaryText,
		13,
		FLinearColor(0.72f, 0.84f, 0.94f, 1.0f));
	SetSettingsLayout(
		Canvas->AddChildToCanvas(SummaryText),
		FVector2D(252.0f, 7.0f),
		FVector2D(1265.0f, 31.0f));

	UButton* CloseButton = AddSettingsButton(
		WidgetTree,
		Canvas,
		TEXT("DesktopSettingsClose"),
		FVector2D(1538.0f, 4.0f),
		FVector2D(46.0f, 32.0f),
		FLinearColor(0.43f, 0.14f, 0.12f, 1.0f));
	CloseButton->OnClicked.AddDynamic(
		this, &UImmortalSettingsWidget::HandleCloseClicked);
	AddSettingsButtonLabel(WidgetTree, CloseButton, TEXT("×"), 22);

	AlwaysOnTopText = AddSettingsButtonLabel(
		WidgetTree,
		AddSettingsButton(
			WidgetTree,
			Canvas,
			TEXT("AlwaysOnTopSetting"),
			FVector2D(22.0f, 62.0f),
			FVector2D(300.0f, 116.0f),
			FLinearColor(0.08f, 0.22f, 0.30f, 1.0f)),
		TEXT("窗口置顶"),
		17);
	CastChecked<UButton>(
		AlwaysOnTopText->GetParent())->OnClicked.AddDynamic(
			this, &UImmortalSettingsWidget::HandleAlwaysOnTopClicked);

	MuteText = AddSettingsButtonLabel(
		WidgetTree,
		AddSettingsButton(
			WidgetTree,
			Canvas,
			TEXT("MuteSetting"),
			FVector2D(340.0f, 62.0f),
			FVector2D(300.0f, 116.0f),
			FLinearColor(0.24f, 0.13f, 0.28f, 1.0f)),
		TEXT("声音"),
		17);
	CastChecked<UButton>(
		MuteText->GetParent())->OnClicked.AddDynamic(
			this, &UImmortalSettingsWidget::HandleMuteClicked);

	FrameRateText = AddSettingsButtonLabel(
		WidgetTree,
		AddSettingsButton(
			WidgetTree,
			Canvas,
			TEXT("FrameRateSetting"),
			FVector2D(658.0f, 62.0f),
			FVector2D(300.0f, 116.0f),
			FLinearColor(0.14f, 0.25f, 0.13f, 1.0f)),
		TEXT("帧率"),
		17);
	CastChecked<UButton>(
		FrameRateText->GetParent())->OnClicked.AddDynamic(
			this, &UImmortalSettingsWidget::HandleFrameRateClicked);

	UButton* MinimizeButton = AddSettingsButton(
		WidgetTree,
		Canvas,
		TEXT("DesktopMinimize"),
		FVector2D(976.0f, 62.0f),
		FVector2D(286.0f, 52.0f),
		FLinearColor(0.10f, 0.18f, 0.26f, 1.0f));
	MinimizeButton->OnClicked.AddDynamic(
		this, &UImmortalSettingsWidget::HandleMinimizeClicked);
	AddSettingsButtonLabel(
		WidgetTree, MinimizeButton, TEXT("最小化到任务栏"), 16);

	UButton* SaveQuitButton = AddSettingsButton(
		WidgetTree,
		Canvas,
		TEXT("DesktopSaveAndQuit"),
		FVector2D(1278.0f, 62.0f),
		FVector2D(300.0f, 116.0f),
		FLinearColor(0.38f, 0.18f, 0.08f, 1.0f));
	SaveQuitButton->OnClicked.AddDynamic(
		this, &UImmortalSettingsWidget::HandleSaveAndQuitClicked);
	AddSettingsButtonLabel(
		WidgetTree,
		SaveQuitButton,
		TEXT("保存并退出\n角色、地图与全部养成进度"),
		17);

	UButton* CloseLargeButton = AddSettingsButton(
		WidgetTree,
		Canvas,
		TEXT("DesktopSettingsReturn"),
		FVector2D(976.0f, 126.0f),
		FVector2D(286.0f, 52.0f),
		FLinearColor(0.18f, 0.18f, 0.20f, 1.0f));
	CloseLargeButton->OnClicked.AddDynamic(
		this, &UImmortalSettingsWidget::HandleCloseClicked);
	AddSettingsButtonLabel(
		WidgetTree, CloseLargeButton, TEXT("返回游戏"), 16);

	UButton* TransparencyButton = AddSettingsButton(WidgetTree, Canvas,
		TEXT("DesktopTransparency"), FVector2D(22.0f, 194.0f), FVector2D(300.0f, 64.0f),
		FLinearColor(0.13f, 0.27f, 0.24f, 1.0f));
	TransparencyButton->OnClicked.AddDynamic(this, &ThisClass::HandleTransparencyClicked);
	TransparencyText = AddSettingsButtonLabel(WidgetTree, TransparencyButton, TEXT("透明桌面"), 16);
	TransparencyButton->SetToolTipText(FText::FromString(TEXT("只隐藏历练背景，人物与按钮保持清晰。透明位置可点击桌面。仅独立运行可用。")));

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("DesktopSettingsResult"));
	ResultText->SetAutoWrapText(true);
	StyleSettingsText(
		ResultText,
		14,
		FLinearColor(0.70f, 0.80f, 0.88f, 1.0f),
		true);
	SetSettingsLayout(
		Canvas->AddChildToCanvas(ResultText),
		FVector2D(340.0f, 198.0f),
		FVector2D(1238.0f, 78.0f));
	ResultText->SetText(FText::FromString(
		TEXT("透明桌面只影响历练画面。打开管理界面仍可操作全部养成功能，后台战斗不暂停。设置不改写角色存档。")));

	RefreshFromPlayer();
}

void UImmortalSettingsWidget::RefreshFromPlayer()
{
	if (Player.IsValid() && TransparencyText)
		TransparencyText->SetText(FText::FromString(Player->IsDesktopTransparent()
			? TEXT("透明桌面  ● 开启") : TEXT("透明桌面  ○ 关闭")));
	if (!Player.IsValid() || !SummaryText)
	{
		return;
	}
	SummaryText->SetText(FText::FromString(FString::Printf(
		TEXT("桌面挂机模式 · %d px 高度 · DirectX 11 · 当前帧率上限 %d FPS"),
		Player->GetDesktopWindowHeight(),
		Player->GetDesktopFrameRateLimit())));
	if (AlwaysOnTopText)
	{
		AlwaysOnTopText->SetText(FText::FromString(FString::Printf(
			TEXT("窗口置顶\n%s\n\n点击切换"),
			Player->IsDesktopAlwaysOnTopEnabled()
				? TEXT("已开启")
				: TEXT("已关闭"))));
	}
	if (MuteText)
	{
		MuteText->SetText(FText::FromString(FString::Printf(
			TEXT("声音\n%s\n\n点击切换"),
			Player->IsDesktopMuted()
				? TEXT("已静音")
				: TEXT("正常播放"))));
	}
	if (FrameRateText)
	{
		FrameRateText->SetText(FText::FromString(FString::Printf(
			TEXT("帧率上限\n%d FPS\n\n30 省电 / 60 流畅"),
			Player->GetDesktopFrameRateLimit())));
	}
}

void UImmortalSettingsWidget::SetResultMessage(
	const FText& Message,
	const bool bSucceeded)
{
	if (!ResultText) return;
	ResultText->SetText(Message);
	ResultText->SetColorAndOpacity(FSlateColor(
		bSucceeded
			? FLinearColor(0.48f, 1.0f, 0.68f, 1.0f)
			: FLinearColor(1.0f, 0.42f, 0.32f, 1.0f)));
}

void UImmortalSettingsWidget::HandleAlwaysOnTopClicked()
{
	if (!Player.IsValid()) return;
	Player->ToggleDesktopAlwaysOnTop();
	RefreshFromPlayer();
	SetResultMessage(
		FText::FromString(TEXT("窗口置顶设置已保存。")),
		true);
}

void UImmortalSettingsWidget::HandleMuteClicked()
{
	if (!Player.IsValid()) return;
	Player->ToggleDesktopMute();
	RefreshFromPlayer();
	SetResultMessage(
		FText::FromString(
			Player->IsDesktopMuted()
				? TEXT("游戏已静音。")
				: TEXT("游戏声音已恢复。")),
		true);
}

void UImmortalSettingsWidget::HandleTransparencyClicked()
{
	if (!Player.IsValid()) return;
	const bool bSucceeded = Player->ToggleDesktopTransparency();
	RefreshFromPlayer();
	SetResultMessage(FText::FromString(bSucceeded
		? TEXT("桌面背景模式已切换并保存。")
		: TEXT("请在独立运行或打包版本中切换，编辑器窗口不会被修改。")), bSucceeded);
}

void UImmortalSettingsWidget::HandleFrameRateClicked()
{
	if (!Player.IsValid()) return;
	Player->CycleDesktopFrameRateLimit();
	RefreshFromPlayer();
	SetResultMessage(
		FText::FromString(FString::Printf(
			TEXT("帧率上限已设为 %d FPS。"),
			Player->GetDesktopFrameRateLimit())),
		true);
}

void UImmortalSettingsWidget::HandleMinimizeClicked()
{
	if (!Player.IsValid()) return;
	SetResultMessage(
		FText::FromString(
			TEXT("正在最小化；点击 Windows 任务栏图标即可恢复。")),
		true);
	if (!Player->MinimizeDesktopWindow())
	{
		SetResultMessage(
			FText::FromString(
				TEXT("编辑器预览不最小化宿主窗口，请在独立运行或打包版中使用。")),
			false);
	}
}

void UImmortalSettingsWidget::HandleSaveAndQuitClicked()
{
	if (!Player.IsValid()) return;
	if (!Player->SaveAndQuitDesktop())
	{
		SetResultMessage(
			FText::FromString(
				TEXT("保存失败，游戏没有退出；请检查磁盘空间或写入权限后重试。")),
			false);
	}
}

void UImmortalSettingsWidget::HandleCloseClicked()
{
	if (Player.IsValid())
	{
		Player->ToggleSettings();
	}
}
