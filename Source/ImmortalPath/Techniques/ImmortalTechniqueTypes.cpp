// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalTechniqueTypes.h"

#include "Engine/DataTable.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	FImmortalCraftingMaterialCost TechniqueMaterial(const TCHAR* Id, const int32 Quantity)
	{
		FImmortalCraftingMaterialCost Result;
		Result.MaterialId = FName(Id);
		Result.Quantity = Quantity;
		return Result;
	}

	FImmortalCraftingCost TechniqueCost(const int32 Stones, std::initializer_list<FImmortalCraftingMaterialCost> Materials)
	{
		FImmortalCraftingCost Result;
		Result.SpiritStones = Stones;
		Result.Materials = Materials;
		return Result;
	}

	FImmortalTechniqueDefinition TechniqueDefinition(
		const TCHAR* Name,
		const TCHAR* Description,
		const TCHAR* Glyph,
		const EImmortalTechniqueQuality Quality,
		const EImmortalTechniqueActiveEffect ActiveEffect,
		const EImmortalElementType Element,
		const int32 Trigger,
		const int32 UltimateTrigger,
		const float ActiveMagnitude,
		const float UltimateMagnitude,
		const float Attack,
		const float Defense,
		const float Health,
		const float AttackSpeed,
		const float Critical,
		const float CultivationRate,
		const int32 Stage,
		const int32 Realm,
		const FImmortalCraftingCost& LearningCost)
	{
		FImmortalTechniqueDefinition Result;
		Result.DisplayName = FText::FromString(Name);
		Result.Description = FText::FromString(Description);
		Result.IconGlyph = FText::FromString(Glyph);
		Result.Quality = Quality;
		Result.ActiveEffect = ActiveEffect;
		Result.Element = Element;
		Result.AttacksPerTrigger = Trigger;
		Result.ActivesPerUltimate = UltimateTrigger;
		Result.ActiveMagnitude = ActiveMagnitude;
		Result.UltimateMagnitude = UltimateMagnitude;
		Result.AttackPercent = Attack;
		Result.DefensePercent = Defense;
		Result.HealthPercent = Health;
		Result.AttackSpeedBonus = AttackSpeed;
		Result.CriticalChanceBonus = Critical;
		Result.CultivationRateBonus = CultivationRate;
		Result.MinimumQingyunStage = Stage;
		Result.MinimumRealmIndex = Realm;
		Result.LearningCost = LearningCost;
		return Result;
	}

	const TMap<FName, FImmortalTechniqueDefinition>& GetTechniqueFallbackCatalog()
	{
		static const TMap<FName, FImmortalTechniqueDefinition> Catalog =
		{
			{TEXT("BasicBreathing"), TechniqueDefinition(
				TEXT("基础吐纳诀"), TEXT("引天地灵气入体，兼顾修炼速度、生命与战斗中的回元护体。"), TEXT("息"),
				EImmortalTechniqueQuality::Mortal, EImmortalTechniqueActiveEffect::BreathRecovery, EImmortalElementType::Water,
				8, 3, 0.08f, 0.24f, 0.0f, 0.04f, 0.06f, 0.0f, 0.0f, 0.12f, 1, 0,
				TechniqueCost(100, {TechniqueMaterial(TEXT("SpiritGrass"), 2), TechniqueMaterial(TEXT("SpiritLiquid"), 1)}))},
			{TEXT("QingyunSwordArt"), TechniqueDefinition(
				TEXT("青云剑诀"), TEXT("青云宗入门剑典，剑气贯体，适合以攻击、暴击和连续剑罡压制妖兽。"), TEXT("剑"),
				EImmortalTechniqueQuality::Spirit, EImmortalTechniqueActiveEffect::SwordWave, EImmortalElementType::Metal,
				6, 3, 1.45f, 3.2f, 0.10f, 0.0f, 0.0f, 0.03f, 0.03f, 0.0f, 20, 0,
				TechniqueCost(400, {TechniqueMaterial(TEXT("DemonCore"), 2), TechniqueMaterial(TEXT("SpiritIron"), 2)}))},
			{TEXT("BurningHeavenArt"), TechniqueDefinition(
				TEXT("焚天诀"), TEXT("以灵力化焚天真火，对成群妖兽造成爆发伤害，并强化攻击与生命。"), TEXT("炎"),
				EImmortalTechniqueQuality::Mystic, EImmortalTechniqueActiveEffect::FlameNova, EImmortalElementType::Fire,
				7, 3, 1.75f, 3.8f, 0.08f, 0.0f, 0.08f, 0.04f, 0.0f, 0.0f, 80, 1,
				TechniqueCost(800, {TechniqueMaterial(TEXT("SpiritLiquid"), 4), TechniqueMaterial(TEXT("DemonCore"), 4), TechniqueMaterial(TEXT("SpiritGrass"), 3)}))},
			{TEXT("NineHeavensThunder"), TechniqueDefinition(
				TEXT("九霄雷法"), TEXT("引九霄神雷连锁轰击敌阵，兼具暴击、攻速与高倍率天劫雷罚。"), TEXT("雷"),
				EImmortalTechniqueQuality::Earth, EImmortalTechniqueActiveEffect::ChainLightning, EImmortalElementType::Thunder,
				8, 3, 2.1f, 4.6f, 0.08f, 0.0f, 0.0f, 0.05f, 0.06f, 0.04f, 160, 2,
				TechniqueCost(1200, {TechniqueMaterial(TEXT("ArtifactFragment"), 5), TechniqueMaterial(TEXT("DemonCore"), 5), TechniqueMaterial(TEXT("SpiritLiquid"), 5)}))}
		};
		return Catalog;
	}

	UDataTable* GetOptionalTechniqueTable()
	{
		static TWeakObjectPtr<UDataTable> Cached;
		static bool bTriedLoad = false;
		if (!bTriedLoad)
		{
			bTriedLoad = true;
			const FString PackageName(TEXT("/Game/GAME/Data/DT_Techniques"));
			if (FPackageName::DoesPackageExist(PackageName)
				&& FSoftObjectPath(PackageName + TEXT(".DT_Techniques")).IsValid())
			{
				Cached = LoadObject<UDataTable>(nullptr, TEXT("/Game/GAME/Data/DT_Techniques.DT_Techniques"));
			}
		}
		return Cached.Get();
	}
}

