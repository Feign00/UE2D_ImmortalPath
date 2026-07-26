// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalEquipmentTypes.h"

#include "Engine/DataTable.h"
#include "Misc/PackageName.h"

namespace
{
	const FName QingyunSetId(TEXT("QingyunSet"));
	const FName HeavenlySwordSetId(TEXT("HeavenlySwordSet"));
	const FName MyriadThunderSetId(TEXT("MyriadThunderSet"));
	const FName BlackTortoiseSetId(TEXT("BlackTortoiseSet"));

	FImmortalEquipmentSetTier MakeSetTier(const int32 Pieces, const TCHAR* Description)
	{
		FImmortalEquipmentSetTier Result;
		Result.RequiredPieces = Pieces;
		Result.Description = FText::FromString(Description);
		return Result;
	}

	FImmortalEquipmentSetDefinition MakeSetDefinition(
		const FName SetId,
		const TCHAR* DisplayName,
		const FLinearColor& Color,
		const TArray<FImmortalEquipmentSetTier>& Tiers)
	{
		FImmortalEquipmentSetDefinition Result;
		Result.SetId = SetId;
		Result.DisplayName = FText::FromString(DisplayName);
		Result.DisplayColor = Color;
		Result.Tiers = Tiers;
		return Result;
	}

	const TMap<FName, FImmortalEquipmentSetDefinition>& GetFallbackSetCatalog()
	{
		static const TMap<FName, FImmortalEquipmentSetDefinition> Catalog = []
		{
			TMap<FName, FImmortalEquipmentSetDefinition> Result;

			FImmortalEquipmentSetTier Qingyun2 = MakeSetTier(2, TEXT("攻击 +8%"));
			Qingyun2.AttackMultiplierBonus = 0.08f;
			FImmortalEquipmentSetTier Qingyun4 = MakeSetTier(4, TEXT("修炼效率 +12%"));
			Qingyun4.CultivationGainBonus = 0.12f;
			FImmortalEquipmentSetTier Qingyun6 = MakeSetTier(6, TEXT("最终伤害 +15%"));
			Qingyun6.FinalDamageBonus = 0.15f;
			Result.Add(QingyunSetId, MakeSetDefinition(QingyunSetId, TEXT("青云套"),
				FLinearColor(0.28f, 0.88f, 0.92f, 1.0f), {Qingyun2, Qingyun4, Qingyun6}));

			FImmortalEquipmentSetTier Sword2 = MakeSetTier(2, TEXT("暴击率 +5%"));
			Sword2.CriticalChanceBonus = 0.05f;
			FImmortalEquipmentSetTier Sword4 = MakeSetTier(4, TEXT("暴击伤害 +25%"));
			Sword4.CriticalDamageBonus = 0.25f;
			FImmortalEquipmentSetTier Sword6 = MakeSetTier(6, TEXT("首领伤害 +25%"));
			Sword6.BossDamageBonus = 0.25f;
			Result.Add(HeavenlySwordSetId, MakeSetDefinition(HeavenlySwordSetId, TEXT("天剑套"),
				FLinearColor(0.95f, 0.78f, 0.28f, 1.0f), {Sword2, Sword4, Sword6}));

			FImmortalEquipmentSetTier Thunder2 = MakeSetTier(2, TEXT("雷系伤害 +15%"));
			Thunder2.ThunderDamageBonus = 0.15f;
			FImmortalEquipmentSetTier Thunder4 = MakeSetTier(4, TEXT("攻击速度 +12%"));
			Thunder4.AttackSpeedBonus = 0.12f;
			FImmortalEquipmentSetTier Thunder6 = MakeSetTier(6, TEXT("最终伤害 +18%"));
			Thunder6.FinalDamageBonus = 0.18f;
			Result.Add(MyriadThunderSetId, MakeSetDefinition(MyriadThunderSetId, TEXT("万雷套"),
				FLinearColor(0.58f, 0.42f, 1.0f, 1.0f), {Thunder2, Thunder4, Thunder6}));

			FImmortalEquipmentSetTier Tortoise2 = MakeSetTier(2, TEXT("防御 +12%"));
			Tortoise2.DefenseMultiplierBonus = 0.12f;
			FImmortalEquipmentSetTier Tortoise4 = MakeSetTier(4, TEXT("生命 +18%"));
			Tortoise4.HealthMultiplierBonus = 0.18f;
			FImmortalEquipmentSetTier Tortoise6 = MakeSetTier(6, TEXT("受到伤害降低 15%"));
			Tortoise6.DamageReductionBonus = 0.15f;
			Result.Add(BlackTortoiseSetId, MakeSetDefinition(BlackTortoiseSetId, TEXT("玄武套"),
				FLinearColor(0.42f, 0.86f, 0.48f, 1.0f), {Tortoise2, Tortoise4, Tortoise6}));
			return Result;
		}();
		return Catalog;
	}

