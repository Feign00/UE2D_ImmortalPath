// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalSectTypes.h"

#include "Misc/DateTime.h"
#include "Misc/Timespan.h"

namespace
{
	const FName QingyunSectId(TEXT("QingyunSect"));
	const FName HeavenlySwordSectId(TEXT("HeavenlySwordSect"));
	const FName MyriadDemonValleyId(TEXT("MyriadDemonValley"));
	const FName DemonSectId(TEXT("DemonSect"));

	const FName SlayDemonsTaskId(TEXT("SlayDemons"));
	const FName AdvanceTrialsTaskId(TEXT("AdvanceTrials"));
	const FName DefeatGuardiansTaskId(TEXT("DefeatGuardians"));

	const FName SectSuppliesOfferId(TEXT("SectSupplies"));
	const FName SpiritStoneStipendOfferId(TEXT("SpiritStoneStipend"));
	const FName TechniqueInsightOfferId(TEXT("TechniqueInsight"));
	const FName SectManualOfferId(TEXT("SectManual"));

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

	FImmortalSectDefinition MakeSect(
		const TCHAR* Id,
		const TCHAR* Name,
		const TCHAR* Description,
		const int32 Realm,
		const TCHAR* MapId,
		const int32 Stage,
		const TCHAR* TechniqueId,
		const FLinearColor& Color)
	{
		FImmortalSectDefinition Result;
		Result.SectId = Id;
		Result.DisplayName = FText::FromString(Name);
		Result.Description = FText::FromString(Description);
		Result.RequiredRealmIndex = Realm;
		Result.RequiredMapId = MapId;
		Result.RequiredMapStage = Stage;
		Result.TechniqueRewardId = TechniqueId;
		Result.DisplayColor = Color;
		return Result;
	}

	const TMap<FName, FImmortalSectDefinition>& SectCatalog()
	{
		static const TMap<FName, FImmortalSectDefinition> Catalog =
		{
			{QingyunSectId, MakeSect(TEXT("QingyunSect"), TEXT("青云宗"),
				TEXT("清正平和，以吐纳养气、守正除妖为门规。适合初入仙途的弟子。"),
				0, TEXT("QingyunMountain"), 1, TEXT("BasicBreathing"),
				FLinearColor(0.30f, 0.82f, 1.0f, 1.0f))},
			{HeavenlySwordSectId, MakeSect(TEXT("HeavenlySwordSect"), TEXT("天剑宗"),
				TEXT("以剑问道，重视历练与破关。通过青云山二十五关方可拜入山门。"),
				0, TEXT("QingyunMountain"), 25, TEXT("QingyunSwordArt"),
				FLinearColor(0.78f, 0.88f, 1.0f, 1.0f))},
			{MyriadDemonValleyId, MakeSect(TEXT("MyriadDemonValley"), TEXT("万妖谷"),
				TEXT("兼纳妖族与御兽修士，以万妖精血淬炼真火，筑基后方可入谷。"),
				1, TEXT("MyriadBeastForest"), 1, TEXT("BurningHeavenArt"),
				FLinearColor(0.42f, 0.95f, 0.46f, 1.0f))},
			{DemonSectId, MakeSect(TEXT("DemonSect"), TEXT("魔宗"),
				TEXT("行事不拘正邪，以雷霆淬心。须成就金丹并抵达幽冥谷。"),
				2, TEXT("NetherValley"), 1, TEXT("NineHeavensThunder"),
				FLinearColor(0.82f, 0.38f, 1.0f, 1.0f))}
		};
		return Catalog;
	}

	FImmortalSectTaskDefinition MakeTask(
		const TCHAR* Id,
		const TCHAR* Name,
		const TCHAR* Description,
		const EImmortalSectTaskMetric Metric,
		const int32 Target,
		const int32 Reward)
	{
		FImmortalSectTaskDefinition Result;
		Result.TaskId = Id;
		Result.DisplayName = FText::FromString(Name);
		Result.Description = FText::FromString(Description);
		Result.Metric = Metric;
		Result.TargetAmount = Target;
		Result.ContributionReward = Reward;
		return Result;
	}

	const TArray<FImmortalSectTaskDefinition>& TaskCatalog()
	{
		static const TArray<FImmortalSectTaskDefinition> Tasks =
		{
			MakeTask(TEXT("SlayDemons"), TEXT("除妖卫道"), TEXT("击败二十只妖物"),
				EImmortalSectTaskMetric::MonsterKills, 20, 60),
			MakeTask(TEXT("AdvanceTrials"), TEXT("推进历练"), TEXT("推进三个关卡"),
				EImmortalSectTaskMetric::StageClears, 3, 90),
			MakeTask(TEXT("DefeatGuardians"), TEXT("斩破守关"), TEXT("击败一名守关首领"),
				EImmortalSectTaskMetric::BossKills, 1, 120)
		};
		return Tasks;
	}

