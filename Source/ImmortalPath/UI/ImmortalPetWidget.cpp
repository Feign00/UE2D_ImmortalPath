// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPetWidget.h"

#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "HAL/PlatformTime.h"
#include "Styling/SlateTypes.h"

namespace
{
	void SetPetLayout(
		UCanvasPanelSlot* Slot,
		const FVector2D Position,
		const FVector2D Size)
	{
		if (!Slot) return;
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetAutoSize(false);
	}

	void StylePetText(
		UTextBlock* Text,
		const int32 Size,
		const FLinearColor& Color,
		const bool bCentered = false)
	{
		if (!Text) return;
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor::Black);
		Text->SetJustification(
			bCentered ? ETextJustify::Center : ETextJustify::Left);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}

	FSlateBrush MakePetBrush(
		const FVector2D Size,
		const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	FButtonStyle MakePetButtonStyle(
		const FVector2D Size,
		const FLinearColor& Tint)
	{
		FButtonStyle Style;
		Style.SetNormal(MakePetBrush(Size, Tint));
		Style.SetHovered(
			MakePetBrush(Size, (Tint * 1.16f).GetClamped()));
		Style.SetPressed(
			MakePetBrush(Size, (Tint * 0.76f).GetClamped()));
		Style.SetDisabled(MakePetBrush(
			Size, FLinearColor(0.065f, 0.070f, 0.080f, 0.96f)));
		return Style;
	}

	UTextBlock* AddPetButtonLabel(
		UWidgetTree* Tree,
		UButton* Button,
		const TCHAR* Label,
		const int32 FontSize = 14)
	{
		if (!Tree || !Button) return nullptr;
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		Text->SetAutoWrapText(true);
		StylePetText(
			Text,
			FontSize,
			FLinearColor(0.94f, 1.0f, 0.88f, 1.0f),
			true);
		Button->AddChild(Text);
		return Text;
	}

	FString GetPetAttackStyleText(
		const EImmortalPetAttackStyle AttackStyle)
	{
		return AttackStyle == EImmortalPetAttackStyle::Ranged
			? TEXT("远程灵宠")
			: TEXT("近战灵宠");
	}

	FString GetMaterialName(const FName MaterialId)
	{
		FImmortalMaterialDefinition Definition;
		return UImmortalMaterialLibrary::GetMaterialDefinition(
			MaterialId, Definition)
			? Definition.DisplayName.ToString()
			: MaterialId.ToString();
	}

	bool CanAffordPetCost(
		const AImmortalPlayerCharacter* Player,
		const FImmortalCraftingCost& Cost)
	{
		if (!Player || Player->GetGold() < Cost.SpiritStones)
		{
			return false;
		}
		for (const FImmortalCraftingMaterialCost& Material : Cost.Materials)
		{
			if (Player->GetMaterialQuantity(Material.MaterialId)
				< Material.Quantity)
			{
				return false;
			}
		}
		return true;
	}

	FString FormatPetCost(
		const AImmortalPlayerCharacter* Player,
		const FImmortalCraftingCost& Cost)
	{
		if (!Player)
		{
			return TEXT("资源数据不可用");
		}
		TArray<FString> Parts;
		if (Cost.SpiritStones > 0)
		{
			Parts.Add(FString::Printf(
				TEXT("灵石 %d/%d"),
				FMath::Max(Player->GetGold(), 0),
				Cost.SpiritStones));
		}
		for (const FImmortalCraftingMaterialCost& Material : Cost.Materials)
		{
			Parts.Add(FString::Printf(
				TEXT("%s %d/%d"),
				*GetMaterialName(Material.MaterialId),
				FMath::Max(
					Player->GetMaterialQuantity(Material.MaterialId),
					0),
				FMath::Max(Material.Quantity, 0)));
		}
		return Parts.IsEmpty()
			? TEXT("无需消耗")
			: FString::Join(Parts, TEXT("、"));
	}

	FString GetStarGlyphs(const int32 Stars)
	{
		FString Result;
		for (int32 Index = 0;
			Index < UImmortalPetLibrary::MaximumPetStars;
			++Index)
		{
			Result += Index < Stars ? TEXT("★") : TEXT("☆");
		}
		return Result;
	}
}

