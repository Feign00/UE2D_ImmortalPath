// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalWorldBossTypes.h"

#include "../Artifacts/ImmortalArtifactTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "Engine/DataTable.h"
#include "Misc/PackageName.h"

namespace
{
	FImmortalWorldBossDefinition MakeWorldBoss(
		const TCHAR* Id,
		const TCHAR* Name,
		const TCHAR* Description,
		const int32 RequiredRealm,
		const int32 Stage,
		const float TimeLimit,
		const float Health,
		const float Attack,
		const float Defense,
		const EImmortalEquipmentQuality Quality,
		const int32 EquipmentDrops,
		const int32 EquipmentLevelBonus,
		const int32 SpiritStones,
		const TCHAR* RareMaterial,
		const int32 RareMaterialQuantity,
		const int32 ArtifactFragments,
		const TCHAR* Artifact,
		const FLinearColor& Color)
	{
		FImmortalWorldBossDefinition Result;
		Result.BossId = FName(Id);
		Result.DisplayName = FText::FromString(Name);
		Result.Description = FText::FromString(Description);
		Result.RequiredRealmIndex = RequiredRealm;
		Result.RecommendedStage = Stage;
		Result.TimeLimitSeconds = TimeLimit;
		Result.HealthMultiplier = Health;
		Result.AttackMultiplier = Attack;
		Result.DefenseBonus = Defense;
		Result.MinimumEquipmentQuality = Quality;
		Result.GuaranteedEquipmentDrops = EquipmentDrops;
		Result.EquipmentLevelBonus = EquipmentLevelBonus;
		Result.SpiritStoneReward = SpiritStones;
		Result.RareMaterialId = FName(RareMaterial);
		Result.RareMaterialQuantity = RareMaterialQuantity;
		Result.ArtifactFragmentQuantity = ArtifactFragments;
		Result.FirstClearArtifactId = FName(Artifact);
		Result.DisplayColor = Color;
		return Result;
	}

	const TMap<FName, FImmortalWorldBossDefinition>& GetFallbackCatalog()
	{
		static const TMap<FName, FImmortalWorldBossDefinition> Catalog =
		{
			{TEXT("AzureScaleDragon"), MakeWorldBoss(
				TEXT("AzureScaleDragon"),
				TEXT("苍鳞妖龙"),
				TEXT("盘踞青云灵脉的上古妖龙。第二阶段召来妖兽，低血量时进入狂暴。"),
				0, 30, 75.0f, 12.0f, 1.80f, 6.0f,
				EImmortalEquipmentQuality::Rare, 4, 4, 360,
				TEXT("DemonCore"), 8, 3, TEXT("XuanGuangSword"),
				FLinearColor(0.18f, 0.82f, 1.0f, 1.0f))},
			{TEXT("SevenStarDemonLord"), MakeWorldBoss(
				TEXT("SevenStarDemonLord"),
				TEXT("七星魔君"),
				TEXT("借七星魔火重临人间的魔道首领，擅长远程星陨与连续召唤。"),
				2, 160, 90.0f, 15.0f, 2.05f, 10.0f,
				EImmortalEquipmentQuality::Epic, 4, 7, 900,
				TEXT("SpiritIron"), 10, 5, TEXT("SevenStarBanner"),
				FLinearColor(0.72f, 0.32f, 1.0f, 1.0f))},
			{TEXT("CloudAbyssLeviathan"), MakeWorldBoss(
				TEXT("CloudAbyssLeviathan"),
				TEXT("云渊蜃主"),
				TEXT("吞吐云海的秘境巨兽，范围潮汐会贯穿整条战斗视野。"),
				4, 360, 105.0f, 18.0f, 2.30f, 16.0f,
				EImmortalEquipmentQuality::Legendary, 5, 10, 1800,
				TEXT("DemonBone"), 14, 7, TEXT("FlowingCloudUmbrella"),
				FLinearColor(0.30f, 0.92f, 0.78f, 1.0f))},
			{TEXT("ChaosHeavenBeast"), MakeWorldBoss(
				TEXT("ChaosHeavenBeast"),
				TEXT("混沌天兽"),
				TEXT("自破碎天门坠落的混沌凶兽，最终阶段会以极高攻速发动全屏重击。"),
				6, 650, 120.0f, 22.0f, 2.60f, 24.0f,
				EImmortalEquipmentQuality::Immortal, 6, 14, 3600,
				TEXT("ArtifactFragment"), 12, 10, TEXT("ChaosPearl"),
				FLinearColor(1.0f, 0.34f, 0.18f, 1.0f))}
		};
		return Catalog;
	}

