// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalMapWidget.h"

#include "ImmortalMapEntryWidget.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Styling/SlateTypes.h"

namespace
{
	void SetMapLayout(UCanvasPanelSlot* Slot, const FVector2D Position, const FVector2D Size)
	{
		if (!Slot) return;
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
	}

	void StyleMapText(UTextBlock* Text, const int32 Size, const FLinearColor& Color, const bool bCentered = false)
	{
		if (!Text) return;
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor::Black);
		Text->SetJustification(bCentered ? ETextJustify::Center : ETextJustify::Left);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}

	FSlateBrush MakeMapButtonBrush(const FVector2D Size, const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	FButtonStyle MakeMapButtonStyle(const FVector2D Size, const FLinearColor& Tint)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeMapButtonBrush(Size, Tint));
		Style.SetHovered(MakeMapButtonBrush(Size, Tint * 1.18f));
		Style.SetPressed(MakeMapButtonBrush(Size, Tint * 0.82f));
		Style.SetDisabled(MakeMapButtonBrush(Size, FLinearColor(0.11f, 0.12f, 0.14f, 0.82f)));
		return Style;
	}

	UTextBlock* AddButtonLabel(UWidgetTree* Tree, UButton* Button, const TCHAR* Label, const int32 FontSize)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		StyleMapText(Text, FontSize, FLinearColor(1.0f, 0.90f, 0.58f, 1.0f), true);
		Button->AddChild(Text);
		return Text;
	}
}

void UImmortalMapWidget::InitializeForPlayer(AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	RefreshFromPlayer();
}

void UImmortalMapWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MapScreenSize"));
	RootSize->SetWidthOverride(900.0f);
	RootSize->SetHeightOverride(600.0f);
	WidgetTree->RootWidget = RootSize;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MapScreenBackground"));
	Background->SetBrushColor(FLinearColor(0.018f, 0.028f, 0.043f, 0.985f));
	Background->SetPadding(FMargin(0.0f));
	RootSize->AddChild(Background);

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MapScreenCanvas"));
	Background->AddChild(Canvas);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MapScreenTitle"));
	Title->SetText(FText::FromString(TEXT("历练地图  [M]")));
	StyleMapText(Title, 28, FLinearColor(1.0f, 0.79f, 0.30f, 1.0f));
	SetMapLayout(Canvas->AddChildToCanvas(Title), FVector2D(24.0f, 13.0f), FVector2D(350.0f, 44.0f));

	CurrentMapText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CurrentMapText"));
	StyleMapText(CurrentMapText, 17, FLinearColor(0.63f, 0.94f, 1.0f, 1.0f), true);
	SetMapLayout(Canvas->AddChildToCanvas(CurrentMapText), FVector2D(384.0f, 20.0f), FVector2D(404.0f, 32.0f));

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MapScreenClose"));
	CloseButton->OnClicked.AddDynamic(this, &UImmortalMapWidget::HandleCloseClicked);
	CloseButton->SetStyle(MakeMapButtonStyle(FVector2D(64.0f), FLinearColor(0.44f, 0.18f, 0.12f, 1.0f)));
	SetMapLayout(Canvas->AddChildToCanvas(CloseButton), FVector2D(816.0f, 8.0f), FVector2D(64.0f));
	AddButtonLabel(WidgetTree, CloseButton, TEXT("×"), 24);

	UBorder* LeftPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MapListPanel"));
	LeftPanel->SetBrushColor(FLinearColor(0.04f, 0.065f, 0.085f, 0.94f));
	LeftPanel->SetPadding(FMargin(10.0f, 8.0f));
	SetMapLayout(Canvas->AddChildToCanvas(LeftPanel), FVector2D(14.0f, 66.0f), FVector2D(356.0f, 520.0f));

	UScrollBox* MapScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("MapListScroll"));
	MapScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	LeftPanel->AddChild(MapScroll);
	MapList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MapList"));
	MapScroll->AddChild(MapList);

	UBorder* DetailPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MapDetailPanel"));
	DetailPanel->SetBrushColor(FLinearColor(0.035f, 0.05f, 0.07f, 0.96f));
	DetailPanel->SetPadding(FMargin(0.0f));
	SetMapLayout(Canvas->AddChildToCanvas(DetailPanel), FVector2D(384.0f, 66.0f), FVector2D(502.0f, 520.0f));

	UCanvasPanel* DetailCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MapDetailCanvas"));
	DetailPanel->AddChild(DetailCanvas);

	DetailNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MapDetailName"));
	StyleMapText(DetailNameText, 27, FLinearColor::White);
	SetMapLayout(DetailCanvas->AddChildToCanvas(DetailNameText), FVector2D(18.0f, 10.0f), FVector2D(302.0f, 42.0f));

	DetailRequirementText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MapDetailRequirement"));
	StyleMapText(DetailRequirementText, 17, FLinearColor(0.62f, 0.90f, 1.0f, 1.0f));
	SetMapLayout(DetailCanvas->AddChildToCanvas(DetailRequirementText), FVector2D(18.0f, 52.0f), FVector2D(462.0f, 30.0f));

	DetailDescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MapDetailDescription"));
	DetailDescriptionText->SetAutoWrapText(true);
	StyleMapText(DetailDescriptionText, 17, FLinearColor(0.88f, 0.90f, 0.92f, 1.0f));
	SetMapLayout(DetailCanvas->AddChildToCanvas(DetailDescriptionText), FVector2D(18.0f, 86.0f), FVector2D(462.0f, 70.0f));

	DetailProgressText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MapDetailProgress"));
	DetailProgressText->SetAutoWrapText(true);
	StyleMapText(DetailProgressText, 18, FLinearColor(1.0f, 0.82f, 0.38f, 1.0f));
	SetMapLayout(DetailCanvas->AddChildToCanvas(DetailProgressText), FVector2D(18.0f, 163.0f), FVector2D(462.0f, 52.0f));

	DetailEnemyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MapDetailEnemies"));
	DetailEnemyText->SetAutoWrapText(true);
	StyleMapText(DetailEnemyText, 17, FLinearColor(1.0f, 0.60f, 0.40f, 1.0f));
	SetMapLayout(DetailCanvas->AddChildToCanvas(DetailEnemyText), FVector2D(18.0f, 220.0f), FVector2D(462.0f, 60.0f));

	DetailDropText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MapDetailDrops"));
	DetailDropText->SetAutoWrapText(true);
	StyleMapText(DetailDropText, 17, FLinearColor(0.66f, 1.0f, 0.76f, 1.0f));
	SetMapLayout(DetailCanvas->AddChildToCanvas(DetailDropText), FVector2D(18.0f, 286.0f), FVector2D(462.0f, 124.0f));

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MapTravelResult"));
	ResultText->SetAutoWrapText(true);
	StyleMapText(ResultText, 17, FLinearColor(0.70f, 0.84f, 0.92f, 1.0f), true);
	SetMapLayout(DetailCanvas->AddChildToCanvas(ResultText), FVector2D(18.0f, 416.0f), FVector2D(462.0f, 44.0f));

	TravelButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MapTravelButton"));
	TravelButton->OnClicked.AddDynamic(this, &UImmortalMapWidget::HandleTravelClicked);
	TravelButton->SetStyle(MakeMapButtonStyle(FVector2D(210.0f, 46.0f), FLinearColor(0.20f, 0.48f, 0.34f, 1.0f)));
	SetMapLayout(DetailCanvas->AddChildToCanvas(TravelButton), FVector2D(146.0f, 466.0f), FVector2D(210.0f, 46.0f));
	TravelButtonText = AddButtonLabel(WidgetTree, TravelButton, TEXT("前往历练"), 18);
}

void UImmortalMapWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Player.IsValid()) return;

	const int32 RealmIndex = static_cast<int32>(Player->GetCultivationRealm());
	const int32 MapRevision = Player->GetMapRevision();
	if (MapRevision != LastObservedMapRevision || RealmIndex != LastObservedRealmIndex)
	{
		RefreshFromPlayer();
	}
}