	FImmortalSectStoreOfferDefinition MakeOffer(
		const TCHAR* Id,
		const TCHAR* Name,
		const TCHAR* Description,
		const EImmortalSectRewardType RewardType,
		const TCHAR* RewardId,
		const int32 Quantity,
		const int32 Cost,
		const int32 DailyLimit,
		const bool bOneTime,
		const int64 RequiredEarned)
	{
		FImmortalSectStoreOfferDefinition Result;
		Result.OfferId = Id;
		Result.DisplayName = FText::FromString(Name);
		Result.Description = FText::FromString(Description);
		Result.RewardType = RewardType;
		Result.RewardId = RewardId;
		Result.RewardQuantity = Quantity;
		Result.ContributionCost = Cost;
		Result.DailyLimit = DailyLimit;
		Result.bOneTime = bOneTime;
		Result.RequiredLifetimeContribution = RequiredEarned;
		return Result;
	}

	bool FindOffer(const FName SectId, const FName OfferId, FImmortalSectStoreOfferDefinition& OutOffer)
	{
		for (const FImmortalSectStoreOfferDefinition& Offer : UImmortalSectLibrary::GetStoreOfferDefinitions(SectId))
		{
			if (Offer.OfferId == OfferId)
			{
				OutOffer = Offer;
				return true;
			}
		}
		return false;
	}

	const FImmortalSectTaskDefinition* FindTaskDefinition(const FName TaskId)
	{
		return TaskCatalog().FindByPredicate([TaskId](const FImmortalSectTaskDefinition& Definition)
		{
			return Definition.TaskId == TaskId;
		});
	}

	TArray<FImmortalSectTaskProgress> FreshTaskProgress()
	{
		TArray<FImmortalSectTaskProgress> Result;
		for (const FImmortalSectTaskDefinition& Definition : TaskCatalog())
		{
			FImmortalSectTaskProgress& Progress = Result.AddDefaulted_GetRef();
			Progress.TaskId = Definition.TaskId;
		}
		return Result;
	}

	TArray<FImmortalSectOfferProgress> FreshOfferProgress(const FName SectId)
	{
		TArray<FImmortalSectOfferProgress> Result;
		for (const FImmortalSectStoreOfferDefinition& Definition : UImmortalSectLibrary::GetStoreOfferDefinitions(SectId))
		{
			FImmortalSectOfferProgress& Progress = Result.AddDefaulted_GetRef();
			Progress.OfferId = Definition.OfferId;
		}
		return Result;
	}

	FText JoinRequirementText(const FImmortalSectDefinition& Definition)
	{
		FImmortalMapDefinition Map;
		const FString MapName = UImmortalMapLibrary::GetMapDefinition(Definition.RequiredMapId, Map)
			? Map.DisplayName.ToString()
			: Definition.RequiredMapId.ToString();
		return FText::FromString(FString::Printf(TEXT("需要%s，并抵达%s第%d关"),
			*UImmortalMapLibrary::GetRealmRequirementText(Definition.RequiredRealmIndex).ToString(),
			*MapName, Definition.RequiredMapStage));
	}
}

TArray<FName> UImmortalSectLibrary::GetKnownSectIds()
{
	return {QingyunSectId, HeavenlySwordSectId, MyriadDemonValleyId, DemonSectId};
}

bool UImmortalSectLibrary::GetSectDefinition(const FName SectId, FImmortalSectDefinition& OutDefinition)
{
	if (const FImmortalSectDefinition* Found = SectCatalog().Find(SectId))
	{
		OutDefinition = *Found;
		return true;
	}
	OutDefinition = FImmortalSectDefinition();
	return false;
}

TArray<FImmortalSectTaskDefinition> UImmortalSectLibrary::GetDailyTaskDefinitions()
{
	return TaskCatalog();
}

