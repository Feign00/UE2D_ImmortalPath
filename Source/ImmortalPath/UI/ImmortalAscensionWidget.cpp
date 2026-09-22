// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalAscensionWidget.h"
#include "ImmortalAscensionSequenceLayout.h"
#include "ImmortalAlchemyArt.h"
#include "ImmortalFeaturePageLayout.h"
#include "ImmortalUITheme.h"

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
		ImmortalFeaturePageLayout::StabilizeButtonLabel(Button, Text);
		return Text;
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

void UImmortalAscensionWidget::PrepareForOpen()
{
	bAwaitingAscensionConfirmation = false;
	ResetResultMessage();
	RefreshFromPlayer();
}

void UImmortalAscensionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AscensionScreenSize"));
	Root->SetWidthOverride(1600);
	Root->SetHeightOverride(600);
	WidgetTree->RootWidget = Root;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AscensionScreenCanvas"));
	Root->AddChild(Canvas);
	ImmortalFeaturePageLayout::AddReadabilityBackground(WidgetTree, Canvas);
	const auto Card = [&](const TCHAR* Name, float X, float Y, float Width, float Height)
	{
		UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		Background->SetBrush(ImmortalUITheme::PanelBrush(FLinearColor(0.045f, 0.065f, 0.075f)));
		Background->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetAscensionLayout(Canvas->AddChildToCanvas(Background), FVector2D(X,Y), FVector2D(Width,Height));
	};
	const auto Text = [&](const TCHAR* Name, const TCHAR* Label, float X, float Y, float W, float H, int32 Font)
	{
		UTextBlock* Result = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Result->SetText(FText::FromString(Label));
		StyleAscensionText(Result, Font, FLinearColor(0.9f,0.94f,0.91f));
		SetAscensionLayout(Canvas->AddChildToCanvas(Result), FVector2D(X,Y), FVector2D(W,H));
		return Result;
	};
	const auto Button = [&](const TCHAR* Name, float X, float Y, float W, float H)
	{
		UButton* Result = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Result->SetStyle(ImmortalUITheme::ButtonStyle());
		SetAscensionLayout(Canvas->AddChildToCanvas(Result), FVector2D(X,Y), FVector2D(W,H));
		return Result;
	};
	UTexture2D* Atlas = AscensionAtlas.LoadSynchronous();
	const auto Art = [&](const TCHAR* Name, int32 Cell, float X, float Y, float Size)
	{
		UImage* Result = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		Result->SetBrush(ImmortalAlchemyArt::CellBrush(Atlas, Cell));
		Result->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetAscensionLayout(Canvas->AddChildToCanvas(Result), FVector2D(X,Y), FVector2D(Size));
		return Result;
	};
	Text(TEXT("AscensionScreenTitle"), TEXT("羽化飞升"), 18, 4, 400, 38, 28);
	HeaderSummaryText = Text(TEXT("AscensionHeaderSummary"), TEXT(""), 500, 9, 1008, 32, 20);
	HeaderSummaryText->SetJustification(ETextJustify::Right);
	UButton* Close = Button(TEXT("AscensionScreenClose"),1540,3,44,36);
	AddAscensionButtonLabel(WidgetTree, Close, TEXT("×"), 22);
	Close->OnClicked.AddDynamic(this, &UImmortalAscensionWidget::HandleCloseClicked);

	Card(TEXT("AscensionGatePanel"),8,52,460,480);
	Text(TEXT("AscensionGateTitle"), TEXT("飞升条件"),24,74,292,38,24);
	Art(TEXT("AscensionCycleArt"),5,346,65,96);
	EligibilityText = Text(TEXT("AscensionEligibility"),TEXT(""),24,130,420,212,20);
	UTextBlock* ResetText = Text(TEXT("AscensionResetSummary"),
		TEXT("重置：境界、层级、当前修为；本轮八图回第 1 关，地面掉落清除。\n\n保留：装备、背包、灵石、历世地图记录及其他系统。"),
		24,354,420,160,19);
	ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, ResetText);
	ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, EligibilityText);

	Card(TEXT("AscensionBattleCard"),480,52,208,376);
	Card(TEXT("AscensionEnlightenmentCard"),702,52,208,376);
	Card(TEXT("AscensionFortuneCard"),924,52,208,376);
	Art(TEXT("AscensionBattleArt"),0,524,70,120);
	Art(TEXT("AscensionEnlightenmentArt"),1,746,70,120);
	Art(TEXT("AscensionFortuneArt"),2,968,70,120);
	BattlePathText = Text(TEXT("AscensionBattleText"),TEXT(""),494,208,180,144,19);
	EnlightenmentPathText = Text(TEXT("AscensionEnlightenmentText"),TEXT(""),716,208,180,144,19);
	FortunePathText = Text(TEXT("AscensionFortuneText"),TEXT(""),938,208,180,144,19);
	for (UTextBlock* PathText : {BattlePathText.Get(), EnlightenmentPathText.Get(), FortunePathText.Get()})
	{
		PathText->SetJustification(ETextJustify::Center);
		ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, PathText);
	}
	BattlePathButton = Button(TEXT("AscensionBattlePathButton"),494,370,180,44);
	EnlightenmentPathButton = Button(TEXT("AscensionEnlightenmentPathButton"),716,370,180,44);
	FortunePathButton = Button(TEXT("AscensionFortunePathButton"),938,370,180,44);
	for (UButton* PathButton : {BattlePathButton.Get(), EnlightenmentPathButton.Get(), FortunePathButton.Get()})
		AddAscensionButtonLabel(WidgetTree, PathButton, TEXT("注入仙印"),18);
	BattlePathButton->OnClicked.AddDynamic(this,&UImmortalAscensionWidget::HandleBattlePathClicked);
	EnlightenmentPathButton->OnClicked.AddDynamic(this,&UImmortalAscensionWidget::HandleEnlightenmentPathClicked);
	FortunePathButton->OnClicked.AddDynamic(this,&UImmortalAscensionWidget::HandleFortunePathClicked);

	Card(TEXT("AscensionBonusPanel"),480,440,652,92);
	Art(TEXT("AscensionSealArt"),3,490,450,72);
	BonusText = Text(TEXT("AscensionBonusSummary"),TEXT(""),578,450,538,74,18);
	ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, BonusText);

	Card(TEXT("AscensionActionPanel"),1144,52,448,480);
	Text(TEXT("AscensionActionTitle"),TEXT("飞升台"),1162,70,400,34,24);
	Art(TEXT("AscensionAltarArt"),4,1248,106,224);
	RewardText = Text(TEXT("AscensionReward"),TEXT(""),1162,334,412,40,22);
	RewardText->SetJustification(ETextJustify::Center);
	ReturnQingyunButton = Button(TEXT("AscensionReturnQingyunButton"),1162,384,412,42);
	AddAscensionButtonLabel(WidgetTree,ReturnQingyunButton,TEXT("返回青云山"),18);
	ReturnQingyunButton->OnClicked.AddDynamic(this,&UImmortalAscensionWidget::HandleReturnQingyunClicked);
	AscendButton = Button(TEXT("AscensionPerformButton"),1162,438,412,48);
	AscendButtonText = AddAscensionButtonLabel(WidgetTree,AscendButton,TEXT("羽化飞升"),20);
	AscendButton->OnClicked.AddDynamic(this,&UImmortalAscensionWidget::HandleAscendClicked);
	CancelAscensionButton = Button(TEXT("AscensionCancelButton"),1162,492,412,32);
	AddAscensionButtonLabel(WidgetTree,CancelAscensionButton,TEXT("取消飞升"),17);
	CancelAscensionButton->OnClicked.AddDynamic(this,&UImmortalAscensionWidget::HandleCancelAscensionClicked);
	CancelAscensionButton->SetVisibility(ESlateVisibility::Hidden);
	ResultText = Text(TEXT("AscensionOperationResult"),TEXT(""),18,546,1560,42,19);
	ImmortalFeaturePageLayout::MakeScrollable(WidgetTree,ResultText);

	// Preserve the original art and 12 FPS playback; corrected pose windows avoid
	// the legacy 17-column slicing error. Display at the exact cropped aspect ratio.
	AscensionSequenceOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("AscensionSequenceOverlay"));
	AscensionSequenceOverlay->SetBrushColor(FLinearColor(0.018f,0.008f,0.040f,0.995f));
	AscensionSequenceOverlay->SetPadding(FMargin(0));
	UCanvasPanelSlot* OverlaySlot = Canvas->AddChildToCanvas(AscensionSequenceOverlay);
	OverlaySlot->SetPosition(FVector2D::ZeroVector);
	OverlaySlot->SetSize(FVector2D(1600,600));
	OverlaySlot->SetZOrder(100);
	UCanvasPanel* SequenceCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("AscensionSequenceCanvas"));
	AscensionSequenceOverlay->AddChild(SequenceCanvas);
	AscensionSequenceImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("AscensionSequenceImage"));
	SetAscensionLayout(SequenceCanvas->AddChildToCanvas(AscensionSequenceImage),FVector2D(736,70),FVector2D(128,430));
	AscensionSequenceCaption = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("AscensionSequenceCaption"));
	StyleAscensionText(AscensionSequenceCaption,26,FLinearColor(0.92f,0.78f,1),true);
	SetAscensionLayout(SequenceCanvas->AddChildToCanvas(AscensionSequenceCaption),FVector2D(460,535),FVector2D(680,40));
	AscensionSequenceOverlay->SetVisibility(ESlateVisibility::Collapsed);
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
		constexpr float FramesPerSecond = ImmortalAscensionSequenceLayout::FramesPerSecond;
		constexpr int32 FrameCount = ImmortalAscensionSequenceLayout::FrameCount;
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
		TEXT("Independent ascension UI sequence started: texture=%s originalPoses=16 fps=12 combatUnaffected=true"),
		*AscensionSequenceTexture->GetPathName());
}