void UImmortalPetWidget::InitializeForPlayer(
	AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalPetWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	PetIds = UImmortalPetLibrary::GetKnownPetIds();
	if (PetIds.Num() > 2)
	{
		PetIds.SetNum(2);
	}

	USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("PetScreenSize"));
	RootSize->SetWidthOverride(1600.0f);
	RootSize->SetHeightOverride(300.0f);
	WidgetTree->RootWidget = RootSize;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("PetScreenBackground"));
	Background->SetBrushColor(
		FLinearColor(0.018f, 0.030f, 0.044f, 0.992f));
	Background->SetPadding(FMargin(0.0f));
	RootSize->AddChild(Background);

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("PetScreenCanvas"));
	Background->AddChild(Canvas);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PetScreenTitle"));
	Title->SetText(FText::FromString(TEXT("灵宠  [P]")));
	StylePetText(
		Title, 23, FLinearColor(0.76f, 0.96f, 1.0f, 1.0f));
	SetPetLayout(
		Canvas->AddChildToCanvas(Title),
		FVector2D(16.0f, 2.0f),
		FVector2D(225.0f, 36.0f));

	HeaderSummaryText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PetHeaderSummary"));
	HeaderSummaryText->SetAutoWrapText(true);
	StylePetText(
		HeaderSummaryText,
		13,
		FLinearColor(0.76f, 0.94f, 0.94f, 1.0f),
		true);
	SetPetLayout(
		Canvas->AddChildToCanvas(HeaderSummaryText),
		FVector2D(236.0f, 2.0f),
		FVector2D(1294.0f, 36.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("PetScreenClose"));
	CloseButton->SetStyle(MakePetButtonStyle(
		FVector2D(46.0f, 32.0f),
		FLinearColor(0.43f, 0.14f, 0.12f, 1.0f)));
	CloseButton->OnClicked.AddDynamic(
		this, &UImmortalPetWidget::HandleCloseClicked);
	SetPetLayout(
		Canvas->AddChildToCanvas(CloseButton),
		FVector2D(1538.0f, 3.0f),
		FVector2D(46.0f, 32.0f));
	AddPetButtonLabel(WidgetTree, CloseButton, TEXT("×"), 22);

	UBorder* SelectionPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("PetSelectionPanel"));
	SelectionPanel->SetBrushColor(
		FLinearColor(0.030f, 0.066f, 0.080f, 0.97f));
	SelectionPanel->SetPadding(FMargin(0.0f));
	SetPetLayout(
		Canvas->AddChildToCanvas(SelectionPanel),
		FVector2D(14.0f, 44.0f),
		FVector2D(370.0f, 243.0f));
	UCanvasPanel* SelectionCanvas =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("PetSelectionCanvas"));
	SelectionPanel->AddChild(SelectionCanvas);

	UTextBlock* SelectionTitle = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PetSelectionTitle"));
	SelectionTitle->SetText(
		FText::FromString(TEXT("灵宠名册 · 点击选择")));
	StylePetText(
		SelectionTitle, 14, FLinearColor(0.65f, 0.96f, 1.0f, 1.0f));
	SetPetLayout(
		SelectionCanvas->AddChildToCanvas(SelectionTitle),
		FVector2D(10.0f, 5.0f),
		FVector2D(350.0f, 27.0f));

	for (int32 Index = 0; Index < PetIds.Num(); ++Index)
	{
		UButton* PetButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			*FString::Printf(TEXT("PetSelectionButton%d"), Index));
		PetButton->SetStyle(MakePetButtonStyle(
			FVector2D(350.0f, 82.0f),
			FLinearColor(0.08f, 0.19f, 0.22f, 1.0f)));
		if (Index == 0)
		{
			PetButton->OnClicked.AddDynamic(
				this, &UImmortalPetWidget::HandleFirstPetClicked);
		}
		else if (Index == 1)
		{
			PetButton->OnClicked.AddDynamic(
				this, &UImmortalPetWidget::HandleSecondPetClicked);
		}
		SetPetLayout(
			SelectionCanvas->AddChildToCanvas(PetButton),
			FVector2D(10.0f, 34.0f + 89.0f * Index),
			FVector2D(350.0f, 82.0f));
		PetButtons.Add(PetButton);
		PetButtonLabels.Add(AddPetButtonLabel(
			WidgetTree, PetButton, TEXT("读取灵宠资料…"), 14));
	}

	UBorder* IdentityPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("PetIdentityPanel"));
	IdentityPanel->SetBrushColor(
		FLinearColor(0.040f, 0.047f, 0.078f, 0.97f));
	IdentityPanel->SetPadding(FMargin(0.0f));
	SetPetLayout(
		Canvas->AddChildToCanvas(IdentityPanel),
		FVector2D(394.0f, 44.0f),
		FVector2D(455.0f, 243.0f));
	UCanvasPanel* IdentityCanvas =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("PetIdentityCanvas"));
	IdentityPanel->AddChild(IdentityCanvas);

	IdentityText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PetIdentity"));
	IdentityText->SetAutoWrapText(true);
	StylePetText(
		IdentityText, 21, FLinearColor(0.84f, 0.82f, 1.0f, 1.0f));
	SetPetLayout(
		IdentityCanvas->AddChildToCanvas(IdentityText),
		FVector2D(12.0f, 7.0f),
		FVector2D(431.0f, 105.0f));

	DescriptionText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PetDescription"));
	DescriptionText->SetAutoWrapText(true);
	StylePetText(
		DescriptionText, 13, FLinearColor(0.82f, 0.88f, 0.92f, 1.0f));
	SetPetLayout(
		IdentityCanvas->AddChildToCanvas(DescriptionText),
		FVector2D(12.0f, 116.0f),
		FVector2D(431.0f, 116.0f));

	UBorder* StatsPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("PetStatsPanel"));
	StatsPanel->SetBrushColor(
		FLinearColor(0.035f, 0.067f, 0.052f, 0.97f));
	StatsPanel->SetPadding(FMargin(10.0f, 7.0f));
	SetPetLayout(
		Canvas->AddChildToCanvas(StatsPanel),
		FVector2D(859.0f, 44.0f),
		FVector2D(420.0f, 243.0f));
	StatsText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PetStats"));
	StatsText->SetAutoWrapText(true);
	StylePetText(
		StatsText, 14, FLinearColor(0.70f, 1.0f, 0.76f, 1.0f));
	StatsPanel->AddChild(StatsText);

	UBorder* ActionPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("PetActionPanel"));
	ActionPanel->SetBrushColor(
		FLinearColor(0.052f, 0.052f, 0.042f, 0.98f));
	ActionPanel->SetPadding(FMargin(0.0f));
	SetPetLayout(
		Canvas->AddChildToCanvas(ActionPanel),
		FVector2D(1289.0f, 44.0f),
		FVector2D(297.0f, 243.0f));
	UCanvasPanel* ActionCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("PetActionCanvas"));
	ActionPanel->AddChild(ActionCanvas);

	CostText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PetCost"));
	CostText->SetAutoWrapText(true);
	StylePetText(
		CostText, 11, FLinearColor(0.92f, 0.86f, 0.66f, 1.0f));
	SetPetLayout(
		ActionCanvas->AddChildToCanvas(CostText),
		FVector2D(10.0f, 5.0f),
		FVector2D(277.0f, 85.0f));

	PrimaryActionButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("PetPrimaryActionButton"));
	PrimaryActionButton->OnClicked.AddDynamic(
		this, &UImmortalPetWidget::HandlePrimaryActionClicked);
	SetPetLayout(
		ActionCanvas->AddChildToCanvas(PrimaryActionButton),
		FVector2D(11.0f, 92.0f),
		FVector2D(275.0f, 43.0f));
	PrimaryActionButtonText = AddPetButtonLabel(
		WidgetTree, PrimaryActionButton, TEXT("驯服灵宠"), 15);

	StarUpButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("PetStarUpButton"));
	StarUpButton->OnClicked.AddDynamic(
		this, &UImmortalPetWidget::HandleStarUpClicked);
	SetPetLayout(
		ActionCanvas->AddChildToCanvas(StarUpButton),
		FVector2D(11.0f, 141.0f),
		FVector2D(275.0f, 38.0f));
	StarUpButtonText = AddPetButtonLabel(
		WidgetTree, StarUpButton, TEXT("灵宠升星"), 14);

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PetOperationResult"));
	ResultText->SetText(
		FText::FromString(TEXT("选择灵宠后可驯服、出战或升星。")));
	ResultText->SetAutoWrapText(true);
	StylePetText(
		ResultText,
		11,
		FLinearColor(0.76f, 0.84f, 0.86f, 1.0f),
		true);
	SetPetLayout(
		ActionCanvas->AddChildToCanvas(ResultText),
		FVector2D(11.0f, 184.0f),
		FVector2D(275.0f, 51.0f));

	RefreshFromPlayer();
}