	UDataTable* GetOptionalWorldBossTable()
	{
		static TWeakObjectPtr<UDataTable> CachedTable;
		static bool bAttemptedLoad = false;
		if (!bAttemptedLoad)
		{
			bAttemptedLoad = true;
			const FString PackageName(TEXT("/Game/GAME/Data/DT_WorldBosses"));
			if (FPackageName::DoesPackageExist(PackageName))
			{
				CachedTable = LoadObject<UDataTable>(
					nullptr, TEXT("/Game/GAME/Data/DT_WorldBosses.DT_WorldBosses"));
			}
		}
		return CachedTable.Get();
	}
}

bool FImmortalWorldBossDefinition::IsValid() const
{
	FImmortalMaterialDefinition MaterialDefinition;
	FImmortalArtifactDefinition ArtifactDefinition;
	return !BossId.IsNone()
		&& !DisplayName.IsEmpty()
		&& RequiredRealmIndex >= 0
		&& RecommendedStage >= 1
		&& TimeLimitSeconds >= 15.0f
		&& HealthMultiplier >= 1.0f
		&& AttackMultiplier > 0.0f
		&& GuaranteedEquipmentDrops >= 1
		&& SpiritStoneReward >= 1
		&& RareMaterialQuantity >= 1
		&& ArtifactFragmentQuantity >= 1
		&& UImmortalMaterialLibrary::GetMaterialDefinition(RareMaterialId, MaterialDefinition)
		&& UImmortalArtifactLibrary::GetArtifactDefinition(FirstClearArtifactId, ArtifactDefinition);
}

bool FImmortalWorldBossRewardBundle::IsValid() const
{
	if (!RewardId.IsValid()
		|| BossId.IsNone()
		|| SpiritStones <= 0
		|| EquipmentItems.IsEmpty())
	{
		return false;
	}
	for (const FImmortalEquipmentItem& Item : EquipmentItems)
	{
		if (!Item.IsValid())
		{
			return false;
		}
	}
	for (const FImmortalMaterialStack& Material : Materials)
	{
		if (!Material.IsValid())
		{
			return false;
		}
	}
	// Do not depend on the live Boss/artifact catalogs here. A saved bundle is
	// self-contained and must survive a future DataTable row rename or a
	// temporarily unavailable content asset; delivery can remain pending.
	return true;
}

TArray<FName> UImmortalWorldBossLibrary::GetKnownWorldBossIds()
{
	TArray<FImmortalWorldBossDefinition> Definitions;
	for (const TPair<FName, FImmortalWorldBossDefinition>& Pair : GetFallbackCatalog())
	{
		Definitions.Add(Pair.Value);
	}
	if (const UDataTable* Table = GetOptionalWorldBossTable())
	{
		for (const FName RowName : Table->GetRowNames())
		{
			if (const FImmortalWorldBossDefinition* Row =
				Table->FindRow<FImmortalWorldBossDefinition>(RowName, TEXT("World Boss catalog"), false))
			{
				FImmortalWorldBossDefinition Definition = *Row;
				if (Definition.BossId.IsNone())
				{
					Definition.BossId = RowName;
				}
				Definitions.RemoveAll([&Definition](const FImmortalWorldBossDefinition& Existing)
				{
					return Existing.BossId == Definition.BossId;
				});
				Definitions.Add(Definition);
			}
		}
	}
	Definitions.Sort([](const FImmortalWorldBossDefinition& Left, const FImmortalWorldBossDefinition& Right)
	{
		if (Left.RequiredRealmIndex != Right.RequiredRealmIndex)
		{
			return Left.RequiredRealmIndex < Right.RequiredRealmIndex;
		}
		if (Left.RecommendedStage != Right.RecommendedStage)
		{
			return Left.RecommendedStage < Right.RecommendedStage;
		}
		return Left.BossId.LexicalLess(Right.BossId);
	});

	TArray<FName> Result;
	for (const FImmortalWorldBossDefinition& Definition : Definitions)
	{
		if (!Definition.BossId.IsNone())
		{
			Result.AddUnique(Definition.BossId);
		}
	}
	return Result;
}

bool UImmortalWorldBossLibrary::GetWorldBossDefinition(
	const FName BossId,
	FImmortalWorldBossDefinition& OutDefinition)
{
	if (BossId.IsNone())
	{
		return false;
	}
	if (const UDataTable* Table = GetOptionalWorldBossTable())
	{
		if (const FImmortalWorldBossDefinition* Row =
			Table->FindRow<FImmortalWorldBossDefinition>(BossId, TEXT("World Boss lookup"), false))
		{
			OutDefinition = *Row;
			if (OutDefinition.BossId.IsNone())
			{
				OutDefinition.BossId = BossId;
			}
			if (OutDefinition.BossId == BossId && OutDefinition.IsValid())
			{
				return true;
			}
		}
		for (const FName RowName : Table->GetRowNames())
		{
			if (const FImmortalWorldBossDefinition* Row =
				Table->FindRow<FImmortalWorldBossDefinition>(
					RowName, TEXT("World Boss ID lookup"), false))
			{
				const FName EffectiveBossId =
					Row->BossId.IsNone() ? RowName : Row->BossId;
				if (EffectiveBossId == BossId)
				{
					OutDefinition = *Row;
					OutDefinition.BossId = EffectiveBossId;
					return OutDefinition.IsValid();
				}
			}
		}
	}
	if (const FImmortalWorldBossDefinition* Found = GetFallbackCatalog().Find(BossId))
	{
		OutDefinition = *Found;
		return true;
	}
	return false;
}

