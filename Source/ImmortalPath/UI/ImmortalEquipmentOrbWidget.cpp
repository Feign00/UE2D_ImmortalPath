// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalEquipmentOrbWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"

void UImmortalEquipmentOrbWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EquipmentOrbSize"));
	RootBox->SetWidthOverride(128.0f);
	RootBox->SetHeightOverride(128.0f);
	WidgetTree->RootWidget = RootBox;

	OrbImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("EquipmentOrbImage"));
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = FVector2D(128.0f, 128.0f);
	if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/GAME/Asset/ui/equipment_orb/equipment_orb.equipment_orb")))
	{
		Brush.SetResourceObject(Texture);
		Brush.TintColor = FSlateColor(FLinearColor::White);
	}
	else
	{
		Brush.TintColor = FSlateColor(FLinearColor(0.4f, 0.75f, 1.0f, 1.0f));
	}
	OrbImage->SetBrush(Brush);
	RootBox->AddChild(OrbImage);
}

void UImmortalEquipmentOrbWidget::SetQuality(const EImmortalEquipmentQuality Quality)
{
	if (OrbImage)
	{
		OrbImage->SetColorAndOpacity(UImmortalEquipmentLibrary::GetQualityColor(Quality));
	}
}
