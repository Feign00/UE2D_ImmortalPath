#include "Misc/AutomationTest.h"

// Source rectangles and editable pivots are not part of the cooked runtime API.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "Materials/MaterialInterface.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalDesktopPixelPreviewAssetsTest,
	"ImmortalPath.Art.DesktopPixelPreviewAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalDesktopPixelPreviewAssetsTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Name : { TEXT("Idle"), TEXT("Move") })
	{
		const FString AssetName = FString::Printf(TEXT("FB_Player_Pixel_%s"), Name);
		UPaperFlipbook* Clip = LoadObject<UPaperFlipbook>(nullptr,
			*FString::Printf(TEXT("/Game/GAME/Asset/Player/desktop_pixel_v2/%s.%s"), *AssetName, *AssetName));
		if (!TestNotNull(AssetName, Clip)) continue;
		TestEqual(TEXT("Eight separately drawn frames"), Clip->GetNumKeyFrames(), 8);
		TestEqual(TEXT("Preview timing is eight FPS"), Clip->GetFramesPerSecond(), 8.0f);
		for (int32 Frame = 0; Frame < Clip->GetNumKeyFrames(); ++Frame)
		{
			const UPaperSprite* Sprite = Clip->GetKeyFrameChecked(Frame).Sprite;
			if (!TestNotNull(TEXT("Sprite exists"), Sprite)) continue;
			TestTrue(TEXT("Uniform cell"), Sprite->GetSourceSize().Equals(FVector2D(384, 512)));
			TestTrue(TEXT("Atlas-local foot pivot"), (Sprite->GetPivotPosition() - Sprite->GetSourceUV()).Equals(FVector2D(176, 464)));
			TestTrue(TEXT("Uniform physical scale"), FMath::IsNearlyEqual(Sprite->GetPixelsPerUnrealUnit(), 2.56f));
			if (TestNotNull(TEXT("Material exists"), Sprite->GetDefaultMaterial()))
				TestEqual(TEXT("Hard masked desktop edges"), Sprite->GetDefaultMaterial()->GetBlendMode(), BLEND_Masked);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalDesktopPixelActionAssetsTest,
	"ImmortalPath.Art.DesktopPixelActionAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalDesktopPixelActionAssetsTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Name : { TEXT("Attack"), TEXT("Hurt"), TEXT("Death") })
	{
		const FString AssetName = FString::Printf(TEXT("FB_Player_Pixel_%s"), Name);
		UPaperFlipbook* Clip = LoadObject<UPaperFlipbook>(nullptr,
			*FString::Printf(TEXT("/Game/GAME/Asset/Player/desktop_pixel_v2/%s.%s"), *AssetName, *AssetName));
		if (!TestNotNull(AssetName, Clip)) continue;
		TestEqual(TEXT("Eight distinct action frames"), Clip->GetNumKeyFrames(), 8);
		TestEqual(TEXT("Authored action speed"), Clip->GetFramesPerSecond(), FString(Name) == TEXT("Death") ? 10.0f : 12.0f);
		TSet<const UPaperSprite*> UniqueSprites;
		for (int32 Frame = 0; Frame < Clip->GetNumKeyFrames(); ++Frame)
		{
			const UPaperSprite* Sprite = Clip->GetKeyFrameChecked(Frame).Sprite;
			if (!TestNotNull(TEXT("Sprite exists"), Sprite)) continue;
			UniqueSprites.Add(Sprite);
			TestEqual(TEXT("No repeated frame runs"), Clip->GetKeyFrameChecked(Frame).FrameRun, 1);
			TestTrue(TEXT("Wide canvas keeps the extended sword"), Sprite->GetSourceSize().Equals(FVector2D(576, 512)));
			TestTrue(TEXT("Atlas-local ground pivot"), (Sprite->GetPivotPosition() - Sprite->GetSourceUV()).Equals(FVector2D(220, 464)));
			TestTrue(TEXT("Same world density as Idle/Move"), FMath::IsNearlyEqual(Sprite->GetPixelsPerUnrealUnit(), 2.56f));
			if (TestNotNull(TEXT("Material exists"), Sprite->GetDefaultMaterial()))
				TestEqual(TEXT("Masked desktop edges"), Sprite->GetDefaultMaterial()->GetBlendMode(), BLEND_Masked);
		}
		TestEqual(TEXT("Every pose is a separate sprite"), UniqueSprites.Num(), 8);
	}
	return true;
}

#endif
