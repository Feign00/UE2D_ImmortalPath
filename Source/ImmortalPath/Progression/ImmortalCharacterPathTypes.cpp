// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalCharacterPathTypes.h"

#include "Engine/DataTable.h"
#include "Misc/PackageName.h"

namespace
{
	FImmortalSpiritRootDefinition RootDefinition(
		const TCHAR* Name, const TCHAR* Description, const TCHAR* Glyph,
		const EImmortalElementType Element, const float Weight, const float Cultivation,
		const float Damage, const float Pill)
	{
		FImmortalSpiritRootDefinition Result;
		Result.DisplayName = FText::FromString(Name);
		Result.Description = FText::FromString(Description);
		Result.IconGlyph = FText::FromString(Glyph);
		Result.Element = Element;
		Result.RollWeight = Weight;
		Result.CultivationRateBonus = Cultivation;
		Result.MatchingElementDamageBonus = Damage;
		Result.PillEffectBonus = Pill;
		return Result;
	}

	FImmortalCultivationPathDefinition PathDefinition(
		const TCHAR* Name, const TCHAR* Description, const TCHAR* Glyph,
		const TCHAR* SkillName, const TCHAR* SkillDescription,
		const EImmortalPathSkillEffect Effect, const EImmortalEquipmentDiscipline Discipline,
		const EImmortalElementType Element, const int32 Trigger, const float Magnitude,
		const float Attack, const float Defense, const float Health, const float Mana,
		const float Speed, const float Critical, const float Reduction, const float Cultivation)
	{
		FImmortalCultivationPathDefinition Result;
		Result.DisplayName = FText::FromString(Name);
		Result.Description = FText::FromString(Description);
		Result.IconGlyph = FText::FromString(Glyph);
		Result.SkillName = FText::FromString(SkillName);
		Result.SkillDescription = FText::FromString(SkillDescription);
		Result.SkillEffect = Effect;
		Result.EquipmentDiscipline = Discipline;
		Result.SkillElement = Element;
		Result.AttacksPerSkill = Trigger;
		Result.SkillMagnitude = Magnitude;
		Result.AttackPercent = Attack;
		Result.DefensePercent = Defense;
		Result.HealthPercent = Health;
		Result.ManaPercent = Mana;
		Result.AttackSpeedBonus = Speed;
		Result.CriticalChanceBonus = Critical;
		Result.DamageReduction = Reduction;
		Result.CultivationRateBonus = Cultivation;
		return Result;
	}

	const TMap<EImmortalSpiritRoot, FImmortalSpiritRootDefinition>& GetRootFallbacks()
	{
		static const TMap<EImmortalSpiritRoot, FImmortalSpiritRootDefinition> Catalog =
		{
			{EImmortalSpiritRoot::Metal, RootDefinition(TEXT("金灵根"), TEXT("金气锋锐，强化金系剑诀与破敌之力。"), TEXT("金"), EImmortalElementType::Metal, 12.0f, 0.08f, 0.18f, 0.05f)},
			{EImmortalSpiritRoot::Wood, RootDefinition(TEXT("木灵根"), TEXT("生机绵长，擅长灵药、恢复与木毒之术。"), TEXT("木"), EImmortalElementType::Wood, 12.0f, 0.10f, 0.16f, 0.18f)},
			{EImmortalSpiritRoot::Water, RootDefinition(TEXT("水灵根"), TEXT("上善若水，修炼平稳且善纳丹药灵性。"), TEXT("水"), EImmortalElementType::Water, 12.0f, 0.12f, 0.15f, 0.15f)},
			{EImmortalSpiritRoot::Fire, RootDefinition(TEXT("火灵根"), TEXT("真火炽烈，火系术法爆发最为强横。"), TEXT("火"), EImmortalElementType::Fire, 12.0f, 0.07f, 0.22f, 0.05f)},
			{EImmortalSpiritRoot::Earth, RootDefinition(TEXT("土灵根"), TEXT("厚土载物，兼顾稳固修炼与护体术法。"), TEXT("土"), EImmortalElementType::Earth, 12.0f, 0.08f, 0.17f, 0.08f)},
			{EImmortalSpiritRoot::Wind, RootDefinition(TEXT("风灵根"), TEXT("风行无迹，修炼吐纳迅捷并强化风系术法。"), TEXT("风"), EImmortalElementType::Wind, 12.0f, 0.14f, 0.18f, 0.06f)},
			{EImmortalSpiritRoot::Thunder, RootDefinition(TEXT("雷灵根"), TEXT("雷霆主杀，修炼艰深但雷法威能冠绝。"), TEXT("雷"), EImmortalElementType::Thunder, 10.0f, 0.06f, 0.25f, 0.04f)},
			{EImmortalSpiritRoot::Ice, RootDefinition(TEXT("冰灵根"), TEXT("玄冰凝神，强化冰系术法与丹药吸收。"), TEXT("冰"), EImmortalElementType::Ice, 6.0f, 0.10f, 0.20f, 0.10f)},
			{EImmortalSpiritRoot::Mutated, RootDefinition(TEXT("变异灵根"), TEXT("天地异变所生，可共鸣全部元素，修炼与丹效俱佳。"), TEXT("异"), EImmortalElementType::Chaos, 2.0f, 0.22f, 0.18f, 0.22f)}
		};
		return Catalog;
	}

