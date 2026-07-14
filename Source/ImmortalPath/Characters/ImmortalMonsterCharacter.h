// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Combat/AutoAttackTarget.h"
#include "PaperCharacter.h"
#include "ImmortalMonsterCharacter.generated.h"

class AController;
class APawn;
class UDamageType;
class UPaperFlipbook;

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

	/** Sent once when this monster reaches zero health. */
	UPROPERTY(BlueprintAssignable, Category = "Immortal Path|Monster")
	FMonsterDeathSignature OnMonsterDeath;

protected:
	/** Temporary test value; final balance will be data driven later. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster", meta = (ClampMin = "1.0"))
	float MaxHealth = 30.0f;

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

	/** Damage is applied this many seconds after the attack animation begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Combat", meta = (ClampMin = "0.0", Units = "s"))
	float AttackWindup = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Combat")
	bool bAutoCombatOnBeginPlay = true;

	/** Set this to match the direction drawn in the source sprite sheet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Monster|Animation")
	bool bArtworkFacesRight = false;

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
};
