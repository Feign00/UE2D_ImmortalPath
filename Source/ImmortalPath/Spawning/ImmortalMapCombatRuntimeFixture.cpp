// A development-only, real-actor regression fixture for map-kill persistence.
#include "ImmortalMonsterSpawner.h"

#if !UE_BUILD_SHIPPING

#include "../Characters/ImmortalMonsterCharacter.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Drops/ImmortalEquipmentDrop.h"
#include "../Drops/ImmortalMaterialDrop.h"
#include "../Drops/ImmortalSpiritStoneDrop.h"
#include "../Save/ImmortalPathSaveGame.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

namespace
{
	bool bMapCombatFixtureClaimed = false;

	struct FMapCombatFixtureSnapshot
	{
		FString Fingerprint;
		FString CombatFingerprint;
		int32 Stage = 0;
		int32 StageKills = 0;
		bool bCompleted = false;
		int64 MonsterKills = 0;
		int64 StageClears = 0;
		int64 BossKills = 0;
		int64 MapCompletions = 0;
		int32 SectRevision = 0;
		int64 PetKills = 0;
	};

	FMapCombatFixtureSnapshot CaptureSnapshot(
		const FImmortalMapSystemState& Map,
		const FImmortalQuestState& Quest,
		const FImmortalSectState& Sect,
		const FImmortalPetState& Pet)
	{
		FMapCombatFixtureSnapshot Result;
		Result.MonsterKills = Quest.LifetimeCounters.MonsterKills;
		Result.StageClears = Quest.LifetimeCounters.StageClears;
		Result.BossKills = Quest.LifetimeCounters.BossKills;
		Result.MapCompletions = Quest.LifetimeCounters.MapCompletions;
		Result.SectRevision = Sect.Revision;
		Result.PetKills = Pet.TotalCombatKills;
		Result.Fingerprint = FString::Printf(
			TEXT("map=%s;quest=%lld,%lld,%lld,%lld,r%d;sect=%s,r%d;pet=%s,%lld,r%d"),
			*Map.ActiveMapId.ToString(),
			Result.MonsterKills, Result.StageClears, Result.BossKills,
			Result.MapCompletions, Quest.Revision,
			*Sect.SectId.ToString(), Sect.Revision,
			*Pet.ActivePetId.ToString(), Result.PetKills, Pet.Revision);
		// Restart may legitimately settle offline cultivation and increase
		// unrelated revisions. Compare combat-owned fields separately there.
		Result.CombatFingerprint = FString::Printf(
			TEXT("map=%s;quest=%lld,%lld,%lld,%lld;sect=%s;pet=%s,%lld"),
			*Map.ActiveMapId.ToString(), Result.MonsterKills,
			Result.StageClears, Result.BossKills, Result.MapCompletions,
			*Sect.SectId.ToString(), *Pet.ActivePetId.ToString(),
			Result.PetKills);
		for (const FName MapId : UImmortalMapLibrary::GetKnownMapIds())
		{
			FImmortalMapProgress Progress;
			if (!UImmortalMapLibrary::GetMapProgress(Map, MapId, Progress))
			{
				Result.Fingerprint += FString::Printf(TEXT(";m:%s=missing"), *MapId.ToString());
				Result.CombatFingerprint += FString::Printf(TEXT(";m:%s=missing"), *MapId.ToString());
				continue;
			}
			const FString MapPart = FString::Printf(TEXT(";m:%s=%d,%d,%d"),
				*MapId.ToString(), Progress.Stage, Progress.StageKills,
				Progress.bCompleted ? 1 : 0);
			Result.Fingerprint += MapPart;
			Result.CombatFingerprint += MapPart;
			if (MapId == Map.ActiveMapId)
			{
				Result.Stage = Progress.Stage;
				Result.StageKills = Progress.StageKills;
				Result.bCompleted = Progress.bCompleted;
			}
		}
		for (const FImmortalSectTaskProgress& Task : Sect.DailyTasks)
		{
			const FString TaskPart = FString::Printf(TEXT(";s:%s=%d,%d"),
				*Task.TaskId.ToString(), Task.Progress, Task.bClaimed ? 1 : 0);
			Result.Fingerprint += TaskPart;
			Result.CombatFingerprint += TaskPart;
		}
		for (const FImmortalPetProgress& Progress : Pet.Pets)
		{
			const FString PetPart = FString::Printf(TEXT(";p:%s=%d,%d,%d,%lld"),
				*Progress.PetId.ToString(), Progress.Level,
				Progress.Experience, Progress.Stars, Progress.TotalCombatKills);
			Result.Fingerprint += PetPart;
			Result.CombatFingerprint += PetPart;
		}
		return Result;
	}

