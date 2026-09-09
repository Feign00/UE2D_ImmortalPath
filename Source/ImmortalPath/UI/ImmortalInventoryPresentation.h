#pragma once

#include "../Items/ImmortalEquipmentTypes.h"

namespace ImmortalInventoryPresentation
{
	constexpr int32 BackpackColumns = 7;
	constexpr float SlotSize = 84;
	// Three slots on either side, four below: portrait space remains unobstructed.
	inline FVector2D EquipmentPosition(int32 Column, int32 Row)
	{
		return Row == 3 ? FVector2D(Column * 100, 390)
			: FVector2D(Column == 0 ? 0 : 304, Row * 110);
	}

	inline FString Compare(const FImmortalEquipmentItem& Item, const FImmortalEquipmentItem& Current, bool bHasCurrent)
	{
		FString Result = bHasCurrent ? TEXT("与同部位已穿戴对比") : TEXT("此部位未穿戴 · 净增加");
		Result += FString::Printf(TEXT("\n装备评分 %+.1f"), UImmortalEquipmentLibrary::CalculateEquipmentPower(Item)
			- (bHasCurrent ? UImmortalEquipmentLibrary::CalculateEquipmentPower(Current) : 0));
		const auto Row = [&Result](const TCHAR* Name, float Value, float Old, bool bPercent = false)
		{
			const float Delta = (Value - Old) * (bPercent ? 100 : 1);
			if (!FMath::IsNearlyZero(Delta, 0.0001f))
				Result += FString::Printf(TEXT("\n%s %+.1f%s"), Name, Delta, bPercent ? TEXT("百分点") : TEXT(""));
		};
		const FImmortalEquipmentItem Empty;
		const auto& Before = bHasCurrent ? Current : Empty;
		Row(TEXT("攻击"), Item.AttackBonus, Before.AttackBonus);
		Row(TEXT("防御"), Item.DefenseBonus, Before.DefenseBonus);
		Row(TEXT("生命"), Item.HealthBonus, Before.HealthBonus);
		Row(TEXT("攻速"), Item.AttackSpeedBonus, Before.AttackSpeedBonus, true);
		Row(TEXT("暴击"), Item.CriticalChanceBonus, Before.CriticalChanceBonus, true);
		Row(TEXT("暴伤"), Item.CriticalDamageBonus, Before.CriticalDamageBonus, true);
		Row(TEXT("火伤"), Item.FireDamageBonus, Before.FireDamageBonus, true);
		Row(TEXT("雷伤"), Item.ThunderDamageBonus, Before.ThunderDamageBonus, true);
		Row(TEXT("冰伤"), Item.IceDamageBonus, Before.IceDamageBonus, true);
		Row(TEXT("吸血"), Item.LifeStealBonus, Before.LifeStealBonus, true);
		Row(TEXT("修炼"), Item.CultivationGainBonus, Before.CultivationGainBonus, true);
		Row(TEXT("掉率"), Item.LootFindBonus, Before.LootFindBonus, true);
		Row(TEXT("首领"), Item.BossDamageBonus, Before.BossDamageBonus, true);
		return Result + TEXT("\n仅装备属性差；未计套装与流派联动");
	}
}
