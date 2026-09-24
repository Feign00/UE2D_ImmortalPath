#include "ImmortalPathSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"

namespace
{
	void SetCombatSnapshot(UImmortalPathSaveGame& Save, const int32 Sequence)
	{
		Save.bHasPlayerData = true;
		Save.bHasStageData = true;
		Save.QingyunStage = 1;
		Save.QingyunStageKills = Sequence;
		Save.MapSystemState = UImmortalMapLibrary::CreateMigratedState(1, Sequence, false);

		Save.bQuestSystemInitialized = true;
		Save.QuestState.bInitialized = true;
		Save.QuestState.LifetimeCounters.MonsterKills = Sequence;
		Save.QuestState.DailyCounters.MonsterKills = Sequence;

		Save.SectState.bInitialized = true;
		Save.SectState.DailyTasks.Reset();
		FImmortalSectTaskProgress& Task = Save.SectState.DailyTasks.AddDefaulted_GetRef();
		Task.TaskId = TEXT("R02CombatTask");
		Task.Progress = Sequence;

		Save.bPetSystemInitialized = true;
		Save.PetState.bInitialized = true;
		Save.PetState.TotalCombatKills = Sequence;
		Save.PetState.Pets.Reset();
		FImmortalPetProgress& Pet = Save.PetState.Pets.AddDefaulted_GetRef();
		Pet.PetId = TEXT("R02CombatPet");
		Pet.bOwned = true;
		Pet.Experience = Sequence;
		Pet.TotalCombatKills = Sequence;
	}

	bool HasCombatSnapshot(const UImmortalPathSaveGame* Save, const int32 Sequence)
	{
		if (!Save || Save->QingyunStageKills != Sequence
			|| Save->QuestState.LifetimeCounters.MonsterKills != Sequence
			|| Save->QuestState.DailyCounters.MonsterKills != Sequence
			|| Save->SectState.DailyTasks.Num() != 1
			|| Save->SectState.DailyTasks[0].Progress != Sequence
			|| Save->PetState.TotalCombatKills != Sequence
			|| Save->PetState.Pets.Num() != 1
			|| Save->PetState.Pets[0].Experience != Sequence
			|| Save->PetState.Pets[0].TotalCombatKills != Sequence)
		{
			return false;
		}
		FImmortalMapProgress Progress;
		return UImmortalMapLibrary::GetMapProgress(
			Save->MapSystemState, UImmortalMapLibrary::GetQingyunMountainId(), Progress)
			&& Progress.Stage == 1 && Progress.StageKills == Sequence;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalMapCombatLogicalWriteTest,
	"ImmortalPath.Save.MapCombatLogicalWriteFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalMapCombatLogicalWriteTest::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("Injected SaveGame logical write attempt"),
		EAutomationExpectedErrorFlags::Contains, 2);

	const FString Slot = TEXT("ImmortalPath_MapCombatWriteTest_")
		+ FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString BackupSlot = Slot + TEXT("_Backup");
	const bool bPreviousForcedFailure =
		UImmortalPathSaveGame::IsDevelopmentWriteFailureEnabled();
	UImmortalPathSaveGame::SetDevelopmentWriteFailure(false);
	UImmortalPathSaveGame::ClearDevelopmentWriteFailureOnAttempt();
	ON_SCOPE_EXIT
	{
		UImmortalPathSaveGame::ClearDevelopmentWriteFailureOnAttempt();
		UImmortalPathSaveGame::SetDevelopmentWriteFailure(bPreviousForcedFailure);
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
		UGameplayStatics::DeleteGameInSlot(BackupSlot, 0);
	};

