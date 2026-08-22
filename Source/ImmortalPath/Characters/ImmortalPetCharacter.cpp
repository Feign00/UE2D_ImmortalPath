// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPetCharacter.h"

#include "../Combat/AutoAttackTarget.h"
#include "ImmortalPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "TimerManager.h"

AImmortalPetCharacter::AImmortalPetCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	Tags.AddUnique(TEXT("Pet"));

	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->SetPlaneConstraintEnabled(true);
	GetCharacterMovement()->SetPlaneConstraintNormal(
		FVector(0.0f, 1.0f, 0.0f));

	GetCapsuleComponent()->InitCapsuleSize(24.0f, 34.0f);
	GetCapsuleComponent()->SetCollisionEnabled(
		ECollisionEnabled::QueryAndPhysics);
	GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(
		ECC_WorldStatic, ECR_Block);
	GetCapsuleComponent()->SetGenerateOverlapEvents(false);
	GetCapsuleComponent()->SetCanEverAffectNavigation(false);

	if (UPaperFlipbookComponent* SpriteComponent = GetSprite())
	{
		BaseSpriteRelativeLocation =
			SpriteComponent->GetRelativeLocation();
		SpriteComponent->SetCollisionEnabled(
			ECollisionEnabled::NoCollision);
		SpriteComponent->SetTranslucentSortPriority(15);
		SpriteComponent->SetCastShadow(false);
	}
}

void AImmortalPetCharacter::BeginPlay()
{
	Super::BeginPlay();
	const float CurrentTime =
		GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	NextAttackTime = CurrentTime + 0.10f;
	NextTargetSearchTime = CurrentTime;

	if (PlayerOwner.IsValid() && !PetId.IsNone())
	{
		RefreshFromPersistentState();
	}
}

void AImmortalPetCharacter::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	CurrentTarget.Reset();
	PlayerOwner.Reset();
	Super::EndPlay(EndPlayReason);
}

void AImmortalPetCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AImmortalPlayerCharacter* Player = PlayerOwner.Get();
	if (!Player || Player->IsActorBeingDestroyed())
	{
		Destroy();
		return;
	}
	if (!bRuntimeDefinitionValid)
	{
		return;
	}

	if (Player->IsDead())
	{
		EnterOwnerDeathState();
		return;
	}
	if (bOwnerDeathState)
	{
		ExitOwnerDeathState();
	}
	if (bDevelopmentDeathPreviewActive || bHurtPreviewActive)
	{
		return;
	}

	UpdateFollowAndCombat(DeltaSeconds);
}

