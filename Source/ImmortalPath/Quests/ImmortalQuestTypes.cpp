// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalQuestTypes.h"

#include "../Items/ImmortalMaterialTypes.h"

#include "Misc/DateTime.h"
#include "Misc/Timespan.h"

namespace
{
	int32 IncrementRevision(const int32 Value)
	{
		return Value >= MAX_int32 ? MAX_int32 : FMath::Max(Value, 0) + 1;
	}

	int64 AddSaturated(const int64 Value, const int64 Amount)
	{
		const int64 SafeValue = FMath::Max<int64>(Value, 0);
		if (Amount <= 0) return SafeValue;
		return SafeValue > MAX_int64 - Amount ? MAX_int64 : SafeValue + Amount;
	}

	FImmortalQuestReward Reward(
		const int32 Stones,
		const int32 Insight = 0,
		const TCHAR* Material = TEXT(""),
		const int32 Quantity = 0)
	{
		FImmortalQuestReward Result;
		Result.SpiritStones = FMath::Max(Stones, 0);
		Result.TechniqueInsight = FMath::Max(Insight, 0);
		Result.MaterialId = Material && FCString::Strlen(Material) > 0
			? FName(Material) : NAME_None;
		Result.MaterialQuantity = FMath::Max(Quantity, 0);
		return Result;
	}

	FImmortalQuestDefinition Quest(
		const TCHAR* Id,
		const EImmortalQuestCategory Category,
		const TCHAR* Name,
		const TCHAR* Description,
		const EImmortalQuestMetric Metric,
		const int64 Target,
		const FImmortalQuestReward& QuestReward,
		const TCHAR* Prerequisite = TEXT(""))
	{
		FImmortalQuestDefinition Result;
		Result.QuestId = Id;
		Result.Category = Category;
		Result.DisplayName = FText::FromString(Name);
		Result.Description = FText::FromString(Description);
		Result.Metric = Metric;
		Result.TargetAmount = FMath::Max<int64>(Target, 1);
		Result.PrerequisiteQuestId = Prerequisite && FCString::Strlen(Prerequisite) > 0
			? FName(Prerequisite) : NAME_None;
		Result.Reward = QuestReward;
		return Result;
	}