void UImmortalPetWidget::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (ResultMessageExpirySeconds > 0.0
		&& FPlatformTime::Seconds()
			>= ResultMessageExpirySeconds)
	{
		ResetResultMessage();
	}
	if (!Player.IsValid()) return;

	RefreshAccumulator += FMath::Max(InDeltaTime, 0.0f);
	if (Player->GetPetRevision() != LastRevision
		|| RefreshAccumulator >= 0.20f)
	{
		RefreshFromPlayer();
	}
}

void UImmortalPetWidget::RefreshFromPlayer()
{
	if (!Player.IsValid() || !HeaderSummaryText)
	{
		return;
	}

	const FImmortalPetState State = Player->GetPetState();
	LastRevision = Player->GetPetRevision();
	RefreshAccumulator = 0.0f;

	if (PetIds.IsEmpty())
	{
		PetIds = UImmortalPetLibrary::GetKnownPetIds();
		if (PetIds.Num() > 2)
		{
			PetIds.SetNum(2);
		}
	}
	if (SelectedPetId.IsNone()
		|| !PetIds.Contains(SelectedPetId))
	{
		SelectedPetId = PetIds.Contains(State.ActivePetId)
			? State.ActivePetId
			: (PetIds.IsEmpty() ? NAME_None : PetIds[0]);
	}
	else if (!LastActivePetId.IsNone()
		&& State.ActivePetId != LastActivePetId
		&& SelectedPetId == LastActivePetId
		&& PetIds.Contains(State.ActivePetId))
	{
		// If an external transaction changes the active pet while the panel
		// was showing the old active entry, follow the new selection. A manual
		// preview of a non-active entry is otherwise left untouched.
		SelectedPetId = State.ActivePetId;
	}
	LastActivePetId = State.ActivePetId;

	int32 OwnedCount = 0;
	for (const FName PetId : PetIds)
	{
		FImmortalPetProgress Progress;
		if (Player->GetPetProgress(PetId, Progress)
			&& Progress.bOwned)
		{
			++OwnedCount;
		}
	}

	FString ActiveName = TEXT("无");
	FImmortalPetDefinition ActiveDefinition;
	if (UImmortalPetLibrary::GetPetDefinition(
		State.ActivePetId, ActiveDefinition))
	{
		ActiveName = ActiveDefinition.DisplayName.ToString();
	}
	HeaderSummaryText->SetText(FText::FromString(FString::Printf(
		TEXT("已驯服 %d/%d  ·  当前出战：%s  ·  灵石：%d  ·  灵宠会自动跟随并协助战斗"),
		OwnedCount,
		PetIds.Num(),
		*ActiveName,
		FMath::Max(Player->GetGold(), 0))));

	RefreshPetSelection(State);
	RefreshPetDetails(State);
	RefreshPetActions(State);
}

