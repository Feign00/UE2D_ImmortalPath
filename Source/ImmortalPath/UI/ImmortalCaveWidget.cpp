// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalCaveWidget.h"

#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Crafting/ImmortalCraftingTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/SlateTypes.h"

namespace
{
	void SetCaveLayout(UCanvasPanelSlot* Slot, const FVector2D Position, const FVector2D Size)
	{
		if (!Slot) return;
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetAutoSize(false);
	}

	void StyleCaveText(UTextBlock* Text, const int32 Size, const FLinearColor& Color, const bool bCentered = false)
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

	FSlateBrush MakeCaveBrush(const FVector2D Size, const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	FButtonStyle MakeCaveButtonStyle(const FVector2D Size, const FLinearColor& Tint)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeCaveBrush(Size, Tint));
		Style.SetHovered(MakeCaveBrush(Size, (Tint * 1.16f).GetClamped()));
		Style.SetPressed(MakeCaveBrush(Size, (Tint * 0.78f).GetClamped()));
		Style.SetDisabled(MakeCaveBrush(Size, FLinearColor(0.10f, 0.11f, 0.12f, 0.82f)));
		return Style;
	}

	UTextBlock* AddCaveButtonLabel(
		UWidgetTree* Tree,
		UButton* Button,
		const TCHAR* Label,
		const int32 FontSize = 16)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		StyleCaveText(Text, FontSize, FLinearColor(1.0f, 0.91f, 0.62f, 1.0f), true);
		Button->AddChild(Text);
		return Text;
	}

	FString FormatStoredAmount(const int32 Stored, const int32 Capacity)
	{
		return FString::Printf(TEXT("%d/%d"), Stored, FMath::Max(Capacity, 0));
	}
}

