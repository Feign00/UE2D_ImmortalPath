// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalCraftingWidget.h"
#include "ImmortalCraftingArt.h"
#include "ImmortalFeaturePageLayout.h"
#include "ImmortalUITheme.h"

#include "ImmortalCraftingEntryWidget.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Crafting/ImmortalCraftingTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
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
	void SetCraftingCanvasLayout(UCanvasPanelSlot* CanvasSlot, const FVector2D Position, const FVector2D Size)
	{
		if (!CanvasSlot) return;
		CanvasSlot->SetPosition(Position);
		CanvasSlot->SetSize(Size);
		CanvasSlot->SetAutoSize(false);
	}

	void StyleCraftingText(UTextBlock* Text, const int32 Size, const FLinearColor& Color)
	{
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor::Black);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}

}

UTexture2D* UImmortalCraftingWidget::GetEquipmentAtlas() const { return EquipmentAtlas.LoadSynchronous(); }
UTexture2D* UImmortalCraftingWidget::GetForgeAtlas() const { return ForgeAtlas.LoadSynchronous(); }
UTexture2D* UImmortalCraftingWidget::GetMaterialAtlas() const { return MaterialAtlas.LoadSynchronous(); }

void UImmortalCraftingWidget::InitializeForPlayer(AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalCraftingWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CraftingPanelSize"));
	Root->SetWidthOverride(1600);
	Root->SetHeightOverride(600);
	WidgetTree->RootWidget = Root;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CraftingCanvas"));
	Root->AddChild(Canvas);
	ImmortalFeaturePageLayout::AddReadabilityBackground(WidgetTree, Canvas);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const float X[] = {8, 304, 744, 1024};
		const float Width[] = {288, 432, 272, 568};
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
			FName(*FString::Printf(TEXT("CraftingSection%d"), Index)));
		Card->SetBrush(ImmortalUITheme::PanelBrush(FLinearColor(0.035f, 0.055f, 0.058f)));
		Card->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetCraftingCanvasLayout(Canvas->AddChildToCanvas(Card), FVector2D(X[Index], 52), FVector2D(Width[Index], 536));
	}
	const auto Text = [this, Canvas](const TCHAR* Name, const TCHAR* Label, FVector2D Position, FVector2D Size, int32 Font = 18)
	{
		UTextBlock* Widget = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
		Widget->SetText(FText::FromString(Label));
		StyleCraftingText(Widget, Font, FLinearColor(0.9f, 0.94f, 0.91f));
		SetCraftingCanvasLayout(Canvas->AddChildToCanvas(Widget), Position, Size);
		return Widget;
	};
	const auto Button = [this, Canvas](const TCHAR* Name, const TCHAR* Label, FVector2D Position, FVector2D Size)
	{
		UButton* Widget = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(Name));
		Widget->SetStyle(ImmortalUITheme::ButtonStyle());
		SetCraftingCanvasLayout(Canvas->AddChildToCanvas(Widget), Position, Size);
		UTextBlock* Caption = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
			FName(*(FString(Name) + TEXT("Text"))));
		Caption->SetText(FText::FromString(Label));
		Caption->SetJustification(ETextJustify::Center);
		StyleCraftingText(Caption, 20, FLinearColor(0.9f, 0.94f, 0.91f));
		Widget->AddChild(Caption);
		ImmortalFeaturePageLayout::StabilizeButtonLabel(Widget, Caption);
		return Widget;
	};
	const auto List = [this, Canvas](const TCHAR* Name, FVector2D Position, FVector2D Size)
	{
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),
			FName(*(FString(Name) + TEXT("Scroll"))));
		ImmortalFeaturePageLayout::StyleScrollBox(Scroll);
		SetCraftingCanvasLayout(Canvas->AddChildToCanvas(Scroll), Position, Size);
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(Name));
		Scroll->AddChild(Box);
		return Box;
	};
	const auto Art = [this, Canvas](const TCHAR* Name, const FSlateBrush& Brush, FVector2D Position, float Size)
	{
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(Name));
		Image->SetBrush(Brush);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetCraftingCanvasLayout(Canvas->AddChildToCanvas(Image), Position, FVector2D(Size));
		return Image;
	};
	Text(TEXT("CraftingTitle"), TEXT("青云炼器坊"), {22, 6}, {290, 38}, 28);
	UButton* Artifact = Button(TEXT("OpenArtifactFurnace"), TEXT("法宝炉 [F]"), {342, 6}, {180, 38});
	Artifact->OnClicked.AddDynamic(this, &ThisClass::HandleArtifactFurnaceClicked);
	CurrencyText = Text(TEXT("CraftingCurrency"), TEXT(""), {950, 10}, {572, 30}, 18);
	CurrencyText->SetJustification(ETextJustify::Right);
	UButton* Close = Button(TEXT("CraftingClose"), TEXT("×"), {1538, 6}, {44, 38});
	Close->OnClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);

	Text(TEXT("CraftingRecipeTitle"), TEXT("打造配方"), {22, 64}, {264, 30}, 22);
	RecipeList = List(TEXT("CraftingRecipeList"), {18, 104}, {268, 470});
	RecipeNameText = Text(TEXT("CraftingRecipeName"), TEXT(""), {322, 68}, {236, 64}, 24);
	RecipeNameText->SetAutoWrapText(true);
	RecipeIcon = Art(TEXT("CraftingRecipeIcon"), FSlateBrush(), {578, 72}, 136);
	RecipeDescriptionText = Text(TEXT("CraftingRecipeDescription"), TEXT(""), {322, 142}, {238, 124});
	RecipeDescriptionText->SetAutoWrapText(true);
	Art(TEXT("CraftingForgeArt"), ImmortalCraftingArt::ForgeBrush(GetForgeAtlas(), TEXT("Forge")), {586, 224}, 120);
	RecipeCostText = Text(TEXT("CraftingRecipeCost"), TEXT("打造消耗"), {322, 286}, {236, 30}, 20);
	RecipeCostList = List(TEXT("CraftingRecipeCosts"), {322, 326}, {392, 176});
	CraftButton = Button(TEXT("CraftEquipmentButton"), TEXT("打造装备"), {322, 524}, {392, 50});
	CraftButtonText = CastChecked<UTextBlock>(WidgetTree->FindWidget(TEXT("CraftEquipmentButtonText")));
	CraftButton->OnClicked.AddDynamic(this, &ThisClass::HandleCraftClicked);

	Text(TEXT("CraftingEquipmentTitle"), TEXT("选择装备"), {758, 64}, {242, 30}, 22);
	EquipmentList = List(TEXT("CraftingEquipmentList"), {754, 104}, {250, 428});
	ItemIcon = Art(TEXT("CraftingItemIcon"), FSlateBrush(), {1040, 70}, 100);
	ItemNameText = Text(TEXT("CraftingItemName"), TEXT(""), {1154, 76}, {416, 86}, 22);
	ItemNameText->SetAutoWrapText(true);
	ItemStatsText = Text(TEXT("CraftingItemStats"), TEXT(""), {1040, 180}, {532, 80});
	ItemStatsText->SetAutoWrapText(true);
	AffixText = Text(TEXT("CraftingAffixes"), TEXT(""), {1040, 270}, {532, 80});
	AffixText->SetAutoWrapText(true);
	EnhancementCostText = Text(TEXT("EnhancementCost"), TEXT("强化消耗"), {1040, 354}, {252, 28}, 20);
	RefinementCostText = Text(TEXT("RefinementCost"), TEXT("洗炼消耗"), {1320, 354}, {252, 28}, 20);
	EnhancementCostList = List(TEXT("CraftingEnhancementCosts"), {1040, 388}, {252, 86});
	RefinementCostList = List(TEXT("CraftingRefinementCosts"), {1320, 388}, {252, 86});
	EnhanceButton = Button(TEXT("EnhanceEquipmentButton"), TEXT("强化一次"), {1040, 484}, {252, 48});
	EnhanceButton->OnClicked.AddDynamic(this, &ThisClass::HandleEnhanceClicked);
	RefineButton = Button(TEXT("RefineEquipmentButton"), TEXT("洗炼词条"), {1320, 484}, {252, 48});
	RefineButton->OnClicked.AddDynamic(this, &ThisClass::HandleRefineClicked);
	ResultText = Text(TEXT("CraftingResult"), TEXT("打造、强化与洗炼结果将在这里显示"), {758, 544}, {814, 38});
	ResultText->SetAutoWrapText(true);
	for (UTextBlock* Value : {RecipeNameText, RecipeDescriptionText, ItemNameText, ItemStatsText, AffixText, ResultText})
		ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, Value);
	RefreshFromPlayer();
}

void UImmortalCraftingWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Player.IsValid()
		&& (LastEquipmentRevision != Player->GetEquipmentInventoryRevision()
			|| LastMaterialRevision != Player->GetMaterialInventoryRevision()
			|| LastSpiritStones != Player->GetGold()
			|| LastStage != Player->GetQingyunStage()
			|| LastCaveRevision != Player->GetCaveRevision()))
	{
		RefreshFromPlayer();
	}
}

void UImmortalCraftingWidget::RefreshFromPlayer()
{
	if (!Player.IsValid() || !RecipeList || !EquipmentList) return;
	const bool bEquipmentChanged = LastEquipmentRevision != Player->GetEquipmentInventoryRevision();
	const bool bStageChanged = LastStage != Player->GetQingyunStage();
	LastEquipmentRevision = Player->GetEquipmentInventoryRevision();
	LastMaterialRevision = Player->GetMaterialInventoryRevision();
	LastSpiritStones = Player->GetGold();
	LastStage = Player->GetQingyunStage();
	LastCaveRevision = Player->GetCaveRevision();
	CurrencyText->SetText(FText::FromString(FString::Printf(TEXT("灵石 %d · 青云山第 %d 关"), LastSpiritStones, LastStage)));

	const TArray<FName> Recipes = UImmortalCraftingLibrary::GetKnownRecipeIds();
	if (SelectedRecipeId.IsNone() || !Recipes.Contains(SelectedRecipeId))
	{
		int32 EarliestStage = MAX_int32;
		SelectedRecipeId = NAME_None;
		for (FName Id : Recipes)
		{
			FImmortalCraftingRecipeDefinition Definition;
			if (UImmortalCraftingLibrary::GetRecipeDefinition(Id, Definition) && Definition.MinimumQingyunStage < EarliestStage)
			{
				SelectedRecipeId = Id;
				EarliestStage = Definition.MinimumQingyunStage;
			}
		}
	}
	FImmortalEquipmentItem SelectedItem;
	bool bEquipped = false;
	if (!Player->GetEquipmentItemById(SelectedItemId, SelectedItem, bEquipped))
	{
		const TArray<FImmortalEquipmentItem> Equipped = Player->GetEquippedItems();
		const TArray<FImmortalEquipmentItem> Inventory = Player->GetInventoryItems();
		SelectedItemId = !Equipped.IsEmpty() ? Equipped[0].ItemId : (!Inventory.IsEmpty() ? Inventory[0].ItemId : FGuid());
	}
	// Combat may change currency every second; do not destroy the row under the mouse for a cost update.
	if (bStageChanged || LastRenderedRecipeId != SelectedRecipeId || RecipeList->GetChildrenCount() == 0)
		RebuildRecipeEntries();
	if (bEquipmentChanged || LastRenderedItemId != SelectedItemId || EquipmentList->GetChildrenCount() == 0)
		RebuildEquipmentEntries();
	LastRenderedRecipeId = SelectedRecipeId;
	LastRenderedItemId = SelectedItemId;
	RefreshRecipeDetails();
	RefreshEquipmentDetails();
}