	const TArray<FImmortalQuestDefinition>& Catalog()
	{
		static const TArray<FImmortalQuestDefinition> Definitions =
		{
			Quest(TEXT("Main_FirstBlood"), EImmortalQuestCategory::Main,
				TEXT("初入青云"), TEXT("击败第一只妖物"),
				EImmortalQuestMetric::MonsterKills, 1, Reward(100, 0, TEXT("SpiritGrass"), 3)),
			Quest(TEXT("Main_TrialPath"), EImmortalQuestCategory::Main,
				TEXT("历练之路"), TEXT("累计通过五个关卡"),
				EImmortalQuestMetric::StageClears, 5, Reward(180, 0, TEXT("Ore"), 3), TEXT("Main_FirstBlood")),
			Quest(TEXT("Main_FirstGuardian"), EImmortalQuestCategory::Main,
				TEXT("斩破守关"), TEXT("击败第一名守关首领"),
				EImmortalQuestMetric::BossKills, 1, Reward(260, 1), TEXT("Main_TrialPath")),
			Quest(TEXT("Main_TreasureHunter"), EImmortalQuestCategory::Main,
				TEXT("储物戒初成"), TEXT("累计拾取十件装备"),
				EImmortalQuestMetric::EquipmentPickups, 10, Reward(320, 0, TEXT("SpiritIron"), 2), TEXT("Main_FirstGuardian")),
			Quest(TEXT("Main_Breakthrough"), EImmortalQuestCategory::Main,
				TEXT("问道破境"), TEXT("完成一次修炼突破"),
				EImmortalQuestMetric::CultivationBreakthroughs, 1, Reward(400, 1), TEXT("Main_TreasureHunter")),
			Quest(TEXT("Main_MapCompletion"), EImmortalQuestCategory::Main,
				TEXT("一方圆满"), TEXT("完整通关一张历练地图"),
				EImmortalQuestMetric::MapCompletions, 1, Reward(800, 2, TEXT("ArtifactFragment"), 2), TEXT("Main_Breakthrough")),
			Quest(TEXT("Main_FirstAscension"), EImmortalQuestCategory::Main,
				TEXT("羽化登仙"), TEXT("完成第一次飞升"),
				EImmortalQuestMetric::Ascensions, 1, Reward(2000, 5, TEXT("ArtifactFragment"), 5), TEXT("Main_MapCompletion")),

			Quest(TEXT("Daily_SlayDemons"), EImmortalQuestCategory::Daily,
				TEXT("每日除妖"), TEXT("今日击败二十只妖物"),
				EImmortalQuestMetric::MonsterKills, 20, Reward(120, 0, TEXT("SpiritGrass"), 2)),
			Quest(TEXT("Daily_ClearStages"), EImmortalQuestCategory::Daily,
				TEXT("每日历练"), TEXT("今日通过三个关卡"),
				EImmortalQuestMetric::StageClears, 3, Reward(160, 0, TEXT("Ore"), 2)),
			Quest(TEXT("Daily_DefeatBoss"), EImmortalQuestCategory::Daily,
				TEXT("每日破关"), TEXT("今日击败一名守关首领"),
				EImmortalQuestMetric::BossKills, 1, Reward(220, 1)),
			Quest(TEXT("Daily_FindEquipment"), EImmortalQuestCategory::Daily,
				TEXT("每日寻宝"), TEXT("今日拾取五件装备"),
				EImmortalQuestMetric::EquipmentPickups, 5, Reward(140, 0, TEXT("SpiritIron"), 1)),
			Quest(TEXT("Daily_CollectStones"), EImmortalQuestCategory::Daily,
				TEXT("每日聚财"), TEXT("今日拾取五百枚实体灵石"),
				EImmortalQuestMetric::SpiritStonesCollected, 500, Reward(100, 0, TEXT("SpiritGrass"), 1)),

			Quest(TEXT("Achievement_Kills100"), EImmortalQuestCategory::Achievement,
				TEXT("百妖斩"), TEXT("累计击败一百只妖物"),
				EImmortalQuestMetric::MonsterKills, 100, Reward(500, 1)),
			Quest(TEXT("Achievement_Kills1000"), EImmortalQuestCategory::Achievement,
				TEXT("千妖伏诛"), TEXT("累计击败一千只妖物"),
				EImmortalQuestMetric::MonsterKills, 1000, Reward(1500, 3, TEXT("DemonCore"), 5)),
			Quest(TEXT("Achievement_Stages100"), EImmortalQuestCategory::Achievement,
				TEXT("百关行者"), TEXT("累计通过一百个关卡"),
				EImmortalQuestMetric::StageClears, 100, Reward(1000, 2)),
			Quest(TEXT("Achievement_Boss25"), EImmortalQuestCategory::Achievement,
				TEXT("妖王克星"), TEXT("累计击败二十五名守关首领"),
				EImmortalQuestMetric::BossKills, 25, Reward(1200, 2, TEXT("ArtifactFragment"), 3)),
			Quest(TEXT("Achievement_Equipment100"), EImmortalQuestCategory::Achievement,
				TEXT("百宝盈戒"), TEXT("累计拾取一百件装备"),
				EImmortalQuestMetric::EquipmentPickups, 100, Reward(900, 2, TEXT("SpiritIron"), 5)),
			Quest(TEXT("Achievement_AllMaps"), EImmortalQuestCategory::Achievement,
				TEXT("踏遍八荒"), TEXT("累计完整通关八张地图"),
				EImmortalQuestMetric::MapCompletions, 8, Reward(3000, 5, TEXT("ArtifactFragment"), 8)),
			Quest(TEXT("Achievement_Ascension"), EImmortalQuestCategory::Achievement,
				TEXT("羽化初成"), TEXT("完成第一次飞升"),
				EImmortalQuestMetric::Ascensions, 1, Reward(2500, 5, TEXT("DemonCore"), 5))
		};
		return Definitions;
	}