	UDataTable* GetOptionalEquipmentSetTable()
	{
		static TWeakObjectPtr<UDataTable> CachedTable;
		static bool bAttemptedLoad = false;
		if (!bAttemptedLoad)
		{
			bAttemptedLoad = true;
			const FString PackageName(TEXT("/Game/GAME/Data/DT_EquipmentSets"));
			if (FPackageName::DoesPackageExist(PackageName))
			{
				CachedTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/GAME/Data/DT_EquipmentSets.DT_EquipmentSets"));
			}
		}
		return CachedTable.Get();
	}

	EImmortalEquipmentQuality ClampQuality(const EImmortalEquipmentQuality Quality)
	{
		return static_cast<EImmortalEquipmentQuality>(FMath::Clamp(
			static_cast<int32>(Quality),
			static_cast<int32>(EImmortalEquipmentQuality::Common),
			static_cast<int32>(EImmortalEquipmentQuality::Divine)));
	}

	EImmortalEquipmentQuality RollQuality(const EImmortalEquipmentQuality MinimumQuality)
	{
		const float Roll = FMath::FRand();
		EImmortalEquipmentQuality RolledQuality = EImmortalEquipmentQuality::Divine;
		if (Roll < 0.5000f) RolledQuality = EImmortalEquipmentQuality::Common;
		else if (Roll < 0.7700f) RolledQuality = EImmortalEquipmentQuality::Uncommon;
		else if (Roll < 0.9100f) RolledQuality = EImmortalEquipmentQuality::Rare;
		else if (Roll < 0.9700f) RolledQuality = EImmortalEquipmentQuality::Epic;
		else if (Roll < 0.9920f) RolledQuality = EImmortalEquipmentQuality::Legendary;
		else if (Roll < 0.9985f) RolledQuality = EImmortalEquipmentQuality::Immortal;
		const int32 MinimumRank = static_cast<int32>(ClampQuality(MinimumQuality));
		return static_cast<EImmortalEquipmentQuality>(FMath::Max(static_cast<int32>(RolledQuality), MinimumRank));
	}

	float GetQualityMultiplier(const EImmortalEquipmentQuality Quality)
	{
		switch (ClampQuality(Quality))
		{
		case EImmortalEquipmentQuality::Uncommon: return 1.35f;
		case EImmortalEquipmentQuality::Rare: return 1.85f;
		case EImmortalEquipmentQuality::Epic: return 2.60f;
		case EImmortalEquipmentQuality::Legendary: return 3.75f;
		case EImmortalEquipmentQuality::Immortal: return 5.40f;
		case EImmortalEquipmentQuality::Divine: return 7.50f;
		default: return 1.0f;
		}
	}

	float Vary(const float Value)
	{
		return Value * FMath::FRandRange(0.85f, 1.15f);
	}

	EImmortalEquipmentDiscipline RollDiscipline()
	{
		const float Roll = FMath::FRand();
		if (Roll < 0.35f) return EImmortalEquipmentDiscipline::Universal;
		return static_cast<EImmortalEquipmentDiscipline>(FMath::RandRange(
			static_cast<int32>(EImmortalEquipmentDiscipline::Body),
			static_cast<int32>(EImmortalEquipmentDiscipline::Thunder)));
	}

	FName RollSetId(const EImmortalEquipmentQuality Quality)
	{
		float Chance = 0.0f;
		switch (ClampQuality(Quality))
		{
		case EImmortalEquipmentQuality::Rare: Chance = 0.12f; break;
		case EImmortalEquipmentQuality::Epic: Chance = 0.25f; break;
		case EImmortalEquipmentQuality::Legendary: Chance = 0.40f; break;
		case EImmortalEquipmentQuality::Immortal: Chance = 0.60f; break;
		case EImmortalEquipmentQuality::Divine: Chance = 0.85f; break;
		default: break;
		}
		if (Chance <= 0.0f || FMath::FRand() >= Chance) return NAME_None;
		const TArray<FName> SetIds = UImmortalEquipmentLibrary::GetKnownSetIds();
		return SetIds.IsEmpty() ? NAME_None : SetIds[FMath::RandRange(0, SetIds.Num() - 1)];
	}

