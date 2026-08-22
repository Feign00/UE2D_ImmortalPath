// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalAscensionWidget.h"

#include "../Ascension/ImmortalAscensionTypes.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Maps/ImmortalMapTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformTime.h"
#include "Styling/SlateTypes.h"

namespace
{
	void SetAscensionLayout(
		UCanvasPanelSlot* Slot,
		const FVector2D Position,
		const FVector2D Size)
	{
		if (!Slot) return;
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
	}

	void StyleAscensionText(
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

	FButtonStyle MakeAscensionButtonStyle(
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
				FVector4(4.0f, 4.0f, 4.0f, 4.0f);
			return Result;
		};
		FButtonStyle Style;
		Style.SetNormal(Brush(Color));
		Style.SetHovered(Brush(Color * 1.18f));
		Style.SetPressed(Brush(Color * 0.78f));
		Style.SetDisabled(Brush(
			FLinearColor(0.11f, 0.11f, 0.12f, 0.72f)));
		return Style;
	}

	UTextBlock* AddAscensionButtonLabel(
		UWidgetTree* Tree,
		UButton* Button,
		const FString& Label,
		const int32 FontSize)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		Text->SetAutoWrapText(true);
		StyleAscensionText(
			Text,
			FontSize,
			FLinearColor(0.96f, 0.91f, 0.74f, 1.0f),
			true);
		Button->AddChild(Text);
		return Text;
	}

	UBorder* AddAscensionPanel(
		UWidgetTree* Tree,
		UCanvasPanel* Canvas,
		const TCHAR* Name,
		const FVector2D Position,
		const FVector2D Size,
		const FLinearColor& Color)
	{
		UBorder* Panel = Tree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), Name);
		Panel->SetBrushColor(Color);
		Panel->SetPadding(FMargin(10.0f, 7.0f));
		SetAscensionLayout(
			Canvas->AddChildToCanvas(Panel),
			Position,
			Size);
		return Panel;
	}

	FString GateGlyph(const bool bSatisfied)
	{
		return bSatisfied ? TEXT("✓") : TEXT("·");
	}
}

