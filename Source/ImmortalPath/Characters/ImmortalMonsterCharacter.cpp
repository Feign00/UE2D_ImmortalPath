// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalMonsterCharacter.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AImmortalMonsterCharacter::AImmortalMonsterCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	Tags.AddUnique(TEXT("Monster"));
}

void AImmortalMonsterCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = FMath::Max(MaxHealth, 1.0f);
	bDead = false;
}

float AImmortalMonsterCharacter::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	if (bDead || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	const float EngineAcceptedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	const float RequestedDamage = EngineAcceptedDamage > 0.0f ? EngineAcceptedDamage : DamageAmount;
	const float DamageApplied = FMath::Min(RequestedDamage, CurrentHealth);

	CurrentHealth = FMath::Max(CurrentHealth - DamageApplied, 0.0f);
	BP_OnMonsterDamaged(DamageApplied, CurrentHealth, DamageCauser);

	if (CurrentHealth <= 0.0f)
	{
		Die(DamageCauser);
	}

	return DamageApplied;
}

bool AImmortalMonsterCharacter::CanBeAutoAttacked_Implementation() const
{
	return !bDead;
}

FVector AImmortalMonsterCharacter::GetAutoAttackTargetLocation_Implementation() const
{
	return GetActorLocation();
}

float AImmortalMonsterCharacter::GetHealthPercent() const
{
	return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
}

void AImmortalMonsterCharacter::Die(AActor* DamageCauser)
{
	if (bDead)
	{
		return;
	}

	bDead = true;
	CurrentHealth = 0.0f;

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	OnMonsterDeath.Broadcast(this, DamageCauser);
	BP_OnMonsterDied(DamageCauser);

	if (bDestroyAfterDeath)
	{
		SetLifeSpan(FMath::Max(DeathLifeSpan, 0.01f));
	}
}
