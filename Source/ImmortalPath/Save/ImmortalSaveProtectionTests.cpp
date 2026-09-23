#include "ImmortalPathSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UserSettings/EnhancedInputUserSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalSaveProtectionTest,
	"ImmortalPath.Save.UnreadableSlotProtectionAndBackupRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalSaveProtectionTest::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("Backup restore refused: main slot is readable"),
		EAutomationExpectedErrorFlags::Contains, 3);
	AddExpectedError(TEXT("Save slot exists or is inaccessible but could not be loaded"),
		EAutomationExpectedErrorFlags::Contains, 6);
	AddExpectedError(TEXT("Save refused: existing slot is invalid or from a newer version"),
		EAutomationExpectedErrorFlags::Contains, 4);
	AddExpectedError(TEXT("Save refused: backup belongs to a newer version"),
		EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Save slot uses newer version 24"),
		EAutomationExpectedErrorFlags::Contains, 3);
	AddExpectedError(TEXT("Backup slot uses newer version 24"),
		EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Main slot is missing but a backup exists or is inaccessible"),
		EAutomationExpectedErrorFlags::Contains, 3);
	AddExpectedError(TEXT("Save refused: main slot is missing while a backup exists or is inaccessible"),
		EAutomationExpectedErrorFlags::Contains, 3);

	const FString Slot = TEXT("ImmortalPath_SaveProtectionTest_")
		+ FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString BackupSlot = Slot + TEXT("_Backup");
	ON_SCOPE_EXIT
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
		UGameplayStatics::DeleteGameInSlot(BackupSlot, 0);
	};

	EImmortalSaveLoadStatus Status = EImmortalSaveLoadStatus::Unreadable;
	UImmortalPathSaveGame* Save = UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status);
	if (!Save)
	{
		AddError(TEXT("An isolated new slot could not be created"));
		return false;
	}
	TestEqual(TEXT("Only an absent slot starts new progress"),
		Status, EImmortalSaveLoadStatus::Missing);
	Save->bHasPlayerData = true;
	Save->PlayerHealth = 91.0f;
	TestTrue(TEXT("First generation is persisted"), Save->SaveToDisk(Slot));
	TestFalse(TEXT("First generation has no previous backup"),
		UImmortalPathSaveGame::HasRestorableBackup(Slot));

	Save->PlayerHealth = 37.0f;
	TestTrue(TEXT("Second generation is persisted"), Save->SaveToDisk(Slot));
	UImmortalPathSaveGame* Current = UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status);
	TestTrue(TEXT("Current generation loads normally"),
		Current && Status == EImmortalSaveLoadStatus::Loaded
		&& Current->PlayerHealth == 37.0f);
	TestTrue(TEXT("Previous generation is recoverable"),
		UImmortalPathSaveGame::HasRestorableBackup(Slot));
	UImmortalPathSaveGame* Backup = UImmortalPathSaveGame::LoadOrCreateSlot(BackupSlot, &Status);
	TestTrue(TEXT("Backup holds the previous snapshot"),
		Backup && Status == EImmortalSaveLoadStatus::Loaded
		&& Backup->PlayerHealth == 91.0f);
	TestFalse(TEXT("Recovery cannot replace a readable main slot"),
		UImmortalPathSaveGame::RestoreBackup(Slot));
	TArray<uint8> FullBytes;
	TestTrue(TEXT("Current bytes are available for truncation checks"),
		UGameplayStatics::LoadDataFromSlot(FullBytes, Slot, 0));

	// A truncated, nonempty file exists at the same slot. Neither normal load
	// nor the final write boundary may silently turn it into a new game.
	const TArray<uint8> TruncatedBytes = { 0x47, 0x56, 0x41, 0x53, 0x01 };
	TestTrue(TEXT("Truncated fixture is stored in its isolated slot"),
		UGameplayStatics::SaveDataToSlot(TruncatedBytes, Slot, 0));
	Status = EImmortalSaveLoadStatus::Missing;
	TestNull(TEXT("Truncated slot never creates a fresh save"),
		UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status));
	TestEqual(TEXT("Truncated slot is reported unreadable"),
		Status, EImmortalSaveLoadStatus::Unreadable);
	TestFalse(TEXT("A stale in-memory save cannot overwrite damaged bytes"),
		Save->SaveToDisk(Slot));
	TArray<uint8> StillDamaged;
	TestTrue(TEXT("Damaged slot remains present"),
		UGameplayStatics::LoadDataFromSlot(StillDamaged, Slot, 0));
	TestTrue(TEXT("Damaged bytes remain unchanged"),
		StillDamaged == TruncatedBytes);
	TestTrue(TEXT("Explicit recovery restores the last valid generation"),
		UImmortalPathSaveGame::RestoreBackup(Slot));
	Current = UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status);
	TestTrue(TEXT("Restored progress is readable after reopening"),
		Current && Status == EImmortalSaveLoadStatus::Loaded
		&& Current->PlayerHealth == 91.0f);
	TestFalse(TEXT("Recovery cannot repeat over the restored main slot"),
		UImmortalPathSaveGame::RestoreBackup(Slot));

	// Keep the real UE header and most properties, but remove the tail.
	// This exercises archive/EOF validation rather than the short-buffer guard.
	if (FullBytes.Num() <= 96)
	{
		AddError(TEXT("The saved fixture is too short for a tail-truncation check"));
		return false;
	}
	TArray<uint8> LateTruncatedBytes = FullBytes;
	LateTruncatedBytes.SetNum(FullBytes.Num() - 64);
	TestTrue(TEXT("Tail-truncated UE save is stored"),
		UGameplayStatics::SaveDataToSlot(LateTruncatedBytes, Slot, 0));
	Status = EImmortalSaveLoadStatus::Missing;
	TestNull(TEXT("Incomplete property stream is rejected"),
		UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status));
	TestEqual(TEXT("Tail truncation reports unreadable"),
		Status, EImmortalSaveLoadStatus::Unreadable);
	TestFalse(TEXT("Tail truncation is not overwritten by a stale object"),
		Save->SaveToDisk(Slot));
	TestTrue(TEXT("Backup recovers from tail truncation"),
		UImmortalPathSaveGame::RestoreBackup(Slot));

	// A well-formed UE save belonging to another USaveGame class is not a
	// missing slot and must not be replaced by this game's stale object.
	UEnhancedInputUserSettings* WrongType = NewObject<UEnhancedInputUserSettings>();
	TestTrue(TEXT("Wrong SaveGame class fixture is stored"),
		UGameplayStatics::SaveGameToSlot(WrongType, Slot, 0));
	TArray<uint8> WrongTypeBytes;
	TestTrue(TEXT("Wrong-type bytes are available for preservation checks"),
		UGameplayStatics::LoadDataFromSlot(WrongTypeBytes, Slot, 0));
	Status = EImmortalSaveLoadStatus::Missing;
	TestNull(TEXT("Wrong SaveGame class is rejected"),
		UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status));
	TestEqual(TEXT("Wrong SaveGame class is reported unreadable"),
		Status, EImmortalSaveLoadStatus::Unreadable);
	TestFalse(TEXT("Stale object cannot overwrite a wrong-type save"),
		Save->SaveToDisk(Slot));
	TArray<uint8> PreservedWrongTypeBytes;
	TestTrue(TEXT("Wrong-type save remains present"),
		UGameplayStatics::LoadDataFromSlot(PreservedWrongTypeBytes, Slot, 0));
	TestTrue(TEXT("Wrong-type save bytes remain unchanged"),
		PreservedWrongTypeBytes == WrongTypeBytes);
	TestTrue(TEXT("Main fixture is restored after wrong-type checks"),
		UGameplayStatics::SaveDataToSlot(FullBytes, Slot, 0));

	// A missing main with a valid backup must not silently start a new game.
	TestTrue(TEXT("Main slot can be removed inside the isolated fixture"),
		UGameplayStatics::DeleteGameInSlot(Slot, 0));
	Status = EImmortalSaveLoadStatus::Missing;
	TestNull(TEXT("Missing main with a backup does not create new progress"),
		UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status));
	TestEqual(TEXT("Backup presence is reported for recovery"),
		Status, EImmortalSaveLoadStatus::BackupAvailable);
	TestFalse(TEXT("A stale object cannot overwrite a recoverable missing main"),
		Save->SaveToDisk(Slot));
	TestTrue(TEXT("Backup remains available after refused new save"),
		UImmortalPathSaveGame::HasRestorableBackup(Slot));
	TestTrue(TEXT("Explicit recovery also recreates a missing main"),
		UImmortalPathSaveGame::RestoreBackup(Slot));

	// An unusable backup still belongs to the player. Do not replace it with
	// new-game bytes just because automatic restoration is unavailable.
	TestTrue(TEXT("Damaged backup fixture is stored"),
		UGameplayStatics::SaveDataToSlot(TruncatedBytes, BackupSlot, 0));
	TestTrue(TEXT("Readable main replaces a damaged old backup on save"),
		Save->SaveToDisk(Slot));
	TestTrue(TEXT("Backup is readable again after repair"),
		UImmortalPathSaveGame::HasRestorableBackup(Slot));
	TestTrue(TEXT("Damaged backup fixture is restored for missing-main checks"),
		UGameplayStatics::SaveDataToSlot(TruncatedBytes, BackupSlot, 0));
	TestTrue(TEXT("Main is removed beside the damaged backup"),
		UGameplayStatics::DeleteGameInSlot(Slot, 0));
	Status = EImmortalSaveLoadStatus::Missing;
	TestNull(TEXT("Missing main with damaged backup cannot create new progress"),
		UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status));
	TestEqual(TEXT("Existing damaged backup still requires a recovery decision"),
		Status, EImmortalSaveLoadStatus::BackupAvailable);
	TestFalse(TEXT("Damaged backup cannot be restored automatically"),
		UImmortalPathSaveGame::HasRestorableBackup(Slot));
	TestFalse(TEXT("New-game save cannot replace a damaged backup"),
		Save->SaveToDisk(Slot));
	TArray<uint8> PreservedBackupBytes;
	TestTrue(TEXT("Damaged backup remains present"),
		UGameplayStatics::LoadDataFromSlot(PreservedBackupBytes, BackupSlot, 0));
	TestTrue(TEXT("Damaged backup bytes remain unchanged"),
		PreservedBackupBytes == TruncatedBytes);
	TestTrue(TEXT("Main fixture is restored after the damaged-backup checks"),
		UGameplayStatics::SaveDataToSlot(FullBytes, Slot, 0));

	// A future schema is not damage: it remains readable, but old code must
	// never overwrite it or replace it with an older backup.
	if (!Current)
	{
		AddError(TEXT("The restored fixture could not be inspected"));
		return false;
	}
	Current->SaveVersion = UImmortalPathSaveGame::CurrentSaveVersion + 1;
	TestTrue(TEXT("Future-version fixture is stored"),
		UGameplayStatics::SaveGameToSlot(Current, Slot, 0));
	UImmortalPathSaveGame* Future = UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status);
	TestTrue(TEXT("Future-version slot is identified"),
		Future && Status == EImmortalSaveLoadStatus::NewerVersion);
	TestFalse(TEXT("Old code cannot overwrite a future-version slot"),
		Save->SaveToDisk(Slot));
	TestFalse(TEXT("Backup cannot replace a readable future-version slot"),
		UImmortalPathSaveGame::RestoreBackup(Slot));

	TestTrue(TEXT("Future-version backup fixture is stored"),
		UGameplayStatics::SaveGameToSlot(Future, BackupSlot, 0));
	TArray<uint8> FutureBackupBytes;
	TestTrue(TEXT("Future-version backup bytes can be inspected"),
		UGameplayStatics::LoadDataFromSlot(FutureBackupBytes, BackupSlot, 0));
	TestTrue(TEXT("Readable current-version main fixture is restored"),
		UGameplayStatics::SaveDataToSlot(FullBytes, Slot, 0));
	Status = EImmortalSaveLoadStatus::Loaded;
	TestTrue(TEXT("Readable main with a future-version backup is detected at startup"),
		UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status)
			&& Status == EImmortalSaveLoadStatus::NewerVersion);
	TestFalse(TEXT("Readable main cannot replace a future-version backup"),
		Save->SaveToDisk(Slot));
	TArray<uint8> PreservedMainBytes;
	TestTrue(TEXT("Current-version main remains present after refused save"),
		UGameplayStatics::LoadDataFromSlot(PreservedMainBytes, Slot, 0));
	TestTrue(TEXT("Current-version main bytes remain unchanged"),
		PreservedMainBytes == FullBytes);
	PreservedBackupBytes.Reset();
	TestTrue(TEXT("Future-version backup remains present after refused save"),
		UGameplayStatics::LoadDataFromSlot(PreservedBackupBytes, BackupSlot, 0));
	TestTrue(TEXT("Future-version backup bytes remain unchanged after refused save"),
		PreservedBackupBytes == FutureBackupBytes);
	TestTrue(TEXT("Main is removed beside the future-version backup"),
		UGameplayStatics::DeleteGameInSlot(Slot, 0));
	Status = EImmortalSaveLoadStatus::Missing;
	TestNull(TEXT("Missing main with future-version backup cannot create new progress"),
		UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status));
	TestEqual(TEXT("Future-version backup still requires a recovery decision"),
		Status, EImmortalSaveLoadStatus::BackupAvailable);
	TestFalse(TEXT("Future-version backup cannot be restored by old code"),
		UImmortalPathSaveGame::HasRestorableBackup(Slot));
	TestFalse(TEXT("New-game save cannot replace a future-version backup"),
		Save->SaveToDisk(Slot));
	PreservedBackupBytes.Reset();
	TestTrue(TEXT("Future-version backup remains present"),
		UGameplayStatics::LoadDataFromSlot(PreservedBackupBytes, BackupSlot, 0));
	TestTrue(TEXT("Future-version backup bytes remain unchanged"),
		PreservedBackupBytes == FutureBackupBytes);

	// Explicit development fixture for native UI verification. The main slot
	// is touched only inside a dedicated isolated UserDir and only when empty.
	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestPrepareSaveRecoveryFixture")))
	{
		if (!UGameplayStatics::SaveDataToSlot(FullBytes, BackupSlot, 0))
		{
			AddError(TEXT("Isolated UI fixture could not restore a current-version backup"));
			return false;
		}
		EImmortalSaveLoadStatus FixtureBackupStatus = EImmortalSaveLoadStatus::Unreadable;
		const UImmortalPathSaveGame* FixtureBackup =
			UImmortalPathSaveGame::LoadOrCreateSlot(BackupSlot, &FixtureBackupStatus);
		if (!FixtureBackup || FixtureBackupStatus != EImmortalSaveLoadStatus::Loaded
			|| !FixtureBackup->bHasPlayerData)
		{
			AddError(TEXT("Recovery UI fixture backup is not a current-version player save"));
			return false;
		}
		FString UserDir;
		const FString FixtureRoot = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectDir() / TEXT("Saved/Automation/SaveProtection"));
		if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir)
			|| !FPaths::IsUnderDirectory(
				FPaths::ConvertRelativePathToFull(UserDir), FixtureRoot))
		{
			AddError(TEXT("Recovery fixture requires an isolated SaveProtection UserDir"));
			return false;
		}
		const FString MainSlot = UImmortalPathSaveGame::GetSlotName();
		if (UGameplayStatics::DoesSaveGameExist(MainSlot, 0)
			|| UGameplayStatics::DoesSaveGameExist(MainSlot + TEXT("_Backup"), 0))
		{
			AddError(TEXT("Recovery fixture refuses to replace an existing main or backup slot"));
			return false;
		}
		TArray<uint8> ValidBackupBytes;
		if (!UGameplayStatics::LoadDataFromSlot(ValidBackupBytes, BackupSlot, 0)
			|| !UGameplayStatics::SaveDataToSlot(
				ValidBackupBytes, MainSlot + TEXT("_Backup"), 0)
			|| !UGameplayStatics::SaveDataToSlot(TruncatedBytes, MainSlot, 0))
		{
			AddError(TEXT("Could not prepare the isolated recovery fixture"));
			return false;
		}
		UE_LOG(LogTemp, Display,
			TEXT("Isolated recovery fixture prepared: unreadable main and valid backup"));
	}
	return true;
}

#endif
