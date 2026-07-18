// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalCharacterBuildWidget.h"

#include "ImmortalCharacterPathEntryWidget.h"
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
	FSlateBrush BuildMakeBrush(const TCHAR* AssetPath, const FVector2D Size, const FLinearColor Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(Tint);
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, AssetPath)) Brush.SetResourceObject(Texture);
		return Brush;
	}

	void BuildSetLayout(UCanvasPanelSlot* Slot, const FVector2D Position, const FVector2D Size)
	{
		if (!Slot) return;
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetAutoSize(false);
	}

	void BuildStyleText(UTextBlock* Text, const int32 Size, const FLinearColor& Color)
	{
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor::Black);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}

	FButtonStyle BuildButtonStyle(const FVector2D Size, const FLinearColor& Tint)
	{
		const FSlateBrush Brush = BuildMakeBrush(TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal"), Size, Tint);
		FButtonStyle Style;
		Style.SetNormal(Brush);
		Style.SetHovered(Brush);
		Style.SetPressed(Brush);
		Style.SetDisabled(Brush);
		return Style;
	}

	UTextBlock* BuildAddButtonLabel(UWidgetTree* Tree, UButton* Button, const TCHAR* Label, const int32 Size = 15)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		Text->SetJustification(ETextJustify::Center);
		BuildStyleText(Text, Size, FLinearColor::White);
		Button->AddChild(Text);
		return Text;
	}
}

