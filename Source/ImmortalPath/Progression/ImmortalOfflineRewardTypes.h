// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalOfflineRewardTypes.generated.h"

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalOfflineRewardResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Offline")
	int64 RawOfflineSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Offline")
	int64 RewardedOfflineSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Offline")
	int32 Cultivation = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Offline")
	int32 SpiritStones = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Offline")
	int32 EquipmentCount = 0;

	/** Number of stage-aware material rolls calculated before they are converted into concrete stacks. */
	UPROPERTY(BlueprintReadOnly, Category = "Offline")
	int32 MaterialBundleCount = 0;

	/** Total concrete material units actually accepted by the inventory. */
	UPROPERTY(BlueprintReadOnly, Category = "Offline")
	int32 MaterialCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Offline")
	bool bEligible = false;

	UPROPERTY(BlueprintReadOnly, Category = "Offline")
	bool bClockRollbackDetected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Offline")
	bool bCappedByMaximum = false;
};

/** Pure offline-reward calculation shared by the player and future cave/idle systems. */
UCLASS()
class IMMORTALPATH_API UImmortalOfflineRewardLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static FImmortalOfflineRewardResult Calculate(
		int64 LastSavedUtcTicks,
		int64 CurrentUtcTicks,
		int32 MinimumOfflineSeconds,
		float MaximumOfflineHours,
		float CultivationPerSecond,
		float CultivationEfficiency,
		float SpiritStonesPerMinute,
		float EquipmentIntervalSeconds,
		int32 MaximumEquipmentCount,
		float MaterialIntervalSeconds,
		int32 MaximumMaterialBundleCount,
		int64 DevelopmentOverrideSeconds = -1);
};
