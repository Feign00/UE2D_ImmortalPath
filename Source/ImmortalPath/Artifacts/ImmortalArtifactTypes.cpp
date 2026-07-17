// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalArtifactTypes.h"

#include "Engine/DataTable.h"
#include "Misc/PackageName.h"

namespace
{
	FImmortalCraftingMaterialCost ArtifactMaterial(const TCHAR* Id, const int32 Quantity)
	{
		FImmortalCraftingMaterialCost Result;
		Result.MaterialId = FName(Id);
		Result.Quantity = Quantity;
		return Result;
	}

	FImmortalCraftingCost ArtifactCost(const int32 Stones, std::initializer_list<FImmortalCraftingMaterialCost> Materials)
	{
		FImmortalCraftingCost Result;
		Result.SpiritStones = Stones;
		for (const FImmortalCraftingMaterialCost& Material : Materials) Result.Materials.Add(Material);
		return Result;
	}

	FImmortalArtifactDefinition ArtifactDefinition(
		const TCHAR* Name,
		const TCHAR* Description,
		const TCHAR* Glyph,
		const EImmortalArtifactQuality Quality,
		const EImmortalArtifactActiveEffect Effect,
		const int32 Trigger,
		const float Magnitude,
		const float Attack,
		const float Defense,
		const float Health,
		const float AttackSpeed,
		const float Crit,
		const int32 Stage,
		const FImmortalCraftingCost& Cost)
	{
		FImmortalArtifactDefinition Result;
		Result.DisplayName = FText::FromString(Name);
		Result.Description = FText::FromString(Description);
		Result.IconGlyph = FText::FromString(Glyph);
		Result.Quality = Quality;
		Result.ActiveEffect = Effect;
		Result.AttacksPerTrigger = Trigger;
		Result.ActiveMagnitude = Magnitude;
		Result.AttackPercent = Attack;
		Result.DefensePercent = Defense;
		Result.HealthPercent = Health;
		Result.AttackSpeedBonus = AttackSpeed;
		Result.CriticalChanceBonus = Crit;
		Result.MinimumQingyunStage = Stage;
		Result.CraftingCost = Cost;
		return Result;
	}

	const TMap<FName, FImmortalArtifactDefinition>& GetArtifactFallbackCatalog()
	{
		static const TMap<FName, FImmortalArtifactDefinition> Catalog =
		{
			{TEXT("XuanGuangSword"), ArtifactDefinition(
				TEXT("玄光仙剑"), TEXT("凝聚玄光追斩当前目标，适合稳定提高挂机输出。"), TEXT("剑"),
				EImmortalArtifactQuality::Spirit, EImmortalArtifactActiveEffect::DirectDamage, 5, 1.8f,
				0.08f, 0.0f, 0.0f, 0.02f, 0.0f, 1,
				ArtifactCost(300, {ArtifactMaterial(TEXT("ArtifactFragment"), 2), ArtifactMaterial(TEXT("SpiritIron"), 2), ArtifactMaterial(TEXT("DemonCore"), 2)}))},
			{TEXT("SevenStarBanner"), ArtifactDefinition(
				TEXT("七星幡"), TEXT("引动七星之力攻击范围内全部妖兽。"), TEXT("星"),
				EImmortalArtifactQuality::Mystic, EImmortalArtifactActiveEffect::AreaDamage, 6, 1.35f,
				0.05f, 0.05f, 0.0f, 0.0f, 0.02f, 50,
				ArtifactCost(600, {ArtifactMaterial(TEXT("ArtifactFragment"), 4), ArtifactMaterial(TEXT("SpiritIron"), 3), ArtifactMaterial(TEXT("DemonCore"), 3)}))},
			{TEXT("FlowingCloudUmbrella"), ArtifactDefinition(
				TEXT("流云宝伞"), TEXT("展开流云护体，恢复生命并生成抵挡伤害的护盾。"), TEXT("伞"),
				EImmortalArtifactQuality::Mystic, EImmortalArtifactActiveEffect::HealAndShield, 6, 0.12f,
				0.0f, 0.08f, 0.12f, 0.0f, 0.0f, 100,
				ArtifactCost(700, {ArtifactMaterial(TEXT("ArtifactFragment"), 5), ArtifactMaterial(TEXT("DemonBone"), 4), ArtifactMaterial(TEXT("SpiritIron"), 3)}))},
			{TEXT("ChaosPearl"), ArtifactDefinition(
				TEXT("混元珠"), TEXT("释放混元爆裂重创当前目标，兼具极高暴击成长。"), TEXT("珠"),
				EImmortalArtifactQuality::Earth, EImmortalArtifactActiveEffect::ChaosBurst, 7, 2.6f,
				0.10f, 0.0f, 0.08f, 0.0f, 0.05f, 200,
				ArtifactCost(1200, {ArtifactMaterial(TEXT("ArtifactFragment"), 8), ArtifactMaterial(TEXT("DemonCore"), 6), ArtifactMaterial(TEXT("SpiritIron"), 5)}))}
		};
		return Catalog;
	}

