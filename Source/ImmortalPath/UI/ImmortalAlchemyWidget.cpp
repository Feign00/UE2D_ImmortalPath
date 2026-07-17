// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalAlchemyWidget.h"

#include "ImmortalAlchemyRecipeSlotWidget.h"
#include "ImmortalPillSlotWidget.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

namespace
{
	FSlateBrush MakeAlchemyBrush(const TCHAR* AssetPath, const FVector2D Size, const FLinearColor Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(Tint);
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, AssetPath)) Brush.SetResourceObject(Texture);
		return Brush;
	}

	void SetAlchemyCanvasLayout(UCanvasPanelSlot* CanvasSlot, const FVector2D Position, const FVector2D Size)
	{
		if (!CanvasSlot) return;
		CanvasSlot->SetPosition(Position);
		CanvasSlot->SetSize(Size);
		CanvasSlot->SetAutoSize(false);
	}

	void StyleText(UTextBlock* Text, const int32 Size, const FLinearColor& Color)
	{
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor::Black);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}

	FButtonStyle MakeTextButtonStyle(const FVector2D Size, const FLinearColor& Tint)
	{
		const FSlateBrush Brush = MakeAlchemyBrush(
			TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal"), Size, Tint);
		FButtonStyle Style;
		Style.SetNormal(Brush);
		Style.SetHovered(Brush);
		Style.SetPressed(Brush);
		Style.SetDisabled(Brush);
		return Style;
	}
}

