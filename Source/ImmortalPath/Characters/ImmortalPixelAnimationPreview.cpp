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
	const bool bActions = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestPixelPlayerActions"));
	if (!bActions && !FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestPixelPlayerPreview"))) return;
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
	if (bActions)
	{
		UPaperFlipbookComponent* Rest = CreatePreview(Idle, 250.0f);
		int32 ActionIndex = 0;
		for (const TCHAR* Name : { TEXT("Attack"), TEXT("Hurt"), TEXT("Death") })
		{
			const FString AssetName = FString::Printf(TEXT("FB_Player_Pixel_%s"), Name);
			UPaperFlipbook* Clip = LoadObject<UPaperFlipbook>(nullptr,
				*FString::Printf(TEXT("/Game/GAME/Asset/Player/desktop_pixel_v2/%s.%s"), *AssetName, *AssetName));
			if (!Clip)
			{
				UE_LOG(LogTemp, Error, TEXT("Pixel actions: missing %s"), *AssetName);
				FPlatformMisc::RequestExit(false);
				return;
			}
			// Every loaded clip remains owned by a registered component during the fixture.
			UPaperFlipbookComponent* Action = CreatePreview(Clip, 250.0f);
			Action->SetLooping(false); Action->Stop(); Action->SetVisibility(false);
			const float StartTime = 1.0f + ActionIndex * 2.0f;
			FTimerHandle StartTimer;
			Player.GetWorldTimerManager().SetTimer(StartTimer,
				FTimerDelegate::CreateWeakLambda(&Player, [Action, Rest]
				{
					Rest->SetVisibility(false); Action->SetVisibility(true); Action->PlayFromStart();
					UE_LOG(LogTemp, Display, TEXT("Pixel action started: %s"), *Action->GetFlipbook()->GetName());
				}), StartTime, false);
			for (int32 Sample = 0; Sample < 3; ++Sample)
			{
				const float Delay = Sample == 0 ? 0.26f : (Sample == 1 ? 0.55f : 0.95f);
				FTimerHandle SampleTimer;
				Player.GetWorldTimerManager().SetTimer(SampleTimer,
					FTimerDelegate::CreateWeakLambda(&Player, [Action, ActionIndex, Sample]
					{
						UE_LOG(LogTemp, Display, TEXT("Pixel action sample: %s sample=%d frame=%d time=%.3f playing=%s"),
							*Action->GetFlipbook()->GetName(), Sample, Action->GetPlaybackPositionInFrames(),
							Action->GetPlaybackPosition(), Action->IsPlaying() ? TEXT("true") : TEXT("false"));
						if (Sample == 2 && (Action->IsPlaying()
							|| Action->GetFlipbook()->GetSpriteAtTime(Action->GetPlaybackPosition(), true)
								!= Action->GetFlipbook()->GetSpriteAtFrame(7)))
							UE_LOG(LogTemp, Error, TEXT("Pixel action did not hold its final sprite."));
						FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),
							FString::Printf(TEXT("Screenshots/PixelAction_%d_%d.png"), ActionIndex, Sample)), true, false);
					}), StartTime + Delay, false);
			}
			if (ActionIndex < 2)
			{
				FTimerHandle RestoreTimer;
				Player.GetWorldTimerManager().SetTimer(RestoreTimer,
					FTimerDelegate::CreateWeakLambda(&Player, [Action, Rest]
					{
						Action->SetVisibility(false); Rest->SetVisibility(true); Rest->PlayFromStart();
						UE_LOG(LogTemp, Display, TEXT("Pixel action returned to Idle at unchanged transform."));
					}), StartTime + 1.05f, false);
			}
			++ActionIndex;
		}
		FTimerHandle ExitTimer;
		Player.GetWorldTimerManager().SetTimer(ExitTimer,
			FTimerDelegate::CreateWeakLambda(&Player, [] { FPlatformMisc::RequestExit(false); }), 7.0f, false);
		return;
	}
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
