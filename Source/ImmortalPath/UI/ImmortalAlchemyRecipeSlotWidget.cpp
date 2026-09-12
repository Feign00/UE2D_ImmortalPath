// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalAlchemyRecipeSlotWidget.h"

#include "ImmortalAlchemyWidget.h"
#include "ImmortalAlchemyArt.h"
#include "ImmortalFeaturePageLayout.h"
#include "ImmortalUITheme.h"
#include "../Alchemy/ImmortalAlchemyTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

void UImmortalAlchemyRecipeSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RecipeSlotSize"));
	Root->SetWidthOverride(264.0f);
	Root->SetHeightOverride(94.0f);
	WidgetTree->RootWidget = Root;
	Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RecipeButton"));
	Button->OnClicked.AddDynamic(this, &UImmortalAlchemyRecipeSlotWidget::HandleClicked);
	Root->AddChild(Button);
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RecipeRow"));
	Row->SetVisibility(ESlateVisibility::HitTestInvisible);
	Button->AddChild(Row);
	USizeBox* ArtSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RecipeArtSize"));
	ArtSize->SetWidthOverride(64);
	ArtSize->SetHeightOverride(64);
	RecipeArt = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RecipeArt"));
	ArtSize->AddChild(RecipeArt);
	Row->AddChildToHorizontalBox(ArtSize)->SetVerticalAlignment(VAlign_Center);
	Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RecipeLabel"));
	Label->SetJustification(ETextJustify::Left);
	Label->SetShadowOffset(FVector2D(1.0f, 1.0f));
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = 16;
	Label->SetFont(Font);
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	LabelSlot->SetPadding(FMargin(6, 0));
	Label->SetAutoWrapText(false);
	RefreshAppearance();
}

void UImmortalAlchemyRecipeSlotWidget::InitializeRecipe(
	UImmortalAlchemyWidget* InOwner,
	const FName InRecipeId,
	const bool bUnlocked,
	const bool bSelected,
	const float SuccessChanceBonus,
	const float ExceptionalChanceBonus)
{
	OwnerAlchemy = InOwner;
	RecipeId = InRecipeId;
	bRecipeUnlocked = bUnlocked;
	bRecipeSelected = bSelected;
	RecipeSuccessChanceBonus = FMath::IsFinite(SuccessChanceBonus) ? SuccessChanceBonus : 0.0f;
	RecipeExceptionalChanceBonus = FMath::IsFinite(ExceptionalChanceBonus) ? ExceptionalChanceBonus : 0.0f;
	RefreshAppearance();
}

void UImmortalAlchemyRecipeSlotWidget::RefreshAppearance()
{
	if (!Button || !Label) return;
	Button->SetStyle(ImmortalUITheme::ButtonStyle(bRecipeSelected));
	Button->SetIsEnabled(bRecipeUnlocked);
	RecipeArt->SetBrush(ImmortalAlchemyArt::Brush(OwnerAlchemy.IsValid() ? OwnerAlchemy->GetAlchemyAtlas() : nullptr, RecipeId));

	FImmortalPillDefinition Definition;
	if (UImmortalAlchemyLibrary::GetPillDefinition(RecipeId, Definition))
	{
		const float ActualSuccessChance = FMath::Clamp(
			Definition.BaseSuccessChance + RecipeSuccessChanceBonus, 0.0f, 1.0f);
		const float ActualExceptionalChance = FMath::Clamp(
			Definition.ExceptionalChance + RecipeExceptionalChanceBonus, 0.0f, ActualSuccessChance);
		Label->SetText(FText::FromString(FString::Printf(
			TEXT("%s\n成丹 %.0f%% · 极品 %.0f%%"),
			*Definition.DisplayName.ToString(),
			ActualSuccessChance * 100.0f,
			ActualExceptionalChance * 100.0f)));
		Label->SetColorAndOpacity(FSlateColor(bRecipeUnlocked
			? (bRecipeSelected ? FLinearColor(1.0f, 0.82f, 0.32f) : Definition.DisplayColor)
			: FLinearColor(0.55f, 0.55f, 0.58f)));
	}
}

void UImmortalAlchemyRecipeSlotWidget::HandleClicked()
{
	if (bRecipeUnlocked && OwnerAlchemy.IsValid()) OwnerAlchemy->SelectRecipe(RecipeId);
}