TArray<FImmortalSectStoreOfferDefinition> UImmortalSectLibrary::GetStoreOfferDefinitions(const FName SectId)
{
	FImmortalSectDefinition Sect;
	if (!GetSectDefinition(SectId, Sect)) return {};

	FName MaterialId;
	int32 MaterialQuantity = 1;
	if (SectId == QingyunSectId) { MaterialId = TEXT("SpiritGrass"); MaterialQuantity = 5; }
	else if (SectId == HeavenlySwordSectId) { MaterialId = TEXT("SpiritIron"); MaterialQuantity = 2; }
	else if (SectId == MyriadDemonValleyId) { MaterialId = TEXT("DemonCore"); MaterialQuantity = 3; }
	else { MaterialId = TEXT("ArtifactFragment"); MaterialQuantity = 1; }

	return
	{
		MakeOffer(TEXT("SectSupplies"), TEXT("宗门物资"), TEXT("每日可兑换的宗门专属材料"),
			EImmortalSectRewardType::Material, *MaterialId.ToString(), MaterialQuantity, 40, 5, false, 0),
		MakeOffer(TEXT("SpiritStoneStipend"), TEXT("灵石俸禄"), TEXT("领取二百五十枚灵石"),
			EImmortalSectRewardType::SpiritStones, TEXT("SpiritStones"), 250, 80, 3, false, 60),
		MakeOffer(TEXT("TechniqueInsight"), TEXT("悟道玉简"), TEXT("获得一点功法悟道点"),
			EImmortalSectRewardType::TechniqueInsight, TEXT("TechniqueInsight"), 1, 150, 2, false, 150),
		MakeOffer(TEXT("SectManual"), TEXT("宗门真传"), TEXT("永久领悟本宗传承功法"),
			EImmortalSectRewardType::Technique, *Sect.TechniqueRewardId.ToString(), 1, 500, 0, true, 300)
	};
}

int32 UImmortalSectLibrary::GetDayKeyFromUtcTicks(const int64 UtcTicks, const int32 UtcOffsetMinutes)
{
	if (UtcTicks <= 0) return 0;
	const FDateTime Local = FDateTime(UtcTicks)
		+ FTimespan::FromMinutes(FMath::Clamp(UtcOffsetMinutes, -720, 840));
	return Local.GetYear() * 10000 + Local.GetMonth() * 100 + Local.GetDay();
}

FText UImmortalSectLibrary::GetRankName(const int64 TotalContributionEarned)
{
	if (TotalContributionEarned >= 2500) return FText::FromString(TEXT("宗门长老"));
	if (TotalContributionEarned >= 1000) return FText::FromString(TEXT("真传弟子"));
	if (TotalContributionEarned >= 300) return FText::FromString(TEXT("内门弟子"));
	return FText::FromString(TEXT("外门弟子"));
}

bool UImmortalSectLibrary::GetTaskProgress(
	const FImmortalSectState& State,
	const FName TaskId,
	FImmortalSectTaskProgress& OutProgress)
{
	if (const FImmortalSectTaskProgress* Found = State.DailyTasks.FindByPredicate([TaskId](const FImmortalSectTaskProgress& Value)
	{
		return Value.TaskId == TaskId;
	}))
	{
		OutProgress = *Found;
		return true;
	}
	OutProgress = FImmortalSectTaskProgress();
	return false;
}

bool UImmortalSectLibrary::GetOfferProgress(
	const FImmortalSectState& State,
	const FName OfferId,
	FImmortalSectOfferProgress& OutProgress)
{
	if (const FImmortalSectOfferProgress* Found = State.OfferProgress.FindByPredicate([OfferId](const FImmortalSectOfferProgress& Value)
	{
		return Value.OfferId == OfferId;
	}))
	{
		OutProgress = *Found;
		return true;
	}
	OutProgress = FImmortalSectOfferProgress();
	return false;
}

FImmortalSectState UImmortalSectLibrary::CreateDefaultState(int64 InitialUtcTicks, const int32 UtcOffsetMinutes)
{
	if (InitialUtcTicks <= 0) InitialUtcTicks = FDateTime::UtcNow().GetTicks();
	FImmortalSectState Result;
	Result.bInitialized = true;
	Result.TaskDayKey = GetDayKeyFromUtcTicks(InitialUtcTicks, UtcOffsetMinutes);
	Result.LastObservedUtcTicks = InitialUtcTicks;
	Result.DailyTasks = FreshTaskProgress();
	return Result;
}