void UImmortalCharacterBuildWidget::InitializeForPlayer(AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalCharacterBuildWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CharacterBuildPanelSize"));
	Root->SetWidthOverride(900.0f);
	Root->SetHeightOverride(600.0f);
	WidgetTree->RootWidget = Root;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CharacterBuildCanvas"));
	Root->AddChild(Canvas);

	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CharacterBuildBackground"));
	Background->SetBrush(BuildMakeBrush(
		TEXT("/Game/GAME/Asset/ui/inventory/panel_background.panel_background"), FVector2D(900.0f, 600.0f),
		FLinearColor(0.72f, 0.86f, 0.78f, 0.99f)));
	BuildSetLayout(Canvas->AddChildToCanvas(Background), FVector2D::ZeroVector, FVector2D(900.0f, 600.0f));

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CharacterBuildTitle"));
	Title->SetText(FText::FromString(TEXT("问道台 · 灵根与流派")));
	BuildStyleText(Title, 28, FLinearColor(0.56f, 1.0f, 0.72f));
	BuildSetLayout(Canvas->AddChildToCanvas(Title), FVector2D(26.0f, 14.0f), FVector2D(350.0f, 42.0f));
	ResourceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CharacterBuildResources"));
	ResourceText->SetJustification(ETextJustify::Right);
	BuildStyleText(ResourceText, 15, FLinearColor(0.78f, 0.94f, 1.0f));
	BuildSetLayout(Canvas->AddChildToCanvas(ResourceText), FVector2D(390.0f, 20.0f), FVector2D(405.0f, 30.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CharacterBuildClose"));
	CloseButton->SetStyle(BuildButtonStyle(FVector2D(64.0f), FLinearColor(0.18f, 0.42f, 0.30f)));
	CloseButton->OnClicked.AddDynamic(this, &UImmortalCharacterBuildWidget::HandleCloseClicked);
	BuildSetLayout(Canvas->AddChildToCanvas(CloseButton), FVector2D(816.0f, 10.0f), FVector2D(64.0f));
	BuildAddButtonLabel(WidgetTree, CloseButton, TEXT("×"), 18);

	UTextBlock* RootTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpiritRootTitle"));
	RootTitle->SetText(FText::FromString(TEXT("先天灵根")));
	BuildStyleText(RootTitle, 20, FLinearColor::White);
	BuildSetLayout(Canvas->AddChildToCanvas(RootTitle), FVector2D(24.0f, 72.0f), FVector2D(250.0f, 30.0f));
	RootNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpiritRootName"));
	BuildStyleText(RootNameText, 27, FLinearColor::White);
	BuildSetLayout(Canvas->AddChildToCanvas(RootNameText), FVector2D(24.0f, 110.0f), FVector2D(250.0f, 42.0f));
	RootMetaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpiritRootMeta"));
	BuildStyleText(RootMetaText, 15, FLinearColor(0.75f, 0.88f, 0.96f));
	BuildSetLayout(Canvas->AddChildToCanvas(RootMetaText), FVector2D(24.0f, 154.0f), FVector2D(250.0f, 30.0f));
	RootDescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpiritRootDescription"));
	RootDescriptionText->SetAutoWrapText(true);
	BuildStyleText(RootDescriptionText, 15, FLinearColor(0.9f, 0.92f, 0.94f));
	BuildSetLayout(Canvas->AddChildToCanvas(RootDescriptionText), FVector2D(24.0f, 190.0f), FVector2D(250.0f, 90.0f));
	RootEffectText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpiritRootEffects"));
	RootEffectText->SetAutoWrapText(true);
	BuildStyleText(RootEffectText, 15, FLinearColor(0.54f, 1.0f, 0.72f));
	BuildSetLayout(Canvas->AddChildToCanvas(RootEffectText), FVector2D(24.0f, 290.0f), FVector2D(250.0f, 170.0f));
	UTextBlock* RootNote = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpiritRootNote"));
	RootNote->SetAutoWrapText(true);
	RootNote->SetText(FText::FromString(TEXT("灵根首次进入游戏时随机觉醒并永久保存。变异灵根可共鸣全部元素。")));
	BuildStyleText(RootNote, 13, FLinearColor(0.68f, 0.72f, 0.78f));
	BuildSetLayout(Canvas->AddChildToCanvas(RootNote), FVector2D(24.0f, 474.0f), FVector2D(250.0f, 92.0f));

	UTextBlock* PathTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CultivationPathTitle"));
	PathTitle->SetText(FText::FromString(TEXT("修炼流派")));
	BuildStyleText(PathTitle, 20, FLinearColor::White);
	BuildSetLayout(Canvas->AddChildToCanvas(PathTitle), FVector2D(292.0f, 72.0f), FVector2D(230.0f, 30.0f));
	UScrollBox* PathScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CultivationPathScroll"));
	BuildSetLayout(Canvas->AddChildToCanvas(PathScroll), FVector2D(288.0f, 106.0f), FVector2D(240.0f, 460.0f));
	PathList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CultivationPathList"));
	PathScroll->AddChild(PathList);

	PathNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PathDetailName"));
	BuildStyleText(PathNameText, 25, FLinearColor::White);
	BuildSetLayout(Canvas->AddChildToCanvas(PathNameText), FVector2D(550.0f, 76.0f), FVector2D(320.0f, 38.0f));
	PathDescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PathDetailDescription"));
	PathDescriptionText->SetAutoWrapText(true);
	BuildStyleText(PathDescriptionText, 14, FLinearColor(0.88f, 0.9f, 0.94f));
	BuildSetLayout(Canvas->AddChildToCanvas(PathDescriptionText), FVector2D(550.0f, 118.0f), FVector2D(320.0f, 65.0f));
	PathSkillText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PathSkill"));
	PathSkillText->SetAutoWrapText(true);
	BuildStyleText(PathSkillText, 15, FLinearColor(1.0f, 0.70f, 0.28f));
	BuildSetLayout(Canvas->AddChildToCanvas(PathSkillText), FVector2D(550.0f, 190.0f), FVector2D(320.0f, 82.0f));
	PathBonusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PathBonuses"));
	PathBonusText->SetAutoWrapText(true);
	BuildStyleText(PathBonusText, 14, FLinearColor(0.54f, 1.0f, 0.72f));
	BuildSetLayout(Canvas->AddChildToCanvas(PathBonusText), FVector2D(550.0f, 278.0f), FVector2D(320.0f, 85.0f));
	EquipmentText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PathEquipment"));
	EquipmentText->SetAutoWrapText(true);
	BuildStyleText(EquipmentText, 14, FLinearColor(0.58f, 0.86f, 1.0f));
	BuildSetLayout(Canvas->AddChildToCanvas(EquipmentText), FVector2D(550.0f, 367.0f), FVector2D(320.0f, 55.0f));
	CostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PathCost"));
	CostText->SetAutoWrapText(true);
	BuildStyleText(CostText, 13, FLinearColor(0.96f, 0.86f, 0.56f));
	BuildSetLayout(Canvas->AddChildToCanvas(CostText), FVector2D(550.0f, 426.0f), FVector2D(320.0f, 58.0f));

	ChoosePathButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ChooseCultivationPath"));
	ChoosePathButton->SetStyle(BuildButtonStyle(FVector2D(160.0f, 48.0f), FLinearColor(0.18f, 0.56f, 0.38f)));
	ChoosePathButton->OnClicked.AddDynamic(this, &UImmortalCharacterBuildWidget::HandleChoosePathClicked);
	BuildSetLayout(Canvas->AddChildToCanvas(ChoosePathButton), FVector2D(630.0f, 494.0f), FVector2D(160.0f, 48.0f));
	BuildAddButtonLabel(WidgetTree, ChoosePathButton, TEXT("选择 / 转修"));
	ResultText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CharacterBuildResult"));
	ResultText->SetAutoWrapText(true);
	ResultText->SetJustification(ETextJustify::Center);
	BuildStyleText(ResultText, 14, FLinearColor(1.0f, 0.72f, 0.28f));
	BuildSetLayout(Canvas->AddChildToCanvas(ResultText), FVector2D(545.0f, 548.0f), FVector2D(330.0f, 42.0f));
	RefreshFromPlayer();
}

void UImmortalCharacterBuildWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Player.IsValid()
		&& (LastBuildRevision != Player->GetCharacterBuildRevision()
			|| LastEquipmentRevision != Player->GetEquipmentInventoryRevision()
			|| LastMaterialRevision != Player->GetMaterialInventoryRevision()
			|| LastSpiritStones != Player->GetGold()))
	{
		RefreshFromPlayer();
	}
}

