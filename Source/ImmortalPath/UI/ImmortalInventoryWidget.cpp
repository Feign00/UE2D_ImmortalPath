// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalInventoryWidget.h"

#include "ImmortalInventorySlotWidget.h"
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

	FSlateBrush MakeSolidBrush(const FVector2D Size, const FLinearColor Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(Color);
		return Brush;
	}

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
		const FLinearColor Color,
		const int32 FontSize = 13)
	{
		FInventoryTextButton Result;
		Result.Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style;
		const FSlateBrush Normal = MakeSolidBrush(Size, Color);
		const FSlateBrush Hovered = MakeSolidBrush(Size, Color * 1.18f);
		const FSlateBrush Pressed = MakeSolidBrush(Size, Color * 0.82f);
		Style.SetNormal(Normal);
		Style.SetHovered(Hovered);
		Style.SetPressed(Pressed);
		Style.SetDisabled(MakeSolidBrush(Size, FLinearColor(0.12f, 0.13f, 0.15f, 0.75f)));
		Result.Button->SetStyle(Style);
		SetCanvasLayout(Canvas->AddChildToCanvas(Result.Button), Position, Size);
		Result.Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(Name.ToString() + TEXT("Label")));
		Result.Text->SetText(FText::FromString(Label));
		Result.Text->SetJustification(ETextJustify::Center);
		SetTextAppearance(Result.Text, FontSize, FLinearColor::White);
		Result.Button->AddChild(Result.Text);
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

	USizeBox* RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("InventoryPanelSize"));
	RootBox->SetWidthOverride(1600.0f);
	RootBox->SetHeightOverride(300.0f);
	WidgetTree->RootWidget = RootBox;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InventoryCanvas"));
	RootBox->AddChild(Canvas);

	auto AddPanel = [this, Canvas](const FName Name, const FVector2D Position, const FVector2D Size, const FLinearColor Color)
	{
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		Image->SetBrush(MakeSolidBrush(Size, Color));
		SetCanvasLayout(Canvas->AddChildToCanvas(Image), Position, Size);
	};
	AddPanel(TEXT("InventoryBackground"), FVector2D::ZeroVector, FVector2D(1600.0f, 300.0f), FLinearColor(0.025f, 0.035f, 0.045f, 0.97f));
	AddPanel(TEXT("InventoryHeaderBand"), FVector2D(8.0f, 5.0f), FVector2D(1584.0f, 40.0f), FLinearColor(0.11f, 0.16f, 0.18f, 0.98f));
	AddPanel(TEXT("InventoryEquipmentPanel"), FVector2D(10.0f, 50.0f), FVector2D(390.0f, 240.0f), FLinearColor(0.07f, 0.09f, 0.11f, 0.94f));
	AddPanel(TEXT("InventoryBackpackPanel"), FVector2D(406.0f, 50.0f), FVector2D(696.0f, 240.0f), FLinearColor(0.055f, 0.07f, 0.085f, 0.94f));
	AddPanel(TEXT("InventoryDetailPanel"), FVector2D(1108.0f, 50.0f), FVector2D(482.0f, 240.0f), FLinearColor(0.07f, 0.09f, 0.11f, 0.94f));

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventoryTitle"));
	Title->SetText(FText::FromString(TEXT("储物戒 · 背包管理")));
	SetTextAppearance(Title, 20, FLinearColor(0.98f, 0.82f, 0.42f, 1.0f));
	SetCanvasLayout(Canvas->AddChildToCanvas(Title), FVector2D(18.0f, 10.0f), FVector2D(240.0f, 30.0f));

	const FInventoryTextButton EquipmentTab = AddTextButton(WidgetTree, Canvas, TEXT("EquipmentTabButton"), TEXT("装备"), FVector2D(278.0f, 9.0f), FVector2D(80.0f, 32.0f), FLinearColor(0.25f, 0.28f, 0.3f, 1.0f));
	EquipmentTab.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleEquipmentTabClicked);
	EquipmentTabText = EquipmentTab.Text;
	const FInventoryTextButton MaterialTab = AddTextButton(WidgetTree, Canvas, TEXT("MaterialTabButton"), TEXT("材料"), FVector2D(362.0f, 9.0f), FVector2D(80.0f, 32.0f), FLinearColor(0.21f, 0.3f, 0.25f, 1.0f));
	MaterialTab.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleMaterialTabClicked);
	MaterialTabText = MaterialTab.Text;
	const FInventoryTextButton PillTab = AddTextButton(WidgetTree, Canvas, TEXT("PillTabButton"), TEXT("丹药"), FVector2D(446.0f, 9.0f), FVector2D(80.0f, 32.0f), FLinearColor(0.29f, 0.22f, 0.32f, 1.0f));
	PillTab.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandlePillTabClicked);
	PillTabText = PillTab.Text;
	const FInventoryTextButton ArtifactTab = AddTextButton(WidgetTree, Canvas, TEXT("ArtifactTabButton"), TEXT("法宝"), FVector2D(530.0f, 9.0f), FVector2D(80.0f, 32.0f), FLinearColor(0.28f, 0.2f, 0.34f, 1.0f));
	ArtifactTab.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleArtifactTabClicked);
	ArtifactTabText = ArtifactTab.Text;
	const FInventoryTextButton QuestTab = AddTextButton(WidgetTree, Canvas, TEXT("QuestItemTabButton"), TEXT("任务物品"), FVector2D(614.0f, 9.0f), FVector2D(96.0f, 32.0f), FLinearColor(0.33f, 0.27f, 0.16f, 1.0f));
	QuestTab.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleQuestItemTabClicked);
	QuestItemTabText = QuestTab.Text;

	FInventoryTextButton Threshold = AddTextButton(WidgetTree, Canvas, TEXT("InventoryQualityButton"), TEXT("筛选≤凡品"), FVector2D(718.0f, 9.0f), FVector2D(100.0f, 32.0f), FLinearColor(0.23f, 0.28f, 0.34f, 1.0f), 12);
	Threshold.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleMaximumQualityClicked);
	MaximumQualityButton = Threshold.Button;
	MaximumQualityButtonText = Threshold.Text;
	const FInventoryTextButton Organize = AddTextButton(WidgetTree, Canvas, TEXT("InventoryOrganizeButton"), TEXT("整理"), FVector2D(822.0f, 9.0f), FVector2D(70.0f, 32.0f), FLinearColor(0.18f, 0.38f, 0.42f, 1.0f));
	Organize.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleOrganizeClicked);
	const FInventoryTextButton BulkSell = AddTextButton(WidgetTree, Canvas, TEXT("InventoryBulkSellButton"), TEXT("批量出售"), FVector2D(896.0f, 9.0f), FVector2D(94.0f, 32.0f), FLinearColor(0.18f, 0.43f, 0.28f, 1.0f), 12);
	BulkSell.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleBatchSellClicked);
	BatchSellButton = BulkSell.Button;
	const FInventoryTextButton BulkDismantle = AddTextButton(WidgetTree, Canvas, TEXT("InventoryBulkDismantleButton"), TEXT("批量分解"), FVector2D(994.0f, 9.0f), FVector2D(94.0f, 32.0f), FLinearColor(0.46f, 0.28f, 0.16f, 1.0f), 12);
	BulkDismantle.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleBatchDismantleClicked);
	BatchDismantleButton = BulkDismantle.Button;

	BackpackCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BackpackCount"));
	BackpackCountText->SetJustification(ETextJustify::Center);
	SetTextAppearance(BackpackCountText, 13, FLinearColor(0.72f, 0.8f, 0.84f, 1.0f));
	SetCanvasLayout(Canvas->AddChildToCanvas(BackpackCountText), FVector2D(1095.0f, 13.0f), FVector2D(170.0f, 26.0f));
	CombatPowerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CombatPowerText"));
	CombatPowerText->SetJustification(ETextJustify::Right);
	SetTextAppearance(CombatPowerText, 15, FLinearColor(1.0f, 0.68f, 0.2f, 1.0f));
	SetCanvasLayout(Canvas->AddChildToCanvas(CombatPowerText), FVector2D(1270.0f, 12.0f), FVector2D(265.0f, 27.0f));
	const FInventoryTextButton Close = AddTextButton(WidgetTree, Canvas, TEXT("InventoryCloseButton"), TEXT("×"), FVector2D(1545.0f, 9.0f), FVector2D(36.0f, 32.0f), FLinearColor(0.48f, 0.17f, 0.14f, 1.0f), 20);
	Close.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleCloseClicked);

	EquipmentTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EquipmentTitle"));
	SetTextAppearance(EquipmentTitleText, 15, FLinearColor(0.9f, 0.92f, 0.94f, 1.0f));
	SetCanvasLayout(Canvas->AddChildToCanvas(EquipmentTitleText), FVector2D(20.0f, 56.0f), FVector2D(360.0f, 24.0f));
	EquipmentGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("EquipmentGrid"));
	EquipmentGrid->SetSlotPadding(FMargin(2.0f));
	SetCanvasLayout(Canvas->AddChildToCanvas(EquipmentGrid), FVector2D(17.0f, 82.0f), FVector2D(376.0f, 150.0f));
	CategoryOverviewText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CategoryOverview"));
	CategoryOverviewText->SetAutoWrapText(true);
	SetTextAppearance(CategoryOverviewText, 13, FLinearColor(0.76f, 0.82f, 0.85f, 1.0f));
	SetCanvasLayout(Canvas->AddChildToCanvas(CategoryOverviewText), FVector2D(20.0f, 84.0f), FVector2D(365.0f, 190.0f));

	UScrollBox* BackpackScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BackpackScroll"));
	BackpackScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	SetCanvasLayout(Canvas->AddChildToCanvas(BackpackScroll), FVector2D(414.0f, 57.0f), FVector2D(680.0f, 226.0f));
	BackpackGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("BackpackGrid"));
	BackpackGrid->SetSlotPadding(FMargin(2.0f));
	BackpackScroll->AddChild(BackpackGrid);

	ItemNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SelectedItemName"));
	SetTextAppearance(ItemNameText, 17, FLinearColor::White);
	SetCanvasLayout(Canvas->AddChildToCanvas(ItemNameText), FVector2D(1122.0f, 58.0f), FVector2D(450.0f, 27.0f));
	UScrollBox* DetailScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("SelectedItemDetailScroll"));
	DetailScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	SetCanvasLayout(Canvas->AddChildToCanvas(DetailScroll), FVector2D(1122.0f, 87.0f), FVector2D(450.0f, 98.0f));
	ItemDetailsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SelectedItemDetails"));
	ItemDetailsText->SetAutoWrapText(true);
	SetTextAppearance(ItemDetailsText, 12, FLinearColor(0.86f, 0.88f, 0.9f, 1.0f));
	DetailScroll->AddChild(ItemDetailsText);
	ComparisonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SelectedItemComparison"));
	SetTextAppearance(ComparisonText, 12, FLinearColor(0.45f, 1.0f, 0.45f, 1.0f));
	SetCanvasLayout(Canvas->AddChildToCanvas(ComparisonText), FVector2D(1122.0f, 188.0f), FVector2D(450.0f, 24.0f));
	OperationMessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventoryOperationMessage"));
	OperationMessageText->SetAutoWrapText(true);
	SetTextAppearance(OperationMessageText, 12, FLinearColor(1.0f, 0.78f, 0.3f, 1.0f));
	SetCanvasLayout(Canvas->AddChildToCanvas(OperationMessageText), FVector2D(1122.0f, 214.0f), FVector2D(450.0f, 32.0f));

	FInventoryTextButton Lock = AddTextButton(WidgetTree, Canvas, TEXT("InventoryLockButton"), TEXT("锁定"), FVector2D(1122.0f, 251.0f), FVector2D(100.0f, 31.0f), FLinearColor(0.42f, 0.34f, 0.13f, 1.0f), 12);
	Lock.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleLockClicked);
	LockButton = Lock.Button;
	LockButtonText = Lock.Text;
	FInventoryTextButton SellSelected = AddTextButton(WidgetTree, Canvas, TEXT("InventorySellSelectedButton"), TEXT("出售此件"), FVector2D(1228.0f, 251.0f), FVector2D(104.0f, 31.0f), FLinearColor(0.16f, 0.4f, 0.25f, 1.0f), 12);
	SellSelected.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleSellSelectedClicked);
	SellSelectedButton = SellSelected.Button;
	FInventoryTextButton DismantleSelected = AddTextButton(WidgetTree, Canvas, TEXT("InventoryDismantleSelectedButton"), TEXT("分解此件"), FVector2D(1338.0f, 251.0f), FVector2D(104.0f, 31.0f), FLinearColor(0.44f, 0.25f, 0.14f, 1.0f), 12);
	DismantleSelected.Button->OnClicked.AddDynamic(this, &UImmortalInventoryWidget::HandleDismantleSelectedClicked);
	DismantleSelectedButton = DismantleSelected.Button;

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
}

