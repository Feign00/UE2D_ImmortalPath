// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Quests/ImmortalQuestTypes.h"
#include "ImmortalQuestEntryWidget.generated.h"

class UButton;
class UImmortalQuestWidget;
class UTextBlock;

/** One compact claimable row inside the native 1286x238 quest page. */
UCLASS()
class IMMORTALPATH_API UImmortalQuestEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Configure(
		UImmortalQuestWidget* InOwner,
		const FImmortalQuestDefinition& InDefinition,
		const FImmortalQuestProgressView& InProgress);

protected:
	virtual void NativeOnInitialized() override;

private:
	void RefreshAppearance();

	UFUNCTION()
	void HandleClaimClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<UImmortalQuestWidget> OwnerQuestWidget;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ClaimButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RowText;

	FImmortalQuestDefinition Definition;
	FImmortalQuestProgressView Progress;
};

