// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalMonsterSpawner.h"

#include "../Characters/ImmortalMonsterCharacter.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Drops/ImmortalEquipmentDrop.h"
#include "../Drops/ImmortalMaterialDrop.h"
#include "../Drops/ImmortalSpiritStoneDrop.h"
#include "../Save/ImmortalPathSaveGame.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "PaperTileMapActor.h"
#include "PaperTileMapComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Farther from the positive-Y camera than gameplay sprites, which occupy the lane near Y=0. */
	constexpr float QingyunBackgroundPlaneY = -350.0f;
	constexpr float QingyunBackgroundOverscan = 1.03f;
	constexpr float QingyunBackgroundRefreshInterval = 0.25f;
}

AImmortalMonsterSpawner::AImmortalMonsterSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpawnArea = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnArea"));
	SpawnArea->SetupAttachment(SceneRoot);
	SpawnArea->SetBoxExtent(FVector(800.0f, 20.0f, 20.0f));
	SpawnArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnArea->SetHiddenInGame(true);

	QingyunMountainBackground = CreateDefaultSubobject<UPaperSpriteComponent>(
		TEXT("QingyunMountainBackground"));
	QingyunMountainBackground->SetupAttachment(SceneRoot);
	QingyunMountainBackground->SetMobility(EComponentMobility::Movable);
	QingyunMountainBackground->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	QingyunMountainBackground->SetGenerateOverlapEvents(false);
	QingyunMountainBackground->SetCastShadow(false);
	QingyunMountainBackground->SetTranslucentSortPriority(-1000);
	QingyunMountainBackground->SetVisibility(false);
	QingyunMountainBackground->SetHiddenInGame(true);

	QingyunMountainBackgroundAsset = TSoftObjectPtr<UPaperSprite>(
		FSoftObjectPath(
			TEXT("/Game/GAME/Asset/backgrounds/generated/")
			TEXT("SP_QingyunMountain_Background.SP_QingyunMountain_Background")));

	static ConstructorHelpers::FClassFinder<AImmortalMonsterCharacter> DogMonsterFinder(
		TEXT("/Game/GAME/BP_DogMonster"));
	if (DogMonsterFinder.Succeeded())
	{
		DefaultAlternateMonsterClass = DogMonsterFinder.Class;
		DefaultBossMonsterClass = DogMonsterFinder.Class;
	}
}

void AImmortalMonsterSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (!QingyunMountainBackgroundSprite
		&& !QingyunMountainBackgroundAsset.IsNull())
	{
		QingyunMountainBackgroundSprite =
			QingyunMountainBackgroundAsset.LoadSynchronous();
		if (QingyunMountainBackground && QingyunMountainBackgroundSprite)
		{
			QingyunMountainBackground->SetSprite(
				QingyunMountainBackgroundSprite);
		}
	}
	LoadStageProgress();

#if !UE_BUILD_SHIPPING
	int32 TestStage = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestStage="), TestStage))
	{
		CurrentStage = FMath::Clamp(TestStage, 1, GetCurrentMaximumStage());
		CurrentStageKills = 0;
		bCurrentMapCompleted = false;
		SyncCurrentProgressToState();
		++MapSystemRevision;
		UE_LOG(LogTemp, Display, TEXT("Map development stage override applied: %s stage %d"),
			*ActiveMapDefinition.DisplayName.ToString(), CurrentStage);
	}
#endif

	ApplyActiveMapPresentation();
	BP_OnActiveMapChanged(NAME_None, MapSystemState.ActiveMapId, ActiveMapDefinition.DisplayName, CurrentStage);
	if (bStartOnBeginPlay)
	{
		SpawnUntilInitialCount();
		StartSpawning();
	}
	UpdateStageHud();
}

void AImmortalMonsterSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bResolvingWorldBossChallenge = true;
	bWorldBossChallengeActive = false;
	bResumeSpawningAfterWorldBoss = false;
	bResolvingEndlessDungeon = true;
	bEndlessDungeonActive = false;
	bResumeSpawningAfterEndlessDungeon = false;
	for (const TWeakObjectPtr<AImmortalMonsterCharacter>& MonsterEntry : SpawnedMonsters)
	{
		if (AImmortalMonsterCharacter* Monster = MonsterEntry.Get())
		{
			Monster->OnMonsterDeath.RemoveDynamic(
				this, &AImmortalMonsterSpawner::HandleMonsterDeath);
			Monster->OnBossPhaseChanged.RemoveDynamic(
				this, &AImmortalMonsterSpawner::HandleBossPhaseChanged);
			Monster->OnDestroyed.RemoveDynamic(
				this, &AImmortalMonsterSpawner::HandleSpawnedMonsterDestroyed);
		}
	}
	ActiveWorldBoss.Reset();
	ActiveEndlessBoss.Reset();
	ActiveEndlessRunId.Invalidate();
	SaveStageProgress();
	StopSpawning();
	GetWorldTimerManager().ClearTimer(WorldBossTimeoutTimerHandle);
	GetWorldTimerManager().ClearTimer(WorldBossHudTimerHandle);
	GetWorldTimerManager().ClearTimer(WorldBossResumeTimerHandle);
	GetWorldTimerManager().ClearTimer(EndlessDungeonHudTimerHandle);
	GetWorldTimerManager().ClearTimer(EndlessDungeonNextFloorTimerHandle);
	GetWorldTimerManager().ClearTimer(EndlessDungeonResumeTimerHandle);
	GetWorldTimerManager().ClearTimer(QingyunBackgroundRefreshTimerHandle);
	SpawnedMonsters.Reset();
	Super::EndPlay(EndPlayReason);
}

AImmortalMonsterCharacter* AImmortalMonsterSpawner::SpawnMonster()
{
	RemoveInvalidMonsters();
	if (bMapTransitionInProgress || bCurrentMapCompleted || bWorldBossChallengeActive
		|| bEndlessDungeonActive)
	{
		return nullptr;
	}

	if (IsCurrentStageBossStage())
	{
		for (const TWeakObjectPtr<AImmortalMonsterCharacter>& Existing : SpawnedMonsters)
		{
			if (Existing.IsValid() && Existing->IsBoss() && !Existing->IsDead())
			{
				return nullptr;
			}
		}
		return SpawnConfiguredMonster(true, false);
	}

	return SpawnConfiguredMonster(false, false);
}

void AImmortalMonsterSpawner::HandleMonsterDeath(
	AImmortalMonsterCharacter* Monster,
	AActor* DamageCauser)
{
	if (!Monster || bMapTransitionInProgress)
	{
		return;
	}
	if (bEndlessDungeonActive)
	{
		HandleEndlessDungeonMonsterDeath(Monster, DamageCauser);
		return;
	}
	if (bWorldBossChallengeActive)
	{
		if (Monster->IsWorldBoss()
			&& ActiveWorldBoss.Get() == Monster
			&& Monster->GetWorldBossId() == ActiveWorldBossDefinition.BossId)
		{
			TGuardValue<bool> ResolveGuard(bResolvingWorldBossChallenge, true);
			GetWorldTimerManager().ClearTimer(WorldBossTimeoutTimerHandle);
			GetWorldTimerManager().ClearTimer(WorldBossHudTimerHandle);
			const float ClearSeconds = GetWorld()
				? FMath::Clamp(
					GetWorld()->GetTimeSeconds() - WorldBossChallengeStartTime,
					0.01f,
					FMath::Max(ActiveWorldBossDefinition.TimeLimitSeconds, 0.01f))
				: FMath::Max(ActiveWorldBossDefinition.TimeLimitSeconds, 0.01f);
			AImmortalPlayerCharacter* Player = Cast<AImmortalPlayerCharacter>(
				UGameplayStatics::GetPlayerCharacter(this, 0));
			if (Player && DamageCauser == Player)
			{
				Player->NotifyPetCombatKill(Monster);
			}
			const FImmortalWorldBossVictoryResult Victory = Player
				? Player->CommitWorldBossVictory(ActiveWorldBossDefinition.BossId, ClearSeconds)
				: FImmortalWorldBossVictoryResult();
			const FText Message = Victory.bSucceeded
				? FText::FromString(FString::Printf(
					TEXT("世界妖王“%s”已击败，用时 %.1f 秒%s%s"),
					*ActiveWorldBossDefinition.DisplayName.ToString(),
					ClearSeconds,
					Victory.bFirstClear ? TEXT("，首通法宝已记录") : TEXT(""),
					Victory.bRewardPending ? TEXT("；奖励等待背包空间") : TEXT("")))
				: FText::FromString(TEXT("世界妖王已击败，但奖励存档失败；本次不记录胜利"));
			FinishWorldBossChallenge(
				Victory.bSucceeded,
				Message,
				Victory.bSucceeded
					? FLinearColor(1.0f, 0.78f, 0.18f, 1.0f)
					: FLinearColor(1.0f, 0.18f, 0.12f, 1.0f),
				Monster);
			return;
		}

		if (Monster->ActorHasTag(TEXT("WorldBossMinion")))
		{
			// World Boss summons belong only to the encounter. They grant no
			// cultivation and never advance map or sect kill gates.
			UE_LOG(LogTemp, Display,
				TEXT("World Boss minion defeated without map/sect progression"));
			return;
		}

		UE_LOG(LogTemp, Warning,
			TEXT("Ignored unrelated monster death while World Boss challenge is active"));
		return;
	}
	if (bCurrentMapCompleted)
	{
		return;
	}
	if (Monster->GetConfiguredMapId() != MapSystemState.ActiveMapId)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Ignored stale monster death from map %s while %s is active"),
			*Monster->GetConfiguredMapId().ToString(), *MapSystemState.ActiveMapId.ToString());
		return;
	}

	TGuardValue<bool> DeathGuard(bHandlingMonsterDeath, true);
	bool bStageCleared = false;
	bool bBossDefeated = false;
	if (IsCurrentStageBossStage())
	{
		if (!Monster->IsBoss())
		{
			if (AImmortalPlayerCharacter* Player = Cast<AImmortalPlayerCharacter>(
				UGameplayStatics::GetPlayerCharacter(this, 0)))
			{
				Player->NotifySectCombatProgress(1, 0, 0);
			}
			UE_LOG(LogTemp, Display, TEXT("%s boss minion defeated; gate remains active at stage %d"),
				*ActiveMapDefinition.DisplayName.ToString(), CurrentStage);
			return;
		}

		const int32 ClearedStage = CurrentStage;
		AdvanceStage(Monster);
		bStageCleared = true;
		bBossDefeated = true;
		++MapSystemRevision;
		BP_OnBossDefeated(ClearedStage, CurrentStage, bCurrentMapCompleted);
		const FString VictoryMessage = bCurrentMapCompleted
			? FString::Printf(TEXT("%s最终首领“%s”已击败！地图通关"),
				*ActiveMapDefinition.DisplayName.ToString(), *ActiveMapDefinition.BossName.ToString())
			: FString::Printf(TEXT("%s守关首领已击败！进入第 %d 关"),
				*ActiveMapDefinition.DisplayName.ToString(), CurrentStage);
		ShowBossMessage(FText::FromString(VictoryMessage), FLinearColor(1.0f, 0.78f, 0.18f, 1.0f));
	}
	else
	{
		++CurrentStageKills;
		const int32 RequiredKills = GetRequiredKillsForCurrentStage();
		UE_LOG(LogTemp, Display, TEXT("%s stage %d progress: %d/%d"),
			*ActiveMapDefinition.DisplayName.ToString(), CurrentStage, CurrentStageKills, RequiredKills);
		if (CurrentStageKills >= RequiredKills)
		{
			AdvanceStage(Monster);
			bStageCleared = true;
		}
		++MapSystemRevision;
	}

	if (AImmortalPlayerCharacter* Player =
		Cast<AImmortalPlayerCharacter>(DamageCauser))
	{
		Player->NotifyPetCombatKill(Monster);
	}
	SyncCurrentProgressToState();
	UpdateStageHud();
	const bool bMapProgressSaved = SaveStageProgress();
	if (AImmortalPlayerCharacter* Player = Cast<AImmortalPlayerCharacter>(
		UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		// A stage/Boss task must never advance when its authoritative map write
		// failed, otherwise restarting would allow the same gate to be claimed
		// repeatedly. The ordinary kill itself is still a valid completed combat.
		Player->NotifySectCombatProgress(
			1,
			bMapProgressSaved && bStageCleared ? 1 : 0,
			bMapProgressSaved && bBossDefeated ? 1 : 0,
			bMapProgressSaved && bStageCleared
				&& bCurrentMapCompleted ? 1 : 0);
	}
}