bool UImmortalSectLibrary::NormalizeState(
	FImmortalSectState& State,
	int64 DefaultUtcTicks,
	const int32 UtcOffsetMinutes)
{
	if (DefaultUtcTicks <= 0) DefaultUtcTicks = FDateTime::UtcNow().GetTicks();
	bool bChanged = false;
	if (!State.bInitialized) { State.bInitialized = true; bChanged = true; }
	if (!State.SectId.IsNone() && !SectCatalog().Contains(State.SectId))
	{
		State.SectId = NAME_None;
		bChanged = true;
	}
	if (State.Contribution < 0) { State.Contribution = 0; bChanged = true; }
	if (State.TotalContributionEarned < 0) { State.TotalContributionEarned = 0; bChanged = true; }
	if (State.TotalContributionSpent < 0) { State.TotalContributionSpent = 0; bChanged = true; }
	if (State.TotalTasksClaimed < 0) { State.TotalTasksClaimed = 0; bChanged = true; }
	const int64 AuditedEarned = AddSaturated(State.TotalContributionSpent, State.Contribution);
	if (State.TotalContributionEarned < AuditedEarned)
	{
		State.TotalContributionEarned = AuditedEarned;
		bChanged = true;
	}
	if (State.LastObservedUtcTicks <= 0)
	{
		State.LastObservedUtcTicks = DefaultUtcTicks;
		bChanged = true;
	}
	if (State.TaskDayKey <= 0)
	{
		State.TaskDayKey = GetDayKeyFromUtcTicks(State.LastObservedUtcTicks, UtcOffsetMinutes);
		bChanged = true;
	}

	TArray<FImmortalSectTaskProgress> CanonicalTasks;
	for (const FImmortalSectTaskDefinition& Definition : TaskCatalog())
	{
		FImmortalSectTaskProgress Value;
		Value.TaskId = Definition.TaskId;
		if (const FImmortalSectTaskProgress* Existing = State.DailyTasks.FindByPredicate(
			[&Definition](const FImmortalSectTaskProgress& Candidate) { return Candidate.TaskId == Definition.TaskId; }))
		{
			Value.Progress = FMath::Clamp(Existing->Progress, 0, FMath::Max(Definition.TargetAmount, 1));
			Value.bClaimed = Existing->bClaimed && Value.Progress >= Definition.TargetAmount;
		}
		CanonicalTasks.Add(Value);
	}
	if (CanonicalTasks.Num() != State.DailyTasks.Num()) bChanged = true;
	else
	{
		for (int32 Index = 0; Index < CanonicalTasks.Num(); ++Index)
		{
			if (CanonicalTasks[Index].TaskId != State.DailyTasks[Index].TaskId
				|| CanonicalTasks[Index].Progress != State.DailyTasks[Index].Progress
				|| CanonicalTasks[Index].bClaimed != State.DailyTasks[Index].bClaimed)
			{
				bChanged = true;
				break;
			}
		}
	}
	State.DailyTasks = MoveTemp(CanonicalTasks);

	TArray<FImmortalSectOfferProgress> CanonicalOffers;
	for (const FImmortalSectStoreOfferDefinition& Definition : GetStoreOfferDefinitions(State.SectId))
	{
		FImmortalSectOfferProgress Value;
		Value.OfferId = Definition.OfferId;
		if (const FImmortalSectOfferProgress* Existing = State.OfferProgress.FindByPredicate(
			[&Definition](const FImmortalSectOfferProgress& Candidate) { return Candidate.OfferId == Definition.OfferId; }))
		{
			Value.DailyPurchaseCount = FMath::Max(Existing->DailyPurchaseCount, 0);
			if (Definition.DailyLimit > 0)
			{
				Value.DailyPurchaseCount = FMath::Min(Value.DailyPurchaseCount, Definition.DailyLimit);
			}
			Value.TotalPurchaseCount = FMath::Max<int64>(Existing->TotalPurchaseCount, 0);
			if (Definition.bOneTime)
			{
				Value.TotalPurchaseCount = FMath::Min<int64>(Value.TotalPurchaseCount, 1);
				Value.DailyPurchaseCount = FMath::Min(Value.DailyPurchaseCount, 1);
			}
		}
		CanonicalOffers.Add(Value);
	}
	if (CanonicalOffers.Num() != State.OfferProgress.Num()) bChanged = true;
	else
	{
		for (int32 Index = 0; Index < CanonicalOffers.Num(); ++Index)
		{
			if (CanonicalOffers[Index].OfferId != State.OfferProgress[Index].OfferId
				|| CanonicalOffers[Index].DailyPurchaseCount != State.OfferProgress[Index].DailyPurchaseCount
				|| CanonicalOffers[Index].TotalPurchaseCount != State.OfferProgress[Index].TotalPurchaseCount)
			{
				bChanged = true;
				break;
			}
		}
	}
	State.OfferProgress = MoveTemp(CanonicalOffers);
	if (State.Revision < 0) { State.Revision = 0; bChanged = true; }
	if (bChanged) State.Revision = IncrementRevision(State.Revision);
	return bChanged;
}

