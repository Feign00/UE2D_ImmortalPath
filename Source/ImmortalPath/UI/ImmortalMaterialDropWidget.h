// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalMaterialDropWidget.generated.h"

struct FImmortalMaterialDefinition;
class UTextBlock;

/** Native world-space material marker used until final 128x128 icons are supplied. */
UCLASS()
class IMMORTALPATH_API UImmortalMaterialDropWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMaterial(const FImmortalMaterialDefinition& Definition, int32 Quantity);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GlyphText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;
};