bool AImmortalMonsterSpawner::SaveStageProgress()
{
	if (!SyncCurrentProgressToState())
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot save map progress because active map state is invalid"));
		return false;
	}

	UImmortalPathSaveGame* SaveGame = UImmortalPathSaveGame::LoadOrCreate(this);
	if (!SaveGame)
	{
		return false;
	}

	SaveGame->MapSystemState = MapSystemState;
	if (const AImmortalPlayerCharacter* Player =
		Cast<AImmortalPlayerCharacter>(
			UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		FImmortalPetState PetState = Player->GetPetState();
		UImmortalPetLibrary::NormalizeState(PetState);
		SaveGame->bPetSystemInitialized = true;
		SaveGame->PetState = PetState;
	}
	MirrorLegacyQingyunProgress(SaveGame);
	const bool bSaved = SaveGame->SaveToDisk();
	if (bSaved)
	{
		bMapMigrationPending = false;
		UE_LOG(LogTemp, Display,
			TEXT("Map progress saved: %s (%s) | stage %d | progress %d/%d | boss=%s | completed=%s | maps=%d | revision=%d"),
			*ActiveMapDefinition.DisplayName.ToString(), *MapSystemState.ActiveMapId.ToString(),
			CurrentStage, CurrentStageKills, GetRequiredKillsForCurrentStage(),
			IsCurrentStageBossStage() ? TEXT("true") : TEXT("false"),
			bCurrentMapCompleted ? TEXT("true") : TEXT("false"),
			MapSystemState.MapProgress.Num(), MapSystemRevision);
	}
	return bSaved;
}

bool AImmortalMonsterSpawner::LoadStageProgress()
{
	UImmortalPathSaveGame* SaveGame = UImmortalPathSaveGame::LoadOrCreate(this);
	if (!SaveGame)
	{
		return false;
	}

	const TArray<FName> KnownMapIds = UImmortalMapLibrary::GetKnownMapIds();
	TSet<FName> SeenMapIds;
	bool bCompleteValidState = SaveGame->MapSystemState.bInitialized
		&& SaveGame->MapSystemState.MapProgress.Num() == KnownMapIds.Num();
	for (const FImmortalMapProgress& Progress : SaveGame->MapSystemState.MapProgress)
	{
		FImmortalMapDefinition Definition;
		if (!Progress.IsValid()
			|| !UImmortalMapLibrary::GetMapDefinition(Progress.MapId, Definition)
			|| Progress.Stage > FMath::Clamp(Definition.MaximumStage, 1, 999)
			|| Progress.StageKills > FMath::Max(KillsPerStage, 1)
			|| (Progress.bCompleted && Progress.Stage < FMath::Clamp(Definition.MaximumStage, 1, 999))
			|| SeenMapIds.Contains(Progress.MapId))
		{
			bCompleteValidState = false;
			continue;
		}
		SeenMapIds.Add(Progress.MapId);
	}
	for (const FName MapId : KnownMapIds)
	{
		if (!SeenMapIds.Contains(MapId))
		{
			bCompleteValidState = false;
			break;
		}
	}
	FImmortalMapDefinition SavedActiveDefinition;
	if (!UImmortalMapLibrary::GetMapDefinition(
		SaveGame->MapSystemState.ActiveMapId, SavedActiveDefinition))
	{
		bCompleteValidState = false;
	}

	const bool bNeedsMigration = SaveGame->SaveVersion < 12 || !bCompleteValidState;
	const bool bHasUsableMapState = SaveGame->MapSystemState.bInitialized
		&& !SaveGame->MapSystemState.MapProgress.IsEmpty();
	if (bHasUsableMapState)
	{
		MapSystemState = SaveGame->MapSystemState;
	}
	else
	{
		const int32 LegacyStage = SaveGame->bHasStageData
			? SaveGame->QingyunStage
			: CurrentStage;
		const int32 LegacyKills = SaveGame->bHasStageData
			? SaveGame->QingyunStageKills
			: 0;
		MapSystemState = UImmortalMapLibrary::CreateMigratedState(
			LegacyStage,
			LegacyKills,
			SaveGame->bHasStageData && SaveGame->bQingyunMountainCompleted);
	}
	UImmortalMapLibrary::NormalizeState(MapSystemState);
	if (!ApplyProgressForMap(MapSystemState.ActiveMapId))
	{
		ApplyProgressForMap(UImmortalMapLibrary::GetQingyunMountainId());
	}

	bMapMigrationPending = bNeedsMigration;
	++MapSystemRevision;
	UE_LOG(LogTemp, Display,
		TEXT("Map progress loaded: %s (%s) | stage %d | progress %d/%d | boss=%s | completed=%s | maps=%d | migrationPending=%s"),
		*ActiveMapDefinition.DisplayName.ToString(), *MapSystemState.ActiveMapId.ToString(),
		CurrentStage, CurrentStageKills, GetRequiredKillsForCurrentStage(),
		IsCurrentStageBossStage() ? TEXT("true") : TEXT("false"),
		bCurrentMapCompleted ? TEXT("true") : TEXT("false"),
		MapSystemState.MapProgress.Num(), bMapMigrationPending ? TEXT("true") : TEXT("false"));

	// Do not write the migration here. Actor BeginPlay order is not guaranteed;
	// updating LastSavedUtcTicks before the player claims offline rewards would erase them.
	return true;
}

