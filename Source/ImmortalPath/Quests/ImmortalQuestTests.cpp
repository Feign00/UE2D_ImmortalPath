// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalQuestTypes.h"
#include "../Save/ImmortalPathSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalQuestCatalogTest,
	"ImmortalPath.Quests.CatalogAndPrerequisites",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalQuestCatalogTest::RunTest(const FString& Parameters)
{
	const TArray<FImmortalQuestDefinition> Main =
		UImmortalQuestLibrary::GetQuestDefinitions(EImmortalQuestCategory::Main);
	const TArray<FImmortalQuestDefinition> Daily =
		UImmortalQuestLibrary::GetQuestDefinitions(EImmortalQuestCategory::Daily);
	const TArray<FImmortalQuestDefinition> Achievements =
		UImmortalQuestLibrary::GetQuestDefinitions(EImmortalQuestCategory::Achievement);
	TestEqual(TEXT("Seven main quests form the first complete journey"), Main.Num(), 7);
	TestEqual(TEXT("Five daily quests cover the idle loop"), Daily.Num(), 5);
	TestEqual(TEXT("Seven durable achievements exist"), Achievements.Num(), 7);

	TSet<FName> Seen;
	for (const EImmortalQuestCategory Category :
		{EImmortalQuestCategory::Main, EImmortalQuestCategory::Daily, EImmortalQuestCategory::Achievement})
	{
		for (const FImmortalQuestDefinition& Definition :
			UImmortalQuestLibrary::GetQuestDefinitions(Category))
		{
			TestTrue(TEXT("Every quest is valid"), Definition.IsValid());
			TestFalse(TEXT("Quest ids are globally unique"), Seen.Contains(Definition.QuestId));
			Seen.Add(Definition.QuestId);
			if (!Definition.PrerequisiteQuestId.IsNone())
			{
				FImmortalQuestDefinition Prerequisite;
				TestTrue(TEXT("Every prerequisite exists"),
					UImmortalQuestLibrary::GetQuestDefinition(Definition.PrerequisiteQuestId, Prerequisite));
				TestEqual(TEXT("Only main quests use prerequisites"),
					Prerequisite.Category, EImmortalQuestCategory::Main);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalQuestMainSequenceTest,
	"ImmortalPath.Quests.MainSequenceAndClaims",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalQuestMainSequenceTest::RunTest(const FString& Parameters)
{
	const int64 Now = FDateTime(2026, 8, 6, 8, 0, 0).GetTicks();
	FImmortalQuestState State = UImmortalQuestLibrary::CreateDefaultState(Now);
	UImmortalQuestLibrary::RecordProgress(State, EImmortalQuestMetric::MonsterKills, 1, Now);
	TestTrue(TEXT("First main quest can be claimed"),
		UImmortalQuestLibrary::EvaluateClaim(State, TEXT("Main_FirstBlood"), Now).bCanClaim);
	TestFalse(TEXT("Second main quest stays locked before prerequisite claim"),
		UImmortalQuestLibrary::GetProgress(State, TEXT("Main_TrialPath")).bUnlocked);
	const FImmortalQuestClaimResult First =
		UImmortalQuestLibrary::TryClaim(State, TEXT("Main_FirstBlood"), Now);
	TestTrue(TEXT("First claim succeeds"), First.bSucceeded);
	TestTrue(TEXT("Second main quest unlocks"),
		UImmortalQuestLibrary::GetProgress(State, TEXT("Main_TrialPath")).bUnlocked);
	UImmortalQuestLibrary::RecordProgress(State, EImmortalQuestMetric::StageClears, 5, Now);
	TestTrue(TEXT("Second main quest reaches target"),
		UImmortalQuestLibrary::GetProgress(State, TEXT("Main_TrialPath")).bCanClaim);
	TestFalse(TEXT("Duplicate first claim is rejected"),
		UImmortalQuestLibrary::TryClaim(State, TEXT("Main_FirstBlood"), Now).bSucceeded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalQuestDailyTest,
	"ImmortalPath.Quests.DailyResetAndClockRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalQuestDailyTest::RunTest(const FString& Parameters)
{
	const int64 DayOne = FDateTime(2026, 8, 6, 4, 0, 0).GetTicks();
	const int64 DayTwo = DayOne + ETimespan::TicksPerDay;
	FImmortalQuestState State = UImmortalQuestLibrary::CreateDefaultState(DayOne);
	UImmortalQuestLibrary::RecordProgress(State, EImmortalQuestMetric::MonsterKills, 20, DayOne);
	TestTrue(TEXT("Daily kill quest completes"),
		UImmortalQuestLibrary::TryClaim(State, TEXT("Daily_SlayDemons"), DayOne).bSucceeded);
	const int64 LifetimeKills = State.LifetimeCounters.MonsterKills;
	const FImmortalQuestDailyRefreshResult Refresh =
		UImmortalQuestLibrary::EnsureDailyState(State, DayTwo);
	TestTrue(TEXT("Next day refresh succeeds"), Refresh.bSucceeded);
	TestTrue(TEXT("Next day resets daily state"), Refresh.bStateChanged);
	TestEqual(TEXT("Daily kills reset"), State.DailyCounters.MonsterKills, static_cast<int64>(0));
	TestEqual(TEXT("Lifetime kills remain"), State.LifetimeCounters.MonsterKills, LifetimeKills);
	TestTrue(TEXT("Daily claim list resets"), State.ClaimedDailyQuestIds.IsEmpty());
	const int32 StableDay = State.DailyDayKey;
	const FImmortalQuestDailyRefreshResult Rollback =
		UImmortalQuestLibrary::EnsureDailyState(State, DayOne);
	TestTrue(TEXT("Clock rollback is detected"), Rollback.bClockRollbackDetected);
	TestEqual(TEXT("Clock rollback never rewinds day key"), State.DailyDayKey, StableDay);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalQuestAchievementNormalizationTest,
	"ImmortalPath.Quests.AchievementsAndNormalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalQuestAchievementNormalizationTest::RunTest(const FString& Parameters)
{
	const int64 Now = FDateTime(2026, 8, 6, 8, 0, 0).GetTicks();
	FImmortalQuestState State = UImmortalQuestLibrary::CreateDefaultState(Now);
	UImmortalQuestLibrary::RecordProgress(State, EImmortalQuestMetric::MonsterKills, 1000, Now);
	TestTrue(TEXT("One event can complete the thousand-kill achievement"),
		UImmortalQuestLibrary::GetProgress(State, TEXT("Achievement_Kills1000")).bCanClaim);
	TestTrue(TEXT("Achievement claim succeeds"),
		UImmortalQuestLibrary::TryClaim(State, TEXT("Achievement_Kills1000"), Now).bSucceeded);
	State.LifetimeCounters.BossKills = -10;
	State.ClaimedAchievementIds.Add(TEXT("Achievement_Kills1000"));
	State.ClaimedAchievementIds.Add(TEXT("UnknownAchievement"));
	State.TotalClaims = -4;
	const bool bNormalized = UImmortalQuestLibrary::NormalizeState(State, Now);
	TestTrue(TEXT("Corrupt state is normalized"), bNormalized);
	TestEqual(TEXT("Negative counters clamp"), State.LifetimeCounters.BossKills, static_cast<int64>(0));
	TestEqual(TEXT("Duplicate and unknown claims are removed"), State.ClaimedAchievementIds.Num(), 1);
	TestEqual(TEXT("Negative total claims clamp"), State.TotalClaims, static_cast<int64>(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalQuestSaveSchemaTest,
	"ImmortalPath.Quests.SaveSchemaV23",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalQuestSaveSchemaTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Quest persistence advances the save schema"),
		UImmortalPathSaveGame::CurrentSaveVersion, 23);
	UImmortalPathSaveGame* Save = NewObject<UImmortalPathSaveGame>();
	TestNotNull(TEXT("Save object can be constructed"), Save);
	if (!Save) return false;
	TestFalse(TEXT("Fresh save requires explicit quest initialization"),
		Save->bQuestSystemInitialized);
	TestFalse(TEXT("Fresh quest snapshot is not silently initialized"),
		Save->QuestState.bInitialized);
	TestFalse(TEXT("Fresh save never starts inside death recovery"),
		Save->bDeathCultivationRecoveryRequired);
	return true;
}

#endif
