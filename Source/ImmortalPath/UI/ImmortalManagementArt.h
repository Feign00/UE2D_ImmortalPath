#pragma once

#include "CoreMinimal.h"
#include "ImmortalManagementTypes.h"

/** Stable cell mapping for the original six-building mortal hub atlas. */
namespace ImmortalManagementArt
{
	inline int32 BuildingCell(EImmortalManagementFeature Feature)
	{
		switch (Feature)
		{
		case EImmortalManagementFeature::Cultivation: return 0;
		case EImmortalManagementFeature::Sect: return 1;
		case EImmortalManagementFeature::Alchemy: return 2;
		case EImmortalManagementFeature::Crafting: return 3;
		case EImmortalManagementFeature::Cave: return 4;
		case EImmortalManagementFeature::Farming: return 5;
		default: return INDEX_NONE;
		}
	}
	inline FBox2f CellUV(int32 Cell)
	{
		check(Cell >= 0 && Cell < 6);
		const FVector2f Min(float(Cell % 3) / 3.0f, float(Cell / 3) / 2.0f);
		return FBox2f(Min, Min + FVector2f(1.0f / 3.0f, 0.5f));
	}
}
