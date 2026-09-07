// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalManagementWidget.h"
#include "ImmortalUITheme.h"

#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Progression/ImmortalCultivationComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformTime.h"
#include "Styling/SlateTypes.h"
#include "UObject/SoftObjectPath.h"
#include "Misc/PackageName.h"

namespace
{
	enum class EManagementWorldTier : uint8
	{
		Mortal,
		Spirit,
		Immortal
	};

	void SetManagementLayout(
		UCanvasPanelSlot* Slot,
		const FVector2D Position,
		const FVector2D Size)
	{
		if (!Slot) return;
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetAutoSize(false);
	}

	void StyleManagementText(
		UTextBlock* Text,
		const int32 FontSize,
		const FLinearColor& Color,
		const bool bCentered = false)
	{
		if (!Text) return;
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor::Black);
		Text->SetJustification(
			bCentered ? ETextJustify::Center : ETextJustify::Left);
	}

	FSlateBrush MakeManagementBrush(
		const FVector2D Size,
		const FLinearColor& Color)
	{
		FSlateBrush Brush = ImmortalUITheme::PanelBrush(Color);
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(Color);
		return Brush;
	}

	FButtonStyle MakeManagementButtonStyle(
		const FVector2D Size,
		const FLinearColor& Color)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeManagementBrush(Size, Color));
		Style.SetHovered(MakeManagementBrush(
			Size, (Color * 1.18f).GetClamped()));
		Style.SetPressed(MakeManagementBrush(
			Size, (Color * 0.76f).GetClamped()));
		Style.SetDisabled(MakeManagementBrush(
			Size, FLinearColor(0.09f, 0.10f, 0.11f, 0.82f)));
		return Style;
	}

	FText GetFeatureDisplayName(const EImmortalManagementFeature Feature)
	{
		switch (Feature)
		{
		case EImmortalManagementFeature::Cultivation:
			return FText::FromString(TEXT("修炼"));
		case EImmortalManagementFeature::Inventory:
			return FText::FromString(TEXT("装备与储物戒"));
		case EImmortalManagementFeature::Alchemy:
			return FText::FromString(TEXT("炼丹"));
		case EImmortalManagementFeature::Crafting:
			return FText::FromString(TEXT("炼器"));
		case EImmortalManagementFeature::Artifact:
			return FText::FromString(TEXT("法宝"));
		case EImmortalManagementFeature::Technique:
			return FText::FromString(TEXT("功法"));
		case EImmortalManagementFeature::CharacterBuild:
			return FText::FromString(TEXT("灵根与修炼流派"));
		case EImmortalManagementFeature::Shop:
			return FText::FromString(TEXT("百宝阁"));
		case EImmortalManagementFeature::Map:
			return FText::FromString(TEXT("历练地图"));
		case EImmortalManagementFeature::Quest:
			return FText::FromString(TEXT("仙途任务"));
		case EImmortalManagementFeature::Cave:
			return FText::FromString(TEXT("洞府"));
		case EImmortalManagementFeature::Farming:
			return FText::FromString(TEXT("灵田"));
		case EImmortalManagementFeature::Sect:
			return FText::FromString(TEXT("宗门"));
		case EImmortalManagementFeature::WorldBoss:
			return FText::FromString(TEXT("世界妖王"));
		case EImmortalManagementFeature::EndlessDungeon:
			return FText::FromString(TEXT("无尽秘境"));
		case EImmortalManagementFeature::Pet:
			return FText::FromString(TEXT("灵宠"));
		case EImmortalManagementFeature::Settings:
			return FText::FromString(TEXT("设置"));
		default:
			return FText::FromString(TEXT("仙府主页"));
		}
	}

	FString GetFeatureAssetToken(const EImmortalManagementFeature Feature)
	{
		switch (Feature)
		{
		case EImmortalManagementFeature::Cultivation: return TEXT("Cultivation");
		case EImmortalManagementFeature::Inventory: return TEXT("Inventory");
		case EImmortalManagementFeature::Alchemy: return TEXT("Alchemy");
		case EImmortalManagementFeature::Crafting: return TEXT("Crafting");
		case EImmortalManagementFeature::Artifact: return TEXT("Artifact");
		case EImmortalManagementFeature::Technique: return TEXT("Technique");
		case EImmortalManagementFeature::CharacterBuild: return TEXT("CharacterBuild");
		case EImmortalManagementFeature::Shop: return TEXT("Shop");
		case EImmortalManagementFeature::Map: return TEXT("Map");
		case EImmortalManagementFeature::Quest: return TEXT("Quest");
		case EImmortalManagementFeature::Cave: return TEXT("Cave");
		case EImmortalManagementFeature::Farming: return TEXT("Farming");
		case EImmortalManagementFeature::Sect: return TEXT("Sect");
		case EImmortalManagementFeature::WorldBoss: return TEXT("WorldBoss");
		case EImmortalManagementFeature::EndlessDungeon: return TEXT("EndlessDungeon");
		case EImmortalManagementFeature::Pet: return TEXT("Pet");
		case EImmortalManagementFeature::Settings: return TEXT("Settings");
		default: return TEXT("Home");
		}
	}

	EImmortalManagementScene GetSceneForFeature(
		const EImmortalManagementFeature Feature,
		const EImmortalManagementScene CurrentScene)
	{
		switch (Feature)
		{
		case EImmortalManagementFeature::Sect:
		case EImmortalManagementFeature::Cultivation:
		case EImmortalManagementFeature::Alchemy:
		case EImmortalManagementFeature::Crafting:
		case EImmortalManagementFeature::Cave:
		case EImmortalManagementFeature::Farming:
			return EImmortalManagementScene::SectSanctuary;
		case EImmortalManagementFeature::Shop:
			return EImmortalManagementScene::MarketTown;
		case EImmortalManagementFeature::Inventory:
		case EImmortalManagementFeature::Artifact:
		case EImmortalManagementFeature::Technique:
		case EImmortalManagementFeature::CharacterBuild:
		case EImmortalManagementFeature::Pet:
			return EImmortalManagementScene::CaveEstate;
		case EImmortalManagementFeature::Map:
		case EImmortalManagementFeature::Quest:
		case EImmortalManagementFeature::WorldBoss:
		case EImmortalManagementFeature::EndlessDungeon:
			return EImmortalManagementScene::AdventureHall;
		default:
			return CurrentScene;
		}
	}

	FText GetSceneDisplayName(const EImmortalManagementScene Scene)
	{
		switch (Scene)
		{
		case EImmortalManagementScene::MarketTown:
			return FText::FromString(TEXT("仙城坊市"));
		case EImmortalManagementScene::CaveEstate:
			return FText::FromString(TEXT("角色养成"));
		case EImmortalManagementScene::AdventureHall:
			return FText::FromString(TEXT("历练台"));
		default:
			return FText::FromString(TEXT("洞府仙居"));
		}
	}

	FString GetSceneAssetToken(const EImmortalManagementScene Scene)
	{
		switch (Scene)
		{
		case EImmortalManagementScene::MarketTown: return TEXT("MarketTown");
		case EImmortalManagementScene::CaveEstate: return TEXT("CaveEstate");
		case EImmortalManagementScene::AdventureHall: return TEXT("AdventureHall");
		default: return TEXT("SectSanctuary");
		}
	}

	EManagementWorldTier GetWorldTier(
		const EImmortalCultivationRealm Realm)
	{
		if (Realm <= EImmortalCultivationRealm::GoldenCore)
		{
			return EManagementWorldTier::Mortal;
		}
		if (Realm <= EImmortalCultivationRealm::VoidRefining)
		{
			return EManagementWorldTier::Spirit;
		}
		return EManagementWorldTier::Immortal;
	}

	FString GetTierAssetToken(const EManagementWorldTier Tier)
	{
		switch (Tier)
		{
		case EManagementWorldTier::Spirit: return TEXT("Spirit");
		case EManagementWorldTier::Immortal: return TEXT("Immortal");
		default: return TEXT("Mortal");
		}
	}

	FText GetTierDisplayName(const EManagementWorldTier Tier)
	{
		switch (Tier)
		{
		case EManagementWorldTier::Spirit:
			return FText::FromString(TEXT("灵界"));
		case EManagementWorldTier::Immortal:
			return FText::FromString(TEXT("仙界"));
		default:
			return FText::FromString(TEXT("人界"));
		}
	}

	FLinearColor GetFeatureFallbackAccent(
		const EImmortalManagementFeature Feature)
	{
		switch (Feature)
		{
		case EImmortalManagementFeature::Cultivation: return FLinearColor(0.20f, 0.58f, 0.42f, 1.0f);
		case EImmortalManagementFeature::Inventory: return FLinearColor(0.30f, 0.46f, 0.72f, 1.0f);
		case EImmortalManagementFeature::Alchemy: return FLinearColor(0.55f, 0.24f, 0.18f, 1.0f);
		case EImmortalManagementFeature::Crafting: return FLinearColor(0.58f, 0.38f, 0.14f, 1.0f);
		case EImmortalManagementFeature::Artifact: return FLinearColor(0.42f, 0.26f, 0.62f, 1.0f);
		case EImmortalManagementFeature::Technique: return FLinearColor(0.18f, 0.42f, 0.62f, 1.0f);
		case EImmortalManagementFeature::CharacterBuild: return FLinearColor(0.48f, 0.30f, 0.54f, 1.0f);
		case EImmortalManagementFeature::Shop: return FLinearColor(0.62f, 0.48f, 0.16f, 1.0f);
		case EImmortalManagementFeature::Map: return FLinearColor(0.18f, 0.50f, 0.56f, 1.0f);
		case EImmortalManagementFeature::Quest: return FLinearColor(0.34f, 0.54f, 0.26f, 1.0f);
		case EImmortalManagementFeature::Cave: return FLinearColor(0.24f, 0.36f, 0.46f, 1.0f);
		case EImmortalManagementFeature::Farming: return FLinearColor(0.36f, 0.58f, 0.20f, 1.0f);
		case EImmortalManagementFeature::Sect: return FLinearColor(0.40f, 0.36f, 0.62f, 1.0f);
		case EImmortalManagementFeature::WorldBoss: return FLinearColor(0.64f, 0.16f, 0.14f, 1.0f);
		case EImmortalManagementFeature::EndlessDungeon: return FLinearColor(0.16f, 0.26f, 0.48f, 1.0f);
		case EImmortalManagementFeature::Pet: return FLinearColor(0.50f, 0.34f, 0.24f, 1.0f);
		case EImmortalManagementFeature::Settings: return FLinearColor(0.32f, 0.38f, 0.42f, 1.0f);
		default: return FLinearColor(0.30f, 0.48f, 0.38f, 1.0f);
		}
	}

	FLinearColor GetTierPlaceholderColor(
		const EManagementWorldTier Tier,
		const EImmortalManagementFeature Feature)
	{
		FLinearColor Base;
		switch (Tier)
		{
		case EManagementWorldTier::Spirit:
			Base = FLinearColor(0.035f, 0.105f, 0.145f, 1.0f);
			break;
		case EManagementWorldTier::Immortal:
			Base = FLinearColor(0.135f, 0.075f, 0.155f, 1.0f);
			break;
		default:
			Base = FLinearColor(0.075f, 0.120f, 0.075f, 1.0f);
			break;
		}
		const FLinearColor Accent = GetFeatureFallbackAccent(Feature);
		return FLinearColor(
			FMath::Lerp(Base.R, Accent.R, 0.36f),
			FMath::Lerp(Base.G, Accent.G, 0.36f),
			FMath::Lerp(Base.B, Accent.B, 0.36f),
			1.0f);
	}

	FString BuildThemeAssetPath(
		const EManagementWorldTier Tier,
		const EImmortalManagementScene Scene)
	{
		const FString TierToken = GetTierAssetToken(Tier);
		const FString AssetName = FString::Printf(
			TEXT("T_BG_%s_%s"),
			*TierToken,
			*GetSceneAssetToken(Scene));
		return FString::Printf(
			TEXT("/Game/GAME/Asset/ui/management/backgrounds/%s/%s.%s"),
			*TierToken.ToLower(),
			*AssetName,
			*AssetName);
	}

	FString BuildHotspotAssetPath(
		const EManagementWorldTier Tier,
		const EImmortalManagementFeature Feature)
	{
		const FString TierToken = GetTierAssetToken(Tier);
		const FString AssetName = FString::Printf(
			TEXT("T_Hotspot_%s_%s"),
			*TierToken,
			*GetFeatureAssetToken(Feature));
		return FString::Printf(
			TEXT("/Game/GAME/Asset/ui/management/hotspots/%s/%s.%s"),
			*TierToken.ToLower(),
			*AssetName,
			*AssetName);
	}

	UTextBlock* AddManagementLabel(
		UWidgetTree* Tree,
		UButton* Button,
		const FText& Label,
		const int32 FontSize)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass());
		Text->SetText(Label);
		StyleManagementText(
			Text,
			FontSize,
			FLinearColor(0.96f, 0.92f, 0.78f, 1.0f),
			true);
		Button->AddChild(Text);
		return Text;
	}
}

