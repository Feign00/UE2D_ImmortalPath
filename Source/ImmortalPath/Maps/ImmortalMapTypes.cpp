// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalMapTypes.h"

namespace
{
	const FName QingyunMountainId(TEXT("QingyunMountain"));
	const FName DemonWolfValleyId(TEXT("DemonWolfValley"));
	const FName MyriadBeastForestId(TEXT("MyriadBeastForest"));
	const FName BlackWindCaveId(TEXT("BlackWindCave"));
	const FName AncientRuinsId(TEXT("AncientRuins"));
	const FName NetherValleyId(TEXT("NetherValley"));
	const FName NineNetherSecretRealmId(TEXT("NineNetherSecretRealm"));
	const FName ImmortalPalaceRuinsId(TEXT("ImmortalPalaceRuins"));

	void ClampProgressToDefinition(FImmortalMapProgress& Progress, const FImmortalMapDefinition& Definition)
	{
		Progress.Stage = FMath::Clamp(Progress.Stage, 1, Definition.MaximumStage);
		Progress.bCompleted = Progress.bCompleted && Progress.Stage >= Definition.MaximumStage;
		if (Progress.bCompleted)
		{
			Progress.StageKills = 1;
			return;
		}

		const bool bBossStage = Progress.Stage >= Definition.MaximumStage
			|| Progress.Stage % FMath::Max(Definition.BossStageInterval, 2) == 0;
		// A pending boss has no partial kill progress. Normal stages advance as
		// soon as the tenth kill is recorded, so only 0..9 is persistent.
		Progress.StageKills = bBossStage ? 0 : FMath::Clamp(Progress.StageKills, 0, 9);
	}

	FImmortalMapDefinition MakeMap(
		const FName Id,
		const TCHAR* Name,
		const TCHAR* Description,
		const TCHAR* MonsterName,
		const TCHAR* BossName,
		const int32 Realm,
		const int32 Order,
		const float Health,
		const float Attack,
		const float Defense,
		const int32 EquipmentLevelBonus,
		const EImmortalEquipmentQuality MinimumQuality,
		const EImmortalEquipmentQuality BossQuality,
		const float EquipmentChanceBonus,
		const float MaterialChanceBonus,
		const float StoneMultiplier,
		const TArray<FName>& Materials,
		const FLinearColor& SceneTint,
		const FLinearColor& MonsterTint,
		const FLinearColor& BossColor)
	{
		FImmortalMapDefinition Result;
		Result.MapId = Id;
		Result.DisplayName = FText::FromString(Name);
		Result.Description = FText::FromString(Description);
		Result.NormalMonsterName = FText::FromString(MonsterName);
		Result.BossName = FText::FromString(BossName);
		Result.RequiredRealmIndex = Realm;
		Result.OrderIndex = Order;
		Result.HealthMultiplier = Health;
		Result.AttackMultiplier = Attack;
		Result.DefenseMultiplier = Defense;
		Result.EquipmentLevelBonus = EquipmentLevelBonus;
		Result.MinimumEquipmentQuality = MinimumQuality;
		Result.BossMinimumEquipmentQuality = BossQuality;
		Result.EquipmentDropChanceBonus = EquipmentChanceBonus;
		Result.MaterialDropChanceBonus = MaterialChanceBonus;
		Result.SpiritStoneMultiplier = StoneMultiplier;
		Result.MaterialPoolIds = Materials;
		Result.SceneTint = SceneTint;
		Result.MonsterTint = MonsterTint;
		Result.BossColor = BossColor;
		return Result;
	}