void UImmortalAscensionWidget::InitializeForPlayer(
	AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalAscensionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("AscensionScreenSize"));
	RootSize->SetWidthOverride(1600.0f);
	RootSize->SetHeightOverride(300.0f);
	WidgetTree->RootWidget = RootSize;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("AscensionScreenBackground"));
	Background->SetBrushColor(
		FLinearColor(0.016f, 0.022f, 0.040f, 0.994f));
	Background->SetPadding(FMargin(0.0f));
	RootSize->AddChild(Background);

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("AscensionScreenCanvas"));
	Background->AddChild(Canvas);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("AscensionScreenTitle"));
	Title->SetText(FText::FromString(TEXT("羽化飞升  [U]")));
	StyleAscensionText(
		Title, 23, FLinearColor(0.90f, 0.78f, 1.0f, 1.0f));
	SetAscensionLayout(
		Canvas->AddChildToCanvas(Title),
		FVector2D(16.0f, 2.0f),
		FVector2D(220.0f, 36.0f));

	HeaderSummaryText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("AscensionHeaderSummary"));
	StyleAscensionText(
		HeaderSummaryText,
		13,
		FLinearColor(0.78f, 0.88f, 1.0f, 1.0f));
	SetAscensionLayout(
		Canvas->AddChildToCanvas(HeaderSummaryText),
		FVector2D(236.0f, 3.0f),
		FVector2D(1294.0f, 34.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("AscensionScreenClose"));
	CloseButton->SetStyle(MakeAscensionButtonStyle(
		FVector2D(46.0f, 32.0f),
		FLinearColor(0.43f, 0.14f, 0.12f, 1.0f)));
	CloseButton->OnClicked.AddDynamic(
		this, &UImmortalAscensionWidget::HandleCloseClicked);
	SetAscensionLayout(
		Canvas->AddChildToCanvas(CloseButton),
		FVector2D(1538.0f, 3.0f),
		FVector2D(46.0f, 32.0f));
	AddAscensionButtonLabel(
		WidgetTree, CloseButton, TEXT("×"), 22);

	UBorder* GatePanel = AddAscensionPanel(
		WidgetTree,
		Canvas,
		TEXT("AscensionGatePanel"),
		FVector2D(14.0f, 44.0f),
		FVector2D(415.0f, 243.0f),
		FLinearColor(0.030f, 0.060f, 0.078f, 0.98f));
	EligibilityText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("AscensionEligibility"));
	EligibilityText->SetAutoWrapText(true);
	StyleAscensionText(
		EligibilityText,
		14,
		FLinearColor(0.72f, 0.94f, 1.0f, 1.0f));
	GatePanel->AddChild(EligibilityText);

	UBorder* BonusPanel = AddAscensionPanel(
		WidgetTree,
		Canvas,
		TEXT("AscensionBonusPanel"),
		FVector2D(439.0f, 44.0f),
		FVector2D(360.0f, 243.0f),
		FLinearColor(0.052f, 0.042f, 0.082f, 0.98f));
	BonusText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("AscensionBonusSummary"));
	BonusText->SetAutoWrapText(true);
	StyleAscensionText(
		BonusText,
		14,
		FLinearColor(0.90f, 0.82f, 1.0f, 1.0f));
	BonusPanel->AddChild(BonusText);

	UBorder* PathPanel = AddAscensionPanel(
		WidgetTree,
		Canvas,
		TEXT("AscensionPathPanel"),
		FVector2D(809.0f, 44.0f),
		FVector2D(480.0f, 243.0f),
		FLinearColor(0.045f, 0.066f, 0.050f, 0.98f));
	PathPanel->SetPadding(FMargin(0.0f));
	UCanvasPanel* PathCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("AscensionPathCanvas"));
	PathPanel->AddChild(PathCanvas);
	UTextBlock* PathTitle = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("AscensionPathTitle"));
	PathTitle->SetText(FText::FromString(
		TEXT("仙途加点 · 阶位越高，下一阶消耗越多")));
	StyleAscensionText(
		PathTitle, 14, FLinearColor(0.70f, 1.0f, 0.78f, 1.0f));
	SetAscensionLayout(
		PathCanvas->AddChildToCanvas(PathTitle),
		FVector2D(10.0f, 5.0f),
		FVector2D(460.0f, 28.0f));

	BattlePathButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("AscensionBattlePathButton"));
	BattlePathButton->SetStyle(MakeAscensionButtonStyle(
		FVector2D(145.0f, 194.0f),
		FLinearColor(0.38f, 0.11f, 0.09f, 1.0f)));
	BattlePathButton->OnClicked.AddDynamic(
		this, &UImmortalAscensionWidget::HandleBattlePathClicked);
	SetAscensionLayout(
		PathCanvas->AddChildToCanvas(BattlePathButton),
		FVector2D(10.0f, 38.0f),
		FVector2D(145.0f, 194.0f));
	BattlePathText = AddAscensionButtonLabel(
		WidgetTree,
		BattlePathButton,
		TEXT("战道"),
		14);

	EnlightenmentPathButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("AscensionEnlightenmentPathButton"));
	EnlightenmentPathButton->SetStyle(MakeAscensionButtonStyle(
		FVector2D(145.0f, 194.0f),
		FLinearColor(0.12f, 0.25f, 0.38f, 1.0f)));
	EnlightenmentPathButton->OnClicked.AddDynamic(
		this,
		&UImmortalAscensionWidget::HandleEnlightenmentPathClicked);
	SetAscensionLayout(
		PathCanvas->AddChildToCanvas(EnlightenmentPathButton),
		FVector2D(167.0f, 38.0f),
		FVector2D(145.0f, 194.0f));
	EnlightenmentPathText = AddAscensionButtonLabel(
		WidgetTree,
		EnlightenmentPathButton,
		TEXT("悟道"),
		14);

	FortunePathButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("AscensionFortunePathButton"));
	FortunePathButton->SetStyle(MakeAscensionButtonStyle(
		FVector2D(145.0f, 194.0f),
		FLinearColor(0.38f, 0.29f, 0.08f, 1.0f)));
	FortunePathButton->OnClicked.AddDynamic(
		this, &UImmortalAscensionWidget::HandleFortunePathClicked);
	SetAscensionLayout(
		PathCanvas->AddChildToCanvas(FortunePathButton),
		FVector2D(324.0f, 38.0f),
		FVector2D(145.0f, 194.0f));
	FortunePathText = AddAscensionButtonLabel(
		WidgetTree,
		FortunePathButton,
		TEXT("福缘"),
		14);

	UBorder* ActionPanel = AddAscensionPanel(
		WidgetTree,
		Canvas,
		TEXT("AscensionActionPanel"),
		FVector2D(1299.0f, 44.0f),
		FVector2D(287.0f, 243.0f),
		FLinearColor(0.060f, 0.050f, 0.036f, 0.99f));
	ActionPanel->SetPadding(FMargin(0.0f));
	UCanvasPanel* ActionCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("AscensionActionCanvas"));
	ActionPanel->AddChild(ActionCanvas);

	UTextBlock* ActionTitle = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("AscensionActionTitle"));
	ActionTitle->SetText(FText::FromString(TEXT("飞升台")));
	StyleAscensionText(
		ActionTitle, 17, FLinearColor(1.0f, 0.86f, 0.58f, 1.0f));
	SetAscensionLayout(
		ActionCanvas->AddChildToCanvas(ActionTitle),
		FVector2D(10.0f, 5.0f),
		FVector2D(267.0f, 29.0f));

	ReturnQingyunButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("AscensionReturnQingyunButton"));
	ReturnQingyunButton->SetStyle(MakeAscensionButtonStyle(
		FVector2D(265.0f, 37.0f),
		FLinearColor(0.10f, 0.26f, 0.31f, 1.0f)));
	ReturnQingyunButton->OnClicked.AddDynamic(
		this, &UImmortalAscensionWidget::HandleReturnQingyunClicked);
	SetAscensionLayout(
		ActionCanvas->AddChildToCanvas(ReturnQingyunButton),
		FVector2D(11.0f, 39.0f),
		FVector2D(265.0f, 37.0f));
	AddAscensionButtonLabel(
		WidgetTree,
		ReturnQingyunButton,
		TEXT("返回青云山"),
		14);

	AscendButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("AscensionPerformButton"));
	AscendButton->SetStyle(MakeAscensionButtonStyle(
		FVector2D(265.0f, 58.0f),
		FLinearColor(0.38f, 0.16f, 0.42f, 1.0f)));
	AscendButton->OnClicked.AddDynamic(
		this, &UImmortalAscensionWidget::HandleAscendClicked);
	SetAscensionLayout(
		ActionCanvas->AddChildToCanvas(AscendButton),
		FVector2D(11.0f, 84.0f),
		FVector2D(265.0f, 58.0f));
	AscendButtonText = AddAscensionButtonLabel(
		WidgetTree,
		AscendButton,
		TEXT("羽化飞升"),
		17);

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("AscensionOperationResult"));
	ResultText->SetAutoWrapText(true);
	StyleAscensionText(
		ResultText,
		11,
		FLinearColor(0.80f, 0.84f, 0.88f, 1.0f),
		true);
	SetAscensionLayout(
		ActionCanvas->AddChildToCanvas(ResultText),
		FVector2D(11.0f, 150.0f),
		FVector2D(265.0f, 82.0f));

	// This top-level overlay owns the visual sequence. The combat character is
	// deliberately untouched so the automatic encounter keeps running behind
	// the opaque ascension page.
	AscensionSequenceOverlay = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("AscensionSequenceOverlay"));
	AscensionSequenceOverlay->SetBrushColor(
		FLinearColor(0.018f, 0.008f, 0.040f, 0.995f));
	AscensionSequenceOverlay->SetPadding(FMargin(0.0f));
	if (UCanvasPanelSlot* OverlaySlot =
		Canvas->AddChildToCanvas(AscensionSequenceOverlay))
	{
		OverlaySlot->SetPosition(FVector2D::ZeroVector);
		OverlaySlot->SetSize(FVector2D(1600.0f, 300.0f));
		OverlaySlot->SetZOrder(100);
	}
	UCanvasPanel* SequenceCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("AscensionSequenceCanvas"));
	AscensionSequenceOverlay->AddChild(SequenceCanvas);

	AscensionSequenceImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("AscensionSequenceImage"));
	SetAscensionLayout(
		SequenceCanvas->AddChildToCanvas(AscensionSequenceImage),
		FVector2D(750.0f, 2.0f),
		FVector2D(100.0f, 282.0f));

	AscensionSequenceCaption = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("AscensionSequenceCaption"));
	AscensionSequenceCaption->SetText(FText::FromString(
		TEXT("\u7FBD\u5316\u98DE\u5347\u4E2D")));
	StyleAscensionText(
		AscensionSequenceCaption,
		22,
		FLinearColor(0.92f, 0.78f, 1.0f, 1.0f),
		true);
	SetAscensionLayout(
		SequenceCanvas->AddChildToCanvas(AscensionSequenceCaption),
		FVector2D(560.0f, 254.0f),
		FVector2D(480.0f, 38.0f));
	AscensionSequenceOverlay->SetVisibility(
		ESlateVisibility::Collapsed);
	ResetResultMessage();
	RefreshFromPlayer();
}

