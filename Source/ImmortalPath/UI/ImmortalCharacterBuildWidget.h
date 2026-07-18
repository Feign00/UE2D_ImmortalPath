// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Progression/ImmortalCharacterPathTypes.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalCharacterBuildWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;
class UVerticalBox;

UCLASS()
class IMMORTALPATH_API UImmortalCharacterBuildWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();
	void SelectPath(EImmortalCultivationPath Path);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RebuildPathEntries();
	void RefreshPathDetails();
	UFUNCTION() void HandleChoosePathClicked();
	UFUNCTION() void HandleCloseClicked();

	UPROPERTY(Transient) TWeakObjectPtr<AImmortalPlayerCharacter> Player;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> PathList;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResourceText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RootNameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RootMetaText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RootDescriptionText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RootEffectText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PathNameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PathDescriptionText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PathSkillText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PathBonusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EquipmentText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CostText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultText;
	UPROPERTY(Transient) TObjectPtr<UButton> ChoosePathButton;

	EImmortalCultivationPath SelectedPath = EImmortalCultivationPath::Body;
	int32 LastBuildRevision = INDEX_NONE;
	int32 LastEquipmentRevision = INDEX_NONE;
	int32 LastMaterialRevision = INDEX_NONE;
	int32 LastSpiritStones = INDEX_NONE;
};
