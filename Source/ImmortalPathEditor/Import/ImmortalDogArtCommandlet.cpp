// Copyright Epic Games, Inc. All Rights Reserved.
#include "ImmortalDogArtCommandlet.h"
#include "ImmortalProtectedMonsterArtBinding.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImmortalDogArtCommandlet)

namespace ImmortalDogArt
{
	const ImmortalProtectedMonsterArtBinding::FConfig Config
	{
		TEXT("/Game/GAME/BP_DogMonster.BP_DogMonster"),
		TEXT("/Game/GAME/BP_DogMonster"),
		TEXT("/Game/GAME/BP_DesktopDogMonster.BP_DesktopDogMonster"),
		TEXT("/Game/GAME/BP_DesktopDogMonster"),
		TEXT("BP_DesktopDogMonster"),
		TEXT("/Game/GAME/Asset/Monsters/desktop_pixel_v2/dog/"),
		TEXT("FB_Dog_Pixel_"),
		FVector2D(512.0, 384.0),
		FVector2D(256.0, 336.0),
		2.56f,
		0.25f,
		8.0f / 12.0f,
		2.0f,
		185.0f,
		{ 8.0f, 8.0f, 12.0f, 12.0f, 10.0f },
		true,
		false,
		false
	};
}

UImmortalDogArtCommandlet::UImmortalDogArtCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UImmortalDogArtCommandlet::Main(const FString& Params)
{
	return ImmortalProtectedMonsterArtBinding::Run(Params, ImmortalDogArt::Config);
}