void AImmortalMonsterSpawner::UpdateStageHud() const
{
	if (AImmortalPlayerCharacter* Player = Cast<AImmortalPlayerCharacter>(
		UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->UpdateStageProgress(
			MapSystemState.ActiveMapId,
			ActiveMapDefinition.DisplayName,
			GetCurrentMaximumStage(),
			CurrentStage,
			CurrentStageKills,
			GetRequiredKillsForCurrentStage(),
			IsCurrentStageBossStage(),
			bCurrentMapCompleted);
	}
}

bool AImmortalMonsterSpawner::IsCurrentStageBossStage() const
{
	if (bCurrentMapCompleted)
	{
		return false;
	}
	const int32 MaximumStage = GetCurrentMaximumStage();
	const int32 SafeStage = FMath::Clamp(CurrentStage, 1, MaximumStage);
	return SafeStage >= MaximumStage
		|| SafeStage % GetCurrentBossStageInterval() == 0;
}

bool AImmortalMonsterSpawner::IsQingyunMountainCompleted() const
{
	FImmortalMapProgress QingyunProgress;
	return UImmortalMapLibrary::GetMapProgress(
		MapSystemState, UImmortalMapLibrary::GetQingyunMountainId(), QingyunProgress)
		&& QingyunProgress.bCompleted;
}

bool AImmortalMonsterSpawner::GetProgressForMap(
	const FName MapId,
	FImmortalMapProgress& OutProgress) const
{
	return UImmortalMapLibrary::GetMapProgress(MapSystemState, MapId, OutProgress);
}

bool AImmortalMonsterSpawner::CanTravelToMap(const FName DestinationMapId) const
{
	if (bWorldBossChallengeActive || bEndlessDungeonActive
		|| bMapTransitionInProgress || bHandlingMonsterDeath)
	{
		return false;
	}
	FImmortalMapDefinition Definition;
	if (!UImmortalMapLibrary::GetMapDefinition(DestinationMapId, Definition))
	{
		return false;
	}
	if (DestinationMapId == MapSystemState.ActiveMapId)
	{
		return true;
	}
	return UImmortalMapLibrary::IsMapUnlocked(DestinationMapId, GetPlayerRealmIndex());
}

FImmortalMapTravelResult AImmortalMonsterSpawner::TravelToMap(const FName DestinationMapId)
{
	FImmortalMapTravelResult Result;
	Result.PreviousMapId = MapSystemState.ActiveMapId;
	Result.DestinationMapId = DestinationMapId;
	if (bEndlessDungeonActive)
	{
		Result.Message = FText::FromString(
			TEXT("无尽秘境挑战进行中，请先退出秘境再切换地图"));
		return Result;
	}
	if (bWorldBossChallengeActive)
	{
		Result.Message = FText::FromString(TEXT("世界妖王挑战进行中，结束或退出挑战后才能切换地图"));
		return Result;
	}

	FImmortalMapDefinition DestinationDefinition;
	if (!UImmortalMapLibrary::GetMapDefinition(DestinationMapId, DestinationDefinition))
	{
		Result.Message = FText::FromString(TEXT("目标地图不存在"));
		return Result;
	}

	Result.bUnlocked = UImmortalMapLibrary::IsMapUnlocked(
		DestinationMapId, GetPlayerRealmIndex());
	if (DestinationMapId == MapSystemState.ActiveMapId)
	{
		Result.bSucceeded = true;
		Result.bUnlocked = true;
		Result.bAlreadyActive = true;
		Result.Message = FText::FromString(FString::Printf(
			TEXT("当前已在%s第 %d 关"), *DestinationDefinition.DisplayName.ToString(), CurrentStage));
		return Result;
	}
	if (!Result.bUnlocked)
	{
		Result.Message = FText::FromString(FString::Printf(
			TEXT("需要达到%s境界才能进入%s"),
			*UImmortalMapLibrary::GetRealmRequirementText(DestinationDefinition.RequiredRealmIndex).ToString(),
			*DestinationDefinition.DisplayName.ToString()));
		return Result;
	}
	if (bMapTransitionInProgress)
	{
		Result.Message = FText::FromString(TEXT("地图正在切换，请稍候"));
		return Result;
	}
	if (bHandlingMonsterDeath)
	{
		Result.Message = FText::FromString(TEXT("战利品正在结算，请稍后切换地图"));
		return Result;
	}
	if (!SaveStageProgress())
	{
		Result.Message = FText::FromString(TEXT("当前地图进度保存失败，未执行切换"));
		return Result;
	}

	const FImmortalMapSystemState PreviousState = MapSystemState;
	const bool bShouldResumeSpawning = bSpawningRequested;
	bMapTransitionInProgress = true;
	SuspendSpawnTimer();

	MapSystemState.ActiveMapId = DestinationMapId;
	if (!ApplyProgressForMap(DestinationMapId) || !SaveStageProgress())
	{
		MapSystemState = PreviousState;
		ApplyProgressForMap(Result.PreviousMapId);
		ApplyActiveMapPresentation();
		UpdateStageHud();
		bMapTransitionInProgress = false;
		if (bShouldResumeSpawning && !bCurrentMapCompleted)
		{
			SpawnUntilInitialCount();
			StartSpawning();
		}
		Result.Message = FText::FromString(TEXT("目标地图保存失败，已安全返回原地图"));
		UE_LOG(LogTemp, Error, TEXT("Map travel rolled back: %s -> %s"),
			*Result.PreviousMapId.ToString(), *DestinationMapId.ToString());
		return Result;
	}

	// Only destroy the old combat scene after the destination state is safely
	// committed. A disk-write failure can then roll back without consuming the
	// player's uncollected physical drops.
	ClearAllMonstersAndDrops();
	++MapSystemRevision;
	ApplyActiveMapPresentation();
	UpdateStageHud();
	BP_OnActiveMapChanged(
		Result.PreviousMapId, DestinationMapId, ActiveMapDefinition.DisplayName, CurrentStage);
	bMapTransitionInProgress = false;
	if (bShouldResumeSpawning && !bCurrentMapCompleted)
	{
		SpawnUntilInitialCount();
		StartSpawning();
	}

	Result.bSucceeded = true;
	Result.Message = FText::FromString(FString::Printf(
		TEXT("已进入%s · 第 %d 关"), *ActiveMapDefinition.DisplayName.ToString(), CurrentStage));
	ShowBossMessage(Result.Message, ActiveMapDefinition.BossColor);
	UE_LOG(LogTemp, Display,
		TEXT("Map travel committed: %s -> %s (%s) | stage %d | completed=%s | revision=%d"),
		*Result.PreviousMapId.ToString(), *DestinationMapId.ToString(),
		*ActiveMapDefinition.DisplayName.ToString(), CurrentStage,
		bCurrentMapCompleted ? TEXT("true") : TEXT("false"), MapSystemRevision);
	return Result;
}

bool AImmortalMonsterSpawner::CompleteMapForDevelopment(
	const FName MapId)
{
#if !UE_BUILD_SHIPPING
	FImmortalMapDefinition Definition;
	FImmortalMapProgress Progress;
	if (!UImmortalMapLibrary::GetMapDefinition(
			MapId, Definition)
		|| !UImmortalMapLibrary::GetMapProgress(
			MapSystemState, MapId, Progress))
	{
		return false;
	}
	const FImmortalMapSystemState PreviousState =
		MapSystemState;
	const int32 PreviousRevision = MapSystemRevision;
	Progress.Stage = Definition.MaximumStage;
	Progress.StageKills = 0;
	Progress.bCompleted = true;
	if (!UImmortalMapLibrary::SetMapProgress(
		MapSystemState, Progress))
	{
		return false;
	}
	if (MapId == MapSystemState.ActiveMapId
		&& !ApplyProgressForMap(MapId))
	{
		MapSystemState = PreviousState;
		MapSystemRevision = PreviousRevision;
		return false;
	}
	++MapSystemRevision;
	if (!SaveStageProgress())
	{
		MapSystemState = PreviousState;
		MapSystemRevision = PreviousRevision;
		return false;
	}
	UE_LOG(LogTemp, Display,
		TEXT("Development map completion prepared: map=%s stage=%d completed=true active=%s revision=%d"),
		*MapId.ToString(),
		Progress.Stage,
		*MapSystemState.ActiveMapId.ToString(),
		MapSystemRevision);
	return true;
#else
	return false;
#endif
}

bool AImmortalMonsterSpawner::CanApplyAscensionCycleReset() const
{
	return HasActorBegunPlay()
		&& GetWorld()
		&& MapSystemState.bInitialized
		&& ActiveMapDefinition.IsValid()
		&& !bMapTransitionInProgress
		&& !bHandlingMonsterDeath
		&& !bWorldBossChallengeActive
		&& !bResolvingWorldBossChallenge
		&& !bEndlessDungeonActive
		&& !bResolvingEndlessDungeon;
}

bool AImmortalMonsterSpawner::IsCanonicalAscensionCycleState(
	const FImmortalMapSystemState& State) const
{
	FImmortalMapSystemState Normalized = State;
	UImmortalMapLibrary::NormalizeState(Normalized);
	if (Normalized.ActiveMapId
			!= UImmortalMapLibrary::GetQingyunMountainId()
		|| Normalized.MapProgress.Num()
			!= UImmortalMapLibrary::GetKnownMapIds().Num())
	{
		return false;
	}
	for (const FName MapId :
		UImmortalMapLibrary::GetKnownMapIds())
	{
		FImmortalMapProgress Progress;
		if (!UImmortalMapLibrary::GetMapProgress(
				Normalized, MapId, Progress)
			|| Progress.Stage != 1
			|| Progress.StageKills != 0
			|| Progress.bCompleted)
		{
			return false;
		}
	}
	return true;
}

void AImmortalMonsterSpawner::ApplyPersistedAscensionCycleState(
	const FImmortalMapSystemState& PersistedCycleState)
{
	FImmortalMapSystemState NewCycleState =
		PersistedCycleState;
	UImmortalMapLibrary::NormalizeState(NewCycleState);
	if (!ensureAlwaysMsgf(
		IsCanonicalAscensionCycleState(NewCycleState),
		TEXT("Persisted ascension state was not canonical after preflight")))
	{
		// Never leave a committed ascension paired with the old runtime map.
		NewCycleState =
			UImmortalMapLibrary::CreateMigratedState(
				1, 0, false);
	}
	ensureAlwaysMsgf(
		CanApplyAscensionCycleReset(),
		TEXT("Ascension cycle runtime preflight changed during a synchronous commit"));

	const FName PreviousMapId =
		MapSystemState.ActiveMapId;
	const bool bShouldResumeSpawning =
		bSpawningRequested;
	bMapTransitionInProgress = true;
	SuspendSpawnTimer();
	MapSystemState = NewCycleState;
	if (!ApplyProgressForMap(
		UImmortalMapLibrary::GetQingyunMountainId()))
	{
		// Keep the committed state authoritative even in a catalog invariant
		// failure; never restore or later write the completed previous cycle.
		MapSystemState =
			UImmortalMapLibrary::CreateMigratedState(
				1, 0, false);
		UImmortalMapLibrary::GetMapDefinition(
			UImmortalMapLibrary::GetQingyunMountainId(),
			ActiveMapDefinition);
		CurrentStage = 1;
		CurrentStageKills = 0;
		bCurrentMapCompleted = false;
		ensureAlwaysMsgf(false,
			TEXT("Canonical ascension cycle could not bind Qingyun progress"));
	}

	// Only consume the old combat scene after the combined player/map write
	// succeeded. A failed write therefore leaves monsters and physical drops.
	ClearAllMonstersAndDrops();
	bMapMigrationPending = false;
	++MapSystemRevision;
	ApplyActiveMapPresentation();
	UpdateStageHud();
	BP_OnActiveMapChanged(
		PreviousMapId,
		MapSystemState.ActiveMapId,
		ActiveMapDefinition.DisplayName,
		CurrentStage);
	bMapTransitionInProgress = false;
	if (bShouldResumeSpawning)
	{
		SpawnUntilInitialCount();
		StartSpawning();
	}

	UE_LOG(LogTemp, Display,
		TEXT("Persisted ascension cycle applied: %s -> %s stage=%d kills=%d completed=%s maps=%d alive=%d revision=%d"),
		*PreviousMapId.ToString(),
		*MapSystemState.ActiveMapId.ToString(),
		CurrentStage,
		CurrentStageKills,
		bCurrentMapCompleted ? TEXT("true") : TEXT("false"),
		MapSystemState.MapProgress.Num(),
		GetAliveMonsterCount(),
		MapSystemRevision);
}

FImmortalWorldBossChallengeResult AImmortalMonsterSpawner::StartWorldBossChallenge(
	const FName BossId)
{
	FImmortalWorldBossChallengeResult Result;
	Result.BossId = BossId;
	if (bEndlessDungeonActive)
	{
		Result.Message = FText::FromString(
			TEXT("无尽秘境挑战进行中，请先退出秘境再挑战世界妖王"));
		return Result;
	}
	FImmortalWorldBossDefinition Definition;
	if (!UImmortalWorldBossLibrary::GetWorldBossDefinition(BossId, Definition))
	{
		Result.Message = FText::FromString(TEXT("指定的世界妖王不存在"));
		return Result;
	}

	AImmortalPlayerCharacter* Player = Cast<AImmortalPlayerCharacter>(
		UGameplayStatics::GetPlayerCharacter(this, 0));
	Result.bUnlocked = Player
		&& UImmortalWorldBossLibrary::IsUnlocked(
			Definition, static_cast<int32>(Player->GetCultivationRealm()));
	if (!Result.bUnlocked)
	{
		Result.Message = FText::FromString(FString::Printf(
			TEXT("境界不足：需要达到第 %d 个大境界才能挑战%s"),
			Definition.RequiredRealmIndex + 1,
			*Definition.DisplayName.ToString()));
		return Result;
	}
	if (bWorldBossChallengeActive)
	{
		Result.bAlreadyActive = ActiveWorldBossDefinition.BossId == BossId;
		Result.Message = FText::FromString(Result.bAlreadyActive
			? TEXT("该世界妖王挑战已经进行中")
			: TEXT("已有另一场世界妖王挑战进行中"));
		return Result;
	}
	if (bMapTransitionInProgress || bHandlingMonsterDeath)
	{
		Result.Message = FText::FromString(TEXT("地图或战利品正在结算，请稍后再发起挑战"));
		return Result;
	}
	if (!SaveStageProgress())
	{
		Result.Message = FText::FromString(TEXT("当前地图进度保存失败，未进入世界妖王挑战"));
		return Result;
	}

	GetWorldTimerManager().ClearTimer(WorldBossResumeTimerHandle);
	bResumeSpawningAfterWorldBoss = bSpawningRequested;
	SuspendSpawnTimer();
	ClearAllMonstersForWorldBoss();
	ActiveWorldBossDefinition = Definition;
	bWorldBossChallengeActive = true;
	bResolvingWorldBossChallenge = false;
	WorldBossChallengeStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	WorldBossChallengeEndTime =
		WorldBossChallengeStartTime + FMath::Max(Definition.TimeLimitSeconds, 15.0f);

	AImmortalMonsterCharacter* Boss = SpawnWorldBoss(Definition);
	if (!Boss)
	{
		bWorldBossChallengeActive = false;
		ActiveWorldBossDefinition = FImmortalWorldBossDefinition();
		WorldBossChallengeStartTime = 0.0f;
		WorldBossChallengeEndTime = 0.0f;
		ResumeMapSpawningAfterWorldBoss();
		Result.Message = FText::FromString(TEXT("世界妖王生成失败，地图挂机已经恢复"));
		return Result;
	}
	ActiveWorldBoss = Boss;
	GetWorldTimerManager().SetTimer(
		WorldBossTimeoutTimerHandle,
		this,
		&AImmortalMonsterSpawner::HandleWorldBossTimeout,
		FMath::Max(Definition.TimeLimitSeconds, 15.0f),
		false);
	GetWorldTimerManager().SetTimer(
		WorldBossHudTimerHandle,
		this,
		&AImmortalMonsterSpawner::UpdateWorldBossHud,
		0.20f,
		true);
	UpdateWorldBossHud();

	Result.bSucceeded = true;
	Result.Message = FText::FromString(FString::Printf(
		TEXT("世界妖王“%s”降临，限时 %.0f 秒"),
		*Definition.DisplayName.ToString(),
		Definition.TimeLimitSeconds));
	ShowBossMessage(Result.Message, Definition.DisplayColor);
	UE_LOG(LogTemp, Display,
		TEXT("World Boss challenge started: %s | realmRequirement=%d | stage=%d | timeLimit=%.1f | map=%s stage=%d/%d"),
		*Definition.BossId.ToString(), Definition.RequiredRealmIndex,
		Definition.RecommendedStage, Definition.TimeLimitSeconds,
		*MapSystemState.ActiveMapId.ToString(), CurrentStage, CurrentStageKills);
	return Result;
}

FImmortalWorldBossChallengeResult AImmortalMonsterSpawner::CancelWorldBossChallenge()
{
	FImmortalWorldBossChallengeResult Result;
	Result.BossId = ActiveWorldBossDefinition.BossId;
	Result.bUnlocked = true;
	if (!bWorldBossChallengeActive)
	{
		Result.Message = FText::FromString(TEXT("当前没有进行中的世界妖王挑战"));
		return Result;
	}
	FinishWorldBossChallenge(
		false,
		FText::FromString(TEXT("已退出世界妖王挑战，未产生任何奖励")),
		FLinearColor(0.85f, 0.75f, 0.62f, 1.0f));
	Result.bSucceeded = true;
	Result.Message = FText::FromString(TEXT("已退出世界妖王挑战"));
	return Result;
}

FImmortalWorldBossRuntimeSnapshot AImmortalMonsterSpawner::GetWorldBossRuntimeSnapshot() const
{
	FImmortalWorldBossRuntimeSnapshot Result;
	Result.bActive = bWorldBossChallengeActive;
	if (!bWorldBossChallengeActive)
	{
		return Result;
	}
	Result.BossId = ActiveWorldBossDefinition.BossId;
	Result.DisplayName = ActiveWorldBossDefinition.DisplayName;
	Result.TimeLimitSeconds = ActiveWorldBossDefinition.TimeLimitSeconds;
	Result.RemainingSeconds = GetWorld()
		? FMath::Max(WorldBossChallengeEndTime - GetWorld()->GetTimeSeconds(), 0.0f)
		: 0.0f;
	if (const AImmortalMonsterCharacter* Boss = ActiveWorldBoss.Get())
	{
		Result.Phase = Boss->GetBossPhase();
		Result.CurrentHealth = Boss->GetCurrentHealth();
		Result.MaximumHealth = Boss->GetMaxHealth();
	}
	return Result;
}

bool AImmortalMonsterSpawner::DefeatActiveWorldBossForDevelopment()
{
#if !UE_BUILD_SHIPPING
	AImmortalMonsterCharacter* Boss = ActiveWorldBoss.Get();
	if (!bWorldBossChallengeActive || !Boss || Boss->IsDead())
	{
		return false;
	}
	UGameplayStatics::ApplyDamage(
		Boss,
		Boss->GetMaxHealth() + 1000000.0f,
		nullptr,
		UGameplayStatics::GetPlayerCharacter(this, 0),
		UDamageType::StaticClass());
	return Boss->IsDead();
#else
	return false;
#endif
}

bool AImmortalMonsterSpawner::DamageActiveWorldBossForDevelopment(
	const float MaximumHealthFraction)
{
#if !UE_BUILD_SHIPPING
	AImmortalMonsterCharacter* Boss = ActiveWorldBoss.Get();
	if (!bWorldBossChallengeActive || !Boss || Boss->IsDead()
		|| MaximumHealthFraction <= 0.0f)
	{
		return false;
	}
	UGameplayStatics::ApplyDamage(
		Boss,
		Boss->GetMaxHealth() * FMath::Clamp(MaximumHealthFraction, 0.01f, 0.95f),
		nullptr,
		UGameplayStatics::GetPlayerCharacter(this, 0),
		UDamageType::StaticClass());
	return !Boss->IsDead();
#else
	return false;
#endif
}

bool AImmortalMonsterSpawner::TimeoutActiveWorldBossForDevelopment()
{
#if !UE_BUILD_SHIPPING
	if (!bWorldBossChallengeActive)
	{
		return false;
	}
	HandleWorldBossTimeout();
	return !bWorldBossChallengeActive;
#else
	return false;
#endif
}

FImmortalEndlessDungeonStartResult AImmortalMonsterSpawner::StartEndlessDungeon(
	const int32 StartFloor)
{
	FImmortalEndlessDungeonStartResult Result;
	Result.StartFloor = StartFloor;
	FImmortalEndlessFloorDescriptor Descriptor;
	if (!UImmortalEndlessDungeonLibrary::GetFloorDescriptor(
		StartFloor, Descriptor))
	{
		Result.Message = FText::FromString(TEXT("无尽秘境起始层无效"));
		return Result;
	}
	if (bEndlessDungeonActive)
	{
		Result.bAlreadyActive = ActiveEndlessFloor == StartFloor;
		Result.StartFloor = ActiveEndlessFloor;
		Result.Message = FText::FromString(TEXT("无尽秘境挑战已经进行中"));
		return Result;
	}
	if (bWorldBossChallengeActive)
	{
		Result.Message = FText::FromString(
			TEXT("世界妖王挑战进行中，请先结束挑战再进入无尽秘境"));
		return Result;
	}
	if (bMapTransitionInProgress || bHandlingMonsterDeath)
	{
		Result.Message = FText::FromString(TEXT("战斗场景正在结算，请稍后进入无尽秘境"));
		return Result;
	}
	AImmortalPlayerCharacter* Player = Cast<AImmortalPlayerCharacter>(
		UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!Player || Player->IsDead())
	{
		Result.Message = FText::FromString(TEXT("角色当前无法进入无尽秘境"));
		return Result;
	}
	if (!SaveStageProgress())
	{
		Result.Message = FText::FromString(
			TEXT("当前地图进度保存失败，未进入无尽秘境"));
		return Result;
	}

	GetWorldTimerManager().ClearTimer(EndlessDungeonResumeTimerHandle);
	GetWorldTimerManager().ClearTimer(EndlessDungeonNextFloorTimerHandle);
	bResumeSpawningAfterEndlessDungeon = bSpawningRequested;
	SuspendSpawnTimer();
	const int32 PreviousMonsterCount = GetAliveMonsterCount();
	ClearOtherMonsters(nullptr);

	bEndlessDungeonActive = true;
	bResolvingEndlessDungeon = false;
	ActiveEndlessRunId = FGuid::NewGuid();
	ActiveEndlessFloor = Descriptor.Floor;
	ActiveEndlessRunStartFloor = Descriptor.Floor;
	ActiveEndlessKills = 0;
	ActiveEndlessRequiredKills = Descriptor.RequiredKills;
	ActiveEndlessBoss.Reset();
	EndlessDungeonStartTime =
		GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	EndlessFloorStartTime = EndlessDungeonStartTime;

	if (!SpawnEndlessFloor())
	{
		FinishEndlessDungeon(
			false,
			FText::FromString(TEXT("无尽秘境怪物生成失败，已安全返回地图")),
			FLinearColor(1.0f, 0.18f, 0.12f, 1.0f));
		Result.Message = FText::FromString(TEXT("无尽秘境怪物生成失败"));
		return Result;
	}
	GetWorldTimerManager().SetTimer(
		EndlessDungeonHudTimerHandle,
		this,
		&AImmortalMonsterSpawner::UpdateEndlessDungeonHud,
		0.20f,
		true);
	UpdateEndlessDungeonHud();

	Result.bSucceeded = true;
	Result.StartFloor = ActiveEndlessFloor;
	Result.Message = FText::FromString(FString::Printf(
		TEXT("进入无尽秘境第 %d 层"), ActiveEndlessFloor));
	ShowBossMessage(
		Result.Message,
		Descriptor.bBoss
			? FLinearColor(1.0f, 0.30f, 0.18f, 1.0f)
			: (Descriptor.bElite
				? FLinearColor(0.78f, 0.48f, 1.0f, 1.0f)
				: FLinearColor(0.38f, 0.82f, 1.0f, 1.0f)));
	UE_LOG(LogTemp, Display,
		TEXT("Endless Dungeon started: run=%s startFloor=%d map=%s stage=%d kills=%d clearedMonsters=%d"),
		*ActiveEndlessRunId.ToString(EGuidFormats::DigitsWithHyphens),
		ActiveEndlessFloor,
		*MapSystemState.ActiveMapId.ToString(),
		CurrentStage,
		CurrentStageKills,
		PreviousMonsterCount);
	return Result;
}

FImmortalEndlessDungeonStartResult AImmortalMonsterSpawner::CancelEndlessDungeon()
{
	FImmortalEndlessDungeonStartResult Result;
	Result.StartFloor = ActiveEndlessRunStartFloor;
	if (!bEndlessDungeonActive)
	{
		Result.Message = FText::FromString(TEXT("当前没有进行中的无尽秘境挑战"));
		return Result;
	}
	FinishEndlessDungeon(
		false,
		FText::FromString(TEXT("已退出无尽秘境，本层未产生奖励")),
		FLinearColor(0.82f, 0.76f, 0.68f, 1.0f));
	Result.bSucceeded = true;
	Result.Message = FText::FromString(TEXT("已退出无尽秘境"));
	return Result;
}

FImmortalEndlessDungeonRuntimeSnapshot
AImmortalMonsterSpawner::GetEndlessDungeonRuntimeSnapshot() const
{
	FImmortalEndlessDungeonRuntimeSnapshot Result;
	Result.bActive = bEndlessDungeonActive;
	if (!bEndlessDungeonActive)
	{
		return Result;
	}
	FImmortalEndlessFloorDescriptor Descriptor;
	UImmortalEndlessDungeonLibrary::GetFloorDescriptor(
		ActiveEndlessFloor, Descriptor);
	Result.Floor = ActiveEndlessFloor;
	Result.Kills = ActiveEndlessKills;
	Result.RequiredKills = ActiveEndlessRequiredKills;
	Result.bElite = Descriptor.bElite;
	Result.bBoss = Descriptor.bBoss;
	Result.RunStartFloor = ActiveEndlessRunStartFloor;
	Result.ElapsedSeconds = GetWorld()
		? FMath::Max(
			GetWorld()->GetTimeSeconds() - EndlessDungeonStartTime,
			0.0f)
		: 0.0f;
	if (const AImmortalMonsterCharacter* Boss = ActiveEndlessBoss.Get())
	{
		Result.BossPhase = Boss->GetBossPhase();
		Result.CurrentHealth = Boss->GetCurrentHealth();
		Result.MaximumHealth = Boss->GetMaxHealth();
	}
	return Result;
}

bool AImmortalMonsterSpawner::SpawnEndlessFloor()
{
	if (!bEndlessDungeonActive || !ActiveEndlessRunId.IsValid())
	{
		return false;
	}
	FImmortalEndlessFloorDescriptor Descriptor;
	if (!UImmortalEndlessDungeonLibrary::GetFloorDescriptor(
		ActiveEndlessFloor, Descriptor))
	{
		return false;
	}
	RemoveInvalidMonsters();
	ActiveEndlessKills = 0;
	ActiveEndlessRequiredKills = FMath::Max(Descriptor.RequiredKills, 1);
	ActiveEndlessBoss.Reset();
	EndlessFloorStartTime =
		GetWorld() ? GetWorld()->GetTimeSeconds() : EndlessDungeonStartTime;

	int32 SpawnedCount = 0;
	for (int32 Index = 0; Index < ActiveEndlessRequiredKills; ++Index)
	{
		AImmortalMonsterCharacter* Monster = SpawnEndlessEnemy(
			Descriptor.bBoss,
			Descriptor.bElite,
			nullptr);
		if (!Monster)
		{
			break;
		}
		++SpawnedCount;
		if (Descriptor.bBoss)
		{
			ActiveEndlessBoss = Monster;
			break;
		}
	}
	if (SpawnedCount != ActiveEndlessRequiredKills)
	{
		UE_LOG(LogTemp, Error,
			TEXT("Endless Dungeon floor spawn incomplete: run=%s floor=%d spawned=%d required=%d"),
			*ActiveEndlessRunId.ToString(EGuidFormats::DigitsWithHyphens),
			ActiveEndlessFloor,
			SpawnedCount,
			ActiveEndlessRequiredKills);
		return false;
	}
	const FString FloorAnnouncement = Descriptor.bBoss
		? FString::Printf(
			TEXT("\u65E0\u5C3D\u79D8\u5883\u7B2C %d \u5C42\uFF1A\u5B88\u5C42\u9996\u9886\u51FA\u73B0"),
			ActiveEndlessFloor)
		: (Descriptor.bElite
			? FString::Printf(
				TEXT("\u65E0\u5C3D\u79D8\u5883\u7B2C %d \u5C42\uFF1A\u7CBE\u82F1\u56F4\u653B"),
				ActiveEndlessFloor)
			: FString::Printf(
				TEXT("\u65E0\u5C3D\u79D8\u5883\u7B2C %d \u5C42"),
				ActiveEndlessFloor));
	ShowBossMessage(
		FText::FromString(FloorAnnouncement),
		Descriptor.bBoss
			? FLinearColor(1.0f, 0.25f, 0.15f, 1.0f)
			: (Descriptor.bElite
				? FLinearColor(0.82f, 0.52f, 1.0f, 1.0f)
				: FLinearColor(0.40f, 0.86f, 1.0f, 1.0f)));
	UpdateEndlessDungeonHud();
	return true;
}

AImmortalMonsterCharacter* AImmortalMonsterSpawner::SpawnEndlessEnemy(
	const bool bBoss,
	const bool bElite,
	AActor* SpawnOwner)
{
	if (!GetWorld() || !bEndlessDungeonActive
		|| !ActiveEndlessRunId.IsValid())
	{
		return nullptr;
	}
	const TSubclassOf<AImmortalMonsterCharacter> EffectiveAlternateClass =
		AlternateMonsterClass ? AlternateMonsterClass : DefaultAlternateMonsterClass;
	const TSubclassOf<AImmortalMonsterCharacter> EffectiveBossClass =
		BossMonsterClass
			? BossMonsterClass
			: (DefaultBossMonsterClass ? DefaultBossMonsterClass : MonsterClass);
	const TSubclassOf<AImmortalMonsterCharacter> ClassToSpawn = bBoss
		? EffectiveBossClass
		: (EffectiveAlternateClass && FMath::RandBool()
			? EffectiveAlternateClass
			: MonsterClass);
	if (!ClassToSpawn)
	{
		return nullptr;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	FVector SpawnLocation;
	if (bBoss)
	{
		SpawnLocation = PlayerPawn
			? PlayerPawn->GetActorLocation()
				+ FVector(
					FMath::Max(EndlessForwardSpawnDistance, 300.0f),
					0.0f,
					SpawnHeightOffset)
			: GetActorLocation()
				+ FVector(
					FMath::Max(EndlessForwardSpawnDistance, 300.0f),
					0.0f,
					SpawnHeightOffset);
	}
	else if (SpawnOwner)
	{
		int32 ExistingSummons = 0;
		for (const TWeakObjectPtr<AImmortalMonsterCharacter>& Entry :
			SpawnedMonsters)
		{
			const AImmortalMonsterCharacter* Existing = Entry.Get();
			if (Existing && Existing->IsEndlessEnemy()
				&& Existing->GetEndlessRunId() == ActiveEndlessRunId
				&& Existing->GetEndlessFloor() == ActiveEndlessFloor
				&& Existing->ActorHasTag(TEXT("EndlessMinion")))
			{
				++ExistingSummons;
			}
		}
		const float Direction = ExistingSummons % 2 == 0 ? -1.0f : 1.0f;
		const float Ring = static_cast<float>(ExistingSummons / 2);
		SpawnLocation = SpawnOwner->GetActorLocation()
			+ FVector(
				Direction * (160.0f + Ring * 100.0f),
				0.0f,
				0.0f);
		if (PlayerPawn)
		{
			// The Boss may already have walked close to the player. Keep every
			// summon in the incoming-wave half of the TBH camera instead of
			// allowing a negative owner-relative offset to place it behind the
			// player or outside the visible strip.
			const float MinimumForwardX =
				PlayerPawn->GetActorLocation().X
				+ FMath::Max(MinDistanceFromPlayer, 300.0f)
				+ ExistingSummons * 90.0f;
			SpawnLocation.X = FMath::Max(
				SpawnLocation.X,
				MinimumForwardX);
		}
	}
	else if (PlayerPawn)
	{
		int32 ExistingPrimaryEnemies = 0;
		for (const TWeakObjectPtr<AImmortalMonsterCharacter>& Entry :
			SpawnedMonsters)
		{
			const AImmortalMonsterCharacter* Existing = Entry.Get();
			if (Existing && Existing->IsEndlessEnemy()
				&& Existing->GetEndlessRunId() == ActiveEndlessRunId
				&& Existing->GetEndlessFloor() == ActiveEndlessFloor
				&& !Existing->ActorHasTag(TEXT("EndlessMinion")))
			{
				++ExistingPrimaryEnemies;
			}
		}
		SpawnLocation = PlayerPawn->GetActorLocation()
			+ FVector(
				FMath::Max(EndlessForwardSpawnDistance, 300.0f)
					+ ExistingPrimaryEnemies
						* FMath::Max(EndlessForwardSpawnSpacing, 80.0f),
				0.0f,
				SpawnHeightOffset);
	}
	else if (!FindSpawnLocation(SpawnLocation))
	{
		return nullptr;
	}
	if (PlayerPawn && bConstrainTo2DPlane)
	{
		SpawnLocation.Y = PlayerPawn->GetActorLocation().Y;
	}
	if (PlayerPawn && bMatchPlayerHeight)
	{
		SpawnLocation.Z =
			PlayerPawn->GetActorLocation().Z + SpawnHeightOffset;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = SpawnOwner ? SpawnOwner : this;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AImmortalMonsterCharacter* Monster =
		GetWorld()->SpawnActor<AImmortalMonsterCharacter>(
			ClassToSpawn,
			SpawnLocation,
			GetActorRotation(),
			SpawnParams);
	if (!Monster)
	{
		return nullptr;
	}
	SpawnedMonsters.Add(Monster);
	Monster->OnMonsterDeath.AddDynamic(
		this, &AImmortalMonsterSpawner::HandleMonsterDeath);
	Monster->OnDestroyed.AddDynamic(
		this, &AImmortalMonsterSpawner::HandleSpawnedMonsterDestroyed);
	if (bBoss)
	{
		Monster->OnBossPhaseChanged.AddDynamic(
			this, &AImmortalMonsterSpawner::HandleBossPhaseChanged);
	}
	FImmortalEndlessFloorDescriptor Descriptor;
	if (!UImmortalEndlessDungeonLibrary::GetFloorDescriptor(
		ActiveEndlessFloor, Descriptor))
	{
		SpawnedMonsters.Remove(Monster);
		Monster->OnMonsterDeath.RemoveDynamic(
			this, &AImmortalMonsterSpawner::HandleMonsterDeath);
		Monster->OnBossPhaseChanged.RemoveDynamic(
			this, &AImmortalMonsterSpawner::HandleBossPhaseChanged);
		Monster->OnDestroyed.RemoveDynamic(
			this, &AImmortalMonsterSpawner::HandleSpawnedMonsterDestroyed);
		Monster->Destroy();
		return nullptr;
	}
	Monster->ConfigureForEndlessFloor(
		ActiveEndlessFloor,
		bBoss,
		bElite,
		ActiveEndlessRunId,
		MapSystemState.ActiveMapId,
		Descriptor.HealthMultiplier,
		Descriptor.AttackMultiplier,
		Descriptor.DefenseBonus);
	BP_OnMonsterSpawned(Monster);
	return Monster;
}

void AImmortalMonsterSpawner::HandleEndlessDungeonMonsterDeath(
	AImmortalMonsterCharacter* Monster,
	AActor* DamageCauser)
{
	if (!bEndlessDungeonActive || bResolvingEndlessDungeon || !Monster
		|| !Monster->IsEndlessEnemy()
		|| Monster->GetEndlessRunId() != ActiveEndlessRunId
		|| Monster->GetEndlessFloor() != ActiveEndlessFloor)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Ignored stale or unrelated Endless Dungeon death"));
		return;
	}
	if (Monster->ActorHasTag(TEXT("EndlessMinion")))
	{
		UE_LOG(LogTemp, Display,
			TEXT("Endless Dungeon summon defeated without floor progress"));
		UpdateEndlessDungeonHud();
		return;
	}

	FImmortalEndlessFloorDescriptor Descriptor;
	if (!UImmortalEndlessDungeonLibrary::GetFloorDescriptor(
		ActiveEndlessFloor, Descriptor))
	{
		FinishEndlessDungeon(
			false,
			FText::FromString(TEXT("秘境层配置失效，挑战已安全终止")),
			FLinearColor(1.0f, 0.18f, 0.12f, 1.0f));
		return;
	}
	if (Descriptor.bBoss)
	{
		if (ActiveEndlessBoss.Get() != Monster)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Ignored non-authoritative Endless Dungeon boss death"));
			return;
		}
		ActiveEndlessBoss.Reset();
		ActiveEndlessKills = ActiveEndlessRequiredKills;
	}
	else
	{
		ActiveEndlessKills = FMath::Min(
			ActiveEndlessKills + 1,
			ActiveEndlessRequiredKills);
	}
	if (AImmortalPlayerCharacter* GrowthPlayer =
		Cast<AImmortalPlayerCharacter>(DamageCauser))
	{
		GrowthPlayer->NotifyPetCombatKill(Monster);
	}
	UpdateEndlessDungeonHud();
	if (ActiveEndlessKills < ActiveEndlessRequiredKills)
	{
		return;
	}

	TGuardValue<bool> ResolveGuard(bResolvingEndlessDungeon, true);
	const int32 ClearedFloor = ActiveEndlessFloor;
	const float ClearSeconds = GetWorld()
		? FMath::Max(
			GetWorld()->GetTimeSeconds() - EndlessFloorStartTime,
			0.01f)
		: 0.01f;
	AImmortalPlayerCharacter* Player = Cast<AImmortalPlayerCharacter>(
		UGameplayStatics::GetPlayerCharacter(this, 0));
	const FImmortalEndlessDungeonFloorClearResult ClearResult = Player
		? Player->CommitEndlessDungeonFloorClear(
			ClearedFloor, ClearSeconds)
		: FImmortalEndlessDungeonFloorClearResult();
	if (!ClearResult.bSucceeded)
	{
		FinishEndlessDungeon(
			false,
			FText::FromString(TEXT("秘境奖励写盘失败，本层进度已回滚")),
			FLinearColor(1.0f, 0.18f, 0.12f, 1.0f),
			Monster);
		return;
	}

	ClearOtherMonsters(Monster);
	const FImmortalEndlessDungeonRules Rules =
		UImmortalEndlessDungeonLibrary::GetRules();
	if (ClearedFloor >= Rules.MaximumFloor)
	{
		FinishEndlessDungeon(
			true,
			FText::FromString(TEXT("已通关当前版本无尽秘境全部层数")),
			FLinearColor(1.0f, 0.82f, 0.22f, 1.0f),
			Monster);
		return;
	}

	++ActiveEndlessFloor;
	ActiveEndlessKills = 0;
	ActiveEndlessRequiredKills = 0;
	ShowBossMessage(
		FText::FromString(FString::Printf(
			TEXT("第 %d 层已通过，正在进入第 %d 层%s"),
			ClearedFloor,
			ActiveEndlessFloor,
			ClearResult.bRewardPending
				? TEXT("；奖励已保留待领取")
				: TEXT(""))),
		FLinearColor(0.38f, 1.0f, 0.58f, 1.0f));
	GetWorldTimerManager().SetTimer(
		EndlessDungeonNextFloorTimerHandle,
		this,
		&AImmortalMonsterSpawner::AdvanceEndlessDungeonFloor,
		0.85f,
		false);
	UE_LOG(LogTemp, Display,
		TEXT("Endless Dungeon floor committed: run=%s cleared=%d next=%d elapsed=%.2f delivered=%s pending=%s"),
		*ActiveEndlessRunId.ToString(EGuidFormats::DigitsWithHyphens),
		ClearedFloor,
		ActiveEndlessFloor,
		ClearSeconds,
		ClearResult.bRewardDelivered ? TEXT("true") : TEXT("false"),
		ClearResult.bRewardPending ? TEXT("true") : TEXT("false"));
}

void AImmortalMonsterSpawner::AdvanceEndlessDungeonFloor()
{
	GetWorldTimerManager().ClearTimer(EndlessDungeonNextFloorTimerHandle);
	if (!bEndlessDungeonActive || bResolvingEndlessDungeon)
	{
		return;
	}
	if (!SpawnEndlessFloor())
	{
		FinishEndlessDungeon(
			false,
			FText::FromString(TEXT("下一层生成失败，已安全退出无尽秘境")),
			FLinearColor(1.0f, 0.18f, 0.12f, 1.0f));
	}
}

void AImmortalMonsterSpawner::FinishEndlessDungeon(
	const bool bCompletedFloor,
	const FText& Message,
	const FLinearColor& Color,
	AImmortalMonsterCharacter* DefeatedMonster)
{
	if (!bEndlessDungeonActive)
	{
		return;
	}
	TGuardValue<bool> ResolveGuard(bResolvingEndlessDungeon, true);
	const FGuid CompletedRunId = ActiveEndlessRunId;
	const int32 FinalFloor = ActiveEndlessFloor;
	GetWorldTimerManager().ClearTimer(EndlessDungeonHudTimerHandle);
	GetWorldTimerManager().ClearTimer(EndlessDungeonNextFloorTimerHandle);
	ClearOtherMonsters(DefeatedMonster);
	if (DefeatedMonster)
	{
		DefeatedMonster->OnMonsterDeath.RemoveDynamic(
			this, &AImmortalMonsterSpawner::HandleMonsterDeath);
		DefeatedMonster->OnBossPhaseChanged.RemoveDynamic(
			this, &AImmortalMonsterSpawner::HandleBossPhaseChanged);
		DefeatedMonster->OnDestroyed.RemoveDynamic(
			this, &AImmortalMonsterSpawner::HandleSpawnedMonsterDestroyed);
	}
	ActiveEndlessBoss.Reset();
	bEndlessDungeonActive = false;
	ActiveEndlessRunId.Invalidate();
	ActiveEndlessFloor = 0;
	ActiveEndlessRunStartFloor = 0;
	ActiveEndlessKills = 0;
	ActiveEndlessRequiredKills = 0;
	EndlessDungeonStartTime = 0.0f;
	EndlessFloorStartTime = 0.0f;
	UpdateStageHud();
	ShowBossMessage(Message, Color);
	if (bResumeSpawningAfterEndlessDungeon)
	{
		GetWorldTimerManager().SetTimer(
			EndlessDungeonResumeTimerHandle,
			this,
			&AImmortalMonsterSpawner::ResumeMapSpawningAfterEndlessDungeon,
			bCompletedFloor ? 0.85f : 0.25f,
			false);
	}
	UE_LOG(LogTemp, Display,
		TEXT("Endless Dungeon finished: run=%s finalFloor=%d completedFloor=%s map=%s stage=%d kills=%d revision=%d"),
		*CompletedRunId.ToString(EGuidFormats::DigitsWithHyphens),
		FinalFloor,
		bCompletedFloor ? TEXT("true") : TEXT("false"),
		*MapSystemState.ActiveMapId.ToString(),
		CurrentStage,
		CurrentStageKills,
		MapSystemRevision);
}

void AImmortalMonsterSpawner::UpdateEndlessDungeonHud()
{
	if (!bEndlessDungeonActive)
	{
		return;
	}
	AImmortalPlayerCharacter* Player = Cast<AImmortalPlayerCharacter>(
		UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!Player || Player->IsDead())
	{
		FinishEndlessDungeon(
			false,
			FText::FromString(TEXT("角色战败，无尽秘境挑战结束，本层不结算奖励")),
			FLinearColor(1.0f, 0.18f, 0.12f, 1.0f));
		return;
	}
	Player->UpdateEndlessDungeonProgress(
		GetEndlessDungeonRuntimeSnapshot());
}

void AImmortalMonsterSpawner::ResumeMapSpawningAfterEndlessDungeon()
{
	GetWorldTimerManager().ClearTimer(EndlessDungeonResumeTimerHandle);
	const bool bShouldResume = bResumeSpawningAfterEndlessDungeon;
	bResumeSpawningAfterEndlessDungeon = false;
	if (!bShouldResume || bMapTransitionInProgress || bCurrentMapCompleted
		|| bWorldBossChallengeActive || bEndlessDungeonActive)
	{
		return;
	}
	SpawnUntilInitialCount();
	StartSpawning();
}

bool AImmortalMonsterSpawner::DamageActiveEndlessBossForDevelopment(
	const float MaximumHealthFraction)
{
#if !UE_BUILD_SHIPPING
	AImmortalMonsterCharacter* Boss = ActiveEndlessBoss.Get();
	if (!bEndlessDungeonActive || !Boss || Boss->IsDead()
		|| !Boss->IsEndlessEnemy()
		|| Boss->GetEndlessRunId() != ActiveEndlessRunId
		|| Boss->GetEndlessFloor() != ActiveEndlessFloor
		|| MaximumHealthFraction <= 0.0f)
	{
		return false;
	}
	UGameplayStatics::ApplyDamage(
		Boss,
		Boss->GetMaxHealth()
			* FMath::Clamp(MaximumHealthFraction, 0.01f, 0.95f),
		nullptr,
		UGameplayStatics::GetPlayerCharacter(this, 0),
		UDamageType::StaticClass());
	return !Boss->IsDead();
#else
	return false;
#endif
}

bool AImmortalMonsterSpawner::ClearActiveEndlessFloorForDevelopment()
{
#if !UE_BUILD_SHIPPING
	if (!bEndlessDungeonActive)
	{
		return false;
	}
	const int32 PreviousFloor = ActiveEndlessFloor;
	const TArray<TWeakObjectPtr<AImmortalMonsterCharacter>> Monsters =
		SpawnedMonsters;
	for (const TWeakObjectPtr<AImmortalMonsterCharacter>& Entry : Monsters)
	{
		AImmortalMonsterCharacter* Monster = Entry.Get();
		if (!Monster || Monster->IsDead()
			|| !Monster->IsEndlessEnemy()
			|| Monster->GetEndlessRunId() != ActiveEndlessRunId
			|| Monster->GetEndlessFloor() != PreviousFloor
			|| Monster->ActorHasTag(TEXT("EndlessMinion")))
		{
			continue;
		}
		UGameplayStatics::ApplyDamage(
			Monster,
			Monster->GetMaxHealth() + 1000000.0f,
			nullptr,
			UGameplayStatics::GetPlayerCharacter(this, 0),
			UDamageType::StaticClass());
	}
	return !bEndlessDungeonActive || ActiveEndlessFloor != PreviousFloor;
#else
	return false;
#endif
}

bool AImmortalMonsterSpawner::FailActiveEndlessDungeonForDevelopment()
{
#if !UE_BUILD_SHIPPING
	if (!bEndlessDungeonActive)
	{
		return false;
	}
	FinishEndlessDungeon(
		false,
		FText::FromString(TEXT("开发验证：无尽秘境失败结算")),
		FLinearColor(1.0f, 0.18f, 0.12f, 1.0f));
	return !bEndlessDungeonActive;
#else
	return false;
#endif
}

AImmortalMonsterCharacter* AImmortalMonsterSpawner::SpawnWorldBoss(
	const FImmortalWorldBossDefinition& Definition)
{
	if (!GetWorld() || !Definition.IsValid())
	{
		return nullptr;
	}
	const TSubclassOf<AImmortalMonsterCharacter> EffectiveBossClass = BossMonsterClass
		? BossMonsterClass
		: (DefaultBossMonsterClass ? DefaultBossMonsterClass : MonsterClass);
	if (!EffectiveBossClass)
	{
		return nullptr;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	FVector SpawnLocation = PlayerPawn
		? PlayerPawn->GetActorLocation() + FVector(540.0f, 0.0f, SpawnHeightOffset)
		: GetActorLocation() + FVector(540.0f, 0.0f, SpawnHeightOffset);
	if (PlayerPawn && bConstrainTo2DPlane)
	{
		SpawnLocation.Y = PlayerPawn->GetActorLocation().Y;
	}
	if (PlayerPawn && bMatchPlayerHeight)
	{
		SpawnLocation.Z = PlayerPawn->GetActorLocation().Z + SpawnHeightOffset;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AImmortalMonsterCharacter* Boss = GetWorld()->SpawnActor<AImmortalMonsterCharacter>(
		EffectiveBossClass, SpawnLocation, GetActorRotation(), SpawnParams);
	if (!Boss)
	{
		return nullptr;
	}
	SpawnedMonsters.Add(Boss);
	Boss->OnMonsterDeath.AddDynamic(this, &AImmortalMonsterSpawner::HandleMonsterDeath);
	Boss->OnBossPhaseChanged.AddDynamic(this, &AImmortalMonsterSpawner::HandleBossPhaseChanged);
	Boss->OnDestroyed.AddDynamic(this, &AImmortalMonsterSpawner::HandleSpawnedMonsterDestroyed);
	Boss->ConfigureAsWorldBoss(Definition, MapSystemState.ActiveMapId);
	BP_OnBossSpawned(Boss, Definition.RecommendedStage);
	BP_OnMonsterSpawned(Boss);
	return Boss;
}

void AImmortalMonsterSpawner::FinishWorldBossChallenge(
	const bool bDefeated,
	const FText& Message,
	const FLinearColor& Color,
	AImmortalMonsterCharacter* DefeatedBoss)
{
	if (!bWorldBossChallengeActive)
	{
		return;
	}
	TGuardValue<bool> ResolveGuard(bResolvingWorldBossChallenge, true);
	const FName CompletedBossId = ActiveWorldBossDefinition.BossId;
	GetWorldTimerManager().ClearTimer(WorldBossTimeoutTimerHandle);
	GetWorldTimerManager().ClearTimer(WorldBossHudTimerHandle);
	ClearOtherMonsters(DefeatedBoss);
	if (!DefeatedBoss && ActiveWorldBoss.IsValid())
	{
		AImmortalMonsterCharacter* Boss = ActiveWorldBoss.Get();
		Boss->OnMonsterDeath.RemoveDynamic(this, &AImmortalMonsterSpawner::HandleMonsterDeath);
		Boss->OnBossPhaseChanged.RemoveDynamic(this, &AImmortalMonsterSpawner::HandleBossPhaseChanged);
		Boss->OnDestroyed.RemoveDynamic(this, &AImmortalMonsterSpawner::HandleSpawnedMonsterDestroyed);
		Boss->Destroy();
	}
	ActiveWorldBoss.Reset();
	bWorldBossChallengeActive = false;
	WorldBossChallengeStartTime = 0.0f;
	WorldBossChallengeEndTime = 0.0f;
	ActiveWorldBossDefinition = FImmortalWorldBossDefinition();
	UpdateStageHud();
	ShowBossMessage(Message, Color);

	if (bResumeSpawningAfterWorldBoss)
	{
		GetWorldTimerManager().SetTimer(
			WorldBossResumeTimerHandle,
			this,
			&AImmortalMonsterSpawner::ResumeMapSpawningAfterWorldBoss,
			bDefeated ? 0.85f : 0.25f,
			false);
	}
	UE_LOG(LogTemp, Display,
		TEXT("World Boss challenge finished: %s | defeated=%s | map=%s stage=%d progress=%d"),
		*CompletedBossId.ToString(), bDefeated ? TEXT("true") : TEXT("false"),
		*MapSystemState.ActiveMapId.ToString(), CurrentStage, CurrentStageKills);
}

void AImmortalMonsterSpawner::HandleWorldBossTimeout()
{
	if (!bWorldBossChallengeActive)
	{
		return;
	}
	FinishWorldBossChallenge(
		false,
		FText::FromString(FString::Printf(
			TEXT("挑战超时：%s退去，未产生奖励"),
			*ActiveWorldBossDefinition.DisplayName.ToString())),
		FLinearColor(1.0f, 0.28f, 0.18f, 1.0f));
}

void AImmortalMonsterSpawner::UpdateWorldBossHud()
{
	if (!bWorldBossChallengeActive)
	{
		return;
	}
	AImmortalPlayerCharacter* Player = Cast<AImmortalPlayerCharacter>(
		UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!Player || Player->IsDead())
	{
		FinishWorldBossChallenge(
			false,
			FText::FromString(TEXT("角色战败，世界妖王挑战结束且不结算奖励")),
			FLinearColor(1.0f, 0.18f, 0.12f, 1.0f));
		return;
	}
	Player->UpdateWorldBossProgress(GetWorldBossRuntimeSnapshot());
}

void AImmortalMonsterSpawner::ResumeMapSpawningAfterWorldBoss()
{
	GetWorldTimerManager().ClearTimer(WorldBossResumeTimerHandle);
	const bool bShouldResume = bResumeSpawningAfterWorldBoss;
	bResumeSpawningAfterWorldBoss = false;
	if (!bShouldResume || bMapTransitionInProgress || bCurrentMapCompleted
		|| bWorldBossChallengeActive || bEndlessDungeonActive)
	{
		return;
	}
	SpawnUntilInitialCount();
	StartSpawning();
}

int32 AImmortalMonsterSpawner::GetAliveMonsterCount() const
{
	int32 AliveCount = 0;
	for (const TWeakObjectPtr<AImmortalMonsterCharacter>& Monster : SpawnedMonsters)
	{
		if (Monster.IsValid() && !Monster->IsDead())
		{
			++AliveCount;
		}
	}
	return AliveCount;
}

void AImmortalMonsterSpawner::StartSpawning()
{
	bSpawningRequested = true;
	if (!GetWorld() || bMapTransitionInProgress || bCurrentMapCompleted
		|| bWorldBossChallengeActive || bEndlessDungeonActive)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&AImmortalMonsterSpawner::HandleSpawnTimer,
		FMath::Max(SpawnInterval, 0.1f),
		true);
}

void AImmortalMonsterSpawner::HandleSpawnTimer()
{
	if (!bMapTransitionInProgress && !bCurrentMapCompleted
		&& !bWorldBossChallengeActive && !bEndlessDungeonActive)
	{
		SpawnMonster();
	}
}

void AImmortalMonsterSpawner::SuspendSpawnTimer()
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
}

void AImmortalMonsterSpawner::StopSpawning()
{
	bSpawningRequested = false;
	SuspendSpawnTimer();
}

void AImmortalMonsterSpawner::SuspendAdventureForCultivation()
{
	if (bWorldBossChallengeActive)
	{
		CancelWorldBossChallenge();
	}
	if (bEndlessDungeonActive)
	{
		CancelEndlessDungeon();
	}
	GetWorldTimerManager().ClearTimer(WorldBossResumeTimerHandle);
	GetWorldTimerManager().ClearTimer(EndlessDungeonResumeTimerHandle);
	bResumeSpawningAfterWorldBoss = false;
	bResumeSpawningAfterEndlessDungeon = false;
	StopSpawning();
	ClearAllMonstersAndDrops();
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Adventure channel suspended for cultivation recovery: map=%s stage=%d"),
		*MapSystemState.ActiveMapId.ToString(),
		CurrentStage);
}