FImmortalWorldBossState UImmortalWorldBossLibrary::CreateDefaultState()
{
	FImmortalWorldBossState Result;
	Result.bInitialized = true;
	for (const FName BossId : GetKnownWorldBossIds())
	{
		FImmortalWorldBossProgress Progress;
		Progress.BossId = BossId;
		Result.BossProgress.Add(Progress);
	}
	return Result;
}

bool UImmortalWorldBossLibrary::NormalizeState(FImmortalWorldBossState& State)
{
	const bool bWasInitialized = State.bInitialized;
	const int32 PreviousRevision = State.Revision;
	const TArray<FImmortalWorldBossProgress> Previous = State.BossProgress;
	const TArray<FName> KnownIds = GetKnownWorldBossIds();
	TArray<FImmortalWorldBossProgress> Normalized;
	for (const FName BossId : KnownIds)
	{
		FImmortalWorldBossProgress Combined;
		Combined.BossId = BossId;
		for (const FImmortalWorldBossProgress& Existing : Previous)
		{
			if (Existing.BossId != BossId)
			{
				continue;
			}
			Combined.DefeatCount = FMath::Max(Combined.DefeatCount, FMath::Max(Existing.DefeatCount, 0));
			if (Existing.BestClearSeconds > 0.0f
				&& (Combined.BestClearSeconds <= 0.0f || Existing.BestClearSeconds < Combined.BestClearSeconds))
			{
				Combined.BestClearSeconds = Existing.BestClearSeconds;
			}
			Combined.LastDefeatedUtcTicks =
				FMath::Max<int64>(Combined.LastDefeatedUtcTicks, FMath::Max<int64>(Existing.LastDefeatedUtcTicks, 0));
			Combined.bFirstClearArtifactClaimed =
				Combined.bFirstClearArtifactClaimed || Existing.bFirstClearArtifactClaimed;
		}
		Combined.BestClearSeconds = FMath::Max(Combined.BestClearSeconds, 0.0f);
		Normalized.Add(Combined);
	}

	TArray<FImmortalWorldBossRewardBundle> NormalizedRewards;
	TSet<FGuid> SeenRewardIds;
	bool bPendingRewardsChanged = false;
	for (FImmortalWorldBossRewardBundle Reward : State.PendingRewards)
	{
		if (!Reward.RewardId.IsValid() || SeenRewardIds.Contains(Reward.RewardId))
		{
			bPendingRewardsChanged = true;
			continue;
		}
		const FImmortalWorldBossRewardBundle OriginalReward = Reward;
		for (FImmortalEquipmentItem& Item : Reward.EquipmentItems)
		{
			UImmortalEquipmentLibrary::NormalizeForgingState(Item);
		}
		UImmortalMaterialLibrary::NormalizeInventory(Reward.Materials);
		Reward.CreatedUtcTicks = FMath::Max<int64>(Reward.CreatedUtcTicks, 0);
		Reward.SpiritStones = FMath::Max(Reward.SpiritStones, 0);
		if (Reward.IsValid())
		{
			bPendingRewardsChanged =
				bPendingRewardsChanged
				|| !FImmortalWorldBossRewardBundle::StaticStruct()->CompareScriptStruct(
					&OriginalReward, &Reward, 0);
			SeenRewardIds.Add(Reward.RewardId);
			NormalizedRewards.Add(MoveTemp(Reward));
		}
		else
		{
			bPendingRewardsChanged = true;
		}
	}

	const int32 PreviousPendingCount = State.PendingRewards.Num();
	State.bInitialized = true;
	State.Revision = FMath::Max(PreviousRevision, 0);
	State.BossProgress = MoveTemp(Normalized);
	State.PendingRewards = MoveTemp(NormalizedRewards);

	bool bChanged = !bWasInitialized || Previous.Num() != State.BossProgress.Num()
		|| PreviousRevision < 0 || PreviousPendingCount != State.PendingRewards.Num()
		|| bPendingRewardsChanged;
	if (!bChanged)
	{
		for (int32 Index = 0; Index < Previous.Num(); ++Index)
		{
			const FImmortalWorldBossProgress& Before = Previous[Index];
			const FImmortalWorldBossProgress& After = State.BossProgress[Index];
			if (Before.BossId != After.BossId
				|| Before.DefeatCount != After.DefeatCount
				|| !FMath::IsNearlyEqual(Before.BestClearSeconds, After.BestClearSeconds)
				|| Before.LastDefeatedUtcTicks != After.LastDefeatedUtcTicks
				|| Before.bFirstClearArtifactClaimed != After.bFirstClearArtifactClaimed)
			{
				bChanged = true;
				break;
			}
		}
	}
	if (bChanged)
	{
		++State.Revision;
	}
	return bChanged;
}