	UDataTable* GetOptionalArtifactTable()
	{
		static TWeakObjectPtr<UDataTable> CachedTable;
		static bool bAttemptedLoad = false;
		if (!bAttemptedLoad)
		{
			bAttemptedLoad = true;
			const FString PackageName(TEXT("/Game/GAME/Data/DT_Artifacts"));
			if (FPackageName::DoesPackageExist(PackageName))
			{
				CachedTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/GAME/Data/DT_Artifacts.DT_Artifacts"));
			}
		}
		return CachedTable.Get();
	}
}

TArray<FName> UImmortalArtifactLibrary::GetKnownArtifactIds()
{
	TArray<FName> Result;
	GetArtifactFallbackCatalog().GetKeys(Result);
	if (const UDataTable* Table = GetOptionalArtifactTable())
	{
		for (const FName Row : Table->GetRowNames()) Result.AddUnique(Row);
	}
	Result.Sort(FNameLexicalLess());
	return Result;
}

bool UImmortalArtifactLibrary::GetArtifactDefinition(const FName ArtifactId, FImmortalArtifactDefinition& OutDefinition)
{
	if (ArtifactId.IsNone()) return false;
	if (const UDataTable* Table = GetOptionalArtifactTable())
	{
		if (const FImmortalArtifactDefinition* Row = Table->FindRow<FImmortalArtifactDefinition>(ArtifactId, TEXT("Artifact lookup"), false))
		{
			OutDefinition = *Row;
			return true;
		}
	}
	if (const FImmortalArtifactDefinition* Found = GetArtifactFallbackCatalog().Find(ArtifactId))
	{
		OutDefinition = *Found;
		return true;
	}
	return false;
}

FImmortalArtifactItem UImmortalArtifactLibrary::CreateArtifact(const FName ArtifactId)
{
	FImmortalArtifactDefinition Definition;
	FImmortalArtifactItem Result;
	if (!GetArtifactDefinition(ArtifactId, Definition)) return Result;
	Result.InstanceId = FGuid::NewGuid();
	Result.ArtifactId = ArtifactId;
	return Result;
}

void UImmortalArtifactLibrary::NormalizeArtifact(FImmortalArtifactItem& Item)
{
	FImmortalArtifactDefinition Definition;
	if (!Item.InstanceId.IsValid() || !GetArtifactDefinition(Item.ArtifactId, Definition))
	{
		Item = FImmortalArtifactItem();
		return;
	}
	Item.Level = FMath::Clamp(Item.Level, 1, 50);
	Item.Stars = FMath::Clamp(Item.Stars, 0, 5);
}

void UImmortalArtifactLibrary::NormalizeInventory(TArray<FImmortalArtifactItem>& Inventory, FGuid& EquippedInstanceId)
{
	for (FImmortalArtifactItem& Item : Inventory) NormalizeArtifact(Item);
	Inventory.RemoveAll([](const FImmortalArtifactItem& Item) { return !Item.IsValid(); });
	TSet<FGuid> Seen;
	Inventory.RemoveAll([&Seen](const FImmortalArtifactItem& Item)
	{
		if (Seen.Contains(Item.InstanceId)) return true;
		Seen.Add(Item.InstanceId);
		return false;
	});
	if (!Inventory.ContainsByPredicate([EquippedInstanceId](const FImmortalArtifactItem& Item)
	{
		return Item.InstanceId == EquippedInstanceId;
	})) EquippedInstanceId.Invalidate();
}

FImmortalArtifactBonuses UImmortalArtifactLibrary::CalculateBonuses(const FImmortalArtifactItem& Item)
{
	FImmortalArtifactBonuses Result;
	FImmortalArtifactDefinition Definition;
	if (!Item.IsValid() || !GetArtifactDefinition(Item.ArtifactId, Definition)) return Result;
	const float Growth = 1.0f + 0.03f * (Item.Level - 1) + 0.15f * Item.Stars;
	Result.AttackMultiplier += Definition.AttackPercent * Growth;
	Result.DefenseMultiplier += Definition.DefensePercent * Growth;
	Result.HealthMultiplier += Definition.HealthPercent * Growth;
	Result.AttackSpeedBonus = Definition.AttackSpeedBonus * Growth;
	Result.CriticalChanceBonus = Definition.CriticalChanceBonus * Growth;
	return Result;
}

float UImmortalArtifactLibrary::CalculateActiveMagnitude(const FImmortalArtifactItem& Item)
{
	FImmortalArtifactDefinition Definition;
	if (!Item.IsValid() || !GetArtifactDefinition(Item.ArtifactId, Definition)) return 0.0f;
	return Definition.ActiveMagnitude * (1.0f + 0.04f * (Item.Level - 1) + 0.18f * Item.Stars);
}