void UImmortalPetWidget::RefreshPetSelection(
	const FImmortalPetState& State)
{
	for (int32 Index = 0; Index < PetIds.Num(); ++Index)
	{
		if (!PetButtons.IsValidIndex(Index)
			|| !PetButtonLabels.IsValidIndex(Index))
		{
			continue;
		}

		FImmortalPetDefinition Definition;
		FImmortalPetProgress Progress;
		if (!UImmortalPetLibrary::GetPetDefinition(
				PetIds[Index], Definition)
			|| !Player->GetPetProgress(PetIds[Index], Progress))
		{
			PetButtonLabels[Index]->SetText(
				FText::FromString(TEXT("灵宠资料不可用")));
			PetButtons[Index]->SetIsEnabled(false);
			continue;
		}

		const bool bSelected = PetIds[Index] == SelectedPetId;
		const bool bActive = PetIds[Index] == State.ActivePetId;
		const FString OwnershipText = bActive
			? TEXT("正在出战")
			: (Progress.bOwned ? TEXT("已拥有") : TEXT("尚未驯服"));
		PetButtonLabels[Index]->SetText(FText::FromString(FString::Printf(
			TEXT("%s  %s\nLv.%d  %s  ·  %s"),
			*Definition.IconGlyph.ToString(),
			*Definition.DisplayName.ToString(),
			FMath::Max(Progress.Level, 1),
			*GetStarGlyphs(Progress.Stars),
			*OwnershipText)));
		PetButtons[Index]->SetIsEnabled(true);
		PetButtons[Index]->SetStyle(MakePetButtonStyle(
			FVector2D(350.0f, 82.0f),
			bSelected
				? Definition.DisplayColor.CopyWithNewOpacity(0.58f)
				: (Progress.bOwned
					? FLinearColor(0.08f, 0.25f, 0.20f, 1.0f)
					: FLinearColor(0.09f, 0.12f, 0.15f, 1.0f))));
	}
}

