// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalEndlessDungeonTypes.h"

#include "Engine/DataTable.h"
#include "Misc/PackageName.h"

namespace
{
	constexpr int32 AbsoluteMaximumFloor = 9999;
	const FName RulesRowName(TEXT("Rules"));
	const FString RulesPackageName(TEXT("/Game/GAME/Data/DT_EndlessDungeon"));
	const FString RulesObjectPath(TEXT("/Game/GAME/Data/DT_EndlessDungeon.DT_EndlessDungeon"));

	const FImmortalEndlessDungeonRules& GetFallbackRules()
	{
		static const FImmortalEndlessDungeonRules Rules;
		return Rules;
	}

	UDataTable* GetOptionalRulesTable()
	{
		static TWeakObjectPtr<UDataTable> CachedTable;
		static bool bAttemptedLoad = false;
		if (!bAttemptedLoad)
		{
			bAttemptedLoad = true;
			if (FPackageName::DoesPackageExist(RulesPackageName))
			{
				CachedTable = LoadObject<UDataTable>(nullptr, *RulesObjectPath);
			}
		}
		return CachedTable.Get();
	}

	bool TryGetRulesOverride(FImmortalEndlessDungeonRules& OutRules)
	{
		const UDataTable* Table = GetOptionalRulesTable();
		if (!Table)
		{
			return false;
		}

		if (const FImmortalEndlessDungeonRules* ExplicitRules =
			Table->FindRow<FImmortalEndlessDungeonRules>(
				RulesRowName, TEXT("Endless Dungeon rules"), false))
		{
			if (ExplicitRules->IsValid())
			{
				OutRules = *ExplicitRules;
				return true;
			}
			UE_LOG(LogTemp, Warning,
				TEXT("DT_EndlessDungeon Rules row is invalid; using the native fallback"));
			return false;
		}

		TArray<FName> RowNames = Table->GetRowNames();
		RowNames.Sort([](const FName Left, const FName Right)
		{
			return Left.LexicalLess(Right);
		});
		for (const FName RowName : RowNames)
		{
			if (const FImmortalEndlessDungeonRules* Candidate =
				Table->FindRow<FImmortalEndlessDungeonRules>(
					RowName, TEXT("Endless Dungeon rules fallback row"), false))
			{
				if (Candidate->IsValid())
				{
					OutRules = *Candidate;
					return true;
				}
			}
		}
		UE_LOG(LogTemp, Warning,
			TEXT("DT_EndlessDungeon contains no valid rules row; using the native fallback"));
		return false;
	}

	void AdvanceRevision(FImmortalEndlessDungeonState& State)
	{
		if (State.Revision < MAX_int32)
		{
			++State.Revision;
		}
	}

	FName MakeMilestoneId(const int32 Floor)
	{
		return FName(*FString::Printf(TEXT("EndlessFloor_%04d"), Floor));
	}

	/**
	 * Pending rewards are intentionally normalized without consulting the live
	 * material catalog. Unknown IDs remain durable and can be delivered if a
	 * temporarily missing DataTable row returns in a later build.
	 */
	void NormalizeSelfContainedMaterials(TArray<FImmortalMaterialStack>& Materials)
	{
		TMap<FName, int64> Quantities;
		for (const FImmortalMaterialStack& Stack : Materials)
		{
			if (Stack.MaterialId.IsNone() || Stack.Quantity <= 0)
			{
				continue;
			}
			int64& Total = Quantities.FindOrAdd(Stack.MaterialId);
			Total = FMath::Min<int64>(
				MAX_int32,
				Total + static_cast<int64>(Stack.Quantity));
		}

		Materials.Reset(Quantities.Num());
		for (const TPair<FName, int64>& Pair : Quantities)
		{
			FImmortalMaterialStack Stack;
			Stack.MaterialId = Pair.Key;
			Stack.Quantity = static_cast<int32>(Pair.Value);
			Materials.Add(Stack);
		}
		Materials.Sort([](
			const FImmortalMaterialStack& Left,
			const FImmortalMaterialStack& Right)
		{
			return Left.MaterialId.LexicalLess(Right.MaterialId);
		});
	}
}

