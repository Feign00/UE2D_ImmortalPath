// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPlayerStatusWidget.h"

#include "../Characters/ImmortalPlayerCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

namespace
{
	constexpr float PlayerBarWidth = 512.0f;
	constexpr float PlayerBarHeight = 64.0f;

	FSlateBrush MakeStatusBrush(
		const TCHAR* AssetPath,
		const FVector2D Size,
		const FLinearColor FallbackColor)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		if (UTexture2D* Texture = LoadObject<UTexture2D>(
			nullptr, AssetPath))
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

void UImmortalPlayerStatusWidget::InitializeForPlayer(
	AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
}

void UImmortalPlayerStatusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	const FVector2D BarSize(PlayerBarWidth, PlayerBarHeight);
	USizeBox* RootBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("PlayerHealthBarSize"));
	RootBox->SetWidthOverride(PlayerBarWidth);
	RootBox->SetHeightOverride(PlayerBarHeight);
	WidgetTree->RootWidget = RootBox;

	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), TEXT("PlayerHealthBarLayers"));
	RootBox->AddChild(Layers);

	UImage* Background = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("PlayerHealthBackground"));
	Background->SetBrush(MakeStatusBrush(
		TEXT("/Game/GAME/Asset/ui/player_bar/background.background"),
		BarSize,
		FLinearColor(0.03f, 0.03f, 0.04f, 0.90f)));
	Layers->AddChildToOverlay(Background);

	HealthProgress = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("PlayerHealth"));
	FProgressBarStyle HealthStyle;
	HealthStyle.SetBackgroundImage(MakeTransparentBrush(BarSize));
	HealthStyle.SetFillImage(MakeStatusBrush(
		TEXT("/Game/GAME/Asset/ui/player_bar/health_fill.health_fill"),
		BarSize,
		FLinearColor(0.75f, 0.03f, 0.03f, 1.0f)));
	HealthStyle.SetMarqueeImage(MakeTransparentBrush(BarSize));
	HealthProgress->SetWidgetStyle(HealthStyle);
	HealthProgress->SetBarFillType(EProgressBarFillType::LeftToRight);
	Layers->AddChildToOverlay(HealthProgress);

	UImage* Border = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("PlayerHealthBorder"));
	Border->SetBrush(MakeStatusBrush(
		TEXT("/Game/GAME/Asset/ui/player_bar/border.border"),
		BarSize,
		FLinearColor(0.85f, 0.75f, 0.45f, 1.0f)));
	Layers->AddChildToOverlay(Border);

	// The health bar remains the only visible battle HUD element. Its invisible
	// hit target is the single route into the separate management interface.
	UButton* ManagementHitTarget = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("OpenManagementHitTarget"));
	FButtonStyle TransparentStyle;
	const FSlateBrush TransparentBrush = MakeTransparentBrush(BarSize);
	TransparentStyle.SetNormal(TransparentBrush);
	TransparentStyle.SetHovered(TransparentBrush);
	TransparentStyle.SetPressed(TransparentBrush);
	ManagementHitTarget->SetStyle(TransparentStyle);
	ManagementHitTarget->SetToolTipText(FText::FromString(
		TEXT("\u70B9\u51FB\u8FDB\u5165\u4FEE\u4ED9\u517B\u6210\u754C\u9762")));
	ManagementHitTarget->OnClicked.AddDynamic(
		this,
		&UImmortalPlayerStatusWidget::HandleOpenManagementClicked);
	Layers->AddChildToOverlay(ManagementHitTarget);
}

void UImmortalPlayerStatusWidget::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Player.IsValid() && HealthProgress)
	{
		HealthProgress->SetPercent(FMath::Clamp(
			Player->GetHealthPercent(), 0.0f, 1.0f));
	}
}

void UImmortalPlayerStatusWidget::HandleOpenManagementClicked()
{
	if (Player.IsValid())
	{
		Player->OpenManagementInterface();
	}
}
