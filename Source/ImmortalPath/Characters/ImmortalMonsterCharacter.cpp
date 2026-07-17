// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalMonsterCharacter.h"

#include "../Drops/ImmortalEquipmentDrop.h"
#include "../Drops/ImmortalMaterialDrop.h"
#include "../Drops/ImmortalSpiritStoneDrop.h"
#include "../Items/ImmortalMaterialTypes.h"
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
	// Spawner-created idle monsters intentionally do not need an AIController.
	// CharacterMovement otherwise discards their AddMovementInput requests.
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
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
	MaterialDropClass = AImmortalMaterialDrop::StaticClass();
	SpiritStoneDropClass = AImmortalSpiritStoneDrop::StaticClass();
}

void AImmortalMonsterCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = FMath::Max(MaxHealth, 1.0f);
	StageBaseMaxHealth = FMath::Max(MaxHealth, 1.0f);
	StageBaseAttackDamage = FMath::Max(AttackDamage, 0.0f);
	StageBaseDefense = FMath::Max(Defense, 0.0f);
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
	if (CurrentHealth > 0.0f)
	{
		UpdateBossPhase();
	}
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

void AImmortalMonsterCharacter::ConfigureForStage(const int32 Stage)
{
	const int32 SafeStage = FMath::Clamp(Stage, 1, 999);
	CurrentConfiguredStage = SafeStage;
	if (StageBaseMaxHealth <= 0.0f)
	{
		StageBaseMaxHealth = FMath::Max(MaxHealth, 1.0f);
		StageBaseAttackDamage = FMath::Max(AttackDamage, 0.0f);
		StageBaseDefense = FMath::Max(Defense, 0.0f);
	}

	const float StageIndex = static_cast<float>(SafeStage - 1);
	const float HealthScale = 1.0f + StageIndex * 0.08f + FMath::Pow(StageIndex, 1.35f) * 0.002f;
	const float AttackScale = 1.0f + StageIndex * 0.035f;
	MaxHealth = StageBaseMaxHealth * HealthScale;
	CurrentHealth = MaxHealth;
	AttackDamage = StageBaseAttackDamage * AttackScale;
	Defense = StageBaseDefense + FMath::FloorToFloat(StageIndex / 20.0f);
	EquipmentItemLevel = 1 + (SafeStage - 1) / 5;
	EquipmentDropChance = FMath::Clamp(0.30f + StageIndex * 0.0003f, 0.30f, 0.60f);
	SpiritStoneMinAmount = 1 + (SafeStage - 1) / 50;
	SpiritStoneMaxAmount = SpiritStoneMinAmount + 2 + SafeStage / 100;
	MaterialDropChance = FMath::Clamp(0.40f + StageIndex * 0.00025f, 0.40f, 0.65f);
	CultivationReward = 0;
	GoldReward = 0;
}