void UImmortalCaveWidget::InitializeForPlayer(AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalCaveWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CaveScreenSize"));
	RootSize->SetWidthOverride(900.0f);
	RootSize->SetHeightOverride(600.0f);
	WidgetTree->RootWidget = RootSize;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CaveScreenBackground"));
	Background->SetBrushColor(FLinearColor(0.025f, 0.045f, 0.035f, 0.988f));
	Background->SetPadding(FMargin(0.0f));
	RootSize->AddChild(Background);

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CaveScreenCanvas"));
	Background->AddChild(Canvas);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CaveScreenTitle"));
	Title->SetText(FText::FromString(TEXT("洞府  [C]")));
	StyleCaveText(Title, 29, FLinearColor(0.70f, 1.0f, 0.68f, 1.0f));
	SetCaveLayout(Canvas->AddChildToCanvas(Title), FVector2D(24.0f, 12.0f), FVector2D(220.0f, 44.0f));

	StoredResourceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CaveHeaderResources"));
	StyleCaveText(StoredResourceText, 15, FLinearColor(0.92f, 0.90f, 0.69f, 1.0f), true);
	SetCaveLayout(Canvas->AddChildToCanvas(StoredResourceText), FVector2D(240.0f, 16.0f), FVector2D(550.0f, 36.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CaveScreenClose"));
	CloseButton->SetStyle(MakeCaveButtonStyle(FVector2D(64.0f), FLinearColor(0.42f, 0.17f, 0.12f, 1.0f)));
	CloseButton->OnClicked.AddDynamic(this, &UImmortalCaveWidget::HandleCloseClicked);
	SetCaveLayout(Canvas->AddChildToCanvas(CloseButton), FVector2D(816.0f, 8.0f), FVector2D(64.0f));
	AddCaveButtonLabel(WidgetTree, CloseButton, TEXT("×"), 24);

	UBorder* LeftPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CaveBuildingPanel"));
	LeftPanel->SetBrushColor(FLinearColor(0.045f, 0.078f, 0.060f, 0.96f));
	LeftPanel->SetPadding(FMargin(10.0f, 8.0f));
	SetCaveLayout(Canvas->AddChildToCanvas(LeftPanel), FVector2D(14.0f, 66.0f), FVector2D(322.0f, 520.0f));

	UCanvasPanel* LeftCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CaveBuildingCanvas"));
	LeftPanel->AddChild(LeftCanvas);

	UTextBlock* BuildingTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CaveBuildingTitle"));
	BuildingTitle->SetText(FText::FromString(TEXT("洞府建筑")));
	StyleCaveText(BuildingTitle, 20, FLinearColor::White);
	SetCaveLayout(LeftCanvas->AddChildToCanvas(BuildingTitle), FVector2D(4.0f, 0.0f), FVector2D(180.0f, 30.0f));

	UScrollBox* BuildingScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CaveBuildingScroll"));
	BuildingScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	SetCaveLayout(LeftCanvas->AddChildToCanvas(BuildingScroll), FVector2D(0.0f, 34.0f), FVector2D(302.0f, 342.0f));
	BuildingList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CaveBuildingList"));
	BuildingScroll->AddChild(BuildingList);

	UButton* FarmingButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CaveOpenFarming"));
	FarmingButton->SetStyle(MakeCaveButtonStyle(FVector2D(294.0f, 56.0f), FLinearColor(0.12f, 0.46f, 0.20f, 1.0f)));
	FarmingButton->OnClicked.AddDynamic(this, &UImmortalCaveWidget::HandleFarmingClicked);
	SetCaveLayout(LeftCanvas->AddChildToCanvas(FarmingButton), FVector2D(4.0f, 382.0f), FVector2D(294.0f, 56.0f));
	AddCaveButtonLabel(WidgetTree, FarmingButton, TEXT("进入灵田 · 播种与收获"), 16);

	UButton* AlchemyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CaveOpenAlchemy"));
	AlchemyButton->SetStyle(MakeCaveButtonStyle(FVector2D(140.0f, 46.0f), FLinearColor(0.16f, 0.42f, 0.28f, 1.0f)));
	AlchemyButton->OnClicked.AddDynamic(this, &UImmortalCaveWidget::HandleAlchemyClicked);
	SetCaveLayout(LeftCanvas->AddChildToCanvas(AlchemyButton), FVector2D(4.0f, 451.0f), FVector2D(140.0f, 46.0f));
	AddCaveButtonLabel(WidgetTree, AlchemyButton, TEXT("进入丹房"));

	UButton* CraftingButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CaveOpenCrafting"));
	CraftingButton->SetStyle(MakeCaveButtonStyle(FVector2D(140.0f, 46.0f), FLinearColor(0.44f, 0.28f, 0.10f, 1.0f)));
	CraftingButton->OnClicked.AddDynamic(this, &UImmortalCaveWidget::HandleCraftingClicked);
	SetCaveLayout(LeftCanvas->AddChildToCanvas(CraftingButton), FVector2D(158.0f, 451.0f), FVector2D(140.0f, 46.0f));
	AddCaveButtonLabel(WidgetTree, CraftingButton, TEXT("进入器室"));

	UBorder* DetailPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CaveDetailPanel"));
	DetailPanel->SetBrushColor(FLinearColor(0.032f, 0.058f, 0.046f, 0.97f));
	DetailPanel->SetPadding(FMargin(0.0f));
	SetCaveLayout(Canvas->AddChildToCanvas(DetailPanel), FVector2D(350.0f, 66.0f), FVector2D(536.0f, 520.0f));

	UCanvasPanel* DetailCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CaveDetailCanvas"));
	DetailPanel->AddChild(DetailCanvas);

	BuildingNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CaveBuildingName"));
	StyleCaveText(BuildingNameText, 27, FLinearColor::White);
	SetCaveLayout(DetailCanvas->AddChildToCanvas(BuildingNameText), FVector2D(18.0f, 10.0f), FVector2D(320.0f, 42.0f));

	BuildingLevelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CaveBuildingLevel"));
	StyleCaveText(BuildingLevelText, 17, FLinearColor(1.0f, 0.82f, 0.36f, 1.0f), true);
	SetCaveLayout(DetailCanvas->AddChildToCanvas(BuildingLevelText), FVector2D(342.0f, 15.0f), FVector2D(174.0f, 30.0f));

	BuildingDescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CaveBuildingDescription"));
	BuildingDescriptionText->SetAutoWrapText(true);
	StyleCaveText(BuildingDescriptionText, 15, FLinearColor(0.86f, 0.89f, 0.86f, 1.0f));
	SetCaveLayout(DetailCanvas->AddChildToCanvas(BuildingDescriptionText), FVector2D(18.0f, 53.0f), FVector2D(500.0f, 52.0f));

	CurrentEffectText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CaveCurrentEffect"));
	CurrentEffectText->SetAutoWrapText(true);
	StyleCaveText(CurrentEffectText, 16, FLinearColor(0.54f, 1.0f, 0.70f, 1.0f));
	SetCaveLayout(DetailCanvas->AddChildToCanvas(CurrentEffectText), FVector2D(18.0f, 111.0f), FVector2D(500.0f, 58.0f));

	NextEffectText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CaveNextEffect"));
	NextEffectText->SetAutoWrapText(true);
	StyleCaveText(NextEffectText, 15, FLinearColor(0.63f, 0.86f, 1.0f, 1.0f));
	SetCaveLayout(DetailCanvas->AddChildToCanvas(NextEffectText), FVector2D(18.0f, 173.0f), FVector2D(500.0f, 58.0f));

	UpgradeCostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CaveUpgradeCost"));
	UpgradeCostText->SetAutoWrapText(true);
	StyleCaveText(UpgradeCostText, 14, FLinearColor(0.96f, 0.84f, 0.55f, 1.0f));
	SetCaveLayout(DetailCanvas->AddChildToCanvas(UpgradeCostText), FVector2D(18.0f, 237.0f), FVector2D(500.0f, 64.0f));

	ProductionRateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CaveProductionRates"));
	ProductionRateText->SetAutoWrapText(true);
	StyleCaveText(ProductionRateText, 14, FLinearColor(0.76f, 0.93f, 0.79f, 1.0f));
	SetCaveLayout(DetailCanvas->AddChildToCanvas(ProductionRateText), FVector2D(18.0f, 307.0f), FVector2D(500.0f, 96.0f));

	UpgradeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CaveUpgradeBuilding"));
	UpgradeButton->SetStyle(MakeCaveButtonStyle(FVector2D(210.0f, 48.0f), FLinearColor(0.18f, 0.52f, 0.31f, 1.0f)));
	UpgradeButton->OnClicked.AddDynamic(this, &UImmortalCaveWidget::HandleUpgradeClicked);
	SetCaveLayout(DetailCanvas->AddChildToCanvas(UpgradeButton), FVector2D(18.0f, 411.0f), FVector2D(210.0f, 48.0f));
	UpgradeButtonText = AddCaveButtonLabel(WidgetTree, UpgradeButton, TEXT("升级建筑"), 17);

	CollectButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CaveCollectResources"));
	CollectButton->SetStyle(MakeCaveButtonStyle(FVector2D(210.0f, 48.0f), FLinearColor(0.49f, 0.36f, 0.10f, 1.0f)));
	CollectButton->OnClicked.AddDynamic(this, &UImmortalCaveWidget::HandleCollectClicked);
	SetCaveLayout(DetailCanvas->AddChildToCanvas(CollectButton), FVector2D(306.0f, 411.0f), FVector2D(210.0f, 48.0f));
	CollectButtonText = AddCaveButtonLabel(WidgetTree, CollectButton, TEXT("收取全部产出"), 17);

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CaveOperationResult"));
	ResultText->SetAutoWrapText(true);
	StyleCaveText(ResultText, 14, FLinearColor(0.68f, 0.85f, 0.73f, 1.0f), true);
	SetCaveLayout(DetailCanvas->AddChildToCanvas(ResultText), FVector2D(18.0f, 468.0f), FVector2D(498.0f, 42.0f));

	RebuildBuildingButtons();
	RefreshFromPlayer();
}

void UImmortalCaveWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Player.IsValid()) return;

	RefreshAccumulator += FMath::Max(InDeltaTime, 0.0f);
	if (Player->GetCaveRevision() != LastCaveRevision
		|| Player->GetMaterialInventoryRevision() != LastMaterialRevision
		|| Player->GetGold() != LastSpiritStones
		|| RefreshAccumulator >= 1.0f)
	{
		RefreshAccumulator = 0.0f;
		RefreshFromPlayer();
	}
}