	const TMap<EImmortalCultivationPath, FImmortalCultivationPathDefinition>& GetPathFallbacks()
	{
		static const TMap<EImmortalCultivationPath, FImmortalCultivationPathDefinition> Catalog =
		{
			{EImmortalCultivationPath::Body, PathDefinition(TEXT("体修"), TEXT("锤炼肉身，以高生命、高防御和减伤正面承受妖兽攻势。"), TEXT("体"),
				TEXT("撼岳震"), TEXT("每 8 次攻击震荡周围敌人并凝聚护体罡气。"), EImmortalPathSkillEffect::BodyQuake,
				EImmortalEquipmentDiscipline::Body, EImmortalElementType::Earth, 8, 1.25f, 0.05f, 0.18f, 0.25f, 0.0f, 0.0f, 0.0f, 0.10f, 0.02f)},
			{EImmortalCultivationPath::Dharma, PathDefinition(TEXT("法修"), TEXT("以灵力御使五行术法，拥有高灵力、范围伤害与修炼效率。"), TEXT("法"),
				TEXT("五行轮转"), TEXT("每 7 次攻击引爆与自身灵根共鸣的范围术法。"), EImmortalPathSkillEffect::FiveElementsSpell,
				EImmortalEquipmentDiscipline::Dharma, EImmortalElementType::None, 7, 1.65f, 0.14f, 0.04f, 0.0f, 0.30f, 0.04f, 0.03f, 0.0f, 0.06f)},
			{EImmortalCultivationPath::Sword, PathDefinition(TEXT("剑修"), TEXT("人剑合一，追求攻击、暴击与连续剑阵的极致斩杀。"), TEXT("剑"),
				TEXT("归元剑阵"), TEXT("每 6 次攻击发动三段剑光，优先斩击当前强敌。"), EImmortalPathSkillEffect::SwordArray,
				EImmortalEquipmentDiscipline::Sword, EImmortalElementType::Metal, 6, 2.10f, 0.20f, 0.0f, 0.0f, 0.0f, 0.08f, 0.08f, 0.0f, 0.0f)},
			{EImmortalCultivationPath::Poison, PathDefinition(TEXT("毒修"), TEXT("以木毒蚀骨，在群战中持续扩散毒雾并汲取生机。"), TEXT("毒"),
				TEXT("万毒蚀心"), TEXT("每 7 次攻击释放毒云，伤害范围妖兽并回收部分生机。"), EImmortalPathSkillEffect::PoisonCloud,
				EImmortalEquipmentDiscipline::Poison, EImmortalElementType::Wood, 7, 1.50f, 0.10f, 0.05f, 0.08f, 0.0f, 0.05f, 0.02f, 0.03f, 0.03f)},
			{EImmortalCultivationPath::Thunder, PathDefinition(TEXT("雷修"), TEXT("引雷淬体，以高攻速和连锁天雷快速清扫敌群。"), TEXT("雷"),
				TEXT("紫霄雷链"), TEXT("每 7 次攻击召来连锁雷霆，依次轰击最多五名敌人。"), EImmortalPathSkillEffect::ThunderChain,
				EImmortalEquipmentDiscipline::Thunder, EImmortalElementType::Thunder, 7, 1.80f, 0.15f, 0.0f, 0.0f, 0.10f, 0.12f, 0.05f, 0.0f, 0.02f)}
		};
		return Catalog;
	}

