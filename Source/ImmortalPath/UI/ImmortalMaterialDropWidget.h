// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalMaterialDropWidget.generated.h"

struct FImmortalMaterialDefinition;
class UImage;
class UTextBlock;
class UTexture2D;

/** Native world-space material marker using shared item art with a glyph fallback. */
UCLASS()
class IMMORTALPATH_API UImmortalMaterialDropWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMaterial(FName MaterialId, const FImmortalMaterialDefinition& Definition, int32 Quantity);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Art")
	TSoftObjectPtr<UTexture2D> ForgeAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_ForgeAtlas.T_ForgeAtlas")));

	UPROPERTY(EditDefaultsOnly, Category = "Art")
	TSoftObjectPtr<UTexture2D> MaterialAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_MaterialAtlas.T_MaterialAtlas")));

	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UImage> IconImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GlyphText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;
};