void UImmortalCaveWidget::RefreshFromPlayer()
{
	if (!Player.IsValid() || !BuildingList) return;

	const FImmortalCaveState State = Player->GetCaveState();
	const FImmortalCaveProductionSnapshot Snapshot = Player->GetCaveProductionSnapshot();
	LastCaveRevision = Player->GetCaveRevision();
	LastMaterialRevision = Player->GetMaterialInventoryRevision();
	LastSpiritStones = Player->GetGold();

	if (StoredResourceText)
	{
		StoredResourceText->SetText(FText::FromString(FString::Printf(
			TEXT("待收取：灵石 %s  ·  灵草 %s  ·  矿石 %s"),
			*FormatStoredAmount(State.StoredSpiritStones, Snapshot.SpiritStoneCapacity),
			*FormatStoredAmount(State.StoredSpiritGrass, Snapshot.SpiritGrassCapacity),
			*FormatStoredAmount(State.StoredOre, Snapshot.OreCapacity))));
	}
	if (ProductionRateText)
	{
		ProductionRateText->SetText(FText::FromString(FString::Printf(
			TEXT("洞府产出：灵石 %.1f/时  ·  灵草 %.2f/时  ·  矿石 %.2f/时\n"
				"储存上限：%.0f 小时  ·  全局产出 ×%.2f  ·  修炼 ×%.2f\n"
				"丹房：成功 +%.1f%% / 极品 +%.1f%%  ·  器室灵石减免 %.1f%%"),
			Snapshot.SpiritStonesPerHour,
			Snapshot.SpiritGrassPerHour,
			Snapshot.OrePerHour,
			Snapshot.StorageHours,
			Snapshot.GlobalProductionMultiplier,
			Snapshot.CultivationRateMultiplier,
			Snapshot.AlchemySuccessChanceBonus * 100.0f,
			Snapshot.AlchemyExceptionalChanceBonus * 100.0f,
			Snapshot.ForgeSpiritStoneDiscount * 100.0f)));
	}

	if (CollectButton)
	{
		CollectButton->SetIsEnabled(
			State.StoredSpiritStones > 0 || State.StoredSpiritGrass > 0 || State.StoredOre > 0);
	}

	RebuildBuildingButtons();
	RefreshBuildingDetails();
}