	const TMap<FName, FImmortalMapDefinition>& GetFallbackMaps()
	{
		static const TMap<FName, FImmortalMapDefinition> Maps = {
			{QingyunMountainId, MakeMap(QingyunMountainId, TEXT("青云山"), TEXT("云海环绕的入道之地，妖狐与灵犬盘踞山道。"), TEXT("青云妖兽"), TEXT("青云妖王"),
				0, 0, 1.00f, 1.00f, 1.00f, 0, EImmortalEquipmentQuality::Common, EImmortalEquipmentQuality::Rare, 0.00f, 0.00f, 1.00f,
				{TEXT("SpiritGrass"), TEXT("Ore"), TEXT("DemonCore"), TEXT("SpiritLiquid")},
				FLinearColor::White, FLinearColor::White, FLinearColor(1.0f, 0.28f, 0.12f, 1.0f))},
			{DemonWolfValleyId, MakeMap(DemonWolfValleyId, TEXT("妖狼谷"), TEXT("月色终年不散的险谷，狼群以妖骨磨砺利爪。"), TEXT("噬月妖狼"), TEXT("啸月狼王"),
				0, 1, 1.35f, 1.18f, 1.10f, 25, EImmortalEquipmentQuality::Uncommon, EImmortalEquipmentQuality::Rare, 0.03f, 0.04f, 1.30f,
				{TEXT("DemonBone"), TEXT("DemonCore"), TEXT("SpiritGrass"), TEXT("Ore")},
				FLinearColor(0.95f, 0.86f, 0.76f, 1.0f), FLinearColor(0.94f, 0.72f, 0.58f, 1.0f), FLinearColor(1.0f, 0.38f, 0.12f, 1.0f))},
			{MyriadBeastForestId, MakeMap(MyriadBeastForestId, TEXT("万妖林"), TEXT("筑基修士方可踏入的古林，浓郁妖气孕育稀有妖丹。"), TEXT("万妖林灵兽"), TEXT("万妖树皇"),
				1, 2, 2.00f, 1.48f, 1.35f, 50, EImmortalEquipmentQuality::Uncommon, EImmortalEquipmentQuality::Epic, 0.06f, 0.08f, 1.70f,
				{TEXT("DemonCore"), TEXT("DemonBone"), TEXT("SpiritGrass"), TEXT("SpiritLiquid")},
				FLinearColor(0.72f, 1.00f, 0.72f, 1.0f), FLinearColor(0.55f, 0.92f, 0.58f, 1.0f), FLinearColor(0.35f, 1.0f, 0.28f, 1.0f))},
			{BlackWindCaveId, MakeMap(BlackWindCaveId, TEXT("黑风洞"), TEXT("黑风卷动灵矿的地底洞府，深处潜伏着黑风魔君。"), TEXT("黑风魔兽"), TEXT("黑风魔君"),
				1, 3, 2.60f, 1.78f, 1.65f, 75, EImmortalEquipmentQuality::Rare, EImmortalEquipmentQuality::Epic, 0.09f, 0.10f, 2.20f,
				{TEXT("Ore"), TEXT("SpiritIron"), TEXT("DemonBone"), TEXT("ArtifactFragment")},
				FLinearColor(0.62f, 0.65f, 0.78f, 1.0f), FLinearColor(0.58f, 0.62f, 0.78f, 1.0f), FLinearColor(0.62f, 0.28f, 0.95f, 1.0f))},
			{AncientRuinsId, MakeMap(AncientRuinsId, TEXT("上古遗迹"), TEXT("金丹修士探寻的残破古城，傀儡守护着灵铁与法宝残片。"), TEXT("遗迹傀儡"), TEXT("遗迹守将"),
				2, 4, 3.80f, 2.25f, 2.10f, 110, EImmortalEquipmentQuality::Rare, EImmortalEquipmentQuality::Legendary, 0.12f, 0.13f, 3.00f,
				{TEXT("SpiritIron"), TEXT("ArtifactFragment"), TEXT("Ore"), TEXT("SpiritLiquid")},
				FLinearColor(1.00f, 0.83f, 0.58f, 1.0f), FLinearColor(0.95f, 0.78f, 0.48f, 1.0f), FLinearColor(1.0f, 0.68f, 0.12f, 1.0f))},
			{NetherValleyId, MakeMap(NetherValleyId, TEXT("幽冥谷"), TEXT("幽冥阴气凝成灵液，谷底鬼王统御无数妖灵。"), TEXT("幽冥妖灵"), TEXT("幽冥鬼王"),
				2, 5, 4.80f, 2.70f, 2.55f, 145, EImmortalEquipmentQuality::Epic, EImmortalEquipmentQuality::Immortal, 0.15f, 0.16f, 3.80f,
				{TEXT("DemonCore"), TEXT("ArtifactFragment"), TEXT("SpiritLiquid"), TEXT("DemonBone")},
				FLinearColor(0.72f, 0.58f, 0.90f, 1.0f), FLinearColor(0.68f, 0.48f, 0.88f, 1.0f), FLinearColor(0.78f, 0.22f, 1.0f, 1.0f))},
			{NineNetherSecretRealmId, MakeMap(NineNetherSecretRealmId, TEXT("九幽秘境"), TEXT("元婴方能承受的九幽裂隙，魔物掉落更高阶装备与法宝材料。"), TEXT("九幽魔物"), TEXT("九幽冥皇"),
				3, 6, 6.50f, 3.45f, 3.25f, 190, EImmortalEquipmentQuality::Legendary, EImmortalEquipmentQuality::Immortal, 0.18f, 0.19f, 5.00f,
				{TEXT("ArtifactFragment"), TEXT("SpiritIron"), TEXT("DemonCore"), TEXT("SpiritLiquid")},
				FLinearColor(0.48f, 0.62f, 0.85f, 1.0f), FLinearColor(0.38f, 0.52f, 0.82f, 1.0f), FLinearColor(0.25f, 0.55f, 1.0f, 1.0f))},
			{ImmortalPalaceRuinsId, MakeMap(ImmortalPalaceRuinsId, TEXT("仙宫遗址"), TEXT("坠落凡尘的仙宫残垣，天将残魂守护最后的仙家遗藏。"), TEXT("仙宫守卫"), TEXT("仙宫天将"),
				3, 7, 8.50f, 4.35f, 4.00f, 240, EImmortalEquipmentQuality::Immortal, EImmortalEquipmentQuality::Divine, 0.22f, 0.23f, 6.50f,
				{TEXT("ArtifactFragment"), TEXT("SpiritIron"), TEXT("SpiritLiquid"), TEXT("SpiritGrass")},
				FLinearColor(0.85f, 0.92f, 1.00f, 1.0f), FLinearColor(0.80f, 0.88f, 1.00f, 1.0f), FLinearColor(0.30f, 0.85f, 1.0f, 1.0f))}
		};
		return Maps;
	}
}

