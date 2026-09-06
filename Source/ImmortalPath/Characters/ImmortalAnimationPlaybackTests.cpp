#include "ImmortalAnimationPlayback.h"
#include "Misc/AutomationTest.h"
#include "PaperFlipbook.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalAnimationSpeedTest,
	"ImmortalPath.Animation.MovementThreshold", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalAnimationSpeedTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Stationary is idle"), ImmortalAnimationPlayback::IsMoving(0));
	TestFalse(TEXT("Small physics jitter is idle"), ImmortalAnimationPlayback::IsMoving(5));
	TestTrue(TEXT("Right movement animates"), ImmortalAnimationPlayback::IsMoving(6));
	TestTrue(TEXT("Left movement animates"), ImmortalAnimationPlayback::IsMoving(-6));
	ImmortalAnimationPlayback::UpdateLocomotion(nullptr, nullptr, nullptr, false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalAnimationLocomotionTest,
	"ImmortalPath.Animation.LocomotionContinuity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalAnimationLocomotionTest::RunTest(const FString& Parameters)
{
	UPaperFlipbook* Idle = LoadObject<UPaperFlipbook>(nullptr,
		TEXT("/Game/GAME/Asset/Player/mortal/generated/FB_Player_Mortal_Idle.FB_Player_Mortal_Idle"));
	UPaperFlipbook* Move = LoadObject<UPaperFlipbook>(nullptr,
		TEXT("/Game/GAME/Asset/Player/mortal/generated/FB_Player_Mortal_Move.FB_Player_Mortal_Move"));
	if (!TestNotNull(TEXT("Idle asset"), Idle) || !TestNotNull(TEXT("Move asset"), Move)) return false;
	UPaperFlipbookComponent* Sprite = NewObject<UPaperFlipbookComponent>();
	ImmortalAnimationPlayback::UpdateLocomotion(Sprite, Idle, Move, true);
	TestTrue(TEXT("Moving selects running loop"), Sprite->GetFlipbook() == Move && Sprite->IsLooping() && Sprite->IsPlaying());
	Sprite->SetPlaybackPosition(0.2f, false);
	ImmortalAnimationPlayback::UpdateLocomotion(Sprite, Idle, Move, true);
	TestEqual(TEXT("Repeated update preserves phase"), Sprite->GetPlaybackPosition(), 0.2f);
	ImmortalAnimationPlayback::UpdateLocomotion(Sprite, Idle, Move, false);
	TestTrue(TEXT("Stopping selects authored idle"), Sprite->GetFlipbook() == Idle && Sprite->IsLooping() && Sprite->IsPlaying());
	Sprite->SetPlaybackPosition(0.2f, false);
	ImmortalAnimationPlayback::UpdateLocomotion(Sprite, Idle, Move, false);
	TestEqual(TEXT("Idle loop does not restart each update"), Sprite->GetPlaybackPosition(), 0.2f);
	ImmortalAnimationPlayback::UpdateLocomotion(Sprite, nullptr, Move, false);
	TestTrue(TEXT("Missing idle holds move pose without running"), Sprite->GetFlipbook() == Move && !Sprite->IsPlaying() && !Sprite->IsLooping());
	TestEqual(TEXT("Fallback uses stable first pose"), Sprite->GetPlaybackPosition(), 0.0f);
	ImmortalAnimationPlayback::UpdateLocomotion(Sprite, nullptr, Move, true);
	TestTrue(TEXT("Movement resumes after held pose"), Sprite->IsPlaying() && Sprite->IsLooping());
	ImmortalAnimationPlayback::UpdateLocomotion(Sprite, Idle, nullptr, true);
	TestFalse(TEXT("Missing move cannot play idle as running"), Sprite->IsPlaying());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalAnimationHurtTest,
	"ImmortalPath.Animation.RepeatedHurtContinuity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalAnimationHurtTest::RunTest(const FString& Parameters)
{
	UPaperFlipbook* Hurt = LoadObject<UPaperFlipbook>(nullptr,
		TEXT("/Game/GAME/Asset/Player/mortal/generated/FB_Player_Mortal_Hurt.FB_Player_Mortal_Hurt"));
	if (!TestNotNull(TEXT("Hurt asset"), Hurt)) return false;
	UPaperFlipbookComponent* Sprite = NewObject<UPaperFlipbookComponent>();
	ImmortalAnimationPlayback::PlayHurtWithoutRestart(Sprite, Hurt, false);
	TestTrue(TEXT("Initial hurt plays once"), Sprite->IsPlaying() && !Sprite->IsLooping());
	Sprite->SetPlaybackPosition(0.2f, false);
	ImmortalAnimationPlayback::PlayHurtWithoutRestart(Sprite, Hurt, true);
	TestEqual(TEXT("Repeated damage preserves hurt frame"), Sprite->GetPlaybackPosition(), 0.2f);
	Sprite->Stop();
	ImmortalAnimationPlayback::PlayHurtWithoutRestart(Sprite, Hurt, true);
	TestFalse(TEXT("Extended reaction does not replay completed clip"), Sprite->IsPlaying());
	ImmortalAnimationPlayback::PlayHurtWithoutRestart(Sprite, Hurt, false);
	TestTrue(TEXT("A new reaction can start after recovery"), Sprite->IsPlaying());
	TestEqual(TEXT("New reaction begins at first frame"), Sprite->GetPlaybackPosition(), 0.0f);
	return true;
}

#endif
