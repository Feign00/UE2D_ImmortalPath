// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalInventoryWidget.h"

#include "ImmortalInventorySlotWidget.h"
#include "ImmortalInventoryPresentation.h"
#include "ImmortalFeaturePageLayout.h"
#include "ImmortalUITheme.h"
#include "Components/ScaleBox.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Styling/SlateTypes.h"

namespace
{
	struct FInventoryTextButton
	{
		UButton* Button = nullptr;
		UTextBlock* Text = nullptr;
	};

	void SetCanvasLayout(UCanvasPanelSlot* Slot, const FVector2D Position, const FVector2D Size)
	{
		if (!Slot) return;
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetAutoSize(false);
	}

	void SetTextAppearance(UTextBlock* Text, const int32 Size, const FLinearColor Color)
	{
		if (!Text) return;
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor::Black);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}

	FInventoryTextButton AddTextButton(
		UWidgetTree* Tree,
		UCanvasPanel* Canvas,
		const FName Name,
		const FString& Label,
		const FVector2D Position,
		const FVector2D Size,
		const int32 FontSize = 13)
	{
		FInventoryTextButton Result;
		Result.Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		SetCanvasLayout(Canvas->AddChildToCanvas(Result.Button), Position, Size);
		Result.Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(Name.ToString() + TEXT("Label")));
		Result.Text->SetText(FText::FromString(Label));
		Result.Text->SetJustification(ETextJustify::Center);
		SetTextAppearance(Result.Text, FontSize, FLinearColor::White);
		Result.Button->AddChild(Result.Text);
		Result.Button->SetStyle(ImmortalUITheme::ButtonStyle());
		ImmortalFeaturePageLayout::StabilizeButtonLabel(Result.Button, Result.Text);
		return Result;
	}

	const FImmortalEquipmentItem* FindEquipment(
		const TArray<FImmortalEquipmentItem>& Equipped,
		const TArray<FImmortalEquipmentItem>& Inventory,
		const FGuid ItemId,
		bool* bOutEquipped = nullptr)
	{
		if (bOutEquipped) *bOutEquipped = false;
		if (!ItemId.IsValid()) return nullptr;
		if (const FImmortalEquipmentItem* Found = Equipped.FindByPredicate([ItemId](const FImmortalEquipmentItem& Item)
		{
			return Item.ItemId == ItemId;
		}))
		{
			if (bOutEquipped) *bOutEquipped = true;
			return Found;
		}
		return Inventory.FindByPredicate([ItemId](const FImmortalEquipmentItem& Item)
		{
			return Item.ItemId == ItemId;
		});
	}

	FString FormatMaterialRewards(const TArray<FImmortalMaterialStack>& Materials)
	{
		TArray<FString> Parts;
		for (const FImmortalMaterialStack& Stack : Materials)
		{
			FImmortalMaterialDefinition Definition;
			const FString Name = UImmortalMaterialLibrary::GetMaterialDefinition(Stack.MaterialId, Definition)
				? Definition.DisplayName.ToString()
				: Stack.MaterialId.ToString();
			Parts.Add(FString::Printf(TEXT("%s×%d"), *Name, Stack.Quantity));
		}
		return FString::Join(Parts, TEXT("、"));
	}
}