FName UImmortalMapLibrary::GetQingyunMountainId()
{
	return QingyunMountainId;
}

FName UImmortalMapLibrary::GetImmortalPalaceRuinsId()
{
	return ImmortalPalaceRuinsId;
}

TArray<FName> UImmortalMapLibrary::GetKnownMapIds()
{
	TArray<FName> Result = {
		QingyunMountainId,
		DemonWolfValleyId,
		MyriadBeastForestId,
		BlackWindCaveId,
		AncientRuinsId,
		NetherValleyId,
		NineNetherSecretRealmId,
		ImmortalPalaceRuinsId
	};
	return Result;
}

bool UImmortalMapLibrary::GetMapDefinition(const FName MapId, FImmortalMapDefinition& OutDefinition)
{
	const FImmortalMapDefinition* Found = GetFallbackMaps().Find(MapId);
	if (!Found)
	{
		OutDefinition = FImmortalMapDefinition();
		return false;
	}
	OutDefinition = *Found;
	return true;
}

bool UImmortalMapLibrary::IsMapUnlocked(const FName MapId, const int32 RealmIndex)
{
	if (RealmIndex < 0)
	{
		return false;
	}
	FImmortalMapDefinition Definition;
	return GetMapDefinition(MapId, Definition)
		&& FMath::Clamp(RealmIndex, 0, 9) >= FMath::Clamp(Definition.RequiredRealmIndex, 0, 9);
}

FText UImmortalMapLibrary::GetRealmRequirementText(const int32 RealmIndex)
{
	switch (FMath::Clamp(RealmIndex, 0, 9))
	{
	case 0: return FText::FromString(TEXT("炼气"));
	case 1: return FText::FromString(TEXT("筑基"));
	case 2: return FText::FromString(TEXT("金丹"));
	case 3: return FText::FromString(TEXT("元婴"));
	case 4: return FText::FromString(TEXT("化神"));
	case 5: return FText::FromString(TEXT("炼虚"));
	case 6: return FText::FromString(TEXT("合体"));
	case 7: return FText::FromString(TEXT("大乘"));
	case 8: return FText::FromString(TEXT("渡劫"));
	default: return FText::FromString(TEXT("飞升"));
	}
}

