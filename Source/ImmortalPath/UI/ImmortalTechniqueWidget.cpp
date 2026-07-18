// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalTechniqueWidget.h"

#include "ImmortalTechniqueEntryWidget.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Crafting/ImmortalCraftingTypes.h"
#include "../Techniques/ImmortalTechniqueTypes.h"
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
	FSlateBrush MakeTechniqueBrush(const TCHAR* AssetPath, const FVector2D Size, const FLinearColor Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(Tint);
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, AssetPath)) Brush.SetResourceObject(Texture);
		return Brush;
	}

	void SetTechniqueLayout(UCanvasPanelSlot* Slot, const FVector2D Position, const FVector2D Size)
	{
		if (!Slot) return;
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetAutoSize(false);
	}

	void StyleTechniqueText(UTextBlock* Text, const int32 Size, const FLinearColor& Color)
	{
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor::Black);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}

	FButtonStyle MakeTechniqueButtonStyle(const FVector2D Size, const FLinearColor& Tint)
	{
		const FSlateBrush Brush = MakeTechniqueBrush(
			TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal"), Size, Tint);
		FButtonStyle Style;
		Style.SetNormal(Brush);
		Style.SetHovered(Brush);
		Style.SetPressed(Brush);
		Style.SetDisabled(Brush);
		return Style;
	}

	UTextBlock* AddTechniqueButtonLabel(UWidgetTree* Tree, UButton* Button, const TCHAR* Label, const int32 FontSize = 14)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		Text->SetJustification(ETextJustify::Center);
		StyleTechniqueText(Text, FontSize, FLinearColor::White);
		Button->AddChild(Text);
		return Text;
	}

	UButton* AddTechniqueButton(
		UWidgetTree* Tree,
		UCanvasPanel* Canvas,
		const FName Name,
		const TCHAR* Label,
		const FVector2D Position,
		const FVector2D Size,
		const FLinearColor& Tint)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetStyle(MakeTechniqueButtonStyle(Size, Tint));
		SetTechniqueLayout(Canvas->AddChildToCanvas(Button), Position, Size);
		AddTechniqueButtonLabel(Tree, Button, Label);
		return Button;
	}
}