	FName RootRowName(const EImmortalSpiritRoot Root)
	{
		switch (Root)
		{
		case EImmortalSpiritRoot::Metal: return TEXT("Metal");
		case EImmortalSpiritRoot::Wood: return TEXT("Wood");
		case EImmortalSpiritRoot::Water: return TEXT("Water");
		case EImmortalSpiritRoot::Fire: return TEXT("Fire");
		case EImmortalSpiritRoot::Earth: return TEXT("Earth");
		case EImmortalSpiritRoot::Wind: return TEXT("Wind");
		case EImmortalSpiritRoot::Thunder: return TEXT("Thunder");
		case EImmortalSpiritRoot::Ice: return TEXT("Ice");
		case EImmortalSpiritRoot::Mutated: return TEXT("Mutated");
		default: return NAME_None;
		}
	}

	FName PathRowName(const EImmortalCultivationPath Path)
	{
		switch (Path)
		{
		case EImmortalCultivationPath::Body: return TEXT("Body");
		case EImmortalCultivationPath::Dharma: return TEXT("Dharma");
		case EImmortalCultivationPath::Sword: return TEXT("Sword");
		case EImmortalCultivationPath::Poison: return TEXT("Poison");
		case EImmortalCultivationPath::Thunder: return TEXT("Thunder");
		default: return NAME_None;
		}
	}

	UDataTable* LoadOptionalTable(const TCHAR* PackagePath, const TCHAR* AssetName)
	{
		const FString PackageName(PackagePath);
		if (!FPackageName::DoesPackageExist(PackageName)) return nullptr;
		return LoadObject<UDataTable>(nullptr, *FString::Printf(TEXT("%s.%s"), PackagePath, AssetName));
	}
}

TArray<EImmortalSpiritRoot> UImmortalCharacterPathLibrary::GetKnownSpiritRoots()
{
	return { EImmortalSpiritRoot::Metal, EImmortalSpiritRoot::Wood, EImmortalSpiritRoot::Water,
		EImmortalSpiritRoot::Fire, EImmortalSpiritRoot::Earth, EImmortalSpiritRoot::Wind,
		EImmortalSpiritRoot::Thunder, EImmortalSpiritRoot::Ice, EImmortalSpiritRoot::Mutated };
}

bool UImmortalCharacterPathLibrary::GetSpiritRootDefinition(
	const EImmortalSpiritRoot Root,
	FImmortalSpiritRootDefinition& OutDefinition)
{
	static TWeakObjectPtr<UDataTable> Table = LoadOptionalTable(TEXT("/Game/GAME/Data/DT_SpiritRoots"), TEXT("DT_SpiritRoots"));
	const FName Row = RootRowName(Root);
	if (Table.IsValid() && !Row.IsNone())
	{
		if (const FImmortalSpiritRootDefinition* Found = Table->FindRow<FImmortalSpiritRootDefinition>(Row, TEXT("Spirit root lookup"), false))
		{
			OutDefinition = *Found;
			return true;
		}
	}
	if (const FImmortalSpiritRootDefinition* Found = GetRootFallbacks().Find(Root))
	{
		OutDefinition = *Found;
		return true;
	}
	return false;
}

