// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalWorldBossWidget.h"

#include "../Artifacts/ImmortalArtifactTypes.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "../Maps/ImmortalMapTypes.h"
#include "../WorldBoss/ImmortalWorldBossTypes.h"
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
	constexpr int32 VisibleWorldBossCount = 4;

	void SetWorldBossLayout(
		UCanvasPanelSlot* Slot,
		const FVector2D Position,
		const FVector2D Size)
	{
		if (!Slot) return;
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetAutoSize(false);
	}

	void StyleWorldBossText(
		UTextBlock* Text,
		const int32 Size,
		const FLinearColor& Color,
		const bool bCentered = false)
	{
		if (!Text) return;
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor::Black);
		Text->SetJustification(bCentered ? ETextJustify::Center : ETextJustify::Left);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}

	FSlateBrush MakeWorldBossBrush(
		const FVector2D Size,
		const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	FButtonStyle MakeWorldBossButtonStyle(
		const FVector2D Size,
		const FLinearColor& Tint)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeWorldBossBrush(Size, Tint));
		Style.SetHovered(MakeWorldBossBrush(Size, (Tint * 1.16f).GetClamped()));
		Style.SetPressed(MakeWorldBossBrush(Size, (Tint * 0.76f).GetClamped()));
		Style.SetDisabled(MakeWorldBossBrush(
			Size, FLinearColor(0.065f, 0.065f, 0.078f, 0.94f)));
		return Style;
	}

	UTextBlock* AddWorldBossButtonLabel(
		UWidgetTree* Tree,
		UButton* Button,
		const TCHAR* Label,
		const int32 FontSize = 14)
	{
		if (!Tree || !Button) return nullptr;
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		Text->SetAutoWrapText(true);
		StyleWorldBossText(
			Text, FontSize, FLinearColor(1.0f, 0.92f, 0.70f, 1.0f), true);
		Button->AddChild(Text);
		return Text;
	}

	FLinearColor MakeDimBossColor(const FLinearColor& Color)
	{
		return FLinearColor(
			Color.R * 0.26f,
			Color.G * 0.26f,
			Color.B * 0.26f,
			0.96f);
	}

	FString GetMaterialDisplayName(const FName MaterialId)
	{
		FImmortalMaterialDefinition Definition;
		return UImmortalMaterialLibrary::GetMaterialDefinition(MaterialId, Definition)
			? Definition.DisplayName.ToString()
			: MaterialId.ToString();
	}

	FString GetArtifactDisplayName(const FName ArtifactId)
	{
		FImmortalArtifactDefinition Definition;
		return UImmortalArtifactLibrary::GetArtifactDefinition(ArtifactId, Definition)
			? Definition.DisplayName.ToString()
			: ArtifactId.ToString();
	}

	FString FormatBestTime(const float Seconds)
	{
		if (Seconds <= 0.0f)
		{
			return TEXT("--");
		}
		const int32 WholeMinutes = FMath::FloorToInt(Seconds / 60.0f);
		const float RemainingSeconds = Seconds - static_cast<float>(WholeMinutes * 60);
		return WholeMinutes > 0
			? FString::Printf(TEXT("%d:%04.1f"), WholeMinutes, RemainingSeconds)
			: FString::Printf(TEXT("%.1f 秒"), Seconds);
	}
}