void UImmortalPetWidget::RefreshPetDetails(
	const FImmortalPetState& State)
{
	if (SelectedPetId.IsNone()) return;

	FImmortalPetDefinition Definition;
	FImmortalPetProgress Progress;
	if (!UImmortalPetLibrary::GetPetDefinition(
		SelectedPetId, Definition)
		|| !Player->GetPetProgress(SelectedPetId, Progress))
	{
		if (IdentityText)
		{
			IdentityText->SetText(
				FText::FromString(TEXT("灵宠资料不可用")));
		}
		return;
	}

	const bool bActive = State.ActivePetId == SelectedPetId;
	const FString StatusText = bActive
		? TEXT("已拥有 · 正在出战")
		: (Progress.bOwned ? TEXT("已拥有 · 待命") : TEXT("尚未驯服"));
	if (IdentityText)
	{
		IdentityText->SetText(FText::FromString(FString::Printf(
			TEXT("【%s】  %s\n%s  ·  %s\n%s"),
			*Definition.IconGlyph.ToString(),
			*Definition.DisplayName.ToString(),
			*GetPetAttackStyleText(Definition.AttackStyle),
			*SelectedPetId.ToString(),
			*StatusText)));
		IdentityText->SetColorAndOpacity(
			FSlateColor(Definition.DisplayColor));
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(FText::FromString(FString::Printf(
			TEXT("%s\n\n自动战斗：攻击范围 %.0f · 搜索范围 %.0f\n跟随偏移 %.0f · 移速 %.0f"),
			*Definition.Description.ToString(),
			FMath::Max(Definition.AttackRange, 0.0f),
			FMath::Max(Definition.SearchRange, 0.0f),
			Definition.FollowOffsetX,
			FMath::Max(Definition.MovementSpeed, 0.0f))));
	}

	FImmortalPetProgress PreviewProgress = Progress;
	PreviewProgress.bOwned = true;
	const int32 ExperienceRequired =
		UImmortalPetLibrary::GetExperienceRequiredForLevel(
			Progress.Level);
	const FString ExperienceText =
		Progress.Level >= UImmortalPetLibrary::MaximumPetLevel
			? TEXT("已满级")
			: FString::Printf(
				TEXT("%d / %d"),
				FMath::Max(Progress.Experience, 0),
				FMath::Max(ExperienceRequired, 1));
	const float DamageRatio =
		UImmortalPetLibrary::CalculateDamageRatio(
			Definition, PreviewProgress);
	const float CombatPower =
		UImmortalPetLibrary::CalculateCombatPower(
			Definition,
			PreviewProgress,
			Player->GetTotalAttackDamage());
	if (StatsText)
	{
		StatsText->SetText(FText::FromString(FString::Printf(
			TEXT("成长与战斗\n"
				"等级：Lv.%d / %d\n"
				"经验：%s\n"
				"星级：%s（%d/%d）\n\n"
				"每击攻击系数：×%.3f（%.1f%%）\n"
				"预计 DPS：%.1f%s\n"
				"攻击间隔：%.2f 秒  ·  暴击：%.0f%%\n"
				"协战击杀：%lld"),
			FMath::Clamp(
				Progress.Level,
				1,
				UImmortalPetLibrary::MaximumPetLevel),
			UImmortalPetLibrary::MaximumPetLevel,
			*ExperienceText,
			*GetStarGlyphs(Progress.Stars),
			FMath::Clamp(
				Progress.Stars,
				0,
				UImmortalPetLibrary::MaximumPetStars),
			UImmortalPetLibrary::MaximumPetStars,
			DamageRatio,
			DamageRatio * 100.0f,
			CombatPower,
			Progress.bOwned ? TEXT("") : TEXT("（驯服后）"),
			FMath::Max(Definition.AttackInterval, 0.1f),
			FMath::Clamp(
				Definition.CriticalChance, 0.0f, 1.0f) * 100.0f,
			Progress.TotalCombatKills)));
	}
}

