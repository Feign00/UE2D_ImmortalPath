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

void AImmortalMonsterCharacter::ConfigureForMapStage(const FName MapId, const int32 Stage)
{
	ConfigureForStage(Stage);
	FImmortalMapDefinition Definition;
	if (!UImmortalMapLibrary::GetMapDefinition(MapId, Definition))
	{
		UImmortalMapLibrary::GetMapDefinition(UImmortalMapLibrary::GetQingyunMountainId(), Definition);
	}
	CurrentMapId = Definition.MapId;
	MonsterDisplayName = Definition.NormalMonsterName;
	MinimumEquipmentDropQuality = Definition.MinimumEquipmentQuality;
	MinimumBossEquipmentDropQuality = Definition.BossMinimumEquipmentQuality;
	MaxHealth *= FMath::Max(Definition.HealthMultiplier, 0.01f);
	CurrentHealth = MaxHealth;
	AttackDamage *= FMath::Max(Definition.AttackMultiplier, 0.01f);
	Defense = Defense * FMath::Max(Definition.DefenseMultiplier, 0.0f) + Definition.OrderIndex * 0.5f;
	EquipmentItemLevel = FMath::Max(EquipmentItemLevel + Definition.EquipmentLevelBonus, 1);
	EquipmentDropChance = FMath::Clamp(
		EquipmentDropChance + FMath::Max(Definition.EquipmentDropChanceBonus, 0.0f), 0.0f, 0.90f);
	MaterialDropChance = FMath::Clamp(
		MaterialDropChance + FMath::Max(Definition.MaterialDropChanceBonus, 0.0f), 0.0f, 0.90f);
	SpiritStoneMinAmount = FMath::Max(FMath::CeilToInt32(
		SpiritStoneMinAmount * FMath::Max(Definition.SpiritStoneMultiplier, 0.1f)), 1);
	SpiritStoneMaxAmount = FMath::Max(FMath::CeilToInt32(
		SpiritStoneMaxAmount * FMath::Max(Definition.SpiritStoneMultiplier, 0.1f)), SpiritStoneMinAmount);
	if (UPaperFlipbookComponent* SpriteComponent = GetSprite())
	{
		SpriteComponent->SetSpriteColor(Definition.MonsterTint);
	}
	UE_LOG(LogTemp, Display, TEXT("Monster configured for map %s stage %d: health %.0f | attack %.1f | defense %.1f | item level %d"),
		*CurrentMapId.ToString(), CurrentConfiguredStage, MaxHealth, AttackDamage, Defense, EquipmentItemLevel);
}

void AImmortalMonsterCharacter::ConfigureAsWorldBossMinion(
	const FName MapId,
	const int32 Stage)
{
	// World Boss summons use the encounter's recommended stage only. The map
	// identifier is retained for presentation/debugging but never scales stats.
	ConfigureForStage(Stage);
	FImmortalMapDefinition PresentationMap;
	if (!UImmortalMapLibrary::GetMapDefinition(MapId, PresentationMap))
	{
		UImmortalMapLibrary::GetMapDefinition(
			UImmortalMapLibrary::GetQingyunMountainId(), PresentationMap);
	}
	CurrentMapId = PresentationMap.MapId;
	MonsterDisplayName = FText::FromString(TEXT("妖王侍从"));
	Tags.AddUnique(TEXT("WorldBossMinion"));
	CultivationReward = 0;
	GoldReward = 0;
	EquipmentDropChance = 0.0f;
	SpiritStoneDropChance = 0.0f;
	MaterialDropChance = 0.0f;
	UE_LOG(LogTemp, Display,
		TEXT("World Boss minion configured independently: stage=%d equipmentChance=0 stoneChance=0 materialChance=0"),
		CurrentConfiguredStage);
}

void AImmortalMonsterCharacter::ConfigureAsBoss(const int32 Stage)
{
	ConfigureAsMapBoss(UImmortalMapLibrary::GetQingyunMountainId(), Stage);
}

