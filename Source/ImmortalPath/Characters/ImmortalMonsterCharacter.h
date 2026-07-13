// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Combat/AutoAttackTarget.h"
#include "PaperCharacter.h"
#include "ImmortalMonsterCharacter.generated.h"

class AController;
class UDamageType;

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

	/** Blueprint hook for hit flash, damage number and future hit animation. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Monster", meta = (DisplayName = "On Monster Damaged"))
	void BP_OnMonsterDamaged(float DamageApplied, float RemainingHealth, AActor* DamageCauser);

	/** Blueprint hook for the death flipbook and drop effects. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Monster", meta = (DisplayName = "On Monster Died"))
	void BP_OnMonsterDied(AActor* DamageCauser);

private:
	void Die(AActor* DamageCauser);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Monster", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Monster", meta = (AllowPrivateAccess = "true"))
	bool bDead = false;
};
