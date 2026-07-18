// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalTechniqueWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;
class UVerticalBox;

UCLASS()
class IMMORTALPATH_API UImmortalTechniqueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();
	void SelectTechnique(FName TechniqueId);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RebuildTechniqueEntries();
	void RefreshDetails();
	void SetResult(const FText& Message);
	UFUNCTION() void HandleLearnClicked();
	UFUNCTION() void HandleEquipSlotOneClicked();
	UFUNCTION() void HandleEquipSlotTwoClicked();
	UFUNCTION() void HandleUpgradeClicked();
	UFUNCTION() void HandleBreakthroughClicked();
	UFUNCTION() void HandleActivePointClicked();
	UFUNCTION() void HandlePassivePointClicked();
	UFUNCTION() void HandleSpecialPointClicked();
	UFUNCTION() void HandleCloseClicked();

	UPROPERTY(Transient) TWeakObjectPtr<AImmortalPlayerCharacter> Player;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> TechniqueList;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResourceText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SlotText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailNameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailMetaText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DescriptionText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ActiveText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> UltimateText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PassiveText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SpecialText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CostText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultText;
	UPROPERTY(Transient) TObjectPtr<UButton> LearnButton;
	UPROPERTY(Transient) TObjectPtr<UButton> EquipSlotOneButton;
	UPROPERTY(Transient) TObjectPtr<UButton> EquipSlotTwoButton;
	UPROPERTY(Transient) TObjectPtr<UButton> UpgradeButton;
	UPROPERTY(Transient) TObjectPtr<UButton> BreakthroughButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ActivePointButton;
	UPROPERTY(Transient) TObjectPtr<UButton> PassivePointButton;
	UPROPERTY(Transient) TObjectPtr<UButton> SpecialPointButton;

	FName SelectedTechniqueId = NAME_None;
	int32 LastTechniqueRevision = INDEX_NONE;
	int32 LastMaterialRevision = INDEX_NONE;
	int32 LastSpiritStones = INDEX_NONE;
	int32 LastCultivation = INDEX_NONE;
	int32 LastStage = INDEX_NONE;
};
