// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalSectWidget.h"
#include "ImmortalFeaturePageLayout.h"
#include "ImmortalUITheme.h"
#include "ImmortalSectArt.h"
#include "Components/Image.h"
#include "Components/ButtonSlot.h"
#include "Components/ProgressBar.h"

#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Maps/ImmortalMapTypes.h"
#include "../Sects/ImmortalSectTypes.h"
#include "../Techniques/ImmortalTechniqueTypes.h"
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
	constexpr int32 VisibleSectCount = 4;
	constexpr int32 VisibleTaskCount = 3;
	constexpr int32 VisibleOfferCount = 4;

	void SetSectLayout(UCanvasPanelSlot* Slot, const FVector2D Position, const FVector2D Size)
	{
		if (!Slot) return;
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetAutoSize(false);
	}

	void StyleSectText(
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

	FSlateBrush MakeSectBrush(const FVector2D Size, const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	FButtonStyle MakeSectButtonStyle(const FVector2D Size, const FLinearColor& Tint)
	{
		FButtonStyle Style = ImmortalUITheme::ButtonStyle();
		Style.SetNormal(ImmortalUITheme::PanelBrush(Tint));
		return Style;
	}

	UTextBlock* AddSectButtonLabel(
		UWidgetTree* Tree,
		UButton* Button,
		const TCHAR* Label,
		const int32 FontSize = 13)
	{
		if (!Tree || !Button) return nullptr;
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		Text->SetAutoWrapText(true);
		StyleSectText(Text, FontSize, FLinearColor(1.0f, 0.91f, 0.66f, 1.0f), true);
		Button->AddChild(Text);
		ImmortalFeaturePageLayout::StabilizeButtonLabel(Button, Text);
		return Text;
	}

	FString GetTechniqueDisplayName(const FName TechniqueId)
	{
		if (TechniqueId.IsNone()) return TEXT("无");
		FImmortalTechniqueDefinition Definition;
		return UImmortalTechniqueLibrary::GetTechniqueDefinition(TechniqueId, Definition)
			? Definition.DisplayName.ToString()
			: TechniqueId.ToString();
	}

	FString GetSectRequirementText(const FImmortalSectDefinition& Definition)
	{
		FString MapName = Definition.RequiredMapId.ToString();
		FImmortalMapDefinition MapDefinition;
		if (UImmortalMapLibrary::GetMapDefinition(Definition.RequiredMapId, MapDefinition))
		{
			MapName = MapDefinition.DisplayName.ToString();
		}
		return FString::Printf(
			TEXT("门槛：%s · %s第%d关"),
			*UImmortalMapLibrary::GetRealmRequirementText(Definition.RequiredRealmIndex).ToString(),
			*MapName,
			FMath::Max(Definition.RequiredMapStage, 1));
	}

	FString GetOfferLimitText(
		const FImmortalSectStoreOfferDefinition& Definition,
		const FImmortalSectOfferProgress& Progress)
	{
		if (Definition.bOneTime)
		{
			return Progress.TotalPurchaseCount > 0 ? TEXT("已经兑换") : TEXT("限兑一次");
		}
		if (Definition.DailyLimit > 0)
		{
			return FString::Printf(
				TEXT("今日 %d/%d"),
				FMath::Max(Progress.DailyPurchaseCount, 0),
				Definition.DailyLimit);
		}
		return TEXT("不限次数");
	}
}

void UImmortalSectWidget::InitializeForPlayer(AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalSectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("SectScreenSize"));
	RootSize->SetWidthOverride(1600.0f);
	RootSize->SetHeightOverride(600.0f);
	WidgetTree->RootWidget = RootSize;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("SectScreenBackground"));
	Background->SetBrushColor(FLinearColor(0.029f, 0.036f, 0.060f, 0.988f));
	Background->SetPadding(FMargin(0.0f));
	RootSize->AddChild(Background);

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("SectScreenCanvas"));
	Background->AddChild(Canvas);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SectScreenTitle"));
	Title->SetText(FText::FromString(TEXT("四方仙门")));
	StyleSectText(Title, 28, FLinearColor(0.87f, 0.71f, 1.0f, 1.0f));
	SetSectLayout(Canvas->AddChildToCanvas(Title), FVector2D(18.0f, 3.0f), FVector2D(170.0f, 36.0f));

	HeaderSummaryText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SectHeaderSummary"));
	HeaderSummaryText->SetAutoWrapText(true);
	StyleSectText(HeaderSummaryText, 18, FLinearColor(0.93f, 0.88f, 0.70f, 1.0f), true);
	SetSectLayout(
		Canvas->AddChildToCanvas(HeaderSummaryText),
		FVector2D(220.0f, 3.0f),
		FVector2D(1308.0f, 52.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("SectScreenClose"));
	CloseButton->SetStyle(MakeSectButtonStyle(
		FVector2D(48.0f, 36.0f), FLinearColor(0.42f, 0.17f, 0.15f, 1.0f)));
	CloseButton->OnClicked.AddDynamic(this, &UImmortalSectWidget::HandleCloseClicked);
	SetSectLayout(
		Canvas->AddChildToCanvas(CloseButton),
		FVector2D(1542.0f, 3.0f),
		FVector2D(48.0f, 36.0f));
	AddSectButtonLabel(WidgetTree, CloseButton, TEXT("×"), 20);

	UBorder* ChoicePanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("SectChoicePanel"));
	ChoicePanel->SetBrushColor(FLinearColor(0.052f, 0.058f, 0.092f, 0.97f));
	ChoicePanel->SetPadding(FMargin(0.0f));
	SetSectLayout(
		Canvas->AddChildToCanvas(ChoicePanel),
		FVector2D(8.0f, 64.0f),
		FVector2D(400.0f, 528.0f));

	UCanvasPanel* ChoiceCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("SectChoiceCanvas"));
	ChoicePanel->AddChild(ChoiceCanvas);

	UTextBlock* ChoiceTitle = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SectChoiceTitle"));
	ChoiceTitle->SetText(FText::FromString(TEXT("四方宗门")));
	StyleSectText(ChoiceTitle, 18, FLinearColor::White);
	SetSectLayout(
		ChoiceCanvas->AddChildToCanvas(ChoiceTitle),
		FVector2D(8.0f, 3.0f),
		FVector2D(150.0f, 24.0f));

	for (int32 SectIndex = 0; SectIndex < VisibleSectCount; ++SectIndex)
	{
		UButton* SectButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), *FString::Printf(TEXT("SectChoiceButton%d"), SectIndex));
		SectButton->SetStyle(MakeSectButtonStyle(
			FVector2D(149.0f, 38.0f), FLinearColor(0.10f, 0.12f, 0.19f, 0.98f)));
		switch (SectIndex)
		{
		case 0: SectButton->OnClicked.AddDynamic(this, &UImmortalSectWidget::HandleSect0Clicked); break;
		case 1: SectButton->OnClicked.AddDynamic(this, &UImmortalSectWidget::HandleSect1Clicked); break;
		case 2: SectButton->OnClicked.AddDynamic(this, &UImmortalSectWidget::HandleSect2Clicked); break;
		case 3: SectButton->OnClicked.AddDynamic(this, &UImmortalSectWidget::HandleSect3Clicked); break;
		default: break;
		}
		const int32 Column = SectIndex % 2;
		const int32 Row = SectIndex / 2;
		SetSectLayout(
			ChoiceCanvas->AddChildToCanvas(SectButton),
			FVector2D(12.0f + Column * 190.0f, 34.0f + Row * 118.0f),
			FVector2D(184.0f, 112.0f));
		SectButtons.Add(SectButton);
		UTextBlock* Label = AddSectButtonLabel(WidgetTree, SectButton, TEXT("宗门"), 18);
		SectButtonLabels.Add(Label);
		Label->RemoveFromParent();
		UCanvasPanel* ButtonCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		ButtonCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
		UButtonSlot* ContentSlot = CastChecked<UButtonSlot>(SectButton->AddChild(ButtonCanvas));
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
		ContentSlot->SetPadding(FMargin(0));
		SetSectLayout(ButtonCanvas->AddChildToCanvas(Label), {4, 78}, {168, 24});
		UImage* Emblem = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
			*FString::Printf(TEXT("SectEmblem%d"), SectIndex));
		Emblem->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetSectLayout(ButtonCanvas->AddChildToCanvas(Emblem), {52, 2}, {72, 72});
		SectImages.Add(Emblem);
	}

	SelectedSectText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SectSelectedDescription"));
	SelectedSectText->SetAutoWrapText(true);
	StyleSectText(SelectedSectText, 18, FLinearColor(0.85f, 0.87f, 0.96f, 1.0f));
	SetSectLayout(
		ChoiceCanvas->AddChildToCanvas(SelectedSectText),
		FVector2D(12.0f, 278.0f),
		FVector2D(376.0f, 146.0f));

	JoinButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("SectJoinButton"));
	JoinButton->SetStyle(MakeSectButtonStyle(
		FVector2D(314.0f, 32.0f), FLinearColor(0.34f, 0.23f, 0.56f, 1.0f)));
	JoinButton->OnClicked.AddDynamic(this, &UImmortalSectWidget::HandleJoinClicked);
	SetSectLayout(
		ChoiceCanvas->AddChildToCanvas(JoinButton),
		FVector2D(12.0f, 432.0f),
		FVector2D(376.0f, 46.0f));
	JoinButtonText = AddSectButtonLabel(WidgetTree, JoinButton, TEXT("加入所选宗门"), 16);

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SectOperationResult"));
	ResultText->SetAutoWrapText(false);
	StyleSectText(ResultText, 14, FLinearColor(0.66f, 0.91f, 0.73f, 1.0f), true);
	SetSectLayout(
		ChoiceCanvas->AddChildToCanvas(ResultText),
		FVector2D(12.0f, 484.0f),
		FVector2D(376.0f, 36.0f));

	UBorder* TaskPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("SectTaskPanel"));
	TaskPanel->SetBrushColor(FLinearColor(0.041f, 0.049f, 0.079f, 0.97f));
	TaskPanel->SetPadding(FMargin(0.0f));
	SetSectLayout(
		Canvas->AddChildToCanvas(TaskPanel),
		FVector2D(416.0f, 64.0f),
		FVector2D(566.0f, 528.0f));

	UCanvasPanel* TaskCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("SectTaskCanvas"));
	TaskPanel->AddChild(TaskCanvas);

	UTextBlock* TaskPanelTitle = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SectTaskPanelTitle"));
	TaskPanelTitle->SetText(FText::FromString(TEXT("每日任务 · 完成后领取贡献")));
	StyleSectText(TaskPanelTitle, 18, FLinearColor(0.70f, 0.89f, 1.0f, 1.0f));
	SetSectLayout(
		TaskCanvas->AddChildToCanvas(TaskPanelTitle),
		FVector2D(8.0f, 3.0f),
		FVector2D(550.0f, 28.0f));

	for (int32 TaskIndex = 0; TaskIndex < VisibleTaskCount; ++TaskIndex)
	{
		const FVector2D CardPosition(6.0f, 38.0f + TaskIndex * 160.0f);
		UBorder* TaskBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), *FString::Printf(TEXT("SectTaskBorder%d"), TaskIndex));
		TaskBorder->SetBrushColor(FLinearColor(0.074f, 0.086f, 0.13f, 0.98f));
		TaskBorder->SetPadding(FMargin(0.0f));
		SetSectLayout(TaskCanvas->AddChildToCanvas(TaskBorder), CardPosition, FVector2D(554.0f, 154.0f));
		TaskBorders.Add(TaskBorder);

		UCanvasPanel* CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), *FString::Printf(TEXT("SectTaskCardCanvas%d"), TaskIndex));
		TaskBorder->AddChild(CardCanvas);

		UTextBlock* TaskTitle = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("SectTaskTitle%d"), TaskIndex));
		StyleSectText(TaskTitle, 20, FLinearColor::White);
		SetSectLayout(
			CardCanvas->AddChildToCanvas(TaskTitle),
			FVector2D(90.0f, 8.0f),
			FVector2D(452.0f, 28.0f));
		TaskTitleTexts.Add(TaskTitle);

		UTextBlock* TaskDetail = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("SectTaskDetail%d"), TaskIndex));
		TaskDetail->SetAutoWrapText(true);
		StyleSectText(TaskDetail, 18, FLinearColor(0.78f, 0.82f, 0.92f, 1.0f));
		SetSectLayout(
			CardCanvas->AddChildToCanvas(TaskDetail),
			FVector2D(90.0f, 40.0f),
			FVector2D(452.0f, 64.0f));
		TaskDetailTexts.Add(TaskDetail);
		UImmortalIconWidget* TaskIcon = CreateWidget<UImmortalIconWidget>(this);
		const int32 TaskIcons[] = {4, 8, 13};
		TaskIcon->SetIcon(TaskIcons[TaskIndex]);
		SetSectLayout(CardCanvas->AddChildToCanvas(TaskIcon), {12, 16}, {64, 64});
		UProgressBar* ProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),
			*FString::Printf(TEXT("SectTaskProgress%d"), TaskIndex));
		FProgressBarStyle ProgressStyle;
		ProgressStyle.SetBackgroundImage(MakeSectBrush({320, 14}, FLinearColor(0.018f, 0.025f, 0.031f)));
		ProgressStyle.SetFillImage(MakeSectBrush({320, 14}, FLinearColor::White));
		ProgressBar->SetWidgetStyle(ProgressStyle);
		ProgressBar->SetFillColorAndOpacity(FLinearColor(0.30f, 0.75f, 0.53f));
		SetSectLayout(CardCanvas->AddChildToCanvas(ProgressBar), {90, 122}, {316, 14});
		TaskProgressBars.Add(ProgressBar);

		UButton* TaskButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), *FString::Printf(TEXT("SectTaskAction%d"), TaskIndex));
		TaskButton->SetStyle(MakeSectButtonStyle(
			FVector2D(132.0f, 38.0f), FLinearColor(0.18f, 0.43f, 0.46f, 1.0f)));
		switch (TaskIndex)
		{
		case 0: TaskButton->OnClicked.AddDynamic(this, &UImmortalSectWidget::HandleTask0Clicked); break;
		case 1: TaskButton->OnClicked.AddDynamic(this, &UImmortalSectWidget::HandleTask1Clicked); break;
		case 2: TaskButton->OnClicked.AddDynamic(this, &UImmortalSectWidget::HandleTask2Clicked); break;
		default: break;
		}
		SetSectLayout(
			CardCanvas->AddChildToCanvas(TaskButton),
			FVector2D(420.0f, 110.0f),
			FVector2D(122.0f, 36.0f));
		TaskActionButtons.Add(TaskButton);
		TaskActionLabels.Add(AddSectButtonLabel(WidgetTree, TaskButton, TEXT("领取"), 15));
	}

	UBorder* OfferPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("SectOfferPanel"));
	OfferPanel->SetBrushColor(FLinearColor(0.048f, 0.043f, 0.071f, 0.97f));
	OfferPanel->SetPadding(FMargin(0.0f));
	SetSectLayout(
		Canvas->AddChildToCanvas(OfferPanel),
		FVector2D(990.0f, 64.0f),
		FVector2D(602.0f, 528.0f));

	UCanvasPanel* OfferCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("SectOfferCanvas"));
	OfferPanel->AddChild(OfferCanvas);

	UTextBlock* OfferPanelTitle = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SectOfferPanelTitle"));
	OfferPanelTitle->SetText(FText::FromString(TEXT("宗门宝库 · 贡献兑换")));
	StyleSectText(OfferPanelTitle, 18, FLinearColor(1.0f, 0.80f, 0.48f, 1.0f));
	SetSectLayout(
		OfferCanvas->AddChildToCanvas(OfferPanelTitle),
		FVector2D(8.0f, 3.0f),
		FVector2D(590.0f, 24.0f));

	for (int32 OfferIndex = 0; OfferIndex < VisibleOfferCount; ++OfferIndex)
	{
		const int32 Column = OfferIndex % 2;
		const int32 Row = OfferIndex / 2;
		const FVector2D CardPosition(6.0f + Column * 296.0f, 38.0f + Row * 244.0f);

		UBorder* OfferBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), *FString::Printf(TEXT("SectOfferBorder%d"), OfferIndex));
		OfferBorder->SetBrushColor(FLinearColor(0.091f, 0.074f, 0.12f, 0.98f));
		OfferBorder->SetPadding(FMargin(0.0f));
		SetSectLayout(
			OfferCanvas->AddChildToCanvas(OfferBorder),
			CardPosition,
			FVector2D(289.0f, 238.0f));
		OfferBorders.Add(OfferBorder);

		UCanvasPanel* CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), *FString::Printf(TEXT("SectOfferCardCanvas%d"), OfferIndex));
		OfferBorder->AddChild(CardCanvas);

		UTextBlock* OfferTitle = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("SectOfferTitle%d"), OfferIndex));
		OfferTitle->SetAutoWrapText(true);
		StyleSectText(OfferTitle, 18, FLinearColor::White);
		SetSectLayout(
			CardCanvas->AddChildToCanvas(OfferTitle),
			FVector2D(90.0f, 12.0f),
			FVector2D(185.0f, 62.0f));
		OfferTitleTexts.Add(OfferTitle);
		UImmortalIconWidget* OfferIcon = CreateWidget<UImmortalIconWidget>(this);
		const int32 OfferIcons[] = {7, 19, 6, 5};
		OfferIcon->SetIcon(OfferIcons[OfferIndex]);
		SetSectLayout(CardCanvas->AddChildToCanvas(OfferIcon), {12, 12}, {64, 64});

		UTextBlock* OfferDetail = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("SectOfferDetail%d"), OfferIndex));
		OfferDetail->SetAutoWrapText(true);
		StyleSectText(OfferDetail, 17, FLinearColor(0.86f, 0.80f, 0.92f, 1.0f));
		SetSectLayout(
			CardCanvas->AddChildToCanvas(OfferDetail),
			FVector2D(12.0f, 86.0f),
			FVector2D(265.0f, 88.0f));
		OfferDetailTexts.Add(OfferDetail);

		UButton* OfferButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), *FString::Printf(TEXT("SectOfferAction%d"), OfferIndex));
		OfferButton->SetStyle(MakeSectButtonStyle(
			FVector2D(132.0f, 29.0f), FLinearColor(0.50f, 0.31f, 0.16f, 1.0f)));
		switch (OfferIndex)
		{
		case 0: OfferButton->OnClicked.AddDynamic(this, &UImmortalSectWidget::HandleOffer0Clicked); break;
		case 1: OfferButton->OnClicked.AddDynamic(this, &UImmortalSectWidget::HandleOffer1Clicked); break;
		case 2: OfferButton->OnClicked.AddDynamic(this, &UImmortalSectWidget::HandleOffer2Clicked); break;
		case 3: OfferButton->OnClicked.AddDynamic(this, &UImmortalSectWidget::HandleOffer3Clicked); break;
		default: break;
		}
		SetSectLayout(
			CardCanvas->AddChildToCanvas(OfferButton),
			FVector2D(12.0f, 186.0f),
			FVector2D(265.0f, 40.0f));
		OfferActionButtons.Add(OfferButton);
		OfferActionLabels.Add(AddSectButtonLabel(WidgetTree, OfferButton, TEXT("兑换"), 15));
	}

	RefreshFromPlayer();
	ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, SelectedSectText);
	ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, ResultText);
	for (UTextBlock* Detail : TaskDetailTexts) ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, Detail);
	for (UTextBlock* Detail : OfferDetailTexts) ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, Detail);
}

void UImmortalSectWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Player.IsValid()) return;

	if (Player->GetSectRevision() != LastSectRevision
		|| Player->GetTechniqueRevision() != LastTechniqueRevision
		|| Player->GetMaterialInventoryRevision() != LastMaterialRevision
		|| Player->GetGold() != LastSpiritStones)
	{
		RefreshFromPlayer();
	}
}

void UImmortalSectWidget::RefreshFromPlayer()
{
	if (!Player.IsValid()
		|| SectButtons.Num() != VisibleSectCount
		|| TaskActionButtons.Num() != VisibleTaskCount
		|| OfferActionButtons.Num() != VisibleOfferCount)
	{
		return;
	}

	const FImmortalSectState State = Player->GetSectState();
	DisplayedSectIds = UImmortalSectLibrary::GetKnownSectIds();
	if (DisplayedSectIds.Num() > VisibleSectCount)
	{
		DisplayedSectIds.SetNum(VisibleSectCount);
	}
	if (State.HasJoined())
	{
		SelectedSectId = State.SectId;
	}
	else if (!DisplayedSectIds.Contains(SelectedSectId))
	{
		SelectedSectId = DisplayedSectIds.IsEmpty() ? NAME_None : DisplayedSectIds[0];
	}

	DisplayedTaskIds.Reset();
	const TArray<FImmortalSectTaskDefinition> TaskDefinitions =
		UImmortalSectLibrary::GetDailyTaskDefinitions();
	for (int32 TaskIndex = 0;
		TaskIndex < TaskDefinitions.Num() && TaskIndex < VisibleTaskCount;
		++TaskIndex)
	{
		DisplayedTaskIds.Add(TaskDefinitions[TaskIndex].TaskId);
	}

	DisplayedOfferIds.Reset();
	if (State.HasJoined())
	{
		const TArray<FImmortalSectStoreOfferDefinition> OfferDefinitions =
			UImmortalSectLibrary::GetStoreOfferDefinitions(State.SectId);
		for (int32 OfferIndex = 0;
			OfferIndex < OfferDefinitions.Num() && OfferIndex < VisibleOfferCount;
			++OfferIndex)
		{
			DisplayedOfferIds.Add(OfferDefinitions[OfferIndex].OfferId);
		}
	}

	LastSectRevision = Player->GetSectRevision();
	LastTechniqueRevision = Player->GetTechniqueRevision();
	LastMaterialRevision = Player->GetMaterialInventoryRevision();
	LastSpiritStones = Player->GetGold();

	if (HeaderSummaryText)
	{
		if (State.HasJoined())
		{
			FImmortalSectDefinition SectDefinition;
			const FString SectName = UImmortalSectLibrary::GetSectDefinition(
				State.SectId, SectDefinition)
				? SectDefinition.DisplayName.ToString()
				: State.SectId.ToString();
			int32 ClaimedTaskCount = 0;
			for (const FImmortalSectTaskProgress& Progress : State.DailyTasks)
			{
				ClaimedTaskCount += Progress.bClaimed ? 1 : 0;
			}
			HeaderSummaryText->SetText(FText::FromString(FString::Printf(
				TEXT("当前宗门：%s · 身份：%s · 贡献 %d · 灵石 %d\n今日任务 %d/%d 已领取 · 累计贡献 %lld"),
				*SectName,
				*UImmortalSectLibrary::GetRankName(State.TotalContributionEarned).ToString(),
				State.Contribution,
				LastSpiritStones,
				ClaimedTaskCount,
				DisplayedTaskIds.Num(),
				State.TotalContributionEarned)));
		}
		else
		{
			HeaderSummaryText->SetText(FText::FromString(FString::Printf(
				TEXT("当前宗门：尚未加入 · 身份：散修 · 贡献 %d · 灵石 %d\n选择宗门并满足入门门槛后即可接取每日任务"),
				State.Contribution,
				LastSpiritStones)));
		}
	}

	RefreshSectChoices();
	RefreshTaskCards();
	RefreshOfferCards();
}

