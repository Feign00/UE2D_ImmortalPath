// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ImmortalDesktopSettings.generated.h"

/**
 * Small, user-owned configuration for the Taskbar Hero style desktop window.
 * It lives in GameUserSettings.ini and is deliberately separate from the
 * versioned RPG SaveGame slot.
 */
UCLASS(Config = GameUserSettings)
class IMMORTALPATH_API UImmortalDesktopSettings : public UObject
{
	GENERATED_BODY()

public:
	static constexpr int32 MinimumWindowHeight = 180;
	static constexpr int32 MaximumWindowHeight = 600;
	static constexpr int32 LowFrameRateLimit = 30;
	static constexpr int32 HighFrameRateLimit = 60;

	static UImmortalDesktopSettings* GetMutable();

	void LoadFromDisk();
	void SaveToDisk();
	bool Normalize();
	void CycleFrameRateLimit();

	UPROPERTY(Config, EditAnywhere, Category = "Desktop")
	bool bAlwaysOnTop = true;

	UPROPERTY(Config, EditAnywhere, Category = "Desktop")
	bool bTransparentBackground = true;

	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	bool bMuted = false;

	UPROPERTY(Config, EditAnywhere, Category = "Performance")
	int32 FrameRateLimit = HighFrameRateLimit;

	UPROPERTY(Config, EditAnywhere, Category = "Desktop")
	int32 WindowHeight = 320;
};
