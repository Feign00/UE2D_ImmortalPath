// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImmortalMaterialDrop.generated.h"

class AImmortalPlayerCharacter;
class USphereComponent;
class UWidgetComponent;

/** A named, stackable material that floats at the kill location then flies into the player. */
UCLASS(Blueprintable)
class IMMORTALPATH_API AImmortalMaterialDrop : public AActor
{
	GENERATED_BODY()

public:
	AImmortalMaterialDrop();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Materials")
	void SetMaterialDrop(FName InMaterialId, int32 InQuantity);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Materials")
	FName GetMaterialId() const { return MaterialId; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Materials")
	int32 GetQuantity() const { return Quantity; }

private:
	void RefreshVisual();
	void Collect(AImmortalPlayerCharacter* Player);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> PickupSphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> Visual;

	UPROPERTY(VisibleInstanceOnly)
	FName MaterialId = TEXT("SpiritGrass");

	UPROPERTY(VisibleInstanceOnly)
	int32 Quantity = 1;

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> TargetPlayer;

	float SpawnTime = 0.0f;
	float BaseHeight = 0.0f;
	bool bCollected = false;
};