void UImmortalTechniqueWidget::InitializeForPlayer(AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalTechniqueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TechniquePanelSize"));
	Root->SetWidthOverride(900.0f);
	Root->SetHeightOverride(600.0f);
	WidgetTree->RootWidget = Root;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TechniqueCanvas"));
	Root->AddChild(Canvas);

	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TechniqueBackground"));
	Background->SetBrush(MakeTechniqueBrush(
		TEXT("/Game/GAME/Asset/ui/inventory/panel_background.panel_background"), FVector2D(900.0f, 600.0f),
		FLinearColor(0.68f, 0.82f, 0.92f, 0.99f)));
	SetTechniqueLayout(Canvas->AddChildToCanvas(Background), FVector2D::ZeroVector, FVector2D(900.0f, 600.0f));

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TechniqueTitle"));
	Title->SetText(FText::FromString(TEXT("藏经阁 · 功法参悟")));
	StyleTechniqueText(Title, 28, FLinearColor(0.55f, 0.92f, 1.0f));
	SetTechniqueLayout(Canvas->AddChildToCanvas(Title), FVector2D(28.0f, 14.0f), FVector2D(320.0f, 42.0f));

	ResourceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TechniqueResources"));
	ResourceText->SetJustification(ETextJustify::Right);
	StyleTechniqueText(ResourceText, 15, FLinearColor(0.74f, 0.96f, 1.0f));
	SetTechniqueLayout(Canvas->AddChildToCanvas(ResourceText), FVector2D(360.0f, 20.0f), FVector2D(440.0f, 34.0f));

	UButton* CloseButton = AddTechniqueButton(WidgetTree, Canvas, TEXT("TechniqueClose"), TEXT("×"),
		FVector2D(816.0f, 10.0f), FVector2D(64.0f), FLinearColor(0.18f, 0.38f, 0.5f, 1.0f));
	CloseButton->OnClicked.AddDynamic(this, &UImmortalTechniqueWidget::HandleCloseClicked);

	UTextBlock* CatalogTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TechniqueCatalogTitle"));
	CatalogTitle->SetText(FText::FromString(TEXT("功法目录")));
	StyleTechniqueText(CatalogTitle, 20, FLinearColor::White);
	SetTechniqueLayout(Canvas->AddChildToCanvas(CatalogTitle), FVector2D(24.0f, 70.0f), FVector2D(250.0f, 30.0f));
	UScrollBox* TechniqueScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("TechniqueScroll"));
	SetTechniqueLayout(Canvas->AddChildToCanvas(TechniqueScroll), FVector2D(20.0f, 104.0f), FVector2D(260.0f, 402.0f));
	TechniqueList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TechniqueList"));
	TechniqueScroll->AddChild(TechniqueList);

	SlotText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TechniqueSlots"));
	SlotText->SetAutoWrapText(true);
	StyleTechniqueText(SlotText, 15, FLinearColor(0.48f, 1.0f, 0.78f));
	SetTechniqueLayout(Canvas->AddChildToCanvas(SlotText), FVector2D(24.0f, 516.0f), FVector2D(260.0f, 66.0f));

	DetailNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TechniqueDetailName"));
	StyleTechniqueText(DetailNameText, 24, FLinearColor(0.65f, 0.92f, 1.0f));
	SetTechniqueLayout(Canvas->AddChildToCanvas(DetailNameText), FVector2D(305.0f, 72.0f), FVector2D(560.0f, 36.0f));
	DetailMetaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TechniqueDetailMeta"));
	StyleTechniqueText(DetailMetaText, 15, FLinearColor(0.72f, 0.86f, 0.96f));
	SetTechniqueLayout(Canvas->AddChildToCanvas(DetailMetaText), FVector2D(305.0f, 108.0f), FVector2D(560.0f, 28.0f));
	DescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TechniqueDescription"));
	DescriptionText->SetAutoWrapText(true);
	StyleTechniqueText(DescriptionText, 14, FLinearColor(0.88f, 0.9f, 0.94f));
	SetTechniqueLayout(Canvas->AddChildToCanvas(DescriptionText), FVector2D(305.0f, 138.0f), FVector2D(560.0f, 48.0f));
	ActiveText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TechniqueActive"));
	ActiveText->SetAutoWrapText(true);
	StyleTechniqueText(ActiveText, 14, FLinearColor(0.44f, 0.9f, 1.0f));
	SetTechniqueLayout(Canvas->AddChildToCanvas(ActiveText), FVector2D(305.0f, 190.0f), FVector2D(560.0f, 45.0f));
	UltimateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TechniqueUltimate"));
	UltimateText->SetAutoWrapText(true);
	StyleTechniqueText(UltimateText, 14, FLinearColor(1.0f, 0.66f, 0.25f));
	SetTechniqueLayout(Canvas->AddChildToCanvas(UltimateText), FVector2D(305.0f, 236.0f), FVector2D(560.0f, 45.0f));
	PassiveText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TechniquePassive"));
	PassiveText->SetAutoWrapText(true);
	StyleTechniqueText(PassiveText, 14, FLinearColor(0.5f, 1.0f, 0.68f));
	SetTechniqueLayout(Canvas->AddChildToCanvas(PassiveText), FVector2D(305.0f, 282.0f), FVector2D(560.0f, 45.0f));
	SpecialText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TechniqueSpecial"));
	SpecialText->SetAutoWrapText(true);
	StyleTechniqueText(SpecialText, 14, FLinearColor(0.95f, 0.58f, 1.0f));
	SetTechniqueLayout(Canvas->AddChildToCanvas(SpecialText), FVector2D(305.0f, 328.0f), FVector2D(560.0f, 45.0f));
	CostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TechniqueCost"));
	CostText->SetAutoWrapText(true);
	StyleTechniqueText(CostText, 13, FLinearColor(0.96f, 0.88f, 0.62f));
	SetTechniqueLayout(Canvas->AddChildToCanvas(CostText), FVector2D(305.0f, 374.0f), FVector2D(560.0f, 55.0f));

	LearnButton = AddTechniqueButton(WidgetTree, Canvas, TEXT("TechniqueLearn"), TEXT("习得功法"),
		FVector2D(305.0f, 438.0f), FVector2D(130.0f, 44.0f), FLinearColor(0.16f, 0.52f, 0.64f));
	LearnButton->OnClicked.AddDynamic(this, &UImmortalTechniqueWidget::HandleLearnClicked);
	EquipSlotOneButton = AddTechniqueButton(WidgetTree, Canvas, TEXT("TechniqueSlotOne"), TEXT("装入槽位一"),
		FVector2D(305.0f, 438.0f), FVector2D(118.0f, 44.0f), FLinearColor(0.18f, 0.48f, 0.56f));
	EquipSlotOneButton->OnClicked.AddDynamic(this, &UImmortalTechniqueWidget::HandleEquipSlotOneClicked);
	EquipSlotTwoButton = AddTechniqueButton(WidgetTree, Canvas, TEXT("TechniqueSlotTwo"), TEXT("装入槽位二"),
		FVector2D(429.0f, 438.0f), FVector2D(118.0f, 44.0f), FLinearColor(0.18f, 0.48f, 0.56f));
	EquipSlotTwoButton->OnClicked.AddDynamic(this, &UImmortalTechniqueWidget::HandleEquipSlotTwoClicked);
	UpgradeButton = AddTechniqueButton(WidgetTree, Canvas, TEXT("TechniqueUpgrade"), TEXT("提升等级"),
		FVector2D(553.0f, 438.0f), FVector2D(112.0f, 44.0f), FLinearColor(0.26f, 0.62f, 0.42f));
	UpgradeButton->OnClicked.AddDynamic(this, &UImmortalTechniqueWidget::HandleUpgradeClicked);
	BreakthroughButton = AddTechniqueButton(WidgetTree, Canvas, TEXT("TechniqueBreakthrough"), TEXT("功法突破"),
		FVector2D(671.0f, 438.0f), FVector2D(112.0f, 44.0f), FLinearColor(0.72f, 0.42f, 0.13f));
	BreakthroughButton->OnClicked.AddDynamic(this, &UImmortalTechniqueWidget::HandleBreakthroughClicked);

	ActivePointButton = AddTechniqueButton(WidgetTree, Canvas, TEXT("TechniqueActivePoint"), TEXT("主动 +1"),
		FVector2D(305.0f, 490.0f), FVector2D(112.0f, 42.0f), FLinearColor(0.12f, 0.48f, 0.7f));
	ActivePointButton->OnClicked.AddDynamic(this, &UImmortalTechniqueWidget::HandleActivePointClicked);
	PassivePointButton = AddTechniqueButton(WidgetTree, Canvas, TEXT("TechniquePassivePoint"), TEXT("被动 +1"),
		FVector2D(423.0f, 490.0f), FVector2D(112.0f, 42.0f), FLinearColor(0.18f, 0.58f, 0.36f));
	PassivePointButton->OnClicked.AddDynamic(this, &UImmortalTechniqueWidget::HandlePassivePointClicked);
	SpecialPointButton = AddTechniqueButton(WidgetTree, Canvas, TEXT("TechniqueSpecialPoint"), TEXT("特殊 +1"),
		FVector2D(541.0f, 490.0f), FVector2D(112.0f, 42.0f), FLinearColor(0.52f, 0.26f, 0.65f));
	SpecialPointButton->OnClicked.AddDynamic(this, &UImmortalTechniqueWidget::HandleSpecialPointClicked);

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TechniqueResult"));
	ResultText->SetAutoWrapText(true);
	ResultText->SetJustification(ETextJustify::Center);
	StyleTechniqueText(ResultText, 14, FLinearColor(1.0f, 0.75f, 0.3f));
	SetTechniqueLayout(Canvas->AddChildToCanvas(ResultText), FVector2D(670.0f, 490.0f), FVector2D(200.0f, 80.0f));
	RefreshFromPlayer();
}

void UImmortalTechniqueWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Player.IsValid()
		&& (LastTechniqueRevision != Player->GetTechniqueRevision()
			|| LastMaterialRevision != Player->GetMaterialInventoryRevision()
			|| LastSpiritStones != Player->GetGold()
			|| LastCultivation != Player->GetCultivation()
			|| LastStage != Player->GetQingyunStage()))
	{
		RefreshFromPlayer();
	}
}

void UImmortalTechniqueWidget::RefreshFromPlayer()
{
	if (!Player.IsValid() || !TechniqueList) return;
	LastTechniqueRevision = Player->GetTechniqueRevision();
	LastMaterialRevision = Player->GetMaterialInventoryRevision();
	LastSpiritStones = Player->GetGold();
	LastCultivation = Player->GetCultivation();
	LastStage = Player->GetQingyunStage();
	ResourceText->SetText(FText::FromString(FString::Printf(TEXT("修为 %d · 灵石 %d · 参悟点 %d · %.1f修为/秒"),
		LastCultivation, LastSpiritStones, Player->GetTechniqueInsightPoints(), Player->GetCultivationPerSecond())));
	const TArray<FName> KnownIds = UImmortalTechniqueLibrary::GetKnownTechniqueIds();
	if (SelectedTechniqueId.IsNone() || !KnownIds.Contains(SelectedTechniqueId))
	{
		SelectedTechniqueId = KnownIds.IsEmpty() ? NAME_None : KnownIds[0];
	}
	RebuildTechniqueEntries();
	const TArray<FName> Slots = Player->GetEquippedTechniqueIds();
	FString SlotNames[2] = { TEXT("未装备"), TEXT("未装备") };
	for (int32 Index = 0; Index < FMath::Min(Slots.Num(), 2); ++Index)
	{
		FImmortalTechniqueDefinition Definition;
		if (UImmortalTechniqueLibrary::GetTechniqueDefinition(Slots[Index], Definition)) SlotNames[Index] = Definition.DisplayName.ToString();
	}
	SlotText->SetText(FText::FromString(FString::Printf(TEXT("运转槽一：%s\n运转槽二：%s"), *SlotNames[0], *SlotNames[1])));
	RefreshDetails();
}

