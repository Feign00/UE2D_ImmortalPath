// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalFarmingWidget.h"
#include "ImmortalFeaturePageLayout.h"
#include "ImmortalUITheme.h"

#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Crafting/ImmortalCraftingTypes.h"
#include "../Farming/ImmortalFarmingTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
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
	constexpr int32 VisiblePlotCount = 6;

	const TArray<FName>& GetDisplayedCropIds()
	{
		static const TArray<FName> CropIds =
		{
			TEXT("SpiritGrassCrop"),
			TEXT("ImmortalFruitCrop"),
			TEXT("SpiritWoodCrop")
		};
		return CropIds;
	}

	void SetFarmingLayout(
		UCanvasPanelSlot* Slot,
		const FVector2D Position,
		const FVector2D Size)
	{
		if (!Slot) return;
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetAutoSize(false);
	}

	void StyleFarmingText(
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

	FSlateBrush MakeFarmingBrush(const FVector2D Size, const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	FButtonStyle MakeFarmingButtonStyle(const FVector2D Size, const FLinearColor& Tint)
	{
		FButtonStyle Style = ImmortalUITheme::ButtonStyle();
		Style.SetNormal(ImmortalUITheme::PanelBrush(Tint));
		return Style;
	}

	UTextBlock* AddFarmingButtonLabel(
		UWidgetTree* Tree,
		UButton* Button,
		const TCHAR* Label,
		const int32 FontSize = 15)
	{
		if (!Tree || !Button) return nullptr;
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		Text->SetAutoWrapText(true);
		StyleFarmingText(Text, FontSize, FLinearColor(1.0f, 0.92f, 0.66f, 1.0f), true);
		Button->AddChild(Text);
		ImmortalFeaturePageLayout::StabilizeButtonLabel(Button, Text);
		return Text;
	}

	FString FormatSeconds(const int64 Seconds)
	{
		const int64 SafeSeconds = FMath::Max<int64>(Seconds, 0);
		const int64 MaximumSafeSeconds = MAX_int64 / ETimespan::TicksPerSecond;
		return UImmortalFarmingLibrary::FormatDuration(
			FMath::Min(SafeSeconds, MaximumSafeSeconds) * ETimespan::TicksPerSecond).ToString();
	}

	FText GetGrowthStageText(const EImmortalFarmingGrowthStage Stage)
	{
		switch (Stage)
		{
		case EImmortalFarmingGrowthStage::Empty:
			return FText::FromString(TEXT("空闲"));
		case EImmortalFarmingGrowthStage::Seedling:
			return FText::FromString(TEXT("幼苗"));
		case EImmortalFarmingGrowthStage::Growing:
			return FText::FromString(TEXT("生长中"));
		case EImmortalFarmingGrowthStage::Ripening:
			return FText::FromString(TEXT("将成熟"));
		case EImmortalFarmingGrowthStage::Mature:
			return FText::FromString(TEXT("可收获"));
		default:
			return FText::FromString(TEXT("未知"));
		}
	}

	FLinearColor GetGrowthStageColor(const EImmortalFarmingGrowthStage Stage)
	{
		switch (Stage)
		{
		case EImmortalFarmingGrowthStage::Empty:
			return FLinearColor(0.68f, 0.72f, 0.68f, 1.0f);
		case EImmortalFarmingGrowthStage::Seedling:
			return FLinearColor(0.50f, 0.96f, 0.58f, 1.0f);
		case EImmortalFarmingGrowthStage::Growing:
			return FLinearColor(0.38f, 0.88f, 0.72f, 1.0f);
		case EImmortalFarmingGrowthStage::Ripening:
			return FLinearColor(0.84f, 0.96f, 0.44f, 1.0f);
		case EImmortalFarmingGrowthStage::Mature:
			return FLinearColor(1.0f, 0.78f, 0.24f, 1.0f);
		default:
			return FLinearColor::White;
		}
	}

	bool GetCropPresentation(
		const FName CropId,
		FImmortalFarmingCropDefinition& OutCrop,
		FImmortalMaterialDefinition& OutMaterial)
	{
		if (!UImmortalFarmingLibrary::GetCropDefinition(CropId, OutCrop)) return false;
		if (!UImmortalMaterialLibrary::GetMaterialDefinition(OutCrop.OutputMaterialId, OutMaterial))
		{
			OutMaterial = FImmortalMaterialDefinition();
			OutMaterial.DisplayName = OutCrop.DisplayName;
			OutMaterial.IconGlyph = FText::FromString(TEXT("植"));
			OutMaterial.DisplayColor = OutCrop.DisplayColor;
		}
		return true;
	}
}

void UImmortalFarmingWidget::InitializeForPlayer(AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalFarmingWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("FarmingScreenSize"));
	RootSize->SetWidthOverride(1600.0f);
	RootSize->SetHeightOverride(270.0f);
	WidgetTree->RootWidget = RootSize;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("FarmingScreenBackground"));
	Background->SetBrushColor(FLinearColor(0.024f, 0.047f, 0.030f, 0.988f));
	Background->SetPadding(FMargin(0.0f));
	RootSize->AddChild(Background);

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("FarmingScreenCanvas"));
	Background->AddChild(Canvas);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("FarmingScreenTitle"));
	Title->SetText(FText::FromString(TEXT("洞府灵田")));
	StyleFarmingText(Title, 28, FLinearColor(0.64f, 1.0f, 0.61f, 1.0f));
	SetFarmingLayout(Canvas->AddChildToCanvas(Title), FVector2D(18.0f, 3.0f), FVector2D(170.0f, 36.0f));

	HeaderSummaryText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("FarmingHeaderSummary"));
	HeaderSummaryText->SetAutoWrapText(true);
	StyleFarmingText(HeaderSummaryText, 15, FLinearColor(0.92f, 0.91f, 0.69f, 1.0f), true);
	SetFarmingLayout(Canvas->AddChildToCanvas(HeaderSummaryText), FVector2D(184.0f, 1.0f), FVector2D(1344.0f, 39.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("FarmingScreenClose"));
	CloseButton->SetStyle(MakeFarmingButtonStyle(
		FVector2D(48.0f, 36.0f), FLinearColor(0.42f, 0.17f, 0.12f, 1.0f)));
	CloseButton->OnClicked.AddDynamic(this, &UImmortalFarmingWidget::HandleCloseClicked);
	SetFarmingLayout(Canvas->AddChildToCanvas(CloseButton), FVector2D(1542.0f, 3.0f), FVector2D(48.0f, 36.0f));
	AddFarmingButtonLabel(WidgetTree, CloseButton, TEXT("×"), 20);

	UBorder* CropPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("FarmingCropPanel"));
	CropPanel->SetBrushColor(FLinearColor(0.043f, 0.082f, 0.051f, 0.97f));
	CropPanel->SetPadding(FMargin(0.0f));
	SetFarmingLayout(Canvas->AddChildToCanvas(CropPanel), FVector2D(8.0f, 42.0f), FVector2D(312.0f, 220.0f));

	UCanvasPanel* CropCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("FarmingCropCanvas"));
	CropPanel->AddChild(CropCanvas);

	UTextBlock* CropTitle = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("FarmingCropTitle"));
	CropTitle->SetText(FText::FromString(TEXT("选择作物")));
	StyleFarmingText(CropTitle, 18, FLinearColor::White);
	SetFarmingLayout(CropCanvas->AddChildToCanvas(CropTitle), FVector2D(8.0f, 3.0f), FVector2D(150.0f, 24.0f));

	for (int32 CropIndex = 0; CropIndex < GetDisplayedCropIds().Num(); ++CropIndex)
	{
		UButton* CropButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), *FString::Printf(TEXT("FarmingCropButton%d"), CropIndex));
		CropButton->SetStyle(MakeFarmingButtonStyle(
			FVector2D(96.0f, 38.0f), FLinearColor(0.085f, 0.15f, 0.10f, 0.98f)));
		switch (CropIndex)
		{
		case 0:
			CropButton->OnClicked.AddDynamic(this, &UImmortalFarmingWidget::HandleSpiritGrassSelected);
			break;
		case 1:
			CropButton->OnClicked.AddDynamic(this, &UImmortalFarmingWidget::HandleImmortalFruitSelected);
			break;
		case 2:
			CropButton->OnClicked.AddDynamic(this, &UImmortalFarmingWidget::HandleSpiritWoodSelected);
			break;
		default:
			break;
		}
		SetFarmingLayout(
			CropCanvas->AddChildToCanvas(CropButton),
			FVector2D(8.0f + CropIndex * 99.0f, 29.0f),
			FVector2D(96.0f, 38.0f));
		CropButtons.Add(CropButton);
		CropButtonLabels.Add(AddFarmingButtonLabel(WidgetTree, CropButton, TEXT(""), 16));
	}

	SelectedCropText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("FarmingSelectedCrop"));
	SelectedCropText->SetAutoWrapText(true);
	StyleFarmingText(SelectedCropText, 16, FLinearColor(0.82f, 0.94f, 0.82f, 1.0f));
	SetFarmingLayout(CropCanvas->AddChildToCanvas(SelectedCropText), FVector2D(8.0f, 73.0f), FVector2D(296.0f, 75.0f));

	PlantAllButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("FarmingPlantAll"));
	PlantAllButton->SetStyle(MakeFarmingButtonStyle(
		FVector2D(296.0f, 34.0f), FLinearColor(0.18f, 0.49f, 0.27f, 1.0f)));
	PlantAllButton->OnClicked.AddDynamic(this, &UImmortalFarmingWidget::HandlePlantAllClicked);
	SetFarmingLayout(CropCanvas->AddChildToCanvas(PlantAllButton), FVector2D(8.0f, 154.0f), FVector2D(296.0f, 34.0f));
	PlantAllButtonText = AddFarmingButtonLabel(WidgetTree, PlantAllButton, TEXT("播种所有空闲田块"), 16);

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("FarmingOperationResult"));
	ResultText->SetAutoWrapText(true);
	StyleFarmingText(ResultText, 14, FLinearColor(0.63f, 0.89f, 0.69f, 1.0f), true);
	SetFarmingLayout(CropCanvas->AddChildToCanvas(ResultText), FVector2D(8.0f, 194.0f), FVector2D(296.0f, 22.0f));

	UBorder* PlotPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("FarmingPlotPanel"));
	PlotPanel->SetBrushColor(FLinearColor(0.031f, 0.061f, 0.038f, 0.97f));
	PlotPanel->SetPadding(FMargin(0.0f));
	SetFarmingLayout(Canvas->AddChildToCanvas(PlotPanel), FVector2D(328.0f, 42.0f), FVector2D(1264.0f, 220.0f));

	UCanvasPanel* PlotCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("FarmingPlotCanvas"));
	PlotPanel->AddChild(PlotCanvas);

	UTextBlock* PlotPanelTitle = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("FarmingPlotPanelTitle"));
	PlotPanelTitle->SetText(FText::FromString(TEXT("六方灵田 · 离线亦会成长")));
	PlotPanelTitle->SetVisibility(ESlateVisibility::Collapsed);

	for (int32 PlotIndex = 0; PlotIndex < VisiblePlotCount; ++PlotIndex)
	{
		const FVector2D PlotPosition(6.0f + PlotIndex * 208.0f, 5.0f);

		UBorder* PlotBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), *FString::Printf(TEXT("FarmingPlotBorder%d"), PlotIndex));
		PlotBorder->SetBrushColor(FLinearColor(0.075f, 0.13f, 0.085f, 0.98f));
		PlotBorder->SetPadding(FMargin(0.0f));
		SetFarmingLayout(PlotCanvas->AddChildToCanvas(PlotBorder), PlotPosition, FVector2D(202.0f, 176.0f));
		PlotBorders.Add(PlotBorder);

		UCanvasPanel* CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), *FString::Printf(TEXT("FarmingPlotCanvas%d"), PlotIndex));
		PlotBorder->AddChild(CardCanvas);

		UTextBlock* PlotTitle = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("FarmingPlotTitle%d"), PlotIndex));
		StyleFarmingText(PlotTitle, 16, FLinearColor::White);
		SetFarmingLayout(CardCanvas->AddChildToCanvas(PlotTitle), FVector2D(7.0f, 4.0f), FVector2D(88.0f, 21.0f));
		PlotTitleTexts.Add(PlotTitle);

		UTextBlock* PlotStatus = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("FarmingPlotStatus%d"), PlotIndex));
		StyleFarmingText(PlotStatus, 14, FLinearColor::White, true);
		SetFarmingLayout(CardCanvas->AddChildToCanvas(PlotStatus), FVector2D(96.0f, 4.0f), FVector2D(99.0f, 21.0f));
		PlotStatusTexts.Add(PlotStatus);

		UTextBlock* PlotGlyph = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("FarmingPlotGlyph%d"), PlotIndex));
		StyleFarmingText(PlotGlyph, 32, FLinearColor(0.62f, 1.0f, 0.62f, 1.0f), true);
		SetFarmingLayout(CardCanvas->AddChildToCanvas(PlotGlyph), FVector2D(8.0f, 28.0f), FVector2D(186.0f, 40.0f));
		PlotGlyphTexts.Add(PlotGlyph);

		UTextBlock* PlotDetail = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("FarmingPlotDetail%d"), PlotIndex));
		PlotDetail->SetAutoWrapText(true);
		StyleFarmingText(PlotDetail, 14, FLinearColor(0.84f, 0.89f, 0.84f, 1.0f), true);
		SetFarmingLayout(CardCanvas->AddChildToCanvas(PlotDetail), FVector2D(7.0f, 73.0f), FVector2D(188.0f, 51.0f));
		PlotDetailTexts.Add(PlotDetail);

		UProgressBar* Progress = WidgetTree->ConstructWidget<UProgressBar>(
			UProgressBar::StaticClass(), *FString::Printf(TEXT("FarmingPlotProgress%d"), PlotIndex));
		FProgressBarStyle ProgressStyle;
		ProgressStyle.SetBackgroundImage(MakeFarmingBrush(
			FVector2D(186.0f, 8.0f), FLinearColor(0.025f, 0.035f, 0.028f, 1.0f)));
		ProgressStyle.SetFillImage(MakeFarmingBrush(
			FVector2D(186.0f, 8.0f), FLinearColor::White));
		ProgressStyle.SetMarqueeImage(MakeFarmingBrush(
			FVector2D(186.0f, 8.0f), FLinearColor::Transparent));
		Progress->SetWidgetStyle(ProgressStyle);
		Progress->SetBarFillType(EProgressBarFillType::LeftToRight);
		SetFarmingLayout(CardCanvas->AddChildToCanvas(Progress), FVector2D(8.0f, 127.0f), FVector2D(186.0f, 8.0f));
		PlotProgressBars.Add(Progress);

		UButton* PlotButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), *FString::Printf(TEXT("FarmingPlotAction%d"), PlotIndex));
		PlotButton->SetStyle(MakeFarmingButtonStyle(
			FVector2D(186.0f, 30.0f), FLinearColor(0.20f, 0.48f, 0.28f, 1.0f)));
		switch (PlotIndex)
		{
		case 0: PlotButton->OnClicked.AddDynamic(this, &UImmortalFarmingWidget::HandlePlot0Clicked); break;
		case 1: PlotButton->OnClicked.AddDynamic(this, &UImmortalFarmingWidget::HandlePlot1Clicked); break;
		case 2: PlotButton->OnClicked.AddDynamic(this, &UImmortalFarmingWidget::HandlePlot2Clicked); break;
		case 3: PlotButton->OnClicked.AddDynamic(this, &UImmortalFarmingWidget::HandlePlot3Clicked); break;
		case 4: PlotButton->OnClicked.AddDynamic(this, &UImmortalFarmingWidget::HandlePlot4Clicked); break;
		case 5: PlotButton->OnClicked.AddDynamic(this, &UImmortalFarmingWidget::HandlePlot5Clicked); break;
		default: break;
		}
		SetFarmingLayout(CardCanvas->AddChildToCanvas(PlotButton), FVector2D(8.0f, 141.0f), FVector2D(186.0f, 30.0f));
		PlotActionButtons.Add(PlotButton);
		PlotActionLabels.Add(AddFarmingButtonLabel(WidgetTree, PlotButton, TEXT(""), 15));
	}

	HarvestAllButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("FarmingHarvestAll"));
	HarvestAllButton->SetStyle(MakeFarmingButtonStyle(
		FVector2D(240.0f, 30.0f), FLinearColor(0.52f, 0.38f, 0.09f, 1.0f)));
	HarvestAllButton->OnClicked.AddDynamic(this, &UImmortalFarmingWidget::HandleHarvestAllClicked);
	SetFarmingLayout(PlotCanvas->AddChildToCanvas(HarvestAllButton), FVector2D(1008.0f, 184.0f), FVector2D(240.0f, 30.0f));
	HarvestAllButtonText = AddFarmingButtonLabel(WidgetTree, HarvestAllButton, TEXT("一键收获"), 16);

	UTextBlock* FooterNote = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("FarmingFooterNote"));
	FooterNote->SetText(FText::FromString(TEXT("灵田等级会解锁田块并提高成长速度与收成")));
	FooterNote->SetAutoWrapText(true);
	StyleFarmingText(FooterNote, 14, FLinearColor(0.63f, 0.78f, 0.65f, 1.0f));
	SetFarmingLayout(PlotCanvas->AddChildToCanvas(FooterNote), FVector2D(8.0f, 187.0f), FVector2D(985.0f, 26.0f));

	RefreshFromPlayer();
	ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, SelectedCropText);
	ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, ResultText);
	for (UTextBlock* Detail : PlotDetailTexts) ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, Detail);
}

void UImmortalFarmingWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Player.IsValid()) return;

	RefreshAccumulator += FMath::Max(InDeltaTime, 0.0f);
	if (Player->GetFarmingRevision() != LastFarmingRevision
		|| Player->GetMaterialInventoryRevision() != LastMaterialRevision
		|| Player->GetGold() != LastSpiritStones
		|| RefreshAccumulator >= 1.0f)
	{
		RefreshAccumulator = 0.0f;
		RefreshFromPlayer();
	}
}

void UImmortalFarmingWidget::RefreshFromPlayer()
{
	if (!Player.IsValid() || PlotActionButtons.Num() != VisiblePlotCount) return;

	FImmortalFarmingCropDefinition SelectedDefinition;
	if (!UImmortalFarmingLibrary::GetCropDefinition(SelectedCropId, SelectedDefinition))
	{
		for (const FName CropId : GetDisplayedCropIds())
		{
			if (UImmortalFarmingLibrary::GetCropDefinition(CropId, SelectedDefinition))
			{
				SelectedCropId = CropId;
				break;
			}
		}
	}

	LastFarmingRevision = Player->GetFarmingRevision();
	LastMaterialRevision = Player->GetMaterialInventoryRevision();
	LastSpiritStones = Player->GetGold();
	const int32 SpiritFieldLevel = Player->GetSpiritFieldLevel();
	const int32 MaximumPlots = FMath::Min(UImmortalFarmingLibrary::GetMaximumPlotCount(), VisiblePlotCount);
	const int32 UnlockedPlots = UImmortalFarmingLibrary::GetUnlockedPlotCount(SpiritFieldLevel);

	if (HeaderSummaryText)
	{
		HeaderSummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("灵田等级 %d · 已开垦 %d/%d · 灵石 %d\n灵草 %d · 仙果 %d · 灵木 %d"),
			SpiritFieldLevel,
			FMath::Min(UnlockedPlots, MaximumPlots),
			MaximumPlots,
			LastSpiritStones,
			Player->GetMaterialQuantity(TEXT("SpiritGrass")),
			Player->GetMaterialQuantity(TEXT("ImmortalFruit")),
			Player->GetMaterialQuantity(TEXT("SpiritWood")))));
	}

	RefreshCropCards();
	RefreshPlotCards();

	bool bCanPlantAny = false;
	for (int32 PlotIndex = 0; PlotIndex < MaximumPlots; ++PlotIndex)
	{
		if (Player->EvaluatePlantCrop(PlotIndex, SelectedCropId).bCanPlant)
		{
			bCanPlantAny = true;
			break;
		}
	}
	if (PlantAllButton) PlantAllButton->SetIsEnabled(bCanPlantAny);
	if (PlantAllButtonText)
	{
		PlantAllButtonText->SetText(FText::FromString(
			bCanPlantAny ? TEXT("播种所有空闲田块") : TEXT("当前无法批量播种")));
	}
}

