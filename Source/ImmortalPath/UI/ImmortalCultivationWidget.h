// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalCultivationWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UProgressBar;
class UTextBlock;

/** Embeddable page for the independent, combat-free auto-cultivation loop. */
UCLASS()
class IMMORTALPATH_API UImmortalCultivationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Cultivation UI")
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Cultivation UI")
	void RefreshFromPlayer();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleAscensionClicked();

	UFUNCTION()
	void HandleHomeClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RealmText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProgressText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RateText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RuleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AscensionHintText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> CultivationProgressBar;

	UPROPERTY(Transient)
	TObjectPtr<UButton> AscensionButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AscensionButtonText;

	float RefreshAccumulator = 0.0f;
};
