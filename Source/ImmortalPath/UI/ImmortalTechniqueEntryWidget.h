// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalTechniqueEntryWidget.generated.h"

class UButton;
class UImmortalTechniqueWidget;
class UTextBlock;

UCLASS()
class IMMORTALPATH_API UImmortalTechniqueEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeEntry(
		UImmortalTechniqueWidget* InOwner,
		FName InTechniqueId,
		bool bLearned,
		int32 Level,
		int32 BreakthroughRank,
		int32 EquippedSlot,
		bool bUnlocked,
		bool bSelected);

protected:
	virtual void NativeOnInitialized() override;

private:
	void RefreshAppearance();
	UFUNCTION() void HandleClicked();

	UPROPERTY(Transient) TWeakObjectPtr<UImmortalTechniqueWidget> OwnerTechnique;
	UPROPERTY(Transient) TObjectPtr<UButton> EntryButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EntryText;
	FName TechniqueId = NAME_None;
	FText DisplayText;
	FLinearColor DisplayColor = FLinearColor::White;
	bool bEnabled = true;
	bool bSelected = false;
};
