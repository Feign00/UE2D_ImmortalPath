// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalEndlessDungeonWidget.h"

#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Endless/ImmortalEndlessDungeonTypes.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
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
	void SetEndlessLayout(
		UCanvasPanelSlot* Slot,
		const FVector2D Position,
		const FVector2D Size)
	{
		if (!Slot) return;
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetAutoSize(false);
	}

	void StyleEndlessText(
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

	FSlateBrush MakeEndlessBrush(
		const FVector2D Size,
		const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	FButtonStyle MakeEndlessButtonStyle(
		const FVector2D Size,
		const FLinearColor& Tint)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeEndlessBrush(Size, Tint));
		Style.SetHovered(MakeEndlessBrush(Size, (Tint * 1.16f).GetClamped()));
		Style.SetPressed(MakeEndlessBrush(Size, (Tint * 0.76f).GetClamped()));
		Style.SetDisabled(MakeEndlessBrush(
			Size, FLinearColor(0.065f, 0.070f, 0.080f, 0.96f)));
		return Style;
	}

	UTextBlock* AddEndlessButtonLabel(
		UWidgetTree* Tree,
		UButton* Button,
		const TCHAR* Label,
		const int32 FontSize = 14)
	{
		if (!Tree || !Button) return nullptr;
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		Text->SetAutoWrapText(true);
		StyleEndlessText(
			Text, FontSize, FLinearColor(0.92f, 1.0f, 0.84f, 1.0f), true);
		Button->AddChild(Text);
		return Text;
	}

	FString GetFloorTypeText(const bool bElite, const bool bBoss)
	{
		if (bBoss)
		{
			return TEXT("守关 Boss");
		}
		if (bElite)
		{
			return TEXT("精英层");
		}
		return TEXT("普通层");
	}

	FString GetMaterialRewardText(
		const TArray<FImmortalMaterialStack>& Materials)
	{
		TArray<FString> Parts;
		for (const FImmortalMaterialStack& Stack : Materials)
		{
			FImmortalMaterialDefinition Definition;
			const FString Name =
				UImmortalMaterialLibrary::GetMaterialDefinition(
					Stack.MaterialId, Definition)
				? Definition.DisplayName.ToString()
				: Stack.MaterialId.ToString();
			Parts.Add(FString::Printf(
				TEXT("%s×%d"), *Name, FMath::Max(Stack.Quantity, 0)));
		}
		return Parts.IsEmpty() ? TEXT("无") : FString::Join(Parts, TEXT("、"));
	}
}

