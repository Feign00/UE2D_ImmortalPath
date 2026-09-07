// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPlayerStatusWidget.h"
#include "ImmortalUITheme.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

#include "../Characters/ImmortalPlayerCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"


void UImmortalPlayerStatusWidget::InitializeForPlayer(
	AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
}

void UImmortalPlayerStatusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>();
	Root->SetWidthOverride(512); Root->SetHeightOverride(64);
	WidgetTree->RootWidget = Root;
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Root->AddChild(Canvas);
	auto Place = [Canvas](UWidget* W, float X, float Y, float Width, float Height)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(W);
		Slot->SetPosition(FVector2D(X,Y)); Slot->SetSize(FVector2D(Width,Height));
	};
	HealthProgress = WidgetTree->ConstructWidget<UProgressBar>();
	FProgressBarStyle HealthStyle;
	HealthStyle.SetBackgroundImage(ImmortalUITheme::PanelBrush(FLinearColor(0.04f,0.045f,0.07f)));
	HealthStyle.SetFillImage(ImmortalUITheme::PanelBrush(FLinearColor(0.36f,0.68f,0.29f)));
	HealthProgress->SetWidgetStyle(HealthStyle);
	HealthProgress->SetFillColorAndOpacity(FLinearColor::White);
	Place(HealthProgress, 4, 24, 250, 20);
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
	Label->SetText(FText::FromString(TEXT("仙途 · 自动历练")));
	FSlateFontInfo Font = Label->GetFont(); Font.Size=12; Label->SetFont(Font);
	Label->SetColorAndOpacity(FLinearColor(1.0f,0.92f,0.73f));
	Label->SetShadowOffset(FVector2D(1)); Label->SetShadowColorAndOpacity(FLinearColor::Black);
	Place(Label, 8, 2, 240, 20);
	auto AddQuickButton = [this, &Place](int32 Icon, const TCHAR* Tooltip, float X)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		Button->SetStyle(ImmortalUITheme::ButtonStyle());
		ImmortalUITheme::IconButton(this, Button, Icon, FText::FromString(Tooltip), true);
		Place(Button, X, 4, 46, 46);
		return Button;
	};
	AddQuickButton(1, TEXT("储物戒 / 装备"), 270)->OnClicked.AddDynamic(this, &ThisClass::HandleInventoryClicked);
	AddQuickButton(0, TEXT("修炼 / 突破"), 320)->OnClicked.AddDynamic(this, &ThisClass::HandleCultivationClicked);
	AddQuickButton(7, TEXT("百宝阁"), 370)->OnClicked.AddDynamic(this, &ThisClass::HandleShopClicked);
	AddQuickButton(12, TEXT("仙府 · 全部功能"), 420)->OnClicked.AddDynamic(this, &ThisClass::HandleOpenManagementClicked);
	AddQuickButton(16, TEXT("设置 / 透明背景"), 466)->OnClicked.AddDynamic(this, &ThisClass::HandleSettingsClicked);
}

void UImmortalPlayerStatusWidget::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (APlayerController* PC = GetOwningPlayer())
	{
		int32 Width = 0, Height = 0;
		PC->GetViewportSize(Width, Height);
		const float Dpi = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
		if (Width > 0 && (LastViewportSize != FIntPoint(Width, Height)
			|| !FMath::IsNearlyEqual(Dpi, LastViewportScale)))
		{
			LastViewportSize = FIntPoint(Width, Height); LastViewportScale = Dpi;
			const float Fit = FMath::Clamp((Width - 16.0f) / 512.0f, 0.1f, 1.0f);
			SetRenderTransformPivot(FVector2D::ZeroVector);
			SetRenderScale(FVector2D(Fit / Dpi));
			const int32 BattleHeight = Player.IsValid() ? Player->GetDesktopCombatViewportHeight() : Height;
			SetPositionInViewport(FVector2D(24, FMath::Max(Height - BattleHeight, 0) + 16), true);
		}
	}
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

void UImmortalPlayerStatusWidget::HandleInventoryClicked()
{
	if (Player.IsValid()) Player->OpenManagementFeature(EImmortalManagementFeature::Inventory);
}
void UImmortalPlayerStatusWidget::HandleCultivationClicked()
{
	if (Player.IsValid()) Player->OpenManagementFeature(EImmortalManagementFeature::Cultivation);
}
void UImmortalPlayerStatusWidget::HandleShopClicked()
{
	if (Player.IsValid()) Player->OpenManagementFeature(EImmortalManagementFeature::Shop);
}
void UImmortalPlayerStatusWidget::HandleSettingsClicked()
{
	if (Player.IsValid()) Player->OpenManagementFeature(EImmortalManagementFeature::Settings);
}