void UImmortalCraftingWidget::RebuildRecipeEntries()
{
	RecipeList->ClearChildren();
	TArray<FName> Recipes = UImmortalCraftingLibrary::GetKnownRecipeIds();
	Recipes.Sort([this](const FName A, const FName B)
	{
		FImmortalCraftingRecipeDefinition Left, Right;
		UImmortalCraftingLibrary::GetRecipeDefinition(A, Left);
		UImmortalCraftingLibrary::GetRecipeDefinition(B, Right);
		return Left.MinimumQingyunStage == Right.MinimumQingyunStage
			? Left.DisplayName.ToString() < Right.DisplayName.ToString()
			: Left.MinimumQingyunStage < Right.MinimumQingyunStage;
	});
	for (const FName RecipeId : Recipes)
	{
		UImmortalCraftingEntryWidget* Entry = CreateWidget<UImmortalCraftingEntryWidget>(
			GetOwningPlayer(), UImmortalCraftingEntryWidget::StaticClass());
		Entry->InitializeRecipeEntry(this, RecipeId, Player->IsCraftingRecipeUnlocked(RecipeId), RecipeId == SelectedRecipeId);
		RecipeList->AddChild(Entry);
	}
	RecipeList->ForceLayoutPrepass();
}

void UImmortalCraftingWidget::RebuildEquipmentEntries()
{
	EquipmentList->ClearChildren();
	for (const FImmortalEquipmentItem& Item : Player->GetEquippedItems())
	{
		UImmortalCraftingEntryWidget* Entry = CreateWidget<UImmortalCraftingEntryWidget>(GetOwningPlayer(), UImmortalCraftingEntryWidget::StaticClass());
		Entry->InitializeEquipmentEntry(this, Item, true, Item.ItemId == SelectedItemId);
		EquipmentList->AddChild(Entry);
	}
	for (const FImmortalEquipmentItem& Item : Player->GetInventoryItems())
	{
		UImmortalCraftingEntryWidget* Entry = CreateWidget<UImmortalCraftingEntryWidget>(GetOwningPlayer(), UImmortalCraftingEntryWidget::StaticClass());
		Entry->InitializeEquipmentEntry(this, Item, false, Item.ItemId == SelectedItemId);
		EquipmentList->AddChild(Entry);
	}
	EquipmentList->ForceLayoutPrepass();
}