void UImmortalPetWidget::RefreshPetActions(
	const FImmortalPetState& State)
{
	if (SelectedPetId.IsNone()) return;

	FImmortalPetDefinition Definition;
	FImmortalPetProgress Progress;
	if (!UImmortalPetLibrary::GetPetDefinition(
		SelectedPetId, Definition)
		|| !Player->GetPetProgress(SelectedPetId, Progress))
	{
		if (PrimaryActionButton)
		{
			PrimaryActionButton->SetIsEnabled(false);
		}
		if (StarUpButton)
		{
			StarUpButton->SetIsEnabled(false);
		}
		return;
	}

	const bool bActive = State.ActivePetId == SelectedPetId;
	const bool bMaximumStars =
		Progress.Stars >= UImmortalPetLibrary::MaximumPetStars;
	const FImmortalCraftingCost StarCost =
		UImmortalPetLibrary::GetStarUpCost(Progress);
	const bool bCanAffordUnlock =
		CanAffordPetCost(Player.Get(), Definition.UnlockCost);
	const bool bCanAffordStar =
		CanAffordPetCost(Player.Get(), StarCost);

	if (CostText)
	{
		const FString UnlockText = Progress.bOwned
			? TEXT("驯服消耗：已完成")
			: FString::Printf(
				TEXT("驯服消耗：%s%s"),
				*FormatPetCost(Player.Get(), Definition.UnlockCost),
				bCanAffordUnlock ? TEXT("") : TEXT("（资源不足）"));
		const FString StarText = !Progress.bOwned
			? TEXT("升星消耗：需先驯服")
			: (bMaximumStars
				? TEXT("升星消耗：已达到 5 星")
				: FString::Printf(
					TEXT("升至 %d 星：%s%s"),
					Progress.Stars + 1,
					*FormatPetCost(Player.Get(), StarCost),
					bCanAffordStar
						? TEXT("")
						: TEXT("（资源不足）")));
		CostText->SetText(FText::FromString(FString::Printf(
			TEXT("资源与培养\n%s\n%s"),
			*UnlockText,
			*StarText)));
		CostText->SetColorAndOpacity(FSlateColor(
			(!Progress.bOwned && !bCanAffordUnlock)
				|| (Progress.bOwned
					&& !bMaximumStars
					&& !bCanAffordStar)
				? FLinearColor(1.0f, 0.55f, 0.37f, 1.0f)
				: FLinearColor(0.92f, 0.86f, 0.66f, 1.0f)));
	}

	if (PrimaryActionButton && PrimaryActionButtonText)
	{
		PrimaryActionButton->SetIsEnabled(!bActive);
		PrimaryActionButton->SetStyle(MakePetButtonStyle(
			FVector2D(275.0f, 43.0f),
			Progress.bOwned
				? FLinearColor(0.12f, 0.42f, 0.32f, 1.0f)
				: FLinearColor(0.46f, 0.28f, 0.10f, 1.0f)));
		PrimaryActionButtonText->SetText(FText::FromString(
			bActive
				? TEXT("正在出战")
				: (Progress.bOwned
					? TEXT("设为出战")
					: TEXT("驯服灵宠"))));
	}
	if (StarUpButton && StarUpButtonText)
	{
		const bool bCanAttemptStar =
			Progress.bOwned && !bMaximumStars;
		StarUpButton->SetIsEnabled(bCanAttemptStar);
		StarUpButton->SetStyle(MakePetButtonStyle(
			FVector2D(275.0f, 38.0f),
			FLinearColor(0.40f, 0.31f, 0.09f, 1.0f)));
		StarUpButtonText->SetText(FText::FromString(
			!Progress.bOwned
				? TEXT("驯服后可升星")
				: (bMaximumStars
					? TEXT("已达到最高 5 星")
					: FString::Printf(
						TEXT("升至 %d 星"),
						Progress.Stars + 1))));
	}
}

