// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Quests/ImmortalQuestTypes.h"
#include "ImmortalQuestWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UScrollBox;
class UTextBlock;

/** Main, daily and achievement tasks embedded in the unified management page. */
UCLASS()
class IMMORTALPATH_API UImmortalQuestWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Quest")
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Quest")
	void RefreshFromPlayer();

	void ClaimQuest(FName QuestId);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void SelectCategory(EImmortalQuestCategory Category);
	void RebuildQuestList();
	void RefreshCategoryButtons();

	UFUNCTION()
	void HandleMainClicked();

	UFUNCTION()
	void HandleDailyClicked();

	UFUNCTION()
	void HandleAchievementClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UButton> MainButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DailyButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> AchievementButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultText;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> QuestList;

	EImmortalQuestCategory SelectedCategory = EImmortalQuestCategory::Main;
	int32 LastQuestRevision = MIN_int32;
	float RefreshAccumulator = 0.0f;
};