void UImmortalManagementWidget::InitializeForPlayer(
	AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
	LastThemeRealm = MAX_uint8;
	RefreshTheme();
}

void UImmortalManagementWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("ManagementLogicalSize"));
	RootSize->SetWidthOverride(1707.0f);
	RootSize->SetHeightOverride(320.0f);
	WidgetTree->RootWidget = RootSize;

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("ManagementRootCanvas"));
	RootSize->AddChild(RootCanvas);

	ThemePlaceholder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("ManagementThemePlaceholder"));
	ThemePlaceholder->SetPadding(FMargin(0.0f));
	ThemePlaceholder->SetBrush(ImmortalUITheme::PanelBrush(FLinearColor(0.035f, 0.055f, 0.055f)));
	ThemePlaceholder->SetVisibility(ESlateVisibility::HitTestInvisible);
	SetManagementLayout(
		RootCanvas->AddChildToCanvas(ThemePlaceholder),
		FVector2D::ZeroVector,
		FVector2D(1707.0f, 320.0f));

	ThemeImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("ManagementThemeImage"));
	ThemeImage->SetVisibility(ESlateVisibility::Collapsed);
	SetManagementLayout(
		RootCanvas->AddChildToCanvas(ThemeImage),
		FVector2D(8.0f, 46.0f),
		FVector2D(1691.0f, 266.0f));

	UBorder* ReadabilityOverlay = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("ManagementReadabilityOverlay"));
	ReadabilityOverlay->SetBrushColor(
		FLinearColor(0.008f, 0.012f, 0.016f, 0.0f));
	ReadabilityOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	ReadabilityOverlay->SetPadding(FMargin(0.0f));
	SetManagementLayout(
		RootCanvas->AddChildToCanvas(ReadabilityOverlay),
		FVector2D::ZeroVector,
		FVector2D(1707.0f, 320.0f));

	PageTitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ManagementPageTitle"));
	ThemeStatusText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ManagementThemeStatus"));

	PageSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(
		UWidgetSwitcher::StaticClass(), TEXT("ManagementFeatureSwitcher"));
	SetManagementLayout(
		RootCanvas->AddChildToCanvas(PageSwitcher),
		FVector2D::ZeroVector,
		FVector2D(1707.0f, 320.0f));
	UWidget* HomePage = BuildHomePage();
	PageSwitcher->AddChild(HomePage);
	RegisteredPages.Add(EImmortalManagementFeature::Home, HomePage);
	PageSwitcher->AddChild(BuildMissingPage());
	PageSwitcher->SetActiveWidget(HomePage);

	UButton* ReturnButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("ManagementReturnToAdventure"));
	ReturnButton->OnClicked.AddDynamic(
		this, &ThisClass::HandleReturnToAdventureClicked);
	AddManagementLabel(
		WidgetTree, ReturnButton, FText::FromString(TEXT("返回历练")), 11);

	UBorder* SceneNavigation = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("ManagementSceneNavigation"));
	SceneNavigation->SetBrushColor(
		FLinearColor(0.012f, 0.020f, 0.023f, 0.88f));
	SceneNavigation->SetPadding(FMargin(5.0f));
	SetManagementLayout(
		RootCanvas->AddChildToCanvas(SceneNavigation),
		FVector2D::ZeroVector,
		FVector2D(1707.0f, 42.0f));

	UCanvasPanel* SceneNavigationCanvas =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("ManagementSceneNavigationCanvas"));
	SceneNavigation->AddChild(SceneNavigationCanvas);

	// Re-parent the existing title/status widgets into the slim location strip.
	PageTitleText->RemoveFromParent();
	StyleManagementText(
		PageTitleText,
		17,
		FLinearColor(1.0f, 0.88f, 0.56f, 1.0f));
	SetManagementLayout(
		SceneNavigationCanvas->AddChildToCanvas(PageTitleText),
		FVector2D(8.0f, 2.0f),
		FVector2D(205.0f, 26.0f));

	ThemeStatusText->RemoveFromParent();
	ThemeStatusText->SetAutoWrapText(false);
	StyleManagementText(
		ThemeStatusText,
		9,
		FLinearColor(0.70f, 0.84f, 0.82f, 1.0f),
		true);
	SetManagementLayout(
		SceneNavigationCanvas->AddChildToCanvas(ThemeStatusText),
		FVector2D(210.0f, 4.0f),
		FVector2D(185.0f, 23.0f));

	auto PlaceSceneButton = [this, SceneNavigationCanvas](
		const EImmortalManagementScene Scene,
		const TCHAR* Label,
		const float X)
	{
		UButton* Button = AddSceneButton(Scene, FText::FromString(Label));
		SetManagementLayout(
			SceneNavigationCanvas->AddChildToCanvas(Button),
			FVector2D(X, 1.0f),
			FVector2D(126.0f, 25.0f));
	};
	PlaceSceneButton(EImmortalManagementScene::SectSanctuary, TEXT("洞府"), 455.0f);
	PlaceSceneButton(EImmortalManagementScene::MarketTown, TEXT("坊市"), 585.0f);
	PlaceSceneButton(EImmortalManagementScene::CaveEstate, TEXT("角色"), 715.0f);
	PlaceSceneButton(EImmortalManagementScene::AdventureHall, TEXT("历练台"), 845.0f);
	UButton* AscensionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ManagementAscension"));
	AscensionButton->SetStyle(ImmortalUITheme::ButtonStyle());
	ImmortalUITheme::IconButton(this, AscensionButton, 0, FText::FromString(TEXT("飞升")));
	AscensionButton->OnClicked.AddDynamic(this, &ThisClass::HandleAscensionClicked);
	SetManagementLayout(SceneNavigationCanvas->AddChildToCanvas(AscensionButton), FVector2D(1224, 1), FVector2D(126, 25));

	BackToSceneButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("ManagementBackToScene"));
	BackToSceneButton->SetStyle(MakeManagementButtonStyle(
		FVector2D(126.0f, 25.0f),
		FLinearColor(0.18f, 0.34f, 0.31f, 0.96f)));
	BackToSceneButton->OnClicked.AddDynamic(
		this, &ThisClass::HandleBackToSceneClicked);
	AddManagementLabel(
		WidgetTree, BackToSceneButton,
		FText::FromString(TEXT("返回场景")), 10);
	ImmortalUITheme::IconButton(this, BackToSceneButton, 12, FText::FromString(TEXT("返回主页")));
	SetManagementLayout(
		SceneNavigationCanvas->AddChildToCanvas(BackToSceneButton),
		FVector2D(984.0f, 1.0f),
		FVector2D(126.0f, 25.0f));

	UButton* SettingsButton = AddNavigationButton(
		EImmortalManagementFeature::Settings,
		FText::FromString(TEXT("设置")));
	SetManagementLayout(
		SceneNavigationCanvas->AddChildToCanvas(SettingsButton),
		FVector2D(1116.0f, 1.0f),
		FVector2D(100.0f, 25.0f));

	ReturnButton->RemoveFromParent();
	ReturnButton->SetStyle(MakeManagementButtonStyle(
		FVector2D(232.0f, 25.0f),
		FLinearColor(0.34f, 0.12f, 0.10f, 1.0f)));
	ImmortalUITheme::IconButton(this, ReturnButton, 8, FText::FromString(TEXT("收起 · 继续历练")));
	SetManagementLayout(
		SceneNavigationCanvas->AddChildToCanvas(ReturnButton),
		FVector2D(1455.0f, 1.0f),
		FVector2D(232.0f, 25.0f));

	NotificationBar = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("ManagementNotificationBar"));
	NotificationBar->SetBrushColor(
		FLinearColor(0.06f, 0.10f, 0.09f, 0.97f));
	NotificationBar->SetPadding(FMargin(10.0f, 2.0f));
	NotificationBar->SetVisibility(ESlateVisibility::Collapsed);
	SetManagementLayout(
		RootCanvas->AddChildToCanvas(NotificationBar),
		FVector2D(400.0f, 328.0f),
		FVector2D(907.0f, 27.0f));

	NotificationText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ManagementNotificationText"));
	NotificationText->SetAutoWrapText(false);
	StyleManagementText(
		NotificationText,
		11,
		FLinearColor::White,
		true);
	NotificationBar->AddChild(NotificationText);

	UpdatePageHeader();
	UpdateNavigationState();
	RefreshTheme();
}