bool AImmortalPetCharacter::InitializeForPlayer(
	AImmortalPlayerCharacter* InPlayer,
	const FName InPetId)
{
	PlayerOwner = InPlayer;
	PetId = InPetId;
	AttackCount = 0;
	HitCount = 0;
	CurrentTarget.Reset();
	bOwnerDeathState = false;
	bHurtPreviewActive = false;
	bDevelopmentDeathPreviewActive = false;
	CancelActiveAttack(true);

	if (!InPlayer || InPetId.IsNone())
	{
		DisableInvalidPet();
		return false;
	}

	SetOwner(InPlayer);
	SetInstigator(InPlayer);
	RefreshFromPersistentState();
	if (!bRuntimeDefinitionValid)
	{
		return false;
	}

	SetActorLocation(
		GetFollowLocation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	if (InPlayer->IsDead())
	{
		EnterOwnerDeathState();
	}
	else
	{
		PlayMoveAnimation();
	}
	return true;
}

void AImmortalPetCharacter::RefreshFromPersistentState()
{
	AImmortalPlayerCharacter* Player = PlayerOwner.Get();
	FImmortalPetProgress Progress;
	FImmortalPetDefinition Definition;
	if (!Player || PetId.IsNone()
		|| !Player->GetPetProgress(PetId, Progress)
		|| !Progress.bOwned
		|| !UImmortalPetLibrary::GetPetDefinition(
			PetId, Definition)
		|| !Definition.IsValid())
	{
		DisableInvalidPet();
		return;
	}

	ActiveDefinition = Definition;
	MoveFlipbook =
		ActiveDefinition.MoveFlipbook.LoadSynchronous();
	AttackFlipbook =
		ActiveDefinition.AttackFlipbook.LoadSynchronous();
	HurtFlipbook =
		ActiveDefinition.HurtFlipbook.LoadSynchronous();
	DeathFlipbook =
		ActiveDefinition.DeathFlipbook.LoadSynchronous();

	GetCharacterMovement()->MaxWalkSpeed =
		FMath::Max(ActiveDefinition.MovementSpeed, 1.0f);
	if (UPaperFlipbookComponent* SpriteComponent = GetSprite())
	{
		SpriteComponent->SetRelativeLocation(
			BaseSpriteRelativeLocation
				// The TBH camera sits on the positive-Y side of the 2D plane.
				// Keep the pet in front of opaque tile-map layers and lift the
				// supplied Fox/Dog pivots slightly above the walk surface.
				+ FVector(0.0f, 160.0f, 34.0f));
		SpriteComponent->SetRelativeScale3D(
			FVector(
				FMath::Max(ActiveDefinition.VisualScale, 0.1f)));
		SpriteComponent->SetSpriteColor(
			ActiveDefinition.DisplayColor);
		SpriteComponent->SetTranslucentSortPriority(15);
	}

	bRuntimeDefinitionValid = true;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
	CurrentTarget.Reset();
	const float CurrentTime =
		GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	NextAttackTime = CurrentTime + 0.10f;
	NextTargetSearchTime = CurrentTime;

	if (Player->IsDead())
	{
		EnterOwnerDeathState();
	}
	else if (!bDevelopmentDeathPreviewActive
		&& !bHurtPreviewActive)
	{
		PlayMoveAnimation();
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Pet runtime refreshed: id=%s level=%d stars=%d move=%s attack=%s hurt=%s death=%s range=%.0f interval=%.2f"),
		*PetId.ToString(),
		Progress.Level,
		Progress.Stars,
		MoveFlipbook ? TEXT("loaded") : TEXT("missing"),
		AttackFlipbook ? TEXT("loaded") : TEXT("missing"),
		HurtFlipbook ? TEXT("loaded") : TEXT("missing"),
		DeathFlipbook ? TEXT("loaded") : TEXT("missing"),
		ActiveDefinition.AttackRange,
		ActiveDefinition.AttackInterval);
}

bool AImmortalPetCharacter::PreviewHurtForDevelopment()
{
#if !UE_BUILD_SHIPPING
	if (!bRuntimeDefinitionValid || !PlayerOwner.IsValid()
		|| PlayerOwner->IsDead() || bOwnerDeathState
		|| bDevelopmentDeathPreviewActive)
	{
		return false;
	}

	CancelActiveAttack(true);
	GetWorldTimerManager().ClearTimer(HurtPreviewTimerHandle);
	bHurtPreviewActive = true;
	GetCharacterMovement()->StopMovementImmediately();
	PlayOneShotAnimation(HurtFlipbook);
	GetWorldTimerManager().SetTimer(
		HurtPreviewTimerHandle,
		this,
		&AImmortalPetCharacter::FinishHurtPreview,
		GetAnimationDuration(HurtFlipbook, 0.45f),
		false);
	UE_LOG(LogTemp, Display,
		TEXT("Pet hurt animation preview started: pet=%s animation=%s"),
		*PetId.ToString(),
		HurtFlipbook ? TEXT("loaded") : TEXT("missing"));
	return true;
#else
	return false;
#endif
}

bool AImmortalPetCharacter::PreviewDeathForDevelopment()
{
#if !UE_BUILD_SHIPPING
	if (!bRuntimeDefinitionValid || !PlayerOwner.IsValid()
		|| PlayerOwner->IsDead() || bOwnerDeathState)
	{
		return false;
	}

	CancelActiveAttack(true);
	GetWorldTimerManager().ClearTimer(HurtPreviewTimerHandle);
	GetWorldTimerManager().ClearTimer(
		DevelopmentDeathPreviewTimerHandle);
	bHurtPreviewActive = false;
	bDevelopmentDeathPreviewActive = true;
	GetCharacterMovement()->StopMovementImmediately();
	PlayOneShotAnimation(DeathFlipbook);
	GetWorldTimerManager().SetTimer(
		DevelopmentDeathPreviewTimerHandle,
		this,
		&AImmortalPetCharacter::FinishDevelopmentDeathPreview,
		GetAnimationDuration(DeathFlipbook, 0.75f),
		false);
	UE_LOG(LogTemp, Display,
		TEXT("Pet death animation preview started: pet=%s animation=%s"),
		*PetId.ToString(),
		DeathFlipbook ? TEXT("loaded") : TEXT("missing"));
	return true;
#else
	return false;
#endif
}

void AImmortalPetCharacter::UpdateFollowAndCombat(
	const float DeltaSeconds)
{
	(void)DeltaSeconds;
	AImmortalPlayerCharacter* Player = PlayerOwner.Get();
	if (!Player || !GetWorld())
	{
		return;
	}

	const float OwnerDistance = FVector::Dist2D(
		GetActorLocation(), Player->GetActorLocation());
	if (OwnerDistance
		> FMath::Max(
			ActiveDefinition.MaximumLeashDistance, 100.0f))
	{
		CancelActiveAttack(true);
		ReturnToFollowPoint(true);
		return;
	}
	if (bAttackPending)
	{
		GetCharacterMovement()->StopMovementImmediately();
		return;
	}

	AActor* Target = CurrentTarget.Get();
	if (!IsTargetAttackable(Target, true))
	{
		CurrentTarget.Reset();
		Target = nullptr;
	}
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (!Target && CurrentTime >= NextTargetSearchTime)
	{
		CurrentTarget = FindNearestTarget();
		Target = CurrentTarget.Get();
		NextTargetSearchTime = CurrentTime + 0.15f;
	}

	if (!Target)
	{
		ReturnToFollowPoint(false);
		return;
	}

	const FVector TargetLocation = GetPetTargetLocation(Target);
	const float HorizontalDelta =
		TargetLocation.X - GetActorLocation().X;
	const float HorizontalDistance = FMath::Abs(HorizontalDelta);
	UpdateFacing(HorizontalDelta);
	if (HorizontalDistance
		<= FMath::Max(ActiveDefinition.AttackRange, 1.0f))
	{
		GetCharacterMovement()->StopMovementImmediately();
		if (CurrentTime >= NextAttackTime)
		{
			BeginAttack(Target);
		}
		else
		{
			// There is no separate Fox/Dog idle flipbook. Keeping the move
			// loop active preserves the authored breathing motion.
			PlayMoveAnimation();
		}
		return;
	}

	const float TargetOwnerDistance = FVector::Dist2D(
		TargetLocation, Player->GetActorLocation());
	const bool bWouldMoveFartherFromOwner =
		TargetOwnerDistance > OwnerDistance;
	if (OwnerDistance
			>= FMath::Max(
				ActiveDefinition.MaximumLeashDistance - 25.0f,
				75.0f)
		&& bWouldMoveFartherFromOwner)
	{
		CurrentTarget.Reset();
		ReturnToFollowPoint(false);
		return;
	}
	MoveHorizontally(HorizontalDelta);
}

void AImmortalPetCharacter::ReturnToFollowPoint(
	const bool bTeleport)
{
	const FVector FollowLocation = GetFollowLocation();
	if (bTeleport)
	{
		SetActorLocation(
			FollowLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		GetCharacterMovement()->StopMovementImmediately();
		PlayMoveAnimation();
		return;
	}

	const float HorizontalDelta =
		FollowLocation.X - GetActorLocation().X;
	if (FMath::Abs(HorizontalDelta) > 38.0f)
	{
		UpdateFacing(HorizontalDelta);
		MoveHorizontally(HorizontalDelta);
	}
	else
	{
		GetCharacterMovement()->StopMovementImmediately();
		PlayMoveAnimation();
	}
}

void AImmortalPetCharacter::MoveHorizontally(
	const float Direction)
{
	if (FMath::IsNearlyZero(Direction))
	{
		GetCharacterMovement()->StopMovementImmediately();
		return;
	}
	GetCharacterMovement()->MaxWalkSpeed =
		FMath::Max(ActiveDefinition.MovementSpeed, 1.0f);
	AddMovementInput(
		FVector::ForwardVector,
		FMath::Sign(Direction));
	PlayMoveAnimation();
}

FVector AImmortalPetCharacter::GetFollowLocation() const
{
	const AImmortalPlayerCharacter* Player = PlayerOwner.Get();
	if (!Player)
	{
		return GetActorLocation();
	}

	FVector Result = Player->GetActorLocation();
	Result.X += ActiveDefinition.FollowOffsetX;
	if (const UCapsuleComponent* PlayerCapsule =
		Player->GetCapsuleComponent())
	{
		Result.Z -= PlayerCapsule->GetScaledCapsuleHalfHeight();
		Result.Z += GetCapsuleComponent()
			->GetScaledCapsuleHalfHeight();
	}
	return Result;
}

AActor* AImmortalPetCharacter::FindNearestTarget() const
{
	const AImmortalPlayerCharacter* Player = PlayerOwner.Get();
	UWorld* World = GetWorld();
	if (!Player || !World || !bRuntimeDefinitionValid)
	{
		return nullptr;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(ImmortalPathPetTargetSearch),
		false,
		this);
	QueryParams.AddIgnoredActor(Player);
	World->OverlapMultiByObjectType(
		Overlaps,
		Player->GetActorLocation(),
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(
			FMath::Max(ActiveDefinition.SearchRange, 1.0f)),
		QueryParams);

	AActor* NearestTarget = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!IsTargetAttackable(Candidate, true))
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(
			GetActorLocation(), GetPetTargetLocation(Candidate));
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestTarget = Candidate;
		}
	}
	return NearestTarget;
}

