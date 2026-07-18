// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalArtifactWidget.h"

#include "ImmortalArtifactEntryWidget.h"
#include "../Artifacts/ImmortalArtifactTypes.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Crafting/ImmortalCraftingTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

namespace
{
	FSlateBrush MakeArtifactBrush(const TCHAR* AssetPath, const FVector2D Size, const FLinearColor Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(Tint);
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, AssetPath)) Brush.SetResourceObject(Texture);
		return Brush;
	}

	void SetArtifactLayout(UCanvasPanelSlot* Slot, const FVector2D Position, const FVector2D Size)
	{
		if (!Slot) return;
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetAutoSize(false);
	}

	void StyleArtifactText(UTextBlock* Text, const int32 Size, const FLinearColor& Color)
	{
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor::Black);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}

	FButtonStyle MakeArtifactButtonStyle(const FVector2D Size, const FLinearColor& Tint)
	{
		const FSlateBrush Brush = MakeArtifactBrush(
			TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal"), Size, Tint);
		FButtonStyle Style;
		Style.SetNormal(Brush);
		Style.SetHovered(Brush);
		Style.SetPressed(Brush);
		Style.SetDisabled(Brush);
		return Style;
	}

	UTextBlock* AddButtonLabel(UWidgetTree* Tree, UButton* Button, const TCHAR* Label)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		Text->SetJustification(ETextJustify::Center);
		StyleArtifactText(Text, 16, FLinearColor::White);
		Button->AddChild(Text);
		return Text;
	}
}

