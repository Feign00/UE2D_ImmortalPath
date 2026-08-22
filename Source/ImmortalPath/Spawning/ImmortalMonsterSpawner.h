// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Endless/ImmortalEndlessDungeonTypes.h"
#include "../Maps/ImmortalMapTypes.h"
#include "../WorldBoss/ImmortalWorldBossTypes.h"
#include "GameFramework/Actor.h"
#include "ImmortalMonsterSpawner.generated.h"

class AImmortalMonsterCharacter;
class UImmortalPathSaveGame;
class UBoxComponent;
class UPaperSprite;
class UPaperSpriteComponent;
class USceneComponent;

/** Maintains a configurable number of monsters inside a 2D spawn volume. */
UCLASS(Blueprintable)
class IMMORTALPATH_API AImmortalMonsterSpawner : public AActor
{
	GENERATED_BODY()

public:
	AImmortalMonsterSpawner();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Attempts to create one monster. Returns it on success. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Spawning")
	AImmortalMonsterCharacter* SpawnMonster();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Spawning")
	int32 GetAliveMonsterCount() const;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Spawning")
	void StartSpawning();

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Spawning")
	void StopSpawning();

	/** Closes normal and optional combat scenes while death recovery is active. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Spawning")
	void SuspendAdventureForCultivation();

	/** Reopens the active map after cultivation has cleared the death gate. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Spawning")
	bool ResumeAdventureAfterCultivation();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Spawning")
	bool IsAdventureSpawningRequested() const { return bSpawningRequested; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Stage")
	int32 GetCurrentStage() const { return CurrentStage; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Stage")
	int32 GetCurrentStageKills() const { return CurrentStageKills; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Stage")
	bool IsCurrentStageBossStage() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Stage")
	bool IsQingyunMountainCompleted() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Maps")
	FName GetActiveMapId() const { return MapSystemState.ActiveMapId; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Maps")
	FText GetActiveMapDisplayName() const { return ActiveMapDefinition.DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Maps")
	FImmortalMapDefinition GetActiveMapDefinition() const { return ActiveMapDefinition; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Maps")
	FImmortalMapSystemState GetMapSystemState() const { return MapSystemState; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Maps")
	int32 GetMapRevision() const { return MapSystemRevision; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Maps")
	bool GetProgressForMap(FName MapId, FImmortalMapProgress& OutProgress) const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Maps")
	bool IsCurrentMapCompleted() const { return bCurrentMapCompleted; }

	/** Returns true only for a known map unlocked by the player's current major realm. */
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Maps")
	bool CanTravelToMap(FName DestinationMapId) const;

	/** Atomically persists the old map, clears its combat actors and activates the destination. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Maps")
	FImmortalMapTravelResult TravelToMap(FName DestinationMapId);

	/** Non-shipping runtime fixture: completes one map without changing the active map. */
	bool CompleteMapForDevelopment(FName MapId);

	/** True only while the normal map scene can safely enter a new ascension cycle. */
	bool CanApplyAscensionCycleReset() const;

	/** Pure validation used before the combined player/map save is committed. */
	bool IsCanonicalAscensionCycleState(
		const FImmortalMapSystemState& State) const;

	/**
	 * Applies an already-persisted stage-one cycle without writing the save
	 * again. This is intentionally non-failing after the caller's preflight.
	 */
	void ApplyPersistedAscensionCycleState(
		const FImmortalMapSystemState& PersistedCycleState);

	/** Starts an optional encounter without mutating the active map or its 1-999 progress. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|World Boss")
	FImmortalWorldBossChallengeResult StartWorldBossChallenge(FName BossId);

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|World Boss")
	FImmortalWorldBossChallengeResult CancelWorldBossChallenge();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|World Boss")
	bool IsWorldBossChallengeActive() const { return bWorldBossChallengeActive; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|World Boss")
	FImmortalWorldBossRuntimeSnapshot GetWorldBossRuntimeSnapshot() const;

	/** Non-shipping runtime fixture hook used by Step 30 restart validation. */
	bool DefeatActiveWorldBossForDevelopment();
	bool DamageActiveWorldBossForDevelopment(float MaximumHealthFraction);
	bool TimeoutActiveWorldBossForDevelopment();

	/** Starts an independent endless run without mutating map, sect or cultivation progress. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Endless Dungeon")
	FImmortalEndlessDungeonStartResult StartEndlessDungeon(int32 StartFloor);

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Endless Dungeon")
	FImmortalEndlessDungeonStartResult CancelEndlessDungeon();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Endless Dungeon")
	bool IsEndlessDungeonActive() const { return bEndlessDungeonActive; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Endless Dungeon")
	FImmortalEndlessDungeonRuntimeSnapshot GetEndlessDungeonRuntimeSnapshot() const;

	/** Non-shipping runtime fixture hooks used by Step 31 validation. */
	bool DamageActiveEndlessBossForDevelopment(float MaximumHealthFraction);
	bool ClearActiveEndlessFloorForDevelopment();
	bool FailActiveEndlessDungeonForDevelopment();

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Save")
	bool SaveStageProgress();

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Save")
	bool LoadStageProgress();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Immortal Path|Spawning")
	TObjectPtr<USceneComponent> SceneRoot;

