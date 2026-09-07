#pragma once
#include "CoreMinimal.h"

/** Pixel-space layout shared by native docking, UMG and world projection. */
namespace ImmortalDesktopPanelLayout
{
	inline int32 BattleHeight(int32 Requested, int32 WorkHeight)
	{
		return FMath::Clamp(Requested, 180, FMath::Max(WorkHeight / 2, 180));
	}
	inline int32 WindowHeight(int32 Battle, int32 WorkHeight, bool bExpanded)
	{
		return bExpanded ? FMath::Min(Battle + 360, WorkHeight) : Battle;
	}
	struct FPanel { FVector2D Position; float Scale; };
	inline FPanel Fit(FVector2D Viewport, FVector2D Logical, int32 Battle)
	{
		const int32 ReservedBattle = Viewport.Y > Battle + 120 ? Battle : 0;
		const float AvailableHeight = FMath::Max(float(Viewport.Y - ReservedBattle - 24), 1.0f);
		const float Scale = FMath::Min3(float(FMath::Max(Viewport.X - 48, 1.0) / Logical.X),
			float(AvailableHeight / Logical.Y), 1.0f);
		return { FVector2D(FMath::Max((Viewport.X - Logical.X * Scale) / 2, 24.0), 12), Scale };
	}
	inline void AnchorBattleProjection(FMatrix& Projection, int32 Height, int32 Battle)
	{
		const double Offset = double(FMath::Max(Height - Battle, 0)) / FMath::Max(Height, 1);
		// Clip Y -= offset * clip W works for both perspective and orthographic views.
		for (int32 Row = 0; Row < 4; ++Row) Projection.M[Row][1] -= Offset * Projection.M[Row][3];
	}
}
