// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalAscensionTypes.h"

namespace
{
	void AdvanceRevision(FImmortalAscensionState& State)
	{
		if (State.Revision < MAX_int32)
		{
			++State.Revision;
		}
	}

	int32* FindMutablePathRank(
		FImmortalAscensionState& State,
		const EImmortalAscensionPath Path)
	{
		switch (Path)
		{
		case EImmortalAscensionPath::Battle:
			return &State.BattlePathRank;
		case EImmortalAscensionPath::Enlightenment:
			return &State.EnlightenmentPathRank;
		case EImmortalAscensionPath::Fortune:
			return &State.FortunePathRank;
		default:
			return nullptr;
		}
	}

	int64 GetAccountedSealCount(const FImmortalAscensionState& State)
	{
		return static_cast<int64>(FMath::Max(State.ImmortalSeals, 0))
			+ UImmortalAscensionLibrary::CalculatePathCumulativeCost(
				State.BattlePathRank)
			+ UImmortalAscensionLibrary::CalculatePathCumulativeCost(
				State.EnlightenmentPathRank)
			+ UImmortalAscensionLibrary::CalculatePathCumulativeCost(
				State.FortunePathRank);
	}

	bool HaveSameMapLegacyRecords(
		const TArray<FImmortalAscensionMapLegacy>& Left,
		const TArray<FImmortalAscensionMapLegacy>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index].MapId != Right[Index].MapId
				|| Left[Index].HighestStage
					!= Right[Index].HighestStage
				|| Left[Index].TimesCompleted
					!= Right[Index].TimesCompleted)
			{
				return false;
			}
		}
		return true;
	}

	void PopulateDefaultMapLegacyRecords(
		TArray<FImmortalAscensionMapLegacy>& Records)
	{
		Records.Reset();
		for (const FName MapId :
			UImmortalMapLibrary::GetKnownMapIds())
		{
			FImmortalAscensionMapLegacy& Record =
				Records.AddDefaulted_GetRef();
			Record.MapId = MapId;
		}
	}
}

FImmortalAscensionState UImmortalAscensionLibrary::CreateDefaultState()
{
	FImmortalAscensionState Result;
	Result.bInitialized = true;
	Result.Revision = 1;
	PopulateDefaultMapLegacyRecords(
		Result.LifetimeMapRecords);
	return Result;
}

bool UImmortalAscensionLibrary::NormalizeState(
	FImmortalAscensionState& State)
{
	const FImmortalAscensionState Previous = State;
	State.bInitialized = true;
	State.Revision = FMath::Max(State.Revision, 0);
	State.AscensionCount = FMath::Clamp(
		State.AscensionCount, 0, MaximumAscensionCount);
	State.ImmortalSeals = FMath::Max(State.ImmortalSeals, 0);
	State.BattlePathRank = FMath::Clamp(
		State.BattlePathRank, 0, MaximumPathRank);
	State.EnlightenmentPathRank = FMath::Clamp(
		State.EnlightenmentPathRank, 0, MaximumPathRank);
	State.FortunePathRank = FMath::Clamp(
		State.FortunePathRank, 0, MaximumPathRank);
	State.LastAscensionUtcTicks =
		FMath::Max<int64>(State.LastAscensionUtcTicks, 0);
	State.TotalImmortalSealsEarned = FMath::Max<int64>(
		FMath::Max<int64>(State.TotalImmortalSealsEarned, 0),
		GetAccountedSealCount(State));

	TMap<FName, FImmortalAscensionMapLegacy> BestMapRecords;
	for (const FImmortalAscensionMapLegacy& Candidate :
		State.LifetimeMapRecords)
	{
		FImmortalMapDefinition Definition;
		if (!UImmortalMapLibrary::GetMapDefinition(
			Candidate.MapId, Definition))
		{
			continue;
		}
		FImmortalAscensionMapLegacy Normalized = Candidate;
		Normalized.HighestStage = FMath::Clamp(
			Normalized.HighestStage,
			1,
			FMath::Clamp(Definition.MaximumStage, 1, 999));
		Normalized.TimesCompleted = FMath::Clamp(
			Normalized.TimesCompleted,
			0,
			MaximumAscensionCount);
		if (FImmortalAscensionMapLegacy* Existing =
			BestMapRecords.Find(Normalized.MapId))
		{
			Existing->HighestStage = FMath::Max(
				Existing->HighestStage,
				Normalized.HighestStage);
			Existing->TimesCompleted = FMath::Max(
				Existing->TimesCompleted,
				Normalized.TimesCompleted);
		}
		else
		{
			BestMapRecords.Add(
				Normalized.MapId, Normalized);
		}
	}
	State.LifetimeMapRecords.Reset();
	for (const FName MapId :
		UImmortalMapLibrary::GetKnownMapIds())
	{
		if (const FImmortalAscensionMapLegacy* Existing =
			BestMapRecords.Find(MapId))
		{
			State.LifetimeMapRecords.Add(*Existing);
		}
		else
		{
			FImmortalAscensionMapLegacy& Added =
				State.LifetimeMapRecords.AddDefaulted_GetRef();
			Added.MapId = MapId;
		}
	}

	const bool bChanged =
		Previous.bInitialized != State.bInitialized
		|| Previous.Revision != State.Revision
		|| Previous.AscensionCount != State.AscensionCount
		|| Previous.ImmortalSeals != State.ImmortalSeals
		|| Previous.TotalImmortalSealsEarned
			!= State.TotalImmortalSealsEarned
		|| Previous.BattlePathRank != State.BattlePathRank
		|| Previous.EnlightenmentPathRank
			!= State.EnlightenmentPathRank
		|| Previous.FortunePathRank != State.FortunePathRank
		|| Previous.LastAscensionUtcTicks
			!= State.LastAscensionUtcTicks
		|| !HaveSameMapLegacyRecords(
			Previous.LifetimeMapRecords,
			State.LifetimeMapRecords);
	if (bChanged)
	{
		AdvanceRevision(State);
	}
	return bChanged;
}