void UImmortalCraftingWidget::RefreshRecipeDetails()
{
	FImmortalCraftingRecipeDefinition Definition;
	if (!UImmortalCraftingLibrary::GetRecipeDefinition(SelectedRecipeId, Definition)) return;
	RecipeNameText->SetText(Definition.DisplayName);
	RecipeNameText->SetColorAndOpacity(FSlateColor(UImmortalEquipmentLibrary::GetQualityColor(Definition.OutputQuality)));
	FImmortalEquipmentSetDefinition SetDefinition;
	const FString SetText = UImmortalEquipmentLibrary::GetSetDefinition(Definition.OutputSetId, SetDefinition)
		? FString::Printf(TEXT(" · %s"), *SetDefinition.DisplayName.ToString()) : FString();
	RecipeDescriptionText->SetText(FText::FromString(FString::Printf(TEXT("%s\n产物：%s · %s%s"),
		*Definition.Description.ToString(),
		*UImmortalEquipmentLibrary::GetQualityText(Definition.OutputQuality).ToString(),
		*UImmortalEquipmentLibrary::GetSlotText(Definition.OutputSlot).ToString(), *SetText)));
	const FImmortalCraftingCost EffectiveCost = Player->ApplyCaveForgeDiscount(Definition.Cost);
	RecipeIcon->SetBrush(ImmortalCraftingArt::EquipmentBrush(GetEquipmentAtlas(), Definition.OutputSlot));
	RecipeCostText->SetText(FText::FromString(TEXT("消耗 · 持有 / 需要")));
	RefreshCostList(RecipeCostList, EffectiveCost);
	const bool bUnlocked = Player->IsCraftingRecipeUnlocked(SelectedRecipeId);
	const bool bCanCraft = Player->CanCraftEquipment(SelectedRecipeId);
	const bool bAffordable = UImmortalCraftingLibrary::CanAfford( Player->GetMaterialInventory(), Player->GetGold(), EffectiveCost);
	CraftButton->SetIsEnabled(bCanCraft);
	CraftButtonText->SetText(FText::FromString(!bUnlocked ? TEXT("关卡未解锁") : (bCanCraft ? TEXT("打造装备")
		: (bAffordable ? TEXT("储物戒已满，请先整理") : TEXT("材料或灵石不足")))));
}