void UImmortalAscensionWidget::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bAscensionSequencePlaying)
	{
		constexpr float FramesPerSecond = 12.0f;
		constexpr int32 FrameCount = 17;
		AscensionSequenceElapsedSeconds +=
			FMath::Max(InDeltaTime, 0.0f);
		const int32 FrameIndex = FMath::FloorToInt(
			AscensionSequenceElapsedSeconds * FramesPerSecond);
		if (FrameIndex >= FrameCount)
		{
			if (AscensionSequenceFrame != FrameCount - 1)
			{
				AscensionSequenceFrame = FrameCount - 1;
				UpdateAscensionSequenceFrame(AscensionSequenceFrame);
				if (AscensionSequenceCaption)
				{
					AscensionSequenceCaption->SetText(FText::FromString(
						TEXT("\u98DE\u5347\u5B8C\u6210\u00B7\u65B0\u8F6E\u56DE\u5DF2\u5F00\u59CB")));
				}
			}
			constexpr float CompletionHoldSeconds = 1.2f;
			if (AscensionSequenceElapsedSeconds
				>= static_cast<float>(FrameCount) / FramesPerSecond
					+ CompletionHoldSeconds)
			{
				bAscensionSequencePlaying = false;
				AscensionSequenceOverlay->SetVisibility(
					ESlateVisibility::Collapsed);
			}
		}
		else if (FrameIndex != AscensionSequenceFrame)
		{
			AscensionSequenceFrame = FrameIndex;
			UpdateAscensionSequenceFrame(AscensionSequenceFrame);
		}
	}
	if (ResultMessageExpirySeconds > 0.0
		&& FPlatformTime::Seconds()
			>= ResultMessageExpirySeconds)
	{
		ResetResultMessage();
	}
	if (!Player.IsValid()) return;
	RefreshAccumulator += FMath::Max(InDeltaTime, 0.0f);
	if (Player->GetAscensionRevision() != LastRevision
		|| RefreshAccumulator >= 0.25f)
	{
		RefreshFromPlayer();
	}
}

