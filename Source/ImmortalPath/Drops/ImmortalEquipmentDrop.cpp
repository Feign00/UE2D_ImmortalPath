// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalEquipmentDrop.h"

#include "../Characters/ImmortalPlayerCharacter.h"
#include "../UI/ImmortalEquipmentOrbWidget.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"

AImmortalEquipmentDrop::AImmortalEquipmentDrop()
{
	PrimaryActorTick.bCanEverTick = true;

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	SetRootComponent(PickupSphere);
	PickupSphere->InitSphereRadius(45.0f);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &AImmortalEquipmentDrop::HandlePickupOverlap);

	OrbVisual = CreateDefaultSubobject<UWidgetComponent>(TEXT("EquipmentOrbVisual"));
	OrbVisual->SetupAttachment(PickupSphere);
	OrbVisual->SetWidgetSpace(EWidgetSpace::Screen);
	OrbVisual->SetDrawSize(FVector2D(128.0f, 128.0f));
	OrbVisual->SetPivot(FVector2D(0.5f, 0.5f));
	OrbVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OrbVisual->SetWidgetClass(UImmortalEquipmentOrbWidget::StaticClass());

	InitialLifeSpan = 8.0f;
}

void AImmortalEquipmentDrop::BeginPlay()
{
	Super::BeginPlay();
	SpawnTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	BaseHeight = GetActorLocation().Z;
	TargetPlayer = Cast<AImmortalPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!EquipmentItem.IsValid())
	{
		GenerateEquipmentForLevel(1);
	}
	RefreshQualityVisual();
}

void AImmortalEquipmentDrop::GenerateEquipmentForLevel(const int32 ItemLevel)
{
	EquipmentItem = UImmortalEquipmentLibrary::GenerateRandomEquipment(FMath::Max(ItemLevel, 1));
	RefreshQualityVisual();
}

void AImmortalEquipmentDrop::RefreshQualityVisual()
{
	if (OrbVisual)
	{
		if (UImmortalEquipmentOrbWidget* OrbWidget = Cast<UImmortalEquipmentOrbWidget>(OrbVisual->GetUserWidgetObject()))
		{
			OrbWidget->SetQuality(EquipmentItem.Quality);
		}
	}
}

void AImmortalEquipmentDrop::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bCollected || !GetWorld())
	{
		return;
	}

	if (!TargetPlayer.IsValid())
	{
		TargetPlayer = Cast<AImmortalPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	}

	const float AliveTime = GetWorld()->GetTimeSeconds() - SpawnTime;
	AImmortalPlayerCharacter* Player = TargetPlayer.Get();
	if (Player && AliveTime >= PickupDelay)
	{
		const FVector TargetLocation = Player->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
		const FVector NewLocation = FMath::VInterpConstantTo(GetActorLocation(), TargetLocation, DeltaSeconds, HomingSpeed);
		SetActorLocation(NewLocation);
		if (FVector::DistSquared(NewLocation, TargetLocation) <= FMath::Square(AutoCollectDistance))
		{
			Collect(Player);
		}
		return;
	}

	FVector FloatingLocation = GetActorLocation();
	FloatingLocation.Z = BaseHeight + FMath::Sin(AliveTime * 5.0f) * 10.0f;
	SetActorLocation(FloatingLocation);
}

void AImmortalEquipmentDrop::HandlePickupOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (GetWorld() && GetWorld()->GetTimeSeconds() - SpawnTime >= PickupDelay)
	{
		Collect(Cast<AImmortalPlayerCharacter>(OtherActor));
	}
}

void AImmortalEquipmentDrop::Collect(AImmortalPlayerCharacter* Player)
{
	if (bCollected || !Player)
	{
		return;
	}

	bCollected = true;
	if (!EquipmentItem.IsValid())
	{
		GenerateEquipmentForLevel(1);
	}
	Player->ReceiveEquipmentItem(EquipmentItem);
	BP_OnEquipmentCollected(Player);
	Destroy();
}
