#pragma once

#include "CoreMinimal.h"

class AImmortalPlayerCharacter;

/** Shared non-shipping battle-animation probes; an animal wrapper supplies only identity and paths. */
namespace ImmortalBattleAnimationFixture
{
	struct FConfig
	{
		const TCHAR* AnimalName;
		const TCHAR* ControlledFlag;
		const TCHAR* WalkingFlag;
		const TCHAR* UserDirectoryRoot;
		const TCHAR* MonsterClassPath;
		const TCHAR* PetId;
		const TCHAR* FlipbookDirectory;
		const TCHAR* FlipbookPrefixPath;
		const TCHAR* LogPrefix;
		const TCHAR* ScreenshotPrefix;
		bool bRangedPet;
		float SetupDelaySeconds;
	};

	/** Must run before player BeginPlay initialization or any save access. Rejects mixed animal/mode flags. */
	bool CheckPreflightIfRequested(const FConfig& Config);
	void StartIfRequested(AImmortalPlayerCharacter& Player, const FConfig& Config);
}
