// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalMonsterHealthWidget.generated.h"

class AImmortalMonsterCharacter;
class UProgressBar;

/** Screen-space health bar attached to every monster. */
UCLASS()
class IMMORTALPATH_API UImmortalMonsterHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForMonster(AImmortalMonsterCharacter* InMonster);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalMonsterCharacter> Monster;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthProgress;
};
