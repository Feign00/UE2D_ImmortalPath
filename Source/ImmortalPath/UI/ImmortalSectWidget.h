// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalSectWidget.generated.h"

class AImmortalPlayerCharacter;
class UBorder;
class UButton;
class UTextBlock;
class UImage;
class UTexture2D;
class UProgressBar;

/** Illustrated full-height sect management page; opening it does not pause combat. */
UCLASS()
class IMMORTALPATH_API UImmortalSectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Immortal Path|Sect Art")
	TSoftObjectPtr<UTexture2D> SectAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_SectAtlas.T_SectAtlas")));

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> SectImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> TaskProgressBars;

	void SelectSectByIndex(int32 SectIndex);
	void HandleTaskClaimByIndex(int32 TaskIndex);
	void HandleOfferExchangeByIndex(int32 OfferIndex);
	void RefreshSectChoices();
	void RefreshTaskCards();
	void RefreshOfferCards();
	void SetResultMessage(const FText& Message, bool bSucceeded);

	UFUNCTION() void HandleSect0Clicked();
	UFUNCTION() void HandleSect1Clicked();
	UFUNCTION() void HandleSect2Clicked();
	UFUNCTION() void HandleSect3Clicked();
	UFUNCTION() void HandleJoinClicked();
	UFUNCTION() void HandleTask0Clicked();
	UFUNCTION() void HandleTask1Clicked();
	UFUNCTION() void HandleTask2Clicked();
	UFUNCTION() void HandleOffer0Clicked();
	UFUNCTION() void HandleOffer1Clicked();
	UFUNCTION() void HandleOffer2Clicked();
	UFUNCTION() void HandleOffer3Clicked();
	UFUNCTION() void HandleCloseClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeaderSummaryText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> SectButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> SectButtonLabels;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SelectedSectText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> JoinButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> JoinButtonText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> TaskBorders;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> TaskTitleTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> TaskDetailTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> TaskActionButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> TaskActionLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> OfferBorders;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferTitleTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferDetailTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> OfferActionButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferActionLabels;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultText;

	TArray<FName> DisplayedSectIds;
	TArray<FName> DisplayedTaskIds;
	TArray<FName> DisplayedOfferIds;
	FName SelectedSectId = NAME_None;
	int32 LastSectRevision = MIN_int32;
	int32 LastTechniqueRevision = MIN_int32;
	int32 LastMaterialRevision = MIN_int32;
	int32 LastSpiritStones = MIN_int32;
};
