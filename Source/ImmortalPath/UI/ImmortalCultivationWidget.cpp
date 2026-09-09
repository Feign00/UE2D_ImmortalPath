// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalCultivationWidget.h"

#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Progression/ImmortalCultivationComponent.h"
#include "ImmortalManagementTypes.h"
#include "ImmortalFeaturePageLayout.h"
#include "ImmortalUITheme.h"
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
		ImmortalFeaturePageLayout::StabilizeButtonLabel(Button, Text);
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
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CultivationPageLogicalSize"));
	Root->SetWidthOverride(1600);
	Root->SetHeightOverride(600);
	WidgetTree->RootWidget = Root;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CultivationPageCanvas"));
	Root->AddChild(Canvas);

	const auto Panel = [this](UCanvasPanel* Parent, const TCHAR* Name, FVector2D Position, FVector2D Size)
	{
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Background->SetBrush(ImmortalUITheme::PanelBrush(FLinearColor(0.025f, 0.055f, 0.053f)));
		Background->SetPadding(FMargin(0));
		SetCultivationLayout(Parent->AddChildToCanvas(Background), Position, Size);
		UCanvasPanel* Body = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
			FName(*(FString(Name) + TEXT("Canvas"))));
		Background->AddChild(Body);
		return Body;
	};
	const auto Text = [this](UCanvasPanel* Parent, const TCHAR* Name, const TCHAR* Caption,
		FVector2D Position, FVector2D Size, int32 Font, bool bCentered = false)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetText(FText::FromString(Caption));
		StyleCultivationText(Label, Font, FLinearColor(0.93f, 0.89f, 0.76f), bCentered);
		Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		SetCultivationLayout(Parent->AddChildToCanvas(Label), Position, Size);
		return Label;
	};
	const auto Button = [this](UCanvasPanel* Parent, const TCHAR* Name, FVector2D Position, FVector2D Size)
	{
		UButton* Action = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Action->SetStyle(ImmortalUITheme::ButtonStyle());
		SetCultivationLayout(Parent->AddChildToCanvas(Action), Position, Size);
		return Action;
	};
	const auto Icon = [this](UCanvasPanel* Parent, const TCHAR* Name, int32 Index, FVector2D Position, float Size)
	{
		UImmortalIconWidget* Symbol = CreateWidget<UImmortalIconWidget>(this, UImmortalIconWidget::StaticClass(), FName(Name));
		Symbol->SetIcon(Index);
		Symbol->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetCultivationLayout(Parent->AddChildToCanvas(Symbol), Position, FVector2D(Size));
	};

	UCanvasPanel* Header = Panel(Canvas, TEXT("CultivationHeader"), {12, 4}, {1576, 52});
	Text(Header, TEXT("CultivationPageTitle"), TEXT("静修问道"), {20, 6}, {420, 40}, 28);
	UButton* Home = Button(Header, TEXT("CultivationReturnHome"), {1344, 6}, {160, 40});
	AddCultivationButtonLabel(WidgetTree, Home, TEXT("返回洞府"), 18);
	Home->OnClicked.AddDynamic(this, &ThisClass::HandleHomeClicked);
	UButton* Close = Button(Header, TEXT("CultivationCloseToHome"), {1516, 6}, {48, 40});
	AddCultivationButtonLabel(WidgetTree, Close, TEXT("×"), 24);
	Close->OnClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);

	UCanvasPanel* Meditation = Panel(Canvas, TEXT("CultivationMeditationPanel"), {12, 68}, {1008, 458});
	Icon(Meditation, TEXT("CultivationRealmEmblem"), 0, {40, 42}, 196);
	Text(Meditation, TEXT("CultivationRealmCaption"), TEXT("当前境界"), {280, 34}, {660, 32}, 20);
	RealmText = Text(Meditation, TEXT("CultivationRealm"), TEXT(""), {280, 82}, {688, 70}, 40);
	RateText = Text(Meditation, TEXT("CultivationRate"), TEXT(""), {280, 170}, {688, 40}, 23);
	Text(Meditation, TEXT("CultivationProgressCaption"), TEXT("修为积累 · 达标自动突破"),
		{28, 272}, {948, 32}, 20);
	ProgressText = Text(Meditation, TEXT("CultivationProgressText"), TEXT(""), {28, 326}, {948, 40}, 26);
	CultivationProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("CultivationProgressBar"));
	FProgressBarStyle BarStyle;
	BarStyle.SetBackgroundImage(ImmortalUITheme::PanelBrush(FLinearColor(0.015f, 0.025f, 0.025f)));
	BarStyle.SetFillImage(ImmortalUITheme::PanelBrush(FLinearColor::White, FLinearColor(0.55f, 0.75f, 0.46f)));
	CultivationProgressBar->SetWidgetStyle(BarStyle);
	CultivationProgressBar->SetFillColorAndOpacity(FLinearColor(0.38f, 0.72f, 0.49f));
	SetCultivationLayout(Meditation->AddChildToCanvas(CultivationProgressBar), {28, 386}, {948, 34});

	UCanvasPanel* Ascension = Panel(Canvas, TEXT("CultivationAscensionPanel"), {1032, 68}, {556, 458});
	Text(Ascension, TEXT("CultivationAscensionTitle"), TEXT("飞升之路"), {28, 18}, {500, 40}, 26, true);
	Icon(Ascension, TEXT("CultivationAscensionEmblem"), 0, {220, 74}, 116);
	AscensionHintText = Text(Ascension, TEXT("CultivationAscensionHint"), TEXT(""),
		{28, 210}, {500, 132}, 20);
	ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, AscensionHintText);
	AscensionButton = Button(Ascension, TEXT("CultivationOpenAscension"), {28, 374}, {500, 60});
	AscensionButton->OnClicked.AddDynamic(this, &ThisClass::HandleAscensionClicked);
	AscensionButtonText = AddCultivationButtonLabel(WidgetTree, AscensionButton, TEXT("查看飞升条件"), 22);

	UCanvasPanel* Footer = Panel(Canvas, TEXT("CultivationStatusPanel"), {12, 538}, {1576, 54});
	RuleText = Text(Footer, TEXT("CultivationIndependentRule"), TEXT(""),
		{16, 5}, {1544, 44}, 18);
	ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, RuleText);
	RefreshFromPlayer();
	ForceLayoutPrepass();
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
	for (const TCHAR* Name : { TEXT("CultivationReturnHome"), TEXT("CultivationCloseToHome") })
	{
		if (UButton* NavigationButton = Cast<UButton>(WidgetTree->FindWidget(FName(Name))))
			NavigationButton->SetIsEnabled(!bDeathRecoveryRequired);
	}

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
			bReachedAscension
				? TEXT("历练通道暂时关闭。当前已修至圆满，调息恢复后解锁。")
				: TEXT("历练通道暂时关闭。完成下一次境界突破后解锁；修炼继续自动进行。")));
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