void UImmortalAscensionWidget::UpdateAscensionSequenceFrame(
	const int32 FrameIndex)
{
	if (!AscensionSequenceTexture || !AscensionSequenceImage)
	{
		return;
	}
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	// The source sheet reserves substantial transparent space above and below
	// every pose. Crop only that shared safe area so the character remains
	// readable in the independent page without distorting its proportions.
	Brush.ImageSize = FVector2D(128.0f, 430.0f);
	Brush.TintColor = FSlateColor(FLinearColor::White);
	Brush.SetResourceObject(AscensionSequenceTexture);
	Brush.SetUVRegion(ImmortalAscensionSequenceLayout::GetFrameUV(FrameIndex));
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

	if (!Eligibility.bEligible && bAwaitingAscensionConfirmation)
	{
		bAwaitingAscensionConfirmation = false;
		ResetResultMessage();
	}
	HeaderSummaryText->SetText(FText::FromString(FString::Printf(
		TEXT("轮回 %d 次  ·  仙印 %d  ·  历世地图 %d / %d"),
		State.AscensionCount, State.ImmortalSeals,
		UImmortalAscensionLibrary::GetLifetimeCompletedMapCount(State),
		UImmortalMapLibrary::GetKnownMapIds().Num())));
	EligibilityText->SetText(FText::FromString(FString::Printf(
		TEXT("%s 已修至飞升境界\n%s 仙宫遗址第 999 关已通关\n%s 当前位于青云山\n%s 世界妖王与无尽秘境均空闲\n%s 未达 999 次飞升上限"),
		*GateGlyph(Eligibility.bReachedAscensionRealm),
		*GateGlyph(Eligibility.bImmortalPalaceCompleted),
		*GateGlyph(Eligibility.bAtQingyunMountain),
		*GateGlyph(Eligibility.bIndependentEncountersIdle),
		*GateGlyph(Eligibility.bBelowAscensionLimit))));
	BonusText->SetText(FText::FromString(FString::Printf(
		TEXT("飞升 %d / %d 次 · 历世 %d / %d 图\n仙印 %d · 累计获得 %lld"),
		State.AscensionCount, UImmortalAscensionLibrary::MaximumAscensionCount,
		UImmortalAscensionLibrary::GetLifetimeCompletedMapCount(State),
		UImmortalMapLibrary::GetKnownMapIds().Num(),
		State.ImmortalSeals, State.TotalImmortalSealsEarned)));
	RewardText->SetText(FText::FromString(Eligibility.bBelowAscensionLimit
		? FString::Printf(TEXT("下次奖励 %d 枚仙印"), Eligibility.RewardImmortalSeals)
		: FString(TEXT("飞升次数已达上限"))));

	if (BattlePathText)
	{
		BattlePathText->SetText(FText::FromString(FString::Printf(
			TEXT("战道  %d / 50 阶\n全部攻击 +4%% / 阶\n当前 ×%.2f\n%s"),
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
			TEXT("悟道  %d / 50 阶\n修炼速度 +6%% / 阶\n当前 ×%.2f\n%s"),
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
			TEXT("福缘  %d / 50 阶\n装备掉落 +3%% / 阶\n当前 ×%.2f\n%s"),
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
		AscendButtonText->SetText(FText::FromString(!Eligibility.bEligible
			? TEXT("尚未满足飞升条件")
			: bAwaitingAscensionConfirmation ? TEXT("确认飞升 · 重开本轮") : TEXT("羽化飞升")));
	}
	if (CancelAscensionButton) CancelAscensionButton->SetVisibility(
		bAwaitingAscensionConfirmation ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
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
	bAwaitingAscensionConfirmation = false;
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

void UImmortalAscensionWidget::HandleCancelAscensionClicked()
{
	bAwaitingAscensionConfirmation = false;
	ResetResultMessage();
	RefreshFromPlayer();
}

void UImmortalAscensionWidget::HandleAscendClicked()
{
	if (!Player.IsValid() || bAscensionSequencePlaying) return;
	const auto Eligibility = Player->EvaluateAscensionEligibility();
	if (!Eligibility.bEligible)
	{
		bAwaitingAscensionConfirmation = false;
		SetResultMessage(Eligibility.Message, false);
		RefreshFromPlayer();
		return;
	}
	if (!bAwaitingAscensionConfirmation)
	{
		bAwaitingAscensionConfirmation = true;
		SetResultMessage(FText::FromString(TEXT("请确认左侧重置/保留内容：再次点击将开启新轮回；也可取消。")), false);
		ResultMessageExpirySeconds = 0;
		RefreshFromPlayer();
		return;
	}
	bAwaitingAscensionConfirmation = false;
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
