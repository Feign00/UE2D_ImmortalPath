// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalAscensionWidget.generated.h"

class AImmortalPlayerCharacter;
class UBorder;
class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

/** Native TBH-height panel for repeatable ascension and permanent path investment. */
UCLASS()
class IMMORTALPATH_API UImmortalAscensionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();

	/** Plays the committed ascension visual inside this independent page. */
	void PlayAscensionSequence();

	bool IsAscensionSequencePlaying() const
	{
		return bAscensionSequencePlaying;
	}

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleReturnQingyunClicked();

	UFUNCTION()
	void HandleAscendClicked();

	UFUNCTION()
	void HandleBattlePathClicked();

	UFUNCTION()
	void HandleEnlightenmentPathClicked();

	UFUNCTION()
	void HandleFortunePathClicked();

	void InvestPath(uint8 PathValue);
	void SetResultMessage(const FText& Message, bool bSucceeded);
	void ResetResultMessage();
	void UpdateAscensionSequenceFrame(int32 FrameIndex);

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeaderSummaryText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EligibilityText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BonusText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> BattlePathButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> EnlightenmentPathButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> FortunePathButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BattlePathText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EnlightenmentPathText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FortunePathText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ReturnQingyunButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> AscendButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AscendButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> AscensionSequenceOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UImage> AscensionSequenceImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AscensionSequenceCaption;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> AscensionSequenceTexture;

	int32 LastRevision = INDEX_NONE;
	float RefreshAccumulator = 0.0f;
	double ResultMessageExpirySeconds = 0.0;
	float AscensionSequenceElapsedSeconds = 0.0f;
	int32 AscensionSequenceFrame = INDEX_NONE;
	bool bAscensionSequencePlaying = false;
};
