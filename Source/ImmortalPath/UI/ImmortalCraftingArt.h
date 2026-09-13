#pragma once
#include "ImmortalAlchemyArt.h"
#include "../Items/ImmortalEquipmentTypes.h"

namespace ImmortalCraftingArt
{
	inline int32 EquipmentCell(EImmortalEquipmentSlot Slot)
	{
		switch (Slot)
		{
		case EImmortalEquipmentSlot::Weapon: return 0;
		case EImmortalEquipmentSlot::Head: return 1;
		case EImmortalEquipmentSlot::Chest: return 2;
		case EImmortalEquipmentSlot::Bracers: return 3;
		case EImmortalEquipmentSlot::Belt: return 4;
		case EImmortalEquipmentSlot::Boots: return 5;
		case EImmortalEquipmentSlot::RingLeft: return 6;
		case EImmortalEquipmentSlot::RingRight: return 7;
		case EImmortalEquipmentSlot::Accessory: return 8;
		default: return INDEX_NONE;
		}
	}
	inline FSlateBrush EquipmentBrush(UTexture2D* Atlas, EImmortalEquipmentSlot Slot)
	{
		const int32 Index = EquipmentCell(Slot);
		FSlateBrush Result;
		Result.DrawAs = Atlas && Index != INDEX_NONE ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
		Result.SetResourceObject(Atlas);
		Result.ImageSize = FVector2D(80);
		if (Index != INDEX_NONE)
		{
			const FVector2f Min(float(Index % 3) / 3, float(Index / 3) / 3);
			Result.SetUVRegion(FBox2f(Min, Min + FVector2f(1.0f / 3)));
		}
		return Result;
	}
	inline int32 ForgeCell(FName Id)
	{
		if (Id == TEXT("DemonBone")) return 0;
		if (Id == TEXT("SpiritIron")) return 1;
		if (Id == TEXT("ArtifactFragment")) return 2;
		if (Id == TEXT("SpiritWood")) return 3;
		if (Id == TEXT("Anvil")) return 4;
		if (Id == TEXT("Forge")) return 5;
		return INDEX_NONE;
	}
	inline FSlateBrush ForgeBrush(UTexture2D* Atlas, FName Id)
	{
		return ImmortalAlchemyArt::CellBrush(Atlas, ForgeCell(Id));
	}
	inline FSlateBrush MaterialBrush(UTexture2D* Forge, UTexture2D* Materials, FName Id)
	{
		return ForgeCell(Id) != INDEX_NONE ? ForgeBrush(Forge, Id) : ImmortalAlchemyArt::MaterialBrush(Materials, Id);
	}
}
