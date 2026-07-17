// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalSpiritStoneWidget.generated.h"

/** Temporary native visual for a spirit-stone drop; can later be replaced by supplied art. */
UCLASS()
class IMMORTALPATH_API UImmortalSpiritStoneWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
};
