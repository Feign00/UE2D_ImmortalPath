// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalWorldBossWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;

/**
 * Native 1600x300 Taskbar Hero strip for selecting and challenging World Bosses.
 *
 * The widget intentionally uses only stock UMG primitives so the feature is
 * immediately usable before dedicated Boss portraits and effects are ready.
 */
UCLASS()
class IMMORTALPATH_API UImmortalWorldBossWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();
	void SelectBoss(FName BossId);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void SelectBossByIndex(int32 BossIndex);
	void RefreshBossButtons();
	void RefreshSelectedBossDetails();
	void RefreshRuntimeStatus();
	void SetResultMessage(const FText& Message, bool bSucceeded);

	UFUNCTION() void HandleBoss0Clicked();
	UFUNCTION() void HandleBoss1Clicked();
	UFUNCTION() void HandleBoss2Clicked();
	UFUNCTION() void HandleBoss3Clicked();
	UFUNCTION() void HandleChallengeClicked();
	UFUNCTION() void HandleRetryRewardsClicked();
	UFUNCTION() void HandleCloseClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeaderSummaryText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> BossButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> BossButtonLabels;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BossNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BossDescriptionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RequirementText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HistoryText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SkillText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DropText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RuntimeText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ChallengeButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ChallengeButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RetryRewardsButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RetryRewardsButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultText;

	TArray<FName> DisplayedBossIds;
	FName SelectedBossId = NAME_None;
	int32 LastWorldBossRevision = MIN_int32;
	int32 LastRealmIndex = MIN_int32;
	FName LastRuntimeBossId = NAME_None;
	int32 LastRuntimePhase = MIN_int32;
	bool bLastChallengeActive = false;
	float RuntimeRefreshAccumulator = 0.0f;
};