FImmortalSectDailyRefreshResult UImmortalSectLibrary::EnsureDailyState(
	FImmortalSectState& State,
	int64 CurrentUtcTicks,
	const int32 UtcOffsetMinutes)
{
	FImmortalSectDailyRefreshResult Result;
	if (CurrentUtcTicks <= 0) CurrentUtcTicks = FDateTime::UtcNow().GetTicks();
	FImmortalSectState Candidate = State;
	bool bChanged = NormalizeState(Candidate, CurrentUtcTicks, UtcOffsetMinutes);
	Result.DayKey = GetDayKeyFromUtcTicks(CurrentUtcTicks, UtcOffsetMinutes);
	if (CurrentUtcTicks < Candidate.LastObservedUtcTicks || Result.DayKey < Candidate.TaskDayKey)
	{
		Result.bClockRollbackDetected = true;
		Result.Message = FText::FromString(TEXT("系统时钟发生回拨，宗门每日状态已冻结"));
		return Result;
	}
	if (Result.DayKey > Candidate.TaskDayKey)
	{
		Candidate.TaskDayKey = Result.DayKey;
		Candidate.DailyTasks = FreshTaskProgress();
		for (FImmortalSectOfferProgress& Progress : Candidate.OfferProgress)
		{
			Progress.DailyPurchaseCount = 0;
		}
		Candidate.Revision = IncrementRevision(Candidate.Revision);
		bChanged = true;
	}
	if (CurrentUtcTicks > Candidate.LastObservedUtcTicks)
	{
		Candidate.LastObservedUtcTicks = CurrentUtcTicks;
		bChanged = true;
	}
	if (bChanged) State = MoveTemp(Candidate);
	Result.bSucceeded = true;
	Result.bStateChanged = bChanged;
	Result.Message = Result.DayKey > 0
		? FText::FromString(bChanged ? TEXT("宗门每日状态已同步") : TEXT("宗门每日状态无需刷新"))
		: FText::FromString(TEXT("宗门时间状态无效"));
	return Result;
}

FImmortalSectJoinResult UImmortalSectLibrary::EvaluateJoin(
	const FImmortalSectState& State,
	const FName SectId,
	const int32 RealmIndex,
	const FImmortalMapSystemState& MapState,
	const int64 CurrentUtcTicks,
	const int32 UtcOffsetMinutes)
{
	FImmortalSectJoinResult Result;
	Result.SectId = SectId;
	FImmortalSectDefinition Definition;
	Result.bKnownSect = GetSectDefinition(SectId, Definition);
	if (!Result.bKnownSect)
	{
		Result.Message = FText::FromString(TEXT("未知宗门"));
		return Result;
	}
	FImmortalSectState Candidate = State;
	const FImmortalSectDailyRefreshResult Refresh = EnsureDailyState(Candidate, CurrentUtcTicks, UtcOffsetMinutes);
	if (Refresh.bClockRollbackDetected)
	{
		Result.bClockRollbackDetected = true;
		Result.Message = Refresh.Message;
		return Result;
	}
	if (Candidate.SectId == SectId)
	{
		Result.bAlreadyJoined = true;
		Result.Message = FText::FromString(FString::Printf(TEXT("已经是%s弟子"), *Definition.DisplayName.ToString()));
		return Result;
	}
	if (Candidate.HasJoined())
	{
		Result.bLockedToOtherSect = true;
		Result.Message = FText::FromString(TEXT("当前版本加入宗门后不可改投其他宗门"));
		return Result;
	}
	FImmortalMapProgress Progress;
	const bool bMapRequirementMet = UImmortalMapLibrary::GetMapProgress(MapState, Definition.RequiredMapId, Progress)
		&& (Progress.bCompleted || Progress.Stage >= Definition.RequiredMapStage);
	Result.bRequirementsMet = RealmIndex >= Definition.RequiredRealmIndex && bMapRequirementMet;
	Result.bCanJoin = Result.bRequirementsMet;
	Result.Message = Result.bCanJoin
		? FText::FromString(FString::Printf(TEXT("可加入%s；选择后本版本不可改投"), *Definition.DisplayName.ToString()))
		: JoinRequirementText(Definition);
	return Result;
}