void UImmortalSectWidget::SelectSectByIndex(const int32 SectIndex)
{
	if (!Player.IsValid()) return;
	const FImmortalSectState State = Player->GetSectState();
	if (State.HasJoined())
	{
		SetResultMessage(FText::FromString(TEXT("已经加入宗门，当前版本不可退宗或改投。")), false);
		return;
	}
	if (!DisplayedSectIds.IsValidIndex(SectIndex))
	{
		SetResultMessage(FText::FromString(TEXT("宗门资料尚未就绪。")), false);
		return;
	}

	SelectedSectId = DisplayedSectIds[SectIndex];
	SetResultMessage(FText::GetEmpty(), true);
	RefreshSectChoices();
}

void UImmortalSectWidget::HandleTaskClaimByIndex(const int32 TaskIndex)
{
	if (!Player.IsValid() || !DisplayedTaskIds.IsValidIndex(TaskIndex))
	{
		SetResultMessage(FText::FromString(TEXT("宗门任务资料尚未就绪。")), false);
		return;
	}

	const FImmortalSectTaskClaimResult Result = Player->ClaimSectTask(DisplayedTaskIds[TaskIndex]);
	RefreshFromPlayer();
	SetResultMessage(Result.Message, Result.bSucceeded && !Result.bPersistenceFailed);
}