FImmortalSpiritRootState UImmortalCharacterPathLibrary::GenerateSpiritRootFromRoll(
	const float TypeRoll,
	const float PurityRoll)
{
	FImmortalSpiritRootState Result;
	const TArray<EImmortalSpiritRoot> Roots = GetKnownSpiritRoots();
	float TotalWeight = 0.0f;
	for (const EImmortalSpiritRoot Root : Roots)
	{
		FImmortalSpiritRootDefinition Definition;
		if (GetSpiritRootDefinition(Root, Definition)) TotalWeight += FMath::Max(Definition.RollWeight, 0.0f);
	}
	float Cursor = FMath::Clamp(TypeRoll, 0.0f, 0.999999f) * FMath::Max(TotalWeight, KINDA_SMALL_NUMBER);
	for (const EImmortalSpiritRoot Root : Roots)
	{
		FImmortalSpiritRootDefinition Definition;
		if (!GetSpiritRootDefinition(Root, Definition)) continue;
		Cursor -= FMath::Max(Definition.RollWeight, 0.0f);
		if (Cursor <= 0.0f)
		{
			Result.Root = Root;
			break;
		}
	}
	if (Result.Root == EImmortalSpiritRoot::Unawakened && !Roots.IsEmpty()) Result.Root = Roots.Last();
	const float MinimumPurity = Result.Root == EImmortalSpiritRoot::Mutated ? 0.90f : 0.60f;
	Result.Purity = FMath::Lerp(MinimumPurity, 1.0f, FMath::Clamp(PurityRoll, 0.0f, 1.0f));
	NormalizeSpiritRoot(Result);
	return Result;
}

FImmortalSpiritRootState UImmortalCharacterPathLibrary::GenerateRandomSpiritRoot()
{
	return GenerateSpiritRootFromRoll(FMath::FRand(), FMath::FRand());
}

void UImmortalCharacterPathLibrary::NormalizeSpiritRoot(FImmortalSpiritRootState& State)
{
	FImmortalSpiritRootDefinition Definition;
	if (!GetSpiritRootDefinition(State.Root, Definition))
	{
		State = FImmortalSpiritRootState();
		return;
	}
	const float MinimumPurity = State.Root == EImmortalSpiritRoot::Mutated ? 0.90f : 0.60f;
	State.Purity = FMath::Clamp(State.Purity, MinimumPurity, 1.0f);
}

float UImmortalCharacterPathLibrary::CalculateCultivationRateMultiplier(const FImmortalSpiritRootState& State)
{
	FImmortalSpiritRootDefinition Definition;
	if (!GetSpiritRootDefinition(State.Root, Definition)) return 1.0f;
	return 1.0f + FMath::Max(Definition.CultivationRateBonus, 0.0f) * FMath::Clamp(State.Purity, 0.0f, 1.0f);
}

float UImmortalCharacterPathLibrary::CalculateElementDamageMultiplier(
	const FImmortalSpiritRootState& State,
	const EImmortalElementType Element)
{
	FImmortalSpiritRootDefinition Definition;
	if (Element == EImmortalElementType::None || !GetSpiritRootDefinition(State.Root, Definition)) return 1.0f;
	const bool bMatches = Definition.Element == EImmortalElementType::Chaos || Definition.Element == Element;
	return bMatches
		? 1.0f + FMath::Max(Definition.MatchingElementDamageBonus, 0.0f) * FMath::Clamp(State.Purity, 0.0f, 1.0f)
		: 1.0f;
}

float UImmortalCharacterPathLibrary::CalculatePillEffectMultiplier(const FImmortalSpiritRootState& State)
{
	FImmortalSpiritRootDefinition Definition;
	if (!GetSpiritRootDefinition(State.Root, Definition)) return 1.0f;
	return 1.0f + FMath::Max(Definition.PillEffectBonus, 0.0f) * FMath::Clamp(State.Purity, 0.0f, 1.0f);
}

