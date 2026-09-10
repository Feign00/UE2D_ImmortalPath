#pragma once
#include "../Farming/ImmortalFarmingTypes.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

namespace ImmortalFarmingArt
{
	// Static state illustrations, not animation frames. Text and progress retain precise growth state.
	inline int32 Cell(const FImmortalFarmingPlotView& View)
	{
		if (!View.bValidPlot || !View.bUnlocked) return 5;
		if (View.CropId.IsNone() || View.GrowthStage == EImmortalFarmingGrowthStage::Empty) return 0;
		if (View.GrowthStage == EImmortalFarmingGrowthStage::Seedling) return 1;
		if (View.CropId == TEXT("SpiritGrassCrop")) return 2;
		if (View.CropId == TEXT("ImmortalFruitCrop")) return 3;
		if (View.CropId == TEXT("SpiritWoodCrop")) return 4;
		return 1;
	}
	inline FBox2f CellUV(int32 Index)
	{
		check(Index >= 0 && Index < 6);
		const FVector2f Min(float(Index % 3) / 3, float(Index / 3) / 2);
		const FVector2f Max(float(Index % 3 + 1) / 3, float(Index / 3 + 1) / 2);
		return FBox2f(Min, Max);
	}
	inline FSlateBrush Brush(UTexture2D* Atlas, int32 Index)
	{
		FSlateBrush Result;
		Result.DrawAs = Atlas ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
		Result.SetResourceObject(Atlas);
		Result.ImageSize = FVector2D(128);
		Result.SetUVRegion(CellUV(Index));
		return Result;
	}
}
