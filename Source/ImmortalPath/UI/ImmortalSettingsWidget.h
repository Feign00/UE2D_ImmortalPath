// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalSettingsWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;

/** Native 1600x300 desktop/settings panel for the TBH Windows build. */
UCLASS()
class IMMORTALPATH_API UImmortalSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void HandleAlwaysOnTopClicked();

	UFUNCTION()
	void HandleMuteClicked();

	UFUNCTION()
	void HandleFrameRateClicked();

	UFUNCTION()
	void HandleMinimizeClicked();

	UFUNCTION()
	void HandleSaveAndQuitClicked();

	UFUNCTION()
	void HandleCloseClicked();

	void SetResultMessage(const FText& Message, bool bSucceeded);

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AlwaysOnTopText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MuteText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FrameRateText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultText;
};