void UImmortalWorldBossWidget::InitializeForPlayer(
	AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalWorldBossWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("WorldBossScreenSize"));
	RootSize->SetWidthOverride(1600.0f);
	RootSize->SetHeightOverride(300.0f);
	WidgetTree->RootWidget = RootSize;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("WorldBossScreenBackground"));
	Background->SetBrushColor(FLinearColor(0.030f, 0.018f, 0.035f, 0.992f));
	Background->SetPadding(FMargin(0.0f));
	RootSize->AddChild(Background);

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("WorldBossScreenCanvas"));
	Background->AddChild(Canvas);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("WorldBossScreenTitle"));
	Title->SetText(FText::FromString(TEXT("世界妖王  [V]")));
	StyleWorldBossText(Title, 23, FLinearColor(1.0f, 0.48f, 0.30f, 1.0f));
	SetWorldBossLayout(
		Canvas->AddChildToCanvas(Title),
		FVector2D(16.0f, 2.0f),
		FVector2D(225.0f, 36.0f));

	HeaderSummaryText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("WorldBossHeaderSummary"));
	HeaderSummaryText->SetAutoWrapText(true);
	StyleWorldBossText(
		HeaderSummaryText, 13, FLinearColor(0.95f, 0.82f, 0.66f, 1.0f), true);
	SetWorldBossLayout(
		Canvas->AddChildToCanvas(HeaderSummaryText),
		FVector2D(236.0f, 2.0f),
		FVector2D(1294.0f, 36.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("WorldBossScreenClose"));
	CloseButton->SetStyle(MakeWorldBossButtonStyle(
		FVector2D(46.0f, 32.0f), FLinearColor(0.48f, 0.13f, 0.10f, 1.0f)));
	CloseButton->OnClicked.AddDynamic(
		this, &UImmortalWorldBossWidget::HandleCloseClicked);
	SetWorldBossLayout(
		Canvas->AddChildToCanvas(CloseButton),
		FVector2D(1538.0f, 3.0f),
		FVector2D(46.0f, 32.0f));
	AddWorldBossButtonLabel(WidgetTree, CloseButton, TEXT("×"), 22);

	UTextBlock* SelectLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("WorldBossSelectLabel"));
	SelectLabel->SetText(FText::FromString(TEXT("妖王选择")));
	StyleWorldBossText(
		SelectLabel, 15, FLinearColor(0.90f, 0.68f, 0.46f, 1.0f), true);
	SetWorldBossLayout(
		Canvas->AddChildToCanvas(SelectLabel),
		FVector2D(16.0f, 49.0f),
		FVector2D(91.0f, 40.0f));

	BossButtons.Reserve(VisibleWorldBossCount);
	BossButtonLabels.Reserve(VisibleWorldBossCount);
	for (int32 Index = 0; Index < VisibleWorldBossCount; ++Index)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			*FString::Printf(TEXT("WorldBossChoice%d"), Index));
		Button->SetStyle(MakeWorldBossButtonStyle(
			FVector2D(190.0f, 48.0f), FLinearColor(0.15f, 0.10f, 0.17f, 0.98f)));
		switch (Index)
		{
		case 0:
			Button->OnClicked.AddDynamic(
				this, &UImmortalWorldBossWidget::HandleBoss0Clicked);
			break;
		case 1:
			Button->OnClicked.AddDynamic(
				this, &UImmortalWorldBossWidget::HandleBoss1Clicked);
			break;
		case 2:
			Button->OnClicked.AddDynamic(
				this, &UImmortalWorldBossWidget::HandleBoss2Clicked);
			break;
		case 3:
			Button->OnClicked.AddDynamic(
				this, &UImmortalWorldBossWidget::HandleBoss3Clicked);
			break;
		default:
			break;
		}
		SetWorldBossLayout(
			Canvas->AddChildToCanvas(Button),
			FVector2D(111.0f + static_cast<float>(Index) * 196.0f, 44.0f),
			FVector2D(190.0f, 48.0f));
		BossButtons.Add(Button);
		BossButtonLabels.Add(
			AddWorldBossButtonLabel(WidgetTree, Button, TEXT("妖王"), 13));
	}

	UBorder* DetailsPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("WorldBossDetailsPanel"));
	DetailsPanel->SetBrushColor(FLinearColor(0.065f, 0.034f, 0.070f, 0.96f));
	DetailsPanel->SetPadding(FMargin(0.0f));
	SetWorldBossLayout(
		Canvas->AddChildToCanvas(DetailsPanel),
		FVector2D(14.0f, 100.0f),
		FVector2D(480.0f, 187.0f));
	UCanvasPanel* DetailsCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("WorldBossDetailsCanvas"));
	DetailsPanel->AddChild(DetailsCanvas);

	BossNameText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("WorldBossName"));
	StyleWorldBossText(BossNameText, 22, FLinearColor::White);
	SetWorldBossLayout(
		DetailsCanvas->AddChildToCanvas(BossNameText),
		FVector2D(10.0f, 3.0f),
		FVector2D(460.0f, 31.0f));

	BossDescriptionText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("WorldBossDescription"));
	BossDescriptionText->SetAutoWrapText(true);
	StyleWorldBossText(
		BossDescriptionText, 12, FLinearColor(0.88f, 0.86f, 0.90f, 1.0f));
	SetWorldBossLayout(
		DetailsCanvas->AddChildToCanvas(BossDescriptionText),
		FVector2D(10.0f, 35.0f),
		FVector2D(460.0f, 52.0f));

	RequirementText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("WorldBossRequirement"));
	RequirementText->SetAutoWrapText(true);
	StyleWorldBossText(
		RequirementText, 12, FLinearColor(0.68f, 0.88f, 1.0f, 1.0f));
	SetWorldBossLayout(
		DetailsCanvas->AddChildToCanvas(RequirementText),
		FVector2D(10.0f, 91.0f),
		FVector2D(460.0f, 43.0f));

	HistoryText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("WorldBossHistory"));
	HistoryText->SetAutoWrapText(true);
	StyleWorldBossText(
		HistoryText, 13, FLinearColor(1.0f, 0.80f, 0.38f, 1.0f));
	SetWorldBossLayout(
		DetailsCanvas->AddChildToCanvas(HistoryText),
		FVector2D(10.0f, 139.0f),
		FVector2D(460.0f, 39.0f));

	UBorder* SkillsPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("WorldBossSkillsPanel"));
	SkillsPanel->SetBrushColor(FLinearColor(0.048f, 0.035f, 0.076f, 0.96f));
	SkillsPanel->SetPadding(FMargin(10.0f, 6.0f));
	SetWorldBossLayout(
		Canvas->AddChildToCanvas(SkillsPanel),
		FVector2D(504.0f, 100.0f),
		FVector2D(365.0f, 187.0f));
	SkillText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("WorldBossSkills"));
	SkillText->SetAutoWrapText(true);
	StyleWorldBossText(
		SkillText, 12, FLinearColor(0.86f, 0.76f, 1.0f, 1.0f));
	SkillsPanel->AddChild(SkillText);

	UBorder* DropsPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("WorldBossDropsPanel"));
	DropsPanel->SetBrushColor(FLinearColor(0.036f, 0.061f, 0.055f, 0.96f));
	DropsPanel->SetPadding(FMargin(10.0f, 6.0f));
	SetWorldBossLayout(
		Canvas->AddChildToCanvas(DropsPanel),
		FVector2D(879.0f, 100.0f),
		FVector2D(382.0f, 187.0f));
	DropText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("WorldBossDrops"));
	DropText->SetAutoWrapText(true);
	StyleWorldBossText(
		DropText, 12, FLinearColor(0.64f, 1.0f, 0.73f, 1.0f));
	DropsPanel->AddChild(DropText);

	UBorder* ActionPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("WorldBossActionPanel"));
	ActionPanel->SetBrushColor(FLinearColor(0.070f, 0.038f, 0.045f, 0.97f));
	ActionPanel->SetPadding(FMargin(0.0f));
	SetWorldBossLayout(
		Canvas->AddChildToCanvas(ActionPanel),
		FVector2D(1271.0f, 43.0f),
		FVector2D(315.0f, 244.0f));
	UCanvasPanel* ActionCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("WorldBossActionCanvas"));
	ActionPanel->AddChild(ActionCanvas);

	RuntimeText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("WorldBossRuntime"));
	RuntimeText->SetAutoWrapText(true);
	StyleWorldBossText(
		RuntimeText, 13, FLinearColor(1.0f, 0.76f, 0.58f, 1.0f), true);
	SetWorldBossLayout(
		ActionCanvas->AddChildToCanvas(RuntimeText),
		FVector2D(9.0f, 7.0f),
		FVector2D(297.0f, 89.0f));

	ChallengeButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("WorldBossChallengeButton"));
	ChallengeButton->OnClicked.AddDynamic(
		this, &UImmortalWorldBossWidget::HandleChallengeClicked);
	SetWorldBossLayout(
		ActionCanvas->AddChildToCanvas(ChallengeButton),
		FVector2D(13.0f, 101.0f),
		FVector2D(289.0f, 49.0f));
	ChallengeButtonText = AddWorldBossButtonLabel(
		WidgetTree, ChallengeButton, TEXT("挑战妖王"), 17);

	RetryRewardsButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("WorldBossRetryRewardsButton"));
	RetryRewardsButton->SetStyle(MakeWorldBossButtonStyle(
		FVector2D(289.0f, 40.0f), FLinearColor(0.40f, 0.29f, 0.09f, 1.0f)));
	RetryRewardsButton->OnClicked.AddDynamic(
		this, &UImmortalWorldBossWidget::HandleRetryRewardsClicked);
	SetWorldBossLayout(
		ActionCanvas->AddChildToCanvas(RetryRewardsButton),
		FVector2D(13.0f, 156.0f),
		FVector2D(289.0f, 40.0f));
	RetryRewardsButtonText = AddWorldBossButtonLabel(
		WidgetTree, RetryRewardsButton, TEXT("暂无待领奖励"), 13);

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("WorldBossOperationResult"));
	ResultText->SetAutoWrapText(true);
	StyleWorldBossText(
		ResultText, 11, FLinearColor(0.76f, 0.82f, 0.86f, 1.0f), true);
	SetWorldBossLayout(
		ActionCanvas->AddChildToCanvas(ResultText),
		FVector2D(9.0f, 201.0f),
		FVector2D(297.0f, 36.0f));

	RefreshFromPlayer();
}

