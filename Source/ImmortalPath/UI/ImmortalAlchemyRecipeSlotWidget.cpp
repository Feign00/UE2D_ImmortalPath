// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalAlchemyRecipeSlotWidget.h"

#include "ImmortalAlchemyWidget.h"
#include "../Alchemy/ImmortalAlchemyTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

namespace
{
	FSlateBrush MakeRecipeBrush(const TCHAR* AssetPath, const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = FVector2D(230.0f, 68.0f);
		Brush.TintColor = FSlateColor(Tint);
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, AssetPath)) Brush.SetResourceObject(Texture);
		return Brush;
	}
}

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
	Label->SetAutoWrapText(true);
	Label->SetShadowOffset(FVector2D(1.0f, 1.0f));
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = 16;
	Label->SetFont(Font);
	Button->AddChild(Label);
	RefreshAppearance();
}

void UImmortalAlchemyRecipeSlotWidget::InitializeRecipe(
	UImmortalAlchemyWidget* InOwner,
	const FName InRecipeId,
	const bool bUnlocked,
	const bool bSelected)
{
	OwnerAlchemy = InOwner;
	RecipeId = InRecipeId;
	bRecipeUnlocked = bUnlocked;
	bRecipeSelected = bSelected;
	RefreshAppearance();
}

void UImmortalAlchemyRecipeSlotWidget::RefreshAppearance()
{
	if (!Button || !Label) return;
	const TCHAR* StatePath = bRecipeSelected
		? TEXT("/Game/GAME/Asset/ui/inventory/slots/selected.selected")
		: TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal");
	const FLinearColor Tint = bRecipeUnlocked ? FLinearColor::White : FLinearColor(0.45f, 0.45f, 0.48f, 0.8f);
	const FSlateBrush Brush = MakeRecipeBrush(StatePath, Tint);
	FButtonStyle Style;
	Style.SetNormal(Brush);
	Style.SetHovered(Brush);
	Style.SetPressed(Brush);
	Style.SetDisabled(Brush);
	Button->SetStyle(Style);
	Button->SetIsEnabled(bRecipeUnlocked);

	FImmortalPillDefinition Definition;
	if (UImmortalAlchemyLibrary::GetPillDefinition(RecipeId, Definition))
	{
		Label->SetText(FText::FromString(FString::Printf(
			TEXT("%s\n成功 %.0f%%  %s"),
			*Definition.DisplayName.ToString(),
			Definition.BaseSuccessChance * 100.0f,
			bRecipeUnlocked ? TEXT("可炼制") : TEXT("未解锁"))));
		Label->SetColorAndOpacity(FSlateColor(bRecipeUnlocked
			? (bRecipeSelected ? FLinearColor(1.0f, 0.82f, 0.32f) : Definition.DisplayColor)
			: FLinearColor(0.55f, 0.55f, 0.58f)));
	}
}

void UImmortalAlchemyRecipeSlotWidget::HandleClicked()
{
	if (bRecipeUnlocked && OwnerAlchemy.IsValid()) OwnerAlchemy->SelectRecipe(RecipeId);
}

