#pragma once

#include "PaperFlipbookComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

/** Shared presentation policy. Callers retain attack/hurt/death state authority. */
namespace ImmortalAnimationPlayback
{
	inline bool IsMoving(const float HorizontalSpeed)
	{
		return FMath::Abs(HorizontalSpeed) > 5.0f;
	}

	inline void UpdateLocomotion(UPaperFlipbookComponent* Sprite,
		UPaperFlipbook* Idle, UPaperFlipbook* Move, const bool bMoving)
	{
		if (!Sprite) return;
		UPaperFlipbook* Desired = bMoving ? Move : Idle;
		const bool bHoldPose = !Desired;
		if (!Desired) Desired = bMoving ? Idle : Move;
		if (!Desired) return;
		const bool bChanged = Sprite->GetFlipbook() != Desired;
		if (bChanged) Sprite->SetFlipbook(Desired);
		Sprite->SetPlayRate(1.0f);
		Sprite->SetLooping(!bHoldPose);
		if (bHoldPose)
		{
			// An explicit temporary fallback, not a fabricated idle animation.
			Sprite->Stop();
			Sprite->SetPlaybackPosition(0.0f, false);
		}
		else if (bChanged || !Sprite->IsPlaying())
		{
			Sprite->PlayFromStart();
		}
#if !UE_BUILD_SHIPPING
		if (GFrameCounter % 120 == 0 && FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestAnimationPlayback")))
		{
			UE_LOG(LogTemp, Display, TEXT("Animation locomotion audit: actor=%s moving=%s authoredIdle=%s playing=%s looping=%s"),
				*GetNameSafe(Sprite->GetOuter()), bMoving ? TEXT("true") : TEXT("false"),
				Idle ? TEXT("true") : TEXT("false"), Sprite->IsPlaying() ? TEXT("true") : TEXT("false"),
				Sprite->IsLooping() ? TEXT("true") : TEXT("false"));
		}
#endif
	}

	inline void PlayHurtWithoutRestart(UPaperFlipbookComponent* Sprite, UPaperFlipbook* Hurt,
		const bool bAlreadyReacting)
	{
		if (!Sprite || !Hurt || (bAlreadyReacting && Sprite->GetFlipbook() == Hurt)) return;
		Sprite->SetFlipbook(Hurt);
		Sprite->SetLooping(false);
		Sprite->PlayFromStart();
	}
}
