// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalDesktopSettings.h"

#include "Misc/ConfigCacheIni.h"

UImmortalDesktopSettings* UImmortalDesktopSettings::GetMutable()
{
	return GetMutableDefault<UImmortalDesktopSettings>();
}

void UImmortalDesktopSettings::LoadFromDisk()
{
	LoadConfig(nullptr, *GGameUserSettingsIni);
	const bool bRepaired = Normalize();
	if (bRepaired)
	{
		SaveToDisk();
	}
}

void UImmortalDesktopSettings::SaveToDisk()
{
	Normalize();
	SaveConfig(CPF_Config, *GGameUserSettingsIni);
}

bool UImmortalDesktopSettings::Normalize()
{
	const int32 PreviousFrameRateLimit = FrameRateLimit;
	const int32 PreviousWindowHeight = WindowHeight;
	FrameRateLimit =
		FrameRateLimit <= (LowFrameRateLimit + HighFrameRateLimit) / 2
			? LowFrameRateLimit
			: HighFrameRateLimit;
	WindowHeight = FMath::Clamp(
		WindowHeight,
		MinimumWindowHeight,
		MaximumWindowHeight);
	return PreviousFrameRateLimit != FrameRateLimit
		|| PreviousWindowHeight != WindowHeight;
}

void UImmortalDesktopSettings::CycleFrameRateLimit()
{
	Normalize();
	FrameRateLimit =
		FrameRateLimit == LowFrameRateLimit
			? HighFrameRateLimit
			: LowFrameRateLimit;
}
