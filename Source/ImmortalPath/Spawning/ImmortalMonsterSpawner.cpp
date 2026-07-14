// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalMonsterSpawner.h"

#include "../Characters/ImmortalMonsterCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AImmortalMonsterSpawner::AImmortalMonsterSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpawnArea = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnArea"));
	SpawnArea->SetupAttachment(SceneRoot);
	SpawnArea->SetBoxExtent(FVector(800.0f, 20.0f, 20.0f));
	SpawnArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnArea->SetHiddenInGame(true);

	static ConstructorHelpers::FClassFinder<AImmortalMonsterCharacter> DogMonsterFinder(
		TEXT("/Game/GAME/BP_DogMonster"));
	if (DogMonsterFinder.Succeeded())
	{
		DefaultAlternateMonsterClass = DogMonsterFinder.Class;
	}
}

void AImmortalMonsterSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bStartOnBeginPlay)
	{
		SpawnUntilInitialCount();
		StartSpawning();
	}
}

void AImmortalMonsterSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSpawning();
	SpawnedMonsters.Reset();
	Super::EndPlay(EndPlayReason);
}

AImmortalMonsterCharacter* AImmortalMonsterSpawner::SpawnMonster()
{
	RemoveInvalidMonsters();

	if (!GetWorld() || !MonsterClass || SpawnedMonsters.Num() >= FMath::Max(MaxAliveMonsters, 1))
	{
		return nullptr;
	}

	const TSubclassOf<AImmortalMonsterCharacter> EffectiveAlternateClass =
		AlternateMonsterClass ? AlternateMonsterClass : DefaultAlternateMonsterClass;
	const TSubclassOf<AImmortalMonsterCharacter> ClassToSpawn =
		EffectiveAlternateClass && FMath::RandBool() ? EffectiveAlternateClass : MonsterClass;

	FVector SpawnLocation;
	if (!FindSpawnLocation(SpawnLocation))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AImmortalMonsterCharacter* Monster = GetWorld()->SpawnActor<AImmortalMonsterCharacter>(
		ClassToSpawn,
		SpawnLocation,
		GetActorRotation(),
		SpawnParams);

	if (Monster)
	{
		SpawnedMonsters.Add(Monster);
		Monster->OnDestroyed.AddDynamic(this, &AImmortalMonsterSpawner::HandleSpawnedMonsterDestroyed);
		BP_OnMonsterSpawned(Monster);
	}

	return Monster;
}

int32 AImmortalMonsterSpawner::GetAliveMonsterCount() const
{
	int32 AliveCount = 0;
	for (const TWeakObjectPtr<AImmortalMonsterCharacter>& Monster : SpawnedMonsters)
	{
		if (Monster.IsValid() && !Monster->IsDead())
		{
			++AliveCount;
		}
	}
	return AliveCount;
}

void AImmortalMonsterSpawner::StartSpawning()
{
	if (!GetWorld())
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&AImmortalMonsterSpawner::HandleSpawnTimer,
		FMath::Max(SpawnInterval, 0.1f),
		true);
}

void AImmortalMonsterSpawner::HandleSpawnTimer()
{
	SpawnMonster();
}

void AImmortalMonsterSpawner::StopSpawning()
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
}

void AImmortalMonsterSpawner::SpawnUntilInitialCount()
{
	const int32 DesiredCount = FMath::Min(FMath::Max(InitialSpawnCount, 0), FMath::Max(MaxAliveMonsters, 1));
	for (int32 Index = GetAliveMonsterCount(); Index < DesiredCount; ++Index)
	{
		if (!SpawnMonster())
		{
			break;
		}
	}
}

bool AImmortalMonsterSpawner::FindSpawnLocation(FVector& OutLocation) const
{
	if (!SpawnArea)
	{
		return false;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector Extent = SpawnArea->GetUnscaledBoxExtent();
	const FTransform AreaTransform = SpawnArea->GetComponentTransform();
	constexpr int32 MaxAttempts = 12;

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		FVector LocalLocation(
			FMath::FRandRange(-Extent.X, Extent.X),
			bConstrainTo2DPlane ? 0.0f : FMath::FRandRange(-Extent.Y, Extent.Y),
			FMath::FRandRange(-Extent.Z, Extent.Z));

		FVector Candidate = AreaTransform.TransformPosition(LocalLocation);

		if (PlayerPawn && bConstrainTo2DPlane)
		{
			Candidate.Y = PlayerPawn->GetActorLocation().Y;
		}

		if (PlayerPawn && bMatchPlayerHeight)
		{
			Candidate.Z = PlayerPawn->GetActorLocation().Z + SpawnHeightOffset;
		}
		else
		{
			Candidate.Z += SpawnHeightOffset;
		}

		if (!PlayerPawn || FVector::DistSquared(PlayerPawn->GetActorLocation(), Candidate) >= FMath::Square(MinDistanceFromPlayer))
		{
			OutLocation = Candidate;
			return true;
		}
	}

	return false;
}

void AImmortalMonsterSpawner::RemoveInvalidMonsters()
{
	SpawnedMonsters.RemoveAll([](const TWeakObjectPtr<AImmortalMonsterCharacter>& Monster)
	{
		return !Monster.IsValid();
	});
}

void AImmortalMonsterSpawner::HandleSpawnedMonsterDestroyed(AActor* DestroyedActor)
{
	SpawnedMonsters.RemoveAll([DestroyedActor](const TWeakObjectPtr<AImmortalMonsterCharacter>& Monster)
	{
		return !Monster.IsValid() || Monster.Get() == DestroyedActor;
	});
}
