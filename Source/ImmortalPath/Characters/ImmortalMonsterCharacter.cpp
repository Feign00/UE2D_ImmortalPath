// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalMonsterCharacter.h"

#include "../Drops/ImmortalEquipmentDrop.h"
#include "../UI/ImmortalMonsterHealthWidget.h"
#include "ImmortalPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "TimerManager.h"

AImmortalMonsterCharacter::AImmortalMonsterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	Tags.AddUnique(TEXT("Monster"));

	GetCharacterMovement()->MaxWalkSpeed = MovementSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->SetPlaneConstraintEnabled(true);
	GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, 1.0f, 0.0f));

	HealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("MonsterHealthBar"));
	HealthBarComponent->SetupAttachment(GetRootComponent());
	HealthBarComponent->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarComponent->SetDrawSize(FVector2D(256.0f, 32.0f));
	HealthBarComponent->SetPivot(FVector2D(0.5f, 1.0f));
	HealthBarComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBarComponent->SetWidgetClass(UImmortalMonsterHealthWidget::StaticClass());

	EquipmentDropClass = AImmortalEquipmentDrop::StaticClass();
}

void AImmortalMonsterCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = FMath::Max(MaxHealth, 1.0f);
	bDead = false;
	NextAttackTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	GetCharacterMovement()->MaxWalkSpeed = FMath::Max(MovementSpeed, 0.0f);
	HealthBarComponent->SetRelativeLocation(FVector(0.0f, VisualDepthOffset, HealthBarHeight));
	if (UImmortalMonsterHealthWidget* HealthWidget = Cast<UImmortalMonsterHealthWidget>(HealthBarComponent->GetUserWidgetObject()))
	{
		HealthWidget->InitializeForMonster(this);
	}

	if (UPaperFlipbookComponent* SpriteComponent = GetSprite())
	{
		FVector VisualLocation = SpriteComponent->GetRelativeLocation();
		VisualLocation.Y += VisualDepthOffset;
		SpriteComponent->SetRelativeLocation(VisualLocation);
		SpriteComponent->SetRelativeScale3D(
			SpriteComponent->GetRelativeScale3D() * FMath::Max(VisualScale, 0.1f));
		SpriteComponent->SetTranslucentSortPriority(VisualSortPriority);
	}

	if (bAutoCombatOnBeginPlay)
	{
		AcquireCombatTarget();
	}

	PlayMoveAnimation();
}

void AImmortalMonsterCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	CombatTarget.Reset();
	Super::EndPlay(EndPlayReason);
}

void AImmortalMonsterCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bAutoCombatOnBeginPlay && !bDead)
	{
		UpdateAutoCombat(DeltaSeconds);
	}
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
	const float ReducedDamage = FMath::Max(RequestedDamage - FMath::Max(Defense, 0.0f), 1.0f);
	const float DamageApplied = FMath::Min(ReducedDamage, CurrentHealth);

	CurrentHealth = FMath::Max(CurrentHealth - DamageApplied, 0.0f);
	BP_OnMonsterDamaged(DamageApplied, CurrentHealth, DamageCauser);

	if (CurrentHealth <= 0.0f)
	{
		Die(DamageCauser);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(AttackWindupTimerHandle);
		GetWorldTimerManager().ClearTimer(AttackFinishTimerHandle);
		bAttackInProgress = false;
		bHurtReacting = true;
		GetCharacterMovement()->StopMovementImmediately();
		PlayOneShotAnimation(HurtFlipbook);

		GetWorldTimerManager().SetTimer(
			HurtFinishTimerHandle,
			this,
			&AImmortalMonsterCharacter::FinishHurtReaction,
			GetAnimationDuration(HurtFlipbook, HurtAnimationDuration),
			false);
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

void AImmortalMonsterCharacter::AcquireCombatTarget()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	CombatTarget = IsValid(PlayerPawn) && !PlayerPawn->IsActorBeingDestroyed() ? PlayerPawn : nullptr;
}

void AImmortalMonsterCharacter::UpdateAutoCombat(const float DeltaSeconds)
{
	if (!CombatTarget.IsValid() || CombatTarget->IsActorBeingDestroyed())
	{
		AcquireCombatTarget();
	}

	APawn* Target = CombatTarget.Get();
	if (!Target || bAttackInProgress || bHurtReacting)
	{
		return;
	}

	const float HorizontalDelta = Target->GetActorLocation().X - GetActorLocation().X;
	const float HorizontalDistance = FMath::Abs(HorizontalDelta);
	UpdateFacing(HorizontalDelta);

	if (HorizontalDistance > AttackRange)
	{
		GetCharacterMovement()->MaxWalkSpeed = FMath::Max(MovementSpeed, 0.0f);
		AddMovementInput(FVector::ForwardVector, FMath::Sign(HorizontalDelta));
		PlayMoveAnimation();
		return;
	}

	GetCharacterMovement()->StopMovementImmediately();
	if (GetWorld() && GetWorld()->GetTimeSeconds() >= NextAttackTime)
	{
		StartAttack();
	}
}

void AImmortalMonsterCharacter::StartAttack()
{
	APawn* Target = CombatTarget.Get();
	if (!Target || bDead || bAttackInProgress || bHurtReacting || !GetWorld())
	{
		return;
	}

	bAttackInProgress = true;
	const float EffectiveAttackInterval = FMath::Max(AttackInterval, 0.05f) / FMath::Max(AttackSpeedMultiplier, 0.1f);
	NextAttackTime = GetWorld()->GetTimeSeconds() + EffectiveAttackInterval;
	GetCharacterMovement()->StopMovementImmediately();
	PlayOneShotAnimation(AttackFlipbook);
	BP_OnMonsterAttackStarted(Target);

	const float AnimationDuration = GetAnimationDuration(AttackFlipbook, AttackWindup);
	GetWorldTimerManager().SetTimer(
		AttackFinishTimerHandle,
		this,
		&AImmortalMonsterCharacter::FinishAttack,
		FMath::Max(AnimationDuration, AttackWindup + 0.01f),
		false);

	if (AttackWindup <= 0.0f)
	{
		ResolveAttack();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			AttackWindupTimerHandle,
			this,
			&AImmortalMonsterCharacter::ResolveAttack,
			AttackWindup,
			false);
	}
}

