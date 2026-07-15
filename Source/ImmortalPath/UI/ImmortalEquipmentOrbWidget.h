// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalEquipmentOrbWidget.generated.h"

/** Visual for an equipment drop actor. */
UCLASS()
class IMMORTALPATH_API UImmortalEquipmentOrbWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetQuality(EImmortalEquipmentQuality Quality);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<class UImage> OrbImage;
};