	/** X/Z define the random spawn area. Y can be locked for a 2D lane. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Immortal Path|Spawning")
	TObjectPtr<UBoxComponent> SpawnArea;

	/** Runtime Qingyun backdrop; hidden for every other map and when its cooked sprite is unavailable. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Immortal Path|Maps")
	TObjectPtr<UPaperSpriteComponent> QingyunMountainBackground;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Spawning")
	TSubclassOf<AImmortalMonsterCharacter> MonsterClass;

	/** Optional second monster type; when set, each spawn randomly picks one of the two classes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Spawning")
	TSubclassOf<AImmortalMonsterCharacter> AlternateMonsterClass;

	/** Optional dedicated boss Blueprint. Falls back to Dog, then the normal monster class. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Boss")
	TSubclassOf<AImmortalMonsterCharacter> BossMonsterClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Spawning", meta = (ClampMin = "0"))
	int32 InitialSpawnCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Spawning", meta = (ClampMin = "1"))
	int32 MaxAliveMonsters = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Spawning", meta = (ClampMin = "0.1", Units = "s"))
	float SpawnInterval = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Spawning")
	bool bStartOnBeginPlay = true;

	/** Keeps every spawn on the spawner component's local Y plane. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Spawning")
	bool bConstrainTo2DPlane = true;

	/** Uses the player's Z as the lane floor, avoiding perspective-viewport placement offsets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Spawning")
	bool bMatchPlayerHeight = true;

	/** Prevents monsters from appearing directly on top of the player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Spawning", meta = (ClampMin = "0.0", Units = "cm"))
	float MinDistanceFromPlayer = 300.0f;

	/** Keeps Endless Dungeon waves inside the forward TBH camera band. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Endless Dungeon", meta = (ClampMin = "300.0", Units = "cm"))
	float EndlessForwardSpawnDistance = 520.0f;

	/** Horizontal separation between simultaneous Endless Dungeon enemies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Endless Dungeon", meta = (ClampMin = "80.0", Units = "cm"))
	float EndlessForwardSpawnSpacing = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Spawning", meta = (Units = "cm"))
	float SpawnHeightOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Stage", meta = (ClampMin = "1", ClampMax = "999"))
	int32 CurrentStage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Stage", meta = (ClampMin = "1", ClampMax = "999"))
	int32 MaxStage = 999;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Stage", meta = (ClampMin = "1"))
	int32 KillsPerStage = 10;

	/** Every Nth stage is a one-boss gate. Stage 999 is always the final boss. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Boss", meta = (ClampMin = "2"))
	int32 BossStageInterval = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Boss", meta = (ClampMin = "0"))
	int32 PhaseTwoSummonCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Boss", meta = (ClampMin = "0"))
	int32 PhaseThreeSummonCount = 2;

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Spawning", meta = (DisplayName = "On Monster Spawned"))
	void BP_OnMonsterSpawned(AImmortalMonsterCharacter* Monster);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Boss", meta = (DisplayName = "On Boss Spawned"))
	void BP_OnBossSpawned(AImmortalMonsterCharacter* Boss, int32 Stage);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Boss", meta = (DisplayName = "On Boss Defeated"))
	void BP_OnBossDefeated(int32 ClearedStage, int32 NextStage, bool bMapCompleted);

	/** Presentation hook for future per-map backgrounds while all maps share the TBH combat level. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Maps", meta = (DisplayName = "On Active Map Changed"))
	void BP_OnActiveMapChanged(FName PreviousMapId, FName NewMapId, const FText& NewMapName, int32 Stage);

private:
	void SpawnUntilInitialCount();
	void HandleSpawnTimer();
	void SuspendSpawnTimer();
	bool FindSpawnLocation(FVector& OutLocation) const;
	void RemoveInvalidMonsters();
	void UpdateStageHud() const;
	void ApplyActiveMapPresentation();
	void RefreshQingyunBackgroundGeometry();
	bool ApplyProgressForMap(FName MapId);
	bool SyncCurrentProgressToState();
	void MirrorLegacyQingyunProgress(UImmortalPathSaveGame* SaveGame) const;
	void ClearAllMonstersAndDrops();
	void ClearAllMonstersForWorldBoss();
	int32 GetPlayerRealmIndex() const;
	int32 GetCurrentMaximumStage() const;
	int32 GetCurrentBossStageInterval() const;
	AImmortalMonsterCharacter* SpawnConfiguredMonster(bool bSpawnBoss, bool bIgnoreAliveLimit, AActor* SpawnOwner = nullptr);
	void SpawnBossMinions(AImmortalMonsterCharacter* Boss, int32 Count);
	void AdvanceStage(AImmortalMonsterCharacter* DefeatedMonster);
	void ClearOtherMonsters(AImmortalMonsterCharacter* Exception);
	int32 GetRequiredKillsForCurrentStage() const;
	void ShowBossMessage(const FText& Message, const FLinearColor& Color) const;
	AImmortalMonsterCharacter* SpawnWorldBoss(
		const FImmortalWorldBossDefinition& Definition);
	void FinishWorldBossChallenge(
		bool bDefeated,
		const FText& Message,
		const FLinearColor& Color,
		AImmortalMonsterCharacter* DefeatedBoss = nullptr);
	void HandleWorldBossTimeout();
	void UpdateWorldBossHud();
	void ResumeMapSpawningAfterWorldBoss();
	bool SpawnEndlessFloor();
	AImmortalMonsterCharacter* SpawnEndlessEnemy(
		bool bBoss,
		bool bElite,
		AActor* SpawnOwner = nullptr);
	void FinishEndlessDungeon(
		bool bCompletedFloor,
		const FText& Message,
		const FLinearColor& Color,
		AImmortalMonsterCharacter* DefeatedMonster = nullptr);
	void AdvanceEndlessDungeonFloor();
	void UpdateEndlessDungeonHud();
	void ResumeMapSpawningAfterEndlessDungeon();
	void HandleEndlessDungeonMonsterDeath(
		AImmortalMonsterCharacter* Monster,
		AActor* DamageCauser);

	UFUNCTION()
	void HandleMonsterDeath(AImmortalMonsterCharacter* Monster, AActor* DamageCauser);

	UFUNCTION()
	void HandleBossPhaseChanged(AImmortalMonsterCharacter* Boss, int32 NewPhase);

	UFUNCTION()
	void HandleSpawnedMonsterDestroyed(AActor* DestroyedActor);

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AImmortalMonsterCharacter>> SpawnedMonsters;

	/** Cook-safe fallback used when the spawner Blueprint has no alternate class assigned. */
	UPROPERTY(Transient)
	TSubclassOf<AImmortalMonsterCharacter> DefaultAlternateMonsterClass;