bool FImmortalEndlessDungeonRules::IsValid() const
{
	FImmortalMaterialDefinition MaterialDefinition;
	return MaximumFloor >= 1
		&& MaximumFloor <= AbsoluteMaximumFloor
		&& CheckpointInterval >= 1
		&& EliteFloorInterval >= 2
		&& BossFloorInterval >= 2
		&& NormalEnemyCount >= 1
		&& EliteEnemyCount >= 1
		&& BossEnemyCount >= 1
		&& PhaseTwoSummonCount >= 0
		&& PhaseThreeSummonCount >= PhaseTwoSummonCount
		&& FMath::IsFinite(BaseHealthMultiplier)
		&& BaseHealthMultiplier > 0.0f
		&& FMath::IsFinite(HealthMultiplierPerFloor)
		&& HealthMultiplierPerFloor >= 0.0f
		&& FMath::IsFinite(HealthMultiplierPerEliteThreshold)
		&& HealthMultiplierPerEliteThreshold >= 0.0f
		&& FMath::IsFinite(HealthMultiplierPerBossThreshold)
		&& HealthMultiplierPerBossThreshold >= 0.0f
		&& FMath::IsFinite(BaseAttackMultiplier)
		&& BaseAttackMultiplier > 0.0f
		&& FMath::IsFinite(AttackMultiplierPerFloor)
		&& AttackMultiplierPerFloor >= 0.0f
		&& FMath::IsFinite(AttackMultiplierPerEliteThreshold)
		&& AttackMultiplierPerEliteThreshold >= 0.0f
		&& FMath::IsFinite(AttackMultiplierPerBossThreshold)
		&& AttackMultiplierPerBossThreshold >= 0.0f
		&& FMath::IsFinite(BaseDefenseBonus)
		&& BaseDefenseBonus >= 0.0f
		&& FMath::IsFinite(DefenseBonusPerFloor)
		&& DefenseBonusPerFloor >= 0.0f
		&& FMath::IsFinite(DefenseBonusPerEliteThreshold)
		&& DefenseBonusPerEliteThreshold >= 0.0f
		&& FMath::IsFinite(DefenseBonusPerBossThreshold)
		&& DefenseBonusPerBossThreshold >= 0.0f
		&& BaseSpiritStones >= 1
		&& SpiritStonesPerFloor >= 0
		&& BaseMaterialQuantity >= 1
		&& MaterialQuantityFloorInterval >= 1
		&& EquipmentLevelFloorInterval >= 1
		&& EliteEquipmentCount >= 1
		&& EliteEquipmentCount <= 2
		&& BossEquipmentCount == 2
		&& UImmortalMaterialLibrary::GetMaterialDefinition(
			BaseMaterialId, MaterialDefinition);
}

bool FImmortalEndlessFloorDescriptor::IsValid() const
{
	if (Floor < 1
		|| Floor > AbsoluteMaximumFloor
		|| RequiredKills < 1
		|| !FMath::IsFinite(HealthMultiplier)
		|| HealthMultiplier <= 0.0f
		|| !FMath::IsFinite(AttackMultiplier)
		|| AttackMultiplier <= 0.0f
		|| !FMath::IsFinite(DefenseBonus)
		|| DefenseBonus < 0.0f
		|| EquipmentItemLevel < 1
		|| EquipmentCount < 0
		|| EquipmentCount > 2
		|| SpiritStones < 1
		|| Materials.IsEmpty())
	{
		return false;
	}
	for (const FImmortalMaterialStack& Material : Materials)
	{
		if (!Material.IsValid())
		{
			return false;
		}
	}
	return true;
}

bool FImmortalEndlessRewardBundle::IsValid() const
{
	if (!RewardId.IsValid()
		|| ClearedFloor < 1
		|| ClearedFloor > AbsoluteMaximumFloor
		|| SpiritStones < 1
		|| Materials.IsEmpty())
	{
		return false;
	}

	TSet<FGuid> SeenItemIds;
	for (const FImmortalEquipmentItem& Item : EquipmentItems)
	{
		if (!Item.IsValid() || SeenItemIds.Contains(Item.ItemId))
		{
			return false;
		}
		SeenItemIds.Add(Item.ItemId);
	}
	for (const FImmortalMaterialStack& Material : Materials)
	{
		if (!Material.IsValid())
		{
			return false;
		}
	}
	// No rule, DataTable or material-catalog lookup belongs here. Once persisted,
	// the exact payload is authoritative even if content definitions later move.
	return true;
}

