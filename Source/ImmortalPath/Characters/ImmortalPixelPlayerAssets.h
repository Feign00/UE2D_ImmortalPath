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
}