	void RebuildEquipmentTotals(FImmortalEquipmentItem& Item)
	{
		const float EnhancementMultiplier = 1.0f + 0.08f * FMath::Clamp(Item.EnhancementLevel, 0, 15);
		Item.AttackBonus = Item.BaseAttackBonus * EnhancementMultiplier;
		Item.DefenseBonus = Item.BaseDefenseBonus * EnhancementMultiplier;
		Item.HealthBonus = Item.BaseHealthBonus * EnhancementMultiplier;
		Item.AttackSpeedBonus = Item.BaseAttackSpeedBonus * EnhancementMultiplier;
		Item.CriticalChanceBonus = Item.BaseCriticalChanceBonus * EnhancementMultiplier;
		Item.CriticalDamageBonus = 0.0f;
		Item.FireDamageBonus = 0.0f;
		Item.ThunderDamageBonus = 0.0f;
		Item.IceDamageBonus = 0.0f;
		Item.LifeStealBonus = 0.0f;
		Item.CultivationGainBonus = 0.0f;
		Item.LootFindBonus = 0.0f;
		Item.BossDamageBonus = 0.0f;
		for (const FImmortalEquipmentAffix& Affix : Item.Affixes)
		{
			switch (Affix.Type)
			{
			case EImmortalEquipmentAffixType::Attack: Item.AttackBonus += Affix.Value; break;
			case EImmortalEquipmentAffixType::Defense: Item.DefenseBonus += Affix.Value; break;
			case EImmortalEquipmentAffixType::Health: Item.HealthBonus += Affix.Value; break;
			case EImmortalEquipmentAffixType::AttackSpeed: Item.AttackSpeedBonus += Affix.Value; break;
			case EImmortalEquipmentAffixType::CriticalChance: Item.CriticalChanceBonus += Affix.Value; break;
			case EImmortalEquipmentAffixType::CriticalDamage: Item.CriticalDamageBonus += Affix.Value; break;
			case EImmortalEquipmentAffixType::FireDamage: Item.FireDamageBonus += Affix.Value; break;
			case EImmortalEquipmentAffixType::ThunderDamage: Item.ThunderDamageBonus += Affix.Value; break;
			case EImmortalEquipmentAffixType::IceDamage: Item.IceDamageBonus += Affix.Value; break;
			case EImmortalEquipmentAffixType::LifeSteal: Item.LifeStealBonus += Affix.Value; break;
			case EImmortalEquipmentAffixType::CultivationGain: Item.CultivationGainBonus += Affix.Value; break;
			case EImmortalEquipmentAffixType::LootFind: Item.LootFindBonus += Affix.Value; break;
			case EImmortalEquipmentAffixType::BossDamage: Item.BossDamageBonus += Affix.Value; break;
			default: break;
			}
		}
	}

	float RollAffixValue(const EImmortalEquipmentAffixType Type, const float Budget)
	{
		switch (Type)
		{
		case EImmortalEquipmentAffixType::Attack: return FMath::Max(Vary(0.50f * Budget), 0.1f);
		case EImmortalEquipmentAffixType::Defense: return FMath::Max(Vary(0.32f * Budget), 0.1f);
		case EImmortalEquipmentAffixType::Health: return FMath::Max(Vary(4.0f * Budget), 1.0f);
		case EImmortalEquipmentAffixType::AttackSpeed: return FMath::Clamp(Vary(0.008f * Budget), 0.005f, 0.25f);
		case EImmortalEquipmentAffixType::CriticalChance: return FMath::Clamp(Vary(0.0035f * Budget), 0.003f, 0.15f);
		case EImmortalEquipmentAffixType::CriticalDamage: return FMath::Clamp(Vary(0.012f * Budget), 0.02f, 0.50f);
		case EImmortalEquipmentAffixType::FireDamage:
		case EImmortalEquipmentAffixType::ThunderDamage:
		case EImmortalEquipmentAffixType::IceDamage:
			return FMath::Clamp(Vary(0.010f * Budget), 0.015f, 0.35f);
		case EImmortalEquipmentAffixType::LifeSteal: return FMath::Clamp(Vary(0.0018f * Budget), 0.002f, 0.08f);
		case EImmortalEquipmentAffixType::CultivationGain: return FMath::Clamp(Vary(0.005f * Budget), 0.005f, 0.20f);
		case EImmortalEquipmentAffixType::LootFind: return FMath::Clamp(Vary(0.004f * Budget), 0.004f, 0.15f);
		case EImmortalEquipmentAffixType::BossDamage: return FMath::Clamp(Vary(0.010f * Budget), 0.01f, 0.40f);
		default: return 0.0f;
		}
	}

	void RollEquipmentAffixes(FImmortalEquipmentItem& Item, const float Budget)
	{
		Item.Affixes.Reset();
		// Defense remains supported for old saves but the new twelve-affix pool follows the design document.
		TArray<EImmortalEquipmentAffixType> Types =
		{
			EImmortalEquipmentAffixType::Attack,
			EImmortalEquipmentAffixType::Health,
			EImmortalEquipmentAffixType::CriticalChance,
			EImmortalEquipmentAffixType::AttackSpeed,
			EImmortalEquipmentAffixType::CriticalDamage,
			EImmortalEquipmentAffixType::FireDamage,
			EImmortalEquipmentAffixType::ThunderDamage,
			EImmortalEquipmentAffixType::IceDamage,
			EImmortalEquipmentAffixType::LifeSteal,
			EImmortalEquipmentAffixType::CultivationGain,
			EImmortalEquipmentAffixType::LootFind,
			EImmortalEquipmentAffixType::BossDamage
		};
		for (int32 Index = Types.Num() - 1; Index > 0; --Index)
		{
			Types.Swap(Index, FMath::RandRange(0, Index));
		}
		const int32 AffixCount = FMath::Clamp(static_cast<int32>(ClampQuality(Item.Quality)) + 1, 1, 6);
		for (int32 Index = 0; Index < AffixCount; ++Index)
		{
			FImmortalEquipmentAffix& Affix = Item.Affixes.AddDefaulted_GetRef();
			Affix.Type = Types[Index];
			Affix.Value = RollAffixValue(Affix.Type, Budget);
		}
	}

