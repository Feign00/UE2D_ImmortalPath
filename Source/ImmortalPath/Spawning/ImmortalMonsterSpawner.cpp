// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalMonsterSpawner.h"

#include "../Characters/ImmortalMonsterCharacter.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Save/ImmortalPathSaveGame.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
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
		DefaultBossMonsterClass = DogMonsterFinder.Class;
	}
}

void AImmortalMonsterSpawner::BeginPlay()
{
	Super::BeginPlay();
	LoadStageProgress();

#if !UE_BUILD_SHIPPING
	int32 TestStage = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestStage="), TestStage))
	{
		CurrentStage = FMath::Clamp(TestStage, 1, FMath::Clamp(MaxStage, 1, 999));
		CurrentStageKills = 0;
		bQingyunMountainCompleted = false;
		UE_LOG(LogTemp, Display, TEXT("Qingyun Mountain development stage override applied: %d"), CurrentStage);
	}
#endif

	if (bStartOnBeginPlay)
	{
		SpawnUntilInitialCount();
		StartSpawning();
	}
	UpdateStageHud();
}

void AImmortalMonsterSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SaveStageProgress();
	StopSpawning();
	SpawnedMonsters.Reset();
	Super::EndPlay(EndPlayReason);
}

AImmortalMonsterCharacter* AImmortalMonsterSpawner::SpawnMonster()
{
	RemoveInvalidMonsters();
	if (bQingyunMountainCompleted)
	{
		return nullptr;
	}

	if (IsCurrentStageBossStage())
	{
		for (const TWeakObjectPtr<AImmortalMonsterCharacter>& Existing : SpawnedMonsters)
		{
			if (Existing.IsValid() && Existing->IsBoss() && !Existing->IsDead())
			{
				return nullptr;
			}
		}
		return SpawnConfiguredMonster(true, false);
	}

	return SpawnConfiguredMonster(false, false);
}

void AImmortalMonsterSpawner::HandleMonsterDeath(AImmortalMonsterCharacter* Monster, AActor* DamageCauser)
{
	if (!Monster || bQingyunMountainCompleted)
	{
		return;
	}

	if (IsCurrentStageBossStage())
	{
		if (!Monster->IsBoss())
		{
			UE_LOG(LogTemp, Display, TEXT("Qingyun Mountain boss minion defeated; boss gate remains active at stage %d"), CurrentStage);
			return;
		}

		const int32 ClearedStage = CurrentStage;
		AdvanceStage(Monster);
		BP_OnBossDefeated(ClearedStage, CurrentStage, bQingyunMountainCompleted);
		ShowBossMessage(
			bQingyunMountainCompleted
				? FText::FromString(TEXT("青云山最终妖王已击败！地图通关"))
				: FText::FromString(FString::Printf(TEXT("守关妖王已击败！进入第 %d 关"), CurrentStage)),
			FLinearColor(1.0f, 0.78f, 0.18f, 1.0f));
	}
	else
	{
		++CurrentStageKills;
		const int32 RequiredKills = GetRequiredKillsForCurrentStage();
		UE_LOG(LogTemp, Display, TEXT("Qingyun Mountain stage %d progress: %d/%d"), CurrentStage, CurrentStageKills, RequiredKills);
		if (CurrentStageKills >= RequiredKills)
		{
			AdvanceStage(Monster);
		}
	}

	UpdateStageHud();
	SaveStageProgress();
}

bool AImmortalMonsterSpawner::SaveStageProgress()
{
	UImmortalPathSaveGame* SaveGame = UImmortalPathSaveGame::LoadOrCreate(this);
	if (!SaveGame)
	{
		return false;
	}

	SaveGame->bHasStageData = true;
	SaveGame->QingyunStage = FMath::Clamp(CurrentStage, 1, FMath::Clamp(MaxStage, 1, 999));
	SaveGame->QingyunStageKills = FMath::Clamp(CurrentStageKills, 0, GetRequiredKillsForCurrentStage());
	SaveGame->bQingyunMountainCompleted = bQingyunMountainCompleted;
	const bool bSaved = SaveGame->SaveToDisk();
	if (bSaved)
	{
		UE_LOG(LogTemp, Display, TEXT("Qingyun Mountain progress saved: stage %d | progress %d/%d | boss=%s | completed=%s"),
			CurrentStage, CurrentStageKills, GetRequiredKillsForCurrentStage(),
			IsCurrentStageBossStage() ? TEXT("true") : TEXT("false"),
			bQingyunMountainCompleted ? TEXT("true") : TEXT("false"));
	}
	return bSaved;
}

