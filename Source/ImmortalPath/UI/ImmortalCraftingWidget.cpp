// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalCraftingWidget.h"

#include "ImmortalCraftingEntryWidget.h"
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
	FSlateBrush MakeCraftingBrush(const TCHAR* AssetPath, const FVector2D Size, const FLinearColor Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(Tint);
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, AssetPath)) Brush.SetResourceObject(Texture);
		return Brush;
	}

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

	FButtonStyle MakeCraftingButtonStyle(const FVector2D Size, const FLinearColor& Tint)
	{
		const FSlateBrush Brush = MakeCraftingBrush(
			TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal"), Size, Tint);
		FButtonStyle Style;
		Style.SetNormal(Brush);
		Style.SetHovered(Brush);
		Style.SetPressed(Brush);
		Style.SetDisabled(Brush);
		return Style;
	}
}

void UImmortalCraftingWidget::InitializeForPlayer(AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalCraftingWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CraftingPanelSize"));
	Root->SetWidthOverride(900.0f);
	Root->SetHeightOverride(600.0f);
	WidgetTree->RootWidget = Root;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CraftingCanvas"));
	Root->AddChild(Canvas);

	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CraftingBackground"));
	Background->SetBrush(MakeCraftingBrush(
		TEXT("/Game/GAME/Asset/ui/inventory/panel_background.panel_background"), FVector2D(900.0f, 600.0f),
		FLinearColor(0.9f, 0.88f, 0.78f, 0.98f)));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(Background), FVector2D::ZeroVector, FVector2D(900.0f, 600.0f));

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftingTitle"));
	Title->SetText(FText::FromString(TEXT("青云炼器炉")));
	StyleCraftingText(Title, 28, FLinearColor(1.0f, 0.72f, 0.25f));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(Title), FVector2D(34.0f, 18.0f), FVector2D(240.0f, 42.0f));
	UButton* ArtifactFurnaceButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("OpenArtifactFurnace"));
	ArtifactFurnaceButton->OnClicked.AddDynamic(this, &UImmortalCraftingWidget::HandleArtifactFurnaceClicked);
	ArtifactFurnaceButton->SetStyle(MakeCraftingButtonStyle(FVector2D(145.0f, 40.0f), FLinearColor(0.48f, 0.25f, 0.68f)));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(ArtifactFurnaceButton), FVector2D(282.0f, 16.0f), FVector2D(145.0f, 40.0f));
	UTextBlock* ArtifactFurnaceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("OpenArtifactFurnaceText"));
	ArtifactFurnaceText->SetText(FText::FromString(TEXT("法宝炉 [F]")));
	ArtifactFurnaceText->SetJustification(ETextJustify::Center);
	StyleCraftingText(ArtifactFurnaceText, 16, FLinearColor(0.94f, 0.76f, 1.0f));
	ArtifactFurnaceButton->AddChild(ArtifactFurnaceText);
	CurrencyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftingCurrency"));
	StyleCraftingText(CurrencyText, 17, FLinearColor(0.65f, 0.95f, 1.0f));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(CurrencyText), FVector2D(565.0f, 27.0f), FVector2D(240.0f, 30.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CraftingClose"));
	CloseButton->OnClicked.AddDynamic(this, &UImmortalCraftingWidget::HandleCloseClicked);
	CloseButton->SetStyle(MakeCraftingButtonStyle(FVector2D(64.0f), FLinearColor::White));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(CloseButton), FVector2D(816.0f, 10.0f), FVector2D(64.0f));
	UTextBlock* CloseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftingCloseText"));
	CloseText->SetText(FText::FromString(TEXT("×")));
	CloseText->SetJustification(ETextJustify::Center);
	StyleCraftingText(CloseText, 28, FLinearColor(1.0f, 0.72f, 0.25f));
	CloseButton->AddChild(CloseText);

	UTextBlock* RecipeTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftingRecipeTitle"));
	RecipeTitle->SetText(FText::FromString(TEXT("打造配方")));
	StyleCraftingText(RecipeTitle, 20, FLinearColor::White);
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(RecipeTitle), FVector2D(28.0f, 76.0f), FVector2D(180.0f, 30.0f));
	UScrollBox* RecipeScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CraftingRecipeScroll"));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(RecipeScroll), FVector2D(24.0f, 108.0f), FVector2D(260.0f, 410.0f));
	RecipeList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CraftingRecipeList"));
	RecipeScroll->AddChild(RecipeList);

	RecipeNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftingRecipeName"));
	StyleCraftingText(RecipeNameText, 22, FLinearColor(0.45f, 1.0f, 0.7f));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(RecipeNameText), FVector2D(300.0f, 80.0f), FVector2D(260.0f, 32.0f));
	RecipeDescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftingRecipeDescription"));
	RecipeDescriptionText->SetAutoWrapText(true);
	StyleCraftingText(RecipeDescriptionText, 15, FLinearColor(0.86f, 0.88f, 0.92f));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(RecipeDescriptionText), FVector2D(300.0f, 118.0f), FVector2D(260.0f, 70.0f));
	RecipeCostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftingRecipeCost"));
	RecipeCostText->SetAutoWrapText(true);
	StyleCraftingText(RecipeCostText, 16, FLinearColor(1.0f, 0.78f, 0.35f));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(RecipeCostText), FVector2D(300.0f, 198.0f), FVector2D(260.0f, 145.0f));
	CraftButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CraftEquipmentButton"));
	CraftButton->OnClicked.AddDynamic(this, &UImmortalCraftingWidget::HandleCraftClicked);
	CraftButton->SetStyle(MakeCraftingButtonStyle(FVector2D(220.0f, 52.0f), FLinearColor(0.3f, 0.72f, 0.46f)));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(CraftButton), FVector2D(318.0f, 350.0f), FVector2D(220.0f, 52.0f));
	CraftButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftEquipmentButtonText"));
	CraftButtonText->SetJustification(ETextJustify::Center);
	StyleCraftingText(CraftButtonText, 19, FLinearColor::White);
	CraftButton->AddChild(CraftButtonText);

	UTextBlock* EquipmentTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftingEquipmentTitle"));
	EquipmentTitle->SetText(FText::FromString(TEXT("装备强化 / 洗炼")));
	StyleCraftingText(EquipmentTitle, 20, FLinearColor::White);
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(EquipmentTitle), FVector2D(584.0f, 76.0f), FVector2D(260.0f, 30.0f));
	UScrollBox* EquipmentScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CraftingEquipmentScroll"));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(EquipmentScroll), FVector2D(580.0f, 108.0f), FVector2D(280.0f, 175.0f));
	EquipmentList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CraftingEquipmentList"));
	EquipmentScroll->AddChild(EquipmentList);
	ItemNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftingItemName"));
	StyleCraftingText(ItemNameText, 18, FLinearColor::White);
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(ItemNameText), FVector2D(584.0f, 292.0f), FVector2D(270.0f, 28.0f));
	ItemStatsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftingItemStats"));
	StyleCraftingText(ItemStatsText, 14, FLinearColor(0.84f, 0.88f, 0.94f));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(ItemStatsText), FVector2D(584.0f, 322.0f), FVector2D(270.0f, 66.0f));
	AffixText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftingAffixes"));
	AffixText->SetAutoWrapText(true);
	StyleCraftingText(AffixText, 13, FLinearColor(0.55f, 0.95f, 0.76f));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(AffixText), FVector2D(584.0f, 390.0f), FVector2D(270.0f, 70.0f));
	EnhancementCostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnhancementCost"));
	StyleCraftingText(EnhancementCostText, 12, FLinearColor(0.9f, 0.84f, 0.62f));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(EnhancementCostText), FVector2D(584.0f, 464.0f), FVector2D(132.0f, 58.0f));
	RefinementCostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RefinementCost"));
	StyleCraftingText(RefinementCostText, 12, FLinearColor(0.9f, 0.84f, 0.62f));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(RefinementCostText), FVector2D(722.0f, 464.0f), FVector2D(132.0f, 58.0f));
	EnhanceButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EnhanceEquipmentButton"));
	EnhanceButton->OnClicked.AddDynamic(this, &UImmortalCraftingWidget::HandleEnhanceClicked);
	EnhanceButton->SetStyle(MakeCraftingButtonStyle(FVector2D(128.0f, 44.0f), FLinearColor(0.28f, 0.56f, 0.82f)));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(EnhanceButton), FVector2D(584.0f, 525.0f), FVector2D(128.0f, 44.0f));
	UTextBlock* EnhanceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnhanceText"));
	EnhanceText->SetText(FText::FromString(TEXT("强化一次")));
	EnhanceText->SetJustification(ETextJustify::Center);
	StyleCraftingText(EnhanceText, 17, FLinearColor::White);
	EnhanceButton->AddChild(EnhanceText);
	RefineButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RefineEquipmentButton"));
	RefineButton->OnClicked.AddDynamic(this, &UImmortalCraftingWidget::HandleRefineClicked);
	RefineButton->SetStyle(MakeCraftingButtonStyle(FVector2D(128.0f, 44.0f), FLinearColor(0.62f, 0.34f, 0.78f)));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(RefineButton), FVector2D(726.0f, 525.0f), FVector2D(128.0f, 44.0f));
	UTextBlock* RefineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RefineText"));
	RefineText->SetText(FText::FromString(TEXT("洗炼词条")));
	RefineText->SetJustification(ETextJustify::Center);
	StyleCraftingText(RefineText, 17, FLinearColor::White);
	RefineButton->AddChild(RefineText);

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftingResult"));
	ResultText->SetAutoWrapText(true);
	ResultText->SetJustification(ETextJustify::Center);
	StyleCraftingText(ResultText, 16, FLinearColor(1.0f, 0.78f, 0.3f));
	SetCraftingCanvasLayout(Canvas->AddChildToCanvas(ResultText), FVector2D(292.0f, 424.0f), FVector2D(280.0f, 140.0f));
	RefreshFromPlayer();
}

void UImmortalCraftingWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Player.IsValid()
		&& (LastEquipmentRevision != Player->GetEquipmentInventoryRevision()
			|| LastMaterialRevision != Player->GetMaterialInventoryRevision()
			|| LastSpiritStones != Player->GetGold()
			|| LastStage != Player->GetQingyunStage()))
	{
		RefreshFromPlayer();
	}
}

void UImmortalCraftingWidget::RefreshFromPlayer()
{
	if (!Player.IsValid() || !RecipeList || !EquipmentList) return;
	LastEquipmentRevision = Player->GetEquipmentInventoryRevision();
	LastMaterialRevision = Player->GetMaterialInventoryRevision();
	LastSpiritStones = Player->GetGold();
	LastStage = Player->GetQingyunStage();
	CurrencyText->SetText(FText::FromString(FString::Printf(TEXT("灵石 %d · 青云山第 %d 关"), LastSpiritStones, LastStage)));

	const TArray<FName> Recipes = UImmortalCraftingLibrary::GetKnownRecipeIds();
	if (SelectedRecipeId.IsNone() || !Recipes.Contains(SelectedRecipeId)) SelectedRecipeId = Recipes.IsEmpty() ? NAME_None : Recipes[0];
	FImmortalEquipmentItem SelectedItem;
	bool bEquipped = false;
	if (!Player->GetEquipmentItemById(SelectedItemId, SelectedItem, bEquipped))
	{
		const TArray<FImmortalEquipmentItem> Equipped = Player->GetEquippedItems();
		const TArray<FImmortalEquipmentItem> Inventory = Player->GetInventoryItems();
		SelectedItemId = !Equipped.IsEmpty() ? Equipped[0].ItemId : (!Inventory.IsEmpty() ? Inventory[0].ItemId : FGuid());
	}
	RebuildRecipeEntries();
	RebuildEquipmentEntries();
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
}