	void UpdateForgedEquipmentName(FImmortalEquipmentItem& Item)
	{
		FImmortalEquipmentSetDefinition SetDefinition;
		const FString SetPrefix = UImmortalEquipmentLibrary::GetSetDefinition(Item.SetId, SetDefinition)
			? FString::Printf(TEXT("[%s] "), *SetDefinition.DisplayName.ToString())
			: FString();
		Item.DisplayName = FName(*FString::Printf(
			TEXT("[%s] %s%s %s +%d"),
			*UImmortalEquipmentLibrary::GetDisciplineText(Item.Discipline).ToString(),
			*SetPrefix,
			*UImmortalEquipmentLibrary::GetQualityText(Item.Quality).ToString(),
			*UImmortalEquipmentLibrary::GetSlotText(Item.Slot).ToString(),
			Item.EnhancementLevel));
	}

	FImmortalEquipmentItem GenerateEquipmentForSlot(
		const int32 ItemLevel,
		const EImmortalEquipmentSlot Slot,
		const EImmortalEquipmentQuality Quality,
		const EImmortalEquipmentDiscipline Discipline,
		const FName ForcedSetId,
		const bool bRollSet)
	{
		FImmortalEquipmentItem Item;
		Item.ItemId = FGuid::NewGuid();
		Item.ItemLevel = FMath::Max(ItemLevel, 1);
		Item.Slot = static_cast<int32>(Slot) >= 0 && static_cast<int32>(Slot) < static_cast<int32>(EImmortalEquipmentSlot::MAX)
			? Slot : EImmortalEquipmentSlot::Weapon;
		Item.Quality = ClampQuality(Quality);
		Item.Discipline = Discipline;
		FImmortalEquipmentSetDefinition SetDefinition;
		Item.SetId = UImmortalEquipmentLibrary::GetSetDefinition(ForcedSetId, SetDefinition)
			? ForcedSetId : (bRollSet ? RollSetId(Item.Quality) : NAME_None);

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
		case EImmortalEquipmentSlot::Bracers:
			Item.BaseAttackBonus = Vary(0.9f * Budget);
			Item.BaseDefenseBonus = Vary(0.35f * Budget);
			Item.BaseAttackSpeedBonus = FMath::Clamp(Vary(0.005f * Budget), 0.002f, 0.18f);
			break;
		case EImmortalEquipmentSlot::Belt:
			Item.BaseHealthBonus = Vary(8.0f * Budget);
			Item.BaseDefenseBonus = Vary(0.7f * Budget);
			break;
		case EImmortalEquipmentSlot::Boots:
			Item.BaseHealthBonus = Vary(5.0f * Budget);
			Item.BaseAttackSpeedBonus = FMath::Clamp(Vary(0.010f * Budget), 0.005f, 0.22f);
			break;
		case EImmortalEquipmentSlot::RingLeft:
		case EImmortalEquipmentSlot::RingRight:
			Item.BaseAttackBonus = Vary(0.75f * Budget);
			Item.BaseCriticalChanceBonus = FMath::Clamp(Vary(0.0035f * Budget), 0.002f, 0.12f);
			break;
		case EImmortalEquipmentSlot::Accessory:
			Item.BaseAttackBonus = Vary(0.8f * Budget);
			Item.BaseCriticalChanceBonus = FMath::Clamp(Vary(0.004f * Budget), 0.003f, 0.14f);
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
			RollQuality(MinimumQuality),
			RollDiscipline(),
			NAME_None,
			true);
	}

	void AccumulateSetTier(FImmortalEquipmentSetBonuses& Result, const FImmortalEquipmentSetTier& Tier)
	{
		Result.AttackMultiplierBonus += Tier.AttackMultiplierBonus;
		Result.DefenseMultiplierBonus += Tier.DefenseMultiplierBonus;
		Result.HealthMultiplierBonus += Tier.HealthMultiplierBonus;
		Result.AttackSpeedBonus += Tier.AttackSpeedBonus;
		Result.CriticalChanceBonus += Tier.CriticalChanceBonus;
		Result.CriticalDamageBonus += Tier.CriticalDamageBonus;
		Result.ThunderDamageBonus += Tier.ThunderDamageBonus;
		Result.CultivationGainBonus += Tier.CultivationGainBonus;
		Result.BossDamageBonus += Tier.BossDamageBonus;
		Result.FinalDamageBonus += Tier.FinalDamageBonus;
		Result.DamageReductionBonus += Tier.DamageReductionBonus;
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
	const EImmortalEquipmentQuality Quality,
	const EImmortalEquipmentDiscipline Discipline,
	const FName SetId)
{
	return GenerateEquipmentForSlot(ItemLevel, Slot, Quality, Discipline, SetId, false);
}

