// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PaperCharacter.h"
#include "ImmortalPlayerCharacter.generated.h"

class AController;
class UCameraComponent;
class UDamageType;
class USpringArmComponent;

/**
 * C++ base for the 2D player character.
 *
 * Reparent the existing Player Blueprint to this class to keep its flipbook,
 * transform and other Blueprint setup while gaining automatic attacks.
 */
UCLASS(Blueprintable)
class IMMORTALPATH_API AImmortalPlayerCharacter : public APaperCharacter
{
	GENERATED_BODY()

public:
	AImmortalPlayerCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual float TakeDamage(
		float DamageAmount,
		const FDamageEvent& DamageEvent,
		AController* EventInstigator,
		AActor* DamageCauser) override;

	/** Start searching for targets and attacking. Safe to call more than once. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Combat")
	void StartAutoAttack();

	/** Stop attacking and clear any pending strike. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Combat")
	void StopAutoAttack();

	/** Immediately perform one target search/attack attempt. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Combat")
	void TryAutoAttack();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Combat")
	AActor* GetCurrentAttackTarget() const { return CurrentAttackTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Attributes")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Attributes")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Attributes")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Attributes")
	bool IsDead() const { return bDead; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Attributes", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	/** Base damage passed to the target's AnyDamage event / TakeDamage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 10.0f;

	/** Maximum world-space distance from the player to a target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat", meta = (ClampMin = "1.0", Units = "cm"))
	float AttackRange = 220.0f;

	/** Seconds between attack attempts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat", meta = (ClampMin = "0.05", Units = "s"))
	float AttackInterval = 1.0f;

	/** Delay between starting the animation and applying damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat", meta = (ClampMin = "0.0", Units = "s"))
	float AttackWindup = 0.25f;

	/** Automatically enable combat when play begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat")
	bool bAutoAttackOnBeginPlay = true;

	/** Draw the search radius while playing for quick editor verification. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat|Debug")
	bool bDrawAttackRange = false;

	/** Shift and widen the existing Blueprint camera for idle combat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Camera")
	bool bConfigureCombatCamera = true;

	/** Moves the camera centre toward monsters so the player stays left of centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Camera", meta = (EditCondition = "bConfigureCombatCamera", Units = "cm"))
	float CameraLeadDistance = 70.0f;

	/** Width used when the Blueprint camera is orthographic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Camera", meta = (EditCondition = "bConfigureCombatCamera", ClampMin = "100.0", Units = "cm"))
	float CombatOrthoWidth = 1600.0f;

	/** Field of view used when the Blueprint camera is perspective. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Camera", meta = (EditCondition = "bConfigureCombatCamera", ClampMin = "5.0", ClampMax = "170.0", Units = "deg"))
	float CombatPerspectiveFOV = 115.0f;

	/** Optional custom damage type for later elemental/skill expansion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat")
	TSubclassOf<UDamageType> DamageTypeClass;

	/** Called when an attack begins. Override this in Player BP to play the attack flipbook. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Combat", meta = (DisplayName = "On Auto Attack Started"))
	void BP_OnAutoAttackStarted(AActor* Target);

	/** Called after the strike resolves; DamageDealt is zero if the target became invalid. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Combat", meta = (DisplayName = "On Auto Attack Resolved"))
	void BP_OnAutoAttackResolved(AActor* Target, float DamageDealt);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Attributes", meta = (DisplayName = "On Player Damaged"))
	void BP_OnPlayerDamaged(float DamageApplied, float RemainingHealth, AActor* DamageCauser);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Attributes", meta = (DisplayName = "On Player Died"))
	void BP_OnPlayerDied(AActor* DamageCauser);

private:
	void ConfigureCombatCamera();
	AActor* FindNearestTarget() const;
	bool IsTargetAttackable(const AActor* Target, bool bCheckRange) const;
	FVector GetAutoAttackLocation(const AActor* Target) const;
	void ResolvePendingAttack();

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentAttackTarget;

	FTimerHandle AutoAttackTimerHandle;
	FTimerHandle AttackWindupTimerHandle;
	bool bAttackPending = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Attributes", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Attributes", meta = (AllowPrivateAccess = "true"))
	bool bDead = false;
};