void UImmortalMapWidget::RefreshFromPlayer()
{
	if (!Player.IsValid() || !MapList)
	{
		return;
	}

	CachedState = Player->GetMapSystemState();
	UImmortalMapLibrary::NormalizeState(CachedState);
	if (SelectedMapId.IsNone())
	{
		SelectedMapId = CachedState.ActiveMapId;
	}
	FImmortalMapDefinition SelectedDefinition;
	if (!UImmortalMapLibrary::GetMapDefinition(SelectedMapId, SelectedDefinition))
	{
		SelectedMapId = CachedState.ActiveMapId;
	}

	LastStateFingerprint = CalculateStateFingerprint(
		CachedState, static_cast<int32>(Player->GetCultivationRealm()));
	LastObservedMapRevision = Player->GetMapRevision();
	LastObservedRealmIndex = static_cast<int32>(Player->GetCultivationRealm());
	RebuildMapEntries();
	RefreshSelectedMapDetails();
}

void UImmortalMapWidget::SelectMap(const FName MapId)
{
	FImmortalMapDefinition Definition;
	if (!UImmortalMapLibrary::GetMapDefinition(MapId, Definition)) return;
	SelectedMapId = MapId;
	SetResultMessage(FText::GetEmpty(), true);
	RebuildMapEntries();
	RefreshSelectedMapDetails();
}

void UImmortalMapWidget::RebuildMapEntries()
{
	if (!MapList || !Player.IsValid()) return;
	MapList->ClearChildren();

	const int32 RealmIndex = static_cast<int32>(Player->GetCultivationRealm());
	for (const FName MapId : UImmortalMapLibrary::GetKnownMapIds())
	{
		FImmortalMapDefinition Definition;
		if (!UImmortalMapLibrary::GetMapDefinition(MapId, Definition)) continue;
		FImmortalMapProgress Progress;
		UImmortalMapLibrary::GetMapProgress(CachedState, MapId, Progress);

		UImmortalMapEntryWidget* Entry = CreateWidget<UImmortalMapEntryWidget>(this, UImmortalMapEntryWidget::StaticClass());
		if (!Entry) continue;
		Entry->InitializeMapEntry(
			this,
			Definition,
			Progress,
			UImmortalMapLibrary::IsMapUnlocked(MapId, RealmIndex),
			CachedState.ActiveMapId == MapId,
			SelectedMapId == MapId);
		MapList->AddChild(Entry);
	}
}