	FMapCombatFixtureSnapshot CaptureLive(
		const AImmortalMonsterSpawner* Spawner,
		const AImmortalPlayerCharacter* Player)
	{
		return CaptureSnapshot(Spawner->GetMapSystemState(), Player->GetQuestState(),
			Player->GetSectState(), Player->GetPetState());
	}

	bool CaptureDisk(FMapCombatFixtureSnapshot& OutSnapshot)
	{
		const UImmortalPathSaveGame* Save = Cast<UImmortalPathSaveGame>(
			UGameplayStatics::LoadGameFromSlot(
				UImmortalPathSaveGame::GetSlotName(), 0));
		if (!Save) return false;
		OutSnapshot = CaptureSnapshot(Save->MapSystemState, Save->QuestState,
			Save->SectState, Save->PetState);
		return true;
	}

	int32 CountPhysicalDrops(const UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<AImmortalEquipmentDrop> It(World); It; ++It) ++Count;
		for (TActorIterator<AImmortalMaterialDrop> It(World); It; ++It) ++Count;
		for (TActorIterator<AImmortalSpiritStoneDrop> It(World); It; ++It) ++Count;
		return Count;
	}

	void FinishFixture(const bool bPassed, const FString& Detail)
	{
		UE_LOG(LogTemp, Display, TEXT("MapCombatFixture RESULT: %s | %s"),
			bPassed ? TEXT("PASS") : TEXT("FAIL"), *Detail);
		if (!bPassed)
		{
			UE_LOG(LogTemp, Error, TEXT("MapCombatFixture failed: %s"), *Detail);
		}
		FPlatformMisc::RequestExit(false);
	}

	bool GetSafeFixtureDirectory(FString& OutDirectory)
	{
		FString UserDirectory;
		if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirectory))
		{
			return false;
		}
		const FString AllowedRoot = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/Automation/MapCombat")));
		OutDirectory = FPaths::ConvertRelativePathToFull(UserDirectory);
	// UE routes SaveGame slots through -UserDir even when ProjectSavedDir()
	// still reports the project-local Saved directory in editor game mode.
	return FPaths::IsUnderDirectory(OutDirectory, AllowedRoot);
	}
}

