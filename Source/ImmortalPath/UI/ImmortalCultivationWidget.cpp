// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalCultivationWidget.h"

#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Progression/ImmortalCultivationComponent.h"
#include "ImmortalManagementTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/SlateTypes.h"

namespace
{
	void SetCultivationLayout(
		UCanvasPanelSlot* Slot,
		const FVector2D Position,
		const FVector2D Size)
	{
		if (!Slot) return;
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetAutoSize(false);
	}

	void StyleCultivationText(
		UTextBlock* Text,
		const int32 FontSize,
		const FLinearColor& Color,
		const bool bCentered = false)
	{
		if (!Text) return;
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor::Black);
		Text->SetJustification(
			bCentered ? ETextJustify::Center : ETextJustify::Left);
	}

	FSlateBrush MakeCultivationBrush(
		const FVector2D Size,
		const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.CornerRadii =
			FVector4(6.0f, 6.0f, 6.0f, 6.0f);
		return Brush;
	}

	FButtonStyle MakeCultivationButtonStyle(
		const FVector2D Size,
		const FLinearColor& Color)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeCultivationBrush(Size, Color));
		Style.SetHovered(MakeCultivationBrush(
			Size, (Color * 1.18f).GetClamped()));
		Style.SetPressed(MakeCultivationBrush(
			Size, (Color * 0.78f).GetClamped()));
		Style.SetDisabled(MakeCultivationBrush(
			Size, FLinearColor(0.10f, 0.11f, 0.12f, 0.84f)));
		return Style;
	}

	UTextBlock* AddCultivationButtonLabel(
		UWidgetTree* Tree,
		UButton* Button,
		const TCHAR* Label,
		const int32 FontSize)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		StyleCultivationText(
			Text,
			FontSize,
			FLinearColor(1.0f, 0.92f, 0.67f, 1.0f),
			true);
		Button->AddChild(Text);
		return Text;
	}
}