void UImmortalEquipmentLibrary::NormalizeForgingState(FImmortalEquipmentItem& Item)
{
	if (!Item.IsValid()) return;
	Item.ItemLevel = FMath::Max(Item.ItemLevel, 1);
	Item.EnhancementLevel = FMath::Clamp(Item.EnhancementLevel, 0, 15);
	Item.RefinementCount = FMath::Max(Item.RefinementCount, 0);
	if (static_cast<int32>(Item.Slot) < 0 || static_cast<int32>(Item.Slot) >= static_cast<int32>(EImmortalEquipmentSlot::MAX))
	{
		Item.Slot = EImmortalEquipmentSlot::Weapon;
	}
	Item.Quality = ClampQuality(Item.Quality);
	if (static_cast<int32>(Item.Discipline) < static_cast<int32>(EImmortalEquipmentDiscipline::Universal)
		|| static_cast<int32>(Item.Discipline) >= static_cast<int32>(EImmortalEquipmentDiscipline::MAX))
	{
		Item.Discipline = EImmortalEquipmentDiscipline::Universal;
	}
	FImmortalEquipmentSetDefinition SetDefinition;
	if (!Item.SetId.IsNone() && !GetSetDefinition(Item.SetId, SetDefinition)) Item.SetId = NAME_None;

	TArray<FImmortalEquipmentAffix> ValidAffixes;
	TSet<uint8> SeenTypes;
	for (const FImmortalEquipmentAffix& Affix : Item.Affixes)
	{
		const int32 TypeValue = static_cast<int32>(Affix.Type);
		const uint8 TypeKey = static_cast<uint8>(Affix.Type);
		if (ValidAffixes.Num() >= 6 || TypeValue < 0
			|| TypeValue >= static_cast<int32>(EImmortalEquipmentAffixType::MAX)
			|| !FMath::IsFinite(Affix.Value) || Affix.Value <= 0.0f || SeenTypes.Contains(TypeKey))
		{
			continue;
		}
		SeenTypes.Add(TypeKey);
		ValidAffixes.Add(Affix);
	}
	Item.Affixes = MoveTemp(ValidAffixes);

	const bool bMissingBaseStats = Item.BaseAttackBonus <= 0.0f
		&& Item.BaseDefenseBonus <= 0.0f
		&& Item.BaseHealthBonus <= 0.0f
		&& Item.BaseAttackSpeedBonus <= 0.0f
		&& Item.BaseCriticalChanceBonus <= 0.0f;
	if (bMissingBaseStats)
	{
		float AffixAttack = 0.0f;
		float AffixDefense = 0.0f;
		float AffixHealth = 0.0f;
		float AffixAttackSpeed = 0.0f;
		float AffixCriticalChance = 0.0f;
		for (const FImmortalEquipmentAffix& Affix : Item.Affixes)
		{
			switch (Affix.Type)
			{
			case EImmortalEquipmentAffixType::Attack: AffixAttack += Affix.Value; break;
			case EImmortalEquipmentAffixType::Defense: AffixDefense += Affix.Value; break;
			case EImmortalEquipmentAffixType::Health: AffixHealth += Affix.Value; break;
			case EImmortalEquipmentAffixType::AttackSpeed: AffixAttackSpeed += Affix.Value; break;
			case EImmortalEquipmentAffixType::CriticalChance: AffixCriticalChance += Affix.Value; break;
			default: break;
			}
		}
		const float InverseEnhancement = 1.0f / (1.0f + 0.08f * Item.EnhancementLevel);
		Item.BaseAttackBonus = FMath::Max((Item.AttackBonus - AffixAttack) * InverseEnhancement, 0.0f);
		Item.BaseDefenseBonus = FMath::Max((Item.DefenseBonus - AffixDefense) * InverseEnhancement, 0.0f);
		Item.BaseHealthBonus = FMath::Max((Item.HealthBonus - AffixHealth) * InverseEnhancement, 0.0f);
		Item.BaseAttackSpeedBonus = FMath::Max((Item.AttackSpeedBonus - AffixAttackSpeed) * InverseEnhancement, 0.0f);
		Item.BaseCriticalChanceBonus = FMath::Max((Item.CriticalChanceBonus - AffixCriticalChance) * InverseEnhancement, 0.0f);
	}
	Item.BaseAttackBonus = FMath::IsFinite(Item.BaseAttackBonus) ? FMath::Max(Item.BaseAttackBonus, 0.0f) : 0.0f;
	Item.BaseDefenseBonus = FMath::IsFinite(Item.BaseDefenseBonus) ? FMath::Max(Item.BaseDefenseBonus, 0.0f) : 0.0f;
	Item.BaseHealthBonus = FMath::IsFinite(Item.BaseHealthBonus) ? FMath::Max(Item.BaseHealthBonus, 0.0f) : 0.0f;
	Item.BaseAttackSpeedBonus = FMath::IsFinite(Item.BaseAttackSpeedBonus) ? FMath::Max(Item.BaseAttackSpeedBonus, 0.0f) : 0.0f;
	Item.BaseCriticalChanceBonus = FMath::IsFinite(Item.BaseCriticalChanceBonus) ? FMath::Max(Item.BaseCriticalChanceBonus, 0.0f) : 0.0f;
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
	case EImmortalEquipmentAffixType::Attack: return FText::FromString(FString::Printf(TEXT("攻击 +%.1f"), Affix.Value));
	case EImmortalEquipmentAffixType::Defense: return FText::FromString(FString::Printf(TEXT("防御 +%.1f"), Affix.Value));
	case EImmortalEquipmentAffixType::Health: return FText::FromString(FString::Printf(TEXT("生命 +%.1f"), Affix.Value));
	case EImmortalEquipmentAffixType::AttackSpeed: return FText::FromString(FString::Printf(TEXT("攻速 +%.1f%%"), Affix.Value * 100.0f));
	case EImmortalEquipmentAffixType::CriticalChance: return FText::FromString(FString::Printf(TEXT("暴击 +%.1f%%"), Affix.Value * 100.0f));
	case EImmortalEquipmentAffixType::CriticalDamage: return FText::FromString(FString::Printf(TEXT("暴伤 +%.1f%%"), Affix.Value * 100.0f));
	case EImmortalEquipmentAffixType::FireDamage: return FText::FromString(FString::Printf(TEXT("火伤 +%.1f%%"), Affix.Value * 100.0f));
	case EImmortalEquipmentAffixType::ThunderDamage: return FText::FromString(FString::Printf(TEXT("雷伤 +%.1f%%"), Affix.Value * 100.0f));
	case EImmortalEquipmentAffixType::IceDamage: return FText::FromString(FString::Printf(TEXT("冰伤 +%.1f%%"), Affix.Value * 100.0f));
	case EImmortalEquipmentAffixType::LifeSteal: return FText::FromString(FString::Printf(TEXT("吸血 +%.1f%%"), Affix.Value * 100.0f));
	case EImmortalEquipmentAffixType::CultivationGain: return FText::FromString(FString::Printf(TEXT("修炼效率 +%.1f%%"), Affix.Value * 100.0f));
	case EImmortalEquipmentAffixType::LootFind: return FText::FromString(FString::Printf(TEXT("装备掉率 +%.1f%%"), Affix.Value * 100.0f));
	case EImmortalEquipmentAffixType::BossDamage: return FText::FromString(FString::Printf(TEXT("首领伤害 +%.1f%%"), Affix.Value * 100.0f));
	default: return FText::GetEmpty();
	}
}