void AImmortalMonsterSpawner::ScheduleMapCombatRuntimeFixture()
{
	FString CaseName;
	const bool bRun = FParse::Value(
		FCommandLine::Get(), TEXT("ImmortalTestMapKillCase="), CaseName);
	const bool bAudit = FParse::Param(
		FCommandLine::Get(), TEXT("ImmortalTestMapKillAudit"));
	if ((!bRun && !bAudit) || bMapCombatFixtureClaimed) return;
	bMapCombatFixtureClaimed = true;

	FString FixtureDirectory;
	if (!GetSafeFixtureDirectory(FixtureDirectory)
		|| !FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestDisableAutoBattle")))
	{
		FinishFixture(false,
			TEXT("requires -ImmortalTestDisableAutoBattle and -UserDir under Saved/Automation/MapCombat/<case>"));
		return;
	}
	StopSpawning();
	ClearAllMonstersAndDrops();
	FTimerHandle FixtureTimer;
	GetWorldTimerManager().SetTimer(
		FixtureTimer,
		FTimerDelegate::CreateWeakLambda(this,
			[this] { RunMapCombatRuntimeFixture(); }),
		1.8f, false);
}

void AImmortalMonsterSpawner::RunMapCombatRuntimeFixture()
{
	FString FixtureDirectory;
	if (!GetSafeFixtureDirectory(FixtureDirectory))
	{
		FinishFixture(false, TEXT("isolated UserDir check failed before runtime test"));
		return;
	}
	AImmortalPlayerCharacter* Player = Cast<AImmortalPlayerCharacter>(
		UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!Player || bSaveLoadBlocked || bWorldBossChallengeActive
		|| bEndlessDungeonActive || bMapTransitionInProgress)
	{
		FinishFixture(false, TEXT("player or ordinary map scene unavailable"));
		return;
	}
	Player->StopAutoAttack();
	const FString ExpectedPath = FPaths::Combine(
		FixtureDirectory, TEXT("MapCombatExpected.txt"));
	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestMapKillAudit")))
	{
		FString Expected;
		FMapCombatFixtureSnapshot Disk;
		if (!FFileHelper::LoadFileToString(Expected, *ExpectedPath)
			|| !CaptureDisk(Disk))
		{
			FinishFixture(false, TEXT("restart audit missing expected snapshot or readable main save"));
			return;
		}
		Expected.TrimStartAndEndInline();
		const FMapCombatFixtureSnapshot Live = CaptureLive(this, Player);
		const bool bPassed = Live.CombatFingerprint == Expected
			&& Disk.CombatFingerprint == Expected;
		FinishFixture(bPassed, FString::Printf(
			TEXT("restart audit live=%s disk=%s expected=%s"),
			*Live.CombatFingerprint, *Disk.CombatFingerprint, *Expected));
		return;
	}

	FString CaseName;
	FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestMapKillCase="), CaseName);
	int32 FailAttempt = 0;
	FParse::Value(FCommandLine::Get(),
		TEXT("ImmortalTestMapKillFailAttempt="), FailAttempt);
	if (MapSystemState.ActiveMapId != UImmortalMapLibrary::GetQingyunMountainId()
		|| (FailAttempt != 0 && FailAttempt != 1 && FailAttempt != 2))
	{
		FinishFixture(false, TEXT("fixture requires QingyunMountain and failure attempt 0, 1 or 2"));
		return;
	}
	int32 ExpectedStage = 0;
	int32 ExpectedKills = 0;
	bool bExpectedComplete = false;
	int32 StageClearDelta = 0;
	int32 BossKillDelta = 0;
	int32 MapCompleteDelta = 0;
	if (CaseName.Equals(TEXT("normal"), ESearchCase::IgnoreCase))
	{
		CurrentStage = 1; CurrentStageKills = 0;
		ExpectedStage = 1; ExpectedKills = 1;
	}
	else if (CaseName.Equals(TEXT("clear"), ESearchCase::IgnoreCase))
	{
		CurrentStage = 1; CurrentStageKills = 9;
		ExpectedStage = 2; ExpectedKills = 0; StageClearDelta = 1;
	}
	else if (CaseName.Equals(TEXT("boss"), ESearchCase::IgnoreCase))
	{
		CurrentStage = 10; CurrentStageKills = 0;
		ExpectedStage = 11; ExpectedKills = 0;
		StageClearDelta = 1; BossKillDelta = 1;
	}
	else if (CaseName.Equals(TEXT("final"), ESearchCase::IgnoreCase))
	{
		CurrentStage = 999; CurrentStageKills = 0;
		ExpectedStage = 999; ExpectedKills = 1;
		bExpectedComplete = true;
		StageClearDelta = 1; BossKillDelta = 1; MapCompleteDelta = 1;
	}
	else
	{
		FinishFixture(false, TEXT("unknown map-kill case; use normal, clear, boss or final"));
		return;
	}

	bCurrentMapCompleted = false;
	if (!SyncCurrentProgressToState() || !SaveStageProgress())
	{
		FinishFixture(false, TEXT("could not seed and save baseline map stage"));
		return;
	}
	if (!Player->GetSectState().HasJoined())
	{
		const FImmortalSectJoinResult Join = Player->JoinSect(
			FName(TEXT("QingyunSect")));
		if (!Join.bSucceeded)
		{
			FinishFixture(false, TEXT("could not seed QingyunSect membership"));
			return;
		}
	}
	if (Player->GetSectState().SectId != FName(TEXT("QingyunSect"))
		|| Player->GetPetState().ActivePetId.IsNone()
		|| !Player->SaveProgress())
	{
		FinishFixture(false,
			TEXT("could not seed QingyunSect, active pet and baseline player save"));
		return;
	}
	FMapCombatFixtureSnapshot BeforeDisk;
	const FMapCombatFixtureSnapshot Before = CaptureLive(this, Player);
	if (!CaptureDisk(BeforeDisk) || Before.Fingerprint != BeforeDisk.Fingerprint)
	{
		FinishFixture(false, TEXT("seeded live baseline does not match main save"));
		return;
	}
	ClearAllMonstersAndDrops();
	AImmortalMonsterCharacter* Monster = SpawnMonster();
	if (!Monster || Monster->IsBoss() != IsCurrentStageBossStage())
	{
		FinishFixture(false, TEXT("could not spawn expected real map monster or boss"));
		return;
	}
	const int32 DropsBefore = CountPhysicalDrops(GetWorld());
	TArray<uint8> SaveBytesBefore;
	if (!UGameplayStatics::LoadDataFromSlot(
		SaveBytesBefore, UImmortalPathSaveGame::GetSlotName(), 0))
	{
		FinishFixture(false, TEXT("could not read baseline save bytes"));
		return;
	}

	UImmortalPathSaveGame::SetDevelopmentWriteFailureOnAttempt(FailAttempt);
	UGameplayStatics::ApplyDamage(Monster,
		Monster->GetMaxHealth() + 1000000.0f, nullptr,
		Player, UDamageType::StaticClass());
	const int32 WriteAttempts =
		UImmortalPathSaveGame::GetDevelopmentWriteAttemptCount();
	UImmortalPathSaveGame::ClearDevelopmentWriteFailureOnAttempt();
	const FMapCombatFixtureSnapshot After = CaptureLive(this, Player);
	FMapCombatFixtureSnapshot AfterDisk;
	TArray<uint8> SaveBytesAfter;
	const bool bReadable = CaptureDisk(AfterDisk)
		&& UGameplayStatics::LoadDataFromSlot(
			SaveBytesAfter, UImmortalPathSaveGame::GetSlotName(), 0);
	const int32 DropsAfter = CountPhysicalDrops(GetWorld());
	const bool bFailedKill = FailAttempt == 1;
	bool bPassed = Monster->IsDead() && WriteAttempts == 1 && bReadable;
	if (bFailedKill)
	{
		bPassed = bPassed && After.Fingerprint == Before.Fingerprint
			&& AfterDisk.Fingerprint == BeforeDisk.Fingerprint
			&& SaveBytesAfter == SaveBytesBefore
			&& DropsAfter == DropsBefore;
	}
	else
	{
		bPassed = bPassed
			&& After.Stage == ExpectedStage
			&& After.StageKills == ExpectedKills
			&& After.bCompleted == bExpectedComplete
			&& After.MonsterKills == Before.MonsterKills + 1
			&& After.StageClears == Before.StageClears + StageClearDelta
			&& After.BossKills == Before.BossKills + BossKillDelta
			&& After.MapCompletions == Before.MapCompletions + MapCompleteDelta
			&& After.SectRevision > Before.SectRevision
			&& After.PetKills == Before.PetKills + 1
			&& After.Fingerprint == AfterDisk.Fingerprint;
	}
	if (!bPassed)
	{
		FinishFixture(false, FString::Printf(
			TEXT("case=%s failAttempt=%d writes=%d drops=%d->%d before=%s after=%s disk=%s"),
			*CaseName, FailAttempt, WriteAttempts, DropsBefore, DropsAfter,
			*Before.Fingerprint, *After.Fingerprint,
			bReadable ? *AfterDisk.Fingerprint : TEXT("unreadable")));
		return;
	}

	// A later, unrelated save must not reintroduce rolled-back counters.
	if (!Player->SaveProgress())
	{
		FinishFixture(false, TEXT("ordinary post-kill save failed"));
		return;
	}
	FMapCombatFixtureSnapshot FinalDisk;
	const FMapCombatFixtureSnapshot FinalLive = CaptureLive(this, Player);
	if (!CaptureDisk(FinalDisk)
		|| FinalLive.Fingerprint != FinalDisk.Fingerprint
		|| !FFileHelper::SaveStringToFile(
			FinalLive.CombatFingerprint, *ExpectedPath))
	{
		FinishFixture(false, TEXT("post-kill save or restart expectation failed"));
		return;
	}
	FinishFixture(true, FString::Printf(
		TEXT("case=%s failAttempt=%d writes=%d drops=%d->%d expected=%s"),
		*CaseName, FailAttempt, WriteAttempts, DropsBefore, DropsAfter,
		*FinalLive.Fingerprint));
}

#endif // !UE_BUILD_SHIPPING