	const FImmortalQuestDefinition* FindDefinition(const FName QuestId)
	{
		return Catalog().FindByPredicate([QuestId](const FImmortalQuestDefinition& Definition)
		{
			return Definition.QuestId == QuestId;
		});
	}

	TArray<FName>& ClaimedIds(FImmortalQuestState& State, const EImmortalQuestCategory Category)
	{
		switch (Category)
		{
		case EImmortalQuestCategory::Daily: return State.ClaimedDailyQuestIds;
		case EImmortalQuestCategory::Achievement: return State.ClaimedAchievementIds;
		default: return State.ClaimedMainQuestIds;
		}
	}

	const TArray<FName>& ClaimedIds(const FImmortalQuestState& State, const EImmortalQuestCategory Category)
	{
		switch (Category)
		{
		case EImmortalQuestCategory::Daily: return State.ClaimedDailyQuestIds;
		case EImmortalQuestCategory::Achievement: return State.ClaimedAchievementIds;
		default: return State.ClaimedMainQuestIds;
		}
	}

	bool NormalizeClaimedIds(TArray<FName>& Ids, const EImmortalQuestCategory Category)
	{
		TSet<FName> Seen;
		TArray<FName> Normalized;
		for (const FName Id : Ids)
		{
			const FImmortalQuestDefinition* Definition = FindDefinition(Id);
			if (Definition && Definition->Category == Category && !Seen.Contains(Id))
			{
				Seen.Add(Id);
				Normalized.Add(Id);
			}
		}
		if (Normalized == Ids) return false;
		Ids = MoveTemp(Normalized);
		return true;
	}
}

int64 FImmortalQuestCounters::GetValue(const EImmortalQuestMetric Metric) const
{
	switch (Metric)
	{
	case EImmortalQuestMetric::MonsterKills: return MonsterKills;
	case EImmortalQuestMetric::StageClears: return StageClears;
	case EImmortalQuestMetric::BossKills: return BossKills;
	case EImmortalQuestMetric::EquipmentPickups: return EquipmentPickups;
	case EImmortalQuestMetric::SpiritStonesCollected: return SpiritStonesCollected;
	case EImmortalQuestMetric::CultivationBreakthroughs: return CultivationBreakthroughs;
	case EImmortalQuestMetric::AlchemyCrafts: return AlchemyCrafts;
	case EImmortalQuestMetric::CraftingActions: return CraftingActions;
	case EImmortalQuestMetric::SectTasksClaimed: return SectTasksClaimed;
	case EImmortalQuestMetric::MapCompletions: return MapCompletions;
	case EImmortalQuestMetric::Ascensions: return Ascensions;
	default: return 0;
	}
}

bool FImmortalQuestCounters::AddValue(const EImmortalQuestMetric Metric, const int64 Amount)
{
	if (Amount <= 0) return false;
	int64* Value = nullptr;
	switch (Metric)
	{
	case EImmortalQuestMetric::MonsterKills: Value = &MonsterKills; break;
	case EImmortalQuestMetric::StageClears: Value = &StageClears; break;
	case EImmortalQuestMetric::BossKills: Value = &BossKills; break;
	case EImmortalQuestMetric::EquipmentPickups: Value = &EquipmentPickups; break;
	case EImmortalQuestMetric::SpiritStonesCollected: Value = &SpiritStonesCollected; break;
	case EImmortalQuestMetric::CultivationBreakthroughs: Value = &CultivationBreakthroughs; break;
	case EImmortalQuestMetric::AlchemyCrafts: Value = &AlchemyCrafts; break;
	case EImmortalQuestMetric::CraftingActions: Value = &CraftingActions; break;
	case EImmortalQuestMetric::SectTasksClaimed: Value = &SectTasksClaimed; break;
	case EImmortalQuestMetric::MapCompletions: Value = &MapCompletions; break;
	case EImmortalQuestMetric::Ascensions: Value = &Ascensions; break;
	default: break;
	}
	if (!Value) return false;
	const int64 Previous = *Value;
	*Value = AddSaturated(*Value, Amount);
	return *Value != Previous;
}