void UImmortalCharacterBuildWidget::RefreshFromPlayer()
{
	if (!Player.IsValid() || !PathList) return;
	LastBuildRevision = Player->GetCharacterBuildRevision();
	LastEquipmentRevision = Player->GetEquipmentInventoryRevision();
	LastMaterialRevision = Player->GetMaterialInventoryRevision();
	LastSpiritStones = Player->GetGold();
	const FImmortalSpiritRootState RootState = Player->GetSpiritRootState();
	const FImmortalCultivationPathState PathState = Player->GetCultivationPathState();
	FImmortalSpiritRootDefinition RootDefinition;
	if (UImmortalCharacterPathLibrary::GetSpiritRootDefinition(RootState.Root, RootDefinition))
	{
		RootNameText->SetText(FText::FromString(FString::Printf(TEXT("%s  %s"),
			*RootDefinition.IconGlyph.ToString(), *RootDefinition.DisplayName.ToString())));
		RootNameText->SetColorAndOpacity(FSlateColor(UImmortalCharacterPathLibrary::GetSpiritRootColor(RootState.Root)));
		RootMetaText->SetText(FText::FromString(FString::Printf(TEXT("纯度 %.1f%% · %s属性"),
			RootState.Purity * 100.0f, *UImmortalCharacterPathLibrary::GetElementText(RootDefinition.Element).ToString())));
		RootDescriptionText->SetText(RootDefinition.Description);
		RootEffectText->SetText(FText::FromString(FString::Printf(
			TEXT("修炼速度 ×%.3f\n对应元素技能 ×%.3f\n丹药效果 ×%.3f\n当前修炼 %.2f/秒"),
			UImmortalCharacterPathLibrary::CalculateCultivationRateMultiplier(RootState),
			UImmortalCharacterPathLibrary::CalculateElementDamageMultiplier(RootState, RootDefinition.Element),
			Player->GetPillEffectMultiplier(), Player->GetCultivationPerSecond())));
	}
	ResourceText->SetText(FText::FromString(FString::Printf(TEXT("灵石 %d · 转修 %d 次 · 装备 %d / 背包 %d"),
		Player->GetGold(), PathState.SwitchCount, Player->GetEquippedItems().Num(), Player->GetInventoryItems().Num())));
	if (!UImmortalCharacterPathLibrary::GetKnownCultivationPaths().Contains(SelectedPath)) SelectedPath = EImmortalCultivationPath::Body;
	RebuildPathEntries();
	RefreshPathDetails();
}

void UImmortalCharacterBuildWidget::RebuildPathEntries()
{
	PathList->ClearChildren();
	const FImmortalCultivationPathState Current = Player->GetCultivationPathState();
	for (const EImmortalCultivationPath Path : UImmortalCharacterPathLibrary::GetKnownCultivationPaths())
	{
		UImmortalCharacterPathEntryWidget* Entry = CreateWidget<UImmortalCharacterPathEntryWidget>(this, UImmortalCharacterPathEntryWidget::StaticClass());
		if (!Entry) continue;
		Entry->InitializeEntry(this, Path, Current.Path == Path, SelectedPath == Path);
		PathList->AddChildToVerticalBox(Entry);
	}
}