void UImmortalCaveWidget::SelectBuilding(const EImmortalCaveBuildingType BuildingType)
{
	SelectedBuilding = BuildingType;
	SetResultMessage(FText::GetEmpty(), true);
	RefreshFromPlayer();
}

void UImmortalCaveWidget::RebuildBuildingButtons()
{
	if (!BuildingList || !Player.IsValid()) return;

	const TArray<EImmortalCaveBuildingType> Types = UImmortalCaveLibrary::GetKnownBuildingTypes();
	if (BuildingButtons.Num() != Types.Num())
	{
		BuildingList->ClearChildren();
		BuildingButtons.Reset();
		BuildingButtonLabels.Reset();

		for (int32 Index = 0; Index < Types.Num(); ++Index)
		{
			USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			RowSize->SetHeightOverride(46.0f);
			UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			RowSize->AddChild(Button);
			UTextBlock* Label = AddCaveButtonLabel(WidgetTree, Button, TEXT(""), 16);

			switch (Types[Index])
			{
			case EImmortalCaveBuildingType::CaveHeart:
				Button->OnClicked.AddDynamic(this, &UImmortalCaveWidget::HandleCaveHeartClicked);
				break;
			case EImmortalCaveBuildingType::MeditationRoom:
				Button->OnClicked.AddDynamic(this, &UImmortalCaveWidget::HandleMeditationRoomClicked);
				break;
			case EImmortalCaveBuildingType::SpiritVein:
				Button->OnClicked.AddDynamic(this, &UImmortalCaveWidget::HandleSpiritVeinClicked);
				break;
			case EImmortalCaveBuildingType::StoragePavilion:
				Button->OnClicked.AddDynamic(this, &UImmortalCaveWidget::HandleStoragePavilionClicked);
				break;
			case EImmortalCaveBuildingType::AlchemyRoom:
				Button->OnClicked.AddDynamic(this, &UImmortalCaveWidget::HandleAlchemyRoomClicked);
				break;
			case EImmortalCaveBuildingType::ForgeRoom:
				Button->OnClicked.AddDynamic(this, &UImmortalCaveWidget::HandleForgeRoomClicked);
				break;
			case EImmortalCaveBuildingType::SpiritField:
				Button->OnClicked.AddDynamic(this, &UImmortalCaveWidget::HandleSpiritFieldClicked);
				break;
			default:
				break;
			}

			if (UVerticalBoxSlot* RowSlot = BuildingList->AddChildToVerticalBox(RowSize))
			{
				RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
			}
			BuildingButtons.Add(Button);
			BuildingButtonLabels.Add(Label);
		}
	}

	const FImmortalCaveState State = Player->GetCaveState();
	for (int32 Index = 0; Index < Types.Num() && Index < BuildingButtons.Num(); ++Index)
	{
		FImmortalCaveBuildingDefinition Definition;
		if (!UImmortalCaveLibrary::GetBuildingDefinition(Types[Index], Definition)) continue;
		const int32 Level = UImmortalCaveLibrary::GetBuildingLevel(State, Types[Index]);
		const bool bSelected = Types[Index] == SelectedBuilding;
		BuildingButtons[Index]->SetStyle(MakeCaveButtonStyle(
			FVector2D(302.0f, 43.0f),
			bSelected
				? Definition.DisplayColor.GetClamped(0.20f, 0.78f)
				: FLinearColor(0.08f, 0.14f, 0.11f, 0.96f)));
		BuildingButtonLabels[Index]->SetText(FText::FromString(FString::Printf(
			TEXT("%s    等级 %d/%d"),
			*Definition.DisplayName.ToString(), Level, Definition.MaximumLevel)));
		BuildingButtonLabels[Index]->SetColorAndOpacity(FSlateColor(
			bSelected ? FLinearColor::White : Definition.DisplayColor.GetClamped(0.45f, 1.0f)));
	}
}

