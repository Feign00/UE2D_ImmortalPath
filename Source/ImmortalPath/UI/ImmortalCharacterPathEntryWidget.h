// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Progression/ImmortalCharacterPathTypes.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalCharacterPathEntryWidget.generated.h"

class UButton;
class UImmortalCharacterBuildWidget;
class UTextBlock;

UCLASS()
class IMMORTALPATH_API UImmortalCharacterPathEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeEntry(
		UImmortalCharacterBuildWidget* InOwner,
		EImmortalCultivationPath InPath,
		bool bCurrent,
		bool bSelected);

protected:
	virtual void NativeOnInitialized() override;

private:
	void RefreshAppearance();
	UFUNCTION() void HandleClicked();

	UPROPERTY(Transient) TWeakObjectPtr<UImmortalCharacterBuildWidget> OwnerBuild;
	UPROPERTY(Transient) TObjectPtr<UButton> EntryButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EntryText;
	EImmortalCultivationPath Path = EImmortalCultivationPath::Unselected;
	FText DisplayText;
	FLinearColor DisplayColor = FLinearColor::White;
	bool bCurrentPath = false;
	bool bSelectedEntry = false;
};
