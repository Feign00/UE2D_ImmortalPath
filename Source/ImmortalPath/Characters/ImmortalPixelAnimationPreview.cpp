#include "ImmortalPixelAnimationPreview.h"

#if !UE_BUILD_SHIPPING
#include "ImmortalPlayerCharacter.h"
#include "ImmortalMonsterCharacter.h"
#include "../Spawning/ImmortalMonsterSpawner.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#endif

void ImmortalPixelAnimationPreview::StartIfRequested(AImmortalPlayerCharacter& Player)
{
#if !UE_BUILD_SHIPPING
	if (!FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestPixelPlayerPreview"))) return;
	FString UserDirectory;
	const FString FixtureRoot = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/Automation/PixelPreview")));
	if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirectory)
		|| !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(UserDirectory), FixtureRoot))
	{
		UE_LOG(LogTemp, Error, TEXT("Pixel preview refused: use an isolated UserDir under Saved/Automation/PixelPreview."));
		return;
	}
	UPaperFlipbook* Idle = LoadObject<UPaperFlipbook>(nullptr,
		TEXT("/Game/GAME/Asset/Player/desktop_pixel_v2/FB_Player_Pixel_Idle.FB_Player_Pixel_Idle"));
	UPaperFlipbook* Move = LoadObject<UPaperFlipbook>(nullptr,
		TEXT("/Game/GAME/Asset/Player/desktop_pixel_v2/FB_Player_Pixel_Move.FB_Player_Pixel_Move"));
	if (!Idle || !Move || !Player.GetSprite())
	{
		UE_LOG(LogTemp, Error, TEXT("Pixel preview refused: required assets are missing."));
		FPlatformMisc::RequestExit(false);
		return;
	}
	Player.StopAutoAttack();
	Player.SetCanBeDamaged(false);
	Player.GetCharacterMovement()->StopMovementImmediately();
	Player.GetCharacterMovement()->DisableMovement();
	Player.SetActorTickEnabled(false);
	for (TActorIterator<AImmortalMonsterSpawner> It(Player.GetWorld()); It; ++It) It->StopSpawning();
	for (TActorIterator<AImmortalMonsterCharacter> It(Player.GetWorld()); It; ++It)
	{
		It->SetActorHiddenInGame(true);
		It->SetActorEnableCollision(false);
		It->SetActorTickEnabled(false);
	}
	Player.GetSprite()->SetVisibility(false);
	auto CreatePreview = [&Player](UPaperFlipbook* Clip, float OffsetX)
	{
		UPaperFlipbookComponent* Component = NewObject<UPaperFlipbookComponent>(&Player);
		Player.AddInstanceComponent(Component);
		Component->SetupAttachment(Player.GetRootComponent());
		Component->SetRelativeTransform(Player.GetSprite()->GetRelativeTransform());
		Component->AddLocalOffset(FVector(OffsetX, 0.0f, 0.0f));
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetFlipbook(Clip);
		Component->SetLooping(true);
		Component->RegisterComponent();
		Component->PlayFromStart();
		return Component;
	};
	UPaperFlipbookComponent* Left = CreatePreview(Idle, 0.0f);
	UPaperFlipbookComponent* Right = CreatePreview(Move, 250.0f);
	// Both clips remain referenced by registered components; timers are weak-owner bound.
	for (int32 Index = 0; Index < 12; ++Index)
	{
		FTimerHandle SampleTimer;
		Player.GetWorldTimerManager().SetTimer(SampleTimer,
			FTimerDelegate::CreateWeakLambda(&Player, [Left, Right, Index]
			{
				UE_LOG(LogTemp, Display, TEXT("Pixel preview sample=%d left=%s frame=%d time=%.3f right=%s frame=%d time=%.3f"),
					Index, *Left->GetFlipbook()->GetName(), Left->GetPlaybackPositionInFrames(), Left->GetPlaybackPosition(),
					*Right->GetFlipbook()->GetName(), Right->GetPlaybackPositionInFrames(), Right->GetPlaybackPosition());
				if (Index == 0 || Index == 4 || Index == 8 || Index == 11)
					FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),
						FString::Printf(TEXT("Screenshots/PixelPreview_%02d.png"), Index)), true, false);
			}), 0.6f + Index * 0.27f, false);
	}
	FTimerHandle SwapTimer;
	Player.GetWorldTimerManager().SetTimer(SwapTimer,
		FTimerDelegate::CreateWeakLambda(&Player, [Left, Right]
		{
			UPaperFlipbook* Previous = Left->GetFlipbook();
			Left->SetFlipbook(Right->GetFlipbook()); Right->SetFlipbook(Previous);
			Left->PlayFromStart(); Right->PlayFromStart();
			UE_LOG(LogTemp, Display, TEXT("Pixel preview: swapped Idle/Move at fixed transforms."));
		}), 2.0f, false);
	FTimerHandle ExitTimer;
	Player.GetWorldTimerManager().SetTimer(ExitTimer,
		FTimerDelegate::CreateWeakLambda(&Player, [] { FPlatformMisc::RequestExit(false); }), 4.5f, false);
#endif
}