void UImmortalAscensionWidget::PlayAscensionSequence()
{
	if (!AscensionSequenceTexture)
	{
		AscensionSequenceTexture = LoadObject<UTexture2D>(
			nullptr,
			TEXT("/Game/GAME/Asset/Player/ascension/generated/T_Player_Ascension.T_Player_Ascension"));
	}
	if (!AscensionSequenceTexture || !AscensionSequenceOverlay
		|| !AscensionSequenceImage)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Ascension UI sequence unavailable: texture=%s overlay=%s image=%s"),
			AscensionSequenceTexture ? TEXT("true") : TEXT("false"),
			AscensionSequenceOverlay ? TEXT("true") : TEXT("false"),
			AscensionSequenceImage ? TEXT("true") : TEXT("false"));
		return;
	}

	AscensionSequenceElapsedSeconds = 0.0f;
	AscensionSequenceFrame = 0;
	bAscensionSequencePlaying = true;
	if (AscensionSequenceCaption)
	{
		AscensionSequenceCaption->SetText(FText::FromString(
			TEXT("\u7FBD\u5316\u98DE\u5347\u4E2D")));
	}
	UpdateAscensionSequenceFrame(0);
	AscensionSequenceOverlay->SetVisibility(ESlateVisibility::Visible);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Independent ascension UI sequence started: texture=%s frames=17 fps=12 combatUnaffected=true"),
		*AscensionSequenceTexture->GetPathName());
}