void AImmortalMonsterCharacter::ConfigureAsMapBoss(const FName MapId, const int32 Stage)
{
	if (bIsBoss)
	{
		return;
	}

	ConfigureForMapStage(MapId, Stage);
	bIsBoss = true;
	CurrentBossPhase = 1;
	Tags.AddUnique(TEXT("Boss"));
	FImmortalMapDefinition MapDefinition;
	UImmortalMapLibrary::GetMapDefinition(CurrentMapId, MapDefinition);
	MonsterDisplayName = MapDefinition.BossName;

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
		SpriteComponent->SetSpriteColor(MapDefinition.BossColor);
	}
	if (HealthBarComponent)
	{
		HealthBarComponent->SetDrawSize(FVector2D(384.0f, 44.0f));
		HealthBarComponent->SetRelativeLocation(FVector(0.0f, VisualDepthOffset, HealthBarHeight + 35.0f));
	}

	BP_OnBossPhaseChanged(CurrentBossPhase);
	UE_LOG(LogTemp, Display,
		TEXT("Map boss configured: %s | map %s | stage %d | health %.0f | attack %.1f | defense %.1f"),
		*MonsterDisplayName.ToString(), *CurrentMapId.ToString(), FMath::Clamp(Stage, 1, 999), MaxHealth, AttackDamage, Defense);
}

void AImmortalMonsterCharacter::ConfigureAsWorldBoss(
	const FImmortalWorldBossDefinition& Definition,
	const FName PresentationMapId)
{
	if (bIsBoss || !Definition.IsValid())
	{
		return;
	}

	// World Boss difficulty is stable regardless of the map from which the
	// optional challenge is opened. PresentationMapId only keeps the encounter
	// associated with the current logical scene.
	ConfigureForStage(Definition.RecommendedStage);
	CurrentMapId = PresentationMapId.IsNone()
		? UImmortalMapLibrary::GetQingyunMountainId()
		: PresentationMapId;
	bIsBoss = true;
	bIsWorldBoss = true;
	WorldBossId = Definition.BossId;
	CurrentBossPhase = 1;
	Tags.AddUnique(TEXT("Boss"));
	Tags.AddUnique(TEXT("WorldBoss"));
	MonsterDisplayName = Definition.DisplayName;

	MaxHealth *= FMath::Max(Definition.HealthMultiplier, 1.0f);
	CurrentHealth = MaxHealth;
	AttackDamage *= FMath::Max(Definition.AttackMultiplier, 0.1f);
	Defense += FMath::Max(Definition.DefenseBonus, 0.0f);
	AttackSpeedMultiplier *= FMath::Max(Definition.AttackSpeedMultiplier, 0.1f);
	BossSkillEveryAttacks = FMath::Max(Definition.SkillEveryAttacks, 2);
	BossSkillDamageMultiplier = FMath::Max(Definition.SkillDamageMultiplier, 1.0f);
	BossSkillBonusRange = FMath::Max(Definition.SkillBonusRange, 0.0f);
	MovementSpeed *= 0.92f;
	GetCharacterMovement()->MaxWalkSpeed = FMath::Max(MovementSpeed, 0.0f);
	NextWorldBossRangedSkillTime = GetWorld() ? GetWorld()->GetTimeSeconds() + 1.25f : 1.25f;

	// Rewards are committed atomically by the player-owned World Boss state.
	// Keeping the generic fields disabled prevents a second map-Boss drop pool.
	EquipmentDropChance = 0.0f;
	SpiritStoneDropChance = 0.0f;
	MaterialDropChance = 0.0f;

	if (UPaperFlipbookComponent* SpriteComponent = GetSprite())
	{
		SpriteComponent->SetRelativeScale3D(
			SpriteComponent->GetRelativeScale3D()
				* FMath::Max(Definition.VisualScaleMultiplier, 1.0f));
		SpriteComponent->SetTranslucentSortPriority(VisualSortPriority + 4);
		SpriteComponent->SetSpriteColor(Definition.DisplayColor);
	}
	if (HealthBarComponent)
	{
		HealthBarComponent->SetDrawSize(FVector2D(460.0f, 48.0f));
		HealthBarComponent->SetRelativeLocation(
			FVector(0.0f, VisualDepthOffset, HealthBarHeight + 45.0f));
	}

	BP_OnBossPhaseChanged(CurrentBossPhase);
	UE_LOG(LogTemp, Display,
		TEXT("World Boss configured: %s (%s) | recommended stage %d | health %.0f | attack %.1f | defense %.1f | ranged %.0f"),
		*MonsterDisplayName.ToString(), *WorldBossId.ToString(), Definition.RecommendedStage,
		MaxHealth, AttackDamage, Defense, AttackRange + BossSkillBonusRange);
}