int32 UImmortalAscensionLibrary::CalculateAscensionReward(
	const int32 CompletedAscensions)
{
	const int32 SafeCount = FMath::Clamp(
		CompletedAscensions, 0, MaximumAscensionCount);
	// The reward grows every five cycles but remains bounded for a damaged save.
	return FMath::Clamp(3 + SafeCount / 5, 3, 12);
}

int32 UImmortalAscensionLibrary::GetPathRank(
	const FImmortalAscensionState& State,
	const EImmortalAscensionPath Path)
{
	switch (Path)
	{
	case EImmortalAscensionPath::Battle:
		return FMath::Clamp(
			State.BattlePathRank, 0, MaximumPathRank);
	case EImmortalAscensionPath::Enlightenment:
		return FMath::Clamp(
			State.EnlightenmentPathRank, 0, MaximumPathRank);
	case EImmortalAscensionPath::Fortune:
		return FMath::Clamp(
			State.FortunePathRank, 0, MaximumPathRank);
	default:
		return 0;
	}
}

int32 UImmortalAscensionLibrary::CalculatePathUpgradeCost(
	const int32 CurrentRank)
{
	const int32 SafeRank = FMath::Clamp(
		CurrentRank, 0, MaximumPathRank);
	return SafeRank >= MaximumPathRank
		? 0
		: 1 + 4 * SafeRank;
}

int64 UImmortalAscensionLibrary::CalculatePathCumulativeCost(
	const int32 Rank)
{
	const int64 SafeRank = static_cast<int64>(
		FMath::Clamp(Rank, 0, MaximumPathRank));
	// Sum for r=0..Rank-1 of (1 + 4r) = 2*Rank^2 - Rank.
	return 2 * SafeRank * SafeRank - SafeRank;
}

FText UImmortalAscensionLibrary::GetPathDisplayName(
	const EImmortalAscensionPath Path)
{
	switch (Path)
	{
	case EImmortalAscensionPath::Battle:
		return FText::FromString(TEXT("战道"));
	case EImmortalAscensionPath::Enlightenment:
		return FText::FromString(TEXT("悟道"));
	case EImmortalAscensionPath::Fortune:
		return FText::FromString(TEXT("福缘"));
	default:
		return FText::FromString(TEXT("未知仙途"));
	}
}

FText UImmortalAscensionLibrary::GetPathDescription(
	const EImmortalAscensionPath Path)
{
	switch (Path)
	{
	case EImmortalAscensionPath::Battle:
		return FText::FromString(TEXT("每阶使全部攻击伤害提高 4%"));
	case EImmortalAscensionPath::Enlightenment:
		return FText::FromString(TEXT("每阶使在线与离线修炼速度提高 6%"));
	case EImmortalAscensionPath::Fortune:
		return FText::FromString(TEXT("每阶使普通装备掉落倍率提高 3%"));
	default:
		return FText::GetEmpty();
	}
}

float UImmortalAscensionLibrary::CalculateBattleDamageMultiplier(
	const FImmortalAscensionState& State)
{
	return 1.0f + 0.04f * GetPathRank(
		State, EImmortalAscensionPath::Battle);
}