void UImmortalSectWidget::HandleOfferExchangeByIndex(const int32 OfferIndex)
{
	if (!Player.IsValid() || !DisplayedOfferIds.IsValidIndex(OfferIndex))
	{
		SetResultMessage(FText::FromString(TEXT("宗门兑换资料尚未就绪。")), false);
		return;
	}

	const FImmortalSectExchangeResult Result = Player->ExchangeSectOffer(DisplayedOfferIds[OfferIndex]);
	RefreshFromPlayer();
	SetResultMessage(Result.Message, Result.bSucceeded && !Result.bPersistenceFailed);
}

void UImmortalSectWidget::RefreshSectChoices()
{
	if (!Player.IsValid()) return;
	const FImmortalSectState State = Player->GetSectState();

	for (int32 SectIndex = 0; SectIndex < VisibleSectCount; ++SectIndex)
	{
		if (!SectButtons.IsValidIndex(SectIndex) || !SectButtonLabels.IsValidIndex(SectIndex)) continue;
		if (!DisplayedSectIds.IsValidIndex(SectIndex))
		{
			SectButtons[SectIndex]->SetIsEnabled(false);
			SectButtonLabels[SectIndex]->SetText(FText::FromString(TEXT("宗门资料缺失")));
			continue;
		}

		FImmortalSectDefinition Definition;
		if (!UImmortalSectLibrary::GetSectDefinition(DisplayedSectIds[SectIndex], Definition))
		{
			SectButtons[SectIndex]->SetIsEnabled(false);
			SectButtonLabels[SectIndex]->SetText(FText::FromString(TEXT("未知宗门")));
			continue;
		}

		if (SectImages.IsValidIndex(SectIndex))
			SectImages[SectIndex]->SetBrush(ImmortalSectArt::Brush(SectAtlas.LoadSynchronous(), Definition.SectId));
		const bool bSelected = Definition.SectId == SelectedSectId;
		const bool bJoinedSect = State.HasJoined() && State.SectId == Definition.SectId;
		SectButtons[SectIndex]->SetStyle(MakeSectButtonStyle(
			FVector2D(149.0f, 44.0f),
			bSelected
				? Definition.DisplayColor.GetClamped(0.18f, 0.72f)
				: FLinearColor(0.10f, 0.12f, 0.19f, 0.98f)));
		SectButtons[SectIndex]->SetIsEnabled(!State.HasJoined());
		SectButtonLabels[SectIndex]->SetColorAndOpacity(FSlateColor(
			bSelected ? FLinearColor::White : Definition.DisplayColor.GetClamped(0.56f, 1.0f)));
		SectButtonLabels[SectIndex]->SetText(Definition.DisplayName);
		SectButtons[SectIndex]->SetToolTipText(FText::FromString(FString::Printf(
			TEXT("%s\n%s"),
			*Definition.DisplayName.ToString(),
			State.HasJoined()
				? (bJoinedSect ? TEXT("【本宗】") : TEXT("不可改投"))
				: (bSelected ? TEXT("【已选】") : TEXT("点击查看")))));
	}

	FImmortalSectDefinition SelectedDefinition;
	if (SelectedSectId.IsNone()
		|| !UImmortalSectLibrary::GetSectDefinition(SelectedSectId, SelectedDefinition))
	{
		if (SelectedSectText)
		{
			SelectedSectText->SetText(FText::FromString(TEXT("暂无可用宗门资料。")));
		}
		if (JoinButton) JoinButton->SetIsEnabled(false);
		if (JoinButtonText) JoinButtonText->SetText(FText::FromString(TEXT("无法加入")));
		return;
	}

	if (State.HasJoined())
	{
		if (SelectedSectText)
		{
			SelectedSectText->SetText(FText::FromString(FString::Printf(
				TEXT("%s\n真传功法：%s · 身份：%s\n已加入本宗，当前不可退宗或改投。"),
				*SelectedDefinition.Description.ToString(),
				*GetTechniqueDisplayName(SelectedDefinition.TechniqueRewardId),
				*UImmortalSectLibrary::GetRankName(State.TotalContributionEarned).ToString())));
			SelectedSectText->SetColorAndOpacity(FSlateColor(
				SelectedDefinition.DisplayColor.GetClamped(0.60f, 1.0f)));
		}
		if (JoinButton) JoinButton->SetIsEnabled(false);
		if (JoinButtonText)
		{
			JoinButtonText->SetText(FText::FromString(FString::Printf(
				TEXT("已加入%s · 不可改投"), *SelectedDefinition.DisplayName.ToString())));
		}
		return;
	}

	const FImmortalSectJoinResult Evaluation = Player->EvaluateJoinSect(SelectedSectId);
	if (SelectedSectText)
	{
		SelectedSectText->SetText(FText::FromString(FString::Printf(
			TEXT("%s\n%s\n%s"),
			*SelectedDefinition.Description.ToString(),
			*GetSectRequirementText(SelectedDefinition),
			*Evaluation.Message.ToString())));
		SelectedSectText->SetColorAndOpacity(FSlateColor(
			SelectedDefinition.DisplayColor.GetClamped(0.60f, 1.0f)));
	}
	if (JoinButton) JoinButton->SetIsEnabled(Evaluation.bCanJoin);
	if (JoinButtonText)
	{
		FString ButtonLabel;
		if (Evaluation.bCanJoin)
		{
			ButtonLabel = FString::Printf(TEXT("加入%s"), *SelectedDefinition.DisplayName.ToString());
		}
		else if (Evaluation.bClockRollbackDetected)
		{
			ButtonLabel = TEXT("系统时钟异常");
		}
		else
		{
			ButtonLabel = TEXT("尚未达到入门要求");
		}
		JoinButtonText->SetText(FText::FromString(ButtonLabel));
	}
}