void UImmortalInventoryWidget::RebuildEquipmentSlots()
{
	EquipmentGrid->ClearChildren();
	const bool bEquipment = ActiveCategory == EImmortalInventoryCategory::Equipment;
	EquipmentGrid->SetVisibility(bEquipment ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	CategoryOverviewText->SetVisibility(bEquipment ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (!bEquipment) return;
	AddEquipmentSlot(EImmortalEquipmentSlot::Weapon, 0, 0);
	AddEquipmentSlot(EImmortalEquipmentSlot::Head, 1, 0);
	AddEquipmentSlot(EImmortalEquipmentSlot::Chest, 2, 0);
	AddEquipmentSlot(EImmortalEquipmentSlot::Bracers, 3, 0);
	AddEquipmentSlot(EImmortalEquipmentSlot::Belt, 4, 0);
	AddEquipmentSlot(EImmortalEquipmentSlot::Boots, 0, 1);
	AddEquipmentSlot(EImmortalEquipmentSlot::RingLeft, 1, 1);
	AddEquipmentSlot(EImmortalEquipmentSlot::RingRight, 2, 1);
	AddEquipmentSlot(EImmortalEquipmentSlot::Accessory, 3, 1);
	AddArtifactEquipmentSlot(4, 1);
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
	if (UUniformGridSlot* GridSlot = EquipmentGrid->AddChildToUniformGrid(SlotWidget, Row, Column))
	{
		GridSlot->SetHorizontalAlignment(HAlign_Center);
		GridSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UImmortalInventoryWidget::AddArtifactEquipmentSlot(const int32 Column, const int32 Row)
{
	FImmortalArtifactItem Artifact;
	const bool bHasEquipped = Player->GetEquippedArtifact(Artifact);
	UImmortalInventorySlotWidget* SlotWidget = CreateWidget<UImmortalInventorySlotWidget>(
		GetOwningPlayer(), UImmortalInventorySlotWidget::StaticClass());
	SlotWidget->InitializeArtifactSlot(this, Artifact, bHasEquipped,
		bHasEquipped && Artifact.InstanceId == SelectedArtifactInstanceId);
	if (UUniformGridSlot* GridSlot = EquipmentGrid->AddChildToUniformGrid(SlotWidget, Row, Column))
	{
		GridSlot->SetHorizontalAlignment(HAlign_Center);
		GridSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UImmortalInventoryWidget::RebuildBackpackSlots()
{
	BackpackGrid->ClearChildren();
	constexpr int32 Columns = 9;
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
		const float CurrentPower = Player->GetEquippedItemForSlot(Item->Slot, Current)
			? UImmortalEquipmentLibrary::CalculateEquipmentPower(Current) : 0.0f;
		const float Difference = ItemPower - CurrentPower;
		ComparisonText->SetText(FText::FromString(FString::Printf(TEXT("相对当前装备 %+.1f 战力"), Difference)));
		ComparisonText->SetColorAndOpacity(FSlateColor(Difference >= 0.0f
			? FLinearColor(0.4f, 1.0f, 0.4f, 1.0f) : FLinearColor(1.0f, 0.4f, 0.35f, 1.0f)));
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
		? TEXT("战斗位 · 9件装备 + 1件独立法宝")
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
	MaximumQualityButtonText->SetText(FText::FromString(FString::Printf(TEXT("筛选%s"),
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
