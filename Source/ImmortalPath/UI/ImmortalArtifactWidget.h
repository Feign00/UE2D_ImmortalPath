// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalArtifactWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;
class UVerticalBox;

UCLASS()
class IMMORTALPATH_API UImmortalArtifactWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();
	void SelectDefinition(FName ArtifactId);
	void SelectArtifact(FGuid InstanceId);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RebuildDefinitionEntries();
	void RebuildArtifactEntries();
	void RefreshDetails();
	UFUNCTION() void HandleCraftClicked();
	UFUNCTION() void HandleEquipClicked();
	UFUNCTION() void HandleUpgradeClicked();
	UFUNCTION() void HandleStarClicked();
	UFUNCTION() void HandleCloseClicked();

	UPROPERTY(Transient) TWeakObjectPtr<AImmortalPlayerCharacter> Player;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> DefinitionList;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> ArtifactList;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CurrencyText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailNameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailMetaText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DescriptionText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ActiveText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PassiveText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CostText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultText;
	UPROPERTY(Transient) TObjectPtr<UButton> CraftButton;
	UPROPERTY(Transient) TObjectPtr<UButton> EquipButton;
	UPROPERTY(Transient) TObjectPtr<UButton> UpgradeButton;
	UPROPERTY(Transient) TObjectPtr<UButton> StarButton;

	FName SelectedDefinitionId = NAME_None;
	FGuid SelectedInstanceId;
	bool bViewingDefinition = true;
	int32 LastArtifactRevision = INDEX_NONE;
	int32 LastMaterialRevision = INDEX_NONE;
	int32 LastSpiritStones = INDEX_NONE;
	int32 LastStage = INDEX_NONE;
};

