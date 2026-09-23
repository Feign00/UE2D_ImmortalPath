// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalSaveRecoveryWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;

/** Blocking recovery screen shown when an existing main save cannot be read. */
UCLASS()
class IMMORTALPATH_API UImmortalSaveRecoveryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(
		AImmortalPlayerCharacter* InPlayer,
		bool bHasBackup,
		bool bMainMissing,
		bool bIncompatibleVersion);
	/** Call after AddToViewport, including while the world is paused. */
	void ActivateInput();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FImmortalSaveRecoveryWidgetStateTest;
#endif

	UFUNCTION()
	void HandleRestoreBackupClicked();

	UFUNCTION()
	void HandleExitClicked();

	void SetStatusMessage(const FText& Message, bool bIsError);
	void FocusRecoveryAction();
	void BuildRecoveryWidgetTree();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ExplanationText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BackupExplanationText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RestoreButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ExitButton;

	bool bBackupAvailable = false;
};