float UImmortalAscensionLibrary::CalculateCultivationRateMultiplier(
	const FImmortalAscensionState& State)
{
	return 1.0f + 0.06f * GetPathRank(
		State, EImmortalAscensionPath::Enlightenment);
}

float UImmortalAscensionLibrary::CalculateEquipmentDropMultiplier(
	const FImmortalAscensionState& State)
{
	return 1.0f + 0.03f * GetPathRank(
		State, EImmortalAscensionPath::Fortune);
}

FImmortalMapSystemState
UImmortalAscensionLibrary::CreateNewCycleMapState()
{
	return UImmortalMapLibrary::CreateMigratedState(
		1, 0, false);
}

bool UImmortalAscensionLibrary::GetLifetimeMapRecord(
	const FImmortalAscensionState& State,
	const FName MapId,
	FImmortalAscensionMapLegacy& OutRecord)
{
	FImmortalMapDefinition Definition;
	if (!UImmortalMapLibrary::GetMapDefinition(
		MapId, Definition))
	{
		OutRecord = FImmortalAscensionMapLegacy();
		return false;
	}
	if (const FImmortalAscensionMapLegacy* Found =
		State.LifetimeMapRecords.FindByPredicate(
			[MapId](
				const FImmortalAscensionMapLegacy& Record)
			{
				return Record.MapId == MapId;
			}))
	{
		OutRecord = *Found;
		OutRecord.HighestStage = FMath::Clamp(
			OutRecord.HighestStage,
			1,
			FMath::Clamp(Definition.MaximumStage, 1, 999));
		OutRecord.TimesCompleted = FMath::Clamp(
			OutRecord.TimesCompleted,
			0,
			MaximumAscensionCount);
		return true;
	}
	OutRecord = FImmortalAscensionMapLegacy();
	OutRecord.MapId = MapId;
	return true;
}

int32 UImmortalAscensionLibrary::GetLifetimeCompletedMapCount(
	const FImmortalAscensionState& State)
{
	int32 CompletedMaps = 0;
	for (const FName MapId :
		UImmortalMapLibrary::GetKnownMapIds())
	{
		FImmortalAscensionMapLegacy Record;
		if (GetLifetimeMapRecord(
			State, MapId, Record)
			&& Record.TimesCompleted > 0)
		{
			++CompletedMaps;
		}
	}
	return CompletedMaps;
}

FImmortalAscensionEligibility
UImmortalAscensionLibrary::EvaluateEligibility(
	const FImmortalAscensionState& State,
	const bool bReachedAscensionRealm,
	const bool bImmortalPalaceCompleted,
	const bool bAtQingyunMountain,
	const bool bWorldBossActive,
	const bool bEndlessDungeonActive)
{
	FImmortalAscensionEligibility Result;
	Result.bReachedAscensionRealm = bReachedAscensionRealm;
	Result.bImmortalPalaceCompleted = bImmortalPalaceCompleted;
	Result.bAtQingyunMountain = bAtQingyunMountain;
	Result.bIndependentEncountersIdle =
		!bWorldBossActive && !bEndlessDungeonActive;
	Result.bBelowAscensionLimit =
		State.AscensionCount < MaximumAscensionCount;
	Result.RewardImmortalSeals = CalculateAscensionReward(
		State.AscensionCount);
	Result.bEligible =
		Result.bReachedAscensionRealm
		&& Result.bImmortalPalaceCompleted
		&& Result.bAtQingyunMountain
		&& Result.bIndependentEncountersIdle
		&& Result.bBelowAscensionLimit;

	if (!Result.bBelowAscensionLimit)
	{
		Result.Message = FText::FromString(TEXT("已达到 999 次飞升上限"));
	}
	else if (!Result.bReachedAscensionRealm)
	{
		Result.Message = FText::FromString(TEXT("需先修至飞升境界"));
	}
	else if (!Result.bImmortalPalaceCompleted)
	{
		Result.Message = FText::FromString(TEXT("需先通关仙宫遗址第 999 关"));
	}
	else if (!Result.bAtQingyunMountain)
	{
		Result.Message = FText::FromString(TEXT("请返回青云山举行飞升仪式"));
	}
	else if (!Result.bIndependentEncountersIdle)
	{
		Result.Message = FText::FromString(TEXT("世界妖王或无尽秘境战斗中不可飞升"));
	}
	else
	{
		Result.Message = FText::FromString(FString::Printf(
			TEXT("可飞升：本次获得 %d 枚仙印"),
			Result.RewardImmortalSeals));
	}
	return Result;
}