TArray<FName> UImmortalTechniqueLibrary::GetKnownTechniqueIds()
{
	TArray<FName> Result;
	GetTechniqueFallbackCatalog().GetKeys(Result);
	if (const UDataTable* Table = GetOptionalTechniqueTable())
	{
		for (const FName Row : Table->GetRowNames()) Result.AddUnique(Row);
	}
	Result.Sort(FNameLexicalLess());
	return Result;
}

bool UImmortalTechniqueLibrary::GetTechniqueDefinition(const FName TechniqueId, FImmortalTechniqueDefinition& OutDefinition)
{
	if (TechniqueId.IsNone()) return false;
	if (const UDataTable* Table = GetOptionalTechniqueTable())
	{
		if (const FImmortalTechniqueDefinition* Row = Table->FindRow<FImmortalTechniqueDefinition>(
			TechniqueId, TEXT("Technique lookup"), false))
		{
			OutDefinition = *Row;
			return true;
		}
	}
	if (const FImmortalTechniqueDefinition* Found = GetTechniqueFallbackCatalog().Find(TechniqueId))
	{
		OutDefinition = *Found;
		return true;
	}
	return false;
}

FImmortalTechniqueProgress UImmortalTechniqueLibrary::CreateTechnique(const FName TechniqueId)
{
	FImmortalTechniqueDefinition DefinitionData;
	FImmortalTechniqueProgress Result;
	if (GetTechniqueDefinition(TechniqueId, DefinitionData)) Result.TechniqueId = TechniqueId;
	return Result;
}

void UImmortalTechniqueLibrary::NormalizeProgress(FImmortalTechniqueProgress& Progress)
{
	FImmortalTechniqueDefinition DefinitionData;
	if (!GetTechniqueDefinition(Progress.TechniqueId, DefinitionData))
	{
		Progress = FImmortalTechniqueProgress();
		return;
	}
	Progress.BreakthroughRank = FMath::Clamp(Progress.BreakthroughRank, 0, 4);
	Progress.Level = FMath::Clamp(Progress.Level, 1, GetLevelCap(Progress));
	Progress.ActivePoints = FMath::Clamp(Progress.ActivePoints, 0, GetBranchPointCap(EImmortalTechniquePointBranch::Active));
	Progress.PassivePoints = FMath::Clamp(Progress.PassivePoints, 0, GetBranchPointCap(EImmortalTechniquePointBranch::Passive));
	Progress.SpecialPoints = FMath::Clamp(Progress.SpecialPoints, 0, GetBranchPointCap(EImmortalTechniquePointBranch::Special));
}