void UImmortalEndlessDungeonWidget::InitializeForPlayer(
	AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalEndlessDungeonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("EndlessDungeonScreenSize"));
	RootSize->SetWidthOverride(1600.0f);
	RootSize->SetHeightOverride(300.0f);
	WidgetTree->RootWidget = RootSize;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("EndlessDungeonScreenBackground"));
	Background->SetBrushColor(FLinearColor(0.016f, 0.032f, 0.044f, 0.992f));
	Background->SetPadding(FMargin(0.0f));
	RootSize->AddChild(Background);

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("EndlessDungeonScreenCanvas"));
	Background->AddChild(Canvas);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("EndlessDungeonScreenTitle"));
	Title->SetText(FText::FromString(TEXT("无尽秘境  [N]")));
	StyleEndlessText(Title, 23, FLinearColor(0.48f, 1.0f, 0.84f, 1.0f));
	SetEndlessLayout(
		Canvas->AddChildToCanvas(Title),
		FVector2D(16.0f, 2.0f),
		FVector2D(225.0f, 36.0f));

	HeaderSummaryText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("EndlessDungeonHeaderSummary"));
	HeaderSummaryText->SetAutoWrapText(true);
	StyleEndlessText(
		HeaderSummaryText, 13, FLinearColor(0.76f, 0.94f, 0.88f, 1.0f), true);
	SetEndlessLayout(
		Canvas->AddChildToCanvas(HeaderSummaryText),
		FVector2D(236.0f, 2.0f),
		FVector2D(1294.0f, 36.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("EndlessDungeonScreenClose"));
	CloseButton->SetStyle(MakeEndlessButtonStyle(
		FVector2D(46.0f, 32.0f), FLinearColor(0.43f, 0.14f, 0.12f, 1.0f)));
	CloseButton->OnClicked.AddDynamic(
		this, &UImmortalEndlessDungeonWidget::HandleCloseClicked);
	SetEndlessLayout(
		Canvas->AddChildToCanvas(CloseButton),
		FVector2D(1538.0f, 3.0f),
		FVector2D(46.0f, 32.0f));
	AddEndlessButtonLabel(WidgetTree, CloseButton, TEXT("×"), 22);

	UBorder* ProgressPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("EndlessDungeonProgressPanel"));
	ProgressPanel->SetBrushColor(FLinearColor(0.030f, 0.070f, 0.080f, 0.97f));
	ProgressPanel->SetPadding(FMargin(0.0f));
	SetEndlessLayout(
		Canvas->AddChildToCanvas(ProgressPanel),
		FVector2D(14.0f, 44.0f),
		FVector2D(370.0f, 243.0f));
	UCanvasPanel* ProgressCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("EndlessDungeonProgressCanvas"));
	ProgressPanel->AddChild(ProgressCanvas);

	ProgressText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("EndlessDungeonProgress"));
	ProgressText->SetAutoWrapText(true);
	StyleEndlessText(
		ProgressText, 14, FLinearColor(0.68f, 1.0f, 0.86f, 1.0f));
	SetEndlessLayout(
		ProgressCanvas->AddChildToCanvas(ProgressText),
		FVector2D(10.0f, 6.0f),
		FVector2D(350.0f, 99.0f));

	RulesText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("EndlessDungeonRules"));
	RulesText->SetAutoWrapText(true);
	StyleEndlessText(
		RulesText, 12, FLinearColor(0.82f, 0.88f, 0.90f, 1.0f));
	SetEndlessLayout(
		ProgressCanvas->AddChildToCanvas(RulesText),
		FVector2D(10.0f, 108.0f),
		FVector2D(350.0f, 126.0f));

	UBorder* RuntimePanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("EndlessDungeonRuntimePanel"));
	RuntimePanel->SetBrushColor(FLinearColor(0.038f, 0.047f, 0.078f, 0.97f));
	RuntimePanel->SetPadding(FMargin(0.0f));
	SetEndlessLayout(
		Canvas->AddChildToCanvas(RuntimePanel),
		FVector2D(394.0f, 44.0f),
		FVector2D(455.0f, 243.0f));
	UCanvasPanel* RuntimeCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("EndlessDungeonRuntimeCanvas"));
	RuntimePanel->AddChild(RuntimeCanvas);

	RuntimeText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("EndlessDungeonRuntime"));
	RuntimeText->SetAutoWrapText(true);
	StyleEndlessText(
		RuntimeText, 14, FLinearColor(0.68f, 0.86f, 1.0f, 1.0f));
	SetEndlessLayout(
		RuntimeCanvas->AddChildToCanvas(RuntimeText),
		FVector2D(10.0f, 6.0f),
		FVector2D(435.0f, 105.0f));

	FloorPreviewText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("EndlessDungeonFloorPreview"));
	FloorPreviewText->SetAutoWrapText(true);
	StyleEndlessText(
		FloorPreviewText, 12, FLinearColor(0.86f, 0.78f, 1.0f, 1.0f));
	SetEndlessLayout(
		RuntimeCanvas->AddChildToCanvas(FloorPreviewText),
		FVector2D(10.0f, 115.0f),
		FVector2D(435.0f, 119.0f));

	UBorder* RewardPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("EndlessDungeonRewardPanel"));
	RewardPanel->SetBrushColor(FLinearColor(0.035f, 0.067f, 0.050f, 0.97f));
	RewardPanel->SetPadding(FMargin(10.0f, 7.0f));
	SetEndlessLayout(
		Canvas->AddChildToCanvas(RewardPanel),
		FVector2D(859.0f, 44.0f),
		FVector2D(420.0f, 243.0f));
	RewardPreviewText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("EndlessDungeonRewardPreview"));
	RewardPreviewText->SetAutoWrapText(true);
	StyleEndlessText(
		RewardPreviewText, 13, FLinearColor(0.68f, 1.0f, 0.72f, 1.0f));
	RewardPanel->AddChild(RewardPreviewText);

	UBorder* ActionPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("EndlessDungeonActionPanel"));
	ActionPanel->SetBrushColor(FLinearColor(0.052f, 0.052f, 0.042f, 0.98f));
	ActionPanel->SetPadding(FMargin(0.0f));
	SetEndlessLayout(
		Canvas->AddChildToCanvas(ActionPanel),
		FVector2D(1289.0f, 44.0f),
		FVector2D(297.0f, 243.0f));
	UCanvasPanel* ActionCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("EndlessDungeonActionCanvas"));
	ActionPanel->AddChild(ActionCanvas);

	StartOrExitButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("EndlessDungeonStartOrExitButton"));
	StartOrExitButton->OnClicked.AddDynamic(
		this, &UImmortalEndlessDungeonWidget::HandleStartOrExitClicked);
	SetEndlessLayout(
		ActionCanvas->AddChildToCanvas(StartOrExitButton),
		FVector2D(11.0f, 12.0f),
		FVector2D(275.0f, 52.0f));
	StartOrExitButtonText = AddEndlessButtonLabel(
		WidgetTree, StartOrExitButton, TEXT("开始挑战"), 17);

	RetryRewardsButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("EndlessDungeonRetryRewardsButton"));
	RetryRewardsButton->SetStyle(MakeEndlessButtonStyle(
		FVector2D(275.0f, 42.0f), FLinearColor(0.38f, 0.31f, 0.08f, 1.0f)));
	RetryRewardsButton->OnClicked.AddDynamic(
		this, &UImmortalEndlessDungeonWidget::HandleRetryRewardsClicked);
	SetEndlessLayout(
		ActionCanvas->AddChildToCanvas(RetryRewardsButton),
		FVector2D(11.0f, 72.0f),
		FVector2D(275.0f, 42.0f));
	RetryRewardsButtonText = AddEndlessButtonLabel(
		WidgetTree, RetryRewardsButton, TEXT("暂无待领奖励"), 13);

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("EndlessDungeonOperationResult"));
	ResultText->SetAutoWrapText(true);
	StyleEndlessText(
		ResultText, 12, FLinearColor(0.76f, 0.84f, 0.86f, 1.0f), true);
	SetEndlessLayout(
		ActionCanvas->AddChildToCanvas(ResultText),
		FVector2D(11.0f, 123.0f),
		FVector2D(275.0f, 107.0f));

	RefreshFromPlayer();
}