float UImmortalEquipmentLibrary::CalculateEquipmentPower(const FImmortalEquipmentItem& Item)
{
	if (!Item.IsValid()) return 0.0f;
	return Item.AttackBonus * 5.0f + Item.DefenseBonus * 4.0f + Item.HealthBonus * 0.2f
		+ Item.AttackSpeedBonus * 20.0f + Item.CriticalChanceBonus * 100.0f
		+ Item.CriticalDamageBonus * 70.0f
		+ (Item.FireDamageBonus + Item.ThunderDamageBonus + Item.IceDamageBonus) * 55.0f
		+ Item.LifeStealBonus * 160.0f + Item.CultivationGainBonus * 60.0f
		+ Item.LootFindBonus * 80.0f + Item.BossDamageBonus * 70.0f;
}

float UImmortalEquipmentLibrary::CalculateLoadoutPower(const TArray<FImmortalEquipmentItem>& EquippedItems)
{
	return CalculateLoadoutPowerWithBaseStats(EquippedItems, 0.0f, 0.0f, 0.0f);
}

float UImmortalEquipmentLibrary::CalculateLoadoutPowerWithBaseStats(
	const TArray<FImmortalEquipmentItem>& EquippedItems,
	const float BaseAttack,
	const float BaseDefense,
	const float BaseHealth)
{
	float Attack = FMath::Max(BaseAttack, 0.0f);
	float Defense = FMath::Max(BaseDefense, 0.0f);
	float Health = FMath::Max(BaseHealth, 0.0f);
	float AttackSpeed = 0.0f;
	float CriticalChance = 0.0f;
	float CriticalDamage = 0.0f;
	float FireDamage = 0.0f;
	float ThunderDamage = 0.0f;
	float IceDamage = 0.0f;
	float LifeSteal = 0.0f;
	float CultivationGain = 0.0f;
	float LootFind = 0.0f;
	float BossDamage = 0.0f;
	TSet<FGuid> CountedIds;
	for (const FImmortalEquipmentItem& Item : EquippedItems)
	{
		if (!Item.IsValid() || CountedIds.Contains(Item.ItemId)) continue;
		CountedIds.Add(Item.ItemId);
		Attack += FMath::Max(Item.AttackBonus, 0.0f);
		Defense += FMath::Max(Item.DefenseBonus, 0.0f);
		Health += FMath::Max(Item.HealthBonus, 0.0f);
		AttackSpeed += FMath::Max(Item.AttackSpeedBonus, 0.0f);
		CriticalChance += FMath::Max(Item.CriticalChanceBonus, 0.0f);
		CriticalDamage += FMath::Max(Item.CriticalDamageBonus, 0.0f);
		FireDamage += FMath::Max(Item.FireDamageBonus, 0.0f);
		ThunderDamage += FMath::Max(Item.ThunderDamageBonus, 0.0f);
		IceDamage += FMath::Max(Item.IceDamageBonus, 0.0f);
		LifeSteal += FMath::Max(Item.LifeStealBonus, 0.0f);
		CultivationGain += FMath::Max(Item.CultivationGainBonus, 0.0f);
		LootFind += FMath::Max(Item.LootFindBonus, 0.0f);
		BossDamage += FMath::Max(Item.BossDamageBonus, 0.0f);
	}
	const FImmortalEquipmentSetBonuses Sets = CalculateSetBonuses(EquippedItems);
	Attack *= 1.0f + FMath::Max(Sets.AttackMultiplierBonus, 0.0f);
	Defense *= 1.0f + FMath::Max(Sets.DefenseMultiplierBonus, 0.0f);
	Health *= 1.0f + FMath::Max(Sets.HealthMultiplierBonus, 0.0f);
	AttackSpeed += FMath::Max(Sets.AttackSpeedBonus, 0.0f);
	CriticalChance += FMath::Max(Sets.CriticalChanceBonus, 0.0f);
	CriticalDamage += FMath::Max(Sets.CriticalDamageBonus, 0.0f);
	ThunderDamage += FMath::Max(Sets.ThunderDamageBonus, 0.0f);
	CultivationGain += FMath::Max(Sets.CultivationGainBonus, 0.0f);
	BossDamage += FMath::Max(Sets.BossDamageBonus, 0.0f);

	// Percentage set effects must scale the aggregate build they actually affect.
	// Treating a six-piece final-damage tier as a fixed number allowed a tiny raw-stat
	// upgrade to dismantle the set even when the real combat result became much weaker.
	const float DamageScore = (Attack * 5.0f
		+ AttackSpeed * 20.0f
		+ CriticalChance * 100.0f
		+ CriticalDamage * 70.0f
		+ (FireDamage + ThunderDamage + IceDamage) * 55.0f
		+ BossDamage * 70.0f)
		* (1.0f + FMath::Clamp(Sets.FinalDamageBonus, 0.0f, 3.0f));
	const float SurvivalScore = (Defense * 4.0f + Health * 0.2f)
		/ (1.0f - FMath::Clamp(Sets.DamageReductionBonus, 0.0f, 0.75f));
	const float UtilityScore = LifeSteal * 160.0f
		+ CultivationGain * 60.0f
		+ LootFind * 80.0f;
	return DamageScore + SurvivalScore + UtilityScore;
}