void UImmortalMapWidget::RefreshSelectedMapDetails()
{
	if (!Player.IsValid()) return;

	FImmortalMapDefinition Definition;
	if (!UImmortalMapLibrary::GetMapDefinition(SelectedMapId, Definition)) return;
	FImmortalMapProgress Progress;
	UImmortalMapLibrary::GetMapProgress(CachedState, SelectedMapId, Progress);
	const int32 RealmIndex = static_cast<int32>(Player->GetCultivationRealm());
	const bool bUnlocked = UImmortalMapLibrary::IsMapUnlocked(SelectedMapId, RealmIndex);
	const bool bActive = CachedState.ActiveMapId == SelectedMapId;

	FImmortalMapDefinition ActiveDefinition;
	const FText ActiveName = UImmortalMapLibrary::GetMapDefinition(CachedState.ActiveMapId, ActiveDefinition)
		? ActiveDefinition.DisplayName
		: FText::FromName(CachedState.ActiveMapId);
	if (CurrentMapText)
	{
		CurrentMapText->SetText(FText::FromString(FString::Printf(
			TEXT("当前历练：%s"), *ActiveName.ToString())));
	}
	if (DetailNameText)
	{
		DetailNameText->SetText(Definition.DisplayName);
		DetailNameText->SetColorAndOpacity(FSlateColor(Definition.SceneTint.GetClamped(0.48f, 1.0f)));
	}
	if (DetailRequirementText)
	{
		const FString UnlockState = bUnlocked ? TEXT("已解锁") : TEXT("境界不足，尚未解锁");
		DetailRequirementText->SetText(FText::FromString(FString::Printf(
			TEXT("进入境界：%s境  ·  %s"),
			*UImmortalMapLibrary::GetRealmRequirementText(Definition.RequiredRealmIndex).ToString(), *UnlockState)));
		DetailRequirementText->SetColorAndOpacity(FSlateColor(bUnlocked
			? FLinearColor(0.45f, 1.0f, 0.70f, 1.0f)
			: FLinearColor(1.0f, 0.38f, 0.28f, 1.0f)));
	}
	if (DetailDescriptionText)
	{
		DetailDescriptionText->SetText(Definition.Description);
	}
	if (DetailProgressText)
	{
		const FString CompletionText = Progress.bCompleted ? TEXT("  [已通关]") : TEXT("");
		DetailProgressText->SetText(FText::FromString(FString::Printf(
			TEXT("独立进度：第 %d / %d 关%s\n每 %d 关出现守关 BOSS"),
			FMath::Clamp(Progress.Stage, 1, Definition.MaximumStage), Definition.MaximumStage,
			*CompletionText, Definition.BossStageInterval)));
	}
	if (DetailEnemyText)
	{
		DetailEnemyText->SetText(FText::FromString(FString::Printf(
			TEXT("妖兽：%s  ·  BOSS：%s\n难度：生命 ×%.2f  攻击 ×%.2f  防御 ×%.2f"),
			*Definition.NormalMonsterName.ToString(), *Definition.BossName.ToString(),
			Definition.HealthMultiplier, Definition.AttackMultiplier, Definition.DefenseMultiplier)));
	}
	if (DetailDropText)
	{
		TArray<FString> MaterialNames;
		for (const FName MaterialId : Definition.MaterialPoolIds)
		{
			FImmortalMaterialDefinition Material;
			MaterialNames.Add(UImmortalMaterialLibrary::GetMaterialDefinition(MaterialId, Material)
				? Material.DisplayName.ToString()
				: MaterialId.ToString());
		}
		DetailDropText->SetText(FText::FromString(FString::Printf(
			TEXT("地图掉落\n装备：%s及以上  ·  BOSS：%s及以上\n灵石倍率：×%.2f  ·  材料：%s"),
			*UImmortalEquipmentLibrary::GetQualityText(Definition.MinimumEquipmentQuality).ToString(),
			*UImmortalEquipmentLibrary::GetQualityText(Definition.BossMinimumEquipmentQuality).ToString(),
			Definition.SpiritStoneMultiplier,
			*FString::Join(MaterialNames, TEXT("、")))));
	}
	if (TravelButton && TravelButtonText)
	{
		TravelButton->SetIsEnabled(bUnlocked && !bActive);
		TravelButtonText->SetText(FText::FromString(
			bActive ? TEXT("正在此地历练") : (bUnlocked ? TEXT("前往历练") : TEXT("境界不足"))));
		TravelButtonText->SetColorAndOpacity(FSlateColor(bUnlocked
			? FLinearColor(1.0f, 0.90f, 0.58f, 1.0f)
			: FLinearColor(0.55f, 0.56f, 0.60f, 1.0f)));
	}
}

uint32 UImmortalMapWidget::CalculateStateFingerprint(
	const FImmortalMapSystemState& State,
	const int32 RealmIndex) const
{
	uint32 Hash = HashCombine(GetTypeHash(State.ActiveMapId), GetTypeHash(RealmIndex));
	Hash = HashCombine(Hash, GetTypeHash(State.bInitialized));
	for (const FImmortalMapProgress& Progress : State.MapProgress)
	{
		Hash = HashCombine(Hash, GetTypeHash(Progress.MapId));
		Hash = HashCombine(Hash, GetTypeHash(Progress.Stage));
		Hash = HashCombine(Hash, GetTypeHash(Progress.StageKills));
		Hash = HashCombine(Hash, GetTypeHash(Progress.bCompleted));
	}
	return Hash;
}

void UImmortalMapWidget::SetResultMessage(const FText& Message, const bool bSucceeded)
{
	if (!ResultText) return;
	ResultText->SetText(Message);
	ResultText->SetColorAndOpacity(FSlateColor(bSucceeded
		? FLinearColor(0.42f, 1.0f, 0.68f, 1.0f)
		: FLinearColor(1.0f, 0.38f, 0.28f, 1.0f)));
}

void UImmortalMapWidget::HandleTravelClicked()
{
	if (!Player.IsValid() || SelectedMapId.IsNone()) return;
	const FImmortalMapTravelResult Result = Player->TravelToMap(SelectedMapId);
	if (Result.bSucceeded)
	{
		SelectedMapId = Result.DestinationMapId;
		RefreshFromPlayer();
		SetResultMessage(Result.Message, true);
		Player->ToggleMapSelection();
		return;
	}
	SetResultMessage(Result.Message, Result.bSucceeded);
}

void UImmortalMapWidget::HandleCloseClicked()
{
	if (Player.IsValid())
	{
		Player->ToggleMapSelection();
	}
}
