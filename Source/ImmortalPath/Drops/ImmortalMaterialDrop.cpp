// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalMaterialDrop.h"

#include "../Characters/ImmortalPlayerCharacter.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "../UI/ImmortalMaterialDropWidget.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"

AImmortalMaterialDrop::AImmortalMaterialDrop()
{
	PrimaryActorTick.bCanEverTick = true;
	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("MaterialPickup"));
	SetRootComponent(PickupSphere);
	PickupSphere->InitSphereRadius(38.0f);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Visual = CreateDefaultSubobject<UWidgetComponent>(TEXT("MaterialVisual"));
	Visual->SetupAttachment(PickupSphere);
	Visual->SetWidgetSpace(EWidgetSpace::Screen);
	Visual->SetDrawSize(FVector2D(112.0f, 88.0f));
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetWidgetClass(UImmortalMaterialDropWidget::StaticClass());
	InitialLifeSpan = 8.0f;
}

void AImmortalMaterialDrop::BeginPlay()
{
	Super::BeginPlay();
	SpawnTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	BaseHeight = GetActorLocation().Z;
	TargetPlayer = Cast<AImmortalPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	RefreshVisual();
}

void AImmortalMaterialDrop::SetMaterialDrop(const FName InMaterialId, const int32 InQuantity)
{
	FImmortalMaterialDefinition Definition;
	if (UImmortalMaterialLibrary::GetMaterialDefinition(InMaterialId, Definition))
	{
		MaterialId = InMaterialId;
		Quantity = FMath::Clamp(InQuantity, 1, FMath::Max(Definition.MaximumStack, 1));
		RefreshVisual();
	}
}

void AImmortalMaterialDrop::RefreshVisual()
{
	FImmortalMaterialDefinition Definition;
	if (Visual && UImmortalMaterialLibrary::GetMaterialDefinition(MaterialId, Definition))
	{
		if (UImmortalMaterialDropWidget* Widget = Cast<UImmortalMaterialDropWidget>(Visual->GetUserWidgetObject()))
		{
			Widget->SetMaterial(Definition, Quantity);
		}
	}
}

void AImmortalMaterialDrop::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bCollected || !GetWorld()) return;
	if (!TargetPlayer.IsValid()) TargetPlayer = Cast<AImmortalPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));

	const float AliveTime = GetWorld()->GetTimeSeconds() - SpawnTime;
	AImmortalPlayerCharacter* Player = TargetPlayer.Get();
	if (Player && AliveTime >= 0.45f)
	{
		const FVector Target = Player->GetActorLocation() + FVector(0.0f, 0.0f, 65.0f);
		const FVector NewLocation = FMath::VInterpConstantTo(GetActorLocation(), Target, DeltaSeconds, 620.0f);
		SetActorLocation(NewLocation);
		if (FVector::DistSquared(NewLocation, Target) <= FMath::Square(70.0f)) Collect(Player);
		return;
	}

	FVector Location = GetActorLocation();
	Location.Z = BaseHeight + FMath::Sin(AliveTime * 5.5f) * 10.0f;
	SetActorLocation(Location);
}

void AImmortalMaterialDrop::Collect(AImmortalPlayerCharacter* Player)
{
	if (bCollected || !Player) return;
	bCollected = true;
	Player->ReceiveMaterial(MaterialId, Quantity, GetActorLocation());
	Destroy();
}

