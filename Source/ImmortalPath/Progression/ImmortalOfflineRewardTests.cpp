// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalOfflineRewardTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalOfflineRewardCalculationTest,
	"ImmortalPath.Progression.OfflineRewards.Calculation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalOfflineRewardCalculationTest::RunTest(const FString& Parameters)
{
	const int64 NowTicks = FDateTime(2026, 7, 17, 12, 0, 0).GetTicks();
	auto CalculateForSeconds = [NowTicks](const int64 Seconds)
	{
		return UImmortalOfflineRewardLibrary::Calculate(
			NowTicks - FTimespan::FromSeconds(static_cast<double>(Seconds)).GetTicks(),
			NowTicks,
			60,
			8.0f,
			2.0f,
			0.75f,
			2.0f,
			900.0f,
			12,
			600.0f,
			24);
	};

	const FImmortalOfflineRewardResult BelowThreshold = CalculateForSeconds(30);
	TestFalse(TEXT("Thirty seconds does not claim"), BelowThreshold.bEligible);
	TestEqual(TEXT("Raw seconds remain observable"), BelowThreshold.RawOfflineSeconds, static_cast<int64>(30));

	const FImmortalOfflineRewardResult TenMinutes = CalculateForSeconds(600);
	TestTrue(TEXT("Ten minutes is eligible"), TenMinutes.bEligible);
	TestEqual(TEXT("Ten-minute cultivation"), TenMinutes.Cultivation, 900);
	TestEqual(TEXT("Ten-minute spirit stones"), TenMinutes.SpiritStones, 20);
	TestEqual(TEXT("Ten minutes has no equipment roll yet"), TenMinutes.EquipmentCount, 0);
	TestEqual(TEXT("Ten minutes grants one material bundle"), TenMinutes.MaterialBundleCount, 1);

	const FImmortalOfflineRewardResult OneHour = CalculateForSeconds(3600);
	TestEqual(TEXT("One-hour equipment count"), OneHour.EquipmentCount, 4);
	TestEqual(TEXT("One-hour material bundle count"), OneHour.MaterialBundleCount, 6);

	const FImmortalOfflineRewardResult TenHours = CalculateForSeconds(36000);
	TestTrue(TEXT("Ten hours is capped"), TenHours.bCappedByMaximum);
	TestEqual(TEXT("Eight-hour reward cap"), TenHours.RewardedOfflineSeconds, static_cast<int64>(28800));
	TestEqual(TEXT("Equipment reward cap"), TenHours.EquipmentCount, 12);
	TestEqual(TEXT("Material reward cap"), TenHours.MaterialBundleCount, 24);

	const FImmortalOfflineRewardResult ClockRollback = UImmortalOfflineRewardLibrary::Calculate(
		NowTicks + FTimespan::FromMinutes(5.0).GetTicks(),
		NowTicks,
		60,
		8.0f,
		2.0f,
		0.75f,
		2.0f,
		900.0f,
		12,
		600.0f,
		24);
	TestTrue(TEXT("Clock rollback is detected"), ClockRollback.bClockRollbackDetected);
	TestFalse(TEXT("Clock rollback grants nothing"), ClockRollback.bEligible);
	return true;
}

#endif
