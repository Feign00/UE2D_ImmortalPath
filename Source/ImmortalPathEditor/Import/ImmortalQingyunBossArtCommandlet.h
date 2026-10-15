// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Commandlets/Commandlet.h"
#include "ImmortalQingyunBossArtCommandlet.generated.h"

/**
 * Fixed Qingyun map-Boss art binding. Creates only BP_DesktopQingyunBoss from
 * protected BP_DogMonster after all five complete actions pass preflight.
 * Attack/Hurt timing and every non-art default are preserved, including the
 * actual legacy Hurt clip duration. VerifyOnly never writes any package.
 */
UCLASS()
class UImmortalQingyunBossArtCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UImmortalQingyunBossArtCommandlet();
	virtual int32 Main(const FString& Params) override;
};