bool AImmortalMonsterSpawner::ResumeAdventureAfterCultivation()
{
	if (!GetWorld() || bMapTransitionInProgress || bCurrentMapCompleted
		|| bWorldBossChallengeActive || bEndlessDungeonActive)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Adventure channel could not resume: transition=%s completed=%s worldBoss=%s endless=%s"),
			bMapTransitionInProgress ? TEXT("true") : TEXT("false"),
			bCurrentMapCompleted ? TEXT("true") : TEXT("false"),
			bWorldBossChallengeActive ? TEXT("true") : TEXT("false"),
			bEndlessDungeonActive ? TEXT("true") : TEXT("false"));
		return false;
	}

	SpawnUntilInitialCount();
	StartSpawning();
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Adventure channel resumed after cultivation recovery: map=%s stage=%d alive=%d"),
		*MapSystemState.ActiveMapId.ToString(),
		CurrentStage,
		GetAliveMonsterCount());
	return bSpawningRequested;
}

void AImmortalMonsterSpawner::SpawnUntilInitialCount()
{
	if (bMapTransitionInProgress || bCurrentMapCompleted
		|| bWorldBossChallengeActive || bEndlessDungeonActive)
	{
		return;
	}
	const int32 DesiredCount = IsCurrentStageBossStage()
		? 1
		: FMath::Min(FMath::Max(InitialSpawnCount, 0), FMath::Max(MaxAliveMonsters, 1));
	for (int32 Index = GetAliveMonsterCount(); Index < DesiredCount; ++Index)
	{
		if (!SpawnMonster())
		{
			break;
		}
	}
}

