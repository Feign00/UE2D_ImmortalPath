// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPathSaveGame.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	constexpr int32 SaveUserIndex = 0;
}

UImmortalPathSaveGame::UImmortalPathSaveGame()
{
	SaveVersion = CurrentSaveVersion;
}

FString UImmortalPathSaveGame::GetSlotName()
{
	return TEXT("ImmortalPath_Main_0");
}

UImmortalPathSaveGame* UImmortalPathSaveGame::LoadOrCreate(const UObject* WorldContextObject)
{
	const FString SlotName = GetSlotName();
	if (UGameplayStatics::DoesSaveGameExist(SlotName, SaveUserIndex))
	{
		if (UImmortalPathSaveGame* Loaded = Cast<UImmortalPathSaveGame>(
			UGameplayStatics::LoadGameFromSlot(SlotName, SaveUserIndex)))
		{
			if (Loaded->SaveVersion > CurrentSaveVersion)
			{
				UE_LOG(LogTemp, Warning, TEXT("Save slot uses newer version %d; current code is version %d"),
					Loaded->SaveVersion, CurrentSaveVersion);
			}
			return Loaded;
		}
		UE_LOG(LogTemp, Warning, TEXT("Save slot exists but could not be loaded: %s"), *SlotName);
	}

	return Cast<UImmortalPathSaveGame>(UGameplayStatics::CreateSaveGameObject(StaticClass()));
}

bool UImmortalPathSaveGame::SaveToDisk()
{
	if (SaveVersion > CurrentSaveVersion)
	{
		UE_LOG(LogTemp, Error, TEXT("Refusing to overwrite newer save version %d with current version %d"),
			SaveVersion, CurrentSaveVersion);
		return false;
	}
	int32 VersionToWrite = CurrentSaveVersion;
#if !UE_BUILD_SHIPPING
	int32 TestLegacyVersion = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestLegacySaveVersion="), TestLegacyVersion)
		&& TestLegacyVersion > 0 && TestLegacyVersion < CurrentSaveVersion)
	{
		VersionToWrite = TestLegacyVersion;
		UE_LOG(LogTemp, Display, TEXT("Writing development legacy save fixture at version %d"), VersionToWrite);
	}
#endif
	SaveVersion = VersionToWrite;
	LastSavedUtcTicks = FDateTime::UtcNow().GetTicks();
	const bool bSaved = UGameplayStatics::SaveGameToSlot(this, GetSlotName(), SaveUserIndex);
	if (!bSaved)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write save slot: %s"), *GetSlotName());
	}
	return bSaved;
}
