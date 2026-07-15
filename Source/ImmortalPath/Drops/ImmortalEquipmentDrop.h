// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "GameFramework/Actor.h"
#include "ImmortalEquipmentDrop.generated.h"

class AImmortalPlayerCharacter;
class USphereComponent;
class UWidgetComponent;

/** A visible equipment orb that homes into the idle player and is collected automatically. */
UCLASS(Blueprintable)
class IMMORTALPATH_API AImmortalEquipmentDrop : public AActor
{
	GENERATED_BODY()

public:
	AImmortalEquipmentDrop();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Drop")
	void GenerateEquipmentForLevel(int32 ItemLevel);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Drop")
	FImmortalEquipmentItem GetEquipmentItem() const { return EquipmentItem; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Immortal Path|Drop")
	TObjectPtr<USphereComponent> PickupSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Immortal Path|Drop")
	TObjectPtr<UWidgetComponent> OrbVisual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Drop", meta = (ClampMin = "0.0", Units = "s"))
	float PickupDelay = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Drop", meta = (ClampMin = "1.0", Units = "cm/s"))
	float HomingSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Drop", meta = (ClampMin = "1.0", Units = "cm"))
	float AutoCollectDistance = 70.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Drop")
	FImmortalEquipmentItem EquipmentItem;

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Drop", meta = (DisplayName = "On Equipment Collected"))
	void BP_OnEquipmentCollected(AImmortalPlayerCharacter* Player);

private:
	UFUNCTION()
	void HandlePickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void Collect(AImmortalPlayerCharacter* Player);
	void RefreshQualityVisual();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> TargetPlayer;

	float SpawnTime = 0.0f;
	float BaseHeight = 0.0f;
	bool bCollected = false;
};