void UImmortalSectWidget::RefreshTaskCards()
{
	if (!Player.IsValid()) return;
	const FImmortalSectState State = Player->GetSectState();
	const TArray<FImmortalSectTaskDefinition> Definitions =
		UImmortalSectLibrary::GetDailyTaskDefinitions();

	for (int32 TaskIndex = 0; TaskIndex < VisibleTaskCount; ++TaskIndex)
	{
		if (!TaskBorders.IsValidIndex(TaskIndex)
			|| !TaskTitleTexts.IsValidIndex(TaskIndex)
			|| !TaskDetailTexts.IsValidIndex(TaskIndex)
			|| !TaskProgressBars.IsValidIndex(TaskIndex)
			|| !TaskActionButtons.IsValidIndex(TaskIndex)
			|| !TaskActionLabels.IsValidIndex(TaskIndex))
		{
			continue;
		}

		TaskProgressBars[TaskIndex]->SetPercent(0.0f);
		if (!DisplayedTaskIds.IsValidIndex(TaskIndex))
		{
			TaskBorders[TaskIndex]->SetBrushColor(FLinearColor(0.055f, 0.058f, 0.075f, 0.98f));
			TaskTitleTexts[TaskIndex]->SetText(FText::FromString(TEXT("暂无任务")));
			TaskDetailTexts[TaskIndex]->SetText(FText::GetEmpty());
			TaskActionButtons[TaskIndex]->SetIsEnabled(false);
			TaskActionLabels[TaskIndex]->SetText(FText::FromString(TEXT("未开放")));
			continue;
		}

		const FName TaskId = DisplayedTaskIds[TaskIndex];
		const FImmortalSectTaskDefinition* Definition = Definitions.FindByPredicate(
			[TaskId](const FImmortalSectTaskDefinition& Candidate)
			{
				return Candidate.TaskId == TaskId;
			});
		if (!Definition)
		{
			TaskTitleTexts[TaskIndex]->SetText(FText::FromString(TEXT("任务资料缺失")));
			TaskDetailTexts[TaskIndex]->SetText(FText::GetEmpty());
			TaskActionButtons[TaskIndex]->SetIsEnabled(false);
			TaskActionLabels[TaskIndex]->SetText(FText::FromString(TEXT("不可领取")));
			continue;
		}

		FImmortalSectTaskProgress Progress;
		Progress.TaskId = TaskId;
		UImmortalSectLibrary::GetTaskProgress(State, TaskId, Progress);
		const int32 TargetAmount = FMath::Max(Definition->TargetAmount, 1);
		const int32 VisibleProgress = FMath::Clamp(Progress.Progress, 0, TargetAmount);
		TaskProgressBars[TaskIndex]->SetPercent(ImmortalSectArt::ProgressFraction(VisibleProgress, TargetAmount, State.HasJoined()));
		TaskProgressBars[TaskIndex]->SetFillColorAndOpacity(Progress.bClaimed
			? FLinearColor(0.50f, 0.45f, 0.28f) : FLinearColor(0.30f, 0.75f, 0.53f));
		TaskTitleTexts[TaskIndex]->SetText(FText::FromString(FString::Printf(
			TEXT("%d. %s"), TaskIndex + 1, *Definition->DisplayName.ToString())));
		TaskDetailTexts[TaskIndex]->SetText(FText::FromString(FString::Printf(
			TEXT("%s · %d/%d · %d贡献"),
			*Definition->Description.ToString(),
			VisibleProgress,
			TargetAmount,
			Definition->ContributionReward)));

		if (!State.HasJoined())
		{
			TaskBorders[TaskIndex]->SetBrushColor(FLinearColor(0.055f, 0.058f, 0.075f, 0.98f));
			TaskActionButtons[TaskIndex]->SetIsEnabled(false);
			TaskActionLabels[TaskIndex]->SetText(FText::FromString(TEXT("尚未入宗")));
			continue;
		}

		const FImmortalSectTaskClaimResult Evaluation = Player->EvaluateSectTaskClaim(TaskId);
		const bool bCompleted = Evaluation.bCompleted || VisibleProgress >= TargetAmount;
		const bool bClaimed = Evaluation.bAlreadyClaimed || Progress.bClaimed;
		TaskBorders[TaskIndex]->SetBrushColor(bClaimed
			? FLinearColor(0.12f, 0.11f, 0.075f, 0.98f)
			: (bCompleted
				? FLinearColor(0.075f, 0.18f, 0.14f, 0.98f)
				: FLinearColor(0.074f, 0.086f, 0.13f, 0.98f)));
		TaskActionButtons[TaskIndex]->SetIsEnabled(Evaluation.bCanClaim);
		if (Evaluation.bCanClaim)
		{
			TaskActionLabels[TaskIndex]->SetText(FText::FromString(TEXT("领取")));
		}
		else if (bClaimed)
		{
			TaskActionLabels[TaskIndex]->SetText(FText::FromString(TEXT("今日已领取")));
		}
		else if (Evaluation.bClockRollbackDetected)
		{
			TaskActionLabels[TaskIndex]->SetText(FText::FromString(TEXT("系统时钟异常")));
		}
		else
		{
			TaskActionLabels[TaskIndex]->SetText(FText::FromString(FString::Printf(
				TEXT("进度 %d/%d"), VisibleProgress, TargetAmount)));
		}
	}
}