void AImmortalMonsterCharacter::ConfigureForEndlessFloor(
	const int32 Floor,
	const bool bBoss,
	const bool bElite,
	const FGuid RunId,
	const FName PresentationMapId,
	const float RuleHealthMultiplier,
	const float RuleAttackMultiplier,
	const float RuleDefenseBonus)
{
	if (bIsWorldBoss || bIsEndlessEnemy || bIsBoss || !RunId.IsValid())
	{
		UE_LOG(LogTemp, Error,
			TEXT("Rejected Endless enemy configuration: floor=%d boss=%s elite=%s runValid=%s alreadyBoss=%s alreadyEndless=%s worldBoss=%s"),
			Floor,
			bBoss ? TEXT("true") : TEXT("false"),
			bElite ? TEXT("true") : TEXT("false"),
			RunId.IsValid() ? TEXT("true") : TEXT("false"),
			bIsBoss ? TEXT("true") : TEXT("false"),
			bIsEndlessEnemy ? TEXT("true") : TEXT("false"),
			bIsWorldBoss ? TEXT("true") : TEXT("false"));
		return;
	}

	// Endless scaling is deliberately independent from ConfigureForStage,
	// whose adventure-map contract clamps at stage 999. These bounded
	// linear/power curves remain finite and strictly increase through floor
	// 9999 without allowing an untrusted floor value to overflow float stats.
	const int32 SafeFloor = FMath::Clamp(Floor, 1, 9999);
	const double FloorIndex = static_cast<double>(SafeFloor - 1);
	const double HealthScaleDouble =
		1.0 + FloorIndex * 0.070 + FMath::Pow(FloorIndex, 1.32) * 0.0060;
	const double AttackScaleDouble =
		1.0 + FloorIndex * 0.032 + FMath::Pow(FloorIndex, 1.22) * 0.00045;
	const double DefenseGrowthDouble =
		FloorIndex * 0.120 + FMath::Pow(FloorIndex, 1.18) * 0.0020;
	const float HealthScale =
		FMath::IsFinite(RuleHealthMultiplier)
			&& RuleHealthMultiplier > 0.0f
		? FMath::Clamp(RuleHealthMultiplier, 0.1f, 1000000.0f)
		: static_cast<float>(
			FMath::Clamp(HealthScaleDouble, 1.0, 1000000.0));
	const float AttackScale =
		FMath::IsFinite(RuleAttackMultiplier)
			&& RuleAttackMultiplier > 0.0f
		? FMath::Clamp(RuleAttackMultiplier, 0.1f, 1000000.0f)
		: static_cast<float>(
			FMath::Clamp(AttackScaleDouble, 1.0, 1000000.0));
	const float DefenseGrowth =
		FMath::IsFinite(RuleDefenseBonus)
			&& RuleDefenseBonus >= 0.0f
		? FMath::Clamp(RuleDefenseBonus, 0.0f, 1000000.0f)
		: static_cast<float>(
			FMath::Clamp(DefenseGrowthDouble, 0.0, 1000000.0));

	if (StageBaseMaxHealth <= 0.0f)
	{
		StageBaseMaxHealth = FMath::Max(MaxHealth, 1.0f);
		StageBaseAttackDamage = FMath::Max(AttackDamage, 0.0f);
		StageBaseDefense = FMath::Max(Defense, 0.0f);
	}

	CurrentConfiguredStage = SafeFloor;
	CurrentMapId = PresentationMapId;
	FImmortalMapDefinition PresentationMap;
	if (!UImmortalMapLibrary::GetMapDefinition(CurrentMapId, PresentationMap))
	{
		CurrentMapId = UImmortalMapLibrary::GetQingyunMountainId();
	}
	bIsEndlessEnemy = true;
	bIsEndlessElite = bElite && !bBoss;
	bIsBoss = bBoss;
	bIsWorldBoss = false;
	WorldBossId = NAME_None;
	EndlessFloor = SafeFloor;
	EndlessRunId = RunId;
	CurrentBossPhase = bIsBoss ? 1 : 0;
	BossAttackCounter = 0;
	bBossSkillAttack = false;
	bForceWorldBossRangedSkill = false;

	Tags.Remove(TEXT("WorldBoss"));
	Tags.Remove(TEXT("WorldBossMinion"));
	Tags.Remove(TEXT("BossMinion"));
	Tags.AddUnique(TEXT("EndlessEnemy"));
	if (bIsEndlessElite)
	{
		Tags.AddUnique(TEXT("EndlessElite"));
	}
	if (bIsBoss)
	{
		Tags.AddUnique(TEXT("Boss"));
		Tags.AddUnique(TEXT("EndlessBoss"));
	}

	MaxHealth = StageBaseMaxHealth * HealthScale;
	AttackDamage = StageBaseAttackDamage * AttackScale;
	Defense = StageBaseDefense + DefenseGrowth;
	AttackSpeedMultiplier *= 1.0f + FMath::Min(
		static_cast<float>(FloorIndex) * 0.00004f, 0.40f);
	EquipmentItemLevel = 1 + (SafeFloor - 1) / 4;

	if (bIsEndlessElite)
	{
		MaxHealth *= 2.40f;
		AttackDamage *= 1.35f;
		Defense += 3.0f + static_cast<float>(SafeFloor) * 0.015f;
		AttackSpeedMultiplier *= 1.08f;
		MovementSpeed *= 1.04f;
		MonsterDisplayName = FText::FromString(FString::Printf(
			TEXT("第 %d 层 · 秘境精英"), SafeFloor));
	}
	else if (bIsBoss)
	{
		MaxHealth *= FMath::Max(BossHealthMultiplier, 1.0f);
		AttackDamage *= FMath::Max(BossAttackMultiplier, 1.0f);
		Defense += FMath::Max(BossDefenseBonus, 0.0f);
		AttackSpeedMultiplier *= 1.05f;
		MovementSpeed *= 0.90f;
		MonsterDisplayName = FText::FromString(FString::Printf(
			TEXT("第 %d 层 · 秘境守层者"), SafeFloor));
	}
	else
	{
		MonsterDisplayName = FText::FromString(FString::Printf(
			TEXT("第 %d 层 · 秘境妖兽"), SafeFloor));
	}
	CurrentHealth = FMath::Max(MaxHealth, 1.0f);
	GetCharacterMovement()->MaxWalkSpeed = FMath::Max(MovementSpeed, 0.0f);

	// The player-owned Endless floor transaction is the only reward writer.
	// Keeping every legacy field disabled protects against both normal and
	// Boss branches in Die accidentally producing an adventure-map reward.
	CultivationReward = 0;
	GoldReward = 0;
	EquipmentDropChance = 0.0f;
	SpiritStoneDropChance = 0.0f;
	MaterialDropChance = 0.0f;

	if (UPaperFlipbookComponent* SpriteComponent = GetSprite())
	{
		if (bIsBoss)
		{
			SpriteComponent->SetRelativeScale3D(
				SpriteComponent->GetRelativeScale3D()
					* FMath::Max(BossVisualScaleMultiplier, 1.0f));
			SpriteComponent->SetTranslucentSortPriority(VisualSortPriority + 3);
			SpriteComponent->SetSpriteColor(
				FLinearColor(0.92f, 0.16f, 0.42f, 1.0f));
		}
		else if (bIsEndlessElite)
		{
			SpriteComponent->SetRelativeScale3D(
				SpriteComponent->GetRelativeScale3D() * 1.12f);
			SpriteComponent->SetTranslucentSortPriority(VisualSortPriority + 1);
			SpriteComponent->SetSpriteColor(
				FLinearColor(0.88f, 0.56f, 1.0f, 1.0f));
		}
		else
		{
			SpriteComponent->SetSpriteColor(
				FLinearColor(0.62f, 0.82f, 1.0f, 1.0f));
		}
	}
	if (HealthBarComponent && bIsBoss)
	{
		HealthBarComponent->SetDrawSize(FVector2D(410.0f, 46.0f));
		HealthBarComponent->SetRelativeLocation(
			FVector(0.0f, VisualDepthOffset, HealthBarHeight + 38.0f));
	}
	else if (HealthBarComponent && bIsEndlessElite)
	{
		HealthBarComponent->SetDrawSize(FVector2D(300.0f, 36.0f));
		HealthBarComponent->SetRelativeLocation(
			FVector(0.0f, VisualDepthOffset, HealthBarHeight + 15.0f));
	}

	if (bIsBoss)
	{
		BP_OnBossPhaseChanged(CurrentBossPhase);
	}
	UE_LOG(LogTemp, Display,
		TEXT("Endless enemy configured: run=%s floor=%d boss=%s elite=%s map=%s health=%.1f attack=%.1f defense=%.1f itemLevel=%d genericDrops=0"),
		*EndlessRunId.ToString(EGuidFormats::DigitsWithHyphens),
		EndlessFloor,
		bIsBoss ? TEXT("true") : TEXT("false"),
		bIsEndlessElite ? TEXT("true") : TEXT("false"),
		*CurrentMapId.ToString(),
		MaxHealth,
		AttackDamage,
		Defense,
		EquipmentItemLevel);
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
		// Unlike the map Boss heavy swing, this is a genuine ranged cast: the
		// World Boss can stop and resolve it before entering normal melee range.
		if (bIsWorldBoss
			&& GetWorld()
			&& HorizontalDistance <= AttackRange + FMath::Max(BossSkillBonusRange, 0.0f)
			&& GetWorld()->GetTimeSeconds() >= NextWorldBossRangedSkillTime)
		{
			bForceWorldBossRangedSkill = true;
			StartAttack();
			return;
		}
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
	const bool bStartedAsWorldBossRangedSkill = bForceWorldBossRangedSkill;
	bBossSkillAttack = bStartedAsWorldBossRangedSkill;
	bForceWorldBossRangedSkill = false;
	if (bIsBoss)
	{
		++BossAttackCounter;
		bBossSkillAttack = bBossSkillAttack
			|| BossAttackCounter % FMath::Max(BossSkillEveryAttacks, 2) == 0;
		if (bBossSkillAttack)
		{
			BP_OnBossSkillStarted(Target);
			const float CastDistance = FMath::Abs(
				Target->GetActorLocation().X - GetActorLocation().X);
			if (bStartedAsWorldBossRangedSkill)
			{
				UE_LOG(LogTemp, Display,
					TEXT("World Boss %s started genuine ranged skill in phase %d at distance %.0f"),
					*MonsterDisplayName.ToString(), CurrentBossPhase, CastDistance);
			}
			else
			{
				UE_LOG(LogTemp, Display,
					TEXT("%s %s started heavy skill in phase %d at distance %.0f"),
					bIsWorldBoss ? TEXT("World Boss") : TEXT("Map boss"),
					*MonsterDisplayName.ToString(), CurrentBossPhase, CastDistance);
			}
		}
	}
	const float EffectiveAttackInterval = FMath::Max(AttackInterval, 0.05f) / FMath::Max(AttackSpeedMultiplier, 0.1f);
	NextAttackTime = GetWorld()->GetTimeSeconds() + EffectiveAttackInterval;
	if (bIsWorldBoss && bBossSkillAttack)
	{
		NextWorldBossRangedSkillTime = GetWorld()->GetTimeSeconds()
			+ FMath::Max(EffectiveAttackInterval * 2.25f, 2.0f);
	}
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
		AttackDamage *= bIsWorldBoss ? 1.25f : 1.18f;
		AttackSpeedMultiplier *= bIsWorldBoss ? 1.18f : 1.12f;
		MovementSpeed *= bIsWorldBoss ? 1.08f : 1.05f;
	}
	else
	{
		AttackDamage *= bIsWorldBoss ? 1.40f : 1.25f;
		AttackSpeedMultiplier *= bIsWorldBoss ? 1.35f : 1.18f;
		MovementSpeed *= bIsWorldBoss ? 1.12f : 1.08f;
		CriticalChance = FMath::Clamp(
			CriticalChance + (bIsWorldBoss ? 0.15f : 0.08f), 0.0f, 1.0f);
		if (bIsWorldBoss)
		{
			BossSkillEveryAttacks = FMath::Max(BossSkillEveryAttacks - 1, 2);
		}
	}
	GetCharacterMovement()->MaxWalkSpeed = FMath::Max(MovementSpeed, 0.0f);

	BP_OnBossPhaseChanged(CurrentBossPhase);
	OnBossPhaseChanged.Broadcast(this, CurrentBossPhase);
	UE_LOG(LogTemp, Display, TEXT("%s %s entered phase %d at %.1f%% health"),
		bIsWorldBoss ? TEXT("World Boss") : TEXT("Map boss"),
		*MonsterDisplayName.ToString(), CurrentBossPhase, GetHealthPercent() * 100.0f);
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

	const AImmortalPlayerCharacter* RewardPlayer = Cast<AImmortalPlayerCharacter>(DamageCauser);
	const float EffectiveEquipmentDropChance = FMath::Clamp(
		EquipmentDropChance * (RewardPlayer ? RewardPlayer->GetEquipmentDropChanceMultiplier() : 1.0f),
		0.0f,
		1.0f);
	const float BlueprintDropChance = bIsWorldBoss || bIsEndlessEnemy
		? 0.0f
		: (bIsBoss ? 1.0f : EffectiveEquipmentDropChance);
	BP_OnMonsterRewardsGranted(0, 0, BlueprintDropChance, DamageCauser);
	BP_OnMonsterDied(DamageCauser);

	if (bIsWorldBoss || bIsEndlessEnemy)
	{
		if (bIsWorldBoss)
		{
			UE_LOG(LogTemp, Display,
				TEXT("World Boss %s skipped generic map rewards; player transaction owns the independent drop pool"),
				*WorldBossId.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Display,
				TEXT("Endless enemy skipped generic map rewards: run=%s floor=%d boss=%s elite=%s"),
				*EndlessRunId.ToString(EGuidFormats::DigitsWithHyphens),
				EndlessFloor,
				bIsBoss ? TEXT("true") : TEXT("false"),
				bIsEndlessElite ? TEXT("true") : TEXT("false"));
		}
		if (bDestroyAfterDeath)
		{
			SetLifeSpan(FMath::Max(
				DeathLifeSpan, GetAnimationDuration(DeathFlipbook, DeathLifeSpan)));
		}
		return;
	}

	const int32 EquipmentDropCount = bIsBoss
		? FMath::Max(BossGuaranteedEquipmentDrops, 1)
		: (FMath::FRand() < EffectiveEquipmentDropChance ? 1 : 0);
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
					MinimumBossEquipmentDropQuality);
			}
			else if (MinimumEquipmentDropQuality != EImmortalEquipmentQuality::Common)
			{
				SpawnedDrop->GenerateEquipmentForLevelWithMinimumQuality(
					FMath::Max(EquipmentItemLevel, 1), MinimumEquipmentDropQuality);
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
		const FImmortalMaterialStack Material = UImmortalMaterialLibrary::GenerateMapDrop(
			CurrentMapId, CurrentConfiguredStage, bIsBoss, DropIndex);
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
		UE_LOG(LogTemp, Display, TEXT("Map boss rewards spawned: %s | %d equipment, spirit stones and %d material entities"),
			*CurrentMapId.ToString(), EquipmentDropCount, MaterialDropCount);
	}

	if (bDestroyAfterDeath)
	{
		SetLifeSpan(FMath::Max(DeathLifeSpan, GetAnimationDuration(DeathFlipbook, DeathLifeSpan)));
	}
}