void UImmortalInventoryWidget::InitializeForPlayer(AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalInventoryWidget::ResetTransientInteraction()
{
	ResetPendingAction();
	SetOperationMessage(FText::GetEmpty());
}

void UImmortalInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("InventoryPanelSize"));
	Root->SetWidthOverride(1600);
	Root->SetHeightOverride(600);
	WidgetTree->RootWidget = Root;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InventoryCanvas"));
	Root->AddChild(Canvas);

	const auto Panel = [this, Canvas](const TCHAR* Name, FVector2D Position, FVector2D Size)
	{
		UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		Background->SetBrush(ImmortalUITheme::PanelBrush(FLinearColor(0.035f, 0.065f, 0.065f)));
		Background->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetCanvasLayout(Canvas->AddChildToCanvas(Background), Position, Size);
	};
	const auto Text = [this, Canvas](const TCHAR* Name, const TCHAR* Caption, FVector2D Position, FVector2D Size, int32 Font)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetText(FText::FromString(Caption));
		SetTextAppearance(Label, Font, FLinearColor(0.94f, 0.89f, 0.75f));
		Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		SetCanvasLayout(Canvas->AddChildToCanvas(Label), Position, Size);
		return Label;
	};
	const auto Button = [this, Canvas](const TCHAR* Name, const TCHAR* Label, FVector2D Position, FVector2D Size)
	{
		return AddTextButton(WidgetTree, Canvas, Name, Label, Position, Size, 17);
	};
	const auto Scroll = [this, Canvas](const TCHAR* Name, FVector2D Position, FVector2D Size)
	{
		UScrollBox* Box = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), Name);
		ImmortalFeaturePageLayout::StyleScrollBox(Box);
		Box->SetClipping(EWidgetClipping::ClipToBounds);
		Box->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		SetCanvasLayout(Canvas->AddChildToCanvas(Box), Position, Size);
		return Box;
	};

	Panel(TEXT("InventoryHeaderBand"), {8, 2}, {1584, 48});
	Panel(TEXT("InventoryEquipmentPanel"), {8, 58}, {420, 534});
	Panel(TEXT("InventoryBackpackPanel"), {436, 58}, {660, 534});
	Panel(TEXT("InventoryDetailPanel"), {1104, 58}, {488, 534});
	Text(TEXT("InventoryTitle"), TEXT("储物戒"), {20, 6}, {170, 30}, 24);

	auto Tab = Button(TEXT("EquipmentTabButton"), TEXT("装备"), {210, 5}, {80, 30});
	Tab.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleEquipmentTabClicked); EquipmentTabText = Tab.Text;
	Tab = Button(TEXT("MaterialTabButton"), TEXT("材料"), {296, 5}, {80, 30});
	Tab.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleMaterialTabClicked); MaterialTabText = Tab.Text;
	Tab = Button(TEXT("PillTabButton"), TEXT("丹药"), {382, 5}, {80, 30});
	Tab.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandlePillTabClicked); PillTabText = Tab.Text;
	Tab = Button(TEXT("ArtifactTabButton"), TEXT("法宝"), {468, 5}, {80, 30});
	Tab.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleArtifactTabClicked); ArtifactTabText = Tab.Text;
	Tab = Button(TEXT("QuestItemTabButton"), TEXT("任务"), {554, 5}, {80, 30});
	Tab.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleQuestItemTabClicked); QuestItemTabText = Tab.Text;
	BackpackCountText = Text(TEXT("BackpackCount"), TEXT(""), {652, 8}, {310, 26}, 16);
	CombatPowerText = Text(TEXT("CombatPowerText"), TEXT(""), {1000, 8}, {530, 26}, 16);
	CombatPowerText->SetJustification(ETextJustify::Right);
	auto Close = Button(TEXT("InventoryCloseButton"), TEXT("×"), {1546, 5}, {34, 30});
	Close.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleCloseClicked);

	EquipmentGrid = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EquipmentGrid"));
	SetCanvasLayout(Canvas->AddChildToCanvas(EquipmentGrid), {24, 100}, {388, 480});
	UScaleBox* PortraitScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("InventoryPortraitScale"));
	PortraitScale->SetStretch(EStretch::ScaleToFit);
	PortraitScale->SetVisibility(ESlateVisibility::HitTestInvisible);
	SetCanvasLayout(Canvas->AddChildToCanvas(PortraitScale), {112, 100}, {200, 320});
	PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("InventoryPortrait"));
	PortraitScale->AddChild(PortraitImage);
	PortraitPanel = PortraitScale;
	EquipmentTitleText = Text(TEXT("EquipmentTitle"), TEXT("随身装备"), {24, 68}, {388, 28}, 20);
	EquipmentTitleText->SetJustification(ETextJustify::Center);
	CategoryOverviewText = Text(TEXT("CategoryOverview"), TEXT(""), {24, 100}, {388, 474}, 18);
	CategoryOverviewText->SetAutoWrapText(true);
	ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, CategoryOverviewText);

	Text(TEXT("BackpackSectionTitle"), TEXT("物品"), {452, 68}, {620, 28}, 20);
	UScrollBox* BackpackScroll = Scroll(TEXT("BackpackScroll"), {448, 100}, {634, 440});
	BackpackGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("BackpackGrid"));
	BackpackGrid->SetSlotPadding(FMargin(2));
	BackpackScroll->AddChild(BackpackGrid);
	auto Threshold = Button(TEXT("InventoryQualityButton"), TEXT("批量≤凡品"), {448, 550}, {182, 36});
	Threshold.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleMaximumQualityClicked);
	MaximumQualityButton = Threshold.Button; MaximumQualityButtonText = Threshold.Text;
	auto Organize = Button(TEXT("InventoryOrganizeButton"), TEXT("整理"), {638, 550}, {90, 36});
	Organize.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleOrganizeClicked);
	auto BulkSell = Button(TEXT("InventoryBulkSellButton"), TEXT("批量出售"), {736, 550}, {165, 36});
	BulkSell.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleBatchSellClicked); BatchSellButton = BulkSell.Button;
	auto BulkDismantle = Button(TEXT("InventoryBulkDismantleButton"), TEXT("批量分解"), {909, 550}, {165, 36});
	BulkDismantle.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleBatchDismantleClicked); BatchDismantleButton = BulkDismantle.Button;

	ItemNameText = Text(TEXT("SelectedItemName"), TEXT("请选择物品"), {1120, 68}, {456, 30}, 21);
	ItemDetailsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SelectedItemDetails"));
	ItemDetailsText->SetAutoWrapText(true);
	SetTextAppearance(ItemDetailsText, 17, FLinearColor(0.89f, 0.9f, 0.86f));
	Scroll(TEXT("SelectedItemDetailScroll"), {1120, 105}, {456, 220})->AddChild(ItemDetailsText);
	ComparisonText = Text(TEXT("SelectedItemComparison"), TEXT(""), {1120, 335}, {456, 156}, 16);
	ComparisonText->SetAutoWrapText(true);
	ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, ComparisonText);
	OperationMessageText = Text(TEXT("InventoryOperationMessage"), TEXT(""), {1120, 498}, {456, 40}, 16);
	OperationMessageText->SetAutoWrapText(true);
	ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, OperationMessageText);
	auto Lock = Button(TEXT("InventoryLockButton"), TEXT("锁定"), {1120, 550}, {120, 36});
	Lock.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleLockClicked); LockButton = Lock.Button; LockButtonText = Lock.Text;
	auto Sell = Button(TEXT("InventorySellSelectedButton"), TEXT("出售此件"), {1248, 550}, {156, 36});
	Sell.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleSellSelectedClicked); SellSelectedButton = Sell.Button;
	auto Dismantle = Button(TEXT("InventoryDismantleSelectedButton"), TEXT("分解此件"), {1412, 550}, {164, 36});
	Dismantle.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleDismantleSelectedClicked); DismantleSelectedButton = Dismantle.Button;
	RefreshFromPlayer();
}

void UImmortalInventoryWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Player.IsValid()) return;
	const bool bEquipmentChanged = LastEquipmentRevision != Player->GetEquipmentInventoryRevision();
	bool bActiveCategoryContentChanged = false;
	switch (ActiveCategory)
	{
	case EImmortalInventoryCategory::Equipment:
		bActiveCategoryContentChanged = bEquipmentChanged
			|| LastArtifactRevision != Player->GetArtifactInventoryRevision();
		break;
	case EImmortalInventoryCategory::Material:
		bActiveCategoryContentChanged = LastMaterialRevision != Player->GetMaterialInventoryRevision();
		break;
	case EImmortalInventoryCategory::Pill:
		bActiveCategoryContentChanged = LastPillRevision != Player->GetPillInventoryRevision();
		break;
	case EImmortalInventoryCategory::Artifact:
		bActiveCategoryContentChanged = LastArtifactRevision != Player->GetArtifactInventoryRevision();
		break;
	case EImmortalInventoryCategory::QuestItem:
		bActiveCategoryContentChanged = LastQuestItemRevision != Player->GetQuestItemInventoryRevision();
		break;
	default:
		break;
	}
	const bool bActionTargetsChanged = ActiveCategory == EImmortalInventoryCategory::Equipment && bEquipmentChanged;
	const bool bHeaderChanged = LastGold != Player->GetGold()
		|| !FMath::IsNearlyEqual(LastCombatPower, Player->GetCombatPower());
	if (bActiveCategoryContentChanged)
	{
		const bool bHadPendingAction = bActionTargetsChanged && PendingAction != EPendingAction::None;
		if (bActionTargetsChanged) ResetPendingAction();
		RefreshFromPlayer();
		if (bHadPendingAction)
		{
			SetOperationMessage(FText::FromString(TEXT("背包内容已变化，请重新确认操作。")), false);
		}
	}
	else if (bHeaderChanged)
	{
		LastGold = Player->GetGold();
		LastCombatPower = Player->GetCombatPower();
		CombatPowerText->SetText(FText::FromString(FString::Printf(
			TEXT("战斗力 %.1f · 灵石 %d"), LastCombatPower, LastGold)));
	}
}