void UImmortalCultivationWidget::InitializeForPlayer(
	AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalCultivationWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("CultivationPageLogicalSize"));
	RootSize->SetWidthOverride(1286.0f);
	RootSize->SetHeightOverride(238.0f);
	WidgetTree->RootWidget = RootSize;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("CultivationPageBackground"));
	Background->SetBrushColor(
		FLinearColor(0.018f, 0.045f, 0.046f, 0.76f));
	Background->SetPadding(FMargin(0.0f));
	RootSize->AddChild(Background);

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("CultivationPageCanvas"));
	Background->AddChild(Canvas);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CultivationPageTitle"));
	Title->SetText(FText::FromString(TEXT("静修问道")));
	StyleCultivationText(
		Title,
		20,
		FLinearColor(0.89f, 0.96f, 0.72f, 1.0f));
	SetCultivationLayout(
		Canvas->AddChildToCanvas(Title),
		FVector2D(18.0f, 4.0f),
		FVector2D(250.0f, 30.0f));

	UButton* HomeButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("CultivationReturnHome"));
	HomeButton->SetStyle(MakeCultivationButtonStyle(
		FVector2D(110.0f, 27.0f),
		FLinearColor(0.10f, 0.28f, 0.26f, 1.0f)));
	HomeButton->OnClicked.AddDynamic(
		this, &UImmortalCultivationWidget::HandleHomeClicked);
	AddCultivationButtonLabel(
		WidgetTree, HomeButton, TEXT("返回主页"), 11);
	SetCultivationLayout(
		Canvas->AddChildToCanvas(HomeButton),
		FVector2D(1112.0f, 4.0f),
		FVector2D(110.0f, 27.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("CultivationCloseToHome"));
	CloseButton->SetStyle(MakeCultivationButtonStyle(
		FVector2D(36.0f, 27.0f),
		FLinearColor(0.38f, 0.13f, 0.11f, 1.0f)));
	CloseButton->OnClicked.AddDynamic(
		this, &UImmortalCultivationWidget::HandleCloseClicked);
	AddCultivationButtonLabel(WidgetTree, CloseButton, TEXT("×"), 16);
	SetCultivationLayout(
		Canvas->AddChildToCanvas(CloseButton),
		FVector2D(1232.0f, 4.0f),
		FVector2D(36.0f, 27.0f));

	RuleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CultivationIndependentRule"));
	RuleText->SetAutoWrapText(true);
	RuleText->SetText(FText::FromString(
		TEXT("修炼与历练相互独立：战斗不产修为。"
			"无论正在查看哪个养成功能，角色都会持续自动修炼，历练地图也会继续自动刷怪。")));
	StyleCultivationText(
		RuleText,
		11,
		FLinearColor(0.72f, 0.91f, 0.82f, 1.0f),
		true);
	SetCultivationLayout(
		Canvas->AddChildToCanvas(RuleText),
		FVector2D(280.0f, 3.0f),
		FVector2D(820.0f, 31.0f));

	UBorder* MeditationPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("CultivationMeditationPanel"));
	MeditationPanel->SetBrushColor(
		FLinearColor(0.025f, 0.085f, 0.078f, 0.90f));
	MeditationPanel->SetPadding(FMargin(0.0f));
	SetCultivationLayout(
		Canvas->AddChildToCanvas(MeditationPanel),
		FVector2D(12.0f, 40.0f),
		FVector2D(1262.0f, 128.0f));

	UCanvasPanel* MeditationCanvas =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("CultivationMeditationCanvas"));
	MeditationPanel->AddChild(MeditationCanvas);

	RealmText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CultivationRealm"));
	StyleCultivationText(
		RealmText,
		25,
		FLinearColor(1.0f, 0.88f, 0.52f, 1.0f),
		true);
	SetCultivationLayout(
		MeditationCanvas->AddChildToCanvas(RealmText),
		FVector2D(18.0f, 10.0f),
		FVector2D(245.0f, 42.0f));

	ProgressText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CultivationProgressText"));
	StyleCultivationText(
		ProgressText,
		14,
		FLinearColor(0.86f, 0.95f, 0.90f, 1.0f),
		true);
	SetCultivationLayout(
		MeditationCanvas->AddChildToCanvas(ProgressText),
		FVector2D(282.0f, 10.0f),
		FVector2D(430.0f, 28.0f));

	CultivationProgressBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("CultivationProgressBar"));
	CultivationProgressBar->SetFillColorAndOpacity(
		FLinearColor(0.40f, 0.92f, 0.58f, 1.0f));
	SetCultivationLayout(
		MeditationCanvas->AddChildToCanvas(CultivationProgressBar),
		FVector2D(282.0f, 44.0f),
		FVector2D(520.0f, 24.0f));

	RateText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CultivationRate"));
	StyleCultivationText(
		RateText,
		14,
		FLinearColor(0.60f, 0.93f, 1.0f, 1.0f),
		true);
	SetCultivationLayout(
		MeditationCanvas->AddChildToCanvas(RateText),
		FVector2D(820.0f, 12.0f),
		FVector2D(420.0f, 28.0f));

	AscensionHintText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CultivationAscensionHint"));
	AscensionHintText->SetAutoWrapText(true);
	StyleCultivationText(
		AscensionHintText,
		11,
		FLinearColor(0.86f, 0.82f, 0.98f, 1.0f),
		true);
	SetCultivationLayout(
		MeditationCanvas->AddChildToCanvas(AscensionHintText),
		FVector2D(820.0f, 43.0f),
		FVector2D(420.0f, 65.0f));

	AscensionButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("CultivationOpenAscension"));
	AscensionButton->SetStyle(MakeCultivationButtonStyle(
		FVector2D(310.0f, 42.0f),
		FLinearColor(0.36f, 0.17f, 0.45f, 1.0f)));
	AscensionButton->OnClicked.AddDynamic(
		this, &UImmortalCultivationWidget::HandleAscensionClicked);
	AscensionButtonText = AddCultivationButtonLabel(
		WidgetTree, AscensionButton, TEXT("查看飞升台"), 14);
	SetCultivationLayout(
		Canvas->AddChildToCanvas(AscensionButton),
		FVector2D(488.0f, 184.0f),
		FVector2D(310.0f, 42.0f));

	RefreshFromPlayer();
}

