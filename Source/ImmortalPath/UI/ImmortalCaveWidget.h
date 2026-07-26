// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Cave/ImmortalCaveTypes.h"
#include "ImmortalCaveWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;
class UVerticalBox;

/** Native 900x600 management screen for the player's persistent cave. */
UCLASS()
class IMMORTALPATH_API UImmortalCaveWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void SelectBuilding(EImmortalCaveBuildingType BuildingType);
	void RebuildBuildingButtons();
	void RefreshBuildingDetails();
	void SetResultMessage(const FText& Message, bool bSucceeded);

	UFUNCTION()
	void HandleCaveHeartClicked();

	UFUNCTION()
	void HandleMeditationRoomClicked();

	UFUNCTION()
	void HandleSpiritVeinClicked();

	UFUNCTION()
	void HandleStoragePavilionClicked();

	UFUNCTION()
	void HandleAlchemyRoomClicked();

	UFUNCTION()
	void HandleForgeRoomClicked();

	UFUNCTION()
	void HandleSpiritFieldClicked();

	UFUNCTION()
	void HandleUpgradeClicked();

	UFUNCTION()
	void HandleCollectClicked();

	UFUNCTION()
	void HandleAlchemyClicked();

	UFUNCTION()
	void HandleCraftingClicked();

	UFUNCTION()
	void HandleFarmingClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> BuildingList;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> BuildingButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> BuildingButtonLabels;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StoredResourceText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProductionRateText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BuildingNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BuildingLevelText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BuildingDescriptionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CurrentEffectText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NextEffectText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> UpgradeCostText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> UpgradeButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> UpgradeButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CollectButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CollectButtonText;

	EImmortalCaveBuildingType SelectedBuilding = EImmortalCaveBuildingType::CaveHeart;
	int32 LastCaveRevision = MIN_int32;
	int32 LastMaterialRevision = MIN_int32;
	int32 LastSpiritStones = MIN_int32;
	float RefreshAccumulator = 0.0f;
};
