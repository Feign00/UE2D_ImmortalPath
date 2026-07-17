// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPlayerStatusWidget.h"

#include "../Characters/ImmortalPlayerCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

namespace
{
	FSlateBrush MakeStatusBrush(const TCHAR* AssetPath, const FVector2D Size, const FLinearColor FallbackColor)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, AssetPath))
		{
			Brush.SetResourceObject(Texture);
			Brush.TintColor = FSlateColor(FLinearColor::White);
		}
		else
		{
			Brush.TintColor = FSlateColor(FallbackColor);
		}
		return Brush;
	}

	FSlateBrush MakeTransparentBrush(const FVector2D Size)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		Brush.TintColor = FSlateColor(FLinearColor::Transparent);
		return Brush;
	}
}

void UImmortalPlayerStatusWidget::InitializeForPlayer(AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
}

void UImmortalPlayerStatusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	const FVector2D BarSize(512.0f, 64.0f);
	USizeBox* RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PlayerStatusSize"));
	RootBox->SetWidthOverride(820.0f);
	RootBox->SetHeightOverride(BarSize.Y);
	WidgetTree->RootWidget = RootBox;

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayerStatusCanvas"));
	RootBox->AddChild(Canvas);

	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PlayerStatusLayers"));
	if (UCanvasPanelSlot* LayersSlot = Canvas->AddChildToCanvas(Layers))
	{
		LayersSlot->SetPosition(FVector2D::ZeroVector);
		LayersSlot->SetSize(BarSize);
	}

	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PlayerStatusBackground"));
	Background->SetBrush(MakeStatusBrush(TEXT("/Game/GAME/Asset/ui/player_bar/background.background"), BarSize, FLinearColor(0.03f, 0.03f, 0.04f, 0.9f)));
	Layers->AddChildToOverlay(Background);

	HealthProgress = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PlayerHealth"));
	FProgressBarStyle HealthStyle;
	HealthStyle.SetBackgroundImage(MakeTransparentBrush(BarSize));
	HealthStyle.SetFillImage(MakeStatusBrush(TEXT("/Game/GAME/Asset/ui/player_bar/health_fill.health_fill"), BarSize, FLinearColor(0.75f, 0.03f, 0.03f, 1.0f)));
	HealthStyle.SetMarqueeImage(MakeTransparentBrush(BarSize));
	HealthProgress->SetWidgetStyle(HealthStyle);
	HealthProgress->SetBarFillType(EProgressBarFillType::LeftToRight);
	Layers->AddChildToOverlay(HealthProgress);

	ManaProgress = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PlayerMana"));
	FProgressBarStyle ManaStyle;
	ManaStyle.SetBackgroundImage(MakeTransparentBrush(BarSize));
	ManaStyle.SetFillImage(MakeStatusBrush(TEXT("/Game/GAME/Asset/ui/player_bar/mana_fill.mana_fill"), BarSize, FLinearColor(0.03f, 0.25f, 0.9f, 1.0f)));
	ManaStyle.SetMarqueeImage(MakeTransparentBrush(BarSize));
	ManaProgress->SetWidgetStyle(ManaStyle);
	ManaProgress->SetBarFillType(EProgressBarFillType::LeftToRight);
	Layers->AddChildToOverlay(ManaProgress);

	UImage* Border = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PlayerStatusBorder"));
	Border->SetBrush(MakeStatusBrush(TEXT("/Game/GAME/Asset/ui/player_bar/border.border"), BarSize, FLinearColor(0.85f, 0.75f, 0.45f, 1.0f)));
	Layers->AddChildToOverlay(Border);

	UButton* InventoryButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("InventoryOpenButton"));
	InventoryButton->OnClicked.AddDynamic(this, &UImmortalPlayerStatusWidget::HandleInventoryClicked);
	const FSlateBrush InventoryBrush = MakeStatusBrush(
		TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal"),
		FVector2D(88.0f, 64.0f),
		FLinearColor(0.12f, 0.12f, 0.15f, 0.95f));
	FButtonStyle InventoryStyle;
	InventoryStyle.SetNormal(InventoryBrush);
	InventoryStyle.SetHovered(InventoryBrush);
	InventoryStyle.SetPressed(InventoryBrush);
	InventoryButton->SetStyle(InventoryStyle);
	if (UCanvasPanelSlot* ButtonSlot = Canvas->AddChildToCanvas(InventoryButton))
	{
		ButtonSlot->SetPosition(FVector2D(526.0f, 0.0f));
		ButtonSlot->SetSize(FVector2D(88.0f, 64.0f));
	}

	UTextBlock* InventoryLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventoryButtonLabel"));
	InventoryLabel->SetText(FText::FromString(TEXT("背包 [I]")));
	InventoryLabel->SetJustification(ETextJustify::Center);
	InventoryLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.82f, 0.42f, 1.0f)));
	InventoryLabel->SetShadowOffset(FVector2D(1.0f, 1.0f));
	FSlateFontInfo LabelFont = InventoryLabel->GetFont();
	LabelFont.Size = 16;
	InventoryLabel->SetFont(LabelFont);
	InventoryButton->AddChild(InventoryLabel);

	UButton* AlchemyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("AlchemyOpenButton"));
	AlchemyButton->OnClicked.AddDynamic(this, &UImmortalPlayerStatusWidget::HandleAlchemyClicked);
	const FSlateBrush AlchemyBrush = MakeStatusBrush(
		TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal"),
		FVector2D(94.0f, 64.0f),
		FLinearColor(0.12f, 0.08f, 0.15f, 0.95f));
	FButtonStyle AlchemyStyle;
	AlchemyStyle.SetNormal(AlchemyBrush);
	AlchemyStyle.SetHovered(AlchemyBrush);
	AlchemyStyle.SetPressed(AlchemyBrush);
	AlchemyButton->SetStyle(AlchemyStyle);
	if (UCanvasPanelSlot* ButtonSlot = Canvas->AddChildToCanvas(AlchemyButton))
	{
		ButtonSlot->SetPosition(FVector2D(620.0f, 0.0f));
		ButtonSlot->SetSize(FVector2D(94.0f, 64.0f));
	}

	UTextBlock* AlchemyLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AlchemyButtonLabel"));
	AlchemyLabel->SetText(FText::FromString(TEXT("炼丹 [L]")));
	AlchemyLabel->SetJustification(ETextJustify::Center);
	AlchemyLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 1.0f, 0.72f, 1.0f)));
	AlchemyLabel->SetShadowOffset(FVector2D(1.0f, 1.0f));
	FSlateFontInfo AlchemyFont = AlchemyLabel->GetFont();
	AlchemyFont.Size = 16;
	AlchemyLabel->SetFont(AlchemyFont);
	AlchemyButton->AddChild(AlchemyLabel);

	UButton* CraftingButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CraftingOpenButton"));
	CraftingButton->OnClicked.AddDynamic(this, &UImmortalPlayerStatusWidget::HandleCraftingClicked);
	const FSlateBrush CraftingBrush = MakeStatusBrush(
		TEXT("/Game/GAME/Asset/ui/inventory/slots/normal.normal"),
		FVector2D(94.0f, 64.0f),
		FLinearColor(0.16f, 0.10f, 0.05f, 0.95f));
	FButtonStyle CraftingStyle;
	CraftingStyle.SetNormal(CraftingBrush);
	CraftingStyle.SetHovered(CraftingBrush);
	CraftingStyle.SetPressed(CraftingBrush);
	CraftingButton->SetStyle(CraftingStyle);
	if (UCanvasPanelSlot* ButtonSlot = Canvas->AddChildToCanvas(CraftingButton))
	{
		ButtonSlot->SetPosition(FVector2D(720.0f, 0.0f));
		ButtonSlot->SetSize(FVector2D(94.0f, 64.0f));
	}
	UTextBlock* CraftingLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CraftingButtonLabel"));
	CraftingLabel->SetText(FText::FromString(TEXT("炼器 [K]")));
	CraftingLabel->SetJustification(ETextJustify::Center);
	CraftingLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.66f, 0.24f, 1.0f)));
	CraftingLabel->SetShadowOffset(FVector2D(1.0f));
	FSlateFontInfo CraftingFont = CraftingLabel->GetFont();
	CraftingFont.Size = 16;
	CraftingLabel->SetFont(CraftingFont);
	CraftingButton->AddChild(CraftingLabel);
}

void UImmortalPlayerStatusWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Player.IsValid())
	{
		HealthProgress->SetPercent(FMath::Clamp(Player->GetHealthPercent(), 0.0f, 1.0f));
		ManaProgress->SetPercent(FMath::Clamp(Player->GetManaPercent(), 0.0f, 1.0f));
	}
}

void UImmortalPlayerStatusWidget::HandleInventoryClicked()
{
	if (Player.IsValid())
	{
		Player->ToggleInventory();
	}
}

void UImmortalPlayerStatusWidget::HandleAlchemyClicked()
{
	if (Player.IsValid())
	{
		Player->ToggleAlchemy();
	}
}

void UImmortalPlayerStatusWidget::HandleCraftingClicked()
{
	if (Player.IsValid())
	{
		Player->ToggleCrafting();
	}
}