void UImmortalAlchemyWidget::InitializeForPlayer(AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalAlchemyWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AlchemyPanelSize"));
	Root->SetWidthOverride(900.0f);
	Root->SetHeightOverride(600.0f);
	WidgetTree->RootWidget = Root;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AlchemyCanvas"));
	Root->AddChild(Canvas);

	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("AlchemyBackground"));
	Background->SetBrush(MakeAlchemyBrush(
		TEXT("/Game/GAME/Asset/ui/inventory/panel_background.panel_background"), FVector2D(900.0f, 600.0f),
		FLinearColor(0.82f, 0.94f, 0.86f, 0.98f)));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(Background), FVector2D::ZeroVector, FVector2D(900.0f, 600.0f));

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AlchemyTitle"));
	Title->SetText(FText::FromString(TEXT("青云丹炉")));
	StyleText(Title, 28, FLinearColor(0.96f, 0.76f, 0.28f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(Title), FVector2D(34.0f, 20.0f), FVector2D(220.0f, 42.0f));

	BoostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AlchemyBoostText"));
	StyleText(BoostText, 16, FLinearColor(0.55f, 0.95f, 0.78f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(BoostText), FVector2D(520.0f, 27.0f), FVector2D(280.0f, 30.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("AlchemyCloseButton"));
	CloseButton->OnClicked.AddDynamic(this, &UImmortalAlchemyWidget::HandleCloseClicked);
	CloseButton->SetStyle(MakeTextButtonStyle(FVector2D(64.0f), FLinearColor::White));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(CloseButton), FVector2D(816.0f, 12.0f), FVector2D(64.0f));
	UTextBlock* CloseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AlchemyCloseText"));
	CloseText->SetText(FText::FromString(TEXT("×")));
	CloseText->SetJustification(ETextJustify::Center);
	StyleText(CloseText, 28, FLinearColor(1.0f, 0.75f, 0.35f));
	CloseButton->AddChild(CloseText);

	UTextBlock* RecipeTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RecipeListTitle"));
	RecipeTitle->SetText(FText::FromString(TEXT("丹方")));
	StyleText(RecipeTitle, 20, FLinearColor::White);
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(RecipeTitle), FVector2D(32.0f, 77.0f), FVector2D(100.0f, 30.0f));
	UScrollBox* RecipeScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("RecipeScroll"));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(RecipeScroll), FVector2D(28.0f, 110.0f), FVector2D(240.0f, 430.0f));
	RecipeList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RecipeList"));
	RecipeScroll->AddChild(RecipeList);

	RecipeNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RecipeName"));
	StyleText(RecipeNameText, 23, FLinearColor(0.45f, 1.0f, 0.7f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(RecipeNameText), FVector2D(292.0f, 80.0f), FVector2D(290.0f, 34.0f));
	RecipeDescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RecipeDescription"));
	RecipeDescriptionText->SetAutoWrapText(true);
	StyleText(RecipeDescriptionText, 15, FLinearColor(0.84f, 0.86f, 0.9f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(RecipeDescriptionText), FVector2D(292.0f, 118.0f), FVector2D(290.0f, 72.0f));
	IngredientText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("IngredientText"));
	IngredientText->SetAutoWrapText(true);
	StyleText(IngredientText, 16, FLinearColor(0.88f, 0.9f, 0.92f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(IngredientText), FVector2D(292.0f, 198.0f), FVector2D(290.0f, 120.0f));
	ChanceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ChanceText"));
	StyleText(ChanceText, 16, FLinearColor(1.0f, 0.75f, 0.3f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(ChanceText), FVector2D(292.0f, 322.0f), FVector2D(290.0f, 50.0f));
	RecipeEffectText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RecipeEffectText"));
	RecipeEffectText->SetAutoWrapText(true);
	StyleText(RecipeEffectText, 14, FLinearColor(0.58f, 0.94f, 0.76f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(RecipeEffectText), FVector2D(292.0f, 376.0f), FVector2D(290.0f, 78.0f));

	CraftButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CraftButton"));
	CraftButton->OnClicked.AddDynamic(this, &UImmortalAlchemyWidget::HandleCraftClicked);
	CraftButton->SetStyle(MakeTextButtonStyle(FVector2D(210.0f, 54.0f), FLinearColor(0.28f, 0.78f, 0.48f, 1.0f)));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(CraftButton), FVector2D(330.0f, 462.0f), FVector2D(210.0f, 54.0f));
	CraftButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftButtonText"));
	CraftButtonText->SetText(FText::FromString(TEXT("炼制一炉")));
	CraftButtonText->SetJustification(ETextJustify::Center);
	StyleText(CraftButtonText, 20, FLinearColor::White);
	CraftButton->AddChild(CraftButtonText);

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AlchemyResult"));
	ResultText->SetAutoWrapText(true);
	ResultText->SetJustification(ETextJustify::Center);
	StyleText(ResultText, 17, FLinearColor(1.0f, 0.82f, 0.32f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(ResultText), FVector2D(282.0f, 525.0f), FVector2D(310.0f, 54.0f));

	UTextBlock* PillTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PillInventoryTitle"));
	PillTitle->SetText(FText::FromString(TEXT("丹药背包")));
	StyleText(PillTitle, 20, FLinearColor::White);
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(PillTitle), FVector2D(620.0f, 77.0f), FVector2D(160.0f, 30.0f));
	PillGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("PillGrid"));
	PillGrid->SetSlotPadding(FMargin(3.0f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(PillGrid), FVector2D(616.0f, 112.0f), FVector2D(210.0f, 205.0f));
	PillNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SelectedPillName"));
	StyleText(PillNameText, 19, FLinearColor(0.45f, 1.0f, 0.7f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(PillNameText), FVector2D(620.0f, 330.0f), FVector2D(220.0f, 30.0f));
	PillEffectText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SelectedPillEffect"));
	PillEffectText->SetAutoWrapText(true);
	StyleText(PillEffectText, 15, FLinearColor(0.86f, 0.88f, 0.92f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(PillEffectText), FVector2D(620.0f, 366.0f), FVector2D(230.0f, 105.0f));
	UseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("UsePillButton"));
	UseButton->OnClicked.AddDynamic(this, &UImmortalAlchemyWidget::HandleUseClicked);
	UseButton->SetStyle(MakeTextButtonStyle(FVector2D(190.0f, 50.0f), FLinearColor(0.46f, 0.3f, 0.78f, 1.0f)));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(UseButton), FVector2D(636.0f, 480.0f), FVector2D(190.0f, 50.0f));
	UTextBlock* UseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UsePillText"));
	UseText->SetText(FText::FromString(TEXT("服用丹药")));
	UseText->SetJustification(ETextJustify::Center);
	StyleText(UseText, 19, FLinearColor::White);
	UseButton->AddChild(UseText);

	RefreshFromPlayer();
}

void UImmortalAlchemyWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Player.IsValid()) return;
	const int32 RealmIndex = static_cast<int32>(Player->GetCultivationRealm());
	const int32 MinorStage = Player->GetCultivationMinorStage();
	if (LastMaterialRevision != Player->GetMaterialInventoryRevision()
		|| LastPillRevision != Player->GetPillInventoryRevision()
		|| LastRealmIndex != RealmIndex
		|| LastMinorStage != MinorStage)
	{
		RefreshFromPlayer();
	}
	if (BoostText)
	{
		const float Remaining = Player->GetAlchemyBoostRemainingSeconds();
		BoostText->SetText(Remaining > 0.0f
			? FText::FromString(FString::Printf(TEXT("悟道中：修炼 ×%.1f  剩余 %.0f 秒"), Player->GetAlchemyBoostMultiplier(), Remaining))
			: FText::FromString(TEXT("悟道增益：未激活")));
	}
}