void UImmortalInventoryWidget::RefreshFromPlayer()
{
	if (!Player.IsValid() || !EquipmentGrid || !BackpackGrid) return;
	LastEquipmentDropCount = Player->GetEquipmentDropCount();
	LastEquipmentRevision = Player->GetEquipmentInventoryRevision();
	LastMaterialRevision = Player->GetMaterialInventoryRevision();
	LastPillRevision = Player->GetPillInventoryRevision();
	LastArtifactRevision = Player->GetArtifactInventoryRevision();
	LastQuestItemRevision = Player->GetQuestItemInventoryRevision();
	LastGold = Player->GetGold();
	LastCombatPower = Player->GetCombatPower();
	CombatPowerText->SetText(FText::FromString(FString::Printf(TEXT("战斗力 %.1f · 灵石 %d"), LastCombatPower, LastGold)));

	const TArray<FImmortalEquipmentItem> Equipped = Player->GetEquippedItems();
	const TArray<FImmortalEquipmentItem> Inventory = Player->GetInventoryItems();
	if (ActiveCategory == EImmortalInventoryCategory::Equipment)
	{
		if (SelectedItemId.IsValid() && !FindEquipment(Equipped, Inventory, SelectedItemId)) SelectedItemId.Invalidate();
		if (!SelectedItemId.IsValid())
		{
			if (!Inventory.IsEmpty()) SelectedItemId = Inventory[0].ItemId;
			else if (!Equipped.IsEmpty()) SelectedItemId = Equipped[0].ItemId;
		}
		BackpackCountText->SetText(FText::FromString(FString::Printf(TEXT("装备 %d/%d"), Inventory.Num(), Player->GetInventoryCapacity())));
	}
	else if (ActiveCategory == EImmortalInventoryCategory::Material)
	{
		const TArray<FImmortalMaterialStack> Materials = Player->GetMaterialInventory();
		if (Player->GetMaterialQuantity(SelectedMaterialId) <= 0) SelectedMaterialId = Materials.IsEmpty() ? NAME_None : Materials[0].MaterialId;
		int64 Total = 0;
		for (const FImmortalMaterialStack& Stack : Materials) Total = FMath::Min<int64>(Total + Stack.Quantity, MAX_int32);
		BackpackCountText->SetText(FText::FromString(FString::Printf(TEXT("材料 %d种 / %lld份"), Materials.Num(), Total)));
	}
	else if (ActiveCategory == EImmortalInventoryCategory::Pill)
	{
		const TArray<FImmortalPillStack> Pills = Player->GetPillInventory();
		if (!Pills.ContainsByPredicate([this](const FImmortalPillStack& Stack)
		{
			return Stack.PillId == SelectedPillId && Stack.Quality == SelectedPillQuality;
		}))
		{
			SelectedPillId = Pills.IsEmpty() ? NAME_None : Pills[0].PillId;
			SelectedPillQuality = Pills.IsEmpty() ? EImmortalPillQuality::Ordinary : Pills[0].Quality;
		}
		int64 Total = 0;
		for (const FImmortalPillStack& Stack : Pills) Total = FMath::Min<int64>(Total + Stack.Quantity, MAX_int32);
		BackpackCountText->SetText(FText::FromString(FString::Printf(TEXT("丹药 %d种 / %lld枚"), Pills.Num(), Total)));
	}
	else if (ActiveCategory == EImmortalInventoryCategory::Artifact)
	{
		const TArray<FImmortalArtifactItem> Artifacts = Player->GetArtifactInventory();
		if (!Artifacts.ContainsByPredicate([this](const FImmortalArtifactItem& Item) { return Item.InstanceId == SelectedArtifactInstanceId; }))
		{
			SelectedArtifactInstanceId = Artifacts.IsEmpty() ? FGuid() : Artifacts[0].InstanceId;
		}
		BackpackCountText->SetText(FText::FromString(FString::Printf(TEXT("法宝 %d件"), Artifacts.Num())));
	}
	else
	{
		const TArray<FImmortalQuestItemStack> QuestItems = Player->GetQuestItemInventory();
		if (Player->GetQuestItemQuantity(SelectedQuestItemId) <= 0) SelectedQuestItemId = QuestItems.IsEmpty() ? NAME_None : QuestItems[0].QuestItemId;
		int64 Total = 0;
		for (const FImmortalQuestItemStack& Stack : QuestItems) Total = FMath::Min<int64>(Total + Stack.Quantity, MAX_int32);
		BackpackCountText->SetText(FText::FromString(FString::Printf(TEXT("任务物品 %d种 / %lld件"), QuestItems.Num(), Total)));
	}

	RefreshTabAppearance();
	RebuildEquipmentSlots();
	RebuildBackpackSlots();
	RefreshCategoryOverview();
	RefreshDetails();
	RefreshActionState();
	ForceLayoutPrepass();
}