void UImmortalAscensionWidget::UpdateAscensionSequenceFrame(
	const int32 FrameIndex)
{
	if (!AscensionSequenceTexture || !AscensionSequenceImage)
	{
		return;
	}
	constexpr int32 FrameCount = 17;
	const int32 SafeFrame = FMath::Clamp(FrameIndex, 0, FrameCount - 1);
	const float Left = static_cast<float>(SafeFrame)
		/ static_cast<float>(FrameCount);
	const float Right = static_cast<float>(SafeFrame + 1)
		/ static_cast<float>(FrameCount);
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	// The source sheet reserves substantial transparent space above and below
	// every pose. Crop only that shared safe area so the character remains
	// readable in a 300px TBH page without distorting its proportions.
	constexpr float SourceHeight = 724.0f;
	constexpr float CropTop = 190.0f / SourceHeight;
	constexpr float CropBottom = 620.0f / SourceHeight;
	Brush.ImageSize = FVector2D(128.0f, 430.0f);
	Brush.TintColor = FSlateColor(FLinearColor::White);
	Brush.SetResourceObject(AscensionSequenceTexture);
	Brush.SetUVRegion(FBox2f(
		FVector2f(Left, CropTop),
		FVector2f(Right, CropBottom)));
	AscensionSequenceImage->SetBrush(Brush);
}