FImmortalAscensionOperationResult
UImmortalAscensionLibrary::GrantAscension(
	FImmortalAscensionState& State,
	const FImmortalMapSystemState& CompletedCycleMapState,
	const int64 CurrentUtcTicks)
{
	NormalizeState(State);
	FImmortalAscensionOperationResult Result;
	Result.AscensionCount = State.AscensionCount;
	Result.ImmortalSeals = State.ImmortalSeals;
	if (State.AscensionCount >= MaximumAscensionCount)
	{
		Result.Message = FText::FromString(TEXT("已达到飞升次数上限"));
		return Result;
	}

	FImmortalMapSystemState NormalizedMapState =
		CompletedCycleMapState;
	UImmortalMapLibrary::NormalizeState(
		NormalizedMapState);
	for (const FName MapId :
		UImmortalMapLibrary::GetKnownMapIds())
	{
		FImmortalMapProgress Progress;
		if (!UImmortalMapLibrary::GetMapProgress(
			NormalizedMapState, MapId, Progress))
		{
			continue;
		}
		FImmortalAscensionMapLegacy* Record =
			State.LifetimeMapRecords.FindByPredicate(
				[MapId](
					const FImmortalAscensionMapLegacy& Candidate)
				{
					return Candidate.MapId == MapId;
				});
		if (!Record)
		{
			continue;
		}
		Record->HighestStage = FMath::Max(
			Record->HighestStage,
			Progress.Stage);
		if (Progress.bCompleted)
		{
			Record->TimesCompleted = FMath::Min(
				Record->TimesCompleted + 1,
				MaximumAscensionCount);
		}
	}

	const int32 Reward = CalculateAscensionReward(
		State.AscensionCount);
	++State.AscensionCount;
	State.ImmortalSeals = State.ImmortalSeals > MAX_int32 - Reward
		? MAX_int32
		: State.ImmortalSeals + Reward;
	State.TotalImmortalSealsEarned =
		State.TotalImmortalSealsEarned > MAX_int64 - Reward
			? MAX_int64
			: State.TotalImmortalSealsEarned + Reward;
	State.LastAscensionUtcTicks = FMath::Max<int64>(
		State.LastAscensionUtcTicks,
		FMath::Max<int64>(CurrentUtcTicks, 0));
	AdvanceRevision(State);

	Result.bSucceeded = true;
	Result.ImmortalSealsGranted = Reward;
	Result.AscensionCount = State.AscensionCount;
	Result.ImmortalSeals = State.ImmortalSeals;
	Result.Message = FText::FromString(FString::Printf(
		TEXT("第 %d 次飞升成功，获得 %d 枚仙印；新一轮从青云山第 1 关开始"),
		State.AscensionCount,
		Reward));
	return Result;
}

FImmortalAscensionPathResult
UImmortalAscensionLibrary::InvestPath(
	FImmortalAscensionState& State,
	const EImmortalAscensionPath Path)
{
	NormalizeState(State);
	FImmortalAscensionPathResult Result;
	Result.Path = Path;
	Result.ImmortalSeals = State.ImmortalSeals;
	int32* Rank = FindMutablePathRank(State, Path);
	if (!Rank)
	{
		Result.Message = FText::FromString(TEXT("仙途不存在"));
		return Result;
	}
	Result.PreviousRank = *Rank;
	Result.CurrentRank = *Rank;
	if (*Rank >= MaximumPathRank)
	{
		Result.Message = FText::FromString(TEXT("该仙途已达到 50 阶"));
		return Result;
	}
	const int32 UpgradeCost =
		CalculatePathUpgradeCost(*Rank);
	if (State.ImmortalSeals < UpgradeCost)
	{
		Result.Message = FText::FromString(FString::Printf(
			TEXT("仙印不足：下一阶需要 %d 枚，当前持有 %d 枚"),
			UpgradeCost,
			State.ImmortalSeals));
		return Result;
	}

	State.ImmortalSeals -= UpgradeCost;
	++(*Rank);
	AdvanceRevision(State);
	Result.bSucceeded = true;
	Result.ImmortalSealsSpent = UpgradeCost;
	Result.CurrentRank = *Rank;
	Result.ImmortalSeals = State.ImmortalSeals;
	Result.Message = FText::FromString(FString::Printf(
		TEXT("%s提升至 %d 阶，消耗 %d 枚仙印"),
		*GetPathDisplayName(Path).ToString(),
		*Rank,
		UpgradeCost));
	return Result;
}