void UImmortalAlchemyWidget::RefreshFromPlayer()
{
	if (!Player.IsValid() || !RecipeList || !PillGrid) return;
	LastMaterialRevision = Player->GetMaterialInventoryRevision();
	LastPillRevision = Player->GetPillInventoryRevision();
	LastRealmIndex = static_cast<int32>(Player->GetCultivationRealm());
	LastMinorStage = Player->GetCultivationMinorStage();

	TArray<FName> RecipeIds = UImmortalAlchemyLibrary::GetKnownRecipeIds();
	RecipeIds.Sort([](const FName Left, const FName Right)
	{
		FImmortalPillDefinition A, B;
		UImmortalAlchemyLibrary::GetPillDefinition(Left, A);
		UImmortalAlchemyLibrary::GetPillDefinition(Right, B);
		if (A.MinimumRealmIndex != B.MinimumRealmIndex) return A.MinimumRealmIndex < B.MinimumRealmIndex;
		if (A.MinimumMinorStage != B.MinimumMinorStage) return A.MinimumMinorStage < B.MinimumMinorStage;
		return A.DisplayName.ToString() < B.DisplayName.ToString();
	});
	if (SelectedRecipeId.IsNone() || !RecipeIds.Contains(SelectedRecipeId))
	{
		for (const FName RecipeId : RecipeIds)
		{
			if (Player->IsAlchemyRecipeUnlocked(RecipeId))
			{
				SelectedRecipeId = RecipeId;
				break;
			}
		}
	}

	const TArray<FImmortalPillStack> Pills = Player->GetPillInventory();
	if (!SelectedPillId.IsNone()
		&& UImmortalAlchemyLibrary::GetPillQuantity(Pills, SelectedPillId, SelectedPillQuality) <= 0)
	{
		SelectedPillId = NAME_None;
	}
	if (SelectedPillId.IsNone() && !Pills.IsEmpty())
	{
		SelectedPillId = Pills[0].PillId;
		SelectedPillQuality = Pills[0].Quality;
	}
	RebuildRecipes();
	RebuildPills();
	RefreshRecipeDetails();
	RefreshPillDetails();
}