void UImmortalAscensionWidget::RefreshFromPlayer()
{
	if (!Player.IsValid() || !HeaderSummaryText)
	{
		return;
	}
	const FImmortalAscensionState State =
		Player->GetAscensionState();
	const FImmortalAscensionEligibility Eligibility =
		Player->EvaluateAscensionEligibility();
	const int32 BattleCost =
		UImmortalAscensionLibrary::CalculatePathUpgradeCost(
			State.BattlePathRank);
	const int32 EnlightenmentCost =
		UImmortalAscensionLibrary::CalculatePathUpgradeCost(
			State.EnlightenmentPathRank);
	const int32 FortuneCost =
		UImmortalAscensionLibrary::CalculatePathUpgradeCost(
			State.FortunePathRank);
	LastRevision = State.Revision;
	RefreshAccumulator = 0.0f;

	HeaderSummaryText->SetText(FText::FromString(FString::Printf(
		TEXT("第 %d 次轮回  ·  仙印 %d  ·  %s  ·  本轮关卡重开，历世记录永久保留"),
		State.AscensionCount,
		State.ImmortalSeals,
		*Eligibility.Message.ToString())));

	EligibilityText->SetText(FText::FromString(FString::Printf(
		TEXT("飞升条件\n"
			"%s 已修至飞升境界\n"
			"%s 仙宫遗址第 999 关已通关\n"
			"%s 当前位于青云山\n"
			"%s 世界妖王与无尽秘境均空闲\n"
			"%s 尚未达到 999 次飞升上限\n\n"
			"重置：境界、层级、当前修为；本轮八图回第 1 关，地面掉落清除。\n"
			"保留：装备、背包、灵石、历世地图记录及其他系统。"),
		*GateGlyph(Eligibility.bReachedAscensionRealm),
		*GateGlyph(Eligibility.bImmortalPalaceCompleted),
		*GateGlyph(Eligibility.bAtQingyunMountain),
		*GateGlyph(Eligibility.bIndependentEncountersIdle),
		*GateGlyph(Eligibility.bBelowAscensionLimit))));
	EligibilityText->SetColorAndOpacity(FSlateColor(
		Eligibility.bEligible
			? FLinearColor(0.55f, 1.0f, 0.72f, 1.0f)
			: FLinearColor(0.72f, 0.88f, 0.96f, 1.0f)));

	BonusText->SetText(FText::FromString(FString::Printf(
		TEXT("永久道果\n"
			"飞升：%d / %d 次\n"
			"仙印：%d（累计获得 %lld）\n\n"
			"历世地图通关：%d / %d\n"
			"战道 %d/50  ·  全部攻击 ×%.2f\n"
			"悟道 %d/50  ·  修炼速度 ×%.2f\n"
			"福缘 %d/50  ·  装备掉落 ×%.2f\n\n"
			"%s"),
		State.AscensionCount,
		UImmortalAscensionLibrary::MaximumAscensionCount,
		State.ImmortalSeals,
		State.TotalImmortalSealsEarned,
		UImmortalAscensionLibrary
			::GetLifetimeCompletedMapCount(State),
		UImmortalMapLibrary::GetKnownMapIds().Num(),
		State.BattlePathRank,
		Player->GetAscensionBattleMultiplier(),
		State.EnlightenmentPathRank,
		Player->GetAscensionCultivationMultiplier(),
		State.FortunePathRank,
		Player->GetAscensionEquipmentDropMultiplier(),
		Eligibility.bBelowAscensionLimit
			? *FString::Printf(
				TEXT("下次飞升奖励：%d 枚仙印"),
				Eligibility.RewardImmortalSeals)
			: TEXT("飞升次数已达到上限"))));

	if (BattlePathText)
	{
		BattlePathText->SetText(FText::FromString(FString::Printf(
			TEXT("战道\n%d / 50 阶\n\n全部攻击\n+4%% / 阶\n\n当前 ×%.2f\n%s"),
			State.BattlePathRank,
			Player->GetAscensionBattleMultiplier(),
			BattleCost > 0
				? *FString::Printf(
					TEXT("下一阶 %d 印"), BattleCost)
				: TEXT("已满阶"))));
	}
	if (EnlightenmentPathText)
	{
		EnlightenmentPathText->SetText(FText::FromString(FString::Printf(
			TEXT("悟道\n%d / 50 阶\n\n修炼速度\n+6%% / 阶\n\n当前 ×%.2f\n%s"),
			State.EnlightenmentPathRank,
			Player->GetAscensionCultivationMultiplier(),
			EnlightenmentCost > 0
				? *FString::Printf(
					TEXT("下一阶 %d 印"), EnlightenmentCost)
				: TEXT("已满阶"))));
	}
	if (FortunePathText)
	{
		FortunePathText->SetText(FText::FromString(FString::Printf(
			TEXT("福缘\n%d / 50 阶\n\n装备掉落\n+3%% / 阶\n\n当前 ×%.2f\n%s"),
			State.FortunePathRank,
			Player->GetAscensionEquipmentDropMultiplier(),
			FortuneCost > 0
				? *FString::Printf(
					TEXT("下一阶 %d 印"), FortuneCost)
				: TEXT("已满阶"))));
	}

	if (BattlePathButton)
	{
		BattlePathButton->SetIsEnabled(
			BattleCost > 0
			&& State.ImmortalSeals >= BattleCost);
	}
	if (EnlightenmentPathButton)
	{
		EnlightenmentPathButton->SetIsEnabled(
			EnlightenmentCost > 0
			&& State.ImmortalSeals >= EnlightenmentCost);
	}
	if (FortunePathButton)
	{
		FortunePathButton->SetIsEnabled(
			FortuneCost > 0
			&& State.ImmortalSeals >= FortuneCost);
	}
	if (ReturnQingyunButton)
	{
		ReturnQingyunButton->SetIsEnabled(
			!Eligibility.bAtQingyunMountain
			&& Eligibility.bIndependentEncountersIdle);
	}
	if (AscendButton)
	{
		AscendButton->SetIsEnabled(Eligibility.bEligible);
	}
	if (AscendButtonText)
	{
		AscendButtonText->SetText(FText::FromString(
			Eligibility.bEligible
				? FString::Printf(
					TEXT("羽化飞升\n获得 %d 枚仙印"),
					Eligibility.RewardImmortalSeals)
				: TEXT("尚未满足飞升条件")));
	}
}