bool UImmortalWorldBossLibrary::GetProgress(
	const FImmortalWorldBossState& State,
	const FName BossId,
	FImmortalWorldBossProgress& OutProgress)
{
	if (const FImmortalWorldBossProgress* Found =
		State.BossProgress.FindByPredicate([BossId](const FImmortalWorldBossProgress& Entry)
		{
			return Entry.BossId == BossId;
		}))
	{
		OutProgress = *Found;
		return true;
	}
	return false;
}

bool UImmortalWorldBossLibrary::IsUnlocked(
	const FImmortalWorldBossDefinition& Definition,
	const int32 RealmIndex)
{
	return Definition.IsValid() && FMath::Max(RealmIndex, 0) >= Definition.RequiredRealmIndex;
}

FImmortalWorldBossRewardBundle UImmortalWorldBossLibrary::CreateRewardBundle(
	const FImmortalWorldBossDefinition& Definition,
	const int32 EquipmentItemLevel,
	const bool bIncludeFirstClearArtifact,
	const int64 CurrentUtcTicks)
{
	FImmortalWorldBossRewardBundle Result;
	if (!Definition.IsValid())
	{
		return Result;
	}
	Result.RewardId = FGuid::NewGuid();
	Result.BossId = Definition.BossId;
	Result.CreatedUtcTicks = FMath::Max<int64>(CurrentUtcTicks, 0);
	Result.SpiritStones = FMath::Max(Definition.SpiritStoneReward, 1);
	for (int32 Index = 0; Index < FMath::Max(Definition.GuaranteedEquipmentDrops, 1); ++Index)
	{
		Result.EquipmentItems.Add(
			UImmortalEquipmentLibrary::GenerateRandomEquipmentWithMinimumQuality(
				FMath::Max(EquipmentItemLevel, 1),
				Definition.MinimumEquipmentQuality));
	}
	UImmortalMaterialLibrary::AddMaterialStack(
		Result.Materials,
		Definition.RareMaterialId,
		FMath::Max(Definition.RareMaterialQuantity, 1));
	UImmortalMaterialLibrary::AddMaterialStack(
		Result.Materials,
		TEXT("ArtifactFragment"),
		FMath::Max(Definition.ArtifactFragmentQuantity, 1));
	Result.ArtifactId = bIncludeFirstClearArtifact
		? Definition.FirstClearArtifactId
		: NAME_None;
	return Result.IsValid() ? Result : FImmortalWorldBossRewardBundle();
}

FImmortalWorldBossRecordResult UImmortalWorldBossLibrary::RecordDefeat(
	FImmortalWorldBossState& State,
	const FName BossId,
	const float ClearSeconds,
	const int64 CurrentUtcTicks)
{
	FImmortalWorldBossRecordResult Result;
	FImmortalWorldBossDefinition Definition;
	if (!GetWorldBossDefinition(BossId, Definition) || ClearSeconds <= 0.0f)
	{
		return Result;
	}
	NormalizeState(State);
	FImmortalWorldBossProgress* Progress =
		State.BossProgress.FindByPredicate([BossId](const FImmortalWorldBossProgress& Entry)
		{
			return Entry.BossId == BossId;
		});
	if (!Progress)
	{
		return Result;
	}

	Result.bFirstClear = Progress->DefeatCount == 0;
	Result.bNewBestTime = Progress->BestClearSeconds <= 0.0f
		|| ClearSeconds < Progress->BestClearSeconds;
	Progress->DefeatCount = FMath::Max(Progress->DefeatCount, 0) + 1;
	if (Result.bNewBestTime)
	{
		Progress->BestClearSeconds = ClearSeconds;
	}
	Progress->LastDefeatedUtcTicks = FMath::Max<int64>(CurrentUtcTicks, 0);
	if (Result.bFirstClear)
	{
		Progress->bFirstClearArtifactClaimed = true;
	}
	++State.Revision;
	Result.bSucceeded = true;
	Result.Progress = *Progress;
	return Result;
}