bool AImmortalPetCharacter::IsTargetAttackable(
	const AActor* Target,
	const bool bCheckSearchRange) const
{
	const AImmortalPlayerCharacter* Player = PlayerOwner.Get();
	if (!Player || !IsValid(Target) || Target == this
		|| Target == Player || Target->IsActorBeingDestroyed()
		|| !Target->Implements<UAutoAttackTarget>()
		|| !IAutoAttackTarget::Execute_CanBeAutoAttacked(
			const_cast<AActor*>(Target)))
	{
		return false;
	}
	if (!bCheckSearchRange)
	{
		return true;
	}

	const float OwnerDistance = FVector::Dist2D(
		GetPetTargetLocation(Target), Player->GetActorLocation());
	const float MaximumReach = FMath::Min(
		FMath::Max(ActiveDefinition.SearchRange, 1.0f),
		FMath::Max(
			ActiveDefinition.MaximumLeashDistance, 100.0f)
			+ FMath::Max(ActiveDefinition.AttackRange, 1.0f));
	return OwnerDistance <= MaximumReach;
}

FVector AImmortalPetCharacter::GetPetTargetLocation(
	const AActor* Target) const
{
	if (Target && Target->Implements<UAutoAttackTarget>())
	{
		return IAutoAttackTarget::Execute_GetAutoAttackTargetLocation(
			const_cast<AActor*>(Target));
	}
	return Target ? Target->GetActorLocation() : FVector::ZeroVector;
}

