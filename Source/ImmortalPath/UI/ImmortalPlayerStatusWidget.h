// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalPlayerStatusWidget.generated.h"

class AImmortalPlayerCharacter;
class UProgressBar;

/**
 * Compact desktop HUD: health and five graphical management shortcuts.
 */
UCLASS()
class IMMORTALPATH_API UImmortalPlayerStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleOpenManagementClicked();
	UFUNCTION()
	void HandleInventoryClicked();
	UFUNCTION()
	void HandleCultivationClicked();
	UFUNCTION()
	void HandleShopClicked();
	UFUNCTION()
	void HandleSettingsClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthProgress;
	FIntPoint LastViewportSize = FIntPoint::ZeroValue;
	float LastViewportScale = 0.0f;
};
