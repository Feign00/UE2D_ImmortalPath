// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "ImmortalFoxArtCommandlet.generated.h"

/**
 * Binds the complete DesktopPixelV2 fox family to the single existing BP_Monster.
 * No asset paths are accepted. Use -VerifyOnly for a read-only binding check.
 * The Blueprint is saved only after asset, compile and default-value validation.
 */
UCLASS()
class UImmortalFoxArtCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UImmortalFoxArtCommandlet();
	virtual int32 Main(const FString& Params) override;
};
