// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalMonsterHealthWidget.h"

#include "../Characters/ImmortalMonsterCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

namespace
{
	FSlateBrush MakeMonsterBrush(const TCHAR* AssetPath, const FVector2D Size, const FLinearColor FallbackColor)
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
}

void UImmortalMonsterHealthWidget::InitializeForMonster(AImmortalMonsterCharacter* InMonster)
{
	Monster = InMonster;
}

void UImmortalMonsterHealthWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	const FVector2D BarSize(256.0f, 32.0f);
	USizeBox* RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MonsterBarSize"));
	RootBox->SetWidthOverride(BarSize.X);
	RootBox->SetHeightOverride(BarSize.Y);
	WidgetTree->RootWidget = RootBox;

	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MonsterBarLayers"));
	RootBox->AddChild(Layers);

	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MonsterBarBackground"));
	Background->SetBrush(MakeMonsterBrush(TEXT("/Game/GAME/Asset/ui/monster_bar/background.background"), BarSize, FLinearColor(0.03f, 0.03f, 0.03f, 0.9f)));
	Layers->AddChildToOverlay(Background);

	HealthProgress = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("MonsterHealth"));
	FProgressBarStyle HealthStyle;
	FSlateBrush TransparentBrush;
	TransparentBrush.DrawAs = ESlateBrushDrawType::Image;
	TransparentBrush.ImageSize = BarSize;
	TransparentBrush.TintColor = FSlateColor(FLinearColor::Transparent);
	HealthStyle.SetBackgroundImage(TransparentBrush);
	HealthStyle.SetFillImage(MakeMonsterBrush(TEXT("/Game/GAME/Asset/ui/monster_bar/health_fill.health_fill"), BarSize, FLinearColor(0.8f, 0.04f, 0.04f, 1.0f)));
	HealthStyle.SetMarqueeImage(TransparentBrush);
	HealthProgress->SetWidgetStyle(HealthStyle);
	HealthProgress->SetBarFillType(EProgressBarFillType::LeftToRight);
	Layers->AddChildToOverlay(HealthProgress);

	UImage* Border = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MonsterBarBorder"));
	Border->SetBrush(MakeMonsterBrush(TEXT("/Game/GAME/Asset/ui/monster_bar/border.border"), BarSize, FLinearColor(0.8f, 0.8f, 0.8f, 1.0f)));
	Layers->AddChildToOverlay(Border);
}

void UImmortalMonsterHealthWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Monster.IsValid() || Monster->IsDead())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
	HealthProgress->SetPercent(FMath::Clamp(Monster->GetHealthPercent(), 0.0f, 1.0f));
}