void UImmortalEndlessDungeonWidget::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Player.IsValid()) return;

	RuntimeRefreshAccumulator += FMath::Max(InDeltaTime, 0.0f);
	const FImmortalEndlessDungeonRuntimeSnapshot Snapshot =
		Player->GetEndlessDungeonRuntimeSnapshot();
	const bool bStaticStateChanged =
		Player->GetEndlessDungeonRevision() != LastRevision
		|| Snapshot.bActive != bLastRunActive
		|| Snapshot.Floor != LastRuntimeFloor
		|| Snapshot.Kills != LastRuntimeKills;
	if (bStaticStateChanged || RuntimeRefreshAccumulator >= 0.10f)
	{
		RefreshFromPlayer();
	}
}

void UImmortalEndlessDungeonWidget::RefreshFromPlayer()
{
	if (!Player.IsValid() || !HeaderSummaryText)
	{
		return;
	}

	const FImmortalEndlessDungeonRuntimeSnapshot Snapshot =
		Player->GetEndlessDungeonRuntimeSnapshot();
	LastRevision = Player->GetEndlessDungeonRevision();
	LastRuntimeFloor = Snapshot.Floor;
	LastRuntimeKills = Snapshot.Kills;
	bLastRunActive = Snapshot.bActive;
	RuntimeRefreshAccumulator = 0.0f;

	RefreshProgressAndRules();
	RefreshRuntimeAndRewardPreview();
	RefreshActions();
}