void UImmortalCraftingWidget::RefreshEquipmentDetails()
{
	FImmortalEquipmentItem Item;
	bool bEquipped = false;
	if (!Player->GetEquipmentItemById(SelectedItemId, Item, bEquipped))
	{
		ItemNameText->SetText(FText::FromString(TEXT("尚无可炼制装备")));
		ItemStatsText->SetText(FText::GetEmpty());
		AffixText->SetText(FText::GetEmpty());
		EnhancementCostText->SetText(FText::GetEmpty());
		RefinementCostText->SetText(FText::GetEmpty());
		ItemIcon->SetBrush(ImmortalCraftingArt::EquipmentBrush(nullptr, EImmortalEquipmentSlot::MAX));
		EnhancementCostList->ClearChildren();
		RefinementCostList->ClearChildren();
		EnhanceButton->SetIsEnabled(false);
		RefineButton->SetIsEnabled(false);
		return;
	}
	ItemNameText->SetText(FText::FromString(FString::Printf(TEXT("%s%s"),
		bEquipped ? TEXT("[已装备] ") : TEXT(""), *Item.DisplayName.ToString())));
	ItemNameText->SetColorAndOpacity(FSlateColor(UImmortalEquipmentLibrary::GetQualityColor(Item.Quality)));
	ItemIcon->SetBrush(ImmortalCraftingArt::EquipmentBrush(GetEquipmentAtlas(), Item.Slot));
	ItemStatsText->SetText(FText::FromString(FString::Printf(
		TEXT("等级 %d · %s契合 · 战力 %.1f · 洗炼 %d 次\n攻击 %.1f  防御 %.1f  生命 %.1f\n攻速 %.1f%%  暴击 %.1f%%"),
		Item.ItemLevel, *UImmortalEquipmentLibrary::GetDisciplineText(Item.Discipline).ToString(),
		UImmortalEquipmentLibrary::CalculateEquipmentPower(Item), Item.RefinementCount,
		Item.AttackBonus, Item.DefenseBonus, Item.HealthBonus,
		Item.AttackSpeedBonus * 100.0f, Item.CriticalChanceBonus * 100.0f)));
	FString Affixes = TEXT("词条：");
	for (const FImmortalEquipmentAffix& Affix : Item.Affixes) Affixes += TEXT("\n") + UImmortalEquipmentLibrary::GetAffixText(Affix).ToString();
	AffixText->SetText(FText::FromString(Affixes));
	const FImmortalCraftingCost EnhancementCost = Player->ApplyCaveForgeDiscount(
		UImmortalCraftingLibrary::GetEnhancementCost(Item));
	const FImmortalCraftingCost RefinementCost = Player->ApplyCaveForgeDiscount(
		UImmortalCraftingLibrary::GetRefinementCost(Item));
	EnhancementCostText->SetText(Item.EnhancementLevel >= 15
		? FText::FromString(TEXT("强化已满级"))
		: FText::FromString(TEXT("强化消耗")));
	RefinementCostText->SetText(FText::FromString(TEXT("洗炼消耗")));
	EnhancementCostList->ClearChildren();
	if (Item.EnhancementLevel < 15) RefreshCostList(EnhancementCostList, EnhancementCost);
	RefreshCostList(RefinementCostList, RefinementCost);
	EnhanceButton->SetIsEnabled(Player->CanEnhanceEquipment(SelectedItemId));
	RefineButton->SetIsEnabled(Player->CanRefineEquipment(SelectedItemId));
}