void UImmortalPetWidget::SetResultMessage(
	const FText& Message,
	const bool bSucceeded)
{
	if (!ResultText) return;
	ResultText->SetText(Message);
	ResultText->SetColorAndOpacity(FSlateColor(
		bSucceeded
			? FLinearColor(0.45f, 1.0f, 0.66f, 1.0f)
			: FLinearColor(1.0f, 0.38f, 0.29f, 1.0f)));
	ResultMessageExpirySeconds =
		FPlatformTime::Seconds() + 5.0;
}

void UImmortalPetWidget::ResetResultMessage()
{
	ResultMessageExpirySeconds = 0.0;
	if (!ResultText) return;
	ResultText->SetText(
		FText::FromString(TEXT("选择灵宠后可驯服、出战或升星。")));
	ResultText->SetColorAndOpacity(FSlateColor(
		FLinearColor(0.76f, 0.84f, 0.86f, 1.0f)));
}

void UImmortalPetWidget::SelectPetByIndex(const int32 PetIndex)
{
	if (!PetIds.IsValidIndex(PetIndex)) return;

	SelectedPetId = PetIds[PetIndex];
	FImmortalPetDefinition Definition;
	const FString Name = UImmortalPetLibrary::GetPetDefinition(
		SelectedPetId, Definition)
		? Definition.DisplayName.ToString()
		: SelectedPetId.ToString();
	SetResultMessage(
		FText::FromString(FString::Printf(
			TEXT("已选择 %s，可查看培养状态。"), *Name)),
		true);
	RefreshFromPlayer();
}

void UImmortalPetWidget::HandleFirstPetClicked()
{
	SelectPetByIndex(0);
}

void UImmortalPetWidget::HandleSecondPetClicked()
{
	SelectPetByIndex(1);
}

void UImmortalPetWidget::HandlePrimaryActionClicked()
{
	if (!Player.IsValid() || SelectedPetId.IsNone()) return;

	FImmortalPetProgress Progress;
	if (!Player->GetPetProgress(SelectedPetId, Progress))
	{
		SetResultMessage(
			FText::FromString(TEXT("无法读取所选灵宠的状态。")),
			false);
		return;
	}

	const FImmortalPetOperationResult Result = Progress.bOwned
		? Player->SetActivePet(SelectedPetId)
		: Player->UnlockPet(SelectedPetId);
	SetResultMessage(Result.Message, Result.bSucceeded);
	RefreshFromPlayer();
}

void UImmortalPetWidget::HandleStarUpClicked()
{
	if (!Player.IsValid() || SelectedPetId.IsNone()) return;

	const FImmortalPetOperationResult Result =
		Player->RaisePetStar(SelectedPetId);
	SetResultMessage(Result.Message, Result.bSucceeded);
	RefreshFromPlayer();
}

void UImmortalPetWidget::HandleCloseClicked()
{
	if (Player.IsValid())
	{
		Player->TogglePet();
	}
}