bool FImmortalQuestCounters::Normalize()
{
	bool bChanged = false;
	auto Clamp = [&bChanged](int64& Value)
	{
		if (Value < 0)
		{
			Value = 0;
			bChanged = true;
		}
	};
	Clamp(MonsterKills); Clamp(StageClears); Clamp(BossKills);
	Clamp(EquipmentPickups); Clamp(SpiritStonesCollected);
	Clamp(CultivationBreakthroughs); Clamp(AlchemyCrafts);
	Clamp(CraftingActions); Clamp(SectTasksClaimed);
	Clamp(MapCompletions); Clamp(Ascensions);
	return bChanged;
}

TArray<FImmortalQuestDefinition> UImmortalQuestLibrary::GetQuestDefinitions(
	const EImmortalQuestCategory Category)
{
	TArray<FImmortalQuestDefinition> Result;
	for (const FImmortalQuestDefinition& Definition : Catalog())
	{
		if (Definition.Category == Category) Result.Add(Definition);
	}
	return Result;
}

bool UImmortalQuestLibrary::GetQuestDefinition(
	const FName QuestId,
	FImmortalQuestDefinition& OutDefinition)
{
	if (const FImmortalQuestDefinition* Definition = FindDefinition(QuestId))
	{
		OutDefinition = *Definition;
		return true;
	}
	OutDefinition = FImmortalQuestDefinition();
	return false;
}

int32 UImmortalQuestLibrary::GetDayKeyFromUtcTicks(
	const int64 UtcTicks,
	const int32 UtcOffsetMinutes)
{
	if (UtcTicks <= 0) return 0;
	const FDateTime Local = FDateTime(UtcTicks)
		+ FTimespan::FromMinutes(FMath::Clamp(UtcOffsetMinutes, -720, 840));
	return Local.GetYear() * 10000 + Local.GetMonth() * 100 + Local.GetDay();
}

FText UImmortalQuestLibrary::GetCategoryText(const EImmortalQuestCategory Category)
{
	switch (Category)
	{
	case EImmortalQuestCategory::Daily: return FText::FromString(TEXT("每日任务"));
	case EImmortalQuestCategory::Achievement: return FText::FromString(TEXT("成就任务"));
	default: return FText::FromString(TEXT("主线任务"));
	}
}

FText UImmortalQuestLibrary::GetMetricText(const EImmortalQuestMetric Metric)
{
	switch (Metric)
	{
	case EImmortalQuestMetric::MonsterKills: return FText::FromString(TEXT("击败妖物"));
	case EImmortalQuestMetric::StageClears: return FText::FromString(TEXT("通过关卡"));
	case EImmortalQuestMetric::BossKills: return FText::FromString(TEXT("击败首领"));
	case EImmortalQuestMetric::EquipmentPickups: return FText::FromString(TEXT("拾取装备"));
	case EImmortalQuestMetric::SpiritStonesCollected: return FText::FromString(TEXT("拾取灵石"));
	case EImmortalQuestMetric::CultivationBreakthroughs: return FText::FromString(TEXT("修炼突破"));
	case EImmortalQuestMetric::AlchemyCrafts: return FText::FromString(TEXT("完成炼丹"));
	case EImmortalQuestMetric::CraftingActions: return FText::FromString(TEXT("完成炼器"));
	case EImmortalQuestMetric::SectTasksClaimed: return FText::FromString(TEXT("宗门任务"));
	case EImmortalQuestMetric::MapCompletions: return FText::FromString(TEXT("通关地图"));
	case EImmortalQuestMetric::Ascensions: return FText::FromString(TEXT("完成飞升"));
	default: return FText::GetEmpty();
	}
}

