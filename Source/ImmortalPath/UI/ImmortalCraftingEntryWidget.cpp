// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalCraftingEntryWidget.h"

#include "ImmortalCraftingWidget.h"
#include "../Crafting/ImmortalCraftingTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

namespace
{
	FSlateBrush MakeCraftingEntryBrush(const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = FVector2D(250.0f, 44.0f);
		Brush.TintColor = FSlateColor(Tint);
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal")))
		{
			Brush.SetResourceObject(Texture);
		}
		return Brush;
	}
}

void UImmortalCraftingEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CraftingEntrySize"));
	Root->SetWidthOverride(250.0f);
	Root->SetHeightOverride(44.0f);
	WidgetTree->RootWidget = Root;
	EntryButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CraftingEntryButton"));
	EntryButton->OnClicked.AddDynamic(this, &UImmortalCraftingEntryWidget::HandleClicked);
	Root->AddChild(EntryButton);
	EntryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftingEntryText"));
	EntryText->SetJustification(ETextJustify::Center);
	EntryText->SetShadowOffset(FVector2D(1.0f));
	EntryText->SetShadowColorAndOpacity(FLinearColor::Black);
	FSlateFontInfo Font = EntryText->GetFont();
	Font.Size = 16;
	EntryText->SetFont(Font);
	EntryButton->AddChild(EntryText);
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
	bEnabled = bUnlocked;
	bSelected = bInSelected;
	FImmortalCraftingRecipeDefinition Definition;
	if (UImmortalCraftingLibrary::GetRecipeDefinition(RecipeId, Definition))
	{
		DisplayText = bUnlocked ? Definition.DisplayName : FText::FromString(FString::Printf(
			TEXT("未解锁 · 第 %d 关"), Definition.MinimumQingyunStage));
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
	DisplayText = FText::FromString(FString::Printf(TEXT("%s%s  强化 +%d"),
		bEquipped ? TEXT("[已装备] ") : TEXT(""),
		*UImmortalEquipmentLibrary::GetSlotText(Item.Slot).ToString(),
		Item.EnhancementLevel));
	DisplayColor = UImmortalEquipmentLibrary::GetQualityColor(Item.Quality);
	RefreshAppearance();
}

void UImmortalCraftingEntryWidget::RefreshAppearance()
{
	if (!EntryButton || !EntryText) return;
	const FLinearColor Tint = bSelected
		? FLinearColor(0.32f, 0.78f, 0.55f, 1.0f)
		: FLinearColor(0.38f, 0.42f, 0.5f, bEnabled ? 0.86f : 0.45f);
	const FSlateBrush Brush = MakeCraftingEntryBrush(Tint);
	FButtonStyle Style;
	Style.SetNormal(Brush);
	Style.SetHovered(Brush);
	Style.SetPressed(Brush);
	Style.SetDisabled(Brush);
	EntryButton->SetStyle(Style);
	EntryButton->SetIsEnabled(bEnabled);
	EntryText->SetText(DisplayText);
	EntryText->SetColorAndOpacity(FSlateColor(DisplayColor));
}

void UImmortalCraftingEntryWidget::HandleClicked()
{
	if (!OwnerCrafting.IsValid()) return;
	if (bRecipeEntry) OwnerCrafting->SelectRecipe(RecipeId);
	else OwnerCrafting->SelectEquipment(ItemId);
}