void UImmortalWorldBossWidget::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Player.IsValid()) return;

	RuntimeRefreshAccumulator += FMath::Max(InDeltaTime, 0.0f);
	const FImmortalWorldBossRuntimeSnapshot Snapshot =
		Player->GetWorldBossRuntimeSnapshot();
	const int32 RealmIndex = static_cast<int32>(Player->GetCultivationRealm());
	const bool bStaticStateChanged =
		Player->GetWorldBossRevision() != LastWorldBossRevision
		|| RealmIndex != LastRealmIndex
		|| Snapshot.bActive != bLastChallengeActive
		|| Snapshot.BossId != LastRuntimeBossId
		|| Snapshot.Phase != LastRuntimePhase;

	if (bStaticStateChanged)
	{
		RefreshFromPlayer();
	}
	else if (RuntimeRefreshAccumulator >= 0.10f)
	{
		RuntimeRefreshAccumulator = 0.0f;
		RefreshRuntimeStatus();
	}
}

void UImmortalWorldBossWidget::RefreshFromPlayer()
{
	if (!Player.IsValid() || !BossNameText) return;

	DisplayedBossIds = UImmortalWorldBossLibrary::GetKnownWorldBossIds();
	if (DisplayedBossIds.Num() > VisibleWorldBossCount)
	{
		DisplayedBossIds.SetNum(VisibleWorldBossCount);
	}

	const FImmortalWorldBossRuntimeSnapshot Snapshot =
		Player->GetWorldBossRuntimeSnapshot();
	if (Snapshot.bActive && !Snapshot.BossId.IsNone())
	{
		SelectedBossId = Snapshot.BossId;
	}
	else if (SelectedBossId.IsNone() || !DisplayedBossIds.Contains(SelectedBossId))
	{
		SelectedBossId =
			DisplayedBossIds.IsEmpty() ? NAME_None : DisplayedBossIds[0];
	}

	LastWorldBossRevision = Player->GetWorldBossRevision();
	LastRealmIndex = static_cast<int32>(Player->GetCultivationRealm());
	LastRuntimeBossId = Snapshot.BossId;
	LastRuntimePhase = Snapshot.Phase;
	bLastChallengeActive = Snapshot.bActive;
	RuntimeRefreshAccumulator = 0.0f;

	RefreshBossButtons();
	RefreshSelectedBossDetails();
	RefreshRuntimeStatus();
}