void UImmortalManagementWidget::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TryDisplayPendingNotification();
	if (NotificationExpirySeconds > 0.0
		&& FPlatformTime::Seconds() >= NotificationExpirySeconds)
	{
		ClearNotification();
	}
	if (!Player.IsValid()) return;

	ThemeRefreshAccumulator += FMath::Max(InDeltaTime, 0.0f);
	if (ThemeRefreshAccumulator < 0.5f) return;
	ThemeRefreshAccumulator = 0.0f;

	const uint8 CurrentRealm = static_cast<uint8>(
		Player->GetCultivationRealm());
	if (CurrentRealm != LastThemeRealm)
	{
		RefreshTheme();
	}
}

void UImmortalManagementWidget::RegisterFeaturePage(
	const EImmortalManagementFeature Feature,
	UUserWidget* Page)
{
	if (!Page || !PageSwitcher) return;
	if (Feature == EImmortalManagementFeature::Crafting
		|| Feature == EImmortalManagementFeature::Artifact
		|| Feature == EImmortalManagementFeature::Technique
		|| Feature == EImmortalManagementFeature::CharacterBuild
		|| Feature == EImmortalManagementFeature::Shop
		|| Feature == EImmortalManagementFeature::Map
		|| Feature == EImmortalManagementFeature::Cave)
		ImmortalUITheme::RestyleFeature(Page);

	if (const TObjectPtr<UUserWidget>* ExistingSource =
		RegisteredPageSources.Find(Feature))
	{
		if (*ExistingSource == Page) return;
	}
	if (TObjectPtr<UWidget>* Existing = RegisteredPages.Find(Feature))
	{
		PageSwitcher->RemoveChild(*Existing);
	}

	Page->RemoveFromParent();
	UScaleBox* PageScale = WidgetTree->ConstructWidget<UScaleBox>(
		UScaleBox::StaticClass());
	PageScale->SetStretch(EStretch::ScaleToFit);
	PageScale->SetStretchDirection(EStretchDirection::Both);
	PageScale->AddChild(Page);
	UCanvasPanel* PageContainer = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass());
	SetManagementLayout(
		PageContainer->AddChildToCanvas(PageScale),
		FVector2D(12.0f, 46.0f), FVector2D(1683.0f, 270.0f));
	PageSwitcher->AddChild(PageContainer);
	RegisteredPages.Add(Feature, PageContainer);
	RegisteredPageSources.Add(Feature, Page);
	if (ActiveFeature == Feature)
	{
		PageSwitcher->SetActiveWidget(PageContainer);
	}
}