FImmortalSectJoinResult UImmortalSectLibrary::TryJoin(
	FImmortalSectState& State,
	const FName SectId,
	const int32 RealmIndex,
	const FImmortalMapSystemState& MapState,
	const int64 CurrentUtcTicks,
	const int32 UtcOffsetMinutes)
{
	FImmortalSectState Candidate = State;
	const FImmortalSectDailyRefreshResult Refresh = EnsureDailyState(Candidate, CurrentUtcTicks, UtcOffsetMinutes);
	FImmortalSectJoinResult Result = EvaluateJoin(
		Candidate, SectId, RealmIndex, MapState, CurrentUtcTicks, UtcOffsetMinutes);
	if (Refresh.bClockRollbackDetected)
	{
		Result.bClockRollbackDetected = true;
		Result.Message = Refresh.Message;
		return Result;
	}
	if (!Result.bCanJoin) return Result;
	Candidate.SectId = SectId;
	Candidate.DailyTasks = FreshTaskProgress();
	Candidate.OfferProgress = FreshOfferProgress(SectId);
	Candidate.Revision = IncrementRevision(Candidate.Revision);
	State = MoveTemp(Candidate);
	Result.bSucceeded = true;
	FImmortalSectDefinition Definition;
	GetSectDefinition(SectId, Definition);
	Result.Message = FText::FromString(FString::Printf(TEXT("已加入%s，宗门任务与贡献商店现已开放"),
		*Definition.DisplayName.ToString()));
	return Result;
}

FImmortalSectTaskProgressResult UImmortalSectLibrary::RecordCombatProgress(
	FImmortalSectState& State,
	const int32 MonsterKills,
	const int32 StageClears,
	const int32 BossKills,
	const int64 CurrentUtcTicks,
	const int32 UtcOffsetMinutes)
{
	FImmortalSectTaskProgressResult Result;
	FImmortalSectState Candidate = State;
	const FImmortalSectDailyRefreshResult Refresh = EnsureDailyState(Candidate, CurrentUtcTicks, UtcOffsetMinutes);
	if (Refresh.bClockRollbackDetected)
	{
		Result.bClockRollbackDetected = true;
		Result.Message = Refresh.Message;
		return Result;
	}
	if (!Candidate.HasJoined())
	{
		Result.Message = FText::FromString(TEXT("尚未加入宗门，战斗不会累计宗门任务"));
		return Result;
	}
	const bool bDailyReset = Candidate.TaskDayKey != State.TaskDayKey;
	bool bProgressChanged = false;
	for (FImmortalSectTaskProgress& Progress : Candidate.DailyTasks)
	{
		const FImmortalSectTaskDefinition* Definition = FindTaskDefinition(Progress.TaskId);
		if (!Definition) continue;
		int32 Requested = 0;
		switch (Definition->Metric)
		{
		case EImmortalSectTaskMetric::MonsterKills: Requested = FMath::Max(MonsterKills, 0); break;
		case EImmortalSectTaskMetric::StageClears: Requested = FMath::Max(StageClears, 0); break;
		case EImmortalSectTaskMetric::BossKills: Requested = FMath::Max(BossKills, 0); break;
		default: break;
		}
		const int32 Previous = Progress.Progress;
		Progress.Progress = FMath::Clamp<int64>(
			static_cast<int64>(Progress.Progress) + Requested, 0, Definition->TargetAmount);
		const int32 Applied = Progress.Progress - Previous;
		if (Applied <= 0) continue;
		bProgressChanged = true;
		switch (Definition->Metric)
		{
		case EImmortalSectTaskMetric::MonsterKills: Result.MonsterKillsApplied = Applied; break;
		case EImmortalSectTaskMetric::StageClears: Result.StageClearsApplied = Applied; break;
		case EImmortalSectTaskMetric::BossKills: Result.BossKillsApplied = Applied; break;
		default: break;
		}
	}
	if (bProgressChanged) Candidate.Revision = IncrementRevision(Candidate.Revision);
	Result.bSucceeded = true;
	// Do not persist a timestamp-only observation for every defeated monster.
	// Once today's tasks are capped, the spawner should return to its single
	// stage-progress write instead of issuing a second synchronous save forever.
	Result.bStateChanged = bDailyReset || bProgressChanged;
	if (Result.bStateChanged) State = MoveTemp(Candidate);
	Result.Message = bProgressChanged
		? FText::FromString(TEXT("宗门任务进度已更新"))
		: FText::FromString(TEXT("今日相关宗门任务已完成"));
	return Result;
}