void UImmortalCultivationWidget::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccumulator += FMath::Max(InDeltaTime, 0.0f);
	if (RefreshAccumulator >= 0.20f)
	{
		RefreshFromPlayer();
	}
}

void UImmortalCultivationWidget::RefreshFromPlayer()
{
	RefreshAccumulator = 0.0f;
	if (!Player.IsValid() || !RealmText) return;

	const UImmortalCultivationComponent* Cultivation =
		Player->GetCultivationComponent();
	if (!Cultivation)
	{
		RealmText->SetText(FText::FromString(TEXT("修炼系统未初始化")));
		return;
	}

	const int32 Current = FMath::Max(
		Cultivation->GetCurrentCultivation(), 0);
	const int32 Required = FMath::Max(
		Cultivation->GetRequiredCultivation(), 1);
	const bool bReachedAscension = Cultivation->HasReachedAscension();
	const bool bDeathRecoveryRequired =
		Player->IsDeathCultivationRecoveryRequired();

	if (RuleText)
	{
		RuleText->SetText(FText::FromString(
			bDeathRecoveryRequired
				? TEXT("历练失败：通道已关闭。独立修炼继续自动增长，完成下一次突破后解锁；飞升境圆满时以调息恢复。")
				: TEXT("修炼与历练相互独立：战斗不产修为。查看其他养成功能时，修炼与正常历练都会继续进行。")));
		RuleText->SetColorAndOpacity(FSlateColor(
			bDeathRecoveryRequired
				? FLinearColor(1.0f, 0.64f, 0.36f, 1.0f)
				: FLinearColor(0.72f, 0.91f, 0.82f, 1.0f)));
	}

	RealmText->SetText(Cultivation->GetFullRealmName());
	ProgressText->SetText(FText::FromString(
		bReachedAscension
			? TEXT("修为圆满 · 已抵达飞升境")
			: FString::Printf(
				TEXT("当前修为 %d / %d"), Current, Required)));
	CultivationProgressBar->SetPercent(
		bReachedAscension
			? 1.0f
			: FMath::Clamp(
				static_cast<float>(Current)
					/ static_cast<float>(Required),
				0.0f,
				1.0f));
	RateText->SetText(FText::FromString(FString::Printf(
		TEXT("自动修炼速度：%.2f 修为 / 秒"),
		Cultivation->GetCultivationPerSecond())));

	AscensionButton->SetIsEnabled(!bDeathRecoveryRequired);
	if (bDeathRecoveryRequired)
	{
		AscensionHintText->SetText(FText::FromString(
			TEXT("当前处于死亡后的强制修炼状态。返回历练与其他功能均已锁定；完成下一次境界突破后，按钮会恢复。")));
		AscensionButtonText->SetText(FText::FromString(
			TEXT("强制修炼中")));
	}
	else if (bReachedAscension)
	{
		AscensionHintText->SetText(FText::FromString(
			TEXT("修为条件已经圆满。进入独立飞升界面确认其他条件，"
				"完成飞升后播放飞升动画。")));
		AscensionButtonText->SetText(FText::FromString(
			TEXT("修为圆满 · 前往飞升")));
	}
	else
	{
		AscensionHintText->SetText(FText::FromString(
			TEXT("修为会自动增长；历练击杀不会增加修为。"
				"达到飞升境后，在独立飞升界面完成飞升。")));
		AscensionButtonText->SetText(FText::FromString(
			TEXT("查看飞升条件")));
	}
}

void UImmortalCultivationWidget::HandleAscensionClicked()
{
	if (Player.IsValid())
	{
		Player->OpenAscensionInterface();
	}
}

void UImmortalCultivationWidget::HandleHomeClicked()
{
	if (Player.IsValid())
	{
		Player->OpenManagementFeature(EImmortalManagementFeature::Home);
	}
}

void UImmortalCultivationWidget::HandleCloseClicked()
{
	if (Player.IsValid())
	{
		Player->OpenManagementFeature(EImmortalManagementFeature::Home);
	}
}