void UImmortalInventoryWidget::RebuildEquipmentSlots()
{
	EquipmentGrid->ClearChildren();
	const bool bEquipment = ActiveCategory == EImmortalInventoryCategory::Equipment;
	EquipmentGrid->SetVisibility(bEquipment ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	CategoryOverviewText->GetParent()->SetVisibility(bEquipment ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	PortraitPanel->SetVisibility(bEquipment ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	EquipmentTitleText->SetVisibility(bEquipment ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bEquipment) return;
	if (UPaperFlipbook* Idle = Player->GetInventoryPortraitFlipbook())
	{
		PortraitImage->SetBrushFromAtlasInterface(Idle->GetSpriteAtFrame(0), true);
	}
	AddEquipmentSlot(EImmortalEquipmentSlot::Weapon, 0, 0);
	AddEquipmentSlot(EImmortalEquipmentSlot::Head, 5, 0);
	AddEquipmentSlot(EImmortalEquipmentSlot::Chest, 0, 1);
	AddEquipmentSlot(EImmortalEquipmentSlot::Bracers, 5, 1);
	AddEquipmentSlot(EImmortalEquipmentSlot::Belt, 0, 2);
	AddEquipmentSlot(EImmortalEquipmentSlot::Boots, 5, 2);
	AddEquipmentSlot(EImmortalEquipmentSlot::RingLeft, 0, 3);
	AddEquipmentSlot(EImmortalEquipmentSlot::RingRight, 1, 3);
	AddEquipmentSlot(EImmortalEquipmentSlot::Accessory, 2, 3);
	AddArtifactEquipmentSlot(3, 3);
}

void UImmortalInventoryWidget::AddEquipmentSlot(
	const EImmortalEquipmentSlot EquipmentSlot,
	const int32 Column,
	const int32 Row)
{
	FImmortalEquipmentItem Item;
	const bool bHasItem = Player->GetEquippedItemForSlot(EquipmentSlot, Item);
	UImmortalInventorySlotWidget* SlotWidget = CreateWidget<UImmortalInventorySlotWidget>(GetOwningPlayer(), UImmortalInventorySlotWidget::StaticClass());
	SlotWidget->InitializeSlot(this, Item, bHasItem, bHasItem, bHasItem && SelectedItemId == Item.ItemId, EquipmentSlot);
	SetCanvasLayout(EquipmentGrid->AddChildToCanvas(SlotWidget),
		ImmortalInventoryPresentation::EquipmentPosition(Column, Row), FVector2D(ImmortalInventoryPresentation::SlotSize));
}

void UImmortalInventoryWidget::AddArtifactEquipmentSlot(const int32 Column, const int32 Row)
{
	FImmortalArtifactItem Artifact;
	const bool bHasEquipped = Player->GetEquippedArtifact(Artifact);
	UImmortalInventorySlotWidget* SlotWidget = CreateWidget<UImmortalInventorySlotWidget>(
		GetOwningPlayer(), UImmortalInventorySlotWidget::StaticClass());
	SlotWidget->InitializeArtifactSlot(this, Artifact, bHasEquipped,
		bHasEquipped && Artifact.InstanceId == SelectedArtifactInstanceId);
	SetCanvasLayout(EquipmentGrid->AddChildToCanvas(SlotWidget),
		ImmortalInventoryPresentation::EquipmentPosition(Column, Row), FVector2D(ImmortalInventoryPresentation::SlotSize));
}

void UImmortalInventoryWidget::RebuildBackpackSlots()
{
	BackpackGrid->ClearChildren();
	constexpr int32 Columns = ImmortalInventoryPresentation::BackpackColumns;
	if (ActiveCategory == EImmortalInventoryCategory::Equipment)
	{
		const TArray<FImmortalEquipmentItem> Inventory = Player->GetInventoryItems();
		const int32 VisibleSlots = FMath::Max(Player->GetInventoryCapacity(), Inventory.Num());
		for (int32 Index = 0; Index < VisibleSlots; ++Index)
		{
			const bool bHasItem = Inventory.IsValidIndex(Index);
			const FImmortalEquipmentItem Item = bHasItem ? Inventory[Index] : FImmortalEquipmentItem();
			UImmortalInventorySlotWidget* SlotWidget = CreateWidget<UImmortalInventorySlotWidget>(GetOwningPlayer(), UImmortalInventorySlotWidget::StaticClass());
			SlotWidget->InitializeSlot(this, Item, bHasItem, false, bHasItem && Item.ItemId == SelectedItemId);
			BackpackGrid->AddChildToUniformGrid(SlotWidget, Index / Columns, Index % Columns);
		}
		return;
	}
	if (ActiveCategory == EImmortalInventoryCategory::Material)
	{
		TArray<FName> Ids = UImmortalMaterialLibrary::GetKnownMaterialIds();
		Ids.Sort([](const FName Left, const FName Right)
		{
			FImmortalMaterialDefinition A;
			FImmortalMaterialDefinition B;
			UImmortalMaterialLibrary::GetMaterialDefinition(Left, A);
			UImmortalMaterialLibrary::GetMaterialDefinition(Right, B);
			if (A.Category != B.Category) return static_cast<uint8>(A.Category) < static_cast<uint8>(B.Category);
			return A.DisplayName.ToString() < B.DisplayName.ToString();
		});
		for (int32 Index = 0; Index < Ids.Num(); ++Index)
		{
			FImmortalMaterialStack Stack{Ids[Index], Player->GetMaterialQuantity(Ids[Index])};
			UImmortalInventorySlotWidget* SlotWidget = CreateWidget<UImmortalInventorySlotWidget>(GetOwningPlayer(), UImmortalInventorySlotWidget::StaticClass());
			SlotWidget->InitializeMaterialSlot(this, Stack, Stack.IsValid() && Stack.MaterialId == SelectedMaterialId);
			BackpackGrid->AddChildToUniformGrid(SlotWidget, Index / Columns, Index % Columns);
		}
		return;
	}
	if (ActiveCategory == EImmortalInventoryCategory::Pill)
	{
		const TArray<FImmortalPillStack> Pills = Player->GetPillInventory();
		const int32 VisibleSlots = FMath::Max(Pills.Num(), Columns);
		for (int32 Index = 0; Index < VisibleSlots; ++Index)
		{
			const FImmortalPillStack Stack = Pills.IsValidIndex(Index) ? Pills[Index] : FImmortalPillStack();
			UImmortalInventorySlotWidget* SlotWidget = CreateWidget<UImmortalInventorySlotWidget>(GetOwningPlayer(), UImmortalInventorySlotWidget::StaticClass());
			SlotWidget->InitializePillSlot(this, Stack, Stack.IsValid() && Stack.PillId == SelectedPillId && Stack.Quality == SelectedPillQuality);
			BackpackGrid->AddChildToUniformGrid(SlotWidget, Index / Columns, Index % Columns);
		}
		return;
	}
	if (ActiveCategory == EImmortalInventoryCategory::Artifact)
	{
		const TArray<FImmortalArtifactItem> Artifacts = Player->GetArtifactInventory();
		FImmortalArtifactItem EquippedArtifact;
		const bool bHasEquipped = Player->GetEquippedArtifact(EquippedArtifact);
		const int32 VisibleSlots = FMath::Max(Artifacts.Num(), Columns);
		for (int32 Index = 0; Index < VisibleSlots; ++Index)
		{
			const FImmortalArtifactItem Item = Artifacts.IsValidIndex(Index) ? Artifacts[Index] : FImmortalArtifactItem();
			UImmortalInventorySlotWidget* SlotWidget = CreateWidget<UImmortalInventorySlotWidget>(GetOwningPlayer(), UImmortalInventorySlotWidget::StaticClass());
			SlotWidget->InitializeArtifactSlot(this, Item,
				bHasEquipped && Item.InstanceId == EquippedArtifact.InstanceId,
				Item.IsValid() && Item.InstanceId == SelectedArtifactInstanceId);
			BackpackGrid->AddChildToUniformGrid(SlotWidget, Index / Columns, Index % Columns);
		}
		return;
	}

	const TArray<FName> QuestIds = UImmortalInventoryLibrary::GetKnownQuestItemIds();
	for (int32 Index = 0; Index < QuestIds.Num(); ++Index)
	{
		FImmortalQuestItemStack Stack{QuestIds[Index], Player->GetQuestItemQuantity(QuestIds[Index])};
		UImmortalInventorySlotWidget* SlotWidget = CreateWidget<UImmortalInventorySlotWidget>(GetOwningPlayer(), UImmortalInventorySlotWidget::StaticClass());
		SlotWidget->InitializeQuestItemSlot(this, Stack, Stack.IsValid() && Stack.QuestItemId == SelectedQuestItemId);
		BackpackGrid->AddChildToUniformGrid(SlotWidget, Index / Columns, Index % Columns);
	}
}

void UImmortalInventoryWidget::RefreshDetails()
{
	ItemNameText->SetToolTipText(FText::GetEmpty());
	ComparisonText->SetColorAndOpacity(FLinearColor(0.87f, 0.88f, 0.79f));
	if (ActiveCategory == EImmortalInventoryCategory::Material)
	{
		FImmortalMaterialDefinition Definition;
		const int32 Quantity = Player->GetMaterialQuantity(SelectedMaterialId);
		if (Quantity <= 0 || !UImmortalMaterialLibrary::GetMaterialDefinition(SelectedMaterialId, Definition))
		{
			ItemNameText->SetText(FText::FromString(TEXT("尚未获得材料")));
			ItemNameText->SetColorAndOpacity(FSlateColor::UseForeground());
			ItemDetailsText->SetText(FText::FromString(TEXT("战斗掉落、离线挂机、洞府和灵田产物会在这里自动堆叠。")));
			ComparisonText->SetText(FText::GetEmpty());
			return;
		}
		ItemNameText->SetText(Definition.DisplayName);
		ItemNameText->SetColorAndOpacity(FSlateColor(Definition.DisplayColor));
		ItemDetailsText->SetText(FText::FromString(FString::Printf(TEXT("%s ×%d\n%s"),
			*UImmortalMaterialLibrary::GetCategoryText(Definition.Category).ToString(), Quantity, *Definition.Description.ToString())));
		ComparisonText->SetText(FText::FromString(TEXT("用于炼丹、炼器、法宝与百宝阁交易")));
		return;
	}
	if (ActiveCategory == EImmortalInventoryCategory::Pill)
	{
		FImmortalPillDefinition Definition;
		const int32 Quantity = Player->GetPillQuantity(SelectedPillId, SelectedPillQuality);
		if (Quantity <= 0 || !UImmortalAlchemyLibrary::GetPillDefinition(SelectedPillId, Definition))
		{
			ItemNameText->SetText(FText::FromString(TEXT("尚未炼得丹药")));
			ItemNameText->SetColorAndOpacity(FSlateColor::UseForeground());
			ItemDetailsText->SetText(FText::FromString(TEXT("按 L 打开炼丹炉，选择丹方炼制与服用。")));
			ComparisonText->SetText(FText::GetEmpty());
			return;
		}
		ItemNameText->SetText(FText::FromString(FString::Printf(TEXT("%s · %s"), *Definition.DisplayName.ToString(),
			*UImmortalAlchemyLibrary::GetQualityText(SelectedPillQuality).ToString())));
		ItemNameText->SetColorAndOpacity(FSlateColor(UImmortalAlchemyLibrary::GetQualityColor(SelectedPillQuality)));
		ItemDetailsText->SetText(FText::FromString(FString::Printf(TEXT("持有 %d 枚\n%s\n药效：%s"), Quantity,
			*Definition.Description.ToString(), *Player->GetEffectivePillEffectText(SelectedPillId, SelectedPillQuality).ToString())));
		ComparisonText->SetText(FText::FromString(TEXT("按 L 前往炼丹炉服用")));
		return;
	}
	if (ActiveCategory == EImmortalInventoryCategory::Artifact)
	{
		const TArray<FImmortalArtifactItem> Artifacts = Player->GetArtifactInventory();
		const FImmortalArtifactItem* Item = Artifacts.FindByPredicate([this](const FImmortalArtifactItem& Entry)
		{
			return Entry.InstanceId == SelectedArtifactInstanceId;
		});
		FImmortalArtifactDefinition Definition;
		if (!Item || !UImmortalArtifactLibrary::GetArtifactDefinition(Item->ArtifactId, Definition))
		{
			ItemNameText->SetText(FText::FromString(TEXT("尚未拥有法宝")));
			ItemNameText->SetColorAndOpacity(FSlateColor::UseForeground());
			ItemDetailsText->SetText(FText::FromString(TEXT("法宝会在百宝阁或炼器系统中获得，详情培养仍在法宝 [F] 界面。")));
			ComparisonText->SetText(FText::GetEmpty());
			return;
		}
		FImmortalArtifactItem Equipped;
		const bool bEquipped = Player->GetEquippedArtifact(Equipped) && Equipped.InstanceId == Item->InstanceId;
		ItemNameText->SetText(Definition.DisplayName);
		ItemNameText->SetColorAndOpacity(FSlateColor(UImmortalArtifactLibrary::GetQualityColor(Definition.Quality)));
		ItemDetailsText->SetText(FText::FromString(FString::Printf(TEXT("%s · 等级%d · 星级%d%s\n%s\n%s"),
			*UImmortalArtifactLibrary::GetQualityText(Definition.Quality).ToString(), Item->Level, Item->Stars,
			Item->bLocked ? TEXT(" · 已锁定") : TEXT(""),
			*UImmortalArtifactLibrary::GetActiveEffectText(*Item).ToString(),
			*UImmortalArtifactLibrary::GetPassiveEffectText(*Item).ToString())));
		ComparisonText->SetText(FText::FromString(bEquipped ? TEXT("已装备 · 按 F 管理法宝") : TEXT("未装备 · 按 F 管理法宝")));
		return;
	}
	if (ActiveCategory == EImmortalInventoryCategory::QuestItem)
	{
		FImmortalQuestItemDefinition Definition;
		const int32 Quantity = Player->GetQuestItemQuantity(SelectedQuestItemId);
		if (Quantity <= 0 || !UImmortalInventoryLibrary::GetQuestItemDefinition(SelectedQuestItemId, Definition))
		{
			ItemNameText->SetText(FText::FromString(TEXT("暂无任务物品")));
			ItemNameText->SetColorAndOpacity(FSlateColor::UseForeground());
			ItemDetailsText->SetText(FText::FromString(TEXT("主线、历练和后续任务获得的凭证会在这里独立保存。")));
			ComparisonText->SetText(FText::FromString(TEXT("任务物品不可出售、不可分解")));
			return;
		}
		ItemNameText->SetText(Definition.DisplayName);
		ItemNameText->SetColorAndOpacity(FSlateColor(Definition.DisplayColor));
		ItemDetailsText->SetText(FText::FromString(FString::Printf(TEXT("持有 ×%d\n%s"), Quantity, *Definition.Description.ToString())));
		ComparisonText->SetText(FText::FromString(TEXT("受任务保护，不占装备背包容量")));
		return;
	}

	const TArray<FImmortalEquipmentItem> Equipped = Player->GetEquippedItems();
	const TArray<FImmortalEquipmentItem> Inventory = Player->GetInventoryItems();
	bool bEquipped = false;
	const FImmortalEquipmentItem* Item = FindEquipment(Equipped, Inventory, SelectedItemId, &bEquipped);
	if (!Item)
	{
		ItemNameText->SetText(FText::FromString(TEXT("请选择装备")));
		ItemNameText->SetColorAndOpacity(FSlateColor::UseForeground());
		ItemDetailsText->SetText(FText::FromString(TEXT("装备自动拾取并比较战力；锁定后不会被出售、分解或满包替换。")));
		ComparisonText->SetText(FText::GetEmpty());
		return;
	}
	ItemNameText->SetText(FText::FromName(Item->DisplayName));
	ItemNameText->SetToolTipText(ItemNameText->GetText());
	ItemNameText->SetColorAndOpacity(FSlateColor(UImmortalEquipmentLibrary::GetQualityColor(Item->Quality)));
	const float ItemPower = UImmortalEquipmentLibrary::CalculateEquipmentPower(*Item);
	FString Details = FString::Printf(
		TEXT("%s · %s契合 · 等级%d · 强化+%d · 洗炼%d%s\n攻击 %.1f  防御 %.1f  生命 %.1f\n攻速 %.1f%%  暴击 %.1f%%  暴伤 %.1f%%\n火/雷/冰 %.1f%% / %.1f%% / %.1f%%  吸血 %.1f%%\n修炼 %.1f%%  掉率 %.1f%%  首领 %.1f%%  战力 %.1f"),
		*UImmortalEquipmentLibrary::GetSlotText(Item->Slot).ToString(),
		*UImmortalEquipmentLibrary::GetDisciplineText(Item->Discipline).ToString(), Item->ItemLevel,
		Item->EnhancementLevel, Item->RefinementCount, Item->bLocked ? TEXT(" · 已锁定") : TEXT(""),
		Item->AttackBonus, Item->DefenseBonus, Item->HealthBonus,
		Item->AttackSpeedBonus * 100.0f, Item->CriticalChanceBonus * 100.0f, Item->CriticalDamageBonus * 100.0f,
		Item->FireDamageBonus * 100.0f, Item->ThunderDamageBonus * 100.0f, Item->IceDamageBonus * 100.0f,
		Item->LifeStealBonus * 100.0f, Item->CultivationGainBonus * 100.0f,
		Item->LootFindBonus * 100.0f, Item->BossDamageBonus * 100.0f, ItemPower);
	FImmortalEquipmentSetDefinition SetDefinition;
	if (UImmortalEquipmentLibrary::GetSetDefinition(Item->SetId, SetDefinition))
	{
		const int32 EquippedPieces = Player->GetActiveEquipmentSetBonuses().PieceCounts.FindRef(Item->SetId);
		Details += FString::Printf(TEXT("\n套装：%s %d/6"), *SetDefinition.DisplayName.ToString(), EquippedPieces);
		for (const FImmortalEquipmentSetTier& Tier : SetDefinition.Tiers)
		{
			Details += FString::Printf(TEXT("\n %d件 %s %s"), Tier.RequiredPieces,
				EquippedPieces >= Tier.RequiredPieces ? TEXT("√") : TEXT("○"), *Tier.Description.ToString());
		}
	}
	Details += TEXT("\n词条：");
	for (const FImmortalEquipmentAffix& Affix : Item->Affixes)
	{
		Details += FString::Printf(TEXT("\n · %s"), *UImmortalEquipmentLibrary::GetAffixText(Affix).ToString());
	}
	ItemDetailsText->SetText(FText::FromString(Details));
	if (bEquipped)
	{
		ComparisonText->SetText(FText::FromString(TEXT("已装备")));
		ComparisonText->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.85f, 1.0f, 1.0f)));
	}
	else
	{
		FImmortalEquipmentItem Current;
		const bool bHasCurrent = Player->GetEquippedItemForSlot(Item->Slot, Current);
		ComparisonText->SetText(FText::FromString(ImmortalInventoryPresentation::Compare(*Item, Current, bHasCurrent)));
	}
}