AImmortalMonsterCharacter* AImmortalMonsterSpawner::SpawnConfiguredMonster(
	const bool bSpawnBoss,
	const bool bIgnoreAliveLimit,
	AActor* SpawnOwner)
{
	RemoveInvalidMonsters();
	const bool bWorldBossOwnedSpawn = SpawnOwner
		&& SpawnOwner->ActorHasTag(TEXT("WorldBoss"));
	if (!GetWorld() || !MonsterClass || bMapTransitionInProgress
		|| (bCurrentMapCompleted && !bWorldBossOwnedSpawn)
		|| (bWorldBossChallengeActive && !bWorldBossOwnedSpawn)
		|| bEndlessDungeonActive)
	{
		return nullptr;
	}
	if (!bIgnoreAliveLimit
		&& GetAliveMonsterCount() >= (bSpawnBoss ? 1 : FMath::Max(MaxAliveMonsters, 1)))
	{
		return nullptr;
	}

	const TSubclassOf<AImmortalMonsterCharacter> EffectiveAlternateClass =
		AlternateMonsterClass ? AlternateMonsterClass : DefaultAlternateMonsterClass;
	const TSubclassOf<AImmortalMonsterCharacter> EffectiveBossClass = BossMonsterClass
		? BossMonsterClass
		: (DefaultBossMonsterClass ? DefaultBossMonsterClass : MonsterClass);
	const TSubclassOf<AImmortalMonsterCharacter> ClassToSpawn = bSpawnBoss
		? EffectiveBossClass
		: (EffectiveAlternateClass && FMath::RandBool() ? EffectiveAlternateClass : MonsterClass);

	FVector SpawnLocation;
	if (!FindSpawnLocation(SpawnLocation))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = SpawnOwner ? SpawnOwner : this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AImmortalMonsterCharacter* Monster = GetWorld()->SpawnActor<AImmortalMonsterCharacter>(
		ClassToSpawn, SpawnLocation, GetActorRotation(), SpawnParams);
	if (!Monster)
	{
		return nullptr;
	}

	SpawnedMonsters.Add(Monster);
	Monster->OnMonsterDeath.AddDynamic(this, &AImmortalMonsterSpawner::HandleMonsterDeath);
	Monster->OnDestroyed.AddDynamic(this, &AImmortalMonsterSpawner::HandleSpawnedMonsterDestroyed);
	if (bSpawnBoss)
	{
		Monster->OnBossPhaseChanged.AddDynamic(this, &AImmortalMonsterSpawner::HandleBossPhaseChanged);
		Monster->ConfigureAsMapBoss(MapSystemState.ActiveMapId, CurrentStage);
		BP_OnBossSpawned(Monster, CurrentStage);
		ShowBossMessage(
			FText::FromString(FString::Printf(TEXT("%s · 第 %d 关：%s出现"),
				*ActiveMapDefinition.DisplayName.ToString(), CurrentStage,
				*ActiveMapDefinition.BossName.ToString())),
			ActiveMapDefinition.BossColor);
		UE_LOG(LogTemp, Display, TEXT("%s boss %s spawned at stage %d"),
			*ActiveMapDefinition.DisplayName.ToString(), *ActiveMapDefinition.BossName.ToString(), CurrentStage);
	}
	else
	{
		if (bWorldBossOwnedSpawn)
		{
			Monster->ConfigureAsWorldBossMinion(
				MapSystemState.ActiveMapId,
				FMath::Max(ActiveWorldBossDefinition.RecommendedStage, 1));
		}
		else
		{
			Monster->ConfigureForMapStage(MapSystemState.ActiveMapId, CurrentStage);
		}
	}
	BP_OnMonsterSpawned(Monster);
	return Monster;
}