float UImmortalCharacterPathLibrary::CalculateEffectivePillMagnitude(
	const FImmortalSpiritRootState& State,
	const float BaseMagnitude,
	const bool bBonusOnly)
{
	const float Multiplier = CalculatePillEffectMultiplier(State);
	return bBonusOnly
		? 1.0f + FMath::Max(BaseMagnitude - 1.0f, 0.0f) * Multiplier
		: FMath::Max(BaseMagnitude, 0.0f) * Multiplier;
}

FLinearColor UImmortalCharacterPathLibrary::GetSpiritRootColor(const EImmortalSpiritRoot Root)
{
	switch (Root)
	{
	case EImmortalSpiritRoot::Metal: return FLinearColor(0.95f, 0.85f, 0.48f);
	case EImmortalSpiritRoot::Wood: return FLinearColor(0.30f, 0.92f, 0.42f);
	case EImmortalSpiritRoot::Water: return FLinearColor(0.25f, 0.62f, 1.0f);
	case EImmortalSpiritRoot::Fire: return FLinearColor(1.0f, 0.30f, 0.12f);
	case EImmortalSpiritRoot::Earth: return FLinearColor(0.72f, 0.52f, 0.28f);
	case EImmortalSpiritRoot::Wind: return FLinearColor(0.45f, 1.0f, 0.84f);
	case EImmortalSpiritRoot::Thunder: return FLinearColor(0.68f, 0.42f, 1.0f);
	case EImmortalSpiritRoot::Ice: return FLinearColor(0.62f, 0.92f, 1.0f);
	case EImmortalSpiritRoot::Mutated: return FLinearColor(1.0f, 0.45f, 0.92f);
	default: return FLinearColor(0.65f, 0.65f, 0.68f);
	}
}

FText UImmortalCharacterPathLibrary::GetElementText(const EImmortalElementType Element)
{
	switch (Element)
	{
	case EImmortalElementType::Metal: return FText::FromString(TEXT("金"));
	case EImmortalElementType::Wood: return FText::FromString(TEXT("木"));
	case EImmortalElementType::Water: return FText::FromString(TEXT("水"));
	case EImmortalElementType::Fire: return FText::FromString(TEXT("火"));
	case EImmortalElementType::Earth: return FText::FromString(TEXT("土"));
	case EImmortalElementType::Wind: return FText::FromString(TEXT("风"));
	case EImmortalElementType::Thunder: return FText::FromString(TEXT("雷"));
	case EImmortalElementType::Ice: return FText::FromString(TEXT("冰"));
	case EImmortalElementType::Chaos: return FText::FromString(TEXT("混沌"));
	default: return FText::FromString(TEXT("无"));
	}
}

TArray<EImmortalCultivationPath> UImmortalCharacterPathLibrary::GetKnownCultivationPaths()
{
	return { EImmortalCultivationPath::Body, EImmortalCultivationPath::Dharma,
		EImmortalCultivationPath::Sword, EImmortalCultivationPath::Poison, EImmortalCultivationPath::Thunder };
}

bool UImmortalCharacterPathLibrary::GetCultivationPathDefinition(
	const EImmortalCultivationPath Path,
	FImmortalCultivationPathDefinition& OutDefinition)
{
	static TWeakObjectPtr<UDataTable> Table = LoadOptionalTable(TEXT("/Game/GAME/Data/DT_CultivationPaths"), TEXT("DT_CultivationPaths"));
	const FName Row = PathRowName(Path);
	if (Table.IsValid() && !Row.IsNone())
	{
		if (const FImmortalCultivationPathDefinition* Found = Table->FindRow<FImmortalCultivationPathDefinition>(Row, TEXT("Path lookup"), false))
		{
			OutDefinition = *Found;
			return true;
		}
	}
	if (const FImmortalCultivationPathDefinition* Found = GetPathFallbacks().Find(Path))
	{
		OutDefinition = *Found;
		return true;
	}
	return false;
}