void UImmortalManagementWidget::ShowFeature(
	const EImmortalManagementFeature Feature)
{
	if (!PageSwitcher) return;
	ActiveScene = GetSceneForFeature(Feature, ActiveScene);
	ActiveFeature = Feature;
	if (Feature == EImmortalManagementFeature::Home
		&& SceneHubSwitcher)
	{
		if (const TObjectPtr<UWidget>* Hub =
			SceneHubPages.Find(ActiveScene))
		{
			SceneHubSwitcher->SetActiveWidget(*Hub);
		}
	}

	if (const TObjectPtr<UWidget>* Page = RegisteredPages.Find(Feature))
	{
		PageSwitcher->SetActiveWidget(*Page);
	}
	else
	{
		if (MissingPageText)
		{
			MissingPageText->SetText(FText::FromString(FString::Printf(
				TEXT("%s\n\n该功能页面尚未注册。\n统一界面与背景已经就绪，可继续接入现有功能组件。"),
				*GetFeatureDisplayName(Feature).ToString())));
		}
		if (PageSwitcher->GetNumWidgets() > 1)
		{
			PageSwitcher->SetActiveWidgetIndex(1);
		}
	}

	UpdatePageHeader();
	UpdateNavigationState();
	RefreshTheme();
	TryDisplayPendingNotification();
}