void UImmortalWorldBossWidget::SelectBoss(const FName BossId)
{
	if (!DisplayedBossIds.Contains(BossId))
	{
		return;
	}
	const FImmortalWorldBossRuntimeSnapshot Snapshot =
		Player.IsValid()
			? Player->GetWorldBossRuntimeSnapshot()
			: FImmortalWorldBossRuntimeSnapshot();
	if (Snapshot.bActive)
	{
		SetResultMessage(
			FText::FromString(TEXT("挑战进行中，退出后才能选择其他妖王")), false);
		return;
	}

	SelectedBossId = BossId;
	SetResultMessage(FText::GetEmpty(), true);
	RefreshBossButtons();
	RefreshSelectedBossDetails();
	RefreshRuntimeStatus();
}

void UImmortalWorldBossWidget::SelectBossByIndex(const int32 BossIndex)
{
	if (DisplayedBossIds.IsValidIndex(BossIndex))
	{
		SelectBoss(DisplayedBossIds[BossIndex]);
	}
}

void UImmortalWorldBossWidget::RefreshBossButtons()
{
	if (!Player.IsValid()) return;

	const bool bChallengeActive =
		Player->GetWorldBossRuntimeSnapshot().bActive;
	for (int32 Index = 0; Index < BossButtons.Num(); ++Index)
	{
		UButton* Button = BossButtons[Index];
		UTextBlock* Label = BossButtonLabels.IsValidIndex(Index)
			? BossButtonLabels[Index]
			: nullptr;
		if (!Button || !Label) continue;

		if (!DisplayedBossIds.IsValidIndex(Index))
		{
			Button->SetIsEnabled(false);
			Label->SetText(FText::FromString(TEXT("暂未现世")));
			continue;
		}

		const FName BossId = DisplayedBossIds[Index];
		FImmortalWorldBossDefinition Definition;
		const bool bValid =
			UImmortalWorldBossLibrary::GetWorldBossDefinition(BossId, Definition);
		const bool bUnlocked = bValid && Player->IsWorldBossUnlocked(BossId);
		const bool bSelected = BossId == SelectedBossId;
		Button->SetIsEnabled(bValid && !bChallengeActive);
		if (!bValid)
		{
			Label->SetText(FText::FromName(BossId));
			continue;
		}

		Button->SetStyle(MakeWorldBossButtonStyle(
			FVector2D(190.0f, 48.0f),
			bSelected
				? Definition.DisplayColor.GetClamped(0.18f, 0.88f)
				: MakeDimBossColor(Definition.DisplayColor)));
		Label->SetText(FText::FromString(FString::Printf(
			TEXT("%s\n%s"),
			*Definition.DisplayName.ToString(),
			bUnlocked ? (bSelected ? TEXT("已选择") : TEXT("可挑战")) : TEXT("境界未解锁"))));
		Label->SetColorAndOpacity(FSlateColor(
			bUnlocked
				? FLinearColor(1.0f, 0.94f, 0.76f, 1.0f)
				: FLinearColor(0.63f, 0.60f, 0.66f, 1.0f)));
	}
}