void AImmortalMonsterCharacter::ConfigureAsBoss(const int32 Stage)
{
	if (bIsBoss)
	{
		return;
	}

	ConfigureForStage(Stage);
	bIsBoss = true;
	CurrentBossPhase = 1;
	Tags.AddUnique(TEXT("Boss"));

	MaxHealth *= FMath::Max(BossHealthMultiplier, 1.0f);
	CurrentHealth = MaxHealth;
	AttackDamage *= FMath::Max(BossAttackMultiplier, 1.0f);
	Defense += FMath::Max(BossDefenseBonus, 0.0f);
	AttackSpeedMultiplier *= 1.05f;
	MovementSpeed *= 0.9f;
	GetCharacterMovement()->MaxWalkSpeed = FMath::Max(MovementSpeed, 0.0f);
	EquipmentDropChance = 1.0f;
	SpiritStoneDropChance = 1.0f;
	MaterialDropChance = 1.0f;

	if (UPaperFlipbookComponent* SpriteComponent = GetSprite())
	{
		SpriteComponent->SetRelativeScale3D(
			SpriteComponent->GetRelativeScale3D() * FMath::Max(BossVisualScaleMultiplier, 1.0f));
		SpriteComponent->SetTranslucentSortPriority(VisualSortPriority + 2);
	}
	if (HealthBarComponent)
	{
		HealthBarComponent->SetDrawSize(FVector2D(384.0f, 44.0f));
		HealthBarComponent->SetRelativeLocation(FVector(0.0f, VisualDepthOffset, HealthBarHeight + 35.0f));
	}

	BP_OnBossPhaseChanged(CurrentBossPhase);
	UE_LOG(LogTemp, Display,
		TEXT("Qingyun Mountain boss configured: stage %d | health %.0f | attack %.1f | defense %.1f"),
		FMath::Clamp(Stage, 1, 999), MaxHealth, AttackDamage, Defense);
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
	bBossSkillAttack = false;
	if (bIsBoss)
	{
		++BossAttackCounter;
		bBossSkillAttack = BossAttackCounter % FMath::Max(BossSkillEveryAttacks, 2) == 0;
		if (bBossSkillAttack)
		{
			BP_OnBossSkillStarted(Target);
			UE_LOG(LogTemp, Display, TEXT("Qingyun Mountain boss started heavy skill in phase %d"), CurrentBossPhase);
		}
	}
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
		const float EffectiveRange = AttackRange + 20.0f + (bBossSkillAttack ? FMath::Max(BossSkillBonusRange, 0.0f) : 0.0f);
		if (HorizontalDistance <= EffectiveRange)
		{
			const bool bCriticalHit = FMath::FRand() < FMath::Clamp(CriticalChance, 0.0f, 1.0f);
			const float CriticalMultiplier = bCriticalHit ? FMath::Max(CriticalDamageMultiplier, 1.0f) : 1.0f;
			const float SkillMultiplier = bBossSkillAttack ? FMath::Max(BossSkillDamageMultiplier, 1.0f) : 1.0f;
			const float RequestedDamage = FMath::Max(AttackDamage, 0.0f) * CriticalMultiplier * SkillMultiplier;
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
	bBossSkillAttack = false;
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

void AImmortalMonsterCharacter::UpdateBossPhase()
{
	if (!bIsBoss || bDead || MaxHealth <= 0.0f)
	{
		return;
	}

	const float HealthPercent = GetHealthPercent();
	const int32 TargetPhase = HealthPercent <= 0.33f ? 3 : (HealthPercent <= 0.66f ? 2 : 1);
	while (CurrentBossPhase < TargetPhase)
	{
		EnterBossPhase(CurrentBossPhase + 1);
	}
}

void AImmortalMonsterCharacter::EnterBossPhase(const int32 NewPhase)
{
	if (!bIsBoss || NewPhase <= CurrentBossPhase || NewPhase > 3)
	{
		return;
	}

	CurrentBossPhase = NewPhase;
	if (CurrentBossPhase == 2)
	{
		AttackDamage *= 1.18f;
		AttackSpeedMultiplier *= 1.12f;
		MovementSpeed *= 1.05f;
	}
	else
	{
		AttackDamage *= 1.25f;
		AttackSpeedMultiplier *= 1.18f;
		MovementSpeed *= 1.08f;
		CriticalChance = FMath::Clamp(CriticalChance + 0.08f, 0.0f, 1.0f);
	}
	GetCharacterMovement()->MaxWalkSpeed = FMath::Max(MovementSpeed, 0.0f);

	BP_OnBossPhaseChanged(CurrentBossPhase);
	OnBossPhaseChanged.Broadcast(this, CurrentBossPhase);
	UE_LOG(LogTemp, Display, TEXT("Qingyun Mountain boss entered phase %d at %.1f%% health"),
		CurrentBossPhase, GetHealthPercent() * 100.0f);
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

	BP_OnMonsterRewardsGranted(0, 0, bIsBoss ? 1.0f : EquipmentDropChance, DamageCauser);
	BP_OnMonsterDied(DamageCauser);

	const int32 EquipmentDropCount = bIsBoss
		? FMath::Max(BossGuaranteedEquipmentDrops, 1)
		: (FMath::FRand() < FMath::Clamp(EquipmentDropChance, 0.0f, 1.0f) ? 1 : 0);
	for (int32 DropIndex = 0; GetWorld() && EquipmentDropClass && DropIndex < EquipmentDropCount; ++DropIndex)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AImmortalEquipmentDrop* SpawnedDrop = GetWorld()->SpawnActor<AImmortalEquipmentDrop>(
			EquipmentDropClass,
			GetActorLocation() + FVector((DropIndex - EquipmentDropCount / 2) * 45.0f, 0.0f, 45.0f + DropIndex * 8.0f),
			FRotator::ZeroRotator,
			SpawnParameters))
		{
			if (bIsBoss)
			{
				SpawnedDrop->GenerateEquipmentForLevelWithMinimumQuality(
					FMath::Max(EquipmentItemLevel + BossEquipmentLevelBonus, 1),
					EImmortalEquipmentQuality::Rare);
			}
			else
			{
				SpawnedDrop->GenerateEquipmentForLevel(FMath::Max(EquipmentItemLevel, 1));
			}
		}
	}

	if (GetWorld() && SpiritStoneDropClass && (bIsBoss || FMath::FRand() < FMath::Clamp(SpiritStoneDropChance, 0.0f, 1.0f)))
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AImmortalSpiritStoneDrop* StoneDrop = GetWorld()->SpawnActor<AImmortalSpiritStoneDrop>(
			SpiritStoneDropClass,
			GetActorLocation() + FVector(35.0f, 0.0f, 38.0f),
			FRotator::ZeroRotator,
			SpawnParameters))
		{
			const int32 BaseAmount = FMath::RandRange(
				FMath::Max(SpiritStoneMinAmount, 1), FMath::Max(SpiritStoneMaxAmount, SpiritStoneMinAmount));
			StoneDrop->SetAmount(BaseAmount * (bIsBoss ? FMath::Max(BossSpiritStoneMultiplier, 1) : 1));
		}
	}

	const int32 MaterialDropCount = bIsBoss
		? FMath::Max(BossGuaranteedMaterialDrops, 1)
		: (FMath::FRand() < FMath::Clamp(MaterialDropChance, 0.0f, 1.0f) ? 1 : 0);
	for (int32 DropIndex = 0; GetWorld() && MaterialDropClass && DropIndex < MaterialDropCount; ++DropIndex)
	{
		const FImmortalMaterialStack Material = UImmortalMaterialLibrary::GenerateStageDrop(
			CurrentConfiguredStage, bIsBoss, DropIndex);
		if (!Material.IsValid())
		{
			continue;
		}
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AImmortalMaterialDrop* MaterialDrop = GetWorld()->SpawnActor<AImmortalMaterialDrop>(
			MaterialDropClass,
			GetActorLocation() + FVector(-45.0f + DropIndex * 45.0f, 0.0f, 70.0f + DropIndex * 7.0f),
			FRotator::ZeroRotator,
			SpawnParameters))
		{
			MaterialDrop->SetMaterialDrop(Material.MaterialId, Material.Quantity);
		}
	}

	if (bIsBoss)
	{
		UE_LOG(LogTemp, Display, TEXT("Qingyun Mountain boss rewards spawned: %d rare+ equipment, spirit stones and %d material entities"),
			EquipmentDropCount, MaterialDropCount);
	}

	if (bDestroyAfterDeath)
	{
		SetLifeSpan(FMath::Max(DeathLifeSpan, GetAnimationDuration(DeathFlipbook, DeathLifeSpan)));
	}
}