int32 UImmortalArtifactLibrary::CalculateTriggerAttackCount(const FImmortalArtifactItem& Item)
{
	FImmortalArtifactDefinition Definition;
	if (!Item.IsValid() || !GetArtifactDefinition(Item.ArtifactId, Definition)) return MAX_int32;
	return FMath::Max(Definition.AttacksPerTrigger - Item.Stars / 2, 2);
}

FImmortalCraftingCost UImmortalArtifactLibrary::GetUpgradeCost(const FImmortalArtifactItem& Item)
{
	if (!Item.IsValid() || Item.Level >= 50) return FImmortalCraftingCost();
	const int32 NextLevel = Item.Level + 1;
	return ArtifactCost(80 * NextLevel, {ArtifactMaterial(TEXT("ArtifactFragment"), 1 + Item.Level / 10)});
}

FImmortalCraftingCost UImmortalArtifactLibrary::GetStarUpCost(const FImmortalArtifactItem& Item)
{
	if (!Item.IsValid() || Item.Stars >= 5) return FImmortalCraftingCost();
	const int32 NextStar = Item.Stars + 1;
	return ArtifactCost(500 * NextStar, {
		ArtifactMaterial(TEXT("ArtifactFragment"), 3 * NextStar),
		ArtifactMaterial(TEXT("SpiritIron"), NextStar)});
}

FText UImmortalArtifactLibrary::GetQualityText(const EImmortalArtifactQuality Quality)
{
	switch (Quality)
	{
	case EImmortalArtifactQuality::Spirit: return FText::FromString(TEXT("灵品"));
	case EImmortalArtifactQuality::Mystic: return FText::FromString(TEXT("玄品"));
	case EImmortalArtifactQuality::Earth: return FText::FromString(TEXT("地品"));
	case EImmortalArtifactQuality::Heaven: return FText::FromString(TEXT("天品"));
	case EImmortalArtifactQuality::Immortal: return FText::FromString(TEXT("仙品"));
	default: return FText::FromString(TEXT("凡品"));
	}
}

FLinearColor UImmortalArtifactLibrary::GetQualityColor(const EImmortalArtifactQuality Quality)
{
	switch (Quality)
	{
	case EImmortalArtifactQuality::Spirit: return FLinearColor(0.3f, 0.95f, 0.42f);
	case EImmortalArtifactQuality::Mystic: return FLinearColor(0.3f, 0.58f, 1.0f);
	case EImmortalArtifactQuality::Earth: return FLinearColor(0.76f, 0.34f, 1.0f);
	case EImmortalArtifactQuality::Heaven: return FLinearColor(1.0f, 0.62f, 0.12f);
	case EImmortalArtifactQuality::Immortal: return FLinearColor(1.0f, 0.25f, 0.22f);
	default: return FLinearColor(0.9f, 0.9f, 0.9f);
	}
}

FText UImmortalArtifactLibrary::GetActiveEffectText(const FImmortalArtifactItem& Item)
{
	FImmortalArtifactDefinition Definition;
	if (!GetArtifactDefinition(Item.ArtifactId, Definition)) return FText::GetEmpty();
	const float Magnitude = CalculateActiveMagnitude(Item);
	const int32 Trigger = CalculateTriggerAttackCount(Item);
	switch (Definition.ActiveEffect)
	{
	case EImmortalArtifactActiveEffect::AreaDamage:
		return FText::FromString(FString::Printf(TEXT("每 %d 次命中，对范围妖兽造成攻击 %.0f%% 伤害"), Trigger, Magnitude * 100.0f));
	case EImmortalArtifactActiveEffect::HealAndShield:
		return FText::FromString(FString::Printf(TEXT("每 %d 次命中，恢复并获得最大生命 %.0f%% 的护盾"), Trigger, Magnitude * 100.0f));
	case EImmortalArtifactActiveEffect::ChaosBurst:
		return FText::FromString(FString::Printf(TEXT("每 %d 次命中，造成攻击 %.0f%% 的混元爆裂"), Trigger, Magnitude * 100.0f));
	default:
		return FText::FromString(FString::Printf(TEXT("每 %d 次命中，造成攻击 %.0f%% 的玄光追斩"), Trigger, Magnitude * 100.0f));
	}
}

FText UImmortalArtifactLibrary::GetPassiveEffectText(const FImmortalArtifactItem& Item)
{
	const FImmortalArtifactBonuses Bonus = CalculateBonuses(Item);
	return FText::FromString(FString::Printf(
		TEXT("攻击 +%.1f%% · 防御 +%.1f%% · 生命 +%.1f%%\n攻速 +%.1f%% · 暴击 +%.1f%%"),
		(Bonus.AttackMultiplier - 1.0f) * 100.0f,
		(Bonus.DefenseMultiplier - 1.0f) * 100.0f,
		(Bonus.HealthMultiplier - 1.0f) * 100.0f,
		Bonus.AttackSpeedBonus * 100.0f,
		Bonus.CriticalChanceBonus * 100.0f));
}