void UImmortalFarmingWidget::SelectCrop(const FName CropId)
{
	FImmortalFarmingCropDefinition Definition;
	if (!UImmortalFarmingLibrary::GetCropDefinition(CropId, Definition))
	{
		SetResultMessage(FText::FromString(TEXT("作物配置尚未就绪")), false);
		return;
	}
	SelectedCropId = CropId;
	SetResultMessage(FText::GetEmpty(), true);
	RefreshFromPlayer();
}

void UImmortalFarmingWidget::RefreshCropCards()
{
	if (!Player.IsValid()) return;
	const TArray<FName>& CropIds = GetDisplayedCropIds();
	for (int32 Index = 0; Index < CropIds.Num() && Index < CropButtons.Num(); ++Index)
	{
		FImmortalFarmingCropDefinition Definition;
		FImmortalMaterialDefinition Material;
		const bool bKnownCrop = GetCropPresentation(CropIds[Index], Definition, Material);
		const bool bSelected = CropIds[Index] == SelectedCropId;
		CropButtons[Index]->SetIsEnabled(bKnownCrop);
		CropButtons[Index]->SetStyle(MakeFarmingButtonStyle(
			FVector2D(96.0f, 52.0f),
			bSelected
				? Definition.DisplayColor.GetClamped(0.22f, 0.78f)
				: FLinearColor(0.075f, 0.14f, 0.09f, 0.98f)));
		if (!CropButtonLabels.IsValidIndex(Index)) continue;
		if (!bKnownCrop)
		{
			CropButtonLabels[Index]->SetText(FText::FromString(TEXT("作物配置缺失")));
			continue;
		}
		CropButtonLabels[Index]->SetText(Definition.DisplayName);
		CropButtons[Index]->SetToolTipText(FText::FromString(FString::Printf(
			TEXT("%s [%s]\n%s · %d~%d"),
			*Definition.DisplayName.ToString(),
			*Material.IconGlyph.ToString(),
			*FormatSeconds(Definition.BaseGrowthSeconds),
			Definition.MinimumBaseYield,
			Definition.MaximumBaseYield)));
		CropButtonLabels[Index]->SetColorAndOpacity(FSlateColor(
			bSelected ? FLinearColor::White : Definition.DisplayColor.GetClamped(0.50f, 1.0f)));
	}

	if (!SelectedCropText) return;
	FImmortalFarmingCropDefinition Definition;
	FImmortalMaterialDefinition Material;
	if (!GetCropPresentation(SelectedCropId, Definition, Material))
	{
		SelectedCropText->SetText(FText::FromString(TEXT("请选择有效作物")));
		return;
	}
	const FText CostText = UImmortalCraftingLibrary::FormatCost(
		Definition.PlantingCost, Player->GetMaterialInventory(), Player->GetGold());
	const FString CompactCost = CostText.ToString().Replace(TEXT("\n"), TEXT(" · "));
	SelectedCropText->SetText(FText::FromString(FString::Printf(
		TEXT("已选：%s · 需灵田 Lv%d\n成长 %s · 收成 %d~%d\n消耗：%s"),
		*Definition.DisplayName.ToString(),
		Definition.RequiredSpiritFieldLevel,
		*FormatSeconds(Definition.BaseGrowthSeconds),
		Definition.MinimumBaseYield,
		Definition.MaximumBaseYield,
		*CompactCost)));
	SelectedCropText->SetColorAndOpacity(FSlateColor(Definition.DisplayColor.GetClamped(0.54f, 1.0f)));
}