bool AImmortalMonsterSpawner::LoadStageProgress()
{
	UImmortalPathSaveGame* SaveGame = UImmortalPathSaveGame::LoadOrCreate(this);
	if (!SaveGame || !SaveGame->bHasStageData)
	{
		CurrentStage = FMath::Clamp(CurrentStage, 1, FMath::Clamp(MaxStage, 1, 999));
		CurrentStageKills = 0;
		UE_LOG(LogTemp, Display, TEXT("No saved Qingyun Mountain progress found; starting at stage %d"), CurrentStage);
		return false;
	}

	CurrentStage = FMath::Clamp(SaveGame->QingyunStage, 1, FMath::Clamp(MaxStage, 1, 999));
	bQingyunMountainCompleted = SaveGame->bQingyunMountainCompleted
		&& CurrentStage >= FMath::Clamp(MaxStage, 1, 999);
	const int32 RequiredKills = GetRequiredKillsForCurrentStage();
	CurrentStageKills = bQingyunMountainCompleted
		? RequiredKills
		: FMath::Clamp(SaveGame->QingyunStageKills, 0, RequiredKills - 1);
	UE_LOG(LogTemp, Display, TEXT("Qingyun Mountain progress loaded: stage %d | progress %d/%d | boss=%s | completed=%s"),
		CurrentStage, CurrentStageKills, RequiredKills,
		IsCurrentStageBossStage() ? TEXT("true") : TEXT("false"),
		bQingyunMountainCompleted ? TEXT("true") : TEXT("false"));
	return true;
}

void AImmortalMonsterSpawner::UpdateStageHud() const
{
	if (AImmortalPlayerCharacter* Player = Cast<AImmortalPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->UpdateStageProgress(
			CurrentStage,
			CurrentStageKills,
			GetRequiredKillsForCurrentStage(),
			IsCurrentStageBossStage(),
			bQingyunMountainCompleted);
	}
}

bool AImmortalMonsterSpawner::IsCurrentStageBossStage() const
{
	if (bQingyunMountainCompleted)
	{
		return false;
	}
	const int32 SafeStage = FMath::Clamp(CurrentStage, 1, FMath::Clamp(MaxStage, 1, 999));
	return SafeStage >= FMath::Clamp(MaxStage, 1, 999)
		|| SafeStage % FMath::Max(BossStageInterval, 2) == 0;
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
	if (!GetWorld() || bQingyunMountainCompleted)
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
	if (!bQingyunMountainCompleted)
	{
		SpawnMonster();
	}
}

void AImmortalMonsterSpawner::StopSpawning()
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
}

void AImmortalMonsterSpawner::SpawnUntilInitialCount()
{
	const int32 DesiredCount = IsCurrentStageBossStage()
		? 1
		: FMath::Min(FMath::Max(InitialSpawnCount, 0), FMath::Max(MaxAliveMonsters, 1));
	for (int32 Index = GetAliveMonsterCount(); Index < DesiredCount; ++Index)
	{
		if (!SpawnMonster())
		{
			break;
		}
	}
}

AImmortalMonsterCharacter* AImmortalMonsterSpawner::SpawnConfiguredMonster(
	const bool bSpawnBoss,
	const bool bIgnoreAliveLimit,
	AActor* SpawnOwner)
{
	RemoveInvalidMonsters();
	if (!GetWorld() || !MonsterClass || bQingyunMountainCompleted)
	{
		return nullptr;
	}
	if (!bIgnoreAliveLimit && GetAliveMonsterCount() >= (bSpawnBoss ? 1 : FMath::Max(MaxAliveMonsters, 1)))
	{
		return nullptr;
	}

	const TSubclassOf<AImmortalMonsterCharacter> EffectiveAlternateClass =
		AlternateMonsterClass ? AlternateMonsterClass : DefaultAlternateMonsterClass;
	const TSubclassOf<AImmortalMonsterCharacter> EffectiveBossClass = BossMonsterClass
		? BossMonsterClass
		: (DefaultBossMonsterClass ? DefaultBossMonsterClass : MonsterClass);
	const TSubclassOf<AImmortalMonsterCharacter> ClassToSpawn = bSpawnBoss
		? EffectiveBossClass
		: (EffectiveAlternateClass && FMath::RandBool() ? EffectiveAlternateClass : MonsterClass);

	FVector SpawnLocation;
	if (!FindSpawnLocation(SpawnLocation))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = SpawnOwner ? SpawnOwner : this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AImmortalMonsterCharacter* Monster = GetWorld()->SpawnActor<AImmortalMonsterCharacter>(
		ClassToSpawn, SpawnLocation, GetActorRotation(), SpawnParams);
	if (!Monster)
	{
		return nullptr;
	}

	SpawnedMonsters.Add(Monster);
	Monster->OnMonsterDeath.AddDynamic(this, &AImmortalMonsterSpawner::HandleMonsterDeath);
	Monster->OnDestroyed.AddDynamic(this, &AImmortalMonsterSpawner::HandleSpawnedMonsterDestroyed);
	if (bSpawnBoss)
	{
		Monster->OnBossPhaseChanged.AddDynamic(this, &AImmortalMonsterSpawner::HandleBossPhaseChanged);
		Monster->ConfigureAsBoss(CurrentStage);
		BP_OnBossSpawned(Monster, CurrentStage);
		ShowBossMessage(
			FText::FromString(FString::Printf(TEXT("第 %d 关：守关妖王出现"), CurrentStage)),
			FLinearColor(1.0f, 0.28f, 0.12f, 1.0f));
		UE_LOG(LogTemp, Display, TEXT("Qingyun Mountain boss spawned at stage %d"), CurrentStage);
	}
	else
	{
		Monster->ConfigureForStage(CurrentStage);
	}
	BP_OnMonsterSpawned(Monster);
	return Monster;
}

