#pragma once

#include "CoreMinimal.h"
#include "ImmortalPixelPlayerAssets.generated.h"

class UPaperFlipbook;

/** Reflected soft references keep the complete default family discoverable by cooking. */
USTRUCT(BlueprintType)
struct FImmortalPixelPlayerAssets
{
	GENERATED_BODY()
	FImmortalPixelPlayerAssets();
	UPROPERTY(EditAnywhere, Category = "Animation") TSoftObjectPtr<UPaperFlipbook> Idle;
	UPROPERTY(EditAnywhere, Category = "Animation") TSoftObjectPtr<UPaperFlipbook> Move;
	UPROPERTY(EditAnywhere, Category = "Animation") TSoftObjectPtr<UPaperFlipbook> Attack;
	UPROPERTY(EditAnywhere, Category = "Animation") TSoftObjectPtr<UPaperFlipbook> Hurt;
	UPROPERTY(EditAnywhere, Category = "Animation") TSoftObjectPtr<UPaperFlipbook> Death;
	bool LoadComplete(TArray<UPaperFlipbook*>& Out) const;
};

namespace ImmortalPixelPlayerTiming
{
	inline float SafeWindup(float Value) { return FMath::IsFinite(Value) ? FMath::Max(Value, 0.0f) : 0.25f; }
	struct FAttackPlayback { float Rate = 1.0f; float Start = 0.0f; float Duration = 0.01f; };
	inline FAttackPlayback Attack(float ClipDuration, float FPS, float Windup)
	{
		FAttackPlayback Result;
		const float Contact = 3.0f / FMath::Max(FPS, 1.0f);
		Windup = SafeWindup(Windup);
		if (Windup <= UE_SMALL_NUMBER) Result.Start = Contact;
		else Result.Rate = Contact / Windup;
		Result.Duration = FMath::Max((ClipDuration - Result.Start) / Result.Rate, 0.01f);
		return Result;
	}

	struct FAttackRecovery
	{
		float Rate = 1.0f;
		float Duration = 0.0f;
		bool bFitsCycle = true;
	};

	/** Re-time only frames after contact; the caller leaves the windup playback unchanged. */
	inline FAttackRecovery AttackRecovery(float ClipDuration, float PlaybackPosition,
		float InitialRate, float Windup, float CycleInterval)
	{
		constexpr float MaxRecoveryRate = 32.0f;
		FAttackRecovery Result;
		// Never slow a valid pre-contact rate, even when the nominal cycle has spare time.
		Result.Rate = FMath::IsFinite(InitialRate) && InitialRate > 0.0f
			? FMath::Max(InitialRate, 0.01f) : 1.0f;
		const float SafeDuration = FMath::IsFinite(ClipDuration)
			? FMath::Max(ClipDuration, 0.0f) : 0.0f;
		const float SafePosition = FMath::IsFinite(PlaybackPosition)
			? FMath::Clamp(PlaybackPosition, 0.0f, SafeDuration) : 0.0f;
		const float RemainingFramesDuration = SafeDuration - SafePosition;
		if (RemainingFramesDuration <= UE_SMALL_NUMBER)
		{
			return Result;
		}

		const float SafeCycle = FMath::IsFinite(CycleInterval)
			? FMath::Max(CycleInterval, 0.0f) : 0.0f;
		const float RecoveryWindow = SafeCycle - SafeWindup(Windup);
		if (RecoveryWindow > UE_SMALL_NUMBER)
		{
			// Finish just ahead of the next looping attack timer. UE does not
			// guarantee an order for timers sharing the exact same expiration.
			const float SafetyMargin = FMath::Min(0.01f, RecoveryWindow * 0.1f);
			const double RequiredRate = static_cast<double>(RemainingFramesDuration)
				/ static_cast<double>(RecoveryWindow - SafetyMargin);
			Result.Rate = FMath::Max(Result.Rate,
				static_cast<float>(FMath::Min(RequiredRate, static_cast<double>(MaxRecoveryRate))));
		}
		else
		{
			// No mathematical post-contact window exists. Bound the display rate and
			// report that the nominal cycle cannot contain a full recovery.
			Result.Rate = FMath::Max(Result.Rate, MaxRecoveryRate);
		}
		Result.Duration = RemainingFramesDuration / Result.Rate;
		Result.bFitsCycle = RecoveryWindow > 0.0f
			&& Result.Duration <= RecoveryWindow + UE_SMALL_NUMBER;
		return Result;
	}
}