void UImmortalFarmingWidget::RefreshPlotCards()
{
	if (!Player.IsValid()) return;
	const FImmortalFarmingState State = Player->GetFarmingState();
	const int32 SpiritFieldLevel = Player->GetSpiritFieldLevel();
	const int64 CurrentUtcTicks = FDateTime::UtcNow().GetTicks();
	int32 ReadyPlotCount = 0;

	for (int32 PlotIndex = 0; PlotIndex < VisiblePlotCount; ++PlotIndex)
	{
		if (!PlotBorders.IsValidIndex(PlotIndex)
			|| !PlotTitleTexts.IsValidIndex(PlotIndex)
			|| !PlotGlyphTexts.IsValidIndex(PlotIndex)
			|| !PlotStatusTexts.IsValidIndex(PlotIndex)
			|| !PlotDetailTexts.IsValidIndex(PlotIndex)
			|| !PlotProgressBars.IsValidIndex(PlotIndex)
			|| !PlotActionButtons.IsValidIndex(PlotIndex)
			|| !PlotActionLabels.IsValidIndex(PlotIndex))
		{
			continue;
		}

		const FImmortalFarmingPlotView View = UImmortalFarmingLibrary::GetPlotView(
			State, PlotIndex, SpiritFieldLevel, CurrentUtcTicks);
		PlotTitleTexts[PlotIndex]->SetText(FText::FromString(FString::Printf(TEXT("灵田 %d"), PlotIndex + 1)));

		if (!View.bValidPlot || !View.bUnlocked)
		{
			const int32 RequiredLevel = View.RequiredSpiritFieldLevel > 0
				? View.RequiredSpiritFieldLevel
				: UImmortalFarmingLibrary::GetRequiredFieldLevelForPlot(PlotIndex);
			PlotBorders[PlotIndex]->SetBrushColor(FLinearColor(0.055f, 0.065f, 0.058f, 0.98f));
			PlotGlyphTexts[PlotIndex]->SetText(FText::FromString(TEXT("封")));
			PlotGlyphTexts[PlotIndex]->SetColorAndOpacity(FSlateColor(FLinearColor(0.40f, 0.43f, 0.40f, 1.0f)));
			PlotStatusTexts[PlotIndex]->SetText(FText::FromString(TEXT("未解锁")));
			PlotStatusTexts[PlotIndex]->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.62f, 0.58f, 1.0f)));
			PlotDetailTexts[PlotIndex]->SetText(FText::FromString(FString::Printf(
				TEXT("灵田等级 %d 解锁\n升级洞府灵田后开放"), RequiredLevel)));
			PlotProgressBars[PlotIndex]->SetPercent(0.0f);
			PlotProgressBars[PlotIndex]->SetFillColorAndOpacity(FLinearColor(0.25f, 0.27f, 0.25f, 1.0f));
			PlotActionButtons[PlotIndex]->SetIsEnabled(false);
			PlotActionLabels[PlotIndex]->SetText(FText::FromString(TEXT("尚未开垦")));
			continue;
		}

		if (View.CropId.IsNone() || View.GrowthStage == EImmortalFarmingGrowthStage::Empty)
		{
			const FImmortalFarmingPlantResult Evaluation = Player->EvaluatePlantCrop(PlotIndex, SelectedCropId);
			FImmortalFarmingCropDefinition SelectedDefinition;
			const bool bKnownSelection = UImmortalFarmingLibrary::GetCropDefinition(
				SelectedCropId, SelectedDefinition);
			PlotBorders[PlotIndex]->SetBrushColor(FLinearColor(0.071f, 0.135f, 0.083f, 0.98f));
			PlotGlyphTexts[PlotIndex]->SetText(FText::FromString(TEXT("田")));
			PlotGlyphTexts[PlotIndex]->SetColorAndOpacity(FSlateColor(FLinearColor(0.58f, 0.85f, 0.57f, 1.0f)));
			PlotStatusTexts[PlotIndex]->SetText(FText::FromString(TEXT("空闲")));
			PlotStatusTexts[PlotIndex]->SetColorAndOpacity(FSlateColor(GetGrowthStageColor(EImmortalFarmingGrowthStage::Empty)));
			PlotDetailTexts[PlotIndex]->SetText(bKnownSelection
				? FText::FromString(FString::Printf(TEXT("已选择：%s\n点击下方按钮播种"),
					*SelectedDefinition.DisplayName.ToString()))
				: FText::FromString(TEXT("请先选择有效作物")));
			PlotProgressBars[PlotIndex]->SetPercent(0.0f);
			PlotProgressBars[PlotIndex]->SetFillColorAndOpacity(FLinearColor(0.36f, 0.72f, 0.42f, 1.0f));
			PlotActionButtons[PlotIndex]->SetIsEnabled(Evaluation.bCanPlant);
			if (Evaluation.bCanPlant)
			{
				PlotActionLabels[PlotIndex]->SetText(FText::FromString(FString::Printf(
					TEXT("播种 %s"), *SelectedDefinition.DisplayName.ToString())));
			}
			else if (!Evaluation.bKnownCrop)
			{
				PlotActionLabels[PlotIndex]->SetText(FText::FromString(TEXT("作物无效")));
			}
			else if (!Evaluation.bCropUnlocked)
			{
				PlotActionLabels[PlotIndex]->SetText(FText::FromString(FString::Printf(
					TEXT("需灵田 Lv%d"), SelectedDefinition.RequiredSpiritFieldLevel)));
			}
			else if (!Evaluation.bAffordable)
			{
				PlotActionLabels[PlotIndex]->SetText(FText::FromString(TEXT("播种资源不足")));
			}
			else if (Evaluation.bClockRollbackDetected)
			{
				PlotActionLabels[PlotIndex]->SetText(FText::FromString(TEXT("时钟异常")));
			}
			else
			{
				PlotActionLabels[PlotIndex]->SetText(FText::FromString(TEXT("暂不可播种")));
			}
			continue;
		}

		FImmortalFarmingCropDefinition CropDefinition;
		FImmortalMaterialDefinition OutputMaterial;
		const bool bKnownCrop = GetCropPresentation(View.CropId, CropDefinition, OutputMaterial);
		const FLinearColor CropColor = bKnownCrop
			? CropDefinition.DisplayColor.GetClamped(0.28f, 1.0f)
			: FLinearColor(0.52f, 0.86f, 0.55f, 1.0f);
		const bool bMature = View.GrowthStage == EImmortalFarmingGrowthStage::Mature;
		ReadyPlotCount += bMature ? 1 : 0;

		PlotBorders[PlotIndex]->SetBrushColor(bMature
			? FLinearColor(0.31f, 0.235f, 0.055f, 0.98f)
			: (CropColor * FLinearColor(0.18f, 0.25f, 0.18f, 0.98f)).GetClamped());
		PlotGlyphTexts[PlotIndex]->SetText(
			OutputMaterial.IconGlyph.IsEmpty() ? FText::FromString(TEXT("植")) : OutputMaterial.IconGlyph);
		PlotGlyphTexts[PlotIndex]->SetColorAndOpacity(FSlateColor(
			bMature ? FLinearColor(1.0f, 0.82f, 0.28f, 1.0f) : CropColor));
		PlotStatusTexts[PlotIndex]->SetText(GetGrowthStageText(View.GrowthStage));
		PlotStatusTexts[PlotIndex]->SetColorAndOpacity(FSlateColor(GetGrowthStageColor(View.GrowthStage)));

		const FString MaterialName = OutputMaterial.DisplayName.IsEmpty()
			? (bKnownCrop ? CropDefinition.DisplayName.ToString() : View.CropId.ToString())
			: OutputMaterial.DisplayName.ToString();
		if (bMature)
		{
			PlotDetailTexts[PlotIndex]->SetText(FText::FromString(FString::Printf(
				TEXT("%s已经成熟\n可收获 ×%d"), *MaterialName, View.PendingYield)));
		}
		else if (View.bClockRollbackDetected)
		{
			PlotDetailTexts[PlotIndex]->SetText(FText::FromString(FString::Printf(
				TEXT("%s · 时钟回退\n成长暂时暂停"), *MaterialName)));
		}
		else
		{
			PlotDetailTexts[PlotIndex]->SetText(FText::FromString(FString::Printf(
				TEXT("%s · 预计 ×%d\n剩余 %s"),
				*MaterialName, View.PendingYield, *FormatSeconds(View.RemainingSeconds))));
		}
		PlotProgressBars[PlotIndex]->SetPercent(FMath::Clamp(View.ProgressPermille / 1000.0f, 0.0f, 1.0f));
		PlotProgressBars[PlotIndex]->SetFillColorAndOpacity(
			bMature ? FLinearColor(1.0f, 0.76f, 0.18f, 1.0f) : CropColor);
		PlotActionButtons[PlotIndex]->SetIsEnabled(bMature);
		PlotActionLabels[PlotIndex]->SetText(FText::FromString(bMature
			? FString::Printf(TEXT("收获 ×%d"), View.PendingYield)
			: FString(TEXT("生长中"))));
	}

	if (HarvestAllButton) HarvestAllButton->SetIsEnabled(ReadyPlotCount > 0);
	if (HarvestAllButtonText)
	{
		HarvestAllButtonText->SetText(FText::FromString(ReadyPlotCount > 0
			? FString::Printf(TEXT("一键收获（%d块成熟）"), ReadyPlotCount)
			: FString(TEXT("暂无成熟作物"))));
	}
}