bool UImmortalMapLibrary::GetMapProgress(
	const FImmortalMapSystemState& State,
	const FName MapId,
	FImmortalMapProgress& OutProgress)
{
	const FImmortalMapProgress* Found = State.MapProgress.FindByPredicate([MapId](const FImmortalMapProgress& Progress)
	{
		return Progress.MapId == MapId;
	});
	if (!Found)
	{
		OutProgress = FImmortalMapProgress();
		OutProgress.MapId = MapId;
		return false;
	}
	OutProgress = *Found;
	return true;
}

FImmortalMapSystemState UImmortalMapLibrary::CreateMigratedState(
	const int32 QingyunStage,
	const int32 QingyunKills,
	const bool bQingyunCompleted)
{
	FImmortalMapSystemState Result;
	Result.bInitialized = true;
	Result.ActiveMapId = QingyunMountainId;
	for (const FName MapId : GetKnownMapIds())
	{
		FImmortalMapProgress& Progress = Result.MapProgress.AddDefaulted_GetRef();
		Progress.MapId = MapId;
		if (MapId == QingyunMountainId)
		{
			Progress.Stage = FMath::Clamp(QingyunStage, 1, 999);
			Progress.StageKills = FMath::Max(QingyunKills, 0);
			Progress.bCompleted = bQingyunCompleted && Progress.Stage >= 999;
		}
	}
	NormalizeState(Result);
	return Result;
}

void UImmortalMapLibrary::NormalizeState(FImmortalMapSystemState& State)
{
	TMap<FName, FImmortalMapProgress> BestProgress;
	for (const FImmortalMapProgress& Candidate : State.MapProgress)
	{
		FImmortalMapDefinition Definition;
		if (!GetMapDefinition(Candidate.MapId, Definition)) continue;
		FImmortalMapProgress Normalized = Candidate;
		ClampProgressToDefinition(Normalized, Definition);

		FImmortalMapProgress* Existing = BestProgress.Find(Normalized.MapId);
		if (!Existing || Normalized.bCompleted || Normalized.Stage > Existing->Stage
			|| (Normalized.Stage == Existing->Stage && Normalized.StageKills > Existing->StageKills))
		{
			BestProgress.Add(Normalized.MapId, Normalized);
		}
	}

	State.MapProgress.Reset();
	for (const FName MapId : GetKnownMapIds())
	{
		if (const FImmortalMapProgress* Existing = BestProgress.Find(MapId))
		{
			State.MapProgress.Add(*Existing);
		}
		else
		{
			FImmortalMapProgress& Added = State.MapProgress.AddDefaulted_GetRef();
			Added.MapId = MapId;
		}
	}
	FImmortalMapDefinition ActiveDefinition;
	if (!GetMapDefinition(State.ActiveMapId, ActiveDefinition)) State.ActiveMapId = QingyunMountainId;
	State.bInitialized = true;
}

bool UImmortalMapLibrary::SetMapProgress(
	FImmortalMapSystemState& State,
	const FImmortalMapProgress& Progress)
{
	FImmortalMapDefinition Definition;
	if (!GetMapDefinition(Progress.MapId, Definition)) return false;
	NormalizeState(State);
	FImmortalMapProgress* Destination = State.MapProgress.FindByPredicate([&Progress](const FImmortalMapProgress& Entry)
	{
		return Entry.MapId == Progress.MapId;
	});
	if (!Destination) return false;
	*Destination = Progress;
	ClampProgressToDefinition(*Destination, Definition);
	return true;
}

int32 UImmortalMapLibrary::GetEffectiveAdventureStage(const FName MapId, const int32 LocalStage)
{
	FImmortalMapDefinition Definition;
	if (!GetMapDefinition(MapId, Definition)) return FMath::Clamp(LocalStage, 1, 999);
	const int32 SafeLocalStage = FMath::Clamp(LocalStage, 1, Definition.MaximumStage);
	return FMath::Clamp(SafeLocalStage + Definition.OrderIndex * 125, 1, 999);
}