void UImmortalWorldBossWidget::RefreshSelectedBossDetails()
{
	if (!Player.IsValid()) return;

	FImmortalWorldBossDefinition Definition;
	if (!UImmortalWorldBossLibrary::GetWorldBossDefinition(
		SelectedBossId, Definition))
	{
		if (BossNameText) BossNameText->SetText(FText::FromString(TEXT("暂无妖王情报")));
		if (BossDescriptionText) BossDescriptionText->SetText(FText::GetEmpty());
		if (RequirementText) RequirementText->SetText(FText::GetEmpty());
		if (HistoryText) HistoryText->SetText(FText::GetEmpty());
		if (SkillText) SkillText->SetText(FText::GetEmpty());
		if (DropText) DropText->SetText(FText::GetEmpty());
		return;
	}

	FImmortalWorldBossProgress Progress;
	Player->GetWorldBossProgress(Definition.BossId, Progress);
	const bool bUnlocked = Player->IsWorldBossUnlocked(Definition.BossId);

	if (BossNameText)
	{
		BossNameText->SetText(Definition.DisplayName);
		BossNameText->SetColorAndOpacity(
			FSlateColor(Definition.DisplayColor.GetClamped(0.48f, 1.0f)));
	}
	if (BossDescriptionText)
	{
		BossDescriptionText->SetText(Definition.Description);
	}
	if (RequirementText)
	{
		RequirementText->SetText(FText::FromString(FString::Printf(
			TEXT("解锁境界：%s  ·  推荐关卡：%d\n限时：%.0f 秒  ·  %s"),
			*UImmortalMapLibrary::GetRealmRequirementText(
				Definition.RequiredRealmIndex).ToString(),
			Definition.RecommendedStage,
			Definition.TimeLimitSeconds,
			bUnlocked ? TEXT("已解锁") : TEXT("当前境界不足"))));
		RequirementText->SetColorAndOpacity(FSlateColor(
			bUnlocked
				? FLinearColor(0.55f, 1.0f, 0.76f, 1.0f)
				: FLinearColor(1.0f, 0.38f, 0.30f, 1.0f)));
	}
	if (HistoryText)
	{
		HistoryText->SetText(FText::FromString(FString::Printf(
			TEXT("历史击杀：%d 次  ·  最佳时间：%s"),
			FMath::Max(Progress.DefeatCount, 0),
			*FormatBestTime(Progress.BestClearSeconds))));
	}
	if (SkillText)
	{
		SkillText->SetText(FText::FromString(FString::Printf(
			TEXT("三阶段技能\n"
				"Ⅰ 100%%—67%%：每 %d 次攻击释放远程术法，伤害 ×%.2f，射程 +%.0f\n"
				"Ⅱ 66%%—34%%：攻击与攻速提升，召唤 %d 只妖兽\n"
				"Ⅲ 33%%以下：进入狂暴，连续强化并召唤 %d 只妖兽"),
			FMath::Max(Definition.SkillEveryAttacks, 2),
			FMath::Max(Definition.SkillDamageMultiplier, 1.0f),
			FMath::Max(Definition.SkillBonusRange, 0.0f),
			FMath::Max(Definition.PhaseTwoSummonCount, 0),
			FMath::Max(Definition.PhaseThreeSummonCount, 0))));
	}
	if (DropText)
	{
		DropText->SetText(FText::FromString(FString::Printf(
			TEXT("独立掉落池\n"
				"装备 ×%d（%s及以上，等级 +%d）\n"
				"灵石 ×%d\n"
				"%s ×%d  ·  法宝碎片 ×%d\n"
				"首杀固定法宝：%s"),
			FMath::Max(Definition.GuaranteedEquipmentDrops, 1),
			*UImmortalEquipmentLibrary::GetQualityText(
				Definition.MinimumEquipmentQuality).ToString(),
			FMath::Max(Definition.EquipmentLevelBonus, 0),
			FMath::Max(Definition.SpiritStoneReward, 1),
			*GetMaterialDisplayName(Definition.RareMaterialId),
			FMath::Max(Definition.RareMaterialQuantity, 1),
			FMath::Max(Definition.ArtifactFragmentQuantity, 1),
			*GetArtifactDisplayName(Definition.FirstClearArtifactId))));
	}
}