void AImmortalMonsterSpawner::SpawnBossMinions(
	AImmortalMonsterCharacter* Boss,
	const int32 Count)
{
	for (int32 Index = 0; Index < FMath::Max(Count, 0); ++Index)
	{
		if (AImmortalMonsterCharacter* Minion = SpawnConfiguredMonster(false, true, Boss))
		{
			Minion->Tags.AddUnique(TEXT("BossMinion"));
		}
	}
	UE_LOG(LogTemp, Display, TEXT("%s summoned %d minions"),
		bWorldBossChallengeActive
			? *ActiveWorldBossDefinition.DisplayName.ToString()
			: *ActiveMapDefinition.DisplayName.ToString(),
		FMath::Max(Count, 0));
}

void AImmortalMonsterSpawner::HandleBossPhaseChanged(
	AImmortalMonsterCharacter* Boss,
	const int32 NewPhase)
{
	if (bMapTransitionInProgress || !Boss || Boss->IsDead() || !Boss->IsBoss())
	{
		return;
	}
	const bool bEndlessPhase = bEndlessDungeonActive
		&& Boss->IsEndlessEnemy()
		&& Boss->GetEndlessRunId() == ActiveEndlessRunId
		&& Boss->GetEndlessFloor() == ActiveEndlessFloor
		&& ActiveEndlessBoss.Get() == Boss;
	if (bEndlessPhase)
	{
		const FImmortalEndlessDungeonRules Rules =
			UImmortalEndlessDungeonLibrary::GetRules();
		const int32 SummonCount = NewPhase == 2
			? Rules.PhaseTwoSummonCount
			: (NewPhase == 3 ? Rules.PhaseThreeSummonCount : 0);
		for (int32 Index = 0; Index < FMath::Max(SummonCount, 0); ++Index)
		{
			if (AImmortalMonsterCharacter* Minion =
				SpawnEndlessEnemy(false, false, Boss))
			{
				Minion->Tags.AddUnique(TEXT("EndlessMinion"));
			}
		}
		ShowBossMessage(
			FText::FromString(FString::Printf(
				TEXT("无尽秘境第 %d 层首领进入第 %d 阶段，召唤 %d 只妖兽"),
				ActiveEndlessFloor, NewPhase, FMath::Max(SummonCount, 0))),
			NewPhase >= 3
				? FLinearColor(1.0f, 0.18f, 0.08f, 1.0f)
				: FLinearColor(0.72f, 0.38f, 1.0f, 1.0f));
		UpdateEndlessDungeonHud();
		return;
	}
	if (bEndlessDungeonActive)
	{
		return;
	}
	const bool bWorldBossPhase = bWorldBossChallengeActive
		&& Boss->IsWorldBoss()
		&& ActiveWorldBoss.Get() == Boss
		&& Boss->GetWorldBossId() == ActiveWorldBossDefinition.BossId;
	if (!bWorldBossPhase && Boss->GetConfiguredMapId() != MapSystemState.ActiveMapId)
	{
		return;
	}
	const int32 SummonCount = NewPhase == 2
		? (bWorldBossPhase
			? ActiveWorldBossDefinition.PhaseTwoSummonCount
			: PhaseTwoSummonCount)
		: (NewPhase == 3
			? (bWorldBossPhase
				? ActiveWorldBossDefinition.PhaseThreeSummonCount
				: PhaseThreeSummonCount)
			: 0);
	SpawnBossMinions(Boss, SummonCount);
	const FText BossName = bWorldBossPhase
		? ActiveWorldBossDefinition.DisplayName
		: ActiveMapDefinition.BossName;
	const FString PhaseMessage = NewPhase >= 3
		? FString::Printf(TEXT("%s进入狂暴阶段！召唤 %d 只妖兽"),
			*BossName.ToString(), SummonCount)
		: FString::Printf(TEXT("%s进入第 %d 阶段，召唤 %d 只妖兽"),
			*BossName.ToString(), NewPhase, SummonCount);
	ShowBossMessage(
		FText::FromString(PhaseMessage),
		NewPhase >= 3
			? FLinearColor(1.0f, 0.12f, 0.08f, 1.0f)
			: (bWorldBossPhase
				? ActiveWorldBossDefinition.DisplayColor
				: ActiveMapDefinition.BossColor));
}

