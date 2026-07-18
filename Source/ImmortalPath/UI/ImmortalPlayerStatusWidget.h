// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalPlayerStatusWidget.generated.h"

class AImmortalPlayerCharacter;
class UProgressBar;

/** Native player HUD assembled from the supplied layered PNG textures. */
UCLASS()
class IMMORTALPATH_API UImmortalPlayerStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleInventoryClicked();

	UFUNCTION()
	void HandleAlchemyClicked();

	UFUNCTION()
	void HandleCraftingClicked();

	UFUNCTION()
	void HandleArtifactClicked();

	UFUNCTION()
	void HandleTechniqueClicked();

	UFUNCTION()
	void HandleCharacterBuildClicked();

	UFUNCTION()
	void HandleShopClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthProgress;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ManaProgress;
};