void UImmortalWorldBossWidget::RefreshRuntimeStatus()
{
	if (!Player.IsValid()) return;

	const FImmortalWorldBossRuntimeSnapshot Snapshot =
		Player->GetWorldBossRuntimeSnapshot();
	const FImmortalWorldBossState State = Player->GetWorldBossState();
	const int32 PendingCount = State.PendingRewards.Num();
	LastRuntimeBossId = Snapshot.BossId;
	LastRuntimePhase = Snapshot.Phase;
	bLastChallengeActive = Snapshot.bActive;

	if (HeaderSummaryText)
	{
		HeaderSummaryText->SetText(FText::FromString(
			Snapshot.bActive
				? FString::Printf(
					TEXT("挑战中：%s  ·  阶段 %d  ·  剩余 %.1f 秒  ·  普通关卡刷怪已暂停"),
					*Snapshot.DisplayName.ToString(),
					FMath::Clamp(Snapshot.Phase, 1, 3),
					FMath::Max(Snapshot.RemainingSeconds, 0.0f))
				: FString::Printf(
					TEXT("独立限时挑战 · 不占用 1—999 关进度 · 待领奖励 %d 份"),
					PendingCount)));
	}
	if (RuntimeText)
	{
		if (Snapshot.bActive)
		{
			const float HealthPercent = Snapshot.MaximumHealth > 0.0f
				? FMath::Clamp(
					Snapshot.CurrentHealth / Snapshot.MaximumHealth, 0.0f, 1.0f)
				: 0.0f;
			RuntimeText->SetText(FText::FromString(FString::Printf(
				TEXT("%s\n阶段 %d / 3\n生命 %.0f / %.0f（%.1f%%）\n倒计时 %.1f / %.0f 秒"),
				*Snapshot.DisplayName.ToString(),
				FMath::Clamp(Snapshot.Phase, 1, 3),
				FMath::Max(Snapshot.CurrentHealth, 0.0f),
				FMath::Max(Snapshot.MaximumHealth, 0.0f),
				HealthPercent * 100.0f,
				FMath::Max(Snapshot.RemainingSeconds, 0.0f),
				FMath::Max(Snapshot.TimeLimitSeconds, 0.0f))));
			RuntimeText->SetColorAndOpacity(
				FSlateColor(FLinearColor(1.0f, 0.57f, 0.36f, 1.0f)));
		}
		else
		{
			FImmortalWorldBossDefinition Definition;
			const bool bValid =
				UImmortalWorldBossLibrary::GetWorldBossDefinition(
					SelectedBossId, Definition);
			RuntimeText->SetText(FText::FromString(
				bValid
					? FString::Printf(
						TEXT("准备挑战\n%s\n限时 %.0f 秒\n失败不会推进或重置关卡"),
						*Definition.DisplayName.ToString(),
						Definition.TimeLimitSeconds)
					: TEXT("请选择一只世界妖王")));
			RuntimeText->SetColorAndOpacity(
				FSlateColor(FLinearColor(0.88f, 0.82f, 0.72f, 1.0f)));
		}
	}
	if (ChallengeButton && ChallengeButtonText)
	{
		const bool bUnlocked = !SelectedBossId.IsNone()
			&& Player->IsWorldBossUnlocked(SelectedBossId);
		ChallengeButton->SetIsEnabled(Snapshot.bActive || bUnlocked);
		ChallengeButton->SetStyle(MakeWorldBossButtonStyle(
			FVector2D(289.0f, 49.0f),
			Snapshot.bActive
				? FLinearColor(0.58f, 0.13f, 0.10f, 1.0f)
				: FLinearColor(0.19f, 0.49f, 0.28f, 1.0f)));
		ChallengeButtonText->SetText(FText::FromString(
			Snapshot.bActive
				? TEXT("退出当前挑战")
				: (bUnlocked ? TEXT("开始挑战") : TEXT("境界不足"))));
	}
	if (RetryRewardsButton && RetryRewardsButtonText)
	{
		RetryRewardsButton->SetIsEnabled(PendingCount > 0);
		RetryRewardsButtonText->SetText(FText::FromString(
			PendingCount > 0
				? FString::Printf(TEXT("重试待领奖励（%d）"), PendingCount)
				: TEXT("暂无待领奖励")));
	}
}