void AImmortalMonsterSpawner::AdvanceStage(AImmortalMonsterCharacter* DefeatedMonster)
{
	const int32 MaximumStage = GetCurrentMaximumStage();
	ClearOtherMonsters(DefeatedMonster);
	if (CurrentStage >= MaximumStage)
	{
		CurrentStage = MaximumStage;
		CurrentStageKills = 1;
		bCurrentMapCompleted = true;
		SuspendSpawnTimer();
		UE_LOG(LogTemp, Display, TEXT("%s completed after defeating final boss at stage %d"),
			*ActiveMapDefinition.DisplayName.ToString(), CurrentStage);
		return;
	}

	++CurrentStage;
	CurrentStageKills = 0;
	UE_LOG(LogTemp, Display, TEXT("%s advanced to stage %d%s"),
		*ActiveMapDefinition.DisplayName.ToString(), CurrentStage,
		IsCurrentStageBossStage() ? TEXT(" (boss stage)") : TEXT(""));
	if (IsCurrentStageBossStage())
	{
		ShowBossMessage(
			FText::FromString(FString::Printf(TEXT("%s第 %d 关为守关首领“%s”"),
				*ActiveMapDefinition.DisplayName.ToString(), CurrentStage,
				*ActiveMapDefinition.BossName.ToString())),
			ActiveMapDefinition.BossColor);
	}
}

void AImmortalMonsterSpawner::ClearOtherMonsters(AImmortalMonsterCharacter* Exception)
{
	const TArray<TWeakObjectPtr<AImmortalMonsterCharacter>> MonstersToClear = SpawnedMonsters;
	for (const TWeakObjectPtr<AImmortalMonsterCharacter>& Monster : MonstersToClear)
	{
		if (!Monster.IsValid() || Monster.Get() == Exception)
		{
			continue;
		}
		Monster->OnMonsterDeath.RemoveDynamic(this, &AImmortalMonsterSpawner::HandleMonsterDeath);
		Monster->OnBossPhaseChanged.RemoveDynamic(this, &AImmortalMonsterSpawner::HandleBossPhaseChanged);
		Monster->OnDestroyed.RemoveDynamic(this, &AImmortalMonsterSpawner::HandleSpawnedMonsterDestroyed);
		Monster->Destroy();
	}
	RemoveInvalidMonsters();
}

void AImmortalMonsterSpawner::ClearAllMonstersForWorldBoss()
{
	const int32 PreviousCount = GetAliveMonsterCount();
	ClearOtherMonsters(nullptr);
	UE_LOG(LogTemp, Display,
		TEXT("World Boss scene prepared: cleared %d combat monsters; physical drops were preserved"),
		PreviousCount);
}

void AImmortalMonsterSpawner::ClearAllMonstersAndDrops()
{
	int32 MonsterCount = 0;
	const TArray<TWeakObjectPtr<AImmortalMonsterCharacter>> MonstersToClear = SpawnedMonsters;
	for (const TWeakObjectPtr<AImmortalMonsterCharacter>& Monster : MonstersToClear)
	{
		if (!Monster.IsValid())
		{
			continue;
		}
		Monster->OnMonsterDeath.RemoveDynamic(this, &AImmortalMonsterSpawner::HandleMonsterDeath);
		Monster->OnBossPhaseChanged.RemoveDynamic(this, &AImmortalMonsterSpawner::HandleBossPhaseChanged);
		Monster->OnDestroyed.RemoveDynamic(this, &AImmortalMonsterSpawner::HandleSpawnedMonsterDestroyed);
		Monster->Destroy();
		++MonsterCount;
	}
	SpawnedMonsters.Reset();

	int32 EquipmentDrops = 0;
	int32 MaterialDrops = 0;
	int32 SpiritStoneDrops = 0;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AImmortalEquipmentDrop> It(World); It; ++It)
		{
			It->Destroy();
			++EquipmentDrops;
		}
		for (TActorIterator<AImmortalMaterialDrop> It(World); It; ++It)
		{
			It->Destroy();
			++MaterialDrops;
		}
		for (TActorIterator<AImmortalSpiritStoneDrop> It(World); It; ++It)
		{
			It->Destroy();
			++SpiritStoneDrops;
		}
	}
	UE_LOG(LogTemp, Display,
		TEXT("Map transition cleanup: monsters=%d equipmentDrops=%d materialDrops=%d spiritStoneDrops=%d"),
		MonsterCount, EquipmentDrops, MaterialDrops, SpiritStoneDrops);
}

int32 AImmortalMonsterSpawner::GetRequiredKillsForCurrentStage() const
{
	return IsCurrentStageBossStage() || bCurrentMapCompleted
		? 1
		: FMath::Max(KillsPerStage, 1);
}

void AImmortalMonsterSpawner::ShowBossMessage(
	const FText& Message,
	const FLinearColor& Color) const
{
	if (AImmortalPlayerCharacter* Player = Cast<AImmortalPlayerCharacter>(
		UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->ShowBossMessage(Message, Color);
	}
}

void AImmortalMonsterSpawner::ApplyActiveMapPresentation()
{
	if (!GetWorld())
	{
		return;
	}

	const bool bIsQingyunMountain =
		MapSystemState.ActiveMapId == UImmortalMapLibrary::GetQingyunMountainId();
	const bool bHasQingyunBackground =
		QingyunMountainBackground && QingyunMountainBackgroundSprite;
	bQingyunBackgroundRequested = bIsQingyunMountain && bHasQingyunBackground;

	GetWorldTimerManager().ClearTimer(QingyunBackgroundRefreshTimerHandle);
	if (QingyunMountainBackground)
	{
		if (bQingyunBackgroundRequested)
		{
			QingyunMountainBackground->SetSprite(QingyunMountainBackgroundSprite);
			QingyunMountainBackground->SetVisibility(false, true);
			QingyunMountainBackground->SetHiddenInGame(true);
			bHasLoggedQingyunBackgroundFit = false;
			RefreshQingyunBackgroundGeometry();
			GetWorldTimerManager().SetTimer(
				QingyunBackgroundRefreshTimerHandle,
				this,
				&AImmortalMonsterSpawner::RefreshQingyunBackgroundGeometry,
				QingyunBackgroundRefreshInterval,
				true,
				0.05f);
		}
		else
		{
			QingyunMountainBackground->SetVisibility(false, true);
			QingyunMountainBackground->SetHiddenInGame(true);
		}
	}

	// Keep the old scene visible until the new backdrop has a valid camera fit.
	// Visibility never changes TileMap collision, so the existing walkable floor
	// continues to drive gameplay after its rendering is hidden.
	const bool bQingyunBackgroundIsVisible =
		bQingyunBackgroundRequested
		&& QingyunMountainBackground
		&& QingyunMountainBackground->IsVisible();
	int32 TintedTileMaps = 0;
	for (TActorIterator<APaperTileMapActor> It(GetWorld()); It; ++It)
	{
		if (UPaperTileMapComponent* TileMap = It->GetRenderComponent())
		{
			TileMap->SetTileMapColor(ActiveMapDefinition.SceneTint);
			TileMap->SetVisibility(!bQingyunBackgroundIsVisible, true);
			++TintedTileMaps;
		}
	}

	if (bIsQingyunMountain && !bHasQingyunBackground)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Qingyun runtime background is unavailable; keeping legacy TileMap. Expected cooked sprite: ")
			TEXT("/Game/GAME/Asset/backgrounds/generated/SP_QingyunMountain_Background"));
	}
	UE_LOG(LogTemp, Display,
		TEXT("Map presentation applied: %s tint=(%.2f, %.2f, %.2f) tileMaps=%d qingyunBackdrop=%s"),
		*ActiveMapDefinition.DisplayName.ToString(),
		ActiveMapDefinition.SceneTint.R,
		ActiveMapDefinition.SceneTint.G,
		ActiveMapDefinition.SceneTint.B,
		TintedTileMaps,
		bQingyunBackgroundIsVisible ? TEXT("visible") : TEXT("legacy/fallback"));
}