void AImmortalMonsterCharacter::ResolveAttack()
{
	APawn* Target = CombatTarget.Get();
	float DamageDealt = 0.0f;

	if (!bDead && bAttackInProgress && IsValid(Target) && !Target->IsActorBeingDestroyed())
	{
		const float HorizontalDistance = FMath::Abs(Target->GetActorLocation().X - GetActorLocation().X);
		if (HorizontalDistance <= AttackRange + 20.0f)
		{
			const bool bCriticalHit = FMath::FRand() < FMath::Clamp(CriticalChance, 0.0f, 1.0f);
			const float CriticalMultiplier = bCriticalHit ? FMath::Max(CriticalDamageMultiplier, 1.0f) : 1.0f;
			const float RequestedDamage = FMath::Max(AttackDamage, 0.0f) * CriticalMultiplier;
			DamageDealt = UGameplayStatics::ApplyDamage(
				Target,
				RequestedDamage,
				GetController(),
				this,
				UDamageType::StaticClass());
			if (bCriticalHit && DamageDealt > 0.0f)
			{
				BP_OnMonsterCriticalHit(Target, DamageDealt);
			}
		}
	}

	BP_OnMonsterAttackResolved(Target, DamageDealt);
}

void AImmortalMonsterCharacter::FinishAttack()
{
	bAttackInProgress = false;
	if (!bDead)
	{
		PlayMoveAnimation();
	}
}

void AImmortalMonsterCharacter::FinishHurtReaction()
{
	bHurtReacting = false;
	if (!bDead)
	{
		PlayMoveAnimation();
	}
}

void AImmortalMonsterCharacter::UpdateFacing(const float HorizontalDirection)
{
	if (FMath::IsNearlyZero(HorizontalDirection) || !GetSprite())
	{
		return;
	}

	const bool bWantsRight = HorizontalDirection > 0.0f;
	FVector FlipbookScale = GetSprite()->GetRelativeScale3D();
	const float Magnitude = FMath::Max(FMath::Abs(FlipbookScale.X), KINDA_SMALL_NUMBER);
	FlipbookScale.X = (bArtworkFacesRight == bWantsRight) ? Magnitude : -Magnitude;
	GetSprite()->SetRelativeScale3D(FlipbookScale);
}

void AImmortalMonsterCharacter::PlayMoveAnimation()
{
	if (!MoveFlipbook || !GetSprite() || bDead || bAttackInProgress || bHurtReacting)
	{
		return;
	}

	if (GetSprite()->GetFlipbook() != MoveFlipbook)
	{
		GetSprite()->SetFlipbook(MoveFlipbook);
	}
	GetSprite()->SetLooping(true);
	GetSprite()->Play();
}

void AImmortalMonsterCharacter::PlayOneShotAnimation(UPaperFlipbook* Flipbook)
{
	if (!Flipbook || !GetSprite())
	{
		return;
	}

	GetSprite()->SetFlipbook(Flipbook);
	GetSprite()->SetLooping(false);
	GetSprite()->PlayFromStart();
}

float AImmortalMonsterCharacter::GetAnimationDuration(UPaperFlipbook* Flipbook, const float FallbackDuration) const
{
	return FMath::Max(Flipbook ? Flipbook->GetTotalDuration() : FallbackDuration, 0.01f);
}

void AImmortalMonsterCharacter::Die(AActor* DamageCauser)
{
	if (bDead)
	{
		return;
	}

	bDead = true;
	CurrentHealth = 0.0f;
	GetWorldTimerManager().ClearAllTimersForObject(this);
	bAttackInProgress = false;
	bHurtReacting = false;

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlayOneShotAnimation(DeathFlipbook);

	OnMonsterDeath.Broadcast(this, DamageCauser);

	AImmortalPlayerCharacter* RewardPlayer = Cast<AImmortalPlayerCharacter>(DamageCauser);
	if (!RewardPlayer && DamageCauser)
	{
		RewardPlayer = Cast<AImmortalPlayerCharacter>(DamageCauser->GetOwner());
	}
	if (RewardPlayer)
	{
		RewardPlayer->ReceiveKillRewards(CultivationReward, GoldReward);
	}
	BP_OnMonsterRewardsGranted(CultivationReward, GoldReward, EquipmentDropChance, DamageCauser);
	BP_OnMonsterDied(DamageCauser);

	if (GetWorld() && EquipmentDropClass && FMath::FRand() < FMath::Clamp(EquipmentDropChance, 0.0f, 1.0f))
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AImmortalEquipmentDrop* SpawnedDrop = GetWorld()->SpawnActor<AImmortalEquipmentDrop>(
			EquipmentDropClass,
			GetActorLocation() + FVector(0.0f, 0.0f, 45.0f),
			FRotator::ZeroRotator,
			SpawnParameters))
		{
			SpawnedDrop->GenerateEquipmentForLevel(FMath::Max(EquipmentItemLevel, 1));
		}
	}

	if (bDestroyAfterDeath)
	{
		SetLifeSpan(FMath::Max(DeathLifeSpan, GetAnimationDuration(DeathFlipbook, DeathLifeSpan)));
	}
}
