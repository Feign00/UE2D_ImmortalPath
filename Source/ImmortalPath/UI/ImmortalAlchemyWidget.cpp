// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalAlchemyWidget.h"
#include "ImmortalAlchemyArt.h"
#include "ImmortalFeaturePageLayout.h"
#include "ImmortalUITheme.h"

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
		return ImmortalUITheme::ButtonStyle();
	}
}

UTexture2D* UImmortalAlchemyWidget::GetAlchemyAtlas() const
{
	return AlchemyAtlas.LoadSynchronous();
}

UTexture2D* UImmortalAlchemyWidget::GetMaterialAtlas() const
{
	return MaterialAtlas.LoadSynchronous();
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
	Root->SetWidthOverride(1600.0f);
	Root->SetHeightOverride(600.0f);
	WidgetTree->RootWidget = Root;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AlchemyCanvas"));
	Root->AddChild(Canvas);

	ImmortalFeaturePageLayout::AddReadabilityBackground(WidgetTree, Canvas);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const float X[] = {8, 316, 1032};
		const float Width[] = {300, 708, 560};
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*FString::Printf(TEXT("AlchemySection%d"), Index)));
		Card->SetBrush(ImmortalUITheme::PanelBrush(FLinearColor(0.035f, 0.055f, 0.058f)));
		Card->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(Card), FVector2D(X[Index], 52), FVector2D(Width[Index], 536));
	}
	UImage* FurnaceArt = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("AlchemyFurnaceArt"));
	FurnaceArt->SetBrush(ImmortalAlchemyArt::Brush(GetAlchemyAtlas(), TEXT("Cauldron")));
	FurnaceArt->SetVisibility(ESlateVisibility::HitTestInvisible);
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(FurnaceArt), FVector2D(812, 92), FVector2D(190));

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AlchemyTitle"));
	Title->SetText(FText::FromString(TEXT("青云丹炉")));
	StyleText(Title, 28, FLinearColor(0.96f, 0.76f, 0.28f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(Title), FVector2D(22.0f, 6.0f), FVector2D(260.0f, 36.0f));

	BoostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AlchemyBoostText"));
	StyleText(BoostText, 18, FLinearColor(0.55f, 0.95f, 0.78f));
	BoostText->SetJustification(ETextJustify::Right);
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(BoostText), FVector2D(950.0f, 8.0f), FVector2D(570.0f, 30.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("AlchemyCloseButton"));
	CloseButton->OnClicked.AddDynamic(this, &UImmortalAlchemyWidget::HandleCloseClicked);
	CloseButton->SetStyle(MakeTextButtonStyle(FVector2D(44.0f, 32.0f), FLinearColor::White));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(CloseButton), FVector2D(1538.0f, 5.0f), FVector2D(44.0f, 32.0f));
	UTextBlock* CloseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AlchemyCloseText"));
	CloseText->SetText(FText::FromString(TEXT("×")));
	CloseText->SetJustification(ETextJustify::Center);
	StyleText(CloseText, 22, FLinearColor(1.0f, 0.75f, 0.35f));
	CloseButton->AddChild(CloseText);

	UTextBlock* RecipeTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RecipeListTitle"));
	RecipeTitle->SetText(FText::FromString(TEXT("丹方")));
	StyleText(RecipeTitle, 20, FLinearColor::White);
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(RecipeTitle), FVector2D(20.0f, 64.0f), FVector2D(260.0f, 28.0f));
	UScrollBox* RecipeScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("RecipeScroll"));
	ImmortalFeaturePageLayout::StyleScrollBox(RecipeScroll);
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(RecipeScroll), FVector2D(18.0f, 98.0f), FVector2D(278.0f, 478.0f));
	RecipeList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RecipeList"));
	RecipeScroll->AddChild(RecipeList);

	RecipeNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RecipeName"));
	StyleText(RecipeNameText, 23, FLinearColor(0.45f, 1.0f, 0.7f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(RecipeNameText), FVector2D(336.0f, 70.0f), FVector2D(450.0f, 34.0f));
	RecipeDescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RecipeDescription"));
	RecipeDescriptionText->SetAutoWrapText(true);
	StyleText(RecipeDescriptionText, 18, FLinearColor(0.84f, 0.86f, 0.9f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(RecipeDescriptionText), FVector2D(336.0f, 114.0f), FVector2D(440.0f, 68.0f));
	IngredientText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("IngredientText"));
	IngredientText->SetAutoWrapText(true);
	StyleText(IngredientText, 20, FLinearColor(0.88f, 0.9f, 0.92f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(IngredientText), FVector2D(336.0f, 198.0f), FVector2D(440.0f, 30.0f));
	UScrollBox* IngredientScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("IngredientScroll"));
	ImmortalFeaturePageLayout::StyleScrollBox(IngredientScroll);
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(IngredientScroll), FVector2D(336, 230), FVector2D(440, 156));
	IngredientList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("IngredientList"));
	IngredientScroll->AddChild(IngredientList);
	ChanceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ChanceText"));
	ChanceText->SetToolTipText(FText::FromString(TEXT("当前概率已计入丹房加成")));
	StyleText(ChanceText, 16, FLinearColor(1.0f, 0.75f, 0.3f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(ChanceText), FVector2D(800.0f, 290.0f), FVector2D(208.0f, 36.0f));
	RecipeEffectText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RecipeEffectText"));
	RecipeEffectText->SetAutoWrapText(true);
	StyleText(RecipeEffectText, 18, FLinearColor(0.58f, 0.94f, 0.76f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(RecipeEffectText), FVector2D(336.0f, 400.0f), FVector2D(650.0f, 78.0f));

	CraftButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CraftButton"));
	CraftButton->OnClicked.AddDynamic(this, &UImmortalAlchemyWidget::HandleCraftClicked);
	CraftButton->SetStyle(MakeTextButtonStyle(FVector2D(190.0f, 44.0f), FLinearColor(0.28f, 0.78f, 0.48f, 1.0f)));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(CraftButton), FVector2D(800.0f, 338.0f), FVector2D(208.0f, 48.0f));
	CraftButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftButtonText"));
	CraftButtonText->SetText(FText::FromString(TEXT("炼制一炉")));
	CraftButtonText->SetJustification(ETextJustify::Center);
	StyleText(CraftButtonText, 18, FLinearColor::White);
	CraftButton->AddChild(CraftButtonText);

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AlchemyResult"));
	ResultText->SetAutoWrapText(true);
	ResultText->SetJustification(ETextJustify::Center);
	StyleText(ResultText, 17, FLinearColor(1.0f, 0.82f, 0.32f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(ResultText), FVector2D(336.0f, 492.0f), FVector2D(668.0f, 80.0f));

	UTextBlock* PillTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PillInventoryTitle"));
	PillTitle->SetText(FText::FromString(TEXT("丹药背包")));
	StyleText(PillTitle, 20, FLinearColor::White);
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(PillTitle), FVector2D(1048.0f, 64.0f), FVector2D(510.0f, 28.0f));
	PillGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("PillGrid"));
	PillGrid->SetSlotPadding(FMargin(3.0f));
	UScrollBox* PillScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("PillInventoryScroll"));
	ImmortalFeaturePageLayout::StyleScrollBox(PillScroll);
	PillScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	PillScroll->SetClipping(EWidgetClipping::ClipToBounds);
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(PillScroll), FVector2D(1048.0f, 98.0f), FVector2D(524.0f, 210.0f));
	PillScroll->AddChild(PillGrid);
	PillNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SelectedPillName"));
	StyleText(PillNameText, 19, FLinearColor(0.45f, 1.0f, 0.7f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(PillNameText), FVector2D(1060.0f, 326.0f), FVector2D(504.0f, 48.0f));
	PillEffectText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SelectedPillEffect"));
	PillEffectText->SetAutoWrapText(true);
	StyleText(PillEffectText, 18, FLinearColor(0.86f, 0.88f, 0.92f));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(PillEffectText), FVector2D(1060.0f, 382.0f), FVector2D(504.0f, 126.0f));
	UseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("UsePillButton"));
	UseButton->OnClicked.AddDynamic(this, &UImmortalAlchemyWidget::HandleUseClicked);
	UseButton->SetStyle(MakeTextButtonStyle(FVector2D(190.0f, 44.0f), FLinearColor(0.46f, 0.3f, 0.78f, 1.0f)));
	SetAlchemyCanvasLayout(Canvas->AddChildToCanvas(UseButton), FVector2D(1060.0f, 526.0f), FVector2D(504.0f, 46.0f));
	UTextBlock* UseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UsePillText"));
	UseText->SetText(FText::FromString(TEXT("服用丹药")));
	UseText->SetJustification(ETextJustify::Center);
	StyleText(UseText, 18, FLinearColor::White);
	UseButton->AddChild(UseText);

	RefreshFromPlayer();
	for (UTextBlock* Detail : {RecipeDescriptionText.Get(), RecipeEffectText.Get(),
		ResultText.Get(), PillEffectText.Get(), PillNameText.Get()})
		ImmortalFeaturePageLayout::MakeScrollable(WidgetTree, Detail);
	ImmortalFeaturePageLayout::StabilizeButtonLabel(CraftButton, CraftButtonText);
	ImmortalFeaturePageLayout::StabilizeButtonLabel(UseButton, UseText);
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
		|| LastMinorStage != MinorStage
		|| LastCaveRevision != Player->GetCaveRevision())
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
	LastCaveRevision = Player->GetCaveRevision();

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
		RecipeSlotWidget->InitializeRecipe(
			this,
			RecipeId,
			Player->IsAlchemyRecipeUnlocked(RecipeId),
			RecipeId == SelectedRecipeId,
			Player->GetCaveAlchemySuccessBonus(),
			Player->GetCaveAlchemyExceptionalBonus());
		RecipeList->AddChild(RecipeSlotWidget);
	}
	// Refresh may run after Slate's normal prepass (e.g. an inventory revision in Tick).
	RecipeList->ForceLayoutPrepass();
}