void UImmortalEndlessDungeonWidget::RefreshProgressAndRules()
{
	if (!Player.IsValid()) return;

	const FImmortalEndlessDungeonState State =
		Player->GetEndlessDungeonState();
	const FImmortalEndlessDungeonRuntimeSnapshot Snapshot =
		Player->GetEndlessDungeonRuntimeSnapshot();
	const int32 CheckpointStart =
		UImmortalEndlessDungeonLibrary::GetCheckpointStartFloor(State);
	const int32 PendingCount = State.PendingRewards.Num();

	if (HeaderSummaryText)
	{
		HeaderSummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("最高 %d 层  ·  检查点 %d 层  ·  累计通关 %lld 层  ·  挑战 %d 次  ·  待领奖 %d 份%s"),
			FMath::Max(State.HighestClearedFloor, 0),
			FMath::Max(CheckpointStart, 1),
			State.TotalFloorsCleared,
			FMath::Max(State.TotalRuns, 0),
			PendingCount,
			Snapshot.bActive ? TEXT("  ·  挑战进行中") : TEXT(""))));
	}
	if (ProgressText)
	{
		ProgressText->SetText(FText::FromString(FString::Printf(
			TEXT("秘境记录\n最高通关：第 %d 层\n已解锁检查点：第 %d 层起步\n累计通关：%lld 层    挑战次数：%d"),
			FMath::Max(State.HighestClearedFloor, 0),
			FMath::Max(CheckpointStart, 1),
			State.TotalFloorsCleared,
			FMath::Max(State.TotalRuns, 0))));
	}
	if (RulesText)
	{
		RulesText->SetText(FText::FromString(
			TEXT("秘境规则\n"
				"普通层：击败 3 只妖兽\n"
				"每 5 层：精英层；每 10 层：守关 Boss\n"
				"通关自动进入下一层，死亡或主动退出结束本轮\n"
				"不改变地图 1–999 关进度，不奖励修为")));
	}
}

