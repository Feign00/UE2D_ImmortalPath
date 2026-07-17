// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImmortalSpiritStoneDrop.generated.h"

class AImmortalPlayerCharacter;
class USphereComponent;
class UWidgetComponent;

/** Visible spirit-stone reward that floats briefly and then flies into the player. */
UCLASS(Blueprintable)
class IMMORTALPATH_API AImmortalSpiritStoneDrop : public AActor
{
	GENERATED_BODY()

public:
	AImmortalSpiritStoneDrop();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void SetAmount(int32 InAmount) { Amount = FMath::Max(InAmount, 1); }

private:
	void Collect(AImmortalPlayerCharacter* Player);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> PickupSphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> Visual;

	UPROPERTY(VisibleInstanceOnly)
	int32 Amount = 1;

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> TargetPlayer;

	float SpawnTime = 0.0f;
	float BaseHeight = 0.0f;
	bool bCollected = false;
};
