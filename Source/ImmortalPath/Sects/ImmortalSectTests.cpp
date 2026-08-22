// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "ImmortalSectTypes.h"

#include "../Save/ImmortalPathSaveGame.h"
#include "Misc/AutomationTest.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"

namespace
{
	bool SameSectState(const FImmortalSectState& Left, const FImmortalSectState& Right)
	{
		if (Left.bInitialized != Right.bInitialized
			|| Left.SectId != Right.SectId
			|| Left.Contribution != Right.Contribution
			|| Left.TotalContributionEarned != Right.TotalContributionEarned
			|| Left.TotalContributionSpent != Right.TotalContributionSpent
			|| Left.TaskDayKey != Right.TaskDayKey
			|| Left.LastObservedUtcTicks != Right.LastObservedUtcTicks
			|| Left.TotalTasksClaimed != Right.TotalTasksClaimed
			|| Left.Revision != Right.Revision
			|| Left.DailyTasks.Num() != Right.DailyTasks.Num()
			|| Left.OfferProgress.Num() != Right.OfferProgress.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.DailyTasks.Num(); ++Index)
		{
			const FImmortalSectTaskProgress& A = Left.DailyTasks[Index];
			const FImmortalSectTaskProgress& B = Right.DailyTasks[Index];
			if (A.TaskId != B.TaskId || A.Progress != B.Progress || A.bClaimed != B.bClaimed)
			{
				return false;
			}
		}
		for (int32 Index = 0; Index < Left.OfferProgress.Num(); ++Index)
		{
			const FImmortalSectOfferProgress& A = Left.OfferProgress[Index];
			const FImmortalSectOfferProgress& B = Right.OfferProgress[Index];
			if (A.OfferId != B.OfferId
				|| A.DailyPurchaseCount != B.DailyPurchaseCount
				|| A.TotalPurchaseCount != B.TotalPurchaseCount)
			{
				return false;
			}
		}
		return true;
	}