FImmortalEndlessDungeonRules UImmortalEndlessDungeonLibrary::GetRules()
{
	FImmortalEndlessDungeonRules Rules;
	if (TryGetRulesOverride(Rules))
	{
		return Rules;
	}
	return GetFallbackRules();
}

bool UImmortalEndlessDungeonLibrary::GetFloorDescriptor(
	const int32 Floor,
	FImmortalEndlessFloorDescriptor& OutDescriptor)
{
	OutDescriptor = FImmortalEndlessFloorDescriptor();
	const FImmortalEndlessDungeonRules Rules = GetRules();
	if (!Rules.IsValid() || Floor < 1 || Floor > Rules.MaximumFloor)
	{
		return false;
	}

	FImmortalEndlessFloorDescriptor Result;
	Result.Floor = Floor;
	Result.bBoss = Floor % Rules.BossFloorInterval == 0;
	Result.bElite = !Result.bBoss
		&& Floor % Rules.EliteFloorInterval == 0;
	Result.RequiredKills = Result.bBoss
		? Rules.BossEnemyCount
		: (Result.bElite ? Rules.EliteEnemyCount : Rules.NormalEnemyCount);

	const int32 CompletedEliteThresholds =
		Floor / Rules.EliteFloorInterval;
	const int32 CompletedBossThresholds =
		Floor / Rules.BossFloorInterval;
	const float FloorOffset = static_cast<float>(Floor - 1);
	Result.HealthMultiplier = Rules.BaseHealthMultiplier
		+ FloorOffset * Rules.HealthMultiplierPerFloor
		+ static_cast<float>(CompletedEliteThresholds)
			* Rules.HealthMultiplierPerEliteThreshold
		+ static_cast<float>(CompletedBossThresholds)
			* Rules.HealthMultiplierPerBossThreshold;
	Result.AttackMultiplier = Rules.BaseAttackMultiplier
		+ FloorOffset * Rules.AttackMultiplierPerFloor
		+ static_cast<float>(CompletedEliteThresholds)
			* Rules.AttackMultiplierPerEliteThreshold
		+ static_cast<float>(CompletedBossThresholds)
			* Rules.AttackMultiplierPerBossThreshold;
	Result.DefenseBonus = Rules.BaseDefenseBonus
		+ FloorOffset * Rules.DefenseBonusPerFloor
		+ static_cast<float>(CompletedEliteThresholds)
			* Rules.DefenseBonusPerEliteThreshold
		+ static_cast<float>(CompletedBossThresholds)
			* Rules.DefenseBonusPerBossThreshold;

	Result.EquipmentItemLevel = FMath::Max(
		1 + (Floor - 1) / Rules.EquipmentLevelFloorInterval,
		1);
	const int32 MaximumQualityRank =
		static_cast<int32>(EImmortalEquipmentQuality::Divine);
	const int32 QualityRank = FMath::Clamp(
		Floor / Rules.BossFloorInterval,
		0,
		MaximumQualityRank);
	Result.MinimumEquipmentQuality =
		static_cast<EImmortalEquipmentQuality>(QualityRank);
	Result.EquipmentCount = Result.bBoss
		? FMath::Clamp(Rules.BossEquipmentCount, 0, 2)
		: (Result.bElite
			? FMath::Clamp(Rules.EliteEquipmentCount, 0, 2)
			: 0);

	const int64 StoneReward = static_cast<int64>(Rules.BaseSpiritStones)
		+ static_cast<int64>(Floor - 1)
			* static_cast<int64>(Rules.SpiritStonesPerFloor);
	Result.SpiritStones = static_cast<int32>(
		FMath::Clamp<int64>(StoneReward, 1, MAX_int32));

	int64 MaterialQuantity = static_cast<int64>(Rules.BaseMaterialQuantity)
		+ (Floor - 1) / Rules.MaterialQuantityFloorInterval;
	if (Result.bBoss)
	{
		MaterialQuantity *= 3;
	}
	else if (Result.bElite)
	{
		MaterialQuantity *= 2;
	}
	MaterialQuantity = FMath::Clamp<int64>(
		MaterialQuantity, 1, MAX_int32);
	UImmortalMaterialLibrary::AddMaterialStack(
		Result.Materials,
		Rules.BaseMaterialId,
		static_cast<int32>(MaterialQuantity));

	if (!Result.IsValid())
	{
		return false;
	}
	OutDescriptor = MoveTemp(Result);
	return true;
}

