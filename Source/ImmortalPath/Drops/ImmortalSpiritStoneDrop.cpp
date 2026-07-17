// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalSpiritStoneDrop.h"

#include "../Characters/ImmortalPlayerCharacter.h"
#include "../UI/ImmortalSpiritStoneWidget.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"

AImmortalSpiritStoneDrop::AImmortalSpiritStoneDrop()
{
	PrimaryActorTick.bCanEverTick = true;
	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("SpiritStonePickup"));
	SetRootComponent(PickupSphere);
	PickupSphere->InitSphereRadius(35.0f);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Visual = CreateDefaultSubobject<UWidgetComponent>(TEXT("SpiritStoneVisual"));
	Visual->SetupAttachment(PickupSphere);
	Visual->SetWidgetSpace(EWidgetSpace::Screen);
	Visual->SetDrawSize(FVector2D(80.0f, 80.0f));
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetWidgetClass(UImmortalSpiritStoneWidget::StaticClass());
	InitialLifeSpan = 8.0f;
}

void AImmortalSpiritStoneDrop::BeginPlay()
{
	Super::BeginPlay();
	SpawnTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	BaseHeight = GetActorLocation().Z;
	TargetPlayer = Cast<AImmortalPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

void AImmortalSpiritStoneDrop::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bCollected || !GetWorld()) return;
	if (!TargetPlayer.IsValid()) TargetPlayer = Cast<AImmortalPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));

	const float AliveTime = GetWorld()->GetTimeSeconds() - SpawnTime;
	AImmortalPlayerCharacter* Player = TargetPlayer.Get();
	if (Player && AliveTime >= 0.35f)
	{
		const FVector Target = Player->GetActorLocation() + FVector(0.0f, 0.0f, 55.0f);
		const FVector NewLocation = FMath::VInterpConstantTo(GetActorLocation(), Target, DeltaSeconds, 700.0f);
		SetActorLocation(NewLocation);
		if (FVector::DistSquared(NewLocation, Target) <= FMath::Square(65.0f)) Collect(Player);
		return;
	}

	FVector Location = GetActorLocation();
	Location.Z = BaseHeight + FMath::Sin(AliveTime * 6.0f) * 9.0f;
	SetActorLocation(Location);
}

void AImmortalSpiritStoneDrop::Collect(AImmortalPlayerCharacter* Player)
{
	if (bCollected || !Player) return;
	bCollected = true;
	Player->ReceiveSpiritStones(Amount, GetActorLocation());
	Destroy();
}