void UImmortalInventoryWidget::RefreshTabAppearance()
{
	auto ColorTab = [this](UTextBlock* Text, const EImmortalInventoryCategory Category, const FLinearColor ActiveColor)
	{
		if (Text) Text->SetColorAndOpacity(FSlateColor(ActiveCategory == Category
			? ActiveColor : FLinearColor(0.66f, 0.69f, 0.72f, 1.0f)));
	};
	ColorTab(EquipmentTabText, EImmortalInventoryCategory::Equipment, FLinearColor(1.0f, 0.78f, 0.28f, 1.0f));
	ColorTab(MaterialTabText, EImmortalInventoryCategory::Material, FLinearColor(0.32f, 1.0f, 0.68f, 1.0f));
	ColorTab(PillTabText, EImmortalInventoryCategory::Pill, FLinearColor(0.92f, 0.5f, 1.0f, 1.0f));
	ColorTab(ArtifactTabText, EImmortalInventoryCategory::Artifact, FLinearColor(0.77f, 0.42f, 1.0f, 1.0f));
	ColorTab(QuestItemTabText, EImmortalInventoryCategory::QuestItem, FLinearColor(1.0f, 0.76f, 0.3f, 1.0f));
	EquipmentTitleText->SetText(FText::FromString(ActiveCategory == EImmortalInventoryCategory::Equipment
		? TEXT("随身装备 · 独立法宝")
		: FString::Printf(TEXT("%s概览"), *UImmortalInventoryLibrary::GetCategoryText(ActiveCategory).ToString())));
}

