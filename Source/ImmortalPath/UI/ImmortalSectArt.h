#pragma once
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

namespace ImmortalSectArt
{
	inline int32 Cell(FName SectId)
	{
		if (SectId == TEXT("QingyunSect")) return 0;
		if (SectId == TEXT("HeavenlySwordSect")) return 1;
		if (SectId == TEXT("MyriadDemonValley")) return 2;
		if (SectId == TEXT("DemonSect")) return 3;
		return INDEX_NONE;
	}
	inline FSlateBrush Brush(UTexture2D* Atlas, FName SectId)
	{
		const int32 Index = Cell(SectId);
		FSlateBrush Result;
		Result.DrawAs = Atlas && Index != INDEX_NONE ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
		Result.SetResourceObject(Atlas);
		Result.ImageSize = FVector2D(88);
		if (Index != INDEX_NONE)
		{
			const FVector2f Min(float(Index % 2) / 2, float(Index / 2) / 2);
			Result.SetUVRegion(FBox2f(Min, Min + FVector2f(0.5f)));
		}
		return Result;
	}
	inline float ProgressFraction(int32 Progress, int32 Target, bool bJoined)
	{
		return bJoined ? FMath::Clamp(float(Progress) / FMath::Max(Target, 1), 0.0f, 1.0f) : 0.0f;
	}
}