void AImmortalPetCharacter::BeginAttack(AActor* Target)
{
	if (bAttackPending || bOwnerDeathState
		|| bDevelopmentDeathPreviewActive || bHurtPreviewActive
		|| !GetWorld() || !IsTargetAttackable(Target, true))
	{
		return;
	}
	const float HorizontalDistance = FMath::Abs(
		GetPetTargetLocation(Target).X - GetActorLocation().X);
	if (HorizontalDistance
		> FMath::Max(ActiveDefinition.AttackRange, 1.0f))
	{
		return;
	}

	bAttackPending = true;
	CurrentTarget = Target;
	AttackCount = AttackCount < MAX_int32
		? AttackCount + 1 : MAX_int32;
	NextAttackTime = GetWorld()->GetTimeSeconds()
		+ FMath::Max(ActiveDefinition.AttackInterval, 0.1f);
	GetCharacterMovement()->StopMovementImmediately();
	UpdateFacing(
		GetPetTargetLocation(Target).X - GetActorLocation().X);
	PlayOneShotAnimation(AttackFlipbook);

	const float AnimationDuration =
		GetAnimationDuration(AttackFlipbook, 0.60f);
	const float MaximumWindup = FMath::Max(
		FMath::Min(AnimationDuration * 0.80f, 0.45f),
		0.05f);
	const float AttackWindup = FMath::Clamp(
		AnimationDuration * 0.45f,
		0.05f,
		MaximumWindup);
	GetWorldTimerManager().SetTimer(
		AttackWindupTimerHandle,
		this,
		&AImmortalPetCharacter::ResolveAttack,
		AttackWindup,
		false);
	GetWorldTimerManager().SetTimer(
		AttackFinishTimerHandle,
		this,
		&AImmortalPetCharacter::FinishAttack,
		FMath::Max(AnimationDuration, AttackWindup + 0.01f),
		false);
}