FLinearColor UImmortalEquipmentLibrary::GetQualityColor(const EImmortalEquipmentQuality Quality)
{
	switch (ClampQuality(Quality))
	{
	case EImmortalEquipmentQuality::Uncommon: return FLinearColor(0.25f, 0.95f, 0.35f, 1.0f);
	case EImmortalEquipmentQuality::Rare: return FLinearColor(0.25f, 0.55f, 1.0f, 1.0f);
	case EImmortalEquipmentQuality::Epic: return FLinearColor(0.75f, 0.30f, 1.0f, 1.0f);
	case EImmortalEquipmentQuality::Legendary: return FLinearColor(1.0f, 0.62f, 0.12f, 1.0f);
	case EImmortalEquipmentQuality::Immortal: return FLinearColor(0.20f, 0.95f, 1.0f, 1.0f);
	case EImmortalEquipmentQuality::Divine: return FLinearColor(1.0f, 0.20f, 0.32f, 1.0f);
	default: return FLinearColor(0.92f, 0.92f, 0.92f, 1.0f);
	}
}

FText UImmortalEquipmentLibrary::GetQualityText(const EImmortalEquipmentQuality Quality)
{
	switch (ClampQuality(Quality))
	{
	case EImmortalEquipmentQuality::Uncommon: return FText::FromString(TEXT("灵品"));
	case EImmortalEquipmentQuality::Rare: return FText::FromString(TEXT("玄品"));
	case EImmortalEquipmentQuality::Epic: return FText::FromString(TEXT("地品"));
	case EImmortalEquipmentQuality::Legendary: return FText::FromString(TEXT("天品"));
	case EImmortalEquipmentQuality::Immortal: return FText::FromString(TEXT("仙品"));
	case EImmortalEquipmentQuality::Divine: return FText::FromString(TEXT("神品"));
	default: return FText::FromString(TEXT("凡品"));
	}
}