void UImmortalWorldBossWidget::SetResultMessage(
	const FText& Message,
	const bool bSucceeded)
{
	if (!ResultText) return;
	ResultText->SetText(Message);
	ResultText->SetColorAndOpacity(FSlateColor(
		bSucceeded
			? FLinearColor(0.45f, 1.0f, 0.66f, 1.0f)
			: FLinearColor(1.0f, 0.38f, 0.29f, 1.0f)));
}

void UImmortalWorldBossWidget::HandleBoss0Clicked()
{
	SelectBossByIndex(0);
}

void UImmortalWorldBossWidget::HandleBoss1Clicked()
{
	SelectBossByIndex(1);
}

void UImmortalWorldBossWidget::HandleBoss2Clicked()
{
	SelectBossByIndex(2);
}

void UImmortalWorldBossWidget::HandleBoss3Clicked()
{
	SelectBossByIndex(3);
}

void UImmortalWorldBossWidget::HandleChallengeClicked()
{
	if (!Player.IsValid()) return;

	const FImmortalWorldBossRuntimeSnapshot Snapshot =
		Player->GetWorldBossRuntimeSnapshot();
	const FImmortalWorldBossChallengeResult Result =
		Snapshot.bActive
			? Player->CancelWorldBossChallenge()
			: Player->StartWorldBossChallenge(SelectedBossId);
	SetResultMessage(Result.Message, Result.bSucceeded);
	if (!Snapshot.bActive && Result.bSucceeded)
	{
		// Reveal the bottom-strip battle immediately after a successful start.
		Player->ToggleWorldBoss();
		return;
	}
	RefreshFromPlayer();
}

void UImmortalWorldBossWidget::HandleRetryRewardsClicked()
{
	if (!Player.IsValid()) return;

	const bool bSucceeded = Player->RetryPendingWorldBossRewards();
	SetResultMessage(
		FText::FromString(
			bSucceeded
				? TEXT("待领奖励已全部发放")
				: TEXT("仍有奖励未能发放，请检查背包容量后重试")),
		bSucceeded);
	RefreshFromPlayer();
}

void UImmortalWorldBossWidget::HandleCloseClicked()
{
	if (Player.IsValid())
	{
		Player->ToggleWorldBoss();
	}
}