void UImmortalManagementWidget::ShowScene(
	const EImmortalManagementScene Scene)
{
	ActiveScene = Scene;
	ActiveFeature = EImmortalManagementFeature::Home;
	if (PageSwitcher)
	{
		if (const TObjectPtr<UWidget>* Home =
			RegisteredPages.Find(EImmortalManagementFeature::Home))
		{
			PageSwitcher->SetActiveWidget(*Home);
		}
	}
	if (SceneHubSwitcher)
	{
		if (const TObjectPtr<UWidget>* Hub = SceneHubPages.Find(Scene))
		{
			SceneHubSwitcher->SetActiveWidget(*Hub);
		}
	}
	UpdatePageHeader();
	UpdateNavigationState();
	RefreshTheme();
	TryDisplayPendingNotification();
}

void UImmortalManagementWidget::QueueNotification(
	const FText Message,
	const FLinearColor Color,
	const float Duration)
{
	if (Message.IsEmpty()) return;
	const float SafeDuration = FMath::Max(Duration, 0.1f);
	if (!IsManagementVisible() || !NotificationBar || !NotificationText)
	{
		PendingNotificationText = Message;
		PendingNotificationColor = Color;
		PendingNotificationDuration = SafeDuration;
		bHasPendingNotification = true;
		return;
	}
	bHasPendingNotification = false;
	PendingNotificationText = FText::GetEmpty();
	DisplayNotification(Message, Color, SafeDuration);
}

void UImmortalManagementWidget::DisplayNotification(
	const FText& Message,
	const FLinearColor& Color,
	const float Duration)
{
	if (!NotificationBar || !NotificationText) return;
	NotificationText->SetText(Message);
	NotificationText->SetColorAndOpacity(FSlateColor(Color));
	NotificationBar->SetBrushColor(FLinearColor(
		FMath::Clamp(Color.R * 0.18f, 0.02f, 0.20f),
		FMath::Clamp(Color.G * 0.18f, 0.02f, 0.20f),
		FMath::Clamp(Color.B * 0.18f, 0.02f, 0.20f),
		0.97f));
	NotificationBar->SetVisibility(ESlateVisibility::HitTestInvisible);
	NotificationExpirySeconds = FPlatformTime::Seconds()
		+ FMath::Max(Duration, 0.1f);
}

void UImmortalManagementWidget::TryDisplayPendingNotification()
{
	if (!bHasPendingNotification || !IsManagementVisible()) return;
	const FText Message = PendingNotificationText;
	const FLinearColor Color = PendingNotificationColor;
	const float Duration = PendingNotificationDuration;
	bHasPendingNotification = false;
	PendingNotificationText = FText::GetEmpty();
	DisplayNotification(Message, Color, Duration);
}