void UImmortalEndlessDungeonWidget::RefreshRuntimeAndRewardPreview()
{
	if (!Player.IsValid()) return;

	const FImmortalEndlessDungeonState State =
		Player->GetEndlessDungeonState();
	const FImmortalEndlessDungeonRuntimeSnapshot Snapshot =
		Player->GetEndlessDungeonRuntimeSnapshot();
	const int32 PreviewFloor = Snapshot.bActive
		? FMath::Max(Snapshot.Floor, 1)
		: UImmortalEndlessDungeonLibrary::GetCheckpointStartFloor(State);

	if (RuntimeText)
	{
		if (Snapshot.bActive)
		{
			const float HealthPercent = Snapshot.MaximumHealth > 0.0f
				? FMath::Clamp(
					Snapshot.CurrentHealth / Snapshot.MaximumHealth,
					0.0f,
					1.0f) * 100.0f
				: 0.0f;
			const FString BossPart = Snapshot.bBoss
				? FString::Printf(
					TEXT("    阶段 %d/3    当前敌人生命 %.0f%%"),
					FMath::Clamp(Snapshot.BossPhase, 1, 3),
					HealthPercent)
				: FString();
			RuntimeText->SetText(FText::FromString(FString::Printf(
				TEXT("挑战进行中\n第 %d 层 · %s\n击杀 %d / %d%s\n本轮起点：第 %d 层    已用时：%.1f 秒"),
				FMath::Max(Snapshot.Floor, 1),
				*GetFloorTypeText(Snapshot.bElite, Snapshot.bBoss),
				FMath::Max(Snapshot.Kills, 0),
				FMath::Max(Snapshot.RequiredKills, 1),
				*BossPart,
				FMath::Max(Snapshot.RunStartFloor, 1),
				FMath::Max(Snapshot.ElapsedSeconds, 0.0f))));
			RuntimeText->SetColorAndOpacity(FSlateColor(
				Snapshot.bBoss
					? FLinearColor(1.0f, 0.45f, 0.24f, 1.0f)
					: FLinearColor(0.62f, 0.90f, 1.0f, 1.0f)));
		}
		else
		{
			RuntimeText->SetText(FText::FromString(FString::Printf(
				TEXT("当前未挑战\n下次从第 %d 层检查点开始\n进入后自动战斗并连续推进\n退出不会回退已经保存的最高纪录"),
				FMath::Max(PreviewFloor, 1))));
			RuntimeText->SetColorAndOpacity(
				FSlateColor(FLinearColor(0.68f, 0.86f, 1.0f, 1.0f)));
		}
	}

	FImmortalEndlessFloorDescriptor Descriptor;
	if (!UImmortalEndlessDungeonLibrary::GetFloorDescriptor(
		PreviewFloor, Descriptor))
	{
		if (FloorPreviewText)
		{
			FloorPreviewText->SetText(
				FText::FromString(TEXT("当前层数配置不可用，请稍后重试。")));
		}
		if (RewardPreviewText)
		{
			RewardPreviewText->SetText(
				FText::FromString(TEXT("奖励预览暂不可用。")));
		}
		return;
	}

	if (FloorPreviewText)
	{
		FloorPreviewText->SetText(FText::FromString(FString::Printf(
			TEXT("第 %d 层预览 · %s\n"
				"目标：击败 %d 只敌人\n"
				"强度：生命 ×%.2f  ·  攻击 ×%.2f  ·  防御 +%.1f\n"
				"装备等级：%d"),
			FMath::Max(PreviewFloor, 1),
			*GetFloorTypeText(Descriptor.bElite, Descriptor.bBoss),
			FMath::Max(Descriptor.RequiredKills, 1),
			FMath::Max(Descriptor.HealthMultiplier, 0.0f),
			FMath::Max(Descriptor.AttackMultiplier, 0.0f),
			FMath::Max(Descriptor.DefenseBonus, 0.0f),
			FMath::Max(Descriptor.EquipmentItemLevel, 1))));
	}
	if (RewardPreviewText)
	{
		const bool bCheckpointReplay =
			PreviewFloor >= 1
			&& PreviewFloor <= State.HighestClearedFloor;
		if (bCheckpointReplay)
		{
			const int32 MaximumFloor = FMath::Max(
				UImmortalEndlessDungeonLibrary::GetRules().MaximumFloor,
				1);
			if (State.HighestClearedFloor >= MaximumFloor)
			{
				RewardPreviewText->SetText(FText::FromString(
					FString::Printf(
						TEXT("\u68C0\u67E5\u70B9\u91CD\u6253\n"
							"\u7B2C %d \u5C42\u7684\u5956\u52B1\u5DF2\u9886\u53D6\uFF1B\u672C\u8F6E\u9700\u91CD\u65B0\u901A\u8FC7\uFF0C\u4F46\u4E0D\u4F1A\u91CD\u590D\u83B7\u5F97\u88C5\u5907\u3001\u7075\u77F3\u6216\u6750\u6599\u3002\n"
							"\u5F53\u524D\u7248\u672C\u6700\u9AD8 %d \u5C42\u5DF2\u5168\u90E8\u5B8C\u6210\uFF1B\u4FEE\u4E3A\u4ECD\u7531\u72EC\u7ACB\u4FEE\u70BC\u7CFB\u7EDF\u7ED3\u7B97\u3002"),
						PreviewFloor,
						MaximumFloor)));
				return;
			}
			RewardPreviewText->SetText(FText::FromString(FString::Printf(
				TEXT("\u68C0\u67E5\u70B9\u91CD\u6253\n"
					"\u7B2C %d \u5C42\u7684\u5956\u52B1\u5DF2\u9886\u53D6\uFF1B\u672C\u8F6E\u9700\u91CD\u65B0\u901A\u8FC7\uFF0C\u4F46\u4E0D\u4F1A\u91CD\u590D\u83B7\u5F97\u88C5\u5907\u3001\u7075\u77F3\u6216\u6750\u6599\u3002\n"
					"\u63A8\u8FDB\u5230\u7B2C %d \u5C42\u540E\u624D\u7EE7\u7EED\u53D1\u653E\u65B0\u5956\u52B1\uFF1B\u4FEE\u4E3A\u4ECD\u7531\u72EC\u7ACB\u4FEE\u70BC\u7CFB\u7EDF\u7ED3\u7B97\u3002"),
				PreviewFloor,
				FMath::Clamp(
					State.HighestClearedFloor + 1,
					1,
					MaximumFloor))));
			return;
		}
		const FString MilestoneText = Descriptor.bBoss
			? TEXT("本层是里程碑守关层：胜利后解锁下一段检查点。")
			: TEXT("每 10 层守关成功后解锁下一检查点，奖励随层数提升。");
		RewardPreviewText->SetText(FText::FromString(FString::Printf(
			TEXT("本层奖励预览\n"
				"装备 ×%d（%s及以上）\n"
				"灵石 +%d\n"
				"材料：%s\n\n"
				"里程碑奖励\n%s\n"
				"奖励不包含修为；背包无法接收时进入待领奖。"),
			FMath::Max(Descriptor.EquipmentCount, 0),
			*UImmortalEquipmentLibrary::GetQualityText(
				Descriptor.MinimumEquipmentQuality).ToString(),
			FMath::Max(Descriptor.SpiritStones, 0),
			*GetMaterialRewardText(Descriptor.Materials),
			*MilestoneText)));
	}
}

