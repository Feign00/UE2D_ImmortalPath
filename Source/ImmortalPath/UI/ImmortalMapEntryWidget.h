// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Maps/ImmortalMapTypes.h"
#include "ImmortalMapEntryWidget.generated.h"

class UButton;
class UImmortalMapWidget;
class UTextBlock;

/** Compact map selector row used by the native adventure-map screen. */
UCLASS()
class IMMORTALPATH_API UImmortalMapEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeMapEntry(
		UImmortalMapWidget* InOwner,
		const FImmortalMapDefinition& InDefinition,
		const FImmortalMapProgress& InProgress,
		bool bInUnlocked,
		bool bInActive,
		bool bInSelected);

protected:
	virtual void NativeOnInitialized() override;

private:
	void RefreshAppearance();

	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<UImmortalMapWidget> OwnerMapWidget;

	UPROPERTY(Transient)
	TObjectPtr<UButton> EntryButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EntryText;

	FName MapId = NAME_None;
	FText MapName;
	FText RealmRequirement;
	FImmortalMapProgress Progress;
	FLinearColor AccentColor = FLinearColor::White;
	int32 MaximumStage = 999;
	bool bUnlocked = false;
	bool bActive = false;
	bool bSelected = false;
};

