// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImmortalMonsterSpawner.generated.h"

class AImmortalMonsterCharacter;
class UBoxComponent;
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

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Stage")
	int32 GetCurrentStage() const { return CurrentStage; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Stage")
	int32 GetCurrentStageKills() const { return CurrentStageKills; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Stage")
	bool IsCurrentStageBossStage() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Stage")
	bool IsQingyunMountainCompleted() const { return bQingyunMountainCompleted; }

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

private:
	void SpawnUntilInitialCount();
	void HandleSpawnTimer();
	bool FindSpawnLocation(FVector& OutLocation) const;
	void RemoveInvalidMonsters();
	void UpdateStageHud() const;
	AImmortalMonsterCharacter* SpawnConfiguredMonster(bool bSpawnBoss, bool bIgnoreAliveLimit, AActor* SpawnOwner = nullptr);
	void SpawnBossMinions(AImmortalMonsterCharacter* Boss, int32 Count);
	void AdvanceStage(AImmortalMonsterCharacter* DefeatedMonster);
	void ClearOtherMonsters(AImmortalMonsterCharacter* Exception);
	int32 GetRequiredKillsForCurrentStage() const;
	void ShowBossMessage(const FText& Message, const FLinearColor& Color) const;

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

	FTimerHandle SpawnTimerHandle;

	UPROPERTY(VisibleInstanceOnly, Category = "Immortal Path|Stage")
	int32 CurrentStageKills = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Immortal Path|Stage")
	bool bQingyunMountainCompleted = false;
};