void UImmortalInventoryWidget::RefreshCategoryOverview()
{
	if (!CategoryOverviewText || ActiveCategory == EImmortalInventoryCategory::Equipment) return;
	FString Summary;
	if (ActiveCategory == EImmortalInventoryCategory::Material)
	{
		TMap<EImmortalMaterialCategory, int32> Totals;
		for (const FImmortalMaterialStack& Stack : Player->GetMaterialInventory())
		{
			FImmortalMaterialDefinition Definition;
			if (UImmortalMaterialLibrary::GetMaterialDefinition(Stack.MaterialId, Definition)) Totals.FindOrAdd(Definition.Category) += Stack.Quantity;
		}
		Summary = TEXT("材料独立堆叠，不占装备背包容量。\n\n");
		for (int32 Index = 0; Index <= static_cast<int32>(EImmortalMaterialCategory::Artifact); ++Index)
		{
			const EImmortalMaterialCategory Category = static_cast<EImmortalMaterialCategory>(Index);
			Summary += FString::Printf(TEXT("%s  %d\n"), *UImmortalMaterialLibrary::GetCategoryText(Category).ToString(), Totals.FindRef(Category));
		}
	}
	else if (ActiveCategory == EImmortalInventoryCategory::Pill)
	{
		int32 Ordinary = 0;
		int32 Exceptional = 0;
		for (const FImmortalPillStack& Stack : Player->GetPillInventory())
		{
			(Stack.Quality == EImmortalPillQuality::Exceptional ? Exceptional : Ordinary) += Stack.Quantity;
		}
		Summary = FString::Printf(TEXT("丹药独立堆叠。\n\n普通丹药  %d\n极品丹药  %d\n\n按 L 打开炼丹炉炼制与服用。"), Ordinary, Exceptional);
	}
	else if (ActiveCategory == EImmortalInventoryCategory::Artifact)
	{
		int32 Locked = 0;
		for (const FImmortalArtifactItem& Item : Player->GetArtifactInventory()) Locked += Item.bLocked ? 1 : 0;
		Summary = FString::Printf(TEXT("法宝不占装备背包容量。\n\n当前拥有  %d\n已经锁定  %d\n\n可在此查看和保护法宝；按 F 进行装备、升级与升星。"),
			Player->GetArtifactInventory().Num(), Locked);
	}
	else
	{
		Summary = FString::Printf(TEXT("任务物品独立保存，不占装备容量。\n\n当前持有  %d 种\n\n任务物品不可出售、不可分解，也不会参与满包自动替换。"),
			Player->GetQuestItemInventory().Num());
	}
	CategoryOverviewText->SetText(FText::FromString(Summary));
}