void AImmortalPetCharacter::ResolveAttack()
{
	AImmortalPlayerCharacter* Player = PlayerOwner.Get();
	AActor* Target = CurrentTarget.Get();
	if (!bAttackPending || !Player || Player->IsDead()
		|| !IsTargetAttackable(Target, true))
	{
		return;
	}

	const float HorizontalDistance = FMath::Abs(
		GetPetTargetLocation(Target).X - GetActorLocation().X);
	if (HorizontalDistance
		> FMath::Max(ActiveDefinition.AttackRange + 24.0f, 1.0f))
	{
		return;
	}

	// Passing zero keeps all persistent level/star scaling and reward
	// attribution inside the authoritative player damage pipeline.
	const float DamageApplied =
		Player->ResolvePetAttack(this, Target, 0.0f);
	if (DamageApplied > 0.0f)
	{
		HitCount = HitCount < MAX_int32
			? HitCount + 1 : MAX_int32;
	}
}

void AImmortalPetCharacter::FinishAttack()
{
	GetWorldTimerManager().ClearTimer(AttackWindupTimerHandle);
	bAttackPending = false;
	if (!IsTargetAttackable(CurrentTarget.Get(), true))
	{
		CurrentTarget.Reset();
	}
	if (PlayerOwner.IsValid() && !PlayerOwner->IsDead()
		&& !bOwnerDeathState && !bHurtPreviewActive
		&& !bDevelopmentDeathPreviewActive)
	{
		PlayMoveAnimation();
	}
}

void AImmortalPetCharacter::CancelActiveAttack(
	const bool bClearTarget)
{
	GetWorldTimerManager().ClearTimer(AttackWindupTimerHandle);
	GetWorldTimerManager().ClearTimer(AttackFinishTimerHandle);
	bAttackPending = false;
	if (bClearTarget)
	{
		CurrentTarget.Reset();
	}
}