void AImmortalMonsterSpawner::SpawnBossMinions(AImmortalMonsterCharacter* Boss, const int32 Count)
{
	for (int32 Index = 0; Index < FMath::Max(Count, 0); ++Index)
	{
		if (AImmortalMonsterCharacter* Minion = SpawnConfiguredMonster(false, true, Boss))
		{
			Minion->Tags.AddUnique(TEXT("BossMinion"));
		}
	}
	UE_LOG(LogTemp, Display, TEXT("Qingyun Mountain boss summoned %d minions"), FMath::Max(Count, 0));
}

void AImmortalMonsterSpawner::HandleBossPhaseChanged(AImmortalMonsterCharacter* Boss, const int32 NewPhase)
{
	if (!Boss || Boss->IsDead() || !Boss->IsBoss())
	{
		return;
	}
	const int32 SummonCount = NewPhase == 2 ? PhaseTwoSummonCount : (NewPhase == 3 ? PhaseThreeSummonCount : 0);
	SpawnBossMinions(Boss, SummonCount);
	const FString PhaseMessage = NewPhase >= 3
		? FString::Printf(TEXT("妖王进入狂暴阶段！召唤 %d 只妖兽"), SummonCount)
		: FString::Printf(TEXT("妖王进入第 %d 阶段，召唤 %d 只妖兽"), NewPhase, SummonCount);
	ShowBossMessage(
		FText::FromString(PhaseMessage),
		NewPhase >= 3 ? FLinearColor(1.0f, 0.12f, 0.08f, 1.0f) : FLinearColor(1.0f, 0.55f, 0.15f, 1.0f));
}

void AImmortalMonsterSpawner::AdvanceStage(AImmortalMonsterCharacter* DefeatedMonster)
{
	const int32 SafeMaxStage = FMath::Clamp(MaxStage, 1, 999);
	ClearOtherMonsters(DefeatedMonster);
	if (CurrentStage >= SafeMaxStage)
	{
		CurrentStage = SafeMaxStage;
		CurrentStageKills = 1;
		bQingyunMountainCompleted = true;
		StopSpawning();
		UE_LOG(LogTemp, Display, TEXT("Qingyun Mountain completed after defeating the final boss at stage %d"), CurrentStage);
		return;
	}

	++CurrentStage;
	CurrentStageKills = 0;
	UE_LOG(LogTemp, Display, TEXT("Qingyun Mountain advanced to stage %d%s"),
		CurrentStage, IsCurrentStageBossStage() ? TEXT(" (boss stage)") : TEXT(""));
	if (IsCurrentStageBossStage())
	{
		ShowBossMessage(
			FText::FromString(FString::Printf(TEXT("第 %d 关为守关 Boss"), CurrentStage)),
			FLinearColor(1.0f, 0.62f, 0.12f, 1.0f));
	}
}

void AImmortalMonsterSpawner::ClearOtherMonsters(AImmortalMonsterCharacter* Exception)
{
	const TArray<TWeakObjectPtr<AImmortalMonsterCharacter>> MonstersToClear = SpawnedMonsters;
	for (const TWeakObjectPtr<AImmortalMonsterCharacter>& Monster : MonstersToClear)
	{
		if (Monster.IsValid() && Monster.Get() != Exception)
		{
			Monster->Destroy();
		}
	}
	RemoveInvalidMonsters();
}

int32 AImmortalMonsterSpawner::GetRequiredKillsForCurrentStage() const
{
	return IsCurrentStageBossStage() || bQingyunMountainCompleted ? 1 : FMath::Max(KillsPerStage, 1);
}

void AImmortalMonsterSpawner::ShowBossMessage(const FText& Message, const FLinearColor& Color) const
{
	if (AImmortalPlayerCharacter* Player = Cast<AImmortalPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->ShowBossMessage(Message, Color);
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