void UImmortalCaveWidget::RefreshBuildingDetails()
{
	if (!Player.IsValid()) return;
	const FImmortalCaveState State = Player->GetCaveState();
	FImmortalCaveBuildingDefinition Definition;
	if (!UImmortalCaveLibrary::GetBuildingDefinition(SelectedBuilding, Definition)) return;

	const int32 Level = UImmortalCaveLibrary::GetBuildingLevel(State, SelectedBuilding);
	const FImmortalCaveUpgradeResult Evaluation = UImmortalCaveLibrary::EvaluateUpgrade(
		State, SelectedBuilding, Player->GetMaterialInventory(), Player->GetGold());

	if (BuildingNameText)
	{
		BuildingNameText->SetText(Definition.DisplayName);
		BuildingNameText->SetColorAndOpacity(FSlateColor(Definition.DisplayColor.GetClamped(0.48f, 1.0f)));
	}
	if (BuildingLevelText)
	{
		BuildingLevelText->SetText(FText::FromString(FString::Printf(
			TEXT("等级 %d / %d"), Level, Definition.MaximumLevel)));
	}
	if (BuildingDescriptionText) BuildingDescriptionText->SetText(Definition.Description);
	if (CurrentEffectText)
	{
		CurrentEffectText->SetText(FText::FromString(TEXT("当前效果：")
			+ UImmortalCaveLibrary::GetCurrentEffectText(State, SelectedBuilding).ToString()));
	}
	if (NextEffectText)
	{
		NextEffectText->SetText(Level >= Definition.MaximumLevel
			? FText::FromString(TEXT("已达到最高等级。"))
			: FText::FromString(TEXT("下级效果：")
				+ UImmortalCaveLibrary::GetNextEffectText(State, SelectedBuilding).ToString()));
	}
	if (UpgradeCostText)
	{
		if (Level >= Definition.MaximumLevel)
		{
			UpgradeCostText->SetText(FText::FromString(TEXT("升级消耗：已满级")));
		}
		else
		{
			const FText FormattedCost = UImmortalCraftingLibrary::FormatCost(
				Evaluation.Cost, Player->GetMaterialInventory(), Player->GetGold());
			UpgradeCostText->SetText(FText::FromString(FString::Printf(
				TEXT("升级消耗：%s\n%s"),
				*FormattedCost.ToString(), *Evaluation.Message.ToString())));
		}
		UpgradeCostText->SetColorAndOpacity(FSlateColor(Evaluation.bCanUpgrade || Level >= Definition.MaximumLevel
			? FLinearColor(0.96f, 0.84f, 0.55f, 1.0f)
			: FLinearColor(1.0f, 0.42f, 0.32f, 1.0f)));
	}
	if (UpgradeButton)
	{
		UpgradeButton->SetIsEnabled(Evaluation.bCanUpgrade);
	}
	if (UpgradeButtonText)
	{
		UpgradeButtonText->SetText(FText::FromString(Level >= Definition.MaximumLevel
			? TEXT("已满级")
			: (Evaluation.bBlockedByCaveHeart ? TEXT("先升级洞府核心") : TEXT("升级建筑"))));
	}
}

