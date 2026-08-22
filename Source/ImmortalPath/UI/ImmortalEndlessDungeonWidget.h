// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalEndlessDungeonWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;

/**
 * Native 1600x300 Taskbar Hero panel for the independent Endless Dungeon.
 *
 * The panel intentionally uses stock UMG primitives so the complete feature is
 * usable before dedicated dungeon backgrounds, floor emblems and affix icons
 * are available.
 */
UCLASS()
class IMMORTALPATH_API UImmortalEndlessDungeonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RefreshProgressAndRules();
	void RefreshRuntimeAndRewardPreview();
	void RefreshActions();
	void SetResultMessage(const FText& Message, bool bSucceeded);

	UFUNCTION()
	void HandleStartOrExitClicked();

	UFUNCTION()
	void HandleRetryRewardsClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeaderSummaryText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProgressText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RulesText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RuntimeText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FloorPreviewText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RewardPreviewText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> StartOrExitButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StartOrExitButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RetryRewardsButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RetryRewardsButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultText;

	int32 LastRevision = MIN_int32;
	int32 LastRuntimeFloor = MIN_int32;
	int32 LastRuntimeKills = MIN_int32;
	bool bLastRunActive = false;
	float RuntimeRefreshAccumulator = 0.0f;
};
