// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalCraftingEntryWidget.h"

#include "ImmortalCraftingWidget.h"
#include "ImmortalCraftingArt.h"
#include "ImmortalFeaturePageLayout.h"
#include "ImmortalUITheme.h"
#include "../Crafting/ImmortalCraftingTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

void UImmortalCraftingEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CraftingEntrySize"));
	Root->SetWidthOverride(236.0f);
	Root->SetHeightOverride(88.0f);
	WidgetTree->RootWidget = Root;
	EntryButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CraftingEntryButton"));
	EntryButton->OnClicked.AddDynamic(this, &UImmortalCraftingEntryWidget::HandleClicked);
	Root->AddChild(EntryButton);
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Row->SetVisibility(ESlateVisibility::HitTestInvisible);
	UButtonSlot* ContentSlot = CastChecked<UButtonSlot>(EntryButton->AddChild(Row));
	ContentSlot->SetHorizontalAlignment(HAlign_Fill);
	ContentSlot->SetVerticalAlignment(VAlign_Fill);
	USizeBox* ArtSize = WidgetTree->ConstructWidget<USizeBox>();
	ArtSize->SetWidthOverride(56); ArtSize->SetHeightOverride(56);
	EntryIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CraftingEntryIcon"));
	ArtSize->AddChild(EntryIcon);
	Row->AddChildToHorizontalBox(ArtSize)->SetVerticalAlignment(VAlign_Center);
	EntryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftingEntryText"));
	EntryText->SetJustification(ETextJustify::Left);
	EntryText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	EntryText->SetShadowOffset(FVector2D(1.0f));
	EntryText->SetShadowColorAndOpacity(FLinearColor::Black);
	FSlateFontInfo Font = EntryText->GetFont();
	Font.Size = 18;
	EntryText->SetFont(Font);
	UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(EntryText);
	TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TextSlot->SetVerticalAlignment(VAlign_Center);
	TextSlot->SetPadding(FMargin(6, 0));
	RefreshAppearance();
}

void UImmortalCraftingEntryWidget::InitializeRecipeEntry(
	UImmortalCraftingWidget* InOwner,
	const FName InRecipeId,
	const bool bUnlocked,
	const bool bInSelected)
{
	OwnerCrafting = InOwner;
	RecipeId = InRecipeId;
	ItemId.Invalidate();
	bRecipeEntry = true;
	bEnabled = true; // Inspect locked recipes; crafting authorization remains in the owner/player.
	bSelected = bInSelected;
	FImmortalCraftingRecipeDefinition Definition;
	if (UImmortalCraftingLibrary::GetRecipeDefinition(RecipeId, Definition))
	{
		DisplaySlot = static_cast<int32>(Definition.OutputSlot);
		DisplayText = FText::FromString(FString::Printf(TEXT("%s\n%s"), *Definition.DisplayName.ToString(),
			bUnlocked ? *UImmortalEquipmentLibrary::GetQualityText(Definition.OutputQuality).ToString()
			: *FString::Printf(TEXT("第 %d 关解锁"), Definition.MinimumQingyunStage)));
		DisplayColor = bUnlocked
			? UImmortalEquipmentLibrary::GetQualityColor(Definition.OutputQuality)
			: FLinearColor(0.48f, 0.5f, 0.54f, 1.0f);
	}
	RefreshAppearance();
}

void UImmortalCraftingEntryWidget::InitializeEquipmentEntry(
	UImmortalCraftingWidget* InOwner,
	const FImmortalEquipmentItem& Item,
	const bool bEquipped,
	const bool bInSelected)
{
	OwnerCrafting = InOwner;
	ItemId = Item.ItemId;
	RecipeId = NAME_None;
	bRecipeEntry = false;
	bEnabled = Item.IsValid();
	bSelected = bInSelected;
	DisplaySlot = static_cast<int32>(Item.Slot);
	DisplayText = FText::FromString(FString::Printf(TEXT("%s\n%s强化 +%d"),
		*Item.DisplayName.ToString(), bEquipped ? TEXT("已装备 · ") : TEXT(""),
		Item.EnhancementLevel));
	DisplayColor = UImmortalEquipmentLibrary::GetQualityColor(Item.Quality);
	RefreshAppearance();
}

void UImmortalCraftingEntryWidget::RefreshAppearance()
{
	if (!EntryButton || !EntryText) return;
	EntryButton->SetStyle(ImmortalUITheme::ButtonStyle(bSelected));
	EntryButton->SetToolTipText(DisplayText);
	EntryButton->SetIsEnabled(bEnabled);
	EntryText->SetText(DisplayText);
	EntryText->SetColorAndOpacity(FSlateColor(DisplayColor));
	EntryIcon->SetBrush(ImmortalCraftingArt::EquipmentBrush(OwnerCrafting.IsValid() ? OwnerCrafting->GetEquipmentAtlas() : nullptr,
		DisplaySlot >= 0 ? static_cast<EImmortalEquipmentSlot>(DisplaySlot) : EImmortalEquipmentSlot::MAX));
}

void UImmortalCraftingEntryWidget::HandleClicked()
{
	if (!OwnerCrafting.IsValid()) return;
	if (bRecipeEntry) OwnerCrafting->SelectRecipe(RecipeId);
	else OwnerCrafting->SelectEquipment(ItemId);
}