void UImmortalAscensionWidget::SetResultMessage(
	const FText& Message,
	const bool bSucceeded)
{
	if (!ResultText) return;
	ResultText->SetText(Message);
	ResultText->SetColorAndOpacity(FSlateColor(
		bSucceeded
			? FLinearColor(0.48f, 1.0f, 0.68f, 1.0f)
			: FLinearColor(1.0f, 0.42f, 0.32f, 1.0f)));
	ResultMessageExpirySeconds =
		FPlatformTime::Seconds() + 5.0;
}

void UImmortalAscensionWidget::ResetResultMessage()
{
	ResultMessageExpirySeconds = 0.0;
	if (!ResultText) return;
	ResultText->SetText(FText::FromString(
		TEXT("满足五项条件即可飞升；金丹后可用破境丹加速高境界突破。")));
	ResultText->SetColorAndOpacity(FSlateColor(
		FLinearColor(0.80f, 0.84f, 0.88f, 1.0f)));
}

void UImmortalAscensionWidget::HandleCloseClicked()
{
	if (Player.IsValid())
	{
		Player->ToggleAscension();
	}
}

void UImmortalAscensionWidget::HandleReturnQingyunClicked()
{
	if (!Player.IsValid()) return;
	const FImmortalMapTravelResult Result = Player->TravelToMap(
		UImmortalMapLibrary::GetQingyunMountainId());
	SetResultMessage(Result.Message, Result.bSucceeded);
	RefreshFromPlayer();
}

void UImmortalAscensionWidget::HandleAscendClicked()
{
	if (!Player.IsValid()) return;
	const FImmortalAscensionOperationResult Result =
		Player->PerformAscension();
	SetResultMessage(Result.Message, Result.bSucceeded);
	RefreshFromPlayer();
}

void UImmortalAscensionWidget::InvestPath(const uint8 PathValue)
{
	if (!Player.IsValid()) return;
	const FImmortalAscensionPathResult Result =
		Player->InvestAscensionPath(
			static_cast<EImmortalAscensionPath>(PathValue));
	SetResultMessage(Result.Message, Result.bSucceeded);
	RefreshFromPlayer();
}

void UImmortalAscensionWidget::HandleBattlePathClicked()
{
	InvestPath(static_cast<uint8>(EImmortalAscensionPath::Battle));
}

void UImmortalAscensionWidget::HandleEnlightenmentPathClicked()
{
	InvestPath(static_cast<uint8>(
		EImmortalAscensionPath::Enlightenment));
}

void UImmortalAscensionWidget::HandleFortunePathClicked()
{
	InvestPath(static_cast<uint8>(EImmortalAscensionPath::Fortune));
}