FText UImmortalQuestLibrary::FormatReward(const FImmortalQuestReward& RewardValue)
{
	TArray<FString> Parts;
	if (RewardValue.SpiritStones > 0)
	{
		Parts.Add(FString::Printf(TEXT("灵石 %d"), RewardValue.SpiritStones));
	}
	if (RewardValue.TechniqueInsight > 0)
	{
		Parts.Add(FString::Printf(TEXT("悟道点 %d"), RewardValue.TechniqueInsight));
	}
	if (!RewardValue.MaterialId.IsNone() && RewardValue.MaterialQuantity > 0)
	{
		FImmortalMaterialDefinition MaterialDefinition;
		const FText MaterialName =
			UImmortalMaterialLibrary::GetMaterialDefinition(
				RewardValue.MaterialId, MaterialDefinition)
			? MaterialDefinition.DisplayName
			: FText::FromName(RewardValue.MaterialId);
		Parts.Add(FString::Printf(TEXT("%s ×%d"),
			*MaterialName.ToString(), RewardValue.MaterialQuantity));
	}
	return FText::FromString(FString::Join(Parts, TEXT(" · ")));
}

FImmortalQuestState UImmortalQuestLibrary::CreateDefaultState(
	const int64 InitialUtcTicks,
	const int32 UtcOffsetMinutes)
{
	const int64 SafeTicks = FMath::Max<int64>(InitialUtcTicks, 0);
	FImmortalQuestState Result;
	Result.bInitialized = true;
	Result.DailyDayKey = GetDayKeyFromUtcTicks(SafeTicks, UtcOffsetMinutes);
	Result.LastObservedUtcTicks = SafeTicks;
	Result.Revision = 1;
	return Result;
}

bool UImmortalQuestLibrary::NormalizeState(
	FImmortalQuestState& State,
	const int64 CurrentUtcTicks,
	const int32 UtcOffsetMinutes)
{
	if (!State.bInitialized)
	{
		State = CreateDefaultState(CurrentUtcTicks, UtcOffsetMinutes);
		return true;
	}
	bool bChanged = false;
	bChanged |= State.LifetimeCounters.Normalize();
	bChanged |= State.DailyCounters.Normalize();
	bChanged |= NormalizeClaimedIds(State.ClaimedMainQuestIds, EImmortalQuestCategory::Main);
	bChanged |= NormalizeClaimedIds(State.ClaimedDailyQuestIds, EImmortalQuestCategory::Daily);
	bChanged |= NormalizeClaimedIds(State.ClaimedAchievementIds, EImmortalQuestCategory::Achievement);
	if (State.TotalClaims < 0)
	{
		State.TotalClaims = 0;
		bChanged = true;
	}
	if (State.LastObservedUtcTicks < 0)
	{
		State.LastObservedUtcTicks = 0;
		bChanged = true;
	}
	const int32 CurrentDayKey = GetDayKeyFromUtcTicks(CurrentUtcTicks, UtcOffsetMinutes);
	if (State.DailyDayKey <= 0 && CurrentDayKey > 0)
	{
		State.DailyDayKey = CurrentDayKey;
		bChanged = true;
	}
	if (State.Revision < 1)
	{
		State.Revision = 1;
		bChanged = true;
	}
	if (bChanged) State.Revision = IncrementRevision(State.Revision);
	return bChanged;
}