void AImmortalMonsterSpawner::RefreshQingyunBackgroundGeometry()
{
	if (!bQingyunBackgroundRequested
		|| !QingyunMountainBackground
		|| !QingyunMountainBackgroundSprite
		|| !GetWorld())
	{
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController)
	{
		return;
	}

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth <= 0 || ViewportHeight <= 0)
	{
		return;
	}

	auto DeprojectToBackgroundPlane =
		[PlayerController](const float ScreenX, const float ScreenY, FVector& OutPoint)
		{
			FVector RayOrigin = FVector::ZeroVector;
			FVector RayDirection = FVector::ZeroVector;
			if (!PlayerController->DeprojectScreenPositionToWorld(
				ScreenX, ScreenY, RayOrigin, RayDirection)
				|| FMath::Abs(RayDirection.Y) <= UE_SMALL_NUMBER)
			{
				return false;
			}

			const float RayDistance =
				(QingyunBackgroundPlaneY - RayOrigin.Y) / RayDirection.Y;
			if (RayDistance <= 0.0f || !FMath::IsFinite(RayDistance))
			{
				return false;
			}

			OutPoint = RayOrigin + RayDirection * RayDistance;
			return !OutPoint.ContainsNaN();
		};

	FVector TopLeft;
	FVector TopRight;
	FVector BottomLeft;
	FVector BottomRight;
	if (!DeprojectToBackgroundPlane(0.0f, 0.0f, TopLeft)
		|| !DeprojectToBackgroundPlane(
			static_cast<float>(ViewportWidth), 0.0f, TopRight)
		|| !DeprojectToBackgroundPlane(
			0.0f, static_cast<float>(ViewportHeight), BottomLeft)
		|| !DeprojectToBackgroundPlane(
			static_cast<float>(ViewportWidth),
			static_cast<float>(ViewportHeight),
			BottomRight))
	{
		return;
	}

	const float MinimumX = FMath::Min(
		FMath::Min(TopLeft.X, TopRight.X),
		FMath::Min(BottomLeft.X, BottomRight.X));
	const float MaximumX = FMath::Max(
		FMath::Max(TopLeft.X, TopRight.X),
		FMath::Max(BottomLeft.X, BottomRight.X));
	const float MinimumZ = FMath::Min(
		FMath::Min(TopLeft.Z, TopRight.Z),
		FMath::Min(BottomLeft.Z, BottomRight.Z));
	const float MaximumZ = FMath::Max(
		FMath::Max(TopLeft.Z, TopRight.Z),
		FMath::Max(BottomLeft.Z, BottomRight.Z));
	const float RequiredWidth = MaximumX - MinimumX;
	const float RequiredHeight = MaximumZ - MinimumZ;

	const FBoxSphereBounds SpriteBounds =
		QingyunMountainBackgroundSprite->GetRenderBounds();
	const float PixelsPerUnrealUnit =
		QingyunMountainBackgroundSprite->GetPixelsPerUnrealUnit();
	const float NativeWorldWidth = SpriteBounds.BoxExtent.X * 2.0f;
	const float NativeWorldHeight = SpriteBounds.BoxExtent.Z * 2.0f;
	if (RequiredWidth <= UE_SMALL_NUMBER
		|| RequiredHeight <= UE_SMALL_NUMBER
		|| NativeWorldWidth <= UE_SMALL_NUMBER
		|| NativeWorldHeight <= UE_SMALL_NUMBER
		|| PixelsPerUnrealUnit <= UE_SMALL_NUMBER)
	{
		return;
	}

	const float CoverScale = FMath::Max(
		RequiredWidth / NativeWorldWidth,
		RequiredHeight / NativeWorldHeight) * QingyunBackgroundOverscan;
	if (!FMath::IsFinite(CoverScale) || CoverScale <= UE_SMALL_NUMBER)
	{
		return;
	}

	const FVector ViewBoundsCenter(
		(MinimumX + MaximumX) * 0.5f,
		QingyunBackgroundPlaneY,
		(MinimumZ + MaximumZ) * 0.5f);
	// The imported TBH crop intentionally uses its road line as a custom pivot.
	// Align the sprite's render-bounds center—not that pivot—to the viewport
	// center so the crop still covers 1707x320 and the road stays near y=230.
	const FVector BackgroundLocation =
		ViewBoundsCenter - SpriteBounds.Origin * CoverScale;
	const FIntPoint ViewportSize(ViewportWidth, ViewportHeight);
	const bool bGeometryChanged =
		ViewportSize != LastQingyunBackgroundViewportSize
		|| !BackgroundLocation.Equals(LastQingyunBackgroundLocation, 0.5f)
		|| !FMath::IsNearlyEqual(CoverScale, LastQingyunBackgroundScale, 0.001f);
	if (bGeometryChanged)
	{
		QingyunMountainBackground->SetWorldLocation(BackgroundLocation);
		QingyunMountainBackground->SetWorldScale3D(FVector(CoverScale));
		LastQingyunBackgroundViewportSize = ViewportSize;
		LastQingyunBackgroundLocation = BackgroundLocation;
		LastQingyunBackgroundScale = CoverScale;
	}

	const bool bWasVisible = QingyunMountainBackground->IsVisible();
	QingyunMountainBackground->SetHiddenInGame(false);
	QingyunMountainBackground->SetVisibility(true, true);
	if (!bWasVisible)
	{
		// This is delayed until a valid viewport/camera projection exists, so a
		// failed deprojection can never leave the player in a blank scene.
		for (TActorIterator<APaperTileMapActor> It(GetWorld()); It; ++It)
		{
			if (UPaperTileMapComponent* TileMap = It->GetRenderComponent())
			{
				TileMap->SetVisibility(false, true);
			}
		}
	}

	if (!bHasLoggedQingyunBackgroundFit)
	{
		bHasLoggedQingyunBackgroundFit = true;
		UE_LOG(LogTemp, Display,
			TEXT("Qingyun runtime background fitted: viewport=%dx%d planeY=%.1f ")
			TEXT("center=(%.1f, %.1f, %.1f) scale=%.4f coverage=(%.1f x %.1f) ")
			TEXT("source=(%.0f x %.0f px) PPU=%.3f"),
			ViewportWidth,
			ViewportHeight,
			QingyunBackgroundPlaneY,
			BackgroundLocation.X,
			BackgroundLocation.Y,
			BackgroundLocation.Z,
			CoverScale,
			NativeWorldWidth * CoverScale,
			NativeWorldHeight * CoverScale,
			NativeWorldWidth * PixelsPerUnrealUnit,
			NativeWorldHeight * PixelsPerUnrealUnit,
			PixelsPerUnrealUnit);
	}
}

bool AImmortalMonsterSpawner::ApplyProgressForMap(const FName MapId)
{
	FImmortalMapDefinition Definition;
	FImmortalMapProgress Progress;
	if (!UImmortalMapLibrary::GetMapDefinition(MapId, Definition)
		|| !UImmortalMapLibrary::GetMapProgress(MapSystemState, MapId, Progress))
	{
		return false;
	}

	MapSystemState.ActiveMapId = MapId;
	ActiveMapDefinition = Definition;
	CurrentStage = FMath::Clamp(Progress.Stage, 1, GetCurrentMaximumStage());
	bCurrentMapCompleted = Progress.bCompleted && CurrentStage >= GetCurrentMaximumStage();
	const int32 RequiredKills = GetRequiredKillsForCurrentStage();
	CurrentStageKills = bCurrentMapCompleted
		? RequiredKills
		: FMath::Clamp(Progress.StageKills, 0, FMath::Max(RequiredKills - 1, 0));
	return SyncCurrentProgressToState();
}

bool AImmortalMonsterSpawner::SyncCurrentProgressToState()
{
	if (!ActiveMapDefinition.IsValid())
	{
		if (!UImmortalMapLibrary::GetMapDefinition(MapSystemState.ActiveMapId, ActiveMapDefinition))
		{
			return false;
		}
	}
	UImmortalMapLibrary::NormalizeState(MapSystemState);
	FImmortalMapProgress Progress;
	Progress.MapId = MapSystemState.ActiveMapId;
	Progress.Stage = FMath::Clamp(CurrentStage, 1, GetCurrentMaximumStage());
	Progress.bCompleted = bCurrentMapCompleted && Progress.Stage >= GetCurrentMaximumStage();
	const int32 RequiredKills = GetRequiredKillsForCurrentStage();
	Progress.StageKills = Progress.bCompleted
		? RequiredKills
		: FMath::Clamp(CurrentStageKills, 0, FMath::Max(RequiredKills - 1, 0));
	CurrentStage = Progress.Stage;
	CurrentStageKills = Progress.StageKills;
	bCurrentMapCompleted = Progress.bCompleted;
	return UImmortalMapLibrary::SetMapProgress(MapSystemState, Progress);
}

void AImmortalMonsterSpawner::MirrorLegacyQingyunProgress(
	UImmortalPathSaveGame* SaveGame) const
{
	if (!SaveGame)
	{
		return;
	}
	FImmortalMapProgress QingyunProgress;
	if (!UImmortalMapLibrary::GetMapProgress(
		MapSystemState, UImmortalMapLibrary::GetQingyunMountainId(), QingyunProgress))
	{
		return;
	}
	FImmortalMapDefinition QingyunDefinition;
	UImmortalMapLibrary::GetMapDefinition(
		UImmortalMapLibrary::GetQingyunMountainId(), QingyunDefinition);
	const int32 QingyunMaximumStage = FMath::Clamp(QingyunDefinition.MaximumStage, 1, 999);
	const int32 QingyunStage = FMath::Clamp(QingyunProgress.Stage, 1, QingyunMaximumStage);
	const bool bQingyunBossStage = !QingyunProgress.bCompleted
		&& (QingyunStage >= QingyunMaximumStage
			|| QingyunStage % FMath::Max(QingyunDefinition.BossStageInterval, 2) == 0);
	const int32 RequiredKills = bQingyunBossStage || QingyunProgress.bCompleted
		? 1
		: FMath::Max(KillsPerStage, 1);

	SaveGame->bHasStageData = true;
	SaveGame->QingyunStage = QingyunStage;
	SaveGame->QingyunStageKills = QingyunProgress.bCompleted
		? 1
		: FMath::Clamp(QingyunProgress.StageKills, 0, FMath::Max(RequiredKills - 1, 0));
	SaveGame->bQingyunMountainCompleted = QingyunProgress.bCompleted
		&& QingyunStage >= QingyunMaximumStage;
}

int32 AImmortalMonsterSpawner::GetPlayerRealmIndex() const
{
	if (const AImmortalPlayerCharacter* Player = Cast<AImmortalPlayerCharacter>(
		UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		return static_cast<int32>(Player->GetCultivationRealm());
	}
	return 0;
}

int32 AImmortalMonsterSpawner::GetCurrentMaximumStage() const
{
	return ActiveMapDefinition.IsValid()
		? FMath::Clamp(ActiveMapDefinition.MaximumStage, 1, 999)
		: FMath::Clamp(MaxStage, 1, 999);
}

int32 AImmortalMonsterSpawner::GetCurrentBossStageInterval() const
{
	return ActiveMapDefinition.IsValid()
		? FMath::Max(ActiveMapDefinition.BossStageInterval, 2)
		: FMath::Max(BossStageInterval, 2);
}

bool AImmortalMonsterSpawner::FindSpawnLocation(FVector& OutLocation) const
{
	if (!SpawnArea)
	{
		return false;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector Extent = SpawnArea->GetUnscaledBoxExtent();
	const FTransform AreaTransform = SpawnArea->GetComponentTransform();
	constexpr int32 MaxAttempts = 12;

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		FVector LocalLocation(
			FMath::FRandRange(-Extent.X, Extent.X),
			bConstrainTo2DPlane ? 0.0f : FMath::FRandRange(-Extent.Y, Extent.Y),
			FMath::FRandRange(-Extent.Z, Extent.Z));

		FVector Candidate = AreaTransform.TransformPosition(LocalLocation);
		if (PlayerPawn && bConstrainTo2DPlane)
		{
			Candidate.Y = PlayerPawn->GetActorLocation().Y;
		}
		if (PlayerPawn && bMatchPlayerHeight)
		{
			Candidate.Z = PlayerPawn->GetActorLocation().Z + SpawnHeightOffset;
		}
		else
		{
			Candidate.Z += SpawnHeightOffset;
		}
		if (!PlayerPawn
			|| FVector::DistSquared(PlayerPawn->GetActorLocation(), Candidate)
				>= FMath::Square(MinDistanceFromPlayer))
		{
			OutLocation = Candidate;
			return true;
		}
	}

	return false;
}

void AImmortalMonsterSpawner::RemoveInvalidMonsters()
{
	SpawnedMonsters.RemoveAll([](const TWeakObjectPtr<AImmortalMonsterCharacter>& Monster)
	{
		return !Monster.IsValid();
	});
}

void AImmortalMonsterSpawner::HandleSpawnedMonsterDestroyed(AActor* DestroyedActor)
{
	const bool bUnexpectedWorldBossLoss = bWorldBossChallengeActive
		&& !bResolvingWorldBossChallenge
		&& ActiveWorldBoss.Get() == DestroyedActor;
	const AImmortalMonsterCharacter* DestroyedMonster =
		Cast<AImmortalMonsterCharacter>(DestroyedActor);
	const bool bUnexpectedEndlessLoss = bEndlessDungeonActive
		&& !bResolvingEndlessDungeon
		&& DestroyedMonster
		&& DestroyedMonster->IsEndlessEnemy()
		&& !DestroyedMonster->IsDead()
		&& !DestroyedMonster->ActorHasTag(TEXT("EndlessMinion"))
		&& DestroyedMonster->GetEndlessRunId() == ActiveEndlessRunId
		&& DestroyedMonster->GetEndlessFloor() == ActiveEndlessFloor;
	SpawnedMonsters.RemoveAll([DestroyedActor](const TWeakObjectPtr<AImmortalMonsterCharacter>& Monster)
	{
		return !Monster.IsValid() || Monster.Get() == DestroyedActor;
	});
	if (bUnexpectedWorldBossLoss)
	{
		FinishWorldBossChallenge(
			false,
			FText::FromString(TEXT("世界妖王实体意外消失，挑战已安全终止且地图挂机已恢复")),
			FLinearColor(1.0f, 0.18f, 0.12f, 1.0f),
			Cast<AImmortalMonsterCharacter>(DestroyedActor));
	}
	else if (bUnexpectedEndlessLoss)
	{
		FinishEndlessDungeon(
			false,
			FText::FromString(
				TEXT("无尽秘境怪物实体意外消失，挑战已安全终止并恢复地图挂机")),
			FLinearColor(1.0f, 0.18f, 0.12f, 1.0f));
	}
}
