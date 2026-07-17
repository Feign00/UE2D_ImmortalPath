// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalEquipmentTypes.h"

namespace
{
	EImmortalEquipmentQuality RollQuality(const EImmortalEquipmentQuality MinimumQuality)
	{
		const float Roll = FMath::FRand();
		EImmortalEquipmentQuality RolledQuality = EImmortalEquipmentQuality::Legendary;
		if (Roll < 0.50f) RolledQuality = EImmortalEquipmentQuality::Common;
		else if (Roll < 0.78f) RolledQuality = EImmortalEquipmentQuality::Uncommon;
		else if (Roll < 0.93f) RolledQuality = EImmortalEquipmentQuality::Rare;
		else if (Roll < 0.985f) RolledQuality = EImmortalEquipmentQuality::Epic;
		return static_cast<EImmortalEquipmentQuality>(FMath::Max(
			static_cast<int32>(RolledQuality), static_cast<int32>(MinimumQuality)));
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

	void RebuildEquipmentTotals(FImmortalEquipmentItem& Item)
	{
		const float EnhancementMultiplier = 1.0f + 0.08f * FMath::Clamp(Item.EnhancementLevel, 0, 15);
		Item.AttackBonus = Item.BaseAttackBonus * EnhancementMultiplier;
		Item.DefenseBonus = Item.BaseDefenseBonus * EnhancementMultiplier;
		Item.HealthBonus = Item.BaseHealthBonus * EnhancementMultiplier;
		Item.AttackSpeedBonus = Item.BaseAttackSpeedBonus * EnhancementMultiplier;
		Item.CriticalChanceBonus = Item.BaseCriticalChanceBonus * EnhancementMultiplier;
		for (const FImmortalEquipmentAffix& Affix : Item.Affixes)
		{
			switch (Affix.Type)
			{
			case EImmortalEquipmentAffixType::Attack: Item.AttackBonus += Affix.Value; break;
			case EImmortalEquipmentAffixType::Defense: Item.DefenseBonus += Affix.Value; break;
			case EImmortalEquipmentAffixType::Health: Item.HealthBonus += Affix.Value; break;
			case EImmortalEquipmentAffixType::AttackSpeed: Item.AttackSpeedBonus += Affix.Value; break;
			case EImmortalEquipmentAffixType::CriticalChance: Item.CriticalChanceBonus += Affix.Value; break;
			default: break;
			}
		}
	}

	void RollEquipmentAffixes(FImmortalEquipmentItem& Item, const float Budget)
	{
		Item.Affixes.Reset();
		TArray<EImmortalEquipmentAffixType> Types =
		{
			EImmortalEquipmentAffixType::Attack,
			EImmortalEquipmentAffixType::Defense,
			EImmortalEquipmentAffixType::Health,
			EImmortalEquipmentAffixType::AttackSpeed,
			EImmortalEquipmentAffixType::CriticalChance
		};
		for (int32 Index = Types.Num() - 1; Index > 0; --Index)
		{
			Types.Swap(Index, FMath::RandRange(0, Index));
		}
		const int32 AffixCount = FMath::Clamp(static_cast<int32>(Item.Quality) + 1, 1, Types.Num());
		for (int32 Index = 0; Index < AffixCount; ++Index)
		{
			FImmortalEquipmentAffix& Affix = Item.Affixes.AddDefaulted_GetRef();
			Affix.Type = Types[Index];
			switch (Affix.Type)
			{
			case EImmortalEquipmentAffixType::Attack: Affix.Value = Vary(0.50f * Budget); break;
			case EImmortalEquipmentAffixType::Defense: Affix.Value = Vary(0.32f * Budget); break;
			case EImmortalEquipmentAffixType::Health: Affix.Value = Vary(4.0f * Budget); break;
			case EImmortalEquipmentAffixType::AttackSpeed: Affix.Value = Vary(0.008f * Budget); break;
			case EImmortalEquipmentAffixType::CriticalChance: Affix.Value = Vary(0.0035f * Budget); break;
			default: break;
			}
		}
	}

	void UpdateForgedEquipmentName(FImmortalEquipmentItem& Item)
	{
		Item.DisplayName = FName(*FString::Printf(
			TEXT("%s %s +%d"),
			*UImmortalEquipmentLibrary::GetQualityText(Item.Quality).ToString(),
			*UImmortalEquipmentLibrary::GetSlotText(Item.Slot).ToString(),
			Item.EnhancementLevel));
	}

	FImmortalEquipmentItem GenerateEquipmentForSlot(
		const int32 ItemLevel,
		const EImmortalEquipmentSlot Slot,
		const EImmortalEquipmentQuality Quality)
	{
		FImmortalEquipmentItem Item;
		Item.ItemId = FGuid::NewGuid();
		Item.ItemLevel = FMath::Max(ItemLevel, 1);
		Item.Slot = Slot;
		Item.Quality = Quality;

		const float Budget = static_cast<float>(Item.ItemLevel) * GetQualityMultiplier(Item.Quality);
		switch (Item.Slot)
		{
		case EImmortalEquipmentSlot::Weapon:
			Item.BaseAttackBonus = Vary(2.3f * Budget);
			break;
		case EImmortalEquipmentSlot::Head:
			Item.BaseHealthBonus = Vary(7.0f * Budget);
			Item.BaseDefenseBonus = Vary(0.6f * Budget);
			break;
		case EImmortalEquipmentSlot::Chest:
			Item.BaseHealthBonus = Vary(11.0f * Budget);
			Item.BaseDefenseBonus = Vary(0.9f * Budget);
			break;
		case EImmortalEquipmentSlot::Boots:
			Item.BaseHealthBonus = Vary(5.0f * Budget);
			Item.BaseAttackSpeedBonus = Vary(0.015f * Budget);
			break;
		case EImmortalEquipmentSlot::Accessory:
			Item.BaseAttackBonus = Vary(0.8f * Budget);
			Item.BaseCriticalChanceBonus = Vary(0.004f * Budget);
			break;
		default:
			break;
		}
		RollEquipmentAffixes(Item, Budget);
		RebuildEquipmentTotals(Item);
		UpdateForgedEquipmentName(Item);
		return Item;
	}

	FImmortalEquipmentItem GenerateEquipment(
		const int32 ItemLevel,
		const EImmortalEquipmentQuality MinimumQuality)
	{
		return GenerateEquipmentForSlot(
			ItemLevel,
			static_cast<EImmortalEquipmentSlot>(FMath::RandRange(0, static_cast<int32>(EImmortalEquipmentSlot::MAX) - 1)),
			RollQuality(MinimumQuality));
	}
}

FImmortalEquipmentItem UImmortalEquipmentLibrary::GenerateRandomEquipment(const int32 ItemLevel)
{
	return GenerateEquipment(ItemLevel, EImmortalEquipmentQuality::Common);
}

FImmortalEquipmentItem UImmortalEquipmentLibrary::GenerateRandomEquipmentWithMinimumQuality(
	const int32 ItemLevel,
	const EImmortalEquipmentQuality MinimumQuality)
{
	return GenerateEquipment(ItemLevel, MinimumQuality);
}

FImmortalEquipmentItem UImmortalEquipmentLibrary::GenerateCraftedEquipment(
	const int32 ItemLevel,
	const EImmortalEquipmentSlot Slot,
	const EImmortalEquipmentQuality Quality)
{
	return GenerateEquipmentForSlot(
		ItemLevel,
		Slot == EImmortalEquipmentSlot::MAX ? EImmortalEquipmentSlot::Weapon : Slot,
		Quality);
}

void UImmortalEquipmentLibrary::NormalizeForgingState(FImmortalEquipmentItem& Item)
{
	if (!Item.IsValid()) return;
	Item.ItemLevel = FMath::Max(Item.ItemLevel, 1);
	Item.EnhancementLevel = FMath::Clamp(Item.EnhancementLevel, 0, 15);
	Item.RefinementCount = FMath::Max(Item.RefinementCount, 0);
	const bool bMissingBaseStats = Item.BaseAttackBonus <= 0.0f
		&& Item.BaseDefenseBonus <= 0.0f
		&& Item.BaseHealthBonus <= 0.0f
		&& Item.BaseAttackSpeedBonus <= 0.0f
		&& Item.BaseCriticalChanceBonus <= 0.0f;
	if (bMissingBaseStats)
	{
		const float InverseEnhancement = 1.0f / (1.0f + 0.08f * Item.EnhancementLevel);
		Item.BaseAttackBonus = FMath::Max(Item.AttackBonus * InverseEnhancement, 0.0f);
		Item.BaseDefenseBonus = FMath::Max(Item.DefenseBonus * InverseEnhancement, 0.0f);
		Item.BaseHealthBonus = FMath::Max(Item.HealthBonus * InverseEnhancement, 0.0f);
		Item.BaseAttackSpeedBonus = FMath::Max(Item.AttackSpeedBonus * InverseEnhancement, 0.0f);
		Item.BaseCriticalChanceBonus = FMath::Max(Item.CriticalChanceBonus * InverseEnhancement, 0.0f);
	}
	Item.Affixes.RemoveAll([](const FImmortalEquipmentAffix& Affix) { return Affix.Value <= 0.0f; });
	RebuildEquipmentTotals(Item);
	UpdateForgedEquipmentName(Item);
}

void UImmortalEquipmentLibrary::RebuildEquipmentStats(FImmortalEquipmentItem& Item)
{
	NormalizeForgingState(Item);
}

bool UImmortalEquipmentLibrary::EnhanceEquipment(FImmortalEquipmentItem& Item, const int32 MaximumEnhancementLevel)
{
	if (!Item.IsValid()) return false;
	NormalizeForgingState(Item);
	const int32 Cap = FMath::Clamp(MaximumEnhancementLevel, 0, 15);
	if (Item.EnhancementLevel >= Cap) return false;
	++Item.EnhancementLevel;
	RebuildEquipmentTotals(Item);
	UpdateForgedEquipmentName(Item);
	return true;
}

bool UImmortalEquipmentLibrary::RerollEquipmentAffixes(FImmortalEquipmentItem& Item)
{
	if (!Item.IsValid()) return false;
	NormalizeForgingState(Item);
	const float Budget = static_cast<float>(Item.ItemLevel) * GetQualityMultiplier(Item.Quality);
	RollEquipmentAffixes(Item, Budget);
	++Item.RefinementCount;
	RebuildEquipmentTotals(Item);
	UpdateForgedEquipmentName(Item);
	return true;
}

FText UImmortalEquipmentLibrary::GetAffixText(const FImmortalEquipmentAffix& Affix)
{
	switch (Affix.Type)
	{
	case EImmortalEquipmentAffixType::Attack:
		return FText::FromString(FString::Printf(TEXT("攻击 +%.1f"), Affix.Value));
	case EImmortalEquipmentAffixType::Defense:
		return FText::FromString(FString::Printf(TEXT("防御 +%.1f"), Affix.Value));
	case EImmortalEquipmentAffixType::Health:
		return FText::FromString(FString::Printf(TEXT("生命 +%.1f"), Affix.Value));
	case EImmortalEquipmentAffixType::AttackSpeed:
		return FText::FromString(FString::Printf(TEXT("攻速 +%.1f%%"), Affix.Value * 100.0f));
	case EImmortalEquipmentAffixType::CriticalChance:
		return FText::FromString(FString::Printf(TEXT("暴击 +%.1f%%"), Affix.Value * 100.0f));
	default:
		return FText::GetEmpty();
	}
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