void UImmortalArtifactWidget::InitializeForPlayer(AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalArtifactWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ArtifactPanelSize"));
	Root->SetWidthOverride(900.0f);
	Root->SetHeightOverride(600.0f);
	WidgetTree->RootWidget = Root;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ArtifactCanvas"));
	Root->AddChild(Canvas);

	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ArtifactBackground"));
	Background->SetBrush(MakeArtifactBrush(
		TEXT("/Game/GAME/Asset/ui/inventory/panel_background.panel_background"), FVector2D(900.0f, 600.0f),
		FLinearColor(0.82f, 0.78f, 0.92f, 0.98f)));
	SetArtifactLayout(Canvas->AddChildToCanvas(Background), FVector2D::ZeroVector, FVector2D(900.0f, 600.0f));

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ArtifactTitle"));
	Title->SetText(FText::FromString(TEXT("万宝炉 · 法宝炼制与蕴养")));
	StyleArtifactText(Title, 28, FLinearColor(0.92f, 0.72f, 1.0f));
	SetArtifactLayout(Canvas->AddChildToCanvas(Title), FVector2D(28.0f, 16.0f), FVector2D(390.0f, 42.0f));
	CurrencyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ArtifactCurrency"));
	CurrencyText->SetJustification(ETextJustify::Right);
	StyleArtifactText(CurrencyText, 16, FLinearColor(0.64f, 0.95f, 1.0f));
	SetArtifactLayout(Canvas->AddChildToCanvas(CurrencyText), FVector2D(500.0f, 25.0f), FVector2D(300.0f, 30.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ArtifactClose"));
	CloseButton->OnClicked.AddDynamic(this, &UImmortalArtifactWidget::HandleCloseClicked);
	CloseButton->SetStyle(MakeArtifactButtonStyle(FVector2D(64.0f), FLinearColor(0.42f, 0.22f, 0.5f, 1.0f)));
	SetArtifactLayout(Canvas->AddChildToCanvas(CloseButton), FVector2D(816.0f, 10.0f), FVector2D(64.0f));
	AddButtonLabel(WidgetTree, CloseButton, TEXT("×"));

	UTextBlock* CatalogTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ArtifactCatalogTitle"));
	CatalogTitle->SetText(FText::FromString(TEXT("炼制图谱")));
	StyleArtifactText(CatalogTitle, 20, FLinearColor::White);
	SetArtifactLayout(Canvas->AddChildToCanvas(CatalogTitle), FVector2D(24.0f, 72.0f), FVector2D(245.0f, 30.0f));
	UScrollBox* DefinitionScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ArtifactDefinitionScroll"));
	SetArtifactLayout(Canvas->AddChildToCanvas(DefinitionScroll), FVector2D(20.0f, 106.0f), FVector2D(250.0f, 425.0f));
	DefinitionList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ArtifactDefinitionList"));
	DefinitionScroll->AddChild(DefinitionList);

	UTextBlock* InventoryTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ArtifactInventoryTitle"));
	InventoryTitle->SetText(FText::FromString(TEXT("法宝储物戒")));
	StyleArtifactText(InventoryTitle, 20, FLinearColor::White);
	SetArtifactLayout(Canvas->AddChildToCanvas(InventoryTitle), FVector2D(284.0f, 72.0f), FVector2D(245.0f, 30.0f));
	UScrollBox* ArtifactScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ArtifactInventoryScroll"));
	SetArtifactLayout(Canvas->AddChildToCanvas(ArtifactScroll), FVector2D(280.0f, 106.0f), FVector2D(250.0f, 425.0f));
	ArtifactList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ArtifactInventoryList"));
	ArtifactScroll->AddChild(ArtifactList);

	DetailNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ArtifactDetailName"));
	StyleArtifactText(DetailNameText, 23, FLinearColor(0.86f, 0.66f, 1.0f));
	SetArtifactLayout(Canvas->AddChildToCanvas(DetailNameText), FVector2D(550.0f, 76.0f), FVector2D(320.0f, 34.0f));
	DetailMetaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ArtifactDetailMeta"));
	StyleArtifactText(DetailMetaText, 15, FLinearColor(0.7f, 0.9f, 1.0f));
	SetArtifactLayout(Canvas->AddChildToCanvas(DetailMetaText), FVector2D(550.0f, 112.0f), FVector2D(320.0f, 28.0f));
	DescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ArtifactDescription"));
	DescriptionText->SetAutoWrapText(true);
	StyleArtifactText(DescriptionText, 14, FLinearColor(0.86f, 0.87f, 0.92f));
	SetArtifactLayout(Canvas->AddChildToCanvas(DescriptionText), FVector2D(550.0f, 145.0f), FVector2D(320.0f, 65.0f));
	ActiveText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ArtifactActiveEffect"));
	ActiveText->SetAutoWrapText(true);
	StyleArtifactText(ActiveText, 14, FLinearColor(1.0f, 0.74f, 0.28f));
	SetArtifactLayout(Canvas->AddChildToCanvas(ActiveText), FVector2D(550.0f, 215.0f), FVector2D(320.0f, 72.0f));
	PassiveText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ArtifactPassiveEffect"));
	PassiveText->SetAutoWrapText(true);
	StyleArtifactText(PassiveText, 14, FLinearColor(0.54f, 1.0f, 0.7f));
	SetArtifactLayout(Canvas->AddChildToCanvas(PassiveText), FVector2D(550.0f, 292.0f), FVector2D(320.0f, 70.0f));
	CostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ArtifactCost"));
	CostText->SetAutoWrapText(true);
	StyleArtifactText(CostText, 13, FLinearColor(0.96f, 0.88f, 0.62f));
	SetArtifactLayout(Canvas->AddChildToCanvas(CostText), FVector2D(550.0f, 368.0f), FVector2D(320.0f, 90.0f));

	CraftButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CraftArtifactButton"));
	CraftButton->OnClicked.AddDynamic(this, &UImmortalArtifactWidget::HandleCraftClicked);
	CraftButton->SetStyle(MakeArtifactButtonStyle(FVector2D(150.0f, 46.0f), FLinearColor(0.48f, 0.25f, 0.68f)));
	SetArtifactLayout(Canvas->AddChildToCanvas(CraftButton), FVector2D(635.0f, 466.0f), FVector2D(150.0f, 46.0f));
	AddButtonLabel(WidgetTree, CraftButton, TEXT("炼制法宝"));
	EquipButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EquipArtifactButton"));
	EquipButton->OnClicked.AddDynamic(this, &UImmortalArtifactWidget::HandleEquipClicked);
	EquipButton->SetStyle(MakeArtifactButtonStyle(FVector2D(96.0f, 46.0f), FLinearColor(0.32f, 0.55f, 0.8f)));
	SetArtifactLayout(Canvas->AddChildToCanvas(EquipButton), FVector2D(550.0f, 466.0f), FVector2D(96.0f, 46.0f));
	AddButtonLabel(WidgetTree, EquipButton, TEXT("装备"));
	UpgradeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("UpgradeArtifactButton"));
	UpgradeButton->OnClicked.AddDynamic(this, &UImmortalArtifactWidget::HandleUpgradeClicked);
	UpgradeButton->SetStyle(MakeArtifactButtonStyle(FVector2D(96.0f, 46.0f), FLinearColor(0.3f, 0.68f, 0.48f)));
	SetArtifactLayout(Canvas->AddChildToCanvas(UpgradeButton), FVector2D(660.0f, 466.0f), FVector2D(96.0f, 46.0f));
	AddButtonLabel(WidgetTree, UpgradeButton, TEXT("蕴养"));
	StarButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StarArtifactButton"));
	StarButton->OnClicked.AddDynamic(this, &UImmortalArtifactWidget::HandleStarClicked);
	StarButton->SetStyle(MakeArtifactButtonStyle(FVector2D(96.0f, 46.0f), FLinearColor(0.72f, 0.48f, 0.18f)));
	SetArtifactLayout(Canvas->AddChildToCanvas(StarButton), FVector2D(770.0f, 466.0f), FVector2D(96.0f, 46.0f));
	AddButtonLabel(WidgetTree, StarButton, TEXT("升星"));

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ArtifactResult"));
	ResultText->SetAutoWrapText(true);
	ResultText->SetJustification(ETextJustify::Center);
	StyleArtifactText(ResultText, 15, FLinearColor(1.0f, 0.74f, 0.32f));
	SetArtifactLayout(Canvas->AddChildToCanvas(ResultText), FVector2D(540.0f, 522.0f), FVector2D(335.0f, 58.0f));
	RefreshFromPlayer();
}

void UImmortalArtifactWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Player.IsValid()
		&& (LastArtifactRevision != Player->GetArtifactInventoryRevision()
			|| LastMaterialRevision != Player->GetMaterialInventoryRevision()
			|| LastSpiritStones != Player->GetGold()
			|| LastStage != Player->GetQingyunStage()))
	{
		RefreshFromPlayer();
	}
}