FImmortalEndlessDungeonState
UImmortalEndlessDungeonLibrary::CreateDefaultState()
{
	FImmortalEndlessDungeonState Result;
	Result.bInitialized = true;
	return Result;
}

bool UImmortalEndlessDungeonLibrary::NormalizeState(
	FImmortalEndlessDungeonState& State)
{
	const FImmortalEndlessDungeonState Before = State;

	State.bInitialized = true;
	State.Revision = FMath::Max(State.Revision, 0);
	// Rules.MaximumFloor is live content and may temporarily be lowered. Never
	// erase the durable high-water mark because doing so would make old floors
	// rewardable again when the content cap is restored.
	State.HighestClearedFloor = FMath::Clamp(
		State.HighestClearedFloor, 0, AbsoluteMaximumFloor);
	State.TotalFloorsCleared = FMath::Max<int64>(
		FMath::Max<int64>(State.TotalFloorsCleared, 0),
		State.HighestClearedFloor);
	State.TotalRuns = FMath::Max(State.TotalRuns, 0);
	if (State.HighestClearedFloor > 0)
	{
		State.TotalRuns = FMath::Max(State.TotalRuns, 1);
	}
	State.LastClearedUtcTicks =
		FMath::Max<int64>(State.LastClearedUtcTicks, 0);
	if (State.HighestClearedFloor == 0)
	{
		State.LastClearedUtcTicks = 0;
	}

	TSet<FName> SeenMilestones;
	TArray<FName> NormalizedMilestones;
	for (const FName MilestoneId : State.ClaimedMilestoneIds)
	{
		if (MilestoneId.IsNone() || SeenMilestones.Contains(MilestoneId))
		{
			continue;
		}
		SeenMilestones.Add(MilestoneId);
		NormalizedMilestones.Add(MilestoneId);
	}
	NormalizedMilestones.Sort([](const FName Left, const FName Right)
	{
		return Left.LexicalLess(Right);
	});
	State.ClaimedMilestoneIds = MoveTemp(NormalizedMilestones);

	TSet<FGuid> SeenRewardIds;
	TSet<int32> SeenRewardFloors;
	TArray<FImmortalEndlessRewardBundle> NormalizedRewards;
	int32 RecoveredHighestFloor = State.HighestClearedFloor;
	int64 RecoveredLastClearedUtcTicks = State.LastClearedUtcTicks;
	for (FImmortalEndlessRewardBundle Reward : State.PendingRewards)
	{
		if (!Reward.RewardId.IsValid()
			|| SeenRewardIds.Contains(Reward.RewardId)
			|| SeenRewardFloors.Contains(Reward.ClearedFloor))
		{
			continue;
		}
		Reward.CreatedUtcTicks =
			FMath::Max<int64>(Reward.CreatedUtcTicks, 0);
		Reward.SpiritStones = FMath::Max(Reward.SpiritStones, 0);
		for (FImmortalEquipmentItem& Item : Reward.EquipmentItems)
		{
			UImmortalEquipmentLibrary::NormalizeForgingState(
				Item,
				false,
				false);
		}
		NormalizeSelfContainedMaterials(Reward.Materials);
		if (!Reward.IsValid())
		{
			continue;
		}
		SeenRewardIds.Add(Reward.RewardId);
		SeenRewardFloors.Add(Reward.ClearedFloor);
		RecoveredHighestFloor = FMath::Max(
			RecoveredHighestFloor,
			Reward.ClearedFloor);
		RecoveredLastClearedUtcTicks = FMath::Max(
			RecoveredLastClearedUtcTicks,
			Reward.CreatedUtcTicks);
		NormalizedRewards.Add(MoveTemp(Reward));
	}
	State.PendingRewards = MoveTemp(NormalizedRewards);
	State.HighestClearedFloor = FMath::Clamp(
		RecoveredHighestFloor,
		0,
		AbsoluteMaximumFloor);
	State.TotalFloorsCleared = FMath::Max<int64>(
		State.TotalFloorsCleared,
		State.HighestClearedFloor);
	if (State.HighestClearedFloor > 0)
	{
		State.TotalRuns = FMath::Max(State.TotalRuns, 1);
		State.LastClearedUtcTicks = FMath::Max<int64>(
			RecoveredLastClearedUtcTicks,
			0);
	}

	const bool bChanged =
		!FImmortalEndlessDungeonState::StaticStruct()->CompareScriptStruct(
			&Before, &State, 0);
	if (bChanged)
	{
		AdvanceRevision(State);
	}
	return bChanged;
}

