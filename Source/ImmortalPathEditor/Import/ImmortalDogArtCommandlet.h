// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "ImmortalDogArtCommandlet.generated.h"

/**
 * Binds only BP_DesktopDogMonster, keeping the legacy dog/Boss package intact.
 * First creation requires -CreateFromLegacy and uses Unreal's asset duplication
 * API after all five authored actions pass preflight. -VerifyOnly never writes.
 * No caller-supplied asset paths or overwrite of the legacy package is allowed.
 */
UCLASS()
class UImmortalDogArtCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UImmortalDogArtCommandlet();
	virtual int32 Main(const FString& Params) override;
};
