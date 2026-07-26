// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalFarmingWidget.generated.h"

class AImmortalPlayerCharacter;
class UBorder;
class UButton;
class UProgressBar;
class UTextBlock;

/** Native 1600x300 TBH spirit-field strip reached through the player's cave. */
UCLASS()
class IMMORTALPATH_API UImmortalFarmingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void SelectCrop(FName CropId);
	void RefreshCropCards();
	void RefreshPlotCards();
	void HandlePlotAction(int32 PlotIndex);
	void SetResultMessage(const FText& Message, bool bSucceeded);

	UFUNCTION() void HandleSpiritGrassSelected();
	UFUNCTION() void HandleImmortalFruitSelected();
	UFUNCTION() void HandleSpiritWoodSelected();
	UFUNCTION() void HandlePlot0Clicked();
	UFUNCTION() void HandlePlot1Clicked();
	UFUNCTION() void HandlePlot2Clicked();
	UFUNCTION() void HandlePlot3Clicked();
	UFUNCTION() void HandlePlot4Clicked();
	UFUNCTION() void HandlePlot5Clicked();
	UFUNCTION() void HandlePlantAllClicked();
	UFUNCTION() void HandleHarvestAllClicked();
	UFUNCTION() void HandleCloseClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeaderSummaryText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> CropButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> CropButtonLabels;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SelectedCropText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PlantAllButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlantAllButtonText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> PlotBorders;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PlotTitleTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PlotGlyphTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PlotStatusTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PlotDetailTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> PlotProgressBars;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> PlotActionButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PlotActionLabels;

	UPROPERTY(Transient)
	TObjectPtr<UButton> HarvestAllButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HarvestAllButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultText;

	FName SelectedCropId = TEXT("SpiritGrassCrop");
	int32 LastFarmingRevision = MIN_int32;
	int32 LastMaterialRevision = MIN_int32;
	int32 LastSpiritStones = MIN_int32;
	float RefreshAccumulator = 0.0f;
};