void UImmortalEndlessDungeonWidget::RefreshActions()
{
	if (!Player.IsValid()) return;

	const FImmortalEndlessDungeonState State =
		Player->GetEndlessDungeonState();
	const FImmortalEndlessDungeonRuntimeSnapshot Snapshot =
		Player->GetEndlessDungeonRuntimeSnapshot();
	const int32 CheckpointStart =
		UImmortalEndlessDungeonLibrary::GetCheckpointStartFloor(State);
	const int32 PendingCount = State.PendingRewards.Num();

	if (StartOrExitButton && StartOrExitButtonText)
	{
		const bool bCanStart = Snapshot.bActive || PendingCount == 0;
		StartOrExitButton->SetIsEnabled(bCanStart);
		StartOrExitButton->SetStyle(MakeEndlessButtonStyle(
			FVector2D(275.0f, 52.0f),
			Snapshot.bActive
				? FLinearColor(0.56f, 0.14f, 0.11f, 1.0f)
				: FLinearColor(0.12f, 0.46f, 0.31f, 1.0f)));
		StartOrExitButtonText->SetText(FText::FromString(
			Snapshot.bActive
				? TEXT("退出当前秘境")
				: (PendingCount > 0
					? TEXT("请先领取待发奖励")
					: FString::Printf(
						TEXT("从第 %d 层开始"), FMath::Max(CheckpointStart, 1)))));
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

void UImmortalEndlessDungeonWidget::SetResultMessage(
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

void UImmortalEndlessDungeonWidget::HandleStartOrExitClicked()
{
	if (!Player.IsValid()) return;

	const FImmortalEndlessDungeonRuntimeSnapshot Snapshot =
		Player->GetEndlessDungeonRuntimeSnapshot();
	const auto Result = Snapshot.bActive
		? Player->CancelEndlessDungeon()
		: Player->StartEndlessDungeon(0);
	SetResultMessage(Result.Message, Result.bSucceeded);
	if (!Snapshot.bActive && Result.bSucceeded)
	{
		// Reveal the bottom-strip battle immediately after a successful start.
		Player->ToggleEndlessDungeon();
		return;
	}
	RefreshFromPlayer();
}

void UImmortalEndlessDungeonWidget::HandleRetryRewardsClicked()
{
	if (!Player.IsValid()) return;

	const bool bSucceeded = Player->RetryPendingEndlessDungeonRewards();
	SetResultMessage(
		FText::FromString(
			bSucceeded
				? TEXT("待领奖励已全部发放。")
				: TEXT("仍有奖励未能发放，请整理背包后重试。")),
		bSucceeded);
	RefreshFromPlayer();
}

void UImmortalEndlessDungeonWidget::HandleCloseClicked()
{
	if (Player.IsValid())
	{
		Player->ToggleEndlessDungeon();
	}
}