void UImmortalTechniqueWidget::RebuildTechniqueEntries()
{
	TechniqueList->ClearChildren();
	for (const FName Id : UImmortalTechniqueLibrary::GetKnownTechniqueIds())
	{
		FImmortalTechniqueProgress Progress;
		const bool bLearned = Player->GetTechniqueProgress(Id, Progress);
		int32 EquippedSlot = INDEX_NONE;
		Player->IsTechniqueEquipped(Id, EquippedSlot);
		UImmortalTechniqueEntryWidget* Entry = CreateWidget<UImmortalTechniqueEntryWidget>(this, UImmortalTechniqueEntryWidget::StaticClass());
		if (!Entry) continue;
		Entry->InitializeEntry(this, Id, bLearned, Progress.Level, Progress.BreakthroughRank,
			EquippedSlot, Player->IsTechniqueUnlocked(Id), Id == SelectedTechniqueId);
		TechniqueList->AddChildToVerticalBox(Entry);
	}
}

void UImmortalTechniqueWidget::RefreshDetails()
{
	if (!Player.IsValid()) return;
	FImmortalTechniqueDefinition Definition;
	if (!UImmortalTechniqueLibrary::GetTechniqueDefinition(SelectedTechniqueId, Definition)) return;
	FImmortalTechniqueProgress Progress;
	const bool bLearned = Player->GetTechniqueProgress(SelectedTechniqueId, Progress);
	if (!bLearned) Progress = UImmortalTechniqueLibrary::CreateTechnique(SelectedTechniqueId);
	int32 EquippedSlot = INDEX_NONE;
	const bool bEquipped = Player->IsTechniqueEquipped(SelectedTechniqueId, EquippedSlot);
	DetailNameText->SetText(FText::FromString(FString::Printf(TEXT("%s  %s"),
		*Definition.IconGlyph.ToString(), *Definition.DisplayName.ToString())));
	DetailNameText->SetColorAndOpacity(FSlateColor(UImmortalTechniqueLibrary::GetQualityColor(Definition.Quality)));
	DetailMetaText->SetText(FText::FromString(bLearned
		? FString::Printf(TEXT("%s · %s元素 · %d级/%d · 突破%d%s · 主%d 被%d 特%d"),
			*UImmortalTechniqueLibrary::GetQualityText(Definition.Quality).ToString(),
			*UImmortalCharacterPathLibrary::GetElementText(Definition.Element).ToString(), Progress.Level,
			UImmortalTechniqueLibrary::GetLevelCap(Progress), Progress.BreakthroughRank,
			bEquipped ? *FString::Printf(TEXT(" · 运转槽%d"), EquippedSlot + 1) : TEXT(""),
			Progress.ActivePoints, Progress.PassivePoints, Progress.SpecialPoints)
		: FString::Printf(TEXT("%s · %s元素 · 第%d关 / 境界%d解锁 · 尚未习得"),
			*UImmortalTechniqueLibrary::GetQualityText(Definition.Quality).ToString(),
			*UImmortalCharacterPathLibrary::GetElementText(Definition.Element).ToString(),
			Definition.MinimumQingyunStage, Definition.MinimumRealmIndex + 1)));
	DescriptionText->SetText(Definition.Description);
	ActiveText->SetText(FText::FromString(TEXT("主动：") + UImmortalTechniqueLibrary::GetActiveEffectText(Progress).ToString()));
	UltimateText->SetText(FText::FromString(TEXT("绝技：") + UImmortalTechniqueLibrary::GetUltimateEffectText(Progress).ToString()));
	PassiveText->SetText(FText::FromString(TEXT("被动：") + UImmortalTechniqueLibrary::GetPassiveEffectText(Progress).ToString()));
	SpecialText->SetText(FText::FromString(TEXT("特殊：") + UImmortalTechniqueLibrary::GetSpecialEffectText(Progress).ToString()));

	if (!bLearned)
	{
		CostText->SetText(FText::FromString(TEXT("习得消耗：")
			+ UImmortalCraftingLibrary::FormatCost(Definition.LearningCost, Player->GetMaterialInventory(), Player->GetGold()).ToString()));
	}
	else
	{
		const int32 UpgradeCost = UImmortalTechniqueLibrary::GetUpgradeCultivationCost(Progress);
		const FText BreakthroughCost = Progress.BreakthroughRank >= 4
			? FText::FromString(TEXT("已达圆满"))
			: UImmortalCraftingLibrary::FormatCost(
				UImmortalTechniqueLibrary::GetBreakthroughCost(Progress), Player->GetMaterialInventory(), Player->GetGold());
		CostText->SetText(FText::FromString(FString::Printf(TEXT("升级消耗：%d修为（当前小境界内） · 突破消耗：%s"),
			UpgradeCost, *BreakthroughCost.ToString())));
	}

	LearnButton->SetVisibility(bLearned ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	LearnButton->SetIsEnabled(Player->IsTechniqueUnlocked(SelectedTechniqueId));
	const ESlateVisibility LearnedVisibility = bLearned ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	EquipSlotOneButton->SetVisibility(LearnedVisibility);
	EquipSlotTwoButton->SetVisibility(LearnedVisibility);
	UpgradeButton->SetVisibility(LearnedVisibility);
	BreakthroughButton->SetVisibility(LearnedVisibility);
	ActivePointButton->SetVisibility(LearnedVisibility);
	PassivePointButton->SetVisibility(LearnedVisibility);
	SpecialPointButton->SetVisibility(LearnedVisibility);
	if (bLearned)
	{
		EquipSlotOneButton->SetIsEnabled(!bEquipped || EquippedSlot != 0);
		EquipSlotTwoButton->SetIsEnabled(!bEquipped || EquippedSlot != 1);
		UpgradeButton->SetIsEnabled(Player->CanUpgradeTechnique(SelectedTechniqueId));
		BreakthroughButton->SetIsEnabled(Player->CanBreakthroughTechnique(SelectedTechniqueId));
		ActivePointButton->SetIsEnabled(Player->CanAllocateTechniquePoint(SelectedTechniqueId, EImmortalTechniquePointBranch::Active));
		PassivePointButton->SetIsEnabled(Player->CanAllocateTechniquePoint(SelectedTechniqueId, EImmortalTechniquePointBranch::Passive));
		SpecialPointButton->SetIsEnabled(Player->CanAllocateTechniquePoint(SelectedTechniqueId, EImmortalTechniquePointBranch::Special));
	}
}

void UImmortalTechniqueWidget::SelectTechnique(const FName TechniqueId)
{
	SelectedTechniqueId = TechniqueId;
	if (ResultText) ResultText->SetText(FText::GetEmpty());
	RefreshFromPlayer();
}

void UImmortalTechniqueWidget::SetResult(const FText& Message)
{
	if (ResultText) ResultText->SetText(Message);
	RefreshFromPlayer();
}

void UImmortalTechniqueWidget::HandleLearnClicked()
{
	if (Player.IsValid()) SetResult(Player->LearnTechnique(SelectedTechniqueId).Message);
}

void UImmortalTechniqueWidget::HandleEquipSlotOneClicked()
{
	if (Player.IsValid()) SetResult(Player->EquipTechnique(SelectedTechniqueId, 0).Message);
}

void UImmortalTechniqueWidget::HandleEquipSlotTwoClicked()
{
	if (Player.IsValid()) SetResult(Player->EquipTechnique(SelectedTechniqueId, 1).Message);
}

void UImmortalTechniqueWidget::HandleUpgradeClicked()
{
	if (Player.IsValid()) SetResult(Player->UpgradeTechnique(SelectedTechniqueId).Message);
}

void UImmortalTechniqueWidget::HandleBreakthroughClicked()
{
	if (Player.IsValid()) SetResult(Player->BreakthroughTechnique(SelectedTechniqueId).Message);
}

void UImmortalTechniqueWidget::HandleActivePointClicked()
{
	if (Player.IsValid()) SetResult(Player->AllocateTechniquePoint(SelectedTechniqueId, EImmortalTechniquePointBranch::Active).Message);
}

void UImmortalTechniqueWidget::HandlePassivePointClicked()
{
	if (Player.IsValid()) SetResult(Player->AllocateTechniquePoint(SelectedTechniqueId, EImmortalTechniquePointBranch::Passive).Message);
}

void UImmortalTechniqueWidget::HandleSpecialPointClicked()
{
	if (Player.IsValid()) SetResult(Player->AllocateTechniquePoint(SelectedTechniqueId, EImmortalTechniquePointBranch::Special).Message);
}

void UImmortalTechniqueWidget::HandleCloseClicked()
{
	if (Player.IsValid()) Player->ToggleTechniques();
}