void UImmortalManagementWidget::ClearNotification()
{
	NotificationExpirySeconds = 0.0;
	if (NotificationBar)
	{
		NotificationBar->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UImmortalManagementWidget::IsManagementVisible() const
{
	const ESlateVisibility CurrentVisibility = GetVisibility();
	return IsInViewport()
		&& CurrentVisibility != ESlateVisibility::Collapsed
		&& CurrentVisibility != ESlateVisibility::Hidden;
}

UTexture2D* UImmortalManagementWidget::LoadOptionalTexture(const FString& AssetPath)
{
	if (const TObjectPtr<UTexture2D>* Cached = ThemeTextureCache.Find(AssetPath))
	{
		return Cached->Get();
	}
	// Optional future-world art must not emit load warnings on every visit.
	if (!FPackageName::DoesPackageExist(
		FPackageName::ObjectPathToPackageName(AssetPath)))
	{
		return nullptr;
	}
	UTexture2D* Texture = Cast<UTexture2D>(FSoftObjectPath(AssetPath).TryLoad());
	if (Texture)
	{
		ThemeTextureCache.Add(AssetPath, Texture);
	}
	return Texture;
}

void UImmortalManagementWidget::RefreshTheme()
{
	if (!ThemePlaceholder || !ThemeImage) return;

	const EImmortalCultivationRealm Realm = Player.IsValid()
		? Player->GetCultivationRealm()
		: EImmortalCultivationRealm::QiRefining;
	const EManagementWorldTier Tier = GetWorldTier(Realm);
	LastThemeRealm = static_cast<uint8>(Realm);
	ThemeRefreshAccumulator = 0.0f;
	ThemePlaceholder->SetBrushColor(
		GetTierPlaceholderColor(Tier, EImmortalManagementFeature::Home));

	const FString AssetPath = BuildThemeAssetPath(Tier, ActiveScene);
	UTexture2D* ThemeTexture = LoadOptionalTexture(AssetPath);
	if (ThemeTexture && ActiveFeature == EImmortalManagementFeature::Home)
	{
		ThemeImage->SetBrushFromTexture(ThemeTexture, false);
		ThemeImage->SetColorAndOpacity(FLinearColor::White);
		ThemeImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		ThemeImage->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (ThemeStatusText)
	{
		ThemeStatusText->SetText(FText::FromString(FString::Printf(
			TEXT("%s · %s%s"),
			*GetTierDisplayName(Tier).ToString(),
			*GetSceneDisplayName(ActiveScene).ToString(),
			ThemeTexture ? TEXT("") : TEXT("（美术占位）"))));
	}
}

void UImmortalManagementWidget::RequestFeature(
	const EImmortalManagementFeature Feature)
{
	if (Player.IsValid())
	{
		Player->OpenManagementFeature(Feature);
		return;
	}
	ShowFeature(Feature);
}

void UImmortalManagementWidget::RequestScene(
	const EImmortalManagementScene Scene)
{
	// Set the target scene before asking the player to open Home so the player
	// remains the single authority for death-recovery redirects.
	ActiveScene = Scene;
	if (Player.IsValid())
	{
		Player->OpenManagementFeature(EImmortalManagementFeature::Home);
		return;
	}
	ShowScene(Scene);
}

void UImmortalManagementWidget::UpdateNavigationState()
{
	for (const TPair<EImmortalManagementFeature, TObjectPtr<UButton>>& Entry
		: NavigationButtons)
	{
		if (!Entry.Value) continue;
		const bool bSelected = Entry.Key == ActiveFeature;
		Entry.Value->SetStyle(MakeManagementButtonStyle(
			FVector2D(130.0f, 21.0f),
			bSelected
				? FLinearColor(0.47f, 0.31f, 0.09f, 1.0f)
				: FLinearColor(0.075f, 0.13f, 0.15f, 0.96f)));
	}
	for (const TPair<EImmortalManagementScene, TObjectPtr<UButton>>& Entry
		: SceneButtons)
	{
		if (!Entry.Value) continue;
		const bool bSelected = Entry.Key == ActiveScene;
		Entry.Value->SetStyle(MakeManagementButtonStyle(
			FVector2D(126.0f, 25.0f),
			bSelected
				? FLinearColor(0.47f, 0.31f, 0.09f, 1.0f)
				: FLinearColor(0.075f, 0.13f, 0.15f, 0.96f)));
	}
	if (BackToSceneButton)
	{
		BackToSceneButton->SetVisibility(
			ActiveFeature == EImmortalManagementFeature::Home
				? ESlateVisibility::Collapsed
				: ESlateVisibility::Visible);
	}
}

void UImmortalManagementWidget::UpdatePageHeader()
{
	if (PageTitleText)
	{
		PageTitleText->SetText(
			ActiveFeature == EImmortalManagementFeature::Home
				? GetSceneDisplayName(ActiveScene)
				: GetFeatureDisplayName(ActiveFeature));
	}
}

UWidget* UImmortalManagementWidget::BuildHomePage()
{
	UCanvasPanel* Page = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("ManagementSceneHome"));

	SceneHubSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(
		UWidgetSwitcher::StaticClass(), TEXT("ManagementSceneHubSwitcher"));
	SetManagementLayout(
		Page->AddChildToCanvas(SceneHubSwitcher),
		FVector2D::ZeroVector,
		FVector2D(1707.0f, 320.0f));

	const EImmortalManagementScene Scenes[] =
	{
		EImmortalManagementScene::SectSanctuary,
		EImmortalManagementScene::MarketTown,
		EImmortalManagementScene::CaveEstate,
		EImmortalManagementScene::AdventureHall
	};
	for (const EImmortalManagementScene Scene : Scenes)
	{
		UWidget* Hub = BuildSceneHub(Scene);
		SceneHubSwitcher->AddChild(Hub);
		SceneHubPages.Add(Scene, Hub);
	}
	if (const TObjectPtr<UWidget>* InitialHub =
		SceneHubPages.Find(ActiveScene))
	{
		SceneHubSwitcher->SetActiveWidget(*InitialHub);
	}
	return Page;
}

UWidget* UImmortalManagementWidget::BuildSceneHub(
	const EImmortalManagementScene Scene)
{
	UCanvasPanel* Hub = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		FName(*FString::Printf(
			TEXT("ManagementSceneHub_%s"),
			*GetSceneAssetToken(Scene))));

	UTextBlock* SceneHeading = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass());
	SceneHeading->SetText(GetSceneDisplayName(Scene));
	StyleManagementText(
		SceneHeading,
		23,
		FLinearColor(1.0f, 0.88f, 0.56f, 1.0f));
	SetManagementLayout(
		Hub->AddChildToCanvas(SceneHeading),
		FVector2D(28.0f, 52.0f),
		FVector2D(300.0f, 34.0f));
	SceneHeading->SetVisibility(ESlateVisibility::Collapsed);

	UTextBlock* SceneHint = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass());
	SceneHint->SetText(FText::FromString(TEXT(
		"点击场景中的建筑或物件进入功能；离开历练画面不会暂停后台挂机。")));
	StyleManagementText(
		SceneHint,
		10,
		FLinearColor(0.78f, 0.90f, 0.82f, 1.0f));
	SetManagementLayout(
		Hub->AddChildToCanvas(SceneHint),
		FVector2D(28.0f, 82.0f),
		FVector2D(590.0f, 24.0f));
	SceneHint->SetVisibility(ESlateVisibility::Collapsed);

	auto Place = [this, Hub](
		const EImmortalManagementFeature Feature,
		const TCHAR* Label,
		const float X,
		const float Y,
		const float Width,
		const float Height)
	{
		UButton* Hotspot = AddSceneHotspot(
			Feature,
			FText::FromString(Label),
			FVector2D(X, Y),
			FVector2D(Width, Height));
		SetManagementLayout(
			Hub->AddChildToCanvas(Hotspot),
			FVector2D(X, Y),
			FVector2D(Width, Height));
	};

	switch (Scene)
	{
	case EImmortalManagementScene::SectSanctuary:
		Place(EImmortalManagementFeature::Cultivation, TEXT("修炼静室"), 24, 80, 265, 208);
		Place(EImmortalManagementFeature::Sect, TEXT("宗门大殿"), 302, 80, 265, 208);
		Place(EImmortalManagementFeature::Alchemy, TEXT("炼丹房"), 580, 80, 265, 208);
		Place(EImmortalManagementFeature::Crafting, TEXT("炼器坊"), 858, 80, 265, 208);
		Place(EImmortalManagementFeature::Cave, TEXT("洞府营造"), 1136, 80, 265, 208);
		Place(EImmortalManagementFeature::Farming, TEXT("灵田"), 1414, 80, 265, 208);
		break;
	case EImmortalManagementScene::MarketTown:
		Place(EImmortalManagementFeature::Shop, TEXT("百宝阁 · 购买 / 出售"), 565, 80, 577, 208);
		break;
	case EImmortalManagementScene::CaveEstate:
		Place(EImmortalManagementFeature::Inventory, TEXT("储物戒 / 装备"), 24, 80, 319, 208);
		Place(EImmortalManagementFeature::Artifact, TEXT("法宝楼"), 359, 80, 319, 208);
		Place(EImmortalManagementFeature::Technique, TEXT("藏经阁"), 694, 80, 319, 208);
		Place(EImmortalManagementFeature::CharacterBuild, TEXT("测灵台"), 1029, 80, 319, 208);
		Place(EImmortalManagementFeature::Pet, TEXT("灵兽园"), 1364, 80, 319, 208);
		break;
	case EImmortalManagementScene::AdventureHall:
		Place(EImmortalManagementFeature::Map, TEXT("山河图"), 25.0f, 68.0f, 390.0f, 220.0f);
		Place(EImmortalManagementFeature::Quest, TEXT("任务玉简"), 415.0f, 68.0f, 360.0f, 220.0f);
		Place(EImmortalManagementFeature::WorldBoss, TEXT("世界妖王"), 775.0f, 60.0f, 510.0f, 228.0f);
		Place(EImmortalManagementFeature::EndlessDungeon, TEXT("无尽秘境"), 1285.0f, 60.0f, 397.0f, 228.0f);
		break;
	}
	return Hub;
}