void UImmortalCaveWidget::SetResultMessage(const FText& Message, const bool bSucceeded)
{
	if (!ResultText) return;
	ResultText->SetText(Message);
	ResultText->SetColorAndOpacity(FSlateColor(bSucceeded
		? FLinearColor(0.44f, 1.0f, 0.67f, 1.0f)
		: FLinearColor(1.0f, 0.40f, 0.30f, 1.0f)));
}

void UImmortalCaveWidget::HandleCaveHeartClicked()
{
	SelectBuilding(EImmortalCaveBuildingType::CaveHeart);
}

void UImmortalCaveWidget::HandleMeditationRoomClicked()
{
	SelectBuilding(EImmortalCaveBuildingType::MeditationRoom);
}

void UImmortalCaveWidget::HandleSpiritVeinClicked()
{
	SelectBuilding(EImmortalCaveBuildingType::SpiritVein);
}

void UImmortalCaveWidget::HandleStoragePavilionClicked()
{
	SelectBuilding(EImmortalCaveBuildingType::StoragePavilion);
}

void UImmortalCaveWidget::HandleAlchemyRoomClicked()
{
	SelectBuilding(EImmortalCaveBuildingType::AlchemyRoom);
}

void UImmortalCaveWidget::HandleForgeRoomClicked()
{
	SelectBuilding(EImmortalCaveBuildingType::ForgeRoom);
}

void UImmortalCaveWidget::HandleSpiritFieldClicked()
{
	SelectBuilding(EImmortalCaveBuildingType::SpiritField);
}

void UImmortalCaveWidget::HandleUpgradeClicked()
{
	if (!Player.IsValid()) return;
	const FImmortalCaveUpgradeResult Result = Player->UpgradeCaveBuilding(SelectedBuilding);
	RefreshFromPlayer();
	SetResultMessage(Result.Message, Result.bSucceeded);
}

void UImmortalCaveWidget::HandleCollectClicked()
{
	if (!Player.IsValid()) return;
	const FImmortalCaveCollectionResult Result = Player->CollectCaveResources();
	RefreshFromPlayer();
	if (Result.bPersistenceFailed)
	{
		SetResultMessage(Result.Message, false);
		return;
	}
	SetResultMessage(
		Result.bCollectedAnything
			? FText::FromString(FString::Printf(
				TEXT("收取完成：灵石 +%d，灵草 +%d，矿石 +%d"),
				Result.SpiritStonesCollected, Result.SpiritGrassCollected, Result.OreCollected))
			: FText::FromString(TEXT("当前没有可收取的洞府产出。")),
		Result.bCollectedAnything);
}

void UImmortalCaveWidget::HandleAlchemyClicked()
{
	if (Player.IsValid()) Player->ToggleAlchemy();
}

void UImmortalCaveWidget::HandleCraftingClicked()
{
	if (Player.IsValid()) Player->ToggleCrafting();
}

void UImmortalCaveWidget::HandleFarmingClicked()
{
	if (Player.IsValid()) Player->ToggleFarming();
}

void UImmortalCaveWidget::HandleCloseClicked()
{
	if (Player.IsValid()) Player->ToggleCave();
}