void UImmortalFarmingWidget::HandlePlotAction(const int32 PlotIndex)
{
	if (!Player.IsValid()) return;
	const FImmortalFarmingPlotView View = UImmortalFarmingLibrary::GetPlotView(
		Player->GetFarmingState(), PlotIndex, Player->GetSpiritFieldLevel(), FDateTime::UtcNow().GetTicks());
	if (!View.bValidPlot || !View.bUnlocked)
	{
		SetResultMessage(View.Message.IsEmpty()
			? FText::FromString(TEXT("该田块尚未解锁"))
			: View.Message, false);
		return;
	}

	if (View.GrowthStage == EImmortalFarmingGrowthStage::Mature)
	{
		const FImmortalFarmingHarvestResult Result = Player->HarvestCrop(PlotIndex);
		RefreshFromPlayer();
		SetResultMessage(Result.Message, Result.bSucceeded && !Result.bPersistenceFailed);
		return;
	}
	if (View.CropId.IsNone() || View.GrowthStage == EImmortalFarmingGrowthStage::Empty)
	{
		const FImmortalFarmingPlantResult Result = Player->PlantCrop(PlotIndex, SelectedCropId);
		RefreshFromPlayer();
		SetResultMessage(Result.Message, Result.bSucceeded && !Result.bPersistenceFailed);
		return;
	}

	SetResultMessage(View.bClockRollbackDetected
		? FText::FromString(TEXT("系统时钟发生回退，当前作物成长已安全暂停"))
		: FText::FromString(TEXT("作物尚未成熟")), false);
}

