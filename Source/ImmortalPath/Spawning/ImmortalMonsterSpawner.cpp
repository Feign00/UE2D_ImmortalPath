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
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PaperTileMapActor.h"
#include "PaperTileMapComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

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
	SaveStageProgress();
	StopSpawning();
	SpawnedMonsters.Reset();
	Super::EndPlay(EndPlayReason);
}

AImmortalMonsterCharacter* AImmortalMonsterSpawner::SpawnMonster()
{
	RemoveInvalidMonsters();
	if (bMapTransitionInProgress || bCurrentMapCompleted)
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
	if (!Monster || bMapTransitionInProgress || bCurrentMapCompleted)
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
			bMapProgressSaved && bBossDefeated ? 1 : 0);
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
	if (!GetWorld() || bMapTransitionInProgress || bCurrentMapCompleted)
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
	if (!bMapTransitionInProgress && !bCurrentMapCompleted)
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

void AImmortalMonsterSpawner::SpawnUntilInitialCount()
{
	if (bMapTransitionInProgress || bCurrentMapCompleted)
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
	if (!GetWorld() || !MonsterClass || bMapTransitionInProgress || bCurrentMapCompleted)
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
		Monster->ConfigureForMapStage(MapSystemState.ActiveMapId, CurrentStage);
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
	UE_LOG(LogTemp, Display, TEXT("%s boss summoned %d minions"),
		*ActiveMapDefinition.DisplayName.ToString(), FMath::Max(Count, 0));
}

void AImmortalMonsterSpawner::HandleBossPhaseChanged(
	AImmortalMonsterCharacter* Boss,
	const int32 NewPhase)
{
	if (bMapTransitionInProgress || !Boss || Boss->IsDead() || !Boss->IsBoss()
		|| Boss->GetConfiguredMapId() != MapSystemState.ActiveMapId)
	{
		return;
	}
	const int32 SummonCount = NewPhase == 2
		? PhaseTwoSummonCount
		: (NewPhase == 3 ? PhaseThreeSummonCount : 0);
	SpawnBossMinions(Boss, SummonCount);
	const FString PhaseMessage = NewPhase >= 3
		? FString::Printf(TEXT("%s进入狂暴阶段！召唤 %d 只妖兽"),
			*ActiveMapDefinition.BossName.ToString(), SummonCount)
		: FString::Printf(TEXT("%s进入第 %d 阶段，召唤 %d 只妖兽"),
			*ActiveMapDefinition.BossName.ToString(), NewPhase, SummonCount);
	ShowBossMessage(
		FText::FromString(PhaseMessage),
		NewPhase >= 3
			? FLinearColor(1.0f, 0.12f, 0.08f, 1.0f)
			: ActiveMapDefinition.BossColor);
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

void AImmortalMonsterSpawner::ApplyActiveMapPresentation() const
{
	if (!GetWorld())
	{
		return;
	}
	int32 TintedTileMaps = 0;
	for (TActorIterator<APaperTileMapActor> It(GetWorld()); It; ++It)
	{
		if (UPaperTileMapComponent* TileMap = It->GetRenderComponent())
		{
			TileMap->SetTileMapColor(ActiveMapDefinition.SceneTint);
			++TintedTileMaps;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("Map presentation applied: %s tint=(%.2f, %.2f, %.2f) tileMaps=%d"),
		*ActiveMapDefinition.DisplayName.ToString(),
		ActiveMapDefinition.SceneTint.R,
		ActiveMapDefinition.SceneTint.G,
		ActiveMapDefinition.SceneTint.B,
		TintedTileMaps);
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
	SpawnedMonsters.RemoveAll([DestroyedActor](const TWeakObjectPtr<AImmortalMonsterCharacter>& Monster)
	{
		return !Monster.IsValid() || Monster.Get() == DestroyedActor;
	});
}