void UImmortalSectWidget::RefreshOfferCards()
{
	if (!Player.IsValid()) return;
	const FImmortalSectState State = Player->GetSectState();
	const TArray<FImmortalSectStoreOfferDefinition> Definitions = State.HasJoined()
		? UImmortalSectLibrary::GetStoreOfferDefinitions(State.SectId)
		: TArray<FImmortalSectStoreOfferDefinition>();

	for (int32 OfferIndex = 0; OfferIndex < VisibleOfferCount; ++OfferIndex)
	{
		if (!OfferBorders.IsValidIndex(OfferIndex)
			|| !OfferTitleTexts.IsValidIndex(OfferIndex)
			|| !OfferDetailTexts.IsValidIndex(OfferIndex)
			|| !OfferActionButtons.IsValidIndex(OfferIndex)
			|| !OfferActionLabels.IsValidIndex(OfferIndex))
		{
			continue;
		}

		if (!State.HasJoined() || !DisplayedOfferIds.IsValidIndex(OfferIndex))
		{
			OfferBorders[OfferIndex]->SetBrushColor(FLinearColor(0.060f, 0.055f, 0.073f, 0.98f));
			OfferTitleTexts[OfferIndex]->SetText(FText::FromString(
				State.HasJoined() ? TEXT("暂无兑换物") : TEXT("加入宗门后开放")));
			OfferDetailTexts[OfferIndex]->SetText(FText::FromString(
				State.HasJoined() ? TEXT("宗门宝库正在整理。") : TEXT("完成宗门任务获得贡献，再来兑换奖励。")));
			OfferActionButtons[OfferIndex]->SetIsEnabled(false);
			OfferActionLabels[OfferIndex]->SetText(FText::FromString(TEXT("未开放")));
			continue;
		}

		const FName OfferId = DisplayedOfferIds[OfferIndex];
		const FImmortalSectStoreOfferDefinition* Definition = Definitions.FindByPredicate(
			[OfferId](const FImmortalSectStoreOfferDefinition& Candidate)
			{
				return Candidate.OfferId == OfferId;
			});
		if (!Definition)
		{
			OfferTitleTexts[OfferIndex]->SetText(FText::FromString(TEXT("兑换资料缺失")));
			OfferDetailTexts[OfferIndex]->SetText(FText::GetEmpty());
			OfferActionButtons[OfferIndex]->SetIsEnabled(false);
			OfferActionLabels[OfferIndex]->SetText(FText::FromString(TEXT("不可兑换")));
			continue;
		}

		FImmortalSectOfferProgress Progress;
		Progress.OfferId = OfferId;
		UImmortalSectLibrary::GetOfferProgress(State, OfferId, Progress);
		const FImmortalSectExchangeResult Evaluation = Player->EvaluateSectExchange(OfferId);
		const FString QuantitySuffix = Definition->RewardQuantity > 1
			? FString::Printf(TEXT(" ×%d"), Definition->RewardQuantity)
			: FString();
		const FString RewardPrefix = Definition->RewardType == EImmortalSectRewardType::Technique
			? TEXT("功法 · ")
			: FString();
		OfferTitleTexts[OfferIndex]->SetText(FText::FromString(FString::Printf(
			TEXT("%s%s%s"),
			*RewardPrefix,
			*Definition->DisplayName.ToString(),
			*QuantitySuffix)));

		FString Rules = FString::Printf(
			TEXT("消耗 %d贡献 · %s"),
			Definition->ContributionCost,
			*GetOfferLimitText(*Definition, Progress));
		if (Definition->RequiredLifetimeContribution > 0)
		{
			Rules += FString::Printf(
				TEXT(" · 累计需%lld"), Definition->RequiredLifetimeContribution);
		}
		OfferDetailTexts[OfferIndex]->SetText(FText::FromString(FString::Printf(
			TEXT("%s\n%s"), *Rules, *Definition->Description.ToString())));
		OfferBorders[OfferIndex]->SetBrushColor(Evaluation.bCanExchange
			? FLinearColor(0.16f, 0.105f, 0.18f, 0.98f)
			: FLinearColor(0.075f, 0.064f, 0.090f, 0.98f));
		OfferActionButtons[OfferIndex]->SetIsEnabled(Evaluation.bCanExchange);

		FString ActionLabel;
		if (Evaluation.bCanExchange)
		{
			ActionLabel = TEXT("兑换");
		}
		else if (Evaluation.bClockRollbackDetected)
		{
			ActionLabel = TEXT("系统时钟异常");
		}
		else if (Evaluation.bOneTimePurchased)
		{
			ActionLabel = Definition->RewardType == EImmortalSectRewardType::Technique
				? TEXT("功法已领悟")
				: TEXT("已经兑换");
		}
		else if (Evaluation.bDailyLimitReached)
		{
			ActionLabel = TEXT("今日已达上限");
		}
		else if (!Evaluation.bRequirementMet)
		{
			ActionLabel = TEXT("累计贡献不足");
		}
		else if (!Evaluation.bAffordable)
		{
			ActionLabel = TEXT("当前贡献不足");
		}
		else
		{
			ActionLabel = TEXT("暂不可兑换");
		}
		OfferActionLabels[OfferIndex]->SetText(FText::FromString(ActionLabel));
	}
}