void UImmortalCharacterBuildWidget::RefreshPathDetails()
{
	FImmortalCultivationPathDefinition Definition;
	if (!Player.IsValid() || !UImmortalCharacterPathLibrary::GetCultivationPathDefinition(SelectedPath, Definition)) return;
	const FImmortalCultivationPathState Current = Player->GetCultivationPathState();
	FImmortalCultivationPathState Preview;
	Preview.Path = SelectedPath;
	const FImmortalCharacterPathBonuses Bonuses = UImmortalCharacterPathLibrary::CalculatePathBonuses(Preview);
	PathNameText->SetText(FText::FromString(FString::Printf(TEXT("%s  %s%s"),
		*Definition.IconGlyph.ToString(), *Definition.DisplayName.ToString(), Current.Path == SelectedPath ? TEXT(" [当前]") : TEXT(""))));
	PathNameText->SetColorAndOpacity(FSlateColor(UImmortalCharacterPathLibrary::GetCultivationPathColor(SelectedPath)));
	PathDescriptionText->SetText(Definition.Description);
	PathSkillText->SetText(FText::FromString(FString::Printf(TEXT("秘技·%s\n%s\n触发：每 %d 次攻击 · 倍率 %.0f%%"),
		*Definition.SkillName.ToString(), *Definition.SkillDescription.ToString(),
		Definition.AttacksPerSkill, Definition.SkillMagnitude * 100.0f)));
	PathBonusText->SetText(FText::FromString(FString::Printf(
		TEXT("攻击 +%.0f%% · 防御 +%.0f%% · 生命 +%.0f%% · 灵力 +%.0f%%\n攻速 +%.0f%% · 暴击 +%.0f%% · 减伤 %.0f%% · 修炼 +%.0f%%"),
		(Bonuses.AttackMultiplier - 1.0f) * 100.0f, (Bonuses.DefenseMultiplier - 1.0f) * 100.0f,
		(Bonuses.HealthMultiplier - 1.0f) * 100.0f, (Bonuses.ManaMultiplier - 1.0f) * 100.0f,
		Bonuses.AttackSpeedBonus * 100.0f, Bonuses.CriticalChanceBonus * 100.0f,
		Bonuses.DamageReduction * 100.0f, (Bonuses.CultivationRateMultiplier - 1.0f) * 100.0f)));
	int32 Compatible = 0;
	for (const FImmortalEquipmentItem& Item : Player->GetEquippedItems())
	{
		if (UImmortalCharacterPathLibrary::IsEquipmentCompatible(SelectedPath, Item.Discipline)) ++Compatible;
	}
	EquipmentText->SetText(FText::FromString(FString::Printf(TEXT("装备要求：通用或%s契合 · 当前兼容 %d/%d"),
		*UImmortalEquipmentLibrary::GetDisciplineText(Definition.EquipmentDiscipline).ToString(),
		Compatible, Player->GetEquippedItems().Num())));
	const FImmortalCraftingCost Cost = UImmortalCharacterPathLibrary::GetPathSwitchCost(Current, SelectedPath);
	if (!Current.IsSelected()) CostText->SetText(FText::FromString(TEXT("首次选择免费；选择后自动整理契合装备")));
	else if (Current.Path == SelectedPath) CostText->SetText(FText::FromString(TEXT("当前流派正在运转")));
	else CostText->SetText(FText::FromString(TEXT("转修消耗：")
		+ UImmortalCraftingLibrary::FormatCost(Cost, Player->GetMaterialInventory(), Player->GetGold()).ToString()));
	ChoosePathButton->SetIsEnabled(Player->CanSelectCultivationPath(SelectedPath));
}

void UImmortalCharacterBuildWidget::SelectPath(const EImmortalCultivationPath Path)
{
	SelectedPath = Path;
	if (ResultText) ResultText->SetText(FText::GetEmpty());
	RefreshFromPlayer();
}

void UImmortalCharacterBuildWidget::HandleChoosePathClicked()
{
	if (!Player.IsValid()) return;
	const FImmortalCharacterPathOperationResult Result = Player->SelectCultivationPath(SelectedPath);
	ResultText->SetText(Result.Message);
	RefreshFromPlayer();
}

void UImmortalCharacterBuildWidget::HandleCloseClicked()
{
	if (Player.IsValid()) Player->ToggleCharacterBuild();
}
