#pragma once
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

namespace ImmortalAlchemyArt
{
	inline int32 Cell(FName Id)
	{
		if (Id == TEXT("HealingPill")) return 0;
		if (Id == TEXT("QiGatheringPill")) return 1;
		if (Id == TEXT("FoundationPill")) return 2;
		if (Id == TEXT("EnlightenmentPill")) return 3;
		if (Id == TEXT("BreakthroughPill")) return 4;
		if (Id == TEXT("Cauldron")) return 5;
		return INDEX_NONE;
	}
	inline int32 MaterialCell(FName Id)
	{
		if (Id == TEXT("SpiritGrass")) return 0;
		if (Id == TEXT("DemonCore")) return 1;
		if (Id == TEXT("SpiritLiquid")) return 2;
		if (Id == TEXT("Ore")) return 3;
		if (Id == TEXT("ImmortalFruit")) return 4;
		if (Id == TEXT("SpiritStones")) return 5;
		return INDEX_NONE;
	}
	inline FSlateBrush CellBrush(UTexture2D* Atlas, int32 Index)
	{
		if (Index < 0 || Index >= 6) Index = INDEX_NONE;
		FSlateBrush Result;
		Result.DrawAs = Atlas && Index != INDEX_NONE ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
		Result.SetResourceObject(Atlas);
		Result.ImageSize = FVector2D(80);
		if (Index != INDEX_NONE)
		{
			const FVector2f Min(float(Index % 3) / 3, float(Index / 3) / 2);
			Result.SetUVRegion(FBox2f(Min, Min + FVector2f(1.0f / 3, 0.5f)));
		}
		return Result;
	}
	inline FSlateBrush Brush(UTexture2D* Atlas, FName Id) { return CellBrush(Atlas, Cell(Id)); }
	inline FSlateBrush MaterialBrush(UTexture2D* Atlas, FName Id) { return CellBrush(Atlas, MaterialCell(Id)); }
}