void UImmortalCraftingWidget::RefreshCostList(UVerticalBox* List, const FImmortalCraftingCost& Cost)
{
	List->ClearChildren();
	const auto AddCost = [this, List](FName Id, const FString& Name, int32 Owned, int32 Needed)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
		IconSize->SetWidthOverride(36); IconSize->SetHeightOverride(36);
		UImage* Icon = WidgetTree->ConstructWidget<UImage>();
		Icon->SetBrush(ImmortalCraftingArt::MaterialBrush(GetForgeAtlas(), GetMaterialAtlas(), Id));
		IconSize->AddChild(Icon);
		Row->AddChildToHorizontalBox(IconSize)->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(FString::Printf(TEXT("%s  %d / %d"), *Name, Owned, Needed)));
		Label->SetAutoWrapText(true);
		Label->SetToolTipText(FText::FromString(Owned >= Needed ? TEXT("材料充足") : TEXT("材料不足")));
		StyleCraftingText(Label, 18, Owned >= Needed ? FLinearColor(0.60f, 0.92f, 0.73f) : FLinearColor(1, 0.47f, 0.35f));
		UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Label);
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Slot->SetVerticalAlignment(VAlign_Center);
		Slot->SetPadding(FMargin(6, 4));
		List->AddChild(Row);
	};
	AddCost(TEXT("SpiritStones"), TEXT("灵石"), Player->GetGold(), Cost.SpiritStones);
	for (const FImmortalCraftingMaterialCost& MaterialCost : Cost.Materials)
	{
		FImmortalMaterialDefinition Definition;
		const bool bKnown = UImmortalMaterialLibrary::GetMaterialDefinition(MaterialCost.MaterialId, Definition);
		AddCost(MaterialCost.MaterialId, bKnown ? Definition.DisplayName.ToString() : MaterialCost.MaterialId.ToString(),
			Player->GetMaterialQuantity(MaterialCost.MaterialId), MaterialCost.Quantity);
	}
	List->ForceLayoutPrepass();
}

void UImmortalCraftingWidget::SelectRecipe(const FName RecipeId)
{
	SelectedRecipeId = RecipeId;
	RefreshFromPlayer();
}

void UImmortalCraftingWidget::SelectEquipment(const FGuid ItemId)
{
	SelectedItemId = ItemId;
	RefreshFromPlayer();
}

void UImmortalCraftingWidget::HandleCraftClicked()
{
	if (!Player.IsValid()) return;
	const FImmortalCraftingResult Result = Player->CraftEquipment(SelectedRecipeId);
	ResultText->SetText(Result.Message);
	ResultText->SetColorAndOpacity(FSlateColor(Result.bSucceeded ? FLinearColor(0.38f, 1.0f, 0.65f) : FLinearColor(1.0f, 0.38f, 0.28f)));
	if (Result.bSucceeded) SelectedItemId = Result.ItemId;
	RefreshFromPlayer();
}

void UImmortalCraftingWidget::HandleEnhanceClicked()
{
	if (!Player.IsValid()) return;
	const FImmortalCraftingResult Result = Player->EnhanceEquipment(SelectedItemId);
	ResultText->SetText(Result.Message);
	ResultText->SetColorAndOpacity(FSlateColor(Result.bSucceeded ? FLinearColor(0.38f, 1.0f, 0.65f) : FLinearColor(1.0f, 0.38f, 0.28f)));
	RefreshFromPlayer();
}

void UImmortalCraftingWidget::HandleRefineClicked()
{
	if (!Player.IsValid()) return;
	const FImmortalCraftingResult Result = Player->RefineEquipment(SelectedItemId);
	ResultText->SetText(Result.Message);
	ResultText->SetColorAndOpacity(FSlateColor(Result.bSucceeded ? FLinearColor(0.38f, 1.0f, 0.65f) : FLinearColor(1.0f, 0.38f, 0.28f)));
	RefreshFromPlayer();
}

void UImmortalCraftingWidget::HandleArtifactFurnaceClicked()
{
	if (Player.IsValid()) Player->ToggleArtifacts();
}

void UImmortalCraftingWidget::HandleCloseClicked()
{
	if (Player.IsValid()) Player->ToggleCrafting();
}