void UImmortalInventoryWidget::RefreshActionState()
{
	const bool bEquipmentCategory = ActiveCategory == EImmortalInventoryCategory::Equipment;
	MaximumQualityButton->SetVisibility(bEquipmentCategory ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	BatchSellButton->SetVisibility(bEquipmentCategory ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	BatchDismantleButton->SetVisibility(bEquipmentCategory ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	MaximumQualityButtonText->SetText(FText::FromString(FString::Printf(TEXT("批量%s"),
		*UImmortalInventoryLibrary::GetMaximumQualityText(MaximumBulkQuality).ToString())));
	const int32 BulkCount = bEquipmentCategory ? Player->GetBulkEquipmentCount(MaximumBulkQuality) : 0;
	BatchSellButton->SetIsEnabled(BulkCount > 0);
	BatchDismantleButton->SetIsEnabled(BulkCount > 0);

	bool bCanLock = false;
	bool bLocked = false;
	bool bCanDestroySelected = false;
	if (bEquipmentCategory)
	{
		bool bEquipped = false;
		const TArray<FImmortalEquipmentItem> Equipped = Player->GetEquippedItems();
		const TArray<FImmortalEquipmentItem> Inventory = Player->GetInventoryItems();
		const FImmortalEquipmentItem* Item = FindEquipment(Equipped, Inventory, SelectedItemId, &bEquipped);
		bCanLock = Item != nullptr;
		bLocked = Item && Item->bLocked;
		bCanDestroySelected = Item && !bEquipped && !Item->bLocked;
	}
	else if (ActiveCategory == EImmortalInventoryCategory::Artifact)
	{
		const TArray<FImmortalArtifactItem> Artifacts = Player->GetArtifactInventory();
		const FImmortalArtifactItem* Item = Artifacts.FindByPredicate([this](const FImmortalArtifactItem& Entry)
		{
			return Entry.InstanceId == SelectedArtifactInstanceId;
		});
		bCanLock = Item != nullptr;
		bLocked = Item && Item->bLocked;
	}
	LockButton->SetVisibility((bEquipmentCategory || ActiveCategory == EImmortalInventoryCategory::Artifact)
		? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	LockButton->SetIsEnabled(bCanLock);
	LockButtonText->SetText(FText::FromString(bLocked ? TEXT("解锁") : TEXT("锁定")));
	SellSelectedButton->SetVisibility(bEquipmentCategory ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	DismantleSelectedButton->SetVisibility(bEquipmentCategory ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SellSelectedButton->SetIsEnabled(bCanDestroySelected);
	DismantleSelectedButton->SetIsEnabled(bCanDestroySelected);
}

void UImmortalInventoryWidget::SetOperationMessage(const FText& Message, const bool bSuccess)
{
	if (!OperationMessageText) return;
	OperationMessageText->SetText(Message);
	OperationMessageText->SetToolTipText(Message);
	if (UScrollBox* Scroll = Cast<UScrollBox>(OperationMessageText->GetParent())) Scroll->ScrollToStart();
	OperationMessageText->SetColorAndOpacity(FSlateColor(bSuccess
		? FLinearColor(0.42f, 1.0f, 0.56f, 1.0f)
		: FLinearColor(1.0f, 0.74f, 0.28f, 1.0f)));
}

void UImmortalInventoryWidget::ResetPendingAction()
{
	PendingAction = EPendingAction::None;
}

void UImmortalInventoryWidget::ShowCategory(const EImmortalInventoryCategory Category)
{
	if (ActiveCategory == Category)
	{
		ResetTransientInteraction();
		RefreshFromPlayer();
		return;
	}
	ActiveCategory = Category;
	if (UScrollBox* Scroll = Cast<UScrollBox>(BackpackGrid->GetParent())) Scroll->ScrollToStart();
	ResetPendingAction();
	SetOperationMessage(FText::GetEmpty());
	RefreshFromPlayer();
}

void UImmortalInventoryWidget::ShowMaterialTab() { ShowCategory(EImmortalInventoryCategory::Material); }
void UImmortalInventoryWidget::ShowPillTab() { ShowCategory(EImmortalInventoryCategory::Pill); }
void UImmortalInventoryWidget::ShowArtifactTab() { ShowCategory(EImmortalInventoryCategory::Artifact); }
void UImmortalInventoryWidget::ShowQuestItemTab() { ShowCategory(EImmortalInventoryCategory::QuestItem); }
void UImmortalInventoryWidget::HandleEquipmentTabClicked() { ShowCategory(EImmortalInventoryCategory::Equipment); }
void UImmortalInventoryWidget::HandleMaterialTabClicked() { ShowMaterialTab(); }
void UImmortalInventoryWidget::HandlePillTabClicked() { ShowPillTab(); }
void UImmortalInventoryWidget::HandleArtifactTabClicked() { ShowArtifactTab(); }
void UImmortalInventoryWidget::HandleQuestItemTabClicked() { ShowQuestItemTab(); }

void UImmortalInventoryWidget::HandleSlotSelected(const FGuid& ItemId)
{
	if (UScrollBox* Scroll = Cast<UScrollBox>(ItemDetailsText->GetParent())) Scroll->ScrollToStart();
	if (UScrollBox* Scroll = Cast<UScrollBox>(ComparisonText->GetParent())) Scroll->ScrollToStart();
	SelectedItemId = ItemId;
	ResetTransientInteraction();
	RefreshFromPlayer();
}

void UImmortalInventoryWidget::HandleMaterialSelected(const FName MaterialId)
{
	SelectedMaterialId = MaterialId;
	ResetTransientInteraction();
	RefreshFromPlayer();
}

void UImmortalInventoryWidget::HandlePillSelected(const FName PillId, const EImmortalPillQuality Quality)
{
	SelectedPillId = PillId;
	SelectedPillQuality = Quality;
	ResetTransientInteraction();
	RefreshFromPlayer();
}

void UImmortalInventoryWidget::HandleArtifactSelected(const FGuid& InstanceId)
{
	SelectedArtifactInstanceId = InstanceId;
	if (ActiveCategory == EImmortalInventoryCategory::Equipment)
	{
		ShowArtifactTab();
		return;
	}
	ResetTransientInteraction();
	RefreshFromPlayer();
}

void UImmortalInventoryWidget::HandleQuestItemSelected(const FName QuestItemId)
{
	SelectedQuestItemId = QuestItemId;
	ResetTransientInteraction();
	RefreshFromPlayer();
}

void UImmortalInventoryWidget::HandleOrganizeClicked()
{
	if (!Player.IsValid()) return;
	ResetPendingAction();
	const FImmortalInventoryOperationResult Result = Player->OrganizeInventory();
	RefreshFromPlayer();
	SetOperationMessage(Result.Message, Result.bSucceeded);
}

void UImmortalInventoryWidget::HandleLockClicked()
{
	if (!Player.IsValid()) return;
	ResetPendingAction();
	FImmortalInventoryOperationResult Result;
	if (ActiveCategory == EImmortalInventoryCategory::Equipment && SelectedItemId.IsValid())
	{
		Result = Player->SetEquipmentLocked(SelectedItemId, !Player->IsEquipmentLocked(SelectedItemId));
	}
	else if (ActiveCategory == EImmortalInventoryCategory::Artifact && SelectedArtifactInstanceId.IsValid())
	{
		Result = Player->SetArtifactLocked(SelectedArtifactInstanceId, !Player->IsArtifactLocked(SelectedArtifactInstanceId));
	}
	RefreshFromPlayer();
	SetOperationMessage(Result.Message, Result.bSucceeded);
}

void UImmortalInventoryWidget::HandleMaximumQualityClicked()
{
	const int32 Next = (static_cast<int32>(MaximumBulkQuality) + 1)
		% (static_cast<int32>(EImmortalEquipmentQuality::Divine) + 1);
	MaximumBulkQuality = static_cast<EImmortalEquipmentQuality>(Next);
	ResetPendingAction();
	RefreshActionState();
	SetOperationMessage(FText::FromString(FString::Printf(TEXT("批量操作仅处理未锁定的%s装备"),
		*UImmortalInventoryLibrary::GetMaximumQualityText(MaximumBulkQuality).ToString())));
}

void UImmortalInventoryWidget::HandleSellSelectedClicked()
{
	if (!Player.IsValid() || !SelectedItemId.IsValid()) return;
	if (PendingAction != EPendingAction::SellSelected)
	{
		PendingAction = EPendingAction::SellSelected;
		SetOperationMessage(FText::FromString(FString::Printf(TEXT("再次点击确认出售，预计获得灵石 %d"),
			Player->GetEquipmentShopSellPrice(SelectedItemId))));
		return;
	}
	ResetPendingAction();
	const FImmortalShopTransactionResult Result = Player->SellEquipmentToShop(SelectedItemId);
	RefreshFromPlayer();
	SetOperationMessage(Result.Message, Result.bSucceeded);
}

void UImmortalInventoryWidget::HandleDismantleSelectedClicked()
{
	if (!Player.IsValid() || !SelectedItemId.IsValid()) return;
	if (PendingAction != EPendingAction::DismantleSelected)
	{
		PendingAction = EPendingAction::DismantleSelected;
		FImmortalEquipmentItem Item;
		bool bEquipped = false;
		Player->GetEquipmentItemById(SelectedItemId, Item, bEquipped);
		SetOperationMessage(FText::FromString(FString::Printf(TEXT("再次点击确认分解：%s"),
			*FormatMaterialRewards(UImmortalInventoryLibrary::GetEquipmentDismantleYield(Item)))));
		return;
	}
	ResetPendingAction();
	const FImmortalInventoryOperationResult Result = Player->DismantleEquipment(SelectedItemId);
	RefreshFromPlayer();
	SetOperationMessage(Result.Message, Result.bSucceeded);
}

void UImmortalInventoryWidget::HandleBatchSellClicked()
{
	if (!Player.IsValid()) return;
	if (PendingAction != EPendingAction::BatchSell)
	{
		PendingAction = EPendingAction::BatchSell;
		SetOperationMessage(FText::FromString(FString::Printf(TEXT("再次点击确认：出售 %d 件未锁装备，获得灵石 %d"),
			Player->GetBulkEquipmentCount(MaximumBulkQuality), Player->GetBulkEquipmentSellValue(MaximumBulkQuality))));
		return;
	}
	ResetPendingAction();
	const FImmortalInventoryOperationResult Result = Player->BatchSellEquipment(MaximumBulkQuality);
	RefreshFromPlayer();
	SetOperationMessage(Result.Message, Result.bSucceeded);
}

void UImmortalInventoryWidget::HandleBatchDismantleClicked()
{
	if (!Player.IsValid()) return;
	if (PendingAction != EPendingAction::BatchDismantle)
	{
		PendingAction = EPendingAction::BatchDismantle;
		SetOperationMessage(FText::FromString(FString::Printf(TEXT("再次点击确认：分解 %d 件未锁装备，获得 %s"),
			Player->GetBulkEquipmentCount(MaximumBulkQuality),
			*FormatMaterialRewards(Player->GetBulkEquipmentDismantleYield(MaximumBulkQuality)))));
		return;
	}
	ResetPendingAction();
	const FImmortalInventoryOperationResult Result = Player->BatchDismantleEquipment(MaximumBulkQuality);
	RefreshFromPlayer();
	SetOperationMessage(Result.Message, Result.bSucceeded);
}

void UImmortalInventoryWidget::HandleCloseClicked()
{
	ResetTransientInteraction();
	if (Player.IsValid()) Player->ToggleInventory();
}