void UImmortalArtifactWidget::RefreshFromPlayer()
{
	if (!Player.IsValid() || !DefinitionList || !ArtifactList) return;
	LastArtifactRevision = Player->GetArtifactInventoryRevision();
	LastMaterialRevision = Player->GetMaterialInventoryRevision();
	LastSpiritStones = Player->GetGold();
	LastStage = Player->GetQingyunStage();
	CurrencyText->SetText(FText::FromString(FString::Printf(TEXT("灵石 %d · 青云山第 %d 关 · 法宝 %d"),
		LastSpiritStones, LastStage, Player->GetArtifactInventory().Num())));
	const TArray<FName> Definitions = UImmortalArtifactLibrary::GetKnownArtifactIds();
	if (SelectedDefinitionId.IsNone() || !Definitions.Contains(SelectedDefinitionId))
	{
		SelectedDefinitionId = Definitions.IsEmpty() ? NAME_None : Definitions[0];
	}
	const TArray<FImmortalArtifactItem> Items = Player->GetArtifactInventory();
	if (!Items.ContainsByPredicate([this](const FImmortalArtifactItem& Item) { return Item.InstanceId == SelectedInstanceId; }))
	{
		FImmortalArtifactItem Equipped;
		SelectedInstanceId = Player->GetEquippedArtifact(Equipped)
			? Equipped.InstanceId
			: (Items.IsEmpty() ? FGuid() : Items[0].InstanceId);
	}
	if (!bViewingDefinition && !SelectedInstanceId.IsValid()) bViewingDefinition = true;
	RebuildDefinitionEntries();
	RebuildArtifactEntries();
	RefreshDetails();
}

void UImmortalArtifactWidget::RebuildDefinitionEntries()
{
	DefinitionList->ClearChildren();
	for (const FName Id : UImmortalArtifactLibrary::GetKnownArtifactIds())
	{
		UImmortalArtifactEntryWidget* Entry = CreateWidget<UImmortalArtifactEntryWidget>(this, UImmortalArtifactEntryWidget::StaticClass());
		if (!Entry) continue;
		Entry->InitializeDefinitionEntry(this, Id, Player->IsArtifactUnlocked(Id), bViewingDefinition && Id == SelectedDefinitionId);
		DefinitionList->AddChildToVerticalBox(Entry);
	}
}

