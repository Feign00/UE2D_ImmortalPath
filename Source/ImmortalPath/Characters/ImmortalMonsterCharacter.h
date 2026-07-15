// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Combat/AutoAttackTarget.h"
#include "PaperCharacter.h"
#include "ImmortalMonsterCharacter.generated.h"

class AController;
class APawn;
class UDamageType;
class AImmortalEquipmentDrop;
class UPaperFlipbook;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FMonsterDeathSignature,
	AImmortalMonsterCharacter*, Monster,
	AActor*, DamageCauser);

/** C++ base class shared by normal, elite and boss monster Blueprints. */
UCLASS(Blueprintable)
class IMMORTALPATH_API AImmortalMonsterCharacter : public APaperCharacter, public IAutoAttackTarget
{
	GENERATED_BODY()

public:
	AImmortalMonsterCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(
		float DamageAmount,
		const FDamageEvent& DamageEvent,
		AController* EventInstigator,
		AActor* DamageCauser) override;

	virtual bool CanBeAutoAttacked_Implementation() const override;
	virtual FVector GetAutoAttackTargetLocation_Implementation() const override;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Monster")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Monster")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Monster")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Monster")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Monster|Combat")
	APawn* GetCombatTarget() const { return CombatTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Monster|Rewards")
	int32 GetCultivationReward() const { return CultivationReward; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Monster|Rewards")
	int32 GetGoldReward() const { return GoldReward; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Monster|Rewards")
	float GetEquipmentDropChance() const { return EquipmentDropChance; }

	/** Sent once when this monster reaches zero health. */
	UPROPERTY(BlueprintAssignable, Category = "Immortal Path|Monster")
	FMonsterDeathSignature OnMonsterDeath;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster", meta = (ClampMin = "1.0"))
	float MaxHealth = 30.0f;

	/** Flat damage reduction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster", meta = (ClampMin = "0.0"))
	float Defense = 0.0f;

	/** Destroy the actor after its death animation window. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster")
	bool bDestroyAfterDeath = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster", meta = (EditCondition = "bDestroyAfterDeath", ClampMin = "0.0", Units = "s"))
	float DeathLifeSpan = 0.75f;

	/** Horizontal speed while automatically approaching the player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Combat", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MovementSpeed = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Combat", meta = (ClampMin = "1.0", Units = "cm"))
	float AttackRange = 115.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Combat", meta = (ClampMin = "0.05", Units = "s"))
	float AttackInterval = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Combat", meta = (ClampMin = "0.1"))
	float AttackSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CriticalChance = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Combat", meta = (ClampMin = "1.0"))
	float CriticalDamageMultiplier = 1.5f;

	/** Damage is applied this many seconds after the attack animation begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Combat", meta = (ClampMin = "0.0", Units = "s"))
	float AttackWindup = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Combat")
	bool bAutoCombatOnBeginPlay = true;

	/** Set this to match the direction drawn in the source sprite sheet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Animation")
	bool bArtworkFacesRight = false;

	/** Visual-only scale used to keep monster sprites readable next to the large player artwork. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Animation", meta = (ClampMin = "0.1"))
	float VisualScale = 3.0f;

	/** Moves only the sprite toward the side-scroller camera; gameplay collision stays on the 2D lane. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Animation", meta = (Units = "cm"))
	float VisualDepthOffset = -100.0f;

	/** Keeps monster artwork in front of the tile map when translucent sprite materials are used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Animation")
	int32 VisualSortPriority = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Animation")
	TObjectPtr<UPaperFlipbook> MoveFlipbook;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Animation")
	TObjectPtr<UPaperFlipbook> AttackFlipbook;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Animation")
	TObjectPtr<UPaperFlipbook> HurtFlipbook;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Animation")
	TObjectPtr<UPaperFlipbook> DeathFlipbook;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Animation", meta = (ClampMin = "0.0", Units = "s"))
	float HurtAnimationDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Rewards", meta = (ClampMin = "0"))
	int32 CultivationReward = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Rewards", meta = (ClampMin = "0"))
	int32 GoldReward = 1;

	/** Chance to create an automatically collected equipment orb at death. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Rewards", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EquipmentDropChance = 0.25f;

	/** Actor spawned when the equipment roll succeeds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Rewards")
	TSubclassOf<AImmortalEquipmentDrop> EquipmentDropClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Rewards", meta = (ClampMin = "1"))
	int32 EquipmentItemLevel = 1;

	/** Height of the screen-space health bar above the monster capsule. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|UI", meta = (Units = "cm"))
	float HealthBarHeight = 135.0f;

	/** Blueprint hook for hit flash, damage number and future hit animation. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Monster", meta = (DisplayName = "On Monster Damaged"))
	void BP_OnMonsterDamaged(float DamageApplied, float RemainingHealth, AActor* DamageCauser);

	/** Blueprint hook for the death flipbook and drop effects. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Monster", meta = (DisplayName = "On Monster Died"))
	void BP_OnMonsterDied(AActor* DamageCauser);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Monster|Combat", meta = (DisplayName = "On Monster Attack Started"))
	void BP_OnMonsterAttackStarted(AActor* Target);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Monster|Combat", meta = (DisplayName = "On Monster Attack Resolved"))
	void BP_OnMonsterAttackResolved(AActor* Target, float DamageDealt);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Monster|Combat", meta = (DisplayName = "On Monster Critical Hit"))
	void BP_OnMonsterCriticalHit(AActor* Target, float DamageDealt);

	/** Fired at death for reward text, effects and later rarity presentation. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Monster|Rewards", meta = (DisplayName = "On Monster Rewards Granted"))
	void BP_OnMonsterRewardsGranted(int32 CultivationGranted, int32 GoldGranted, float DropChance, AActor* DamageCauser);

private:
	void AcquireCombatTarget();
	void UpdateAutoCombat(float DeltaSeconds);
	void StartAttack();
	void ResolveAttack();
	void FinishAttack();
	void FinishHurtReaction();
	void UpdateFacing(float HorizontalDirection);
	void PlayMoveAnimation();
	void PlayOneShotAnimation(UPaperFlipbook* Flipbook);
	float GetAnimationDuration(UPaperFlipbook* Flipbook, float FallbackDuration) const;
	void Die(AActor* DamageCauser);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Monster", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Monster", meta = (AllowPrivateAccess = "true"))
	bool bDead = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> CombatTarget;

	FTimerHandle AttackWindupTimerHandle;
	FTimerHandle AttackFinishTimerHandle;
	FTimerHandle HurtFinishTimerHandle;
	float NextAttackTime = 0.0f;
	bool bAttackInProgress = false;
	bool bHurtReacting = false;

	UPROPERTY(VisibleAnywhere, Category = "Immortal Path|Monster|UI")
	TObjectPtr<UWidgetComponent> HealthBarComponent;
};
