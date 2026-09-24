// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPathSaveGame.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PlatformFeatures.h"
#include "SaveGameSystem.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

namespace
{
	constexpr int32 SaveUserIndex = 0;

	FString ResolvedSlotName(const FString& SlotName)
	{
		return SlotName.IsEmpty() ? UImmortalPathSaveGame::GetSlotName() : SlotName;
	}

	FString BackupSlotName(const FString& SlotName)
	{
		return SlotName + TEXT("_Backup");
	}

	ISaveGameSystem::ESaveExistsResult GetSlotExistence(const FString& SlotName)
	{
		if (ISaveGameSystem* SaveSystem = IPlatformFeaturesModule::Get().GetSaveGameSystem())
		{
			return SaveSystem->DoesSaveGameExistWithResult(*SlotName, SaveUserIndex);
		}
		return ISaveGameSystem::ESaveExistsResult::UnspecifiedError;
	}

	UImmortalPathSaveGame* ReadValidatedSaveBytes(const TArray<uint8>& Bytes)
	{
		// The engine's LoadGameFromMemory can return an object after a partial
		// property stream. Verify that its archive actually reaches a clean end.
		if (Bytes.Num() < 32) return nullptr;
		UImmortalPathSaveGame* Loaded = Cast<UImmortalPathSaveGame>(
			UGameplayStatics::LoadGameFromMemory(Bytes));
		if (!Loaded) return nullptr;
		FMemoryReader Reader = UGameplayStatics::StripSaveGameHeader(Bytes);
		if (Reader.IsError()) return nullptr;
		UImmortalPathSaveGame* Probe = NewObject<UImmortalPathSaveGame>();
		FObjectAndNameAsStringProxyArchive Archive(Reader, true);
		Probe->Serialize(Archive);
		return !Reader.IsError() && Reader.Tell() == Reader.TotalSize()
			? Loaded : nullptr;
	}