int32 UImmortalEndlessDungeonLibrary::GetCheckpointStartFloor(
	const FImmortalEndlessDungeonState& State)
{
	const FImmortalEndlessDungeonRules Rules = GetRules();
	const int32 MaximumFloor = Rules.IsValid()
		? Rules.MaximumFloor
		: AbsoluteMaximumFloor;
	const int32 Interval = Rules.IsValid()
		? FMath::Max(Rules.CheckpointInterval, 1)
		: 10;
	const int32 HighestFloor = FMath::Clamp(
		State.HighestClearedFloor, 0, MaximumFloor);
	const int64 SegmentStart = static_cast<int64>(
		HighestFloor / Interval) * Interval + 1;
	return static_cast<int32>(FMath::Clamp<int64>(
		SegmentStart, 1, MaximumFloor));
}

FImmortalEndlessRecordResult
UImmortalEndlessDungeonLibrary::RecordFloorClear(
	FImmortalEndlessDungeonState& State,
	const int32 Floor,
	const int64 CurrentUtcTicks)
{
	FImmortalEndlessRecordResult Result;
	NormalizeState(State);
	Result.PreviousHighestFloor = State.HighestClearedFloor;
	Result.HighestClearedFloor = State.HighestClearedFloor;

	FImmortalEndlessFloorDescriptor Descriptor;
	const int64 ExpectedFloor =
		static_cast<int64>(State.HighestClearedFloor) + 1;
	if (!GetFloorDescriptor(Floor, Descriptor)
		|| ExpectedFloor > MAX_int32
		|| Floor != static_cast<int32>(ExpectedFloor))
	{
		return Result;
	}

	State.HighestClearedFloor = Floor;
	State.TotalFloorsCleared = State.TotalFloorsCleared >= MAX_int64
		? MAX_int64
		: State.TotalFloorsCleared + 1;
	State.TotalRuns = FMath::Max(State.TotalRuns, 1);
	State.LastClearedUtcTicks =
		FMath::Max<int64>(CurrentUtcTicks, 0);
	if (Descriptor.bElite || Descriptor.bBoss)
	{
		State.ClaimedMilestoneIds.AddUnique(MakeMilestoneId(Floor));
		State.ClaimedMilestoneIds.Sort([](const FName Left, const FName Right)
		{
			return Left.LexicalLess(Right);
		});
	}
	AdvanceRevision(State);

	Result.bSucceeded = true;
	Result.bNewHighest = true;
	Result.HighestClearedFloor = State.HighestClearedFloor;
	return Result;
}

FImmortalEndlessRewardBundle
UImmortalEndlessDungeonLibrary::CreateRewardBundle(
	const FImmortalEndlessFloorDescriptor& Descriptor,
	const int64 CurrentUtcTicks)
{
	FImmortalEndlessRewardBundle Result;
	if (!Descriptor.IsValid())
	{
		return Result;
	}

	Result.RewardId = FGuid::NewGuid();
	Result.ClearedFloor = Descriptor.Floor;
	Result.CreatedUtcTicks = FMath::Max<int64>(
		CurrentUtcTicks, 0);
	Result.SpiritStones = Descriptor.SpiritStones;
	Result.Materials = Descriptor.Materials;
	for (int32 Index = 0;
		Index < FMath::Clamp(Descriptor.EquipmentCount, 0, 2);
		++Index)
	{
		Result.EquipmentItems.Add(
			UImmortalEquipmentLibrary::GenerateRandomEquipmentWithMinimumQuality(
				Descriptor.EquipmentItemLevel,
				Descriptor.MinimumEquipmentQuality));
	}
	return Result.IsValid()
		? Result
		: FImmortalEndlessRewardBundle();
}
