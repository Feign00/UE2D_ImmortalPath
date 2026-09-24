// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalSaveExitWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;

/** Compact, non-pausing prompt shown after a desktop quit-save fails. */
UCLASS()
class IMMORTALPATH_API UImmortalSaveExitWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	/** Call after AddToViewport. Changes input focus but does not pause the world. */
	void ActivateInput();
	void ShowFailureMessage();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FImmortalSaveExitWidgetStateTest;
#endif

	UFUNCTION()
	void HandleRetryClicked();

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleExitWithoutSavingClicked();

	void BuildWidgetTree();
	void SetStatusMessage(const FText& Message, bool bIsError);
	void DisarmExitWithoutSaving();
	void FocusRetryAction();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ExitWithoutSavingLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RetryButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ContinueButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ExitWithoutSavingButton;

	bool bExitWithoutSavingArmed = false;
};