void UImmortalAlchemyWidget::RebuildPills()
{
	PillGrid->ClearChildren();
	const TArray<FImmortalPillStack> Pills = Player->GetPillInventory();
	const int32 DisplaySlots = FMath::Max(Pills.Num(), 5);
	for (int32 Index = 0; Index < DisplaySlots; ++Index)
	{
		const FImmortalPillStack Stack = Pills.IsValidIndex(Index) ? Pills[Index] : FImmortalPillStack();
		UImmortalPillSlotWidget* PillSlotWidget = CreateWidget<UImmortalPillSlotWidget>(GetOwningPlayer(), UImmortalPillSlotWidget::StaticClass());
		PillSlotWidget->InitializePill(this, Stack, Stack.IsValid() && Stack.PillId == SelectedPillId && Stack.Quality == SelectedPillQuality);
		PillGrid->AddChildToUniformGrid(PillSlotWidget, Index / 5, Index % 5);
	}
	PillGrid->ForceLayoutPrepass();
}

void UImmortalAlchemyWidget::RefreshRecipeDetails()
{
	FImmortalPillDefinition Definition;
	if (!UImmortalAlchemyLibrary::GetPillDefinition(SelectedRecipeId, Definition)) return;
	RecipeNameText->SetText(Definition.DisplayName);
	RecipeNameText->SetColorAndOpacity(FSlateColor(Definition.DisplayColor));
	RecipeDescriptionText->SetText(Definition.Description);
	IngredientText->SetText(FText::FromString(TEXT("所需材料 · 持有 / 消耗")));
	IngredientList->ClearChildren();
	for (const FImmortalAlchemyIngredient& Cost : Definition.Ingredients)
	{
		FImmortalMaterialDefinition Material;
		UImmortalMaterialLibrary::GetMaterialDefinition(Cost.MaterialId, Material);
		const int32 Owned = Player->GetMaterialQuantity(Cost.MaterialId);
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
		IconSize->SetWidthOverride(38);
		IconSize->SetHeightOverride(38);
		UImage* Icon = WidgetTree->ConstructWidget<UImage>();
		Icon->SetBrush(ImmortalAlchemyArt::MaterialBrush(GetMaterialAtlas(), Cost.MaterialId));
		IconSize->AddChild(Icon);
		Row->AddChildToHorizontalBox(IconSize);
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(FString::Printf(TEXT("%s  %d / %d  %s"),
			*Material.DisplayName.ToString(), Owned, Cost.Quantity, Owned >= Cost.Quantity ? TEXT("充足") : TEXT("不足"))));
		StyleText(Label, 18, Owned >= Cost.Quantity ? FLinearColor(0.60f, 0.92f, 0.73f) : FLinearColor(1, 0.47f, 0.35f));
		UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetPadding(FMargin(8, 0));
		IngredientList->AddChild(Row);
	}
	IngredientList->ForceLayoutPrepass();
	const float ActualSuccessChance = FMath::Clamp(
		Definition.BaseSuccessChance + Player->GetCaveAlchemySuccessBonus(), 0.0f, 1.0f);
	const float ActualExceptionalChance = FMath::Clamp(
		Definition.ExceptionalChance + Player->GetCaveAlchemyExceptionalBonus(), 0.0f, ActualSuccessChance);
	ChanceText->SetText(FText::FromString(FString::Printf(
		TEXT("成丹 %.0f%%  ·  极品 %.0f%%"),
		ActualSuccessChance * 100.0f,
		ActualExceptionalChance * 100.0f)));
	RecipeEffectText->SetText(FText::FromString(FString::Printf(
		TEXT("普通：%s\n极品：%s"),
		*Player->GetEffectivePillEffectText(SelectedRecipeId, EImmortalPillQuality::Ordinary).ToString(),
		*Player->GetEffectivePillEffectText(SelectedRecipeId, EImmortalPillQuality::Exceptional).ToString())));
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
		*Player->GetEffectivePillEffectText(SelectedPillId, SelectedPillQuality).ToString())));
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