void AImmortalPetCharacter::EnterOwnerDeathState()
{
	if (bOwnerDeathState)
	{
		return;
	}
	bOwnerDeathState = true;
	CancelActiveAttack(true);
	GetWorldTimerManager().ClearTimer(HurtPreviewTimerHandle);
	GetWorldTimerManager().ClearTimer(
		DevelopmentDeathPreviewTimerHandle);
	bHurtPreviewActive = false;
	bDevelopmentDeathPreviewActive = false;
	GetCharacterMovement()->StopMovementImmediately();
	PlayOneShotAnimation(DeathFlipbook);
	UE_LOG(LogTemp, Display,
		TEXT("Pet entered owner-death state: pet=%s animation=%s"),
		*PetId.ToString(),
		DeathFlipbook ? TEXT("loaded") : TEXT("missing"));
}

void AImmortalPetCharacter::ExitOwnerDeathState()
{
	bOwnerDeathState = false;
	const float CurrentTime =
		GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	NextAttackTime = CurrentTime + 0.10f;
	NextTargetSearchTime = CurrentTime;
	if (PlayerOwner.IsValid()
		&& FVector::Dist2D(
			GetActorLocation(),
			PlayerOwner->GetActorLocation())
			> FMath::Max(
				ActiveDefinition.MaximumLeashDistance, 100.0f))
	{
		ReturnToFollowPoint(true);
	}
	else
	{
		PlayMoveAnimation();
	}
	UE_LOG(LogTemp, Display,
		TEXT("Pet resumed after owner revive: pet=%s"),
		*PetId.ToString());
}

void AImmortalPetCharacter::FinishHurtPreview()
{
	bHurtPreviewActive = false;
	if (PlayerOwner.IsValid() && PlayerOwner->IsDead())
	{
		EnterOwnerDeathState();
		return;
	}
	PlayMoveAnimation();
}

void AImmortalPetCharacter::FinishDevelopmentDeathPreview()
{
	bDevelopmentDeathPreviewActive = false;
	if (PlayerOwner.IsValid() && PlayerOwner->IsDead())
	{
		EnterOwnerDeathState();
		return;
	}
	PlayMoveAnimation();
}

void AImmortalPetCharacter::UpdateFacing(
	const float HorizontalDirection)
{
	if (FMath::IsNearlyZero(HorizontalDirection) || !GetSprite())
	{
		return;
	}

	// Both supplied Fox/Dog sheets face left.
	const bool bWantsRight = HorizontalDirection > 0.0f;
	FVector Scale = GetSprite()->GetRelativeScale3D();
	const float Magnitude =
		FMath::Max(FMath::Abs(Scale.X), KINDA_SMALL_NUMBER);
	Scale.X = bWantsRight ? -Magnitude : Magnitude;
	GetSprite()->SetRelativeScale3D(Scale);
}

void AImmortalPetCharacter::PlayMoveAnimation()
{
	if (!MoveFlipbook || !GetSprite() || bOwnerDeathState
		|| bAttackPending || bHurtPreviewActive
		|| bDevelopmentDeathPreviewActive)
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

void AImmortalPetCharacter::PlayOneShotAnimation(
	UPaperFlipbook* Flipbook)
{
	if (!Flipbook || !GetSprite())
	{
		return;
	}
	GetSprite()->SetFlipbook(Flipbook);
	GetSprite()->SetLooping(false);
	GetSprite()->PlayFromStart();
}

float AImmortalPetCharacter::GetAnimationDuration(
	UPaperFlipbook* Flipbook,
	const float FallbackDuration) const
{
	return FMath::Max(
		Flipbook ? Flipbook->GetTotalDuration()
			: FallbackDuration,
		0.01f);
}

void AImmortalPetCharacter::DisableInvalidPet()
{
	bRuntimeDefinitionValid = false;
	CancelActiveAttack(true);
	GetWorldTimerManager().ClearTimer(HurtPreviewTimerHandle);
	GetWorldTimerManager().ClearTimer(
		DevelopmentDeathPreviewTimerHandle);
	bHurtPreviewActive = false;
	bDevelopmentDeathPreviewActive = false;
	bOwnerDeathState = false;
	GetCharacterMovement()->StopMovementImmediately();
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Pet runtime disabled because its owner, ownership state or definition is invalid: id=%s"),
		*PetId.ToString());
}
