// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Maps/ImmortalMapTypes.h"
#include "ImmortalMapWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;
class UVerticalBox;

/** Native 900x600 two-column screen for selecting one of the eight adventure maps. */
UCLASS()
class IMMORTALPATH_API UImmortalMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();
	void SelectMap(FName MapId);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RebuildMapEntries();
	void RefreshSelectedMapDetails();
	uint32 CalculateStateFingerprint(const FImmortalMapSystemState& State, int32 RealmIndex) const;
	void SetResultMessage(const FText& Message, bool bSucceeded);

	UFUNCTION()
	void HandleTravelClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> MapList;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CurrentMapText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailRequirementText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailDescriptionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailProgressText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailEnemyText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailDropText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TravelButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TravelButtonText;

	FName SelectedMapId = NAME_None;
	FImmortalMapSystemState CachedState;
	uint32 LastStateFingerprint = MAX_uint32;
	int32 LastObservedMapRevision = MIN_int32;
	int32 LastObservedRealmIndex = MIN_int32;
};
