// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalArtifactEntryWidget.generated.h"

class UButton;
class UImmortalArtifactWidget;
class UTextBlock;
struct FImmortalArtifactItem;

UCLASS()
class IMMORTALPATH_API UImmortalArtifactEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeDefinitionEntry(UImmortalArtifactWidget* InOwner, FName InArtifactId, bool bUnlocked, bool bSelected);
	void InitializeArtifactEntry(UImmortalArtifactWidget* InOwner, const FImmortalArtifactItem& Item, bool bEquipped, bool bSelected);

protected:
	virtual void NativeOnInitialized() override;

private:
	void RefreshAppearance();
	UFUNCTION() void HandleClicked();

	UPROPERTY(Transient) TWeakObjectPtr<UImmortalArtifactWidget> OwnerArtifact;
	UPROPERTY(Transient) TObjectPtr<UButton> EntryButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EntryText;
	FName ArtifactId = NAME_None;
	FGuid InstanceId;
	FText DisplayText;
	FLinearColor DisplayColor = FLinearColor::White;
	bool bDefinitionEntry = true;
	bool bEnabled = true;
	bool bSelected = false;
};