void UImmortalTechniqueLibrary::NormalizeLibrary(
	TArray<FImmortalTechniqueProgress>& Library,
	TArray<FName>& EquippedTechniqueIds,
	int32& InsightPoints)
{
	for (FImmortalTechniqueProgress& Progress : Library) NormalizeProgress(Progress);
	Library.RemoveAll([](const FImmortalTechniqueProgress& Progress) { return !Progress.IsValid(); });
	TSet<FName> Seen;
	Library.RemoveAll([&Seen](const FImmortalTechniqueProgress& Progress)
	{
		if (Seen.Contains(Progress.TechniqueId)) return true;
		Seen.Add(Progress.TechniqueId);
		return false;
	});
	EquippedTechniqueIds.RemoveAll([&Library](const FName Id)
	{
		return Id.IsNone() || !Library.ContainsByPredicate([Id](const FImmortalTechniqueProgress& Progress)
		{
			return Progress.TechniqueId == Id;
		});
	});
	TSet<FName> EquippedSeen;
	EquippedTechniqueIds.RemoveAll([&EquippedSeen](const FName Id)
	{
		if (EquippedSeen.Contains(Id)) return true;
		EquippedSeen.Add(Id);
		return false;
	});
	if (EquippedTechniqueIds.Num() > 2) EquippedTechniqueIds.SetNum(2);
	InsightPoints = FMath::Clamp(InsightPoints, 0, 9999);
}

int32 UImmortalTechniqueLibrary::GetLevelCap(const FImmortalTechniqueProgress& Progress)
{
	return FMath::Clamp((FMath::Clamp(Progress.BreakthroughRank, 0, 4) + 1) * 10, 10, 50);
}

int32 UImmortalTechniqueLibrary::GetUpgradeCultivationCost(const FImmortalTechniqueProgress& Progress)
{
	if (!Progress.IsValid() || Progress.Level >= GetLevelCap(Progress) || Progress.Level >= 50) return 0;
	return FMath::Clamp(25 * FMath::Max(Progress.Level, 1), 25, 5000);
}

FImmortalCraftingCost UImmortalTechniqueLibrary::GetBreakthroughCost(const FImmortalTechniqueProgress& Progress)
{
	if (!Progress.IsValid() || Progress.BreakthroughRank >= 4 || Progress.Level < GetLevelCap(Progress)) return FImmortalCraftingCost();
	const int32 NextRank = Progress.BreakthroughRank + 1;
	return TechniqueCost(350 * NextRank, {
		TechniqueMaterial(TEXT("DemonCore"), 2 * NextRank),
		TechniqueMaterial(TEXT("SpiritLiquid"), NextRank)});
}

FImmortalTechniqueBonuses UImmortalTechniqueLibrary::CalculateBonuses(const FImmortalTechniqueProgress& Progress)
{
	FImmortalTechniqueBonuses Result;
	FImmortalTechniqueDefinition DefinitionData;
	if (!Progress.IsValid() || !GetTechniqueDefinition(Progress.TechniqueId, DefinitionData)) return Result;
	const float Growth = 1.0f + 0.025f * (Progress.Level - 1) + 0.12f * Progress.BreakthroughRank;
	const float PassiveScale = Growth * (1.0f + 0.08f * Progress.PassivePoints);
	Result.AttackMultiplier += DefinitionData.AttackPercent * PassiveScale;
	Result.DefenseMultiplier += DefinitionData.DefensePercent * PassiveScale;
	Result.HealthMultiplier += DefinitionData.HealthPercent * PassiveScale;
	Result.AttackSpeedBonus = DefinitionData.AttackSpeedBonus * PassiveScale;
	Result.CriticalChanceBonus = DefinitionData.CriticalChanceBonus * PassiveScale;
	Result.CultivationRateMultiplier += DefinitionData.CultivationRateBonus * PassiveScale;
	return Result;
}