void UImmortalCharacterPathLibrary::NormalizeCultivationPath(FImmortalCultivationPathState& State)
{
	FImmortalCultivationPathDefinition Definition;
	if (State.Path != EImmortalCultivationPath::Unselected && !GetCultivationPathDefinition(State.Path, Definition))
	{
		State.Path = EImmortalCultivationPath::Unselected;
	}
	State.SwitchCount = FMath::Clamp(State.SwitchCount, 0, 999);
}

FImmortalCharacterPathBonuses UImmortalCharacterPathLibrary::CalculatePathBonuses(
	const FImmortalCultivationPathState& State)
{
	FImmortalCharacterPathBonuses Result;
	FImmortalCultivationPathDefinition Definition;
	if (!GetCultivationPathDefinition(State.Path, Definition)) return Result;
	Result.AttackMultiplier += FMath::Max(Definition.AttackPercent, 0.0f);
	Result.DefenseMultiplier += FMath::Max(Definition.DefensePercent, 0.0f);
	Result.HealthMultiplier += FMath::Max(Definition.HealthPercent, 0.0f);
	Result.ManaMultiplier += FMath::Max(Definition.ManaPercent, 0.0f);
	Result.AttackSpeedBonus = FMath::Max(Definition.AttackSpeedBonus, 0.0f);
	Result.CriticalChanceBonus = FMath::Max(Definition.CriticalChanceBonus, 0.0f);
	Result.DamageReduction = FMath::Clamp(Definition.DamageReduction, 0.0f, 0.75f);
	Result.CultivationRateMultiplier += FMath::Max(Definition.CultivationRateBonus, 0.0f);
	return Result;
}

FImmortalCraftingCost UImmortalCharacterPathLibrary::GetPathSwitchCost(
	const FImmortalCultivationPathState& State,
	const EImmortalCultivationPath NewPath)
{
	FImmortalCraftingCost Result;
	FImmortalCultivationPathDefinition Definition;
	if (!State.IsSelected() || State.Path == NewPath || !GetCultivationPathDefinition(NewPath, Definition)) return Result;
	const int32 Rank = FMath::Clamp(State.SwitchCount + 1, 1, 100);
	Result.SpiritStones = 1000 * Rank;
	FImmortalCraftingMaterialCost Core;
	Core.MaterialId = TEXT("DemonCore");
	Core.Quantity = 2 * Rank;
	Result.Materials.Add(Core);
	FImmortalCraftingMaterialCost Liquid;
	Liquid.MaterialId = TEXT("SpiritLiquid");
	Liquid.Quantity = Rank;
	Result.Materials.Add(Liquid);
	return Result;
}

bool UImmortalCharacterPathLibrary::IsEquipmentCompatible(
	const EImmortalCultivationPath Path,
	const EImmortalEquipmentDiscipline Discipline)
{
	if (Path == EImmortalCultivationPath::Unselected || Discipline == EImmortalEquipmentDiscipline::Universal) return true;
	FImmortalCultivationPathDefinition Definition;
	return GetCultivationPathDefinition(Path, Definition) && Definition.EquipmentDiscipline == Discipline;
}

FLinearColor UImmortalCharacterPathLibrary::GetCultivationPathColor(const EImmortalCultivationPath Path)
{
	switch (Path)
	{
	case EImmortalCultivationPath::Body: return FLinearColor(0.92f, 0.58f, 0.25f);
	case EImmortalCultivationPath::Dharma: return FLinearColor(0.35f, 0.82f, 1.0f);
	case EImmortalCultivationPath::Sword: return FLinearColor(0.92f, 0.92f, 0.78f);
	case EImmortalCultivationPath::Poison: return FLinearColor(0.45f, 0.95f, 0.32f);
	case EImmortalCultivationPath::Thunder: return FLinearColor(0.68f, 0.42f, 1.0f);
	default: return FLinearColor(0.65f, 0.65f, 0.68f);
	}
}
