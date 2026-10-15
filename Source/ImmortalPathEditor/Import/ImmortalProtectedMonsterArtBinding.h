// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/** Shared editor-only driver for fixed, protected monster-art bindings. */
namespace ImmortalProtectedMonsterArtBinding
{
	struct FConfig
	{
		const TCHAR* LegacyPath;
		const TCHAR* LegacyPackage;
		const TCHAR* BlueprintPath;
		const TCHAR* BlueprintPackage;
		const TCHAR* BlueprintName;
		const TCHAR* AssetDirectory;
		const TCHAR* FlipbookPrefix;
		FVector2D FrameSize;
		FVector2D FootPivot;
		float PixelsPerUnit;
		float Windup;
		float HurtDuration;
		float VisualScale;
		float HealthBarHeightAboveFeet;
		float FramesPerSecond[5];
		bool bReplaceReactionTimings;
		bool bPreserveHurtClipDuration;
		bool bPreserveAuthoredMapBossPalette;
	};

	/** No caller-supplied paths. Preflight all assets, preserve the source, save only the validated destination. */
	int32 Run(const FString& Params, const FConfig& Config);
}
