// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalEquipmentTypes.h"

namespace
{
	EImmortalEquipmentQuality RollQuality()
	{
		const float Roll = FMath::FRand();
		if (Roll < 0.50f) return EImmortalEquipmentQuality::Common;
		if (Roll < 0.78f) return EImmortalEquipmentQuality::Uncommon;
		if (Roll < 0.93f) return EImmortalEquipmentQuality::Rare;
		if (Roll < 0.985f) return EImmortalEquipmentQuality::Epic;
		return EImmortalEquipmentQuality::Legendary;
	}

	float GetQualityMultiplier(const EImmortalEquipmentQuality Quality)
	{
		switch (Quality)
		{
		case EImmortalEquipmentQuality::Uncommon: return 1.35f;
		case EImmortalEquipmentQuality::Rare: return 1.85f;
		case EImmortalEquipmentQuality::Epic: return 2.60f;
		case EImmortalEquipmentQuality::Legendary: return 3.75f;
		default: return 1.0f;
		}
	}

	float Vary(const float Value)
	{
		return Value * FMath::FRandRange(0.85f, 1.15f);
	}
}

FImmortalEquipmentItem UImmortalEquipmentLibrary::GenerateRandomEquipment(const int32 ItemLevel)
{
	FImmortalEquipmentItem Item;
	Item.ItemId = FGuid::NewGuid();
	Item.ItemLevel = FMath::Max(ItemLevel, 1);
	Item.Slot = static_cast<EImmortalEquipmentSlot>(FMath::RandRange(0, static_cast<int32>(EImmortalEquipmentSlot::MAX) - 1));
	Item.Quality = RollQuality();

	const float Budget = static_cast<float>(Item.ItemLevel) * GetQualityMultiplier(Item.Quality);
	switch (Item.Slot)
	{
	case EImmortalEquipmentSlot::Weapon:
		Item.AttackBonus = Vary(2.8f * Budget);
		Item.CriticalChanceBonus = Vary(0.004f * Budget);
		break;
	case EImmortalEquipmentSlot::Head:
		Item.HealthBonus = Vary(9.0f * Budget);
		Item.DefenseBonus = Vary(0.8f * Budget);
		break;
	case EImmortalEquipmentSlot::Chest:
		Item.HealthBonus = Vary(14.0f * Budget);
		Item.DefenseBonus = Vary(1.2f * Budget);
		break;
	case EImmortalEquipmentSlot::Boots:
		Item.HealthBonus = Vary(6.0f * Budget);
		Item.AttackSpeedBonus = Vary(0.025f * Budget);
		break;
	case EImmortalEquipmentSlot::Accessory:
		Item.AttackBonus = Vary(1.1f * Budget);
		Item.AttackSpeedBonus = Vary(0.012f * Budget);
		Item.CriticalChanceBonus = Vary(0.009f * Budget);
		break;
	default:
		break;
	}

	Item.DisplayName = FName(*FString::Printf(
		TEXT("%s %s +%d"),
		*GetQualityText(Item.Quality).ToString(),
		*GetSlotText(Item.Slot).ToString(),
		Item.ItemLevel));
	return Item;
}

float UImmortalEquipmentLibrary::CalculateEquipmentPower(const FImmortalEquipmentItem& Item)
{
	if (!Item.IsValid()) return 0.0f;
	return Item.AttackBonus * 5.0f + Item.DefenseBonus * 4.0f + Item.HealthBonus * 0.2f
		+ Item.AttackSpeedBonus * 20.0f + Item.CriticalChanceBonus * 100.0f;
}

FLinearColor UImmortalEquipmentLibrary::GetQualityColor(const EImmortalEquipmentQuality Quality)
{
	switch (Quality)
	{
	case EImmortalEquipmentQuality::Uncommon: return FLinearColor(0.25f, 0.95f, 0.35f, 1.0f);
	case EImmortalEquipmentQuality::Rare: return FLinearColor(0.25f, 0.55f, 1.0f, 1.0f);
	case EImmortalEquipmentQuality::Epic: return FLinearColor(0.75f, 0.30f, 1.0f, 1.0f);
	case EImmortalEquipmentQuality::Legendary: return FLinearColor(1.0f, 0.62f, 0.12f, 1.0f);
	default: return FLinearColor(0.92f, 0.92f, 0.92f, 1.0f);
	}
}

FText UImmortalEquipmentLibrary::GetQualityText(const EImmortalEquipmentQuality Quality)
{
	switch (Quality)
	{
	case EImmortalEquipmentQuality::Uncommon: return FText::FromString(TEXT("优秀"));
	case EImmortalEquipmentQuality::Rare: return FText::FromString(TEXT("稀有"));
	case EImmortalEquipmentQuality::Epic: return FText::FromString(TEXT("史诗"));
	case EImmortalEquipmentQuality::Legendary: return FText::FromString(TEXT("传说"));
	default: return FText::FromString(TEXT("普通"));
	}
}

FText UImmortalEquipmentLibrary::GetSlotText(const EImmortalEquipmentSlot Slot)
{
	switch (Slot)
	{
	case EImmortalEquipmentSlot::Head: return FText::FromString(TEXT("头部"));
	case EImmortalEquipmentSlot::Chest: return FText::FromString(TEXT("衣服"));
	case EImmortalEquipmentSlot::Boots: return FText::FromString(TEXT("鞋子"));
	case EImmortalEquipmentSlot::Accessory: return FText::FromString(TEXT("饰品"));
	default: return FText::FromString(TEXT("武器"));
	}
}