void UImmortalCraftingWidget::RefreshRecipeDetails()
{
	FImmortalCraftingRecipeDefinition Definition;
	if (!UImmortalCraftingLibrary::GetRecipeDefinition(SelectedRecipeId, Definition)) return;
	RecipeNameText->SetText(Definition.DisplayName);
	RecipeNameText->SetColorAndOpacity(FSlateColor(UImmortalEquipmentLibrary::GetQualityColor(Definition.OutputQuality)));
	RecipeDescriptionText->SetText(FText::FromString(FString::Printf(TEXT("%s\n产物：%s · %s"),
		*Definition.Description.ToString(),
		*UImmortalEquipmentLibrary::GetQualityText(Definition.OutputQuality).ToString(),
		*UImmortalEquipmentLibrary::GetSlotText(Definition.OutputSlot).ToString())));
	RecipeCostText->SetText(UImmortalCraftingLibrary::FormatCost(Definition.Cost, Player->GetMaterialInventory(), Player->GetGold()));
	const bool bUnlocked = Player->IsCraftingRecipeUnlocked(SelectedRecipeId);
	const bool bCanCraft = Player->CanCraftEquipment(SelectedRecipeId);
	CraftButton->SetIsEnabled(bCanCraft);
	CraftButtonText->SetText(FText::FromString(!bUnlocked ? TEXT("关卡未解锁") : (bCanCraft ? TEXT("打造装备") : TEXT("材料或灵石不足"))));
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
		EnhanceButton->SetIsEnabled(false);
		RefineButton->SetIsEnabled(false);
		return;
	}
	ItemNameText->SetText(FText::FromString(FString::Printf(TEXT("%s%s  +%d"),
		bEquipped ? TEXT("[已装备] ") : TEXT(""), *Item.DisplayName.ToString(), Item.EnhancementLevel)));
	ItemNameText->SetColorAndOpacity(FSlateColor(UImmortalEquipmentLibrary::GetQualityColor(Item.Quality)));
	ItemStatsText->SetText(FText::FromString(FString::Printf(
		TEXT("等级 %d · %s契合 · 战力 %.1f · 洗炼 %d 次\n攻击 %.1f  防御 %.1f  生命 %.1f\n攻速 %.1f%%  暴击 %.1f%%"),
		Item.ItemLevel, *UImmortalEquipmentLibrary::GetDisciplineText(Item.Discipline).ToString(),
		UImmortalEquipmentLibrary::CalculateEquipmentPower(Item), Item.RefinementCount,
		Item.AttackBonus, Item.DefenseBonus, Item.HealthBonus,
		Item.AttackSpeedBonus * 100.0f, Item.CriticalChanceBonus * 100.0f)));
	FString Affixes = TEXT("词条：");
	for (const FImmortalEquipmentAffix& Affix : Item.Affixes) Affixes += TEXT("\n") + UImmortalEquipmentLibrary::GetAffixText(Affix).ToString();
	AffixText->SetText(FText::FromString(Affixes));
	const FImmortalCraftingCost EnhancementCost = UImmortalCraftingLibrary::GetEnhancementCost(Item);
	const FImmortalCraftingCost RefinementCost = UImmortalCraftingLibrary::GetRefinementCost(Item);
	EnhancementCostText->SetText(Item.EnhancementLevel >= 15
		? FText::FromString(TEXT("强化已满级"))
		: UImmortalCraftingLibrary::FormatCost(EnhancementCost, Player->GetMaterialInventory(), Player->GetGold()));
	RefinementCostText->SetText(UImmortalCraftingLibrary::FormatCost(RefinementCost, Player->GetMaterialInventory(), Player->GetGold()));
	EnhanceButton->SetIsEnabled(Player->CanEnhanceEquipment(SelectedItemId));
	RefineButton->SetIsEnabled(Player->CanRefineEquipment(SelectedItemId));
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