void UImmortalAlchemyWidget::RebuildRecipes()
{
	RecipeList->ClearChildren();
	TArray<FName> RecipeIds = UImmortalAlchemyLibrary::GetKnownRecipeIds();
	RecipeIds.Sort([this](const FName Left, const FName Right)
	{
		FImmortalPillDefinition A, B;
		UImmortalAlchemyLibrary::GetPillDefinition(Left, A);
		UImmortalAlchemyLibrary::GetPillDefinition(Right, B);
		if (A.MinimumRealmIndex != B.MinimumRealmIndex) return A.MinimumRealmIndex < B.MinimumRealmIndex;
		return A.MinimumMinorStage < B.MinimumMinorStage;
	});
	for (const FName RecipeId : RecipeIds)
	{
		UImmortalAlchemyRecipeSlotWidget* RecipeSlotWidget = CreateWidget<UImmortalAlchemyRecipeSlotWidget>(
			GetOwningPlayer(), UImmortalAlchemyRecipeSlotWidget::StaticClass());
		RecipeSlotWidget->InitializeRecipe(this, RecipeId, Player->IsAlchemyRecipeUnlocked(RecipeId), RecipeId == SelectedRecipeId);
		RecipeList->AddChild(RecipeSlotWidget);
	}
}

void UImmortalAlchemyWidget::RebuildPills()
{
	PillGrid->ClearChildren();
	const TArray<FImmortalPillStack> Pills = Player->GetPillInventory();
	const int32 DisplaySlots = FMath::Max(Pills.Num(), 4);
	for (int32 Index = 0; Index < DisplaySlots; ++Index)
	{
		const FImmortalPillStack Stack = Pills.IsValidIndex(Index) ? Pills[Index] : FImmortalPillStack();
		UImmortalPillSlotWidget* PillSlotWidget = CreateWidget<UImmortalPillSlotWidget>(GetOwningPlayer(), UImmortalPillSlotWidget::StaticClass());
		PillSlotWidget->InitializePill(this, Stack, Stack.IsValid() && Stack.PillId == SelectedPillId && Stack.Quality == SelectedPillQuality);
		PillGrid->AddChildToUniformGrid(PillSlotWidget, Index / 2, Index % 2);
	}
}

void UImmortalAlchemyWidget::RefreshRecipeDetails()
{
	FImmortalPillDefinition Definition;
	if (!UImmortalAlchemyLibrary::GetPillDefinition(SelectedRecipeId, Definition)) return;
	RecipeNameText->SetText(Definition.DisplayName);
	RecipeNameText->SetColorAndOpacity(FSlateColor(Definition.DisplayColor));
	RecipeDescriptionText->SetText(Definition.Description);
	FString Ingredients = TEXT("所需材料：\n");
	for (const FImmortalAlchemyIngredient& Cost : Definition.Ingredients)
	{
		FImmortalMaterialDefinition Material;
		UImmortalMaterialLibrary::GetMaterialDefinition(Cost.MaterialId, Material);
		const int32 Owned = Player->GetMaterialQuantity(Cost.MaterialId);
		Ingredients += FString::Printf(TEXT("%s %s  %d / %d\n"),
			Owned >= Cost.Quantity ? TEXT("✓") : TEXT("✗"), *Material.DisplayName.ToString(), Owned, Cost.Quantity);
	}
	IngredientText->SetText(FText::FromString(Ingredients));
	ChanceText->SetText(FText::FromString(FString::Printf(
		TEXT("成丹率 %.0f%%    极品率 %.0f%%"), Definition.BaseSuccessChance * 100.0f, Definition.ExceptionalChance * 100.0f)));
	RecipeEffectText->SetText(FText::FromString(FString::Printf(
		TEXT("普通：%s\n极品：%s"),
		*UImmortalAlchemyLibrary::GetEffectText(Definition, EImmortalPillQuality::Ordinary).ToString(),
		*UImmortalAlchemyLibrary::GetEffectText(Definition, EImmortalPillQuality::Exceptional).ToString())));
	const bool bUnlocked = Player->IsAlchemyRecipeUnlocked(SelectedRecipeId);
	CraftButton->SetIsEnabled(bUnlocked && Player->CanCraftPill(SelectedRecipeId));
	CraftButtonText->SetText(FText::FromString(!bUnlocked ? TEXT("境界未解锁")
		: (Player->CanCraftPill(SelectedRecipeId) ? TEXT("炼制一炉") : TEXT("材料不足"))));
}