UWidget* UImmortalManagementWidget::BuildMissingPage()
{
	UBorder* Page = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("ManagementMissingPage"));
	Page->SetBrushColor(FLinearColor(0.055f, 0.045f, 0.035f, 0.78f));
	Page->SetPadding(FMargin(40.0f));

	MissingPageText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ManagementMissingPageText"));
	MissingPageText->SetAutoWrapText(true);
	StyleManagementText(
		MissingPageText,
		17,
		FLinearColor(0.94f, 0.82f, 0.58f, 1.0f),
		true);
	Page->AddChild(MissingPageText);
	return Page;
}

UButton* UImmortalManagementWidget::AddNavigationButton(
	const EImmortalManagementFeature Feature,
	const FText& Label)
{
	const FName ButtonName(*FString::Printf(
		TEXT("ManagementNav_%s"),
		*GetFeatureAssetToken(Feature)));
	UButton* Button = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), ButtonName);
	Button->SetStyle(MakeManagementButtonStyle(
		FVector2D(130.0f, 21.0f),
		FLinearColor(0.075f, 0.13f, 0.15f, 0.96f)));
	ImmortalUITheme::IconButton(this, Button,
		FMath::Max(static_cast<int32>(Feature)-1, 0), Label);

	switch (Feature)
	{
	case EImmortalManagementFeature::Home:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleHomeClicked);
		break;
	case EImmortalManagementFeature::Cultivation:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleCultivationClicked);
		break;
	case EImmortalManagementFeature::Inventory:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleInventoryClicked);
		break;
	case EImmortalManagementFeature::Alchemy:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleAlchemyClicked);
		break;
	case EImmortalManagementFeature::Crafting:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleCraftingClicked);
		break;
	case EImmortalManagementFeature::Artifact:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleArtifactClicked);
		break;
	case EImmortalManagementFeature::Technique:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleTechniqueClicked);
		break;
	case EImmortalManagementFeature::CharacterBuild:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleCharacterBuildClicked);
		break;
	case EImmortalManagementFeature::Shop:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleShopClicked);
		break;
	case EImmortalManagementFeature::Map:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleMapClicked);
		break;
	case EImmortalManagementFeature::Quest:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleQuestClicked);
		break;
	case EImmortalManagementFeature::Cave:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleCaveClicked);
		break;
	case EImmortalManagementFeature::Farming:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleFarmingClicked);
		break;
	case EImmortalManagementFeature::Sect:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleSectClicked);
		break;
	case EImmortalManagementFeature::WorldBoss:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleWorldBossClicked);
		break;
	case EImmortalManagementFeature::EndlessDungeon:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleEndlessDungeonClicked);
		break;
	case EImmortalManagementFeature::Pet:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandlePetClicked);
		break;
	case EImmortalManagementFeature::Settings:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleSettingsClicked);
		break;
	}

	NavigationButtons.Add(Feature, Button);
	return Button;
}

UButton* UImmortalManagementWidget::AddSceneButton(
	const EImmortalManagementScene Scene,
	const FText& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		FName(*FString::Printf(
			TEXT("ManagementScene_%s"),
			*GetSceneAssetToken(Scene))));
	Button->SetStyle(MakeManagementButtonStyle(
		FVector2D(126.0f, 25.0f),
		FLinearColor(0.075f, 0.13f, 0.15f, 0.96f)));
	const int32 SceneIcon = Scene == EImmortalManagementScene::SectSanctuary ? 12
		: Scene == EImmortalManagementScene::MarketTown ? 7
		: Scene == EImmortalManagementScene::CaveEstate ? 10 : 8;
	ImmortalUITheme::IconButton(this, Button, SceneIcon, Label);
	switch (Scene)
	{
	case EImmortalManagementScene::MarketTown:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleMarketSceneClicked);
		break;
	case EImmortalManagementScene::CaveEstate:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleCaveSceneClicked);
		break;
	case EImmortalManagementScene::AdventureHall:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleAdventureSceneClicked);
		break;
	default:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleSectSceneClicked);
		break;
	}
	SceneButtons.Add(Scene, Button);
	return Button;
}