	FImmortalMapSystemState MakeMapState(const int32 QingyunStage)
	{
		FImmortalMapSystemState Result = UImmortalMapLibrary::CreateMigratedState(QingyunStage, 0, false);
		UImmortalMapLibrary::NormalizeState(Result);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalSectCatalogAndMembershipTest,
	"ImmortalPath.Sect.CatalogAndMembership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalSectCatalogAndMembershipTest::RunTest(const FString& Parameters)
{
	const int64 Now = FDateTime(2026, 7, 22, 2, 0).GetTicks();
	const TArray<FName> SectIds = UImmortalSectLibrary::GetKnownSectIds();
	TestEqual(TEXT("Four stable sects are available"), SectIds.Num(), 4);
	TestTrue(TEXT("Qingyun sect is stable"), SectIds.Contains(TEXT("QingyunSect")));
	TestTrue(TEXT("Heavenly Sword sect is stable"), SectIds.Contains(TEXT("HeavenlySwordSect")));
	TestTrue(TEXT("Myriad Demon Valley is stable"), SectIds.Contains(TEXT("MyriadDemonValley")));
	TestTrue(TEXT("Demon sect is stable"), SectIds.Contains(TEXT("DemonSect")));

	const TMap<FName, FName> ExpectedTechniques =
	{
		{TEXT("QingyunSect"), TEXT("BasicBreathing")},
		{TEXT("HeavenlySwordSect"), TEXT("QingyunSwordArt")},
		{TEXT("MyriadDemonValley"), TEXT("BurningHeavenArt")},
		{TEXT("DemonSect"), TEXT("NineHeavensThunder")}
	};
	for (const FName SectId : SectIds)
	{
		FImmortalSectDefinition Definition;
		TestTrue(FString::Printf(TEXT("Definition exists for %s"), *SectId.ToString()),
			UImmortalSectLibrary::GetSectDefinition(SectId, Definition));
		TestEqual(FString::Printf(TEXT("Technique mapping for %s"), *SectId.ToString()),
			Definition.TechniqueRewardId, ExpectedTechniques.FindRef(SectId));
		const TArray<FImmortalSectStoreOfferDefinition> Offers =
			UImmortalSectLibrary::GetStoreOfferDefinitions(SectId);
		TestEqual(FString::Printf(TEXT("Four offers for %s"), *SectId.ToString()), Offers.Num(), 4);
	}

	const TArray<FImmortalSectTaskDefinition> Tasks = UImmortalSectLibrary::GetDailyTaskDefinitions();
	TestEqual(TEXT("Three daily tasks"), Tasks.Num(), 3);
	TestEqual(TEXT("Monster task target"), Tasks[0].TargetAmount, 20);
	TestEqual(TEXT("Monster task reward"), Tasks[0].ContributionReward, 60);
	TestEqual(TEXT("Stage task target"), Tasks[1].TargetAmount, 3);
	TestEqual(TEXT("Stage task reward"), Tasks[1].ContributionReward, 90);
	TestEqual(TEXT("Boss task target"), Tasks[2].TargetAmount, 1);
	TestEqual(TEXT("Boss task reward"), Tasks[2].ContributionReward, 120);
	TestEqual(TEXT("SaveGame version is v23 after death recovery persistence was added"),
		UImmortalPathSaveGame::CurrentSaveVersion, 23);

	const FImmortalMapSystemState EarlyMaps = MakeMapState(1);
	FImmortalSectState QingyunState = UImmortalSectLibrary::CreateDefaultState(Now);
	const FImmortalSectJoinResult QingyunJoin = UImmortalSectLibrary::TryJoin(
		QingyunState, TEXT("QingyunSect"), 0, EarlyMaps, Now);
	TestTrue(TEXT("Qingyun is available immediately"), QingyunJoin.bSucceeded);
	const FImmortalSectState JoinedSnapshot = QingyunState;
	const FImmortalSectJoinResult SwitchResult = UImmortalSectLibrary::TryJoin(
		QingyunState, TEXT("HeavenlySwordSect"), 9, MakeMapState(999), Now);
	TestFalse(TEXT("Joined player cannot switch sect"), SwitchResult.bSucceeded);
	TestTrue(TEXT("Switch rejection reports permanent lock"), SwitchResult.bLockedToOtherSect);
	TestTrue(TEXT("Rejected switch changes nothing"), SameSectState(QingyunState, JoinedSnapshot));

	FImmortalSectState HeavenlyState = UImmortalSectLibrary::CreateDefaultState(Now);
	const FImmortalSectJoinResult HeavenlyLocked = UImmortalSectLibrary::TryJoin(
		HeavenlyState, TEXT("HeavenlySwordSect"), 0, EarlyMaps, Now);
	TestFalse(TEXT("Heavenly Sword needs Qingyun stage 25"), HeavenlyLocked.bSucceeded);
	TestTrue(TEXT("Rejected requirement changes nothing"),
		SameSectState(HeavenlyState, UImmortalSectLibrary::CreateDefaultState(Now)));
	const FImmortalSectJoinResult HeavenlyJoin = UImmortalSectLibrary::TryJoin(
		HeavenlyState, TEXT("HeavenlySwordSect"), 0, MakeMapState(25), Now);
	TestTrue(TEXT("Heavenly Sword unlocks at stage 25"), HeavenlyJoin.bSucceeded);

	FImmortalSectState MyriadState = UImmortalSectLibrary::CreateDefaultState(Now);
	TestFalse(TEXT("Myriad Demon rejects Qi Refining"), UImmortalSectLibrary::TryJoin(
		MyriadState, TEXT("MyriadDemonValley"), 0, EarlyMaps, Now).bSucceeded);
	TestTrue(TEXT("Myriad Demon accepts Foundation Establishment"), UImmortalSectLibrary::TryJoin(
		MyriadState, TEXT("MyriadDemonValley"), 1, EarlyMaps, Now).bSucceeded);
	FImmortalSectState DemonState = UImmortalSectLibrary::CreateDefaultState(Now);
	TestFalse(TEXT("Demon Sect rejects Foundation Establishment"), UImmortalSectLibrary::TryJoin(
		DemonState, TEXT("DemonSect"), 1, EarlyMaps, Now).bSucceeded);
	TestTrue(TEXT("Demon Sect accepts Golden Core"), UImmortalSectLibrary::TryJoin(
		DemonState, TEXT("DemonSect"), 2, EarlyMaps, Now).bSucceeded);

	FImmortalSectState Corrupt;
	Corrupt.bInitialized = true;
	Corrupt.SectId = TEXT("UnknownSect");
	Corrupt.Contribution = -7;
	Corrupt.TotalContributionEarned = -9;
	Corrupt.TotalContributionSpent = -11;
	Corrupt.TotalTasksClaimed = -4;
	Corrupt.Revision = -3;
	FImmortalSectTaskProgress BadTask;
	BadTask.TaskId = TEXT("SlayDemons");
	BadTask.Progress = 999;
	BadTask.bClaimed = true;
	Corrupt.DailyTasks = {BadTask, BadTask};
	FImmortalSectOfferProgress BadOffer;
	BadOffer.OfferId = TEXT("UnknownOffer");
	BadOffer.DailyPurchaseCount = -2;
	BadOffer.TotalPurchaseCount = -5;
	Corrupt.OfferProgress = {BadOffer};
	TestTrue(TEXT("Normalize repairs corrupt state"), UImmortalSectLibrary::NormalizeState(Corrupt, Now));
	TestTrue(TEXT("Unknown membership is cleared"), Corrupt.SectId.IsNone());
	TestEqual(TEXT("Contribution is clamped"), Corrupt.Contribution, 0);
	TestEqual(TEXT("Canonical tasks restored"), Corrupt.DailyTasks.Num(), 3);
	TestEqual(TEXT("Monster progress capped"), Corrupt.DailyTasks[0].Progress, 20);
	TestEqual(TEXT("Unjoined offer state removed"), Corrupt.OfferProgress.Num(), 0);
	TestTrue(TEXT("Revision is valid"), Corrupt.Revision > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalSectDailyTransactionsTest,
	"ImmortalPath.Sect.DailyTransactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalSectDailyTransactionsTest::RunTest(const FString& Parameters)
{
	const int64 DayOne = FDateTime(2026, 7, 22, 2, 0).GetTicks();
	const int64 DayTwo = DayOne + ETimespan::TicksPerDay;
	FImmortalSectState State = UImmortalSectLibrary::CreateDefaultState(DayOne);
	TestTrue(TEXT("Join succeeds for transaction test"), UImmortalSectLibrary::TryJoin(
		State, TEXT("QingyunSect"), 0, MakeMapState(1), DayOne).bSucceeded);

	const FImmortalSectTaskProgressResult Progress = UImmortalSectLibrary::RecordCombatProgress(
		State, 999, 999, 999, DayOne + ETimespan::TicksPerSecond);
	TestTrue(TEXT("Combat progress succeeds"), Progress.bSucceeded);
	FImmortalSectTaskProgress Task;
	UImmortalSectLibrary::GetTaskProgress(State, TEXT("SlayDemons"), Task);
	TestEqual(TEXT("Monster progress caps at 20"), Task.Progress, 20);
	UImmortalSectLibrary::GetTaskProgress(State, TEXT("AdvanceTrials"), Task);
	TestEqual(TEXT("Stage progress caps at 3"), Task.Progress, 3);
	UImmortalSectLibrary::GetTaskProgress(State, TEXT("DefeatGuardians"), Task);
	TestEqual(TEXT("Boss progress caps at 1"), Task.Progress, 1);
	const FImmortalSectState CappedProgressSnapshot = State;
	const FImmortalSectTaskProgressResult CappedProgress = UImmortalSectLibrary::RecordCombatProgress(
		State, 1, 0, 0, DayOne + 1500 * ETimespan::TicksPerMillisecond);
	TestFalse(TEXT("Capped tasks do not request another save"), CappedProgress.bStateChanged);
	TestTrue(TEXT("Capped combat changes no persisted field"), SameSectState(State, CappedProgressSnapshot));

	for (const FName TaskId : {FName(TEXT("SlayDemons")), FName(TEXT("AdvanceTrials")), FName(TEXT("DefeatGuardians"))})
	{
		TestTrue(FString::Printf(TEXT("Claim succeeds for %s"), *TaskId.ToString()),
			UImmortalSectLibrary::TryClaimTask(State, TaskId, DayOne + 2 * ETimespan::TicksPerSecond).bSucceeded);
	}
	TestEqual(TEXT("All task rewards total 270 contribution"), State.Contribution, 270);
	TestEqual(TEXT("Lifetime earned ledger is 270"), State.TotalContributionEarned, 270LL);
	TestEqual(TEXT("Three task claims audited"), State.TotalTasksClaimed, 3LL);
	const FImmortalSectState ClaimedSnapshot = State;
	TestFalse(TEXT("Duplicate task claim is rejected"), UImmortalSectLibrary::TryClaimTask(
		State, TEXT("SlayDemons"), DayOne + 3 * ETimespan::TicksPerSecond).bSucceeded);
	TestTrue(TEXT("Duplicate claim changes nothing"), SameSectState(State, ClaimedSnapshot));

	State.Contribution = 10000;
	State.TotalContributionEarned = 10000;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		TestTrue(FString::Printf(TEXT("Daily material exchange %d succeeds"), Index + 1),
			UImmortalSectLibrary::TryExchange(State, TEXT("SectSupplies"),
				DayOne + (10 + Index) * ETimespan::TicksPerSecond).bSucceeded);
	}
	const FImmortalSectState DailyLimitSnapshot = State;
	const FImmortalSectExchangeResult SixthSupply = UImmortalSectLibrary::TryExchange(
		State, TEXT("SectSupplies"), DayOne + 20 * ETimespan::TicksPerSecond);
	TestFalse(TEXT("Sixth daily material exchange is rejected"), SixthSupply.bSucceeded);
	TestTrue(TEXT("Daily limit is reported"), SixthSupply.bDailyLimitReached);
	TestTrue(TEXT("Daily-limit rejection changes nothing"), SameSectState(State, DailyLimitSnapshot));

	const FImmortalSectExchangeResult Manual = UImmortalSectLibrary::TryExchange(
		State, TEXT("SectManual"), DayOne + 21 * ETimespan::TicksPerSecond);
	TestTrue(TEXT("Sect manual can be exchanged once"), Manual.bSucceeded);
	const FImmortalSectState ManualSnapshot = State;
	const FImmortalSectExchangeResult DuplicateManual = UImmortalSectLibrary::TryExchange(
		State, TEXT("SectManual"), DayOne + 22 * ETimespan::TicksPerSecond);
	TestFalse(TEXT("Sect manual cannot be exchanged twice"), DuplicateManual.bSucceeded);
	TestTrue(TEXT("One-time state is reported"), DuplicateManual.bOneTimePurchased);
	TestTrue(TEXT("One-time rejection changes nothing"), SameSectState(State, ManualSnapshot));

	const FImmortalSectDailyRefreshResult NextDay = UImmortalSectLibrary::EnsureDailyState(State, DayTwo);
	TestTrue(TEXT("Future day refreshes state"), NextDay.bStateChanged);
	UImmortalSectLibrary::GetTaskProgress(State, TEXT("SlayDemons"), Task);
	TestEqual(TEXT("New day resets task progress"), Task.Progress, 0);
	TestFalse(TEXT("New day resets task claim"), Task.bClaimed);
	FImmortalSectOfferProgress OfferProgress;
	UImmortalSectLibrary::GetOfferProgress(State, TEXT("SectSupplies"), OfferProgress);
	TestEqual(TEXT("New day resets daily purchase count"), OfferProgress.DailyPurchaseCount, 0);
	UImmortalSectLibrary::GetOfferProgress(State, TEXT("SectManual"), OfferProgress);
	TestEqual(TEXT("New day preserves one-time purchase"), OfferProgress.TotalPurchaseCount, 1LL);

	const FImmortalSectState SameDaySnapshot = State;
	const FImmortalSectDailyRefreshResult SameDay = UImmortalSectLibrary::EnsureDailyState(State, DayTwo);
	TestFalse(TEXT("Same timestamp is idempotent"), SameDay.bStateChanged);
	TestTrue(TEXT("Idempotent refresh changes nothing"), SameSectState(State, SameDaySnapshot));

	const FImmortalSectState RollbackSnapshot = State;
	const FImmortalSectTaskProgressResult Rollback = UImmortalSectLibrary::RecordCombatProgress(
		State, 5, 1, 1, DayTwo - ETimespan::TicksPerHour);
	TestTrue(TEXT("Clock rollback is detected"), Rollback.bClockRollbackDetected);
	TestTrue(TEXT("Clock rollback changes every state field by zero"), SameSectState(State, RollbackSnapshot));

	const int32 PreviousDayKey = State.TaskDayKey;
	const FImmortalSectDailyRefreshResult Jump = UImmortalSectLibrary::EnsureDailyState(
		State, DayTwo + 3 * ETimespan::TicksPerDay);
	TestTrue(TEXT("Skipped days perform one current-day refresh"), Jump.bStateChanged);
	TestTrue(TEXT("Day key advances without replaying missed rewards"), State.TaskDayKey > PreviousDayKey);
	TestEqual(TEXT("No contribution is manufactured by skipped days"), State.Contribution,
		RollbackSnapshot.Contribution);
	return true;
}

#endif