FImmortalSectTaskClaimResult UImmortalSectLibrary::EvaluateTaskClaim(
	const FImmortalSectState& State,
	const FName TaskId,
	const int64 CurrentUtcTicks,
	const int32 UtcOffsetMinutes)
{
	FImmortalSectTaskClaimResult Result;
	Result.TaskId = TaskId;
	const FImmortalSectTaskDefinition* Definition = FindTaskDefinition(TaskId);
	Result.bKnownTask = Definition != nullptr;
	if (!Definition)
	{
		Result.Message = FText::FromString(TEXT("未知宗门任务"));
		return Result;
	}
	FImmortalSectState Candidate = State;
	const FImmortalSectDailyRefreshResult Refresh = EnsureDailyState(Candidate, CurrentUtcTicks, UtcOffsetMinutes);
	if (Refresh.bClockRollbackDetected)
	{
		Result.bClockRollbackDetected = true;
		Result.Message = Refresh.Message;
		return Result;
	}
	if (!Candidate.HasJoined())
	{
		Result.Message = FText::FromString(TEXT("加入宗门后才能领取任务贡献"));
		return Result;
	}
	FImmortalSectTaskProgress Progress;
	if (!GetTaskProgress(Candidate, TaskId, Progress))
	{
		Result.Message = FText::FromString(TEXT("任务状态缺失"));
		return Result;
	}
	Result.bCompleted = Progress.Progress >= Definition->TargetAmount;
	Result.bAlreadyClaimed = Progress.bClaimed;
	Result.bCanClaim = Result.bCompleted && !Result.bAlreadyClaimed
		&& Candidate.Contribution <= MAX_int32 - Definition->ContributionReward;
	Result.ContributionAfter = Candidate.Contribution;
	if (Result.bAlreadyClaimed) Result.Message = FText::FromString(TEXT("今日该任务奖励已经领取"));
	else if (!Result.bCompleted) Result.Message = FText::FromString(FString::Printf(TEXT("任务尚未完成（%d/%d）"),
		Progress.Progress, Definition->TargetAmount));
	else if (!Result.bCanClaim) Result.Message = FText::FromString(TEXT("宗门贡献已达到上限"));
	else Result.Message = FText::FromString(FString::Printf(TEXT("可领取%d点宗门贡献"), Definition->ContributionReward));
	return Result;
}

FImmortalSectTaskClaimResult UImmortalSectLibrary::TryClaimTask(
	FImmortalSectState& State,
	const FName TaskId,
	const int64 CurrentUtcTicks,
	const int32 UtcOffsetMinutes)
{
	FImmortalSectState Candidate = State;
	const FImmortalSectDailyRefreshResult Refresh = EnsureDailyState(Candidate, CurrentUtcTicks, UtcOffsetMinutes);
	FImmortalSectTaskClaimResult Result = EvaluateTaskClaim(
		Candidate, TaskId, CurrentUtcTicks, UtcOffsetMinutes);
	if (Refresh.bClockRollbackDetected)
	{
		Result.bClockRollbackDetected = true;
		Result.Message = Refresh.Message;
		return Result;
	}
	if (!Result.bCanClaim) return Result;
	const FImmortalSectTaskDefinition* Definition = FindTaskDefinition(TaskId);
	FImmortalSectTaskProgress* Progress = Candidate.DailyTasks.FindByPredicate([TaskId](const FImmortalSectTaskProgress& Value)
	{
		return Value.TaskId == TaskId;
	});
	if (!Definition || !Progress) return Result;
	Progress->bClaimed = true;
	Candidate.Contribution += Definition->ContributionReward;
	Candidate.TotalContributionEarned = AddSaturated(
		Candidate.TotalContributionEarned, Definition->ContributionReward);
	Candidate.TotalTasksClaimed = AddSaturated(Candidate.TotalTasksClaimed, 1);
	Candidate.Revision = IncrementRevision(Candidate.Revision);
	State = MoveTemp(Candidate);
	Result.bSucceeded = true;
	Result.ContributionAwarded = Definition->ContributionReward;
	Result.ContributionAfter = State.Contribution;
	Result.Message = FText::FromString(FString::Printf(TEXT("任务完成，获得%d点宗门贡献"),
		Definition->ContributionReward));
	return Result;
}