UButton* UImmortalManagementWidget::AddSceneHotspot(
	const EImmortalManagementFeature Feature,
	const FText& Label,
	const FVector2D& Position,
	const FVector2D& Size)
{
	(void)Position;
	UButton* Button = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		FName(*FString::Printf(
			TEXT("ManagementHotspot_%s"),
			*GetFeatureAssetToken(Feature))));
	FLinearColor HotspotColor = GetFeatureFallbackAccent(Feature);
	FButtonStyle HotspotStyle;
	FLinearColor NormalColor(0.035f, 0.055f, 0.055f, 1.0f);
	FLinearColor HoveredColor = HotspotColor;
	HoveredColor.A = 1.0f;
	FLinearColor PressedColor = HotspotColor;
	PressedColor.A = 1.0f;
	FSlateBrush NormalBrush = MakeManagementBrush(Size, NormalColor);
	NormalBrush.OutlineSettings.Width = 2;
	HotspotStyle.SetNormal(NormalBrush);
	HotspotStyle.SetHovered(MakeManagementBrush(Size, HoveredColor));
	HotspotStyle.SetPressed(MakeManagementBrush(Size, PressedColor));
	HotspotStyle.SetDisabled(MakeManagementBrush(
		Size, FLinearColor(0.05f, 0.06f, 0.07f, 0.10f)));
	Button->SetStyle(HotspotStyle);

	UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass());
	Button->AddChild(Content);

	const float IconSize = 76.0f;
	UImmortalIconWidget* Icon = CreateWidget<UImmortalIconWidget>(this);
	Icon->SetIcon(FMath::Max(static_cast<int32>(Feature)-1, 0));
	Button->SetToolTipText(Label);
	SetManagementLayout(
		Content->AddChildToCanvas(Icon),
		FVector2D((Size.X - IconSize) * 0.5f, FMath::Max((Size.Y - 110.0f) * 0.5f, 24.0f)),
		FVector2D(IconSize, IconSize));

	const float LabelWidth = FMath::Min(Size.X - 18.0f, 180.0f);
	UBorder* LabelPlate = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass());
	LabelPlate->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.026f, 0.82f));
	LabelPlate->SetPadding(FMargin(6.0f, 1.0f));
	SetManagementLayout(
		Content->AddChildToCanvas(LabelPlate),
		FVector2D((Size.X - LabelWidth) * 0.5f, Size.Y - 31.0f),
		FVector2D(LabelWidth, 27.0f));

	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass());
	LabelText->SetText(Label);
	StyleManagementText(
		LabelText,
		13,
		FLinearColor(1.0f, 0.94f, 0.75f, 1.0f),
		true);
	LabelPlate->AddChild(LabelText);

	switch (Feature)
	{
	case EImmortalManagementFeature::Cultivation:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleCultivationClicked);
		break;
	case EImmortalManagementFeature::Inventory:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleInventoryClicked);
		break;
	case EImmortalManagementFeature::Alchemy:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleAlchemyClicked);
		break;
	case EImmortalManagementFeature::Crafting:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleCraftingClicked);
		break;
	case EImmortalManagementFeature::Artifact:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleArtifactClicked);
		break;
	case EImmortalManagementFeature::Technique:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleTechniqueClicked);
		break;
	case EImmortalManagementFeature::CharacterBuild:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleCharacterBuildClicked);
		break;
	case EImmortalManagementFeature::Shop:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleShopClicked);
		break;
	case EImmortalManagementFeature::Map:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleMapClicked);
		break;
	case EImmortalManagementFeature::Quest:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleQuestClicked);
		break;
	case EImmortalManagementFeature::Cave:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleCaveClicked);
		break;
	case EImmortalManagementFeature::Farming:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleFarmingClicked);
		break;
	case EImmortalManagementFeature::Sect:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleSectClicked);
		break;
	case EImmortalManagementFeature::WorldBoss:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleWorldBossClicked);
		break;
	case EImmortalManagementFeature::EndlessDungeon:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandleEndlessDungeonClicked);
		break;
	case EImmortalManagementFeature::Pet:
		Button->OnClicked.AddDynamic(this, &ThisClass::HandlePetClicked);
		break;
	default:
		break;
	}
	return Button;
}

void UImmortalManagementWidget::HandleSectSceneClicked()
{
	RequestScene(EImmortalManagementScene::SectSanctuary);
}

void UImmortalManagementWidget::HandleMarketSceneClicked()
{
	RequestScene(EImmortalManagementScene::MarketTown);
}

void UImmortalManagementWidget::HandleCaveSceneClicked()
{
	RequestScene(EImmortalManagementScene::CaveEstate);
}

void UImmortalManagementWidget::HandleAscensionClicked()
{
	if (Player.IsValid()) Player->ToggleAscension();
}

void UImmortalManagementWidget::HandleAdventureSceneClicked()
{
	RequestScene(EImmortalManagementScene::AdventureHall);
}

void UImmortalManagementWidget::HandleBackToSceneClicked()
{
	RequestFeature(EImmortalManagementFeature::Home);
}

void UImmortalManagementWidget::HandleHomeClicked()
{
	RequestFeature(EImmortalManagementFeature::Home);
}

void UImmortalManagementWidget::HandleCultivationClicked()
{
	RequestFeature(EImmortalManagementFeature::Cultivation);
}

void UImmortalManagementWidget::HandleInventoryClicked()
{
	RequestFeature(EImmortalManagementFeature::Inventory);
}

void UImmortalManagementWidget::HandleAlchemyClicked()
{
	RequestFeature(EImmortalManagementFeature::Alchemy);
}

void UImmortalManagementWidget::HandleCraftingClicked()
{
	RequestFeature(EImmortalManagementFeature::Crafting);
}

void UImmortalManagementWidget::HandleArtifactClicked()
{
	RequestFeature(EImmortalManagementFeature::Artifact);
}

void UImmortalManagementWidget::HandleTechniqueClicked()
{
	RequestFeature(EImmortalManagementFeature::Technique);
}

void UImmortalManagementWidget::HandleCharacterBuildClicked()
{
	RequestFeature(EImmortalManagementFeature::CharacterBuild);
}

void UImmortalManagementWidget::HandleShopClicked()
{
	RequestFeature(EImmortalManagementFeature::Shop);
}

void UImmortalManagementWidget::HandleMapClicked()
{
	RequestFeature(EImmortalManagementFeature::Map);
}

void UImmortalManagementWidget::HandleQuestClicked()
{
	RequestFeature(EImmortalManagementFeature::Quest);
}

void UImmortalManagementWidget::HandleCaveClicked()
{
	RequestFeature(EImmortalManagementFeature::Cave);
}

void UImmortalManagementWidget::HandleFarmingClicked()
{
	RequestFeature(EImmortalManagementFeature::Farming);
}

void UImmortalManagementWidget::HandleSectClicked()
{
	RequestFeature(EImmortalManagementFeature::Sect);
}

void UImmortalManagementWidget::HandleWorldBossClicked()
{
	RequestFeature(EImmortalManagementFeature::WorldBoss);
}

void UImmortalManagementWidget::HandleEndlessDungeonClicked()
{
	RequestFeature(EImmortalManagementFeature::EndlessDungeon);
}

void UImmortalManagementWidget::HandlePetClicked()
{
	RequestFeature(EImmortalManagementFeature::Pet);
}

void UImmortalManagementWidget::HandleSettingsClicked()
{
	RequestFeature(EImmortalManagementFeature::Settings);
}

void UImmortalManagementWidget::HandleReturnToAdventureClicked()
{
	if (Player.IsValid())
	{
		Player->CloseManagementInterface();
	}
}