float UImmortalTechniqueLibrary::CalculateActiveMagnitude(const FImmortalTechniqueProgress& Progress)
{
	FImmortalTechniqueDefinition DefinitionData;
	if (!Progress.IsValid() || !GetTechniqueDefinition(Progress.TechniqueId, DefinitionData)) return 0.0f;
	return DefinitionData.ActiveMagnitude * (1.0f + 0.04f * (Progress.Level - 1)
		+ 0.15f * Progress.BreakthroughRank + 0.10f * Progress.ActivePoints);
}

float UImmortalTechniqueLibrary::CalculateUltimateMagnitude(const FImmortalTechniqueProgress& Progress)
{
	FImmortalTechniqueDefinition DefinitionData;
	if (!Progress.IsValid() || !GetTechniqueDefinition(Progress.TechniqueId, DefinitionData)) return 0.0f;
	return DefinitionData.UltimateMagnitude * (1.0f + 0.05f * (Progress.Level - 1)
		+ 0.2f * Progress.BreakthroughRank + 0.12f * Progress.ActivePoints + 0.08f * Progress.SpecialPoints);
}

int32 UImmortalTechniqueLibrary::CalculateTriggerAttackCount(const FImmortalTechniqueProgress& Progress)
{
	FImmortalTechniqueDefinition DefinitionData;
	if (!Progress.IsValid() || !GetTechniqueDefinition(Progress.TechniqueId, DefinitionData)) return MAX_int32;
	return FMath::Max(DefinitionData.AttacksPerTrigger - Progress.ActivePoints / 4 - Progress.BreakthroughRank / 2, 3);
}

int32 UImmortalTechniqueLibrary::CalculateUltimateActiveCount(const FImmortalTechniqueProgress& Progress)
{
	FImmortalTechniqueDefinition DefinitionData;
	if (!Progress.IsValid() || !GetTechniqueDefinition(Progress.TechniqueId, DefinitionData)) return MAX_int32;
	return FMath::Max(DefinitionData.ActivesPerUltimate - Progress.SpecialPoints / 3, 2);
}

int32 UImmortalTechniqueLibrary::GetBranchPointCap(const EImmortalTechniquePointBranch Branch)
{
	return Branch == EImmortalTechniquePointBranch::Special ? 5 : 10;
}

FText UImmortalTechniqueLibrary::GetQualityText(const EImmortalTechniqueQuality Quality)
{
	switch (Quality)
	{
	case EImmortalTechniqueQuality::Spirit: return FText::FromString(TEXT("灵品"));
	case EImmortalTechniqueQuality::Mystic: return FText::FromString(TEXT("玄品"));
	case EImmortalTechniqueQuality::Earth: return FText::FromString(TEXT("地品"));
	case EImmortalTechniqueQuality::Heaven: return FText::FromString(TEXT("天品"));
	case EImmortalTechniqueQuality::Immortal: return FText::FromString(TEXT("仙品"));
	default: return FText::FromString(TEXT("凡品"));
	}
}

FLinearColor UImmortalTechniqueLibrary::GetQualityColor(const EImmortalTechniqueQuality Quality)
{
	switch (Quality)
	{
	case EImmortalTechniqueQuality::Spirit: return FLinearColor(0.3f, 0.95f, 0.42f);
	case EImmortalTechniqueQuality::Mystic: return FLinearColor(0.3f, 0.58f, 1.0f);
	case EImmortalTechniqueQuality::Earth: return FLinearColor(0.76f, 0.34f, 1.0f);
	case EImmortalTechniqueQuality::Heaven: return FLinearColor(1.0f, 0.62f, 0.12f);
	case EImmortalTechniqueQuality::Immortal: return FLinearColor(1.0f, 0.25f, 0.22f);
	default: return FLinearColor(0.9f, 0.9f, 0.9f);
	}
}

