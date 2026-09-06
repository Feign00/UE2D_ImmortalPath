#include "Misc/AutomationTest.h"

// Source rectangles and editable pivots are not part of the cooked runtime API.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "PaperFlipbook.h"
#include "PaperSprite.h"

namespace
{
	struct FMortalPlayerFlipbookExpectation
	{
		const TCHAR* Path = nullptr;
		int32 Frames = 0;
		float FramesPerSecond = 0.0f;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalMortalPlayerAnimationSetTest,
	"ImmortalPath.Art.MortalPlayerAnimationSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalMortalPlayerAnimationSetTest::RunTest(const FString& Parameters)
{
	const TArray<FMortalPlayerFlipbookExpectation> Expectations = {
		{TEXT("/Game/GAME/Asset/Player/mortal/generated/FB_Player_Mortal_Idle.FB_Player_Mortal_Idle"), 8, 8.0f},
		{TEXT("/Game/GAME/Asset/Player/mortal/generated/FB_Player_Mortal_Move.FB_Player_Mortal_Move"), 8, 10.0f},
		{TEXT("/Game/GAME/Asset/Player/mortal/generated/FB_Player_Mortal_Attack.FB_Player_Mortal_Attack"), 8, 12.0f},
		{TEXT("/Game/GAME/Asset/Player/mortal/generated/FB_Player_Mortal_Hurt.FB_Player_Mortal_Hurt"), 6, 12.0f},
		{TEXT("/Game/GAME/Asset/Player/mortal/generated/FB_Player_Mortal_Death.FB_Player_Mortal_Death"), 8, 10.0f},
	};

	for (const FMortalPlayerFlipbookExpectation& Expectation : Expectations)
	{
		UPaperFlipbook* Flipbook = LoadObject<UPaperFlipbook>(nullptr, Expectation.Path);
		if (!TestNotNull(FString::Printf(TEXT("Flipbook loads: %s"), Expectation.Path), Flipbook))
		{
			continue;
		}

		TestEqual(
			FString::Printf(TEXT("Frame count: %s"), Expectation.Path),
			Flipbook->GetNumKeyFrames(),
			Expectation.Frames);
		TestTrue(
			FString::Printf(TEXT("Frame rate: %s"), Expectation.Path),
			FMath::IsNearlyEqual(Flipbook->GetFramesPerSecond(), Expectation.FramesPerSecond));

		for (int32 FrameIndex = 0; FrameIndex < Flipbook->GetNumKeyFrames(); ++FrameIndex)
		{
			const UPaperSprite* Sprite = Flipbook->GetKeyFrameChecked(FrameIndex).Sprite;
			if (!TestNotNull(
				FString::Printf(TEXT("Frame %d has a sprite: %s"), FrameIndex, Expectation.Path),
				Sprite))
			{
				continue;
			}

			const FVector2D SourceSize = Sprite->GetSourceSize();
			const FVector2D Pivot = Sprite->GetPivotPosition();
			TestTrue(
				FString::Printf(TEXT("Frame %d source is 512x512: %s"), FrameIndex, Expectation.Path),
				SourceSize.Equals(FVector2D(512.0, 512.0), 0.01));
			TestTrue(
				FString::Printf(TEXT("Frame %d uses shared foot pivot: %s"), FrameIndex, Expectation.Path),
				Pivot.Equals(FVector2D(256.0, 448.0), 0.01));
		}
	}

	UPaperFlipbook* AscensionFlipbook = LoadObject<UPaperFlipbook>(
		nullptr,
		TEXT("/Game/GAME/Asset/Player/ascension/generated/FB_Player_Ascension.FB_Player_Ascension"));
	if (TestNotNull(TEXT("Existing ascension Flipbook remains available"), AscensionFlipbook))
	{
		TestEqual(TEXT("Ascension remains the original 17-frame sequence"), AscensionFlipbook->GetNumKeyFrames(), 17);
	}

	return true;
}

#endif
