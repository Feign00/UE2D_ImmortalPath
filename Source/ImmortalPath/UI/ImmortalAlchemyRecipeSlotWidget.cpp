// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalAlchemyRecipeSlotWidget.h"

#include "ImmortalAlchemyWidget.h"
#include "ImmortalFeaturePageLayout.h"
#include "ImmortalUITheme.h"
#include "../Alchemy/ImmortalAlchemyTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

void UImmortalAlchemyRecipeSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RecipeSlotSize"));
	Root->SetWidthOverride(230.0f);
	Root->SetHeightOverride(68.0f);
	WidgetTree->RootWidget = Root;
	Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RecipeButton"));
	Button->OnClicked.AddDynamic(this, &UImmortalAlchemyRecipeSlotWidget::HandleClicked);
	Root->AddChild(Button);
	Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RecipeLabel"));
	Label->SetJustification(ETextJustify::Center);
	Label->SetShadowOffset(FVector2D(1.0f, 1.0f));
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = 16;
	Label->SetFont(Font);
	Button->AddChild(Label);
	ImmortalFeaturePageLayout::StabilizeButtonLabel(Button, Label);
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

	FImmortalPillDefinition Definition;
	if (UImmortalAlchemyLibrary::GetPillDefinition(RecipeId, Definition))
	{
		const float ActualSuccessChance = FMath::Clamp(
			Definition.BaseSuccessChance + RecipeSuccessChanceBonus, 0.0f, 1.0f);
		const float ActualExceptionalChance = FMath::Clamp(
			Definition.ExceptionalChance + RecipeExceptionalChanceBonus, 0.0f, ActualSuccessChance);
		Label->SetText(FText::FromString(FString::Printf(
			TEXT("%s\n成丹 %.0f%%  极品 %.0f%%"),
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
