// Copyright Epic Games, Inc. All Rights Reserved.
#include "ImmortalQingyunBossArtCommandlet.h"
#include "ImmortalProtectedMonsterArtBinding.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImmortalQingyunBossArtCommandlet)

namespace ImmortalQingyunBossArt
{
	const ImmortalProtectedMonsterArtBinding::FConfig Config
	{
		TEXT("/Game/GAME/BP_DogMonster.BP_DogMonster"),
		TEXT("/Game/GAME/BP_DogMonster"),
		TEXT("/Game/GAME/BP_DesktopQingyunBoss.BP_DesktopQingyunBoss"),
		TEXT("/Game/GAME/BP_DesktopQingyunBoss"),
		TEXT("BP_DesktopQingyunBoss"),
		TEXT("/Game/GAME/Asset/DesktopPixelV2/QingyunBoss/"),
		TEXT("FB_QingyunBoss_"),
		FVector2D(512.0, 384.0),
		FVector2D(256.0, 336.0),
		2.56f,
		0.35f,
		0.35f,
		2.0f,
		275.0f,
		{ 8.0f, 8.0f, 12.0f, 120.0f / 7.0f, 10.0f },
		false,
		true,
		true
	};
}

UImmortalQingyunBossArtCommandlet::UImmortalQingyunBossArtCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UImmortalQingyunBossArtCommandlet::Main(const FString& Params)
{
	return ImmortalProtectedMonsterArtBinding::Run(Params, ImmortalQingyunBossArt::Config);
}