	UPROPERTY(Transient)
	TSubclassOf<AImmortalMonsterCharacter> DefaultBossMonsterClass;

	/** Cook-visible soft reference; missing source art safely falls back to the legacy TileMap. */
	UPROPERTY(EditDefaultsOnly, Category = "Immortal Path|Maps")
	TSoftObjectPtr<UPaperSprite> QingyunMountainBackgroundAsset;

	UPROPERTY(Transient)
	TObjectPtr<UPaperSprite> QingyunMountainBackgroundSprite;

	FTimerHandle SpawnTimerHandle;
	FTimerHandle QingyunBackgroundRefreshTimerHandle;
	bool bQingyunBackgroundRequested = false;
	bool bHasLoggedQingyunBackgroundFit = false;
	FIntPoint LastQingyunBackgroundViewportSize = FIntPoint::ZeroValue;
	FVector LastQingyunBackgroundLocation = FVector::ZeroVector;
	float LastQingyunBackgroundScale = 0.0f;
	bool bSpawningRequested = false;
	bool bMapTransitionInProgress = false;
	bool bHandlingMonsterDeath = false;
	bool bMapMigrationPending = false;
	bool bWorldBossChallengeActive = false;
	bool bResolvingWorldBossChallenge = false;
	bool bResumeSpawningAfterWorldBoss = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalMonsterCharacter> ActiveWorldBoss;

	UPROPERTY(VisibleInstanceOnly, Category = "Immortal Path|World Boss")
	FImmortalWorldBossDefinition ActiveWorldBossDefinition;

	float WorldBossChallengeStartTime = 0.0f;
	float WorldBossChallengeEndTime = 0.0f;
	FTimerHandle WorldBossTimeoutTimerHandle;
	FTimerHandle WorldBossHudTimerHandle;
	FTimerHandle WorldBossResumeTimerHandle;

	bool bEndlessDungeonActive = false;
	bool bResolvingEndlessDungeon = false;
	bool bResumeSpawningAfterEndlessDungeon = false;
	FGuid ActiveEndlessRunId;
	int32 ActiveEndlessFloor = 0;
	int32 ActiveEndlessRunStartFloor = 0;
	int32 ActiveEndlessKills = 0;
	int32 ActiveEndlessRequiredKills = 0;
	float EndlessDungeonStartTime = 0.0f;
	float EndlessFloorStartTime = 0.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalMonsterCharacter> ActiveEndlessBoss;

	FTimerHandle EndlessDungeonHudTimerHandle;
	FTimerHandle EndlessDungeonNextFloorTimerHandle;
	FTimerHandle EndlessDungeonResumeTimerHandle;

	UPROPERTY(VisibleInstanceOnly, Category = "Immortal Path|Stage")
	int32 CurrentStageKills = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Immortal Path|Maps")
	bool bCurrentMapCompleted = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Immortal Path|Maps")
	FImmortalMapSystemState MapSystemState;

	UPROPERTY(VisibleInstanceOnly, Category = "Immortal Path|Maps")
	FImmortalMapDefinition ActiveMapDefinition;

	UPROPERTY(VisibleInstanceOnly, Category = "Immortal Path|Maps")
	int32 MapSystemRevision = 0;
};