void UImmortalAlchemyWidget::RefreshPillDetails()
{
	FImmortalPillDefinition Definition;
	const int32 Quantity = Player->GetPillQuantity(SelectedPillId, SelectedPillQuality);
	if (Quantity <= 0 || !UImmortalAlchemyLibrary::GetPillDefinition(SelectedPillId, Definition))
	{
		PillNameText->SetText(FText::FromString(TEXT("尚无丹药")));
		PillNameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		PillEffectText->SetText(FText::FromString(TEXT("炼制成功后，丹药会按品质自动堆叠到这里。")));
		UseButton->SetIsEnabled(false);
		return;
	}
	PillNameText->SetText(FText::FromString(FString::Printf(TEXT("%s·%s ×%d"),
		*UImmortalAlchemyLibrary::GetQualityText(SelectedPillQuality).ToString(),
		*Definition.DisplayName.ToString(), Quantity)));
	PillNameText->SetColorAndOpacity(FSlateColor(UImmortalAlchemyLibrary::GetQualityColor(SelectedPillQuality)));
	PillEffectText->SetText(FText::FromString(FString::Printf(TEXT("%s\n\n%s"),
		*Definition.Description.ToString(),
		*UImmortalAlchemyLibrary::GetEffectText(Definition, SelectedPillQuality).ToString())));
	UseButton->SetIsEnabled(Player->CanUsePill(SelectedPillId, SelectedPillQuality));
}

void UImmortalAlchemyWidget::SelectRecipe(const FName RecipeId)
{
	SelectedRecipeId = RecipeId;
	RefreshFromPlayer();
}

void UImmortalAlchemyWidget::SelectPill(const FName PillId, const EImmortalPillQuality Quality)
{
	SelectedPillId = PillId;
	SelectedPillQuality = Quality;
	RefreshFromPlayer();
}

void UImmortalAlchemyWidget::HandleCraftClicked()
{
	if (!Player.IsValid()) return;
	const FImmortalAlchemyCraftResult Result = Player->CraftPill(SelectedRecipeId);
	ResultText->SetText(Result.Message);
	ResultText->SetColorAndOpacity(FSlateColor(Result.Outcome == EImmortalAlchemyOutcome::Failure
		? FLinearColor(1.0f, 0.35f, 0.25f)
		: (Result.Outcome == EImmortalAlchemyOutcome::Exceptional
			? FLinearColor(1.0f, 0.62f, 0.16f)
			: FLinearColor(0.38f, 1.0f, 0.68f))));
	if (Result.PillQuantityGranted > 0)
	{
		SelectedPillId = SelectedRecipeId;
		SelectedPillQuality = Result.Outcome == EImmortalAlchemyOutcome::Exceptional
			? EImmortalPillQuality::Exceptional : EImmortalPillQuality::Ordinary;
	}
	RefreshFromPlayer();
}

void UImmortalAlchemyWidget::HandleUseClicked()
{
	if (!Player.IsValid()) return;
	FImmortalPillDefinition Definition;
	UImmortalAlchemyLibrary::GetPillDefinition(SelectedPillId, Definition);
	const bool bUsed = Player->UsePill(SelectedPillId, SelectedPillQuality);
	ResultText->SetText(bUsed
		? FText::FromString(FString::Printf(TEXT("已服用：%s·%s"),
			*UImmortalAlchemyLibrary::GetQualityText(SelectedPillQuality).ToString(), *Definition.DisplayName.ToString()))
		: FText::FromString(TEXT("当前状态无法服用此丹药")));
	ResultText->SetColorAndOpacity(FSlateColor(bUsed
		? FLinearColor(0.38f, 1.0f, 0.68f)
		: FLinearColor(1.0f, 0.4f, 0.3f)));
	RefreshFromPlayer();
}

void UImmortalAlchemyWidget::HandleCloseClicked()
{
	if (Player.IsValid()) Player->ToggleAlchemy();
}