FText UImmortalEquipmentLibrary::GetSlotText(const EImmortalEquipmentSlot Slot)
{
	switch (Slot)
	{
	case EImmortalEquipmentSlot::Head: return FText::FromString(TEXT("头盔"));
	case EImmortalEquipmentSlot::Chest: return FText::FromString(TEXT("护甲"));
	case EImmortalEquipmentSlot::Bracers: return FText::FromString(TEXT("护腕"));
	case EImmortalEquipmentSlot::Belt: return FText::FromString(TEXT("腰带"));
	case EImmortalEquipmentSlot::Boots: return FText::FromString(TEXT("鞋子"));
	case EImmortalEquipmentSlot::RingLeft: return FText::FromString(TEXT("戒指一"));
	case EImmortalEquipmentSlot::RingRight: return FText::FromString(TEXT("戒指二"));
	case EImmortalEquipmentSlot::Accessory: return FText::FromString(TEXT("项链"));
	default: return FText::FromString(TEXT("武器"));
	}
}

FText UImmortalEquipmentLibrary::GetDisciplineText(const EImmortalEquipmentDiscipline Discipline)
{
	switch (Discipline)
	{
	case EImmortalEquipmentDiscipline::Body: return FText::FromString(TEXT("体修"));
	case EImmortalEquipmentDiscipline::Dharma: return FText::FromString(TEXT("法修"));
	case EImmortalEquipmentDiscipline::Sword: return FText::FromString(TEXT("剑修"));
	case EImmortalEquipmentDiscipline::Poison: return FText::FromString(TEXT("毒修"));
	case EImmortalEquipmentDiscipline::Thunder: return FText::FromString(TEXT("雷修"));
	default: return FText::FromString(TEXT("通用"));
	}
}

TArray<FName> UImmortalEquipmentLibrary::GetKnownSetIds()
{
	TArray<FName> Result;
	GetFallbackSetCatalog().GetKeys(Result);
	if (const UDataTable* Table = GetOptionalEquipmentSetTable())
	{
		for (const FName RowName : Table->GetRowNames()) Result.AddUnique(RowName);
	}
	Result.Sort(FNameLexicalLess());
	return Result;
}

bool UImmortalEquipmentLibrary::GetSetDefinition(
	const FName SetId,
	FImmortalEquipmentSetDefinition& OutDefinition)
{
	if (SetId.IsNone()) return false;
	if (const UDataTable* Table = GetOptionalEquipmentSetTable())
	{
		if (const FImmortalEquipmentSetDefinition* Row = Table->FindRow<FImmortalEquipmentSetDefinition>(
			SetId, TEXT("Equipment set lookup"), false))
		{
			OutDefinition = *Row;
			if (OutDefinition.SetId.IsNone()) OutDefinition.SetId = SetId;
			return true;
		}
	}
	if (const FImmortalEquipmentSetDefinition* Found = GetFallbackSetCatalog().Find(SetId))
	{
		OutDefinition = *Found;
		return true;
	}
	return false;
}

FImmortalEquipmentSetBonuses UImmortalEquipmentLibrary::CalculateSetBonuses(
	const TArray<FImmortalEquipmentItem>& EquippedItems)
{
	FImmortalEquipmentSetBonuses Result;
	TSet<FGuid> CountedItemIds;
	for (const FImmortalEquipmentItem& Item : EquippedItems)
	{
		FImmortalEquipmentSetDefinition Definition;
		if (!Item.IsValid() || Item.SetId.IsNone() || CountedItemIds.Contains(Item.ItemId)
			|| !GetSetDefinition(Item.SetId, Definition))
		{
			continue;
		}
		CountedItemIds.Add(Item.ItemId);
		Result.PieceCounts.FindOrAdd(Item.SetId)++;
	}

	for (const TPair<FName, int32>& Pair : Result.PieceCounts)
	{
		FImmortalEquipmentSetDefinition Definition;
		if (!GetSetDefinition(Pair.Key, Definition)) continue;
		for (const FImmortalEquipmentSetTier& Tier : Definition.Tiers)
		{
			if (Tier.RequiredPieces >= 2 && Pair.Value >= Tier.RequiredPieces) AccumulateSetTier(Result, Tier);
		}
	}
	return Result;
}

FText UImmortalEquipmentLibrary::GetSetSummaryText(const TArray<FImmortalEquipmentItem>& EquippedItems)
{
	const FImmortalEquipmentSetBonuses Bonuses = CalculateSetBonuses(EquippedItems);
	if (Bonuses.PieceCounts.IsEmpty()) return FText::FromString(TEXT("套装：无"));

	FString Summary(TEXT("套装进度"));
	for (const FName SetId : GetKnownSetIds())
	{
		const int32 Count = Bonuses.PieceCounts.FindRef(SetId);
		if (Count <= 0) continue;
		FImmortalEquipmentSetDefinition Definition;
		if (!GetSetDefinition(SetId, Definition)) continue;
		Summary += FString::Printf(TEXT("\n%s %d/6"), *Definition.DisplayName.ToString(), Count);
		for (const FImmortalEquipmentSetTier& Tier : Definition.Tiers)
		{
			Summary += FString::Printf(TEXT("\n  %d件 %s %s"), Tier.RequiredPieces,
				Count >= Tier.RequiredPieces ? TEXT("√") : TEXT("○"), *Tier.Description.ToString());
		}
	}
	return FText::FromString(Summary);
}
