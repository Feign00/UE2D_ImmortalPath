// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalDesktopSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalDesktopSettingsNormalizationTest,
	"ImmortalPath.Settings.DesktopNormalization",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FImmortalDesktopSettingsNormalizationTest::RunTest(
	const FString& Parameters)
{
	UImmortalDesktopSettings* Settings =
		NewObject<UImmortalDesktopSettings>();
	TestNotNull(TEXT("Transient settings object exists"), Settings);
	if (!Settings)
	{
		return false;
	}

	Settings->FrameRateLimit = 1;
	Settings->WindowHeight = 20;
	TestTrue(TEXT("Invalid low values are repaired"), Settings->Normalize());
	TestEqual(
		TEXT("Low frame rate is clamped to 30"),
		Settings->FrameRateLimit,
		UImmortalDesktopSettings::LowFrameRateLimit);
	TestEqual(
		TEXT("Window height respects its minimum"),
		Settings->WindowHeight,
		UImmortalDesktopSettings::MinimumWindowHeight);

	Settings->FrameRateLimit = 999;
	Settings->WindowHeight = 9999;
	TestTrue(TEXT("Invalid high values are repaired"), Settings->Normalize());
	TestEqual(
		TEXT("High frame rate is clamped to 60"),
		Settings->FrameRateLimit,
		UImmortalDesktopSettings::HighFrameRateLimit);
	TestEqual(
		TEXT("Window height respects its maximum"),
		Settings->WindowHeight,
		UImmortalDesktopSettings::MaximumWindowHeight);

	Settings->CycleFrameRateLimit();
	TestEqual(
		TEXT("Frame-rate cycle reaches 30"),
		Settings->FrameRateLimit,
		UImmortalDesktopSettings::LowFrameRateLimit);
	Settings->CycleFrameRateLimit();
	TestEqual(
		TEXT("Frame-rate cycle returns to 60"),
		Settings->FrameRateLimit,
		UImmortalDesktopSettings::HighFrameRateLimit);
	TestFalse(
		TEXT("Canonical settings are idempotent"),
		Settings->Normalize());
	return true;
}

#endif
