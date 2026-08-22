// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPathSaveGame.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	constexpr int32 SaveUserIndex = 0;
#if !UE_BUILD_SHIPPING
	bool bForceDevelopmentWriteFailure = false;
#endif
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

#if !UE_BUILD_SHIPPING
void UImmortalPathSaveGame::SetDevelopmentWriteFailure(
	const bool bShouldFail)
{
	bForceDevelopmentWriteFailure = bShouldFail;
}

bool UImmortalPathSaveGame::IsDevelopmentWriteFailureEnabled()
{
	return bForceDevelopmentWriteFailure;
}
#endif

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
		// Apply development schema fixtures at the final write boundary. The
		// player, map spawner and shutdown path all write this shared slot.
		if (TestLegacyVersion < 16) bInventoryManagementInitialized = false;
		if (TestLegacyVersion < 17) bEquipmentExpansionInitialized = false;
		if (TestLegacyVersion < 18)
		{
			bWorldBossInitialized = false;
			WorldBossState = FImmortalWorldBossState();
		}
		if (TestLegacyVersion < 19)
		{
			bEndlessDungeonInitialized = false;
			EndlessDungeonState = FImmortalEndlessDungeonState();
		}
		if (TestLegacyVersion < 20)
		{
			bPetSystemInitialized = false;
			PetState = FImmortalPetState();
		}
		if (TestLegacyVersion < 21)
		{
			bAscensionSystemInitialized = false;
			AscensionState = FImmortalAscensionState();
		}
		if (TestLegacyVersion < 22)
		{
			bQuestSystemInitialized = false;
			QuestState = FImmortalQuestState();
		}
		if (TestLegacyVersion < 23)
		{
			bDeathCultivationRecoveryRequired = false;
		}
		UE_LOG(LogTemp, Display, TEXT("Writing development legacy save fixture at version %d"), VersionToWrite);
	}
	if (FParse::Param(
		FCommandLine::Get(),
		TEXT("ImmortalTestEndlessMarkerMismatch")))
	{
		bEndlessDungeonInitialized = false;
		EndlessDungeonState.bInitialized = false;
	}
	if (FParse::Param(
		FCommandLine::Get(),
		TEXT("ImmortalTestPetMarkerMismatch")))
	{
		bPetSystemInitialized = false;
		PetState.bInitialized = false;
	}
	if (FParse::Param(
		FCommandLine::Get(),
		TEXT("ImmortalTestAscensionMarkerMismatch")))
	{
		bAscensionSystemInitialized = false;
		AscensionState.bInitialized = false;
	}
	if (FParse::Param(
		FCommandLine::Get(),
		TEXT("ImmortalTestQuestMarkerMismatch")))
	{
		bQuestSystemInitialized = false;
		QuestState.bInitialized = false;
	}
#endif
	SaveVersion = VersionToWrite;
	LastSavedUtcTicks = FDateTime::UtcNow().GetTicks();
#if !UE_BUILD_SHIPPING
	if (bForceDevelopmentWriteFailure)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Injected SaveGame write-boundary failure; slot was not modified: %s"),
			*GetSlotName());
		return false;
	}
#endif
	const bool bSaved = UGameplayStatics::SaveGameToSlot(this, GetSlotName(), SaveUserIndex);
	if (!bSaved)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write save slot: %s"), *GetSlotName());
	}
	return bSaved;
}