	EImmortalSaveLoadStatus Status = EImmortalSaveLoadStatus::Unreadable;
	UImmortalPathSaveGame* Save = UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status);
	if (!Save || Status != EImmortalSaveLoadStatus::Missing)
	{
		AddError(TEXT("An isolated map-combat test slot could not be created"));
		return false;
	}
	SetCombatSnapshot(*Save, 1);
	TestTrue(TEXT("Baseline map/quest/sect/pet snapshot is persisted"),
		Save->SaveToDisk(Slot));
	TArray<uint8> BaselineBytes;
	TestTrue(TEXT("Baseline bytes are readable"),
		UGameplayStatics::LoadDataFromSlot(BaselineBytes, Slot, 0));

	// The first logical write fails before either the main or backup is changed.
	UImmortalPathSaveGame::SetDevelopmentWriteFailureOnAttempt(1);
	SetCombatSnapshot(*Save, 2);
	TestFalse(TEXT("Attempt 1 is rejected"), Save->SaveToDisk(Slot));
	TestEqual(TEXT("Exactly one logical save was attempted"),
		UImmortalPathSaveGame::GetDevelopmentWriteAttemptCount(), 1);
	TArray<uint8> AfterFirstFailure;
	TestTrue(TEXT("Main bytes remain readable after attempt 1 fails"),
		UGameplayStatics::LoadDataFromSlot(AfterFirstFailure, Slot, 0));
	TestTrue(TEXT("Attempt 1 leaves the original bytes unchanged"),
		AfterFirstFailure == BaselineBytes);
	TestFalse(TEXT("Failed first overwrite does not create a backup"),
		UGameplayStatics::DoesSaveGameExist(BackupSlot, 0));
	TestTrue(TEXT("Restart after attempt 1 sees the complete prior snapshot"),
		HasCombatSnapshot(UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status), 1)
		&& Status == EImmortalSaveLoadStatus::Loaded);

	TestTrue(TEXT("The one-shot failure allows the next write"), Save->SaveToDisk(Slot));
	TestEqual(TEXT("One successful retry adds one logical save"),
		UImmortalPathSaveGame::GetDevelopmentWriteAttemptCount(), 2);
	TestTrue(TEXT("Restart after retry sees map, quest, sect and pet together"),
		HasCombatSnapshot(UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status), 2)
		&& Status == EImmortalSaveLoadStatus::Loaded);

	// The second attempt in a new sequence fails without producing a mixed
	// generation: all four systems stay at the first successful generation.
	UImmortalPathSaveGame::SetDevelopmentWriteFailureOnAttempt(2);
	SetCombatSnapshot(*Save, 3);
	TestTrue(TEXT("Attempt 1 of the second sequence commits"), Save->SaveToDisk(Slot));
	TestEqual(TEXT("First write in the second sequence is counted once"),
		UImmortalPathSaveGame::GetDevelopmentWriteAttemptCount(), 1);
	TArray<uint8> BeforeSecondFailure;
	TArray<uint8> BackupBeforeSecondFailure;
	TestTrue(TEXT("Committed main bytes are readable"),
		UGameplayStatics::LoadDataFromSlot(BeforeSecondFailure, Slot, 0));
	TestTrue(TEXT("Previous generation backup bytes are readable"),
		UGameplayStatics::LoadDataFromSlot(BackupBeforeSecondFailure, BackupSlot, 0));
	SetCombatSnapshot(*Save, 4);
	TestFalse(TEXT("Attempt 2 of the second sequence is rejected"),
		Save->SaveToDisk(Slot));
	TestEqual(TEXT("A failed second write is counted as one logical attempt"),
		UImmortalPathSaveGame::GetDevelopmentWriteAttemptCount(), 2);
	TArray<uint8> AfterSecondFailure;
	TArray<uint8> BackupAfterSecondFailure;
	TestTrue(TEXT("Main bytes remain readable after attempt 2 fails"),
		UGameplayStatics::LoadDataFromSlot(AfterSecondFailure, Slot, 0));
	TestTrue(TEXT("Backup bytes remain readable after attempt 2 fails"),
		UGameplayStatics::LoadDataFromSlot(BackupAfterSecondFailure, BackupSlot, 0));
	TestTrue(TEXT("Attempt 2 does not change the committed main"),
		AfterSecondFailure == BeforeSecondFailure);
	TestTrue(TEXT("Attempt 2 does not change the previous backup"),
		BackupAfterSecondFailure == BackupBeforeSecondFailure);
	TestTrue(TEXT("Restart sees no mixed map/quest/sect/pet generation"),
		HasCombatSnapshot(UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status), 3)
		&& Status == EImmortalSaveLoadStatus::Loaded);

	TestTrue(TEXT("The N=2 failure is one-shot"), Save->SaveToDisk(Slot));
	TestEqual(TEXT("One-shot recovery has exactly three logical attempts"),
		UImmortalPathSaveGame::GetDevelopmentWriteAttemptCount(), 3);
	TestTrue(TEXT("Restart after the final write sees the complete new snapshot"),
		HasCombatSnapshot(UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status), 4)
		&& Status == EImmortalSaveLoadStatus::Loaded);
	return true;
}

#endif
