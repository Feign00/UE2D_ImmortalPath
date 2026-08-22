// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalQuestWidget.h"

#include "ImmortalQuestEntryWidget.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/SlateTypes.h"

namespace
{
	void SetQuestLayout(UCanvasPanelSlot* Slot, const FVector2D Position, const FVector2D Size)
	{
		if (!Slot) return;
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetAutoSize(false);
	}

	void StyleQuestText(UTextBlock* Text, const int32 FontSize, const FLinearColor& Color, const bool bCenter = false)
	{
		if (!Text) return;
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor::Black);
		Text->SetJustification(bCenter ? ETextJustify::Center : ETextJustify::Left);
	}

	FSlateBrush MakeQuestBrush(const FVector2D Size, const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.CornerRadii = FVector4(4.0f);
		return Brush;
	}

	FButtonStyle QuestButtonStyle(const bool bSelected)
	{
		const FLinearColor Base = bSelected
			? FLinearColor(0.42f, 0.28f, 0.08f, 1.0f)
			: FLinearColor(0.06f, 0.13f, 0.15f, 0.96f);
		FButtonStyle Style;
		Style.SetNormal(MakeQuestBrush(FVector2D(130.0f, 28.0f), Base));
		Style.SetHovered(MakeQuestBrush(FVector2D(130.0f, 28.0f), (Base * 1.20f).GetClamped()));
		Style.SetPressed(MakeQuestBrush(FVector2D(130.0f, 28.0f), (Base * 0.75f).GetClamped()));
		return Style;
	}

	UTextBlock* AddQuestButtonText(UWidgetTree* Tree, UButton* Button, const TCHAR* Label)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		StyleQuestText(Text, 12, FLinearColor(0.95f, 0.89f, 0.70f, 1.0f), true);
		Button->AddChild(Text);
		return Text;
	}
}

void UImmortalQuestWidget::InitializeForPlayer(AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	LastQuestRevision = MIN_int32;
	RefreshFromPlayer();
}

void UImmortalQuestWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("QuestLogicalSize"));
	Root->SetWidthOverride(1286.0f);
	Root->SetHeightOverride(238.0f);
	WidgetTree->RootWidget = Root;

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("QuestCanvas"));
	Root->AddChild(Canvas);

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("QuestBackground"));
	Background->SetBrushColor(FLinearColor(0.025f, 0.07f, 0.075f, 0.84f));
	Background->SetPadding(FMargin(0.0f));
	SetQuestLayout(Canvas->AddChildToCanvas(Background), FVector2D::ZeroVector, FVector2D(1286.0f, 238.0f));

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("QuestTitle"));
	Title->SetText(FText::FromString(TEXT("仙途任务")));
	StyleQuestText(Title, 18, FLinearColor(0.96f, 0.82f, 0.48f, 1.0f));
	SetQuestLayout(Canvas->AddChildToCanvas(Title), FVector2D(12.0f, 3.0f), FVector2D(190.0f, 30.0f));

	MainButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("QuestMainTab"));
	MainButton->OnClicked.AddDynamic(this, &UImmortalQuestWidget::HandleMainClicked);
	AddQuestButtonText(WidgetTree, MainButton, TEXT("主线任务"));
	SetQuestLayout(Canvas->AddChildToCanvas(MainButton), FVector2D(205.0f, 3.0f), FVector2D(130.0f, 28.0f));

	DailyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("QuestDailyTab"));
	DailyButton->OnClicked.AddDynamic(this, &UImmortalQuestWidget::HandleDailyClicked);
	AddQuestButtonText(WidgetTree, DailyButton, TEXT("每日任务"));
	SetQuestLayout(Canvas->AddChildToCanvas(DailyButton), FVector2D(341.0f, 3.0f), FVector2D(130.0f, 28.0f));

	AchievementButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("QuestAchievementTab"));
	AchievementButton->OnClicked.AddDynamic(this, &UImmortalQuestWidget::HandleAchievementClicked);
	AddQuestButtonText(WidgetTree, AchievementButton, TEXT("成就任务"));
	SetQuestLayout(Canvas->AddChildToCanvas(AchievementButton), FVector2D(477.0f, 3.0f), FVector2D(130.0f, 28.0f));

	SummaryText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("QuestSummary"));
	StyleQuestText(SummaryText, 11, FLinearColor(0.70f, 0.92f, 0.86f, 1.0f), true);
	SetQuestLayout(Canvas->AddChildToCanvas(SummaryText), FVector2D(625.0f, 4.0f), FVector2D(645.0f, 27.0f));

	QuestList = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(), TEXT("QuestScrollableList"));
	QuestList->SetAnimateWheelScrolling(true);
	QuestList->SetScrollBarVisibility(ESlateVisibility::Visible);
	SetQuestLayout(Canvas->AddChildToCanvas(QuestList), FVector2D(12.0f, 36.0f), FVector2D(1260.0f, 170.0f));

	UBorder* ResultBar = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("QuestResultBar"));
	ResultBar->SetBrushColor(FLinearColor(0.04f, 0.10f, 0.10f, 0.96f));
	ResultBar->SetPadding(FMargin(8.0f, 1.0f));
	SetQuestLayout(Canvas->AddChildToCanvas(ResultBar), FVector2D(12.0f, 210.0f), FVector2D(1260.0f, 24.0f));

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("QuestResultText"));
	ResultText->SetText(FText::FromString(TEXT("任务进度由挂机战斗与养成功能自动记录；点击已完成任务即可领取。")));
	StyleQuestText(ResultText, 10, FLinearColor(0.82f, 0.88f, 0.82f, 1.0f), true);
	ResultBar->AddChild(ResultText);

	RefreshCategoryButtons();
	RebuildQuestList();
}

void UImmortalQuestWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccumulator += FMath::Max(InDeltaTime, 0.0f);
	if (RefreshAccumulator < 0.35f || !Player.IsValid()) return;
	RefreshAccumulator = 0.0f;
	if (Player->GetQuestRevision() != LastQuestRevision)
	{
		RefreshFromPlayer();
	}
}

void UImmortalQuestWidget::RefreshFromPlayer()
{
	if (!Player.IsValid()) return;
	const FImmortalQuestState State = Player->GetQuestState();
	LastQuestRevision = State.Revision;
	if (SummaryText)
	{
		SummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("今日 %d · 累计领取 %lld · 击败妖物 %lld · 通关地图 %lld"),
			State.DailyDayKey,
			State.TotalClaims,
			State.LifetimeCounters.MonsterKills,
			State.LifetimeCounters.MapCompletions)));
	}
	RebuildQuestList();
}

void UImmortalQuestWidget::SelectCategory(const EImmortalQuestCategory Category)
{
	if (SelectedCategory == Category && QuestList && QuestList->GetChildrenCount() > 0) return;
	SelectedCategory = Category;
	RefreshCategoryButtons();
	RebuildQuestList();
}

void UImmortalQuestWidget::RefreshCategoryButtons()
{
	if (MainButton) MainButton->SetStyle(QuestButtonStyle(SelectedCategory == EImmortalQuestCategory::Main));
	if (DailyButton) DailyButton->SetStyle(QuestButtonStyle(SelectedCategory == EImmortalQuestCategory::Daily));
	if (AchievementButton) AchievementButton->SetStyle(QuestButtonStyle(SelectedCategory == EImmortalQuestCategory::Achievement));
}

void UImmortalQuestWidget::RebuildQuestList()
{
	if (!QuestList) return;
	QuestList->ClearChildren();
	const FImmortalQuestState State = Player.IsValid()
		? Player->GetQuestState() : FImmortalQuestState();
	for (const FImmortalQuestDefinition& Definition :
		UImmortalQuestLibrary::GetQuestDefinitions(SelectedCategory))
	{
		UImmortalQuestEntryWidget* Entry = CreateWidget<UImmortalQuestEntryWidget>(
			this, UImmortalQuestEntryWidget::StaticClass());
		if (!Entry) continue;
		Entry->Configure(this, Definition,
			UImmortalQuestLibrary::GetProgress(State, Definition.QuestId));
		QuestList->AddChild(Entry);
	}
	QuestList->ScrollToStart();
}

void UImmortalQuestWidget::ClaimQuest(const FName QuestId)
{
	if (!Player.IsValid()) return;
	const FImmortalQuestClaimResult Result = Player->ClaimQuest(QuestId);
	if (ResultText)
	{
		ResultText->SetText(Result.Message);
		ResultText->SetColorAndOpacity(FSlateColor(Result.bSucceeded
			? FLinearColor(0.58f, 1.0f, 0.66f, 1.0f)
			: FLinearColor(1.0f, 0.58f, 0.42f, 1.0f)));
	}
	RefreshFromPlayer();
}

void UImmortalQuestWidget::HandleMainClicked()
{
	SelectCategory(EImmortalQuestCategory::Main);
}

void UImmortalQuestWidget::HandleDailyClicked()
{
	SelectCategory(EImmortalQuestCategory::Daily);
}

void UImmortalQuestWidget::HandleAchievementClicked()
{
	SelectCategory(EImmortalQuestCategory::Achievement);
}