FImmortalQuestDailyRefreshResult UImmortalQuestLibrary::EnsureDailyState(
	FImmortalQuestState& State,
	const int64 CurrentUtcTicks,
	const int32 UtcOffsetMinutes)
{
	FImmortalQuestDailyRefreshResult Result;
	Result.DayKey = GetDayKeyFromUtcTicks(CurrentUtcTicks, UtcOffsetMinutes);
	if (CurrentUtcTicks <= 0 || Result.DayKey <= 0)
	{
		Result.Message = FText::FromString(TEXT("当前时间无效，未刷新每日任务"));
		return Result;
	}
	if (!State.bInitialized)
	{
		State = CreateDefaultState(CurrentUtcTicks, UtcOffsetMinutes);
		Result.bSucceeded = true;
		Result.bStateChanged = true;
		Result.Message = FText::FromString(TEXT("每日任务已初始化"));
		return Result;
	}
	NormalizeState(State, CurrentUtcTicks, UtcOffsetMinutes);
	if (State.LastObservedUtcTicks > 0 && CurrentUtcTicks < State.LastObservedUtcTicks)
	{
		Result.bSucceeded = true;
		Result.bClockRollbackDetected = true;
		Result.DayKey = State.DailyDayKey;
		Result.Message = FText::FromString(TEXT("检测到系统时间回拨；每日任务不会提前重置"));
		return Result;
	}
	if (Result.DayKey < State.DailyDayKey)
	{
		Result.bSucceeded = true;
		Result.bClockRollbackDetected = true;
		Result.DayKey = State.DailyDayKey;
		Result.Message = FText::FromString(TEXT("日期早于已记录日期；每日任务保持不变"));
		return Result;
	}
	if (Result.DayKey > State.DailyDayKey)
	{
		State.DailyDayKey = Result.DayKey;
		State.DailyCounters = FImmortalQuestCounters();
		State.ClaimedDailyQuestIds.Reset();
		State.Revision = IncrementRevision(State.Revision);
		Result.bStateChanged = true;
	}
	State.LastObservedUtcTicks = FMath::Max(State.LastObservedUtcTicks, CurrentUtcTicks);
	Result.bSucceeded = true;
	Result.Message = Result.bStateChanged
		? FText::FromString(TEXT("每日任务已刷新"))
		: FText::FromString(TEXT("每日任务日期有效"));
	return Result;
}

FImmortalQuestRecordResult UImmortalQuestLibrary::RecordProgress(
	FImmortalQuestState& State,
	const EImmortalQuestMetric Metric,
	const int64 Amount,
	const int64 CurrentUtcTicks,
	const int32 UtcOffsetMinutes)
{
	FImmortalQuestRecordResult Result;
	Result.Metric = Metric;
	if (Amount <= 0)
	{
		Result.Message = FText::FromString(TEXT("任务进度增量必须大于零"));
		return Result;
	}
	const FImmortalQuestDailyRefreshResult Refresh = EnsureDailyState(
		State, CurrentUtcTicks, UtcOffsetMinutes);
	if (!Refresh.bSucceeded)
	{
		Result.Message = Refresh.Message;
		return Result;
	}
	Result.bClockRollbackDetected = Refresh.bClockRollbackDetected;
	const bool bLifetimeChanged = State.LifetimeCounters.AddValue(Metric, Amount);
	const bool bDailyChanged = State.DailyCounters.AddValue(Metric, Amount);
	Result.bStateChanged = bLifetimeChanged || bDailyChanged;
	if (Result.bStateChanged)
	{
		State.Revision = IncrementRevision(State.Revision);
		Result.AmountApplied = Amount;
	}
	Result.bSucceeded = true;
	Result.Message = FText::FromString(TEXT("任务进度已记录"));
	return Result;
}