FText UImmortalTechniqueLibrary::GetActiveEffectText(const FImmortalTechniqueProgress& Progress)
{
	FImmortalTechniqueDefinition DefinitionData;
	if (!GetTechniqueDefinition(Progress.TechniqueId, DefinitionData)) return FText::GetEmpty();
	const int32 Trigger = CalculateTriggerAttackCount(Progress);
	const float Magnitude = CalculateActiveMagnitude(Progress);
	switch (DefinitionData.ActiveEffect)
	{
	case EImmortalTechniqueActiveEffect::BreathRecovery:
		return FText::FromString(FString::Printf(TEXT("每 %d 次攻击施展回元诀，恢复生命与灵力 %.1f%%"), Trigger, Magnitude * 100.0f));
	case EImmortalTechniqueActiveEffect::SwordWave:
		return FText::FromString(FString::Printf(TEXT("每 %d 次攻击释放青云剑罡，造成攻击 %.0f%% 伤害"), Trigger, Magnitude * 100.0f));
	case EImmortalTechniqueActiveEffect::FlameNova:
		return FText::FromString(FString::Printf(TEXT("每 %d 次攻击引爆焚天火环，对范围妖兽造成攻击 %.0f%% 伤害"), Trigger, Magnitude * 100.0f));
	default:
		return FText::FromString(FString::Printf(TEXT("每 %d 次攻击释放连锁天雷，造成攻击 %.0f%% 伤害"), Trigger, Magnitude * 100.0f));
	}
}

FText UImmortalTechniqueLibrary::GetUltimateEffectText(const FImmortalTechniqueProgress& Progress)
{
	FImmortalTechniqueDefinition DefinitionData;
	if (!GetTechniqueDefinition(Progress.TechniqueId, DefinitionData)) return FText::GetEmpty();
	const int32 Trigger = CalculateUltimateActiveCount(Progress);
	const float Magnitude = CalculateUltimateMagnitude(Progress);
	const TCHAR* Name = TEXT("周天归元");
	switch (DefinitionData.ActiveEffect)
	{
	case EImmortalTechniqueActiveEffect::SwordWave: Name = TEXT("万剑归宗"); break;
	case EImmortalTechniqueActiveEffect::FlameNova: Name = TEXT("焚天火海"); break;
	case EImmortalTechniqueActiveEffect::ChainLightning: Name = TEXT("九霄雷劫"); break;
	default: break;
	}
	return FText::FromString(FString::Printf(TEXT("每 %d 次主动技施展终极技·%s，效果 %.0f%%"), Trigger, Name, Magnitude * 100.0f));
}

FText UImmortalTechniqueLibrary::GetPassiveEffectText(const FImmortalTechniqueProgress& Progress)
{
	const FImmortalTechniqueBonuses Bonus = CalculateBonuses(Progress);
	return FText::FromString(FString::Printf(
		TEXT("攻击 +%.1f%% · 防御 +%.1f%% · 生命 +%.1f%%\n攻速 +%.1f%% · 暴击 +%.1f%% · 修炼 +%.1f%%"),
		(Bonus.AttackMultiplier - 1.0f) * 100.0f,
		(Bonus.DefenseMultiplier - 1.0f) * 100.0f,
		(Bonus.HealthMultiplier - 1.0f) * 100.0f,
		Bonus.AttackSpeedBonus * 100.0f,
		Bonus.CriticalChanceBonus * 100.0f,
		(Bonus.CultivationRateMultiplier - 1.0f) * 100.0f));
}

FText UImmortalTechniqueLibrary::GetSpecialEffectText(const FImmortalTechniqueProgress& Progress)
{
	FImmortalTechniqueDefinition DefinitionData;
	if (!GetTechniqueDefinition(Progress.TechniqueId, DefinitionData)) return FText::GetEmpty();
	const int32 Points = Progress.SpecialPoints;
	switch (DefinitionData.ActiveEffect)
	{
	case EImmortalTechniqueActiveEffect::BreathRecovery:
		return FText::FromString(FString::Printf(TEXT("回元后获得最大生命 %.1f%% 护盾"), Points * 2.0f));
	case EImmortalTechniqueActiveEffect::SwordWave:
		return FText::FromString(FString::Printf(TEXT("剑罡额外追斩伤害 +%d%%"), Points * 12));
	case EImmortalTechniqueActiveEffect::FlameNova:
		return FText::FromString(Points >= 3 ? TEXT("火环追加第二次余焰爆炸") : TEXT("投入 3 点解锁余焰爆炸"));
	default:
		return FText::FromString(FString::Printf(TEXT("连锁目标 +%d，终极雷劫强化"), Points));
	}
}