void UImmortalSectWidget::SetResultMessage(const FText& Message, const bool bSucceeded)
{
	if (!ResultText) return;
	ResultText->SetText(Message);
	ResultText->SetColorAndOpacity(FSlateColor(bSucceeded
		? FLinearColor(0.43f, 1.0f, 0.65f, 1.0f)
		: FLinearColor(1.0f, 0.39f, 0.29f, 1.0f)));
}

void UImmortalSectWidget::HandleSect0Clicked() { SelectSectByIndex(0); }
void UImmortalSectWidget::HandleSect1Clicked() { SelectSectByIndex(1); }
void UImmortalSectWidget::HandleSect2Clicked() { SelectSectByIndex(2); }
void UImmortalSectWidget::HandleSect3Clicked() { SelectSectByIndex(3); }
void UImmortalSectWidget::HandleJoinClicked()
{
	if (!Player.IsValid() || SelectedSectId.IsNone())
	{
		SetResultMessage(FText::FromString(TEXT("请先选择一个有效宗门。")), false);
		return;
	}
	const FImmortalSectJoinResult Result = Player->JoinSect(SelectedSectId);
	RefreshFromPlayer();
	SetResultMessage(Result.Message, Result.bSucceeded && !Result.bPersistenceFailed);
}
void UImmortalSectWidget::HandleTask0Clicked() { HandleTaskClaimByIndex(0); }
void UImmortalSectWidget::HandleTask1Clicked() { HandleTaskClaimByIndex(1); }
void UImmortalSectWidget::HandleTask2Clicked() { HandleTaskClaimByIndex(2); }
void UImmortalSectWidget::HandleOffer0Clicked() { HandleOfferExchangeByIndex(0); }
void UImmortalSectWidget::HandleOffer1Clicked() { HandleOfferExchangeByIndex(1); }
void UImmortalSectWidget::HandleOffer2Clicked() { HandleOfferExchangeByIndex(2); }
void UImmortalSectWidget::HandleOffer3Clicked() { HandleOfferExchangeByIndex(3); }
void UImmortalSectWidget::HandleCloseClicked()
{
	if (Player.IsValid()) Player->ToggleSect();
}