FImmortalSectExchangeResult UImmortalSectLibrary::EvaluateExchange(
	const FImmortalSectState& State,
	const FName OfferId,
	const int64 CurrentUtcTicks,
	const int32 UtcOffsetMinutes)
{
	FImmortalSectExchangeResult Result;
	Result.OfferId = OfferId;
	FImmortalSectState Candidate = State;
	const FImmortalSectDailyRefreshResult Refresh = EnsureDailyState(Candidate, CurrentUtcTicks, UtcOffsetMinutes);
	if (Refresh.bClockRollbackDetected)
	{
		Result.bClockRollbackDetected = true;
		Result.Message = Refresh.Message;
		return Result;
	}
	if (!Candidate.HasJoined())
	{
		Result.Message = FText::FromString(TEXT("加入宗门后才能使用贡献商店"));
		return Result;
	}
	FImmortalSectStoreOfferDefinition Offer;
	Result.bKnownOffer = FindOffer(Candidate.SectId, OfferId, Offer);
	if (!Result.bKnownOffer)
	{
		Result.Message = FText::FromString(TEXT("未知宗门兑换项"));
		return Result;
	}
	Result.RewardType = Offer.RewardType;
	Result.RewardId = Offer.RewardId;
	Result.RewardQuantity = Offer.RewardQuantity;
	Result.ContributionAfter = Candidate.Contribution;
	FImmortalSectOfferProgress Progress;
	if (!GetOfferProgress(Candidate, OfferId, Progress))
	{
		Result.Message = FText::FromString(TEXT("兑换状态缺失"));
		return Result;
	}
	Result.bAffordable = Candidate.Contribution >= Offer.ContributionCost;
	Result.bRequirementMet = Candidate.TotalContributionEarned >= Offer.RequiredLifetimeContribution;
	Result.bDailyLimitReached = Offer.DailyLimit > 0 && Progress.DailyPurchaseCount >= Offer.DailyLimit;
	Result.bOneTimePurchased = Offer.bOneTime && Progress.TotalPurchaseCount > 0;
	Result.bCanExchange = Result.bAffordable && Result.bRequirementMet
		&& !Result.bDailyLimitReached && !Result.bOneTimePurchased;
	if (!Result.bRequirementMet)
	{
		Result.Message = FText::FromString(FString::Printf(TEXT("累计获得%d贡献后开放"),
			static_cast<int32>(FMath::Min<int64>(Offer.RequiredLifetimeContribution, MAX_int32))));
	}
	else if (Result.bOneTimePurchased) Result.Message = FText::FromString(TEXT("该真传奖励已经兑换"));
	else if (Result.bDailyLimitReached) Result.Message = FText::FromString(TEXT("今日兑换次数已用完"));
	else if (!Result.bAffordable) Result.Message = FText::FromString(FString::Printf(TEXT("宗门贡献不足，需要%d"), Offer.ContributionCost));
	else Result.Message = FText::FromString(FString::Printf(TEXT("消耗%d贡献兑换"), Offer.ContributionCost));
	return Result;
}

FImmortalSectExchangeResult UImmortalSectLibrary::TryExchange(
	FImmortalSectState& State,
	const FName OfferId,
	const int64 CurrentUtcTicks,
	const int32 UtcOffsetMinutes)
{
	FImmortalSectState Candidate = State;
	const FImmortalSectDailyRefreshResult Refresh = EnsureDailyState(Candidate, CurrentUtcTicks, UtcOffsetMinutes);
	FImmortalSectExchangeResult Result = EvaluateExchange(
		Candidate, OfferId, CurrentUtcTicks, UtcOffsetMinutes);
	if (Refresh.bClockRollbackDetected)
	{
		Result.bClockRollbackDetected = true;
		Result.Message = Refresh.Message;
		return Result;
	}
	if (!Result.bCanExchange) return Result;
	FImmortalSectStoreOfferDefinition Offer;
	FImmortalSectOfferProgress* Progress = Candidate.OfferProgress.FindByPredicate([OfferId](const FImmortalSectOfferProgress& Value)
	{
		return Value.OfferId == OfferId;
	});
	if (!FindOffer(Candidate.SectId, OfferId, Offer) || !Progress) return Result;
	Candidate.Contribution -= Offer.ContributionCost;
	Candidate.TotalContributionSpent = AddSaturated(Candidate.TotalContributionSpent, Offer.ContributionCost);
	Progress->DailyPurchaseCount = Progress->DailyPurchaseCount >= MAX_int32
		? MAX_int32 : Progress->DailyPurchaseCount + 1;
	Progress->TotalPurchaseCount = AddSaturated(Progress->TotalPurchaseCount, 1);
	Candidate.Revision = IncrementRevision(Candidate.Revision);
	State = MoveTemp(Candidate);
	Result.bSucceeded = true;
	Result.ContributionSpent = Offer.ContributionCost;
	Result.ContributionAfter = State.Contribution;
	Result.Message = FText::FromString(FString::Printf(TEXT("兑换成功，剩余贡献%d"), State.Contribution));
	return Result;
}