FImmortalQuestProgressView UImmortalQuestLibrary::GetProgress(
	const FImmortalQuestState& State,
	const FName QuestId)
{
	FImmortalQuestProgressView Result;
	Result.QuestId = QuestId;
	const FImmortalQuestDefinition* Definition = FindDefinition(QuestId);
	if (!Definition) return Result;
	Result.bKnownQuest = true;
	Result.Target = FMath::Max<int64>(Definition->TargetAmount, 1);
	const FImmortalQuestCounters& Counters = Definition->Category == EImmortalQuestCategory::Daily
		? State.DailyCounters : State.LifetimeCounters;
	Result.Progress = FMath::Clamp<int64>(Counters.GetValue(Definition->Metric), 0, Result.Target);
	Result.bClaimed = ClaimedIds(State, Definition->Category).Contains(QuestId);
	Result.bUnlocked = Definition->Category != EImmortalQuestCategory::Main
		|| Definition->PrerequisiteQuestId.IsNone()
		|| State.ClaimedMainQuestIds.Contains(Definition->PrerequisiteQuestId);
	Result.bCompleted = Result.Progress >= Result.Target;
	Result.bCanClaim = Result.bUnlocked && Result.bCompleted && !Result.bClaimed;
	return Result;
}

FImmortalQuestClaimResult UImmortalQuestLibrary::EvaluateClaim(
	const FImmortalQuestState& State,
	const FName QuestId,
	const int64 CurrentUtcTicks,
	const int32 UtcOffsetMinutes)
{
	FImmortalQuestClaimResult Result;
	Result.QuestId = QuestId;
	FImmortalQuestState Candidate = State;
	const FImmortalQuestDailyRefreshResult Refresh = EnsureDailyState(
		Candidate, CurrentUtcTicks, UtcOffsetMinutes);
	Result.bClockRollbackDetected = Refresh.bClockRollbackDetected;
	const FImmortalQuestDefinition* Definition = FindDefinition(QuestId);
	if (!Definition)
	{
		Result.Message = FText::FromString(TEXT("未找到任务"));
		return Result;
	}
	Result.bKnownQuest = true;
	Result.Reward = Definition->Reward;
	const FImmortalQuestProgressView Progress = GetProgress(Candidate, QuestId);
	Result.bUnlocked = Progress.bUnlocked;
	Result.bCompleted = Progress.bCompleted;
	Result.bAlreadyClaimed = Progress.bClaimed;
	Result.bCanClaim = Progress.bCanClaim;
	if (Progress.bCanClaim)
	{
		Result.Message = FText::FromString(TEXT("任务已完成，可以领取奖励"));
	}
	else if (Progress.bClaimed)
	{
		Result.Message = FText::FromString(TEXT("任务奖励已经领取"));
	}
	else if (!Progress.bUnlocked)
	{
		Result.Message = FText::FromString(TEXT("请先领取前置主线任务"));
	}
	else
	{
		Result.Message = FText::FromString(TEXT("任务进度尚未完成"));
	}
	return Result;
}

FImmortalQuestClaimResult UImmortalQuestLibrary::TryClaim(
	FImmortalQuestState& State,
	const FName QuestId,
	const int64 CurrentUtcTicks,
	const int32 UtcOffsetMinutes)
{
	const FImmortalQuestDailyRefreshResult Refresh = EnsureDailyState(
		State, CurrentUtcTicks, UtcOffsetMinutes);
	FImmortalQuestClaimResult Result = EvaluateClaim(
		State, QuestId, CurrentUtcTicks, UtcOffsetMinutes);
	Result.bClockRollbackDetected = Result.bClockRollbackDetected
		|| Refresh.bClockRollbackDetected;
	if (!Result.bCanClaim) return Result;
	const FImmortalQuestDefinition* Definition = FindDefinition(QuestId);
	if (!Definition) return Result;
	TArray<FName>& Claims = ClaimedIds(State, Definition->Category);
	Claims.AddUnique(QuestId);
	State.TotalClaims = AddSaturated(State.TotalClaims, 1);
	State.Revision = IncrementRevision(State.Revision);
	Result.bSucceeded = true;
	Result.bCanClaim = false;
	Result.Message = FText::FromString(FString::Printf(
		TEXT("已领取：%s"), *FormatReward(Result.Reward).ToString()));
	return Result;
}
