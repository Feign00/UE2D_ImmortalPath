// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalOfflineRewardTypes.h"

FImmortalOfflineRewardResult UImmortalOfflineRewardLibrary::Calculate(
	const int64 LastSavedUtcTicks,
	const int64 CurrentUtcTicks,
	const int32 MinimumOfflineSeconds,
	const float MaximumOfflineHours,
	const float CultivationPerSecond,
	const float CultivationEfficiency,
	const float SpiritStonesPerMinute,
	const float EquipmentIntervalSeconds,
	const int32 MaximumEquipmentCount,
	const float MaterialIntervalSeconds,
	const int32 MaximumMaterialBundleCount,
	const int64 DevelopmentOverrideSeconds)
{
	FImmortalOfflineRewardResult Result;
	if (DevelopmentOverrideSeconds >= 0)
	{
		Result.RawOfflineSeconds = DevelopmentOverrideSeconds;
	}
	else
	{
		if (LastSavedUtcTicks <= 0 || CurrentUtcTicks <= 0)
		{
			return Result;
		}
		if (CurrentUtcTicks < LastSavedUtcTicks)
		{
			Result.bClockRollbackDetected = true;
			return Result;
		}
		Result.RawOfflineSeconds = FMath::Max<int64>(
			FMath::FloorToInt64(FTimespan(CurrentUtcTicks - LastSavedUtcTicks).GetTotalSeconds()), 0);
	}

	if (Result.RawOfflineSeconds < FMath::Max(MinimumOfflineSeconds, 0))
	{
		return Result;
	}

	const int64 MaximumSeconds = FMath::Max<int64>(
		FMath::FloorToInt64(FMath::Max(MaximumOfflineHours, 0.0f) * 3600.0), 0);
	Result.RewardedOfflineSeconds = MaximumSeconds > 0
		? FMath::Min(Result.RawOfflineSeconds, MaximumSeconds)
		: Result.RawOfflineSeconds;
	Result.bCappedByMaximum = Result.RewardedOfflineSeconds < Result.RawOfflineSeconds;
	Result.bEligible = Result.RewardedOfflineSeconds > 0;

	const double CultivationReward = static_cast<double>(Result.RewardedOfflineSeconds)
		* FMath::Max(CultivationPerSecond, 0.0f)
		* FMath::Max(CultivationEfficiency, 0.0f);
	Result.Cultivation = static_cast<int32>(FMath::Clamp<int64>(
		FMath::FloorToInt64(CultivationReward), 0, MAX_int32));

	const double SpiritStoneReward = static_cast<double>(Result.RewardedOfflineSeconds) / 60.0
		* FMath::Max(SpiritStonesPerMinute, 0.0f);
	Result.SpiritStones = static_cast<int32>(FMath::Clamp<int64>(
		FMath::FloorToInt64(SpiritStoneReward), 0, MAX_int32));

	if (EquipmentIntervalSeconds > 0.0f)
	{
		Result.EquipmentCount = FMath::Clamp(
			FMath::FloorToInt(static_cast<double>(Result.RewardedOfflineSeconds) / EquipmentIntervalSeconds),
			0,
			FMath::Max(MaximumEquipmentCount, 0));
	}
	if (MaterialIntervalSeconds > 0.0f)
	{
		Result.MaterialBundleCount = FMath::Clamp(
			FMath::FloorToInt(static_cast<double>(Result.RewardedOfflineSeconds) / MaterialIntervalSeconds),
			0,
			FMath::Max(MaximumMaterialBundleCount, 0));
	}
	return Result;
}