void UImmortalArtifactWidget::RebuildArtifactEntries()
{
	ArtifactList->ClearChildren();
	FImmortalArtifactItem Equipped;
	const bool bHasEquipped = Player->GetEquippedArtifact(Equipped);
	for (const FImmortalArtifactItem& Item : Player->GetArtifactInventory())
	{
		UImmortalArtifactEntryWidget* Entry = CreateWidget<UImmortalArtifactEntryWidget>(this, UImmortalArtifactEntryWidget::StaticClass());
		if (!Entry) continue;
		Entry->InitializeArtifactEntry(
			this, Item, bHasEquipped && Item.InstanceId == Equipped.InstanceId,
			!bViewingDefinition && Item.InstanceId == SelectedInstanceId);
		ArtifactList->AddChildToVerticalBox(Entry);
	}
	if (Player->GetArtifactInventory().IsEmpty())
	{
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Empty->SetText(FText::FromString(TEXT("尚无法宝\n请从左侧图谱炼制")));
		Empty->SetJustification(ETextJustify::Center);
		StyleArtifactText(Empty, 15, FLinearColor(0.62f, 0.64f, 0.7f));
		ArtifactList->AddChildToVerticalBox(Empty);
	}
}

void UImmortalArtifactWidget::RefreshDetails()
{
	if (!Player.IsValid()) return;
	if (bViewingDefinition)
	{
		FImmortalArtifactDefinition Definition;
		if (!UImmortalArtifactLibrary::GetArtifactDefinition(SelectedDefinitionId, Definition)) return;
		const FImmortalArtifactItem Preview = UImmortalArtifactLibrary::CreateArtifact(SelectedDefinitionId);
		DetailNameText->SetText(FText::FromString(FString::Printf(TEXT("%s  %s"),
			*Definition.IconGlyph.ToString(), *Definition.DisplayName.ToString())));
		DetailNameText->SetColorAndOpacity(FSlateColor(UImmortalArtifactLibrary::GetQualityColor(Definition.Quality)));
		DetailMetaText->SetText(FText::FromString(FString::Printf(TEXT("%s · 第 %d 关解锁 · 初始 1 级 0 星"),
			*UImmortalArtifactLibrary::GetQualityText(Definition.Quality).ToString(), Definition.MinimumQingyunStage)));
		DescriptionText->SetText(Definition.Description);
		ActiveText->SetText(FText::FromString(TEXT("主动：") + UImmortalArtifactLibrary::GetActiveEffectText(Preview).ToString()));
		PassiveText->SetText(FText::FromString(TEXT("被动：") + UImmortalArtifactLibrary::GetPassiveEffectText(Preview).ToString()));
		CostText->SetText(FText::FromString(TEXT("炼制消耗\n")
			+ UImmortalCraftingLibrary::FormatCost(Definition.CraftingCost, Player->GetMaterialInventory(), Player->GetGold()).ToString()));
		CraftButton->SetVisibility(ESlateVisibility::Visible);
		CraftButton->SetIsEnabled(Player->CanCraftArtifact(SelectedDefinitionId));
		EquipButton->SetVisibility(ESlateVisibility::Collapsed);
		UpgradeButton->SetVisibility(ESlateVisibility::Collapsed);
		StarButton->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	FImmortalArtifactItem Selected;
	for (const FImmortalArtifactItem& Item : Player->GetArtifactInventory())
	{
		if (Item.InstanceId == SelectedInstanceId) { Selected = Item; break; }
	}
	FImmortalArtifactDefinition Definition;
	if (!Selected.IsValid() || !UImmortalArtifactLibrary::GetArtifactDefinition(Selected.ArtifactId, Definition)) return;
	FImmortalArtifactItem Equipped;
	const bool bEquipped = Player->GetEquippedArtifact(Equipped) && Equipped.InstanceId == Selected.InstanceId;
	DetailNameText->SetText(FText::FromString(FString::Printf(TEXT("%s  %s"),
		*Definition.IconGlyph.ToString(), *Definition.DisplayName.ToString())));
	DetailNameText->SetColorAndOpacity(FSlateColor(UImmortalArtifactLibrary::GetQualityColor(Definition.Quality)));
	DetailMetaText->SetText(FText::FromString(FString::Printf(TEXT("%s · %d 级 · %d 星%s"),
		*UImmortalArtifactLibrary::GetQualityText(Definition.Quality).ToString(), Selected.Level, Selected.Stars,
		bEquipped ? TEXT(" · 已装备") : TEXT(""))));
	DescriptionText->SetText(Definition.Description);
	ActiveText->SetText(FText::FromString(TEXT("主动：") + UImmortalArtifactLibrary::GetActiveEffectText(Selected).ToString()));
	PassiveText->SetText(FText::FromString(TEXT("被动：") + UImmortalArtifactLibrary::GetPassiveEffectText(Selected).ToString()));
	const FText UpgradeCost = Selected.Level >= 50
		? FText::FromString(TEXT("已满级"))
		: UImmortalCraftingLibrary::FormatCost(UImmortalArtifactLibrary::GetUpgradeCost(Selected), Player->GetMaterialInventory(), Player->GetGold());
	const FText StarCost = Selected.Stars >= 5
		? FText::FromString(TEXT("已满星"))
		: UImmortalCraftingLibrary::FormatCost(UImmortalArtifactLibrary::GetStarUpCost(Selected), Player->GetMaterialInventory(), Player->GetGold());
	CostText->SetText(FText::FromString(FString::Printf(TEXT("蕴养：%s\n升星：%s"), *UpgradeCost.ToString(), *StarCost.ToString())));
	CraftButton->SetVisibility(ESlateVisibility::Collapsed);
	EquipButton->SetVisibility(ESlateVisibility::Visible);
	EquipButton->SetIsEnabled(!bEquipped);
	UpgradeButton->SetVisibility(ESlateVisibility::Visible);
	UpgradeButton->SetIsEnabled(Player->CanUpgradeArtifact(Selected.InstanceId));
	StarButton->SetVisibility(ESlateVisibility::Visible);
	StarButton->SetIsEnabled(Player->CanStarUpArtifact(Selected.InstanceId));
}

void UImmortalArtifactWidget::SelectDefinition(const FName ArtifactId)
{
	SelectedDefinitionId = ArtifactId;
	bViewingDefinition = true;
	if (ResultText) ResultText->SetText(FText::GetEmpty());
	RefreshFromPlayer();
}

void UImmortalArtifactWidget::SelectArtifact(const FGuid InstanceId)
{
	SelectedInstanceId = InstanceId;
	bViewingDefinition = false;
	if (ResultText) ResultText->SetText(FText::GetEmpty());
	RefreshFromPlayer();
}

void UImmortalArtifactWidget::HandleCraftClicked()
{
	if (!Player.IsValid()) return;
	const FImmortalArtifactOperationResult Result = Player->CraftArtifact(SelectedDefinitionId);
	ResultText->SetText(Result.Message);
	if (Result.bSucceeded)
	{
		SelectedInstanceId = Result.InstanceId;
		bViewingDefinition = false;
	}
	RefreshFromPlayer();
}

void UImmortalArtifactWidget::HandleEquipClicked()
{
	if (!Player.IsValid()) return;
	const FImmortalArtifactOperationResult Result = Player->EquipArtifact(SelectedInstanceId);
	ResultText->SetText(Result.Message);
	RefreshFromPlayer();
}

void UImmortalArtifactWidget::HandleUpgradeClicked()
{
	if (!Player.IsValid()) return;
	const FImmortalArtifactOperationResult Result = Player->UpgradeArtifact(SelectedInstanceId);
	ResultText->SetText(Result.Message);
	RefreshFromPlayer();
}

void UImmortalArtifactWidget::HandleStarClicked()
{
	if (!Player.IsValid()) return;
	const FImmortalArtifactOperationResult Result = Player->StarUpArtifact(SelectedInstanceId);
	ResultText->SetText(Result.Message);
	RefreshFromPlayer();
}

void UImmortalArtifactWidget::HandleCloseClicked()
{
	if (Player.IsValid()) Player->ToggleArtifacts();
}

