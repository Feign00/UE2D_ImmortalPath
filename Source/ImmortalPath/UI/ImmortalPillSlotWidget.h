// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Alchemy/ImmortalAlchemyTypes.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalPillSlotWidget.generated.h"

class UButton;
class UImmortalAlchemyWidget;
class UTextBlock;

UCLASS()
class IMMORTALPATH_API UImmortalPillSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializePill(UImmortalAlchemyWidget* InOwner, const FImmortalPillStack& InStack, bool bSelected);

protected:
	virtual void NativeOnInitialized() override;

private:
	void RefreshAppearance();

	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<UImmortalAlchemyWidget> OwnerAlchemy;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GlyphText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> QuantityText;

	FImmortalPillStack Stack;
	bool bPillSelected = false;
};