void UImmortalFarmingWidget::SetResultMessage(const FText& Message, const bool bSucceeded)
{
	if (!ResultText) return;
	ResultText->SetText(Message);
	ResultText->SetColorAndOpacity(FSlateColor(bSucceeded
		? FLinearColor(0.43f, 1.0f, 0.65f, 1.0f)
		: FLinearColor(1.0f, 0.39f, 0.29f, 1.0f)));
}

void UImmortalFarmingWidget::HandleSpiritGrassSelected()
{
	SelectCrop(TEXT("SpiritGrassCrop"));
}

void UImmortalFarmingWidget::HandleImmortalFruitSelected()
{
	SelectCrop(TEXT("ImmortalFruitCrop"));
}

void UImmortalFarmingWidget::HandleSpiritWoodSelected()
{
	SelectCrop(TEXT("SpiritWoodCrop"));
}

void UImmortalFarmingWidget::HandlePlot0Clicked()
{
	HandlePlotAction(0);
}

void UImmortalFarmingWidget::HandlePlot1Clicked()
{
	HandlePlotAction(1);
}

void UImmortalFarmingWidget::HandlePlot2Clicked()
{
	HandlePlotAction(2);
}

void UImmortalFarmingWidget::HandlePlot3Clicked()
{
	HandlePlotAction(3);
}

void UImmortalFarmingWidget::HandlePlot4Clicked()
{
	HandlePlotAction(4);
}

void UImmortalFarmingWidget::HandlePlot5Clicked()
{
	HandlePlotAction(5);
}

void UImmortalFarmingWidget::HandlePlantAllClicked()
{
	if (!Player.IsValid()) return;
	const FImmortalFarmingBatchPlantResult Result = Player->PlantCropInAllEmptyPlots(SelectedCropId);
	RefreshFromPlayer();
	SetResultMessage(Result.Message, Result.bSucceeded && !Result.bPersistenceFailed);
}

void UImmortalFarmingWidget::HandleHarvestAllClicked()
{
	if (!Player.IsValid()) return;
	const FImmortalFarmingBatchHarvestResult Result = Player->HarvestAllReadyCrops();
	RefreshFromPlayer();
	SetResultMessage(Result.Message, Result.bSucceeded && !Result.bPersistenceFailed);
}

void UImmortalFarmingWidget::HandleCloseClicked()
{
	if (Player.IsValid()) Player->ToggleFarming();
}
