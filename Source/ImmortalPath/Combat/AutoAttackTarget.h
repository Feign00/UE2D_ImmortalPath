// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AutoAttackTarget.generated.h"

/**
 * Implement this interface on actors that can be selected by the player's
 * automatic attack. Future monster classes will implement it in C++.
 */
UINTERFACE(BlueprintType)
class IMMORTALPATH_API UAutoAttackTarget : public UInterface
{
	GENERATED_BODY()
};

class IMMORTALPATH_API IAutoAttackTarget
{
	GENERATED_BODY()

public:
	/** Whether this actor is currently a valid living target. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Immortal Path|Combat")
	bool CanBeAutoAttacked() const;

	/** Point used for range checks. Defaults to the actor location. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Immortal Path|Combat")
	FVector GetAutoAttackTargetLocation() const;
};