	UImmortalPathSaveGame* ReadValidatedSlot(
		const FString& SlotName,
		TArray<uint8>* OutBytes = nullptr)
	{
		TArray<uint8> Bytes;
		if (!UGameplayStatics::LoadDataFromSlot(Bytes, SlotName, SaveUserIndex))
		{
			return nullptr;
		}
		UImmortalPathSaveGame* Loaded = ReadValidatedSaveBytes(Bytes);
		if (Loaded && OutBytes) *OutBytes = MoveTemp(Bytes);
		return Loaded;
	}
#if !UE_BUILD_SHIPPING
	bool bForceDevelopmentWriteFailure = false;
	int32 DevelopmentWriteFailureAttempt = 0;
	int32 DevelopmentWriteAttemptCount = 0;
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

UImmortalPathSaveGame* UImmortalPathSaveGame::LoadOrCreate(
	const UObject* WorldContextObject,
	EImmortalSaveLoadStatus* OutStatus)
{
	(void)WorldContextObject;
	return LoadOrCreateSlot(GetSlotName(), OutStatus);
}

UImmortalPathSaveGame* UImmortalPathSaveGame::LoadOrCreateSlot(
	const FString& SlotName,
	EImmortalSaveLoadStatus* OutStatus)
{
	if (OutStatus) *OutStatus = EImmortalSaveLoadStatus::Unreadable;
	if (SlotName.IsEmpty()) return nullptr;

	const ISaveGameSystem::ESaveExistsResult Exists = GetSlotExistence(SlotName);
	if (Exists == ISaveGameSystem::ESaveExistsResult::DoesNotExist)
	{
		if (!SlotName.EndsWith(TEXT("_Backup")))
		{
			const ISaveGameSystem::ESaveExistsResult BackupExists =
				GetSlotExistence(BackupSlotName(SlotName));
			if (BackupExists != ISaveGameSystem::ESaveExistsResult::DoesNotExist)
			{
				if (BackupExists == ISaveGameSystem::ESaveExistsResult::OK && OutStatus)
				{
					// Any existing backup requires the recovery screen. The UI
					// separately checks whether automatic restore is possible.
					*OutStatus = EImmortalSaveLoadStatus::BackupAvailable;
				}
				UE_LOG(LogTemp, Error,
					TEXT("Main slot is missing but a backup exists or is inaccessible; refusing to start a new game: %s"),
					*SlotName);
				return nullptr;
			}
		}
		UImmortalPathSaveGame* Created = Cast<UImmortalPathSaveGame>(
			UGameplayStatics::CreateSaveGameObject(StaticClass()));
		if (Created && OutStatus) *OutStatus = EImmortalSaveLoadStatus::Missing;
		return Created;
	}
	if (Exists == ISaveGameSystem::ESaveExistsResult::OK)
	{
		if (UImmortalPathSaveGame* Loaded = ReadValidatedSlot(SlotName))
		{
			if (Loaded->SaveVersion > CurrentSaveVersion)
			{
				if (OutStatus) *OutStatus = EImmortalSaveLoadStatus::NewerVersion;
				UE_LOG(LogTemp, Warning, TEXT("Save slot uses newer version %d; current code is version %d"),
					Loaded->SaveVersion, CurrentSaveVersion);
			}
			else if (!SlotName.EndsWith(TEXT("_Backup")))
			{
				const FString BackupSlot = BackupSlotName(SlotName);
				const ISaveGameSystem::ESaveExistsResult BackupExists =
					GetSlotExistence(BackupSlot);
				if (BackupExists == ISaveGameSystem::ESaveExistsResult::OK)
				{
					const UImmortalPathSaveGame* Backup = ReadValidatedSlot(BackupSlot);
					if (Backup && Backup->SaveVersion > CurrentSaveVersion)
					{
						if (OutStatus) *OutStatus = EImmortalSaveLoadStatus::NewerVersion;
						UE_LOG(LogTemp, Warning,
							TEXT("Backup slot uses newer version %d; current code is version %d"),
							Backup->SaveVersion, CurrentSaveVersion);
						return Loaded;
					}
				}
				else if (BackupExists != ISaveGameSystem::ESaveExistsResult::DoesNotExist)
				{
					UE_LOG(LogTemp, Error,
						TEXT("Backup slot state is unknown; refusing to load main save: %s"),
						*BackupSlot);
					return nullptr;
				}
				if (OutStatus) *OutStatus = EImmortalSaveLoadStatus::Loaded;
			}
			else if (OutStatus)
			{
				*OutStatus = EImmortalSaveLoadStatus::Loaded;
			}
			return Loaded;
		}
	}
	UE_LOG(LogTemp, Error, TEXT("Save slot exists or is inaccessible but could not be loaded; refusing to create over it: %s"), *SlotName);
	return nullptr;
}

bool UImmortalPathSaveGame::HasRestorableBackup(const FString& SlotName)
{
	EImmortalSaveLoadStatus Status = EImmortalSaveLoadStatus::Unreadable;
	const UImmortalPathSaveGame* Backup = LoadOrCreateSlot(
		BackupSlotName(ResolvedSlotName(SlotName)), &Status);
	return Status == EImmortalSaveLoadStatus::Loaded
		&& Backup && Backup->bHasPlayerData;
}

bool UImmortalPathSaveGame::RestoreBackup(const FString& SlotName)
{
	const FString MainSlot = ResolvedSlotName(SlotName);
	const ISaveGameSystem::ESaveExistsResult MainExists = GetSlotExistence(MainSlot);
	if (MainExists == ISaveGameSystem::ESaveExistsResult::OK)
	{
		EImmortalSaveLoadStatus MainStatus = EImmortalSaveLoadStatus::Unreadable;
		LoadOrCreateSlot(MainSlot, &MainStatus);
		if (MainStatus != EImmortalSaveLoadStatus::Unreadable)
		{
			UE_LOG(LogTemp, Error, TEXT("Backup restore refused: main slot is readable: %s"), *MainSlot);
			return false;
		}
	}
	else if (MainExists != ISaveGameSystem::ESaveExistsResult::DoesNotExist)
	{
		UE_LOG(LogTemp, Error, TEXT("Backup restore refused: main slot state is unknown: %s"), *MainSlot);
		return false;
	}

	const FString BackupSlot = BackupSlotName(MainSlot);
	if (!HasRestorableBackup(MainSlot)) return false;
	TArray<uint8> BackupBytes;
	const UImmortalPathSaveGame* Backup = ReadValidatedSlot(BackupSlot, &BackupBytes);
	if (!Backup || !Backup->bHasPlayerData
		|| Backup->SaveVersion > CurrentSaveVersion)
	{
		return false;
	}
	if (!UGameplayStatics::SaveDataToSlot(BackupBytes, MainSlot, SaveUserIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("Backup restore write failed: %s"), *MainSlot);
		return false;
	}
	EImmortalSaveLoadStatus RestoredStatus = EImmortalSaveLoadStatus::Unreadable;
	const UImmortalPathSaveGame* Restored = LoadOrCreateSlot(MainSlot, &RestoredStatus);
	return RestoredStatus == EImmortalSaveLoadStatus::Loaded && Restored && Restored->bHasPlayerData;
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

void UImmortalPathSaveGame::SetDevelopmentWriteFailureOnAttempt(
	const int32 AttemptNumber)
{
	DevelopmentWriteFailureAttempt = FMath::Max(AttemptNumber, 0);
	DevelopmentWriteAttemptCount = 0;
}

void UImmortalPathSaveGame::ClearDevelopmentWriteFailureOnAttempt()
{
	DevelopmentWriteFailureAttempt = 0;
	DevelopmentWriteAttemptCount = 0;
}

int32 UImmortalPathSaveGame::GetDevelopmentWriteAttemptCount()
{
	return DevelopmentWriteAttemptCount;
}
#endif

bool UImmortalPathSaveGame::SaveToDisk(const FString& SlotName)
{
	const FString TargetSlot = ResolvedSlotName(SlotName);
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
	// Count logical save attempts only when they reach the shared disk-write
	// boundary. Failure is one-shot, before either backup or main is touched.
	if (DevelopmentWriteAttemptCount < MAX_int32)
	{
		++DevelopmentWriteAttemptCount;
	}
	if (DevelopmentWriteFailureAttempt > 0
		&& DevelopmentWriteAttemptCount == DevelopmentWriteFailureAttempt)
	{
		DevelopmentWriteFailureAttempt = 0;
		UE_LOG(LogTemp, Error,
			TEXT("Injected SaveGame logical write attempt %d failure; slot was not modified: %s"),
			DevelopmentWriteAttemptCount, *TargetSlot);
		return false;
	}
	if (bForceDevelopmentWriteFailure)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Injected SaveGame write-boundary failure; slot was not modified: %s"),
			*TargetSlot);
		return false;
	}
#endif
	const ISaveGameSystem::ESaveExistsResult Exists = GetSlotExistence(TargetSlot);
	if (Exists == ISaveGameSystem::ESaveExistsResult::OK)
	{
		// Read and validate the old bytes before replacing the only live copy.
		TArray<uint8> PreviousBytes;
		const UImmortalPathSaveGame* Previous = ReadValidatedSlot(TargetSlot, &PreviousBytes);
		if (!Previous || Previous->SaveVersion > CurrentSaveVersion)
		{
			UE_LOG(LogTemp, Error, TEXT("Save refused: existing slot is invalid or from a newer version: %s"), *TargetSlot);
			return false;
		}
		const FString BackupSlot = BackupSlotName(TargetSlot);
		const ISaveGameSystem::ESaveExistsResult BackupExists =
			GetSlotExistence(BackupSlot);
		if (BackupExists == ISaveGameSystem::ESaveExistsResult::OK)
		{
			const UImmortalPathSaveGame* ExistingBackup =
				ReadValidatedSlot(BackupSlot);
			if (ExistingBackup && ExistingBackup->SaveVersion > CurrentSaveVersion)
			{
				UE_LOG(LogTemp, Error,
					TEXT("Save refused: backup belongs to a newer version: %s"),
					*BackupSlot);
				return false;
			}
			// A damaged old backup is replaced by the verified current main.
			// This repairs backup coverage without blocking normal gameplay.
		}
		else if (BackupExists != ISaveGameSystem::ESaveExistsResult::DoesNotExist)
		{
			UE_LOG(LogTemp, Error,
				TEXT("Save refused: backup existence could not be verified: %s"),
				*BackupSlot);
			return false;
		}
		if (!UGameplayStatics::SaveDataToSlot(
			PreviousBytes, BackupSlot, SaveUserIndex))
		{
			UE_LOG(LogTemp, Error, TEXT("Save refused: previous snapshot could not be backed up: %s"), *TargetSlot);
			return false;
		}
	}
	else if (Exists != ISaveGameSystem::ESaveExistsResult::DoesNotExist)
	{
		UE_LOG(LogTemp, Error, TEXT("Save refused: slot existence could not be verified: %s"), *TargetSlot);
		return false;
	}
	else if (!TargetSlot.EndsWith(TEXT("_Backup"))
		&& GetSlotExistence(BackupSlotName(TargetSlot))
			!= ISaveGameSystem::ESaveExistsResult::DoesNotExist)
	{
		UE_LOG(LogTemp, Error,
			TEXT("Save refused: main slot is missing while a backup exists or is inaccessible: %s"),
			*TargetSlot);
		return false;
	}
	const bool bSaved = UGameplayStatics::SaveGameToSlot(this, TargetSlot, SaveUserIndex);
	if (!bSaved)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write save slot: %s"), *TargetSlot);
	}
	return bSaved;
}
