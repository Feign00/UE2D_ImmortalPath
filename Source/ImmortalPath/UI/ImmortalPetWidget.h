// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Pets/ImmortalPetTypes.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalPetWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;

/**
 * Native 1600x300 Taskbar Hero panel for selecting and growing combat pets.
 *
 * The first two entries in UImmortalPetLibrary's catalog are presented as
 * compact selection cards. Dedicated pet portraits can replace the glyph
 * presentation later without changing the interaction or persistent PetId.
 */
UCLASS()
class IMMORTALPATH_API UImmortalPetWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	void SelectPetByIndex(int32 PetIndex);
	void RefreshPetSelection(
		const FImmortalPetState& State);
	void RefreshPetDetails(
		const FImmortalPetState& State);
	void RefreshPetActions(
		const FImmortalPetState& State);
	void SetResultMessage(
		const FText& Message,
		bool bSucceeded);
	void ResetResultMessage();

	UFUNCTION()
	void HandleFirstPetClicked();

	UFUNCTION()
	void HandleSecondPetClicked();

	UFUNCTION()
	void HandlePrimaryActionClicked();

	UFUNCTION()
	void HandleStarUpClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> PetButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PetButtonLabels;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeaderSummaryText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> IdentityText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatsText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CostText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PrimaryActionButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PrimaryActionButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> StarUpButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StarUpButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultText;

	TArray<FName> PetIds;
	FName SelectedPetId = NAME_None;
	FName LastActivePetId = NAME_None;
	int32 LastRevision = MIN_int32;
	float RefreshAccumulator = 0.0f;
	double ResultMessageExpirySeconds = 0.0;
};
