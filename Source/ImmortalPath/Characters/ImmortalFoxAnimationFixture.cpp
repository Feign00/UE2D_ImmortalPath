#include "ImmortalFoxAnimationFixture.h"
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "ImmortalMonsterCharacter.h"
#include "ImmortalPetCharacter.h"
#include "ImmortalPlayerCharacter.h"
#include "../Spawning/ImmortalMonsterSpawner.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/ObjectKey.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"

namespace
{
	bool bFoxFixtureClaimed = false;

	bool GetSafeDirectory(FString& Directory)
	{
		const bool bFlying = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestFoxArt"));
		const bool bWalking = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestFoxWalking"));
		const bool bDisabled = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestDisableAutoBattle"));
		if (bFlying == bWalking || (bWalking ? bDisabled : !bDisabled)) return false;
		FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectDir(), TEXT("Saved/Automation/FoxArt")));
		if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), Directory) || Directory.IsEmpty()) return false;
		Directory = FPaths::ConvertRelativePathToFull(Directory);
		FPaths::NormalizeDirectoryName(Directory);
		FPaths::NormalizeDirectoryName(Root);
		return FPaths::CollapseRelativeDirectories(Directory) && FPaths::CollapseRelativeDirectories(Root)
			&& FPaths::IsUnderDirectory(Directory, Root)
			&& !Directory.Equals(Root, ESearchCase::IgnoreCase);
	}

	struct FFoxWalkingActorStats
	{
		FString Name;
		int32 Samples = 0;
		int32 GroundedSamples = 0;
		int32 StableGroundStreak = 0;
		int32 MaxStableGroundStreak = 0;
		int32 StableMoveStreak = 0;
		int32 MaxStableMoveStreak = 0;
		int32 MoveSamples = 0;
		int32 MoveAdvanceSamples = 0;
		float LastMovePosition = -1.0f;
		int32 LocomotionSamples = 0;
		int32 LocomotionAdvanceSamples = 0;
		TSet<int32> LocomotionFramesSeen;
		int32 StableLocomotionStreak = 0;
		int32 MaxStableLocomotionStreak = 0;
		const UPaperFlipbook* LastLocomotionClip = nullptr;
		float LastLocomotionPosition = -1.0f;
		int32 FootMismatchStreak = 0;
		int32 MaxFootMismatchStreak = 0;
		int32 VisualAlignedStreak = 0;
		int32 MaxVisualAlignedStreak = 0;
		int32 VisibleStreak = 0;
		int32 MaxVisibleStreak = 0;
		int32 NewAnimationSamples = 0;
		int32 FollowSamples = 0;
		float LargestFootError = 0.0f;
		float LargestDepthError = 0.0f;
		float NextLogTime = 0.0f;
	};

	struct FFoxVisualProbe
	{
		float CapsuleBottomZ = 0.0f;
		float ReferenceGroundOffset = 0.0f;
		float ExpectedFootZ = 0.0f;
		float FootError = BIG_NUMBER;
		float DepthError = BIG_NUMBER;
		FVector FootWorld = FVector::ZeroVector;
		FVector Scale = FVector::ZeroVector;
		FVector BoundsSize = FVector::ZeroVector;
		FVector2D ScreenFoot = FVector2D(-1, -1);
		FVector2D ScreenBoundsSize = FVector2D::ZeroVector;
		bool bFootProjected = false;
		bool bBoundsProjected = false;
	};

	FFoxVisualProbe MeasureVisual(const APaperCharacter& Actor, const AImmortalPlayerCharacter& Player)
	{
		FFoxVisualProbe Result;
		const UPaperFlipbookComponent* Sprite = Actor.GetSprite();
		const UCapsuleComponent* Capsule = Actor.GetCapsuleComponent();
		const UCapsuleComponent* PlayerCapsule = Player.GetCapsuleComponent();
		if (!Sprite || !Capsule || !Player.GetSprite() || !PlayerCapsule) return Result;
		Result.CapsuleBottomZ = Capsule->GetComponentLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
		// Calculate independently from runtime placement so a helper regression cannot make the assertion self-fulfilling.
		Result.ReferenceGroundOffset = Player.GetSprite()->GetComponentLocation().Z
			- (PlayerCapsule->GetComponentLocation().Z - PlayerCapsule->GetScaledCapsuleHalfHeight());
		Result.ExpectedFootZ = Result.CapsuleBottomZ + Result.ReferenceGroundOffset;
		Result.FootWorld = Sprite->GetComponentLocation();
		Result.FootError = FMath::Abs(Result.FootWorld.Z - Result.ExpectedFootZ);
		Result.DepthError = FMath::Abs(Result.FootWorld.Y - Player.GetSprite()->GetComponentLocation().Y);
		Result.Scale = Sprite->GetComponentScale();
		const FBoxSphereBounds Bounds = Sprite->CalcBounds(Sprite->GetComponentTransform());
		Result.BoundsSize = Bounds.BoxExtent * 2.0;
		const APlayerController* Controller = Cast<APlayerController>(Player.GetController());
		if (!Controller) return Result;
		Result.bFootProjected = UGameplayStatics::ProjectWorldToScreen(Controller, Result.FootWorld, Result.ScreenFoot, false);
		FVector2D Minimum(BIG_NUMBER, BIG_NUMBER);
		FVector2D Maximum(-BIG_NUMBER, -BIG_NUMBER);
		Result.bBoundsProjected = true;
		for (const float XSign : {-1.0f, 1.0f})
			for (const float ZSign : {-1.0f, 1.0f})
			{
				FVector2D Screen;
				const FVector Corner = Bounds.Origin + FVector(XSign * Bounds.BoxExtent.X, 0, ZSign * Bounds.BoxExtent.Z);
				if (!UGameplayStatics::ProjectWorldToScreen(Controller, Corner, Screen, false))
				{
					Result.bBoundsProjected = false;
					continue;
				}
				Minimum.X = FMath::Min(Minimum.X, Screen.X);
				Minimum.Y = FMath::Min(Minimum.Y, Screen.Y);
				Maximum.X = FMath::Max(Maximum.X, Screen.X);
				Maximum.Y = FMath::Max(Maximum.Y, Screen.Y);
			}
		if (Result.bBoundsProjected) Result.ScreenBoundsSize = Maximum - Minimum;
		return Result;
	}

	void LogVisual(const APaperCharacter& Actor, const AImmortalPlayerCharacter& Player, const TCHAR* Context)
	{
		const FFoxVisualProbe Probe = MeasureVisual(Actor, Player);
		const UPaperFlipbookComponent* Sprite = Actor.GetSprite();
		const UCharacterMovementComponent* Movement = Actor.GetCharacterMovement();
		const APlayerController* Controller = Cast<APlayerController>(Player.GetController());
		FVector CameraLocation = FVector::ZeroVector;
		FRotator CameraRotation = FRotator::ZeroRotator;
		if (Controller) Controller->GetPlayerViewPoint(CameraLocation, CameraRotation);
		UE_LOG(LogTemp, Display, TEXT("R07FoxVisual context=%s actor=%s actorLocation=%s capsuleBottomZ=%.3f referenceGroundOffset=%.3f expectedFootZ=%.3f spriteRelative=%s spriteFootWorld=%s footError=%.3f depthError=%.3f spriteWorldScale=%s worldBoundsSize=%s screenFoot=%s footProjected=%s screenBoundsSize=%s boundsProjected=%s mode=%d vx=%.2f maxWalkSpeed=%.2f maxFlySpeed=%.2f pendingInput=%s cameraLocation=%s cameraRotation=%s cameraFOV=%.2f"),
			Context, *Actor.GetName(), *Actor.GetActorLocation().ToString(), Probe.CapsuleBottomZ,
			Probe.ReferenceGroundOffset, Probe.ExpectedFootZ, Sprite ? *Sprite->GetRelativeLocation().ToString() : TEXT("none"),
			*Probe.FootWorld.ToString(), Probe.FootError, Probe.DepthError, *Probe.Scale.ToString(), *Probe.BoundsSize.ToString(),
			*Probe.ScreenFoot.ToString(), Probe.bFootProjected ? TEXT("true") : TEXT("false"),
			*Probe.ScreenBoundsSize.ToString(), Probe.bBoundsProjected ? TEXT("true") : TEXT("false"),
			Movement ? static_cast<int32>(Movement->MovementMode) : -1, Actor.GetVelocity().X,
			Movement ? Movement->MaxWalkSpeed : -1.0f, Movement ? Movement->MaxFlySpeed : -1.0f,
			*Actor.GetPendingMovementInputVector().ToString(), *CameraLocation.ToString(), *CameraRotation.ToString(),
			Controller && Controller->PlayerCameraManager ? Controller->PlayerCameraManager->GetFOVAngle() : -1.0f);
	}

	struct FFoxWalkingState
	{
		TWeakObjectPtr<AImmortalPlayerCharacter> Player;
		TMap<FObjectKey, FFoxWalkingActorStats> FoxStats;
		TWeakObjectPtr<AImmortalPetCharacter> ObservedPet;
		FFoxWalkingActorStats PetStats;
		FFoxWalkingActorStats PlayerStats;
		FString Directory;
		int32 Samples = 0;
		int32 JointVisibleSamples = 0;
		int32 PausedSamples = 0;
		int32 Checks = 0;
		int32 Failures = 0;
		int32 OwnerDeadSamples = 0;
		FTimerHandle SampleTimer;

		void Check(const bool bPassed, const TCHAR* Label)
		{
			++Checks;
			if (!bPassed) ++Failures;
			UE_LOG(LogTemp, Display, TEXT("R07FoxWalking check %02d: %s | %s"),
				Checks, bPassed ? TEXT("PASS") : TEXT("FAIL"), Label);
		}
	};

	bool SpriteInsideViewport(const APaperCharacter& Actor, const APlayerController* Controller)
	{
		const UPaperFlipbookComponent* Sprite = Actor.GetSprite();
		if (!Sprite || !Sprite->IsVisible() || Actor.IsHidden() || !Controller) return false;
		int32 Width = 0;
		int32 Height = 0;
		Controller->GetViewportSize(Width, Height);
		if (Width <= 0 || Height <= 0) return false;
		const FBoxSphereBounds Bounds = Sprite->CalcBounds(Sprite->GetComponentTransform());
		for (const float XSign : {-1.0f, 1.0f})
			for (const float ZSign : {-1.0f, 1.0f})
			{
				const FVector Corner = Bounds.Origin + FVector(XSign * Bounds.BoxExtent.X, 0, ZSign * Bounds.BoxExtent.Z);
				FVector2D Screen;
				if (!UGameplayStatics::ProjectWorldToScreen(Controller, Corner, Screen, false)
					|| Screen.X < 0 || Screen.Y < 0 || Screen.X > Width || Screen.Y > Height) return false;
			}
		return true;
	}

	bool SampleWalkingActor(APaperCharacter& Actor, AImmortalPlayerCharacter& Player,
		FFoxWalkingActorStats& Stats, const TCHAR* Role, const bool bCheckFoot, const bool bPet)
	{
		UCharacterMovementComponent* Movement = Actor.GetCharacterMovement();
		UPaperFlipbookComponent* Sprite = Actor.GetSprite();
		const float Now = Player.GetWorld()->GetTimeSeconds();
		Stats.Name = Actor.GetName();
		++Stats.Samples;
		const bool bWalking = Movement && (Movement->MovementMode == MOVE_Walking || Movement->MovementMode == MOVE_NavWalking);
		const bool bGrounded = bWalking && Movement->IsMovingOnGround() && Movement->CurrentFloor.IsWalkableFloor();
		Stats.GroundedSamples += bGrounded ? 1 : 0;
		Stats.StableGroundStreak = bGrounded ? Stats.StableGroundStreak + 1 : 0;
		Stats.MaxStableGroundStreak = FMath::Max(Stats.MaxStableGroundStreak, Stats.StableGroundStreak);
		const FString ClipName = Sprite && Sprite->GetFlipbook() ? Sprite->GetFlipbook()->GetPathName() : TEXT("none");
		const bool bNewAnimation = ClipName.StartsWith(TEXT("/Game/GAME/Asset/Monsters/desktop_pixel_v2/fox/FB_Fox_Pixel_"));
		Stats.NewAnimationSamples += bNewAnimation ? 1 : 0;
		const bool bMoving = bGrounded && bNewAnimation && FMath::Abs(Actor.GetVelocity().X) > 5.0f
			&& ClipName.EndsWith(TEXT("FB_Fox_Pixel_Move")) && Sprite->IsPlaying() && Sprite->IsLooping();
		Stats.MoveSamples += bMoving ? 1 : 0;
		if (bMoving)
		{
			const float Position = Sprite->GetPlaybackPosition();
			if (Stats.LastMovePosition >= 0 && !FMath::IsNearlyEqual(Position, Stats.LastMovePosition, 0.001f))
				++Stats.MoveAdvanceSamples;
			Stats.LastMovePosition = Position;
		}
		else Stats.LastMovePosition = -1.0f;
		Stats.StableMoveStreak = bMoving ? Stats.StableMoveStreak + 1 : 0;
		Stats.MaxStableMoveStreak = FMath::Max(Stats.MaxStableMoveStreak, Stats.StableMoveStreak);
		const bool bLocomotionPlaying = bGrounded && bNewAnimation && Sprite->IsPlaying() && Sprite->IsLooping()
			&& (ClipName.EndsWith(TEXT("FB_Fox_Pixel_Idle")) || ClipName.EndsWith(TEXT("FB_Fox_Pixel_Move")));
		Stats.LocomotionSamples += bLocomotionPlaying ? 1 : 0;
		if (bLocomotionPlaying)
		{
			Stats.LocomotionFramesSeen.Add(Sprite->GetPlaybackPositionInFrames()
				+ (ClipName.EndsWith(TEXT("FB_Fox_Pixel_Move")) ? 8 : 0));
			const float Position = Sprite->GetPlaybackPosition();
			if (Stats.LastLocomotionClip == Sprite->GetFlipbook() && Stats.LastLocomotionPosition >= 0
				&& !FMath::IsNearlyEqual(Position, Stats.LastLocomotionPosition, 0.001f)) ++Stats.LocomotionAdvanceSamples;
			Stats.LastLocomotionPosition = Position;
			Stats.LastLocomotionClip = Sprite->GetFlipbook();
		}
		else
		{
			Stats.LastLocomotionPosition = -1.0f;
			Stats.LastLocomotionClip = nullptr;
		}
		Stats.StableLocomotionStreak = bLocomotionPlaying ? Stats.StableLocomotionStreak + 1 : 0;
		Stats.MaxStableLocomotionStreak = FMath::Max(Stats.MaxStableLocomotionStreak, Stats.StableLocomotionStreak);
		const FFoxVisualProbe Probe = MeasureVisual(Actor, Player);
		const float ExpectedScale = bPet ? 1.25f : 2.0f;
		const bool bVisualAligned = Probe.FootError <= 1.5f && Probe.DepthError <= 1.5f
			&& FMath::IsNearlyEqual(FMath::Abs(Probe.Scale.X), ExpectedScale, 0.01)
			&& FMath::IsNearlyEqual(Probe.Scale.Z, ExpectedScale, 0.01)
			&& Probe.BoundsSize.X > 0 && Probe.BoundsSize.Z > 0;
		const bool bFootMismatch = bCheckFoot && bGrounded && !bVisualAligned;
		Stats.FootMismatchStreak = bFootMismatch ? Stats.FootMismatchStreak + 1 : 0;
		Stats.MaxFootMismatchStreak = FMath::Max(Stats.MaxFootMismatchStreak, Stats.FootMismatchStreak);
		Stats.VisualAlignedStreak = bCheckFoot && bGrounded && bVisualAligned ? Stats.VisualAlignedStreak + 1 : 0;
		Stats.MaxVisualAlignedStreak = FMath::Max(Stats.MaxVisualAlignedStreak, Stats.VisualAlignedStreak);
		if (bCheckFoot && bGrounded)
		{
			Stats.LargestFootError = FMath::Max(Stats.LargestFootError, Probe.FootError);
			Stats.LargestDepthError = FMath::Max(Stats.LargestDepthError, Probe.DepthError);
		}
		const bool bVisible = SpriteInsideViewport(Actor, Cast<APlayerController>(Player.GetController()));
		Stats.VisibleStreak = bVisible ? Stats.VisibleStreak + 1 : 0;
		Stats.MaxVisibleStreak = FMath::Max(Stats.MaxVisibleStreak, Stats.VisibleStreak);
		float OwnerDistance = 0.0f;
		if (bPet)
		{
			FImmortalPetDefinition Definition;
			UImmortalPetLibrary::GetPetDefinition(TEXT("SpiritFox"), Definition);
			OwnerDistance = FVector::Dist2D(Actor.GetActorLocation(), Player.GetActorLocation());
			if (bGrounded && Actor.GetOwner() == &Player && OwnerDistance <= FMath::Max(Definition.MaximumLeashDistance, 100.0f))
				++Stats.FollowSamples;
		}
		if (Now >= Stats.NextLogTime)
		{
			Stats.NextLogTime = Now + 1.0f;
			UE_LOG(LogTemp, Display, TEXT("R07FoxWalking sample: role=%s actor=%s mode=%d grounded=%s floorDistance=%.2f vx=%.2f spriteFootError=%.3f spriteDepthError=%.3f visualAligned=%s viewportVisible=%s ownerDistance=%.1f clip=%s frame=%d playing=%s"),
				Role, *Stats.Name, Movement ? static_cast<int32>(Movement->MovementMode) : -1,
				bGrounded ? TEXT("true") : TEXT("false"), Movement ? Movement->CurrentFloor.FloorDist : -1.0f,
				Actor.GetVelocity().X, Probe.FootError, Probe.DepthError, bVisualAligned ? TEXT("true") : TEXT("false"),
				bVisible ? TEXT("true") : TEXT("false"), OwnerDistance,
				*ClipName, Sprite ? Sprite->GetPlaybackPositionInFrames() : -1,
				Sprite && Sprite->IsPlaying() ? TEXT("true") : TEXT("false"));
			LogVisual(Actor, Player, Role);
		}
		return bVisible;
	}

	void StartWalkingObserver(AImmortalPlayerCharacter& Player, const FString& Directory)
	{
		const TSharedRef<FFoxWalkingState> State = MakeShared<FFoxWalkingState>();
		State->Player = &Player;
		State->Directory = Directory;
		State->Check(Player.GetWorld() && Player.GetWorld()->WorldType == EWorldType::Game,
			TEXT("Walking observer requires standalone Game world"));
		if (State->Failures)
		{
			FPlatformMisc::RequestExitWithStatus(false, 1);
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("R07FoxWalking started: natural normal-map combat only; no actor spawning, teleport, mode changes, stat changes or battle suppression. First two seconds are settling time."));
		Player.GetWorldTimerManager().SetTimer(State->SampleTimer,
			FTimerDelegate::CreateWeakLambda(&Player, [&Player, State]
			{
				++State->Samples;
				if (UGameplayStatics::IsGamePaused(&Player)) ++State->PausedSamples;
				if (Player.IsDead()) { ++State->OwnerDeadSamples; return; }
				const bool bPlayerVisible = SampleWalkingActor(Player, Player, State->PlayerStats, TEXT("Player"), false, false);
				bool bFoxVisible = false;
				for (TActorIterator<AImmortalMonsterCharacter> It(Player.GetWorld()); It; ++It)
				{
					AImmortalMonsterCharacter* Fox = *It;
					if (Fox->IsDead() || Fox->IsActorBeingDestroyed() || !Cast<AImmortalMonsterSpawner>(Fox->GetOwner())
						|| Fox->GetClass()->GetPathName() != TEXT("/Game/GAME/BP_Monster.BP_Monster_C")) continue;
					FFoxWalkingActorStats& Stats = State->FoxStats.FindOrAdd(FObjectKey(Fox));
					bFoxVisible |= SampleWalkingActor(*Fox, Player, Stats, TEXT("MapFox"), true, false);
				}
				AImmortalPetCharacter* Pet = Player.GetActivePetActor();
				if (Pet && Pet->GetOwner() == &Player && Pet->GetPetId() == TEXT("SpiritFox") && !Pet->IsActorBeingDestroyed())
				{
					if (State->ObservedPet.Get() != Pet)
					{
						State->ObservedPet = Pet;
						State->PetStats = FFoxWalkingActorStats();
					}
					SampleWalkingActor(*Pet, Player, State->PetStats, TEXT("OwnedPet"), true, true);
				}
				if (bPlayerVisible && bFoxVisible) ++State->JointVisibleSamples;
			}), 0.1f, true, 2.0f);
		for (const float Delay : {3.0f, 7.0f, 11.0f})
		{
			FTimerHandle ShotTimer;
			Player.GetWorldTimerManager().SetTimer(ShotTimer,
				FTimerDelegate::CreateWeakLambda(&Player, [State, Delay]
				{
					FScreenshotRequest::RequestScreenshot(FPaths::Combine(State->Directory,
						FString::Printf(TEXT("R07_Fox_Walking_%02d.png"), FMath::RoundToInt(Delay))), true, false);
				}), Delay, false);
		}
		FTimerHandle FinalTimer;
		Player.GetWorldTimerManager().SetTimer(FinalTimer,
			FTimerDelegate::CreateWeakLambda(&Player, [&Player, State]
			{
				Player.GetWorldTimerManager().ClearTimer(State->SampleTimer);
				int32 MaxFoxGround = 0;
				int32 MaxFoxMove = 0;
				int32 MaxFoxVisible = 0;
				int32 FoxMoveAdvances = 0;
				int32 FoxNewAnimationSamples = 0;
				int32 FoxFootMismatch = 0;
				int32 MaxFoxVisualAligned = 0;
				for (const auto& Pair : State->FoxStats)
				{
					const FFoxWalkingActorStats& Stats = Pair.Value;
					MaxFoxGround = FMath::Max(MaxFoxGround, Stats.MaxStableGroundStreak);
					MaxFoxMove = FMath::Max(MaxFoxMove, Stats.MaxStableMoveStreak);
					MaxFoxVisible = FMath::Max(MaxFoxVisible, Stats.MaxVisibleStreak);
					FoxMoveAdvances += Stats.MoveAdvanceSamples;
					FoxNewAnimationSamples += Stats.NewAnimationSamples;
					FoxFootMismatch = FMath::Max(FoxFootMismatch, Stats.MaxFootMismatchStreak);
					MaxFoxVisualAligned = FMath::Max(MaxFoxVisualAligned, Stats.MaxVisualAlignedStreak);
					UE_LOG(LogTemp, Display, TEXT("R07FoxWalking fox summary: actor=%s samples=%d grounded=%d maxGroundStreak=%d moveSamples=%d moveAdvances=%d maxMoveStreak=%d maxVisibleStreak=%d maxVisualMismatchStreak=%d maxVisualAlignedStreak=%d largestFootError=%.3f largestDepthError=%.3f newAnimationSamples=%d"),
						*Stats.Name, Stats.Samples, Stats.GroundedSamples, Stats.MaxStableGroundStreak,
						Stats.MoveSamples, Stats.MoveAdvanceSamples, Stats.MaxStableMoveStreak, Stats.MaxVisibleStreak,
						Stats.MaxFootMismatchStreak, Stats.MaxVisualAlignedStreak, Stats.LargestFootError,
						Stats.LargestDepthError, Stats.NewAnimationSamples);
				}
				const FFoxWalkingActorStats& Pet = State->PetStats;
				UE_LOG(LogTemp, Display, TEXT("R07FoxWalking overall: observedFoxes=%d samples=%d ownerDeadSamples=%d jointVisibleSamples=%d petSamples=%d petGrounded=%d petGroundStreak=%d petNaturalMoveSamples=%d petNaturalMoveAdvances=%d petNaturalMoveStreak=%d petLeashSamples=%d petLocomotionSamples=%d petLocomotionAdvances=%d petLocomotionStreak=%d petVisibleStreak=%d petVisualMismatchStreak=%d petVisualAlignedStreak=%d petLargestFootError=%.3f petLargestDepthError=%.3f"),
					State->FoxStats.Num(), State->Samples, State->OwnerDeadSamples, State->JointVisibleSamples,
					Pet.Samples, Pet.GroundedSamples, Pet.MaxStableGroundStreak, Pet.MoveSamples,
					Pet.MoveAdvanceSamples, Pet.MaxStableMoveStreak, Pet.FollowSamples,
					Pet.LocomotionSamples, Pet.LocomotionAdvanceSamples, Pet.MaxStableLocomotionStreak,
					Pet.MaxVisibleStreak, Pet.MaxFootMismatchStreak, Pet.MaxVisualAlignedStreak,
					Pet.LargestFootError, Pet.LargestDepthError);
				State->Check(State->Samples >= 50 && State->PausedSamples == 0, TEXT("World provided repeated unpaused observation windows; not a whole-run pause audit"));
				State->Check(State->FoxStats.Num() > 0, TEXT("Observer saw naturally spawned actual BP_Monster foxes"));
				State->Check(State->PlayerStats.MaxStableGroundStreak >= 6, TEXT("Natural player repeatedly settled into grounded walking mode"));
				State->Check(MaxFoxGround >= 6, TEXT("At least one real map fox repeatedly uses grounded walking mode"));
				State->Check(MaxFoxMove >= 3 && FoxMoveAdvances >= 2, TEXT("A naturally spawned fox advances its Move clip while walking on the actual map"));
				State->Check(FoxNewAnimationSamples >= 6, TEXT("Normal map fox actors use the new authored animation family"));
				State->Check(FoxFootMismatch < 5 && MaxFoxVisualAligned >= 6,
					TEXT("Map fox repeatedly matches capsule bottom plus player visual ground offset, visual depth and scale two"));
				State->Check(State->JointVisibleSamples >= 5 && MaxFoxVisible >= 5
					&& State->PlayerStats.MaxVisibleStreak >= 5, TEXT("Player and real map fox render bounds repeatedly fit the same viewport"));
				State->Check(Pet.MaxStableGroundStreak >= 6, TEXT("Actual authority-owned SpiritFox repeatedly uses grounded walking mode"));
				// Normal combat leaves less than .6 seconds of Idle between pet
				// attacks. Require actual distinct poses and repeated playback, not
				// an uninterrupted Idle window longer than the real combat cadence.
				State->Check(Pet.FollowSamples >= 6 && Pet.MaxStableLocomotionStreak >= 3
					&& Pet.LocomotionSamples >= 6 && Pet.LocomotionAdvanceSamples >= 4
					&& Pet.LocomotionFramesSeen.Num() >= 3,
					TEXT("Grounded owned pet stays within its leash while real Idle or Move repeatedly plays at least three distinct poses between attacks"));
				UE_LOG(LogTemp, Display, TEXT("R07FoxWalking pet distinct locomotion poses=%d"), Pet.LocomotionFramesSeen.Num());
				State->Check(Pet.NewAnimationSamples >= 6 && Pet.MaxVisibleStreak >= 5,
					TEXT("Real owned pet's new animation and bounds are repeatedly visible"));
				State->Check(Pet.MaxFootMismatchStreak < 5 && Pet.MaxVisualAlignedStreak >= 6,
					TEXT("Owned pet repeatedly matches capsule bottom plus player visual ground offset, visual depth and scale 1.25"));
				UE_LOG(LogTemp, Display, TEXT("R07FoxWalking pet natural Move observation: observed=%s samples=%d advances=%d. Stationary Idle is valid in this observer; controlled fixture separately covers left/right follow. No natural follow movement claim is made when absent."),
					Pet.MoveSamples > 0 ? TEXT("true") : TEXT("false"), Pet.MoveSamples, Pet.MoveAdvanceSamples);
				UE_LOG(LogTemp, Display, TEXT("R07FoxWalking coverage: untouched normal map and combat; grounded actors, actual visual lane placement and source-viewport bounds only. Source screenshots do not validate native desktop compositing. Natural map fox Move remains required; pet Idle or Move playback and grounded leash are required, not forced follow movement."));
				UE_LOG(LogTemp, Display, TEXT("R07FoxWalking finished: checks=%d failures=%d RESULT: %s"),
					State->Checks, State->Failures, State->Failures == 0 ? TEXT("PASS") : TEXT("FAIL"));
				if (State->Failures) UE_LOG(LogTemp, Error, TEXT("R07FoxWalking failed %d normal-map checks"), State->Failures);
				FPlatformMisc::RequestExitWithStatus(false, State->Failures == 0 ? 0 : 1);
			}), 15.0f, false);
	}

	struct FFoxFixtureState
	{
		TWeakObjectPtr<AImmortalPlayerCharacter> Player;
		TWeakObjectPtr<AImmortalMonsterCharacter> Fox;
		TWeakObjectPtr<AImmortalMonsterCharacter> DeathFox;
		TWeakObjectPtr<AImmortalMonsterCharacter> PetTarget;
		TWeakObjectPtr<AImmortalPetCharacter> Pet;
		TArray<TStrongObjectPtr<UPaperFlipbook>> Clips;
		FString Directory;
		FVector Origin = FVector::ZeroVector;
		FVector MoveOrigin = FVector::ZeroVector;
		float IdleLastPosition = -1.0f;
		int32 IdleAdvances = 0;
		int32 IdleWraps = 0;
		float PetIdleLastPosition = -1.0f;
		int32 PetIdleAdvances = 0;
		int32 PetIdleWraps = 0;
		float MonsterAttackStart = -1.0f;
		float MonsterContactTime = -1.0f;
		float LastPlayerHealth = 0.0f;
		int32 MonsterDamageEvents = 0;
		int32 ContactFrame = -1;
		float HurtLastPosition = -1.0f;
		int32 HurtAdvances = 0;
		int32 HurtRegressions = 0;
		float DeathAttackStart = -1.0f;
		bool bFatalInjected = false;
		float HealthAtFatal = 0.0f;
		float PetAttackStart = -1.0f;
		float PetContactTime = -1.0f;
		float LastPetTargetHealth = 0.0f;
		int32 PetDamageEvents = 0;
		int32 PetAttacksBefore = 0;
		int32 PetHitsBefore = 0;
		int32 Checks = 0;
		int32 Failures = 0;
		bool bFinished = false;
		bool bSampleIdle = false;
		bool bSampleHurt = false;
		bool bSampleMonsterAttack = false;
		bool bSampleDeathAttack = false;
		bool bSamplePetIdle = false;
		bool bSamplePetAttack = false;
		FTimerHandle SampleTimer;

		UPaperFlipbook* Clip(const int32 Index) const { return Clips.IsValidIndex(Index) ? Clips[Index].Get() : nullptr; }
		void Check(const bool bCondition, const TCHAR* Label)
		{
			++Checks;
			if (!bCondition) ++Failures;
			UE_LOG(LogTemp, Display, TEXT("R07FoxFixture check %03d: %s | %s"), Checks,
				bCondition ? TEXT("PASS") : TEXT("FAIL"), Label);
		}
		void Finish()
		{
			if (bFinished) return;
			bFinished = true;
			if (AImmortalPlayerCharacter* Owner = Player.Get()) Owner->GetWorldTimerManager().ClearTimer(SampleTimer);
			UE_LOG(LogTemp, Display, TEXT("R07FoxFixture finished: checks=%d failures=%d RESULT: %s"),
				Checks, Failures, Failures == 0 ? TEXT("PASS") : TEXT("FAIL"));
			if (Failures != 0) UE_LOG(LogTemp, Error, TEXT("R07FoxFixture failed %d real-actor checks"), Failures);
			FPlatformMisc::RequestExitWithStatus(false, Failures == 0 ? 0 : 1);
		}
	};

	bool SetBool(UObject* Object, const FName Name, const bool Value)
	{
		FBoolProperty* Property = Object ? FindFProperty<FBoolProperty>(Object->GetClass(), Name) : nullptr;
		if (!Property) return false;
		Property->SetPropertyValue_InContainer(Object, Value);
		return true;
	}
	bool SetFloat(UObject* Object, const FName Name, const float Value)
	{
		FFloatProperty* Property = Object ? FindFProperty<FFloatProperty>(Object->GetClass(), Name) : nullptr;
		if (!Property) return false;
		Property->SetPropertyValue_InContainer(Object, Value);
		return true;
	}
	float ReadFloat(const UObject* Object, const FName Name)
	{
		const FFloatProperty* Property = Object ? FindFProperty<FFloatProperty>(Object->GetClass(), Name) : nullptr;
		return Property ? Property->GetPropertyValue_InContainer(Object) : -1000.0f;
	}
	UPaperFlipbook* ReadClip(const UObject* Object, const FName Name)
	{
		const FObjectProperty* Property = Object ? FindFProperty<FObjectProperty>(Object->GetClass(), Name) : nullptr;
		return Property ? Cast<UPaperFlipbook>(Property->GetObjectPropertyValue_InContainer(Object)) : nullptr;
	}
	void Quarantine(AActor* Actor)
	{
		if (!Actor) return;
		Actor->GetWorldTimerManager().ClearAllTimersForObject(Actor);
		Actor->SetActorHiddenInGame(true);
		Actor->SetActorEnableCollision(false);
		Actor->SetActorTickEnabled(false);
		if (ACharacter* Character = Cast<ACharacter>(Actor))
		{
			Character->ConsumeMovementInputVector();
			Character->GetCharacterMovement()->StopMovementImmediately();
		}
	}
	void StopControlledMovement(ACharacter& Actor)
	{
		Actor.ConsumeMovementInputVector();
		Actor.GetCharacterMovement()->StopMovementImmediately();
	}
	void ConfigureControlledFlying(ACharacter& Actor, const float Speed)
	{
		UCharacterMovementComponent* Movement = Actor.GetCharacterMovement();
		Movement->SetMovementMode(MOVE_Flying);
		Movement->MaxFlySpeed = Speed;
		Movement->BrakingDecelerationFlying = 2048.0f;
		// This fixture isolates animation/AI transitions, not terrain physics.
		// Keep real Walking collision coverage in the untouched-map observer.
		Actor.GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
		StopControlledMovement(Actor);
	}
	void CheckControlledVisual(const APaperCharacter& Actor, AImmortalPlayerCharacter& Player,
		const TSharedRef<FFoxFixtureState>& State, const float ExpectedScale, const bool bMonster)
	{
		const FFoxVisualProbe Probe = MeasureVisual(Actor, Player);
		State->Check(Probe.FootError <= 1.5f && Probe.DepthError <= 1.5f,
			TEXT("Real sprite matches its own capsule bottom plus player visual ground offset and player sprite depth"));
		State->Check(FMath::IsNearlyEqual(FMath::Abs(Probe.Scale.X), ExpectedScale, 0.01)
			&& FMath::IsNearlyEqual(Probe.Scale.Z, ExpectedScale, 0.01),
			TEXT("Real sprite retains its deliberately authored scene scale"));
		if (bMonster)
		{
			const UWidgetComponent* Bar = Actor.FindComponentByClass<UWidgetComponent>();
			FVector ExpectedBar = Actor.GetRootComponent()->GetComponentTransform().TransformPosition(
				FVector(0.0f, ReadFloat(&Actor, TEXT("VisualDepthOffset")), ReadFloat(&Actor, TEXT("HealthBarHeight"))));
			ExpectedBar.Z += Probe.ReferenceGroundOffset;
			ExpectedBar.Y = Player.GetSprite()->GetComponentLocation().Y;
			State->Check(Bar && Bar->GetComponentLocation().Equals(ExpectedBar, 1.5f),
				TEXT("Real monster health bar keeps its nonaccumulating height baseline plus player visual offset and sprite depth"));
		}
		LogVisual(Actor, Player, bMonster ? TEXT("ControlledMonster") : TEXT("ControlledPet"));
	}
	bool IsClip(const APaperCharacter* Actor, const UPaperFlipbook* Clip)
	{
		return Actor && Actor->GetSprite() && Actor->GetSprite()->GetFlipbook() == Clip;
	}
	bool IsLastPose(const APaperCharacter* Actor, UPaperFlipbook* Clip)
	{
		return IsClip(Actor, Clip) && Clip && Clip->GetNumKeyFrames() > 0
			&& !Actor->GetSprite()->IsLooping() && !Actor->GetSprite()->IsPlaying()
			&& Clip->GetSpriteAtTime(Actor->GetSprite()->GetPlaybackPosition(), true)
				== Clip->GetKeyFrameChecked(Clip->GetNumKeyFrames() - 1).Sprite;
	}
	FVector PetFollowLocation(AImmortalPlayerCharacter& Player, AImmortalPetCharacter& Pet)
	{
		FImmortalPetDefinition Definition;
		UImmortalPetLibrary::GetPetDefinition(TEXT("SpiritFox"), Definition);
		FVector Location = Player.GetActorLocation();
		Location.X += Definition.FollowOffsetX;
		Location.Z += Pet.GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
			- Player.GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		return Location;
	}
	FVector FoxLaneLocation(AImmortalPlayerCharacter& Player, const float OffsetX)
	{
		// Deliberately do not repair actor Z: presentation must work with each actor's own capsule bottom.
		return Player.GetActorLocation() + FVector(OffsetX, 0, 0);
	}
	AImmortalMonsterCharacter* SpawnFox(AImmortalPlayerCharacter& Player, const FVector& Location,
		const TSharedRef<FFoxFixtureState>& State)
	{
		UClass* Class = LoadClass<AImmortalMonsterCharacter>(nullptr, TEXT("/Game/GAME/BP_Monster.BP_Monster_C"));
		State->Check(Class != nullptr, TEXT("Actual BP_Monster class loads"));
		if (!Class) return nullptr;
		FTransform Transform(FRotator::ZeroRotator, Location);
		AImmortalMonsterCharacter* Actor = Player.GetWorld()->SpawnActorDeferred<AImmortalMonsterCharacter>(
			Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		State->Check(Actor != nullptr, TEXT("Actual BP_Monster actor spawns"));
		if (!Actor) return nullptr;
		const TCHAR* Names[] = {TEXT("IdleFlipbook"), TEXT("MoveFlipbook"), TEXT("AttackFlipbook"), TEXT("HurtFlipbook"), TEXT("DeathFlipbook")};
		for (int32 Index = 0; Index < 5; ++Index)
			State->Check(ReadClip(Actor, FName(Names[Index])) == State->Clip(Index), Names[Index]);
		State->Check(FMath::IsNearlyEqual(ReadFloat(Actor, TEXT("VisualScale")), 2.0f), TEXT("Real actor uses scene scale two"));
		const FBoolProperty* AlignProperty = FindFProperty<FBoolProperty>(Actor->GetClass(), TEXT("bAlignSpriteToPlayerLane"));
		State->Check(AlignProperty && AlignProperty->GetPropertyValue_InContainer(Actor),
			TEXT("Actual BP opts into player battle-lane sprite placement"));
		State->Check(FMath::IsNearlyEqual(ReadFloat(Actor, TEXT("AttackWindup")), 0.25f), TEXT("Real actor uses .25s authored contact"));
		State->Check(FMath::IsNearlyEqual(ReadFloat(Actor, TEXT("HurtAnimationDuration")), 8.0f / 12.0f), TEXT("Real actor has complete Hurt duration"));
		// Only fixture instances are modified. The real BP defaults above remain the assertions.
		UE_LOG(LogTemp, Display, TEXT("R07FoxFixture original BP attack range=%.3f; isolated actor range=100 for in-viewport locomotion probes; production defaults remain unchanged"),
			ReadFloat(Actor, TEXT("AttackRange")));
		State->Check(SetBool(Actor, TEXT("bAutoCombatOnBeginPlay"), false)
			&& SetBool(Actor, TEXT("bDestroyAfterDeath"), false)
			&& SetFloat(Actor, TEXT("MaxHealth"), 10000.0f)
			&& SetFloat(Actor, TEXT("AttackInterval"), 30.0f)
			&& SetFloat(Actor, TEXT("AttackRange"), 100.0f)
			&& SetFloat(Actor, TEXT("CriticalChance"), 0.0f)
			&& SetFloat(Actor, TEXT("EquipmentDropChance"), 0.0f)
			&& SetFloat(Actor, TEXT("SpiritStoneDropChance"), 0.0f)
			&& SetFloat(Actor, TEXT("MaterialDropChance"), 0.0f), TEXT("Isolated actor controls exist; no random loot or extra attacks"));
		UGameplayStatics::FinishSpawningActor(Actor, Transform);
		// Character initialization can settle its capsule onto the floor before
		// controlled mode is configured. Preserve that real spawned transform;
		// do not restore the requested Z or mistake engine floor adjustment for
		// a sprite-placement mutation.
		const FTransform SpawnedTransform = Actor->GetActorTransform();
		ConfigureControlledFlying(*Actor, 80.0f);
		Actor->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		State->Check(FMath::IsNearlyEqual(ReadFloat(Actor, TEXT("HealthBarHeight")),
			185.0f - Actor->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), 0.1f), TEXT("Fox health bar retains the scene-art height baseline"));
		State->Check(Actor->GetActorTransform().Equals(SpawnedTransform, 0.1f),
			TEXT("Controlled mode preserves the normally initialized actor transform without a fixture-only Z correction"));
		UE_LOG(LogTemp, Display, TEXT("R07FoxFixture spawn placement: requested=%s initialized=%s controlled=%s; no actor-position repair applied"),
			*Location.ToString(), *SpawnedTransform.GetLocation().ToString(), *Actor->GetActorLocation().ToString());
		State->Check(FMath::IsNearlyEqual(Actor->GetCharacterMovement()->MaxFlySpeed, 80.0f)
			&& FMath::IsNearlyEqual(Actor->GetCharacterMovement()->BrakingDecelerationFlying, 2048.0f)
			&& Actor->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Ignore,
			TEXT("Controlled Flying explicitly isolates terrain, with speed limit and braking; normal Walking is tested separately"));
		CheckControlledVisual(*Actor, Player, State, 2.0f, true);
		return Actor;
	}
}

#endif

bool ImmortalFoxAnimationFixture::CheckPreflightIfRequested()
{
#if !UE_BUILD_SHIPPING
	if (!FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestFoxArt"))
		&& !FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestFoxWalking"))) return true;
	FString Directory;
	if (!GetSafeDirectory(Directory))
	{
		UE_LOG(LogTemp, Error, TEXT("R07FoxFixture preflight refused before player initialization: requires isolated UserDir below Saved/Automation/FoxArt and exactly one FoxArt/FoxWalking mode; FoxArt needs DisableAutoBattle and FoxWalking forbids it"));
		FPlatformMisc::RequestExitWithStatus(false, 2);
		return false;
	}
#endif
	return true;
}

void ImmortalFoxAnimationFixture::StartIfRequested(AImmortalPlayerCharacter& Player)
{
#if !UE_BUILD_SHIPPING
	const bool bWalking = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestFoxWalking"));
	if ((!FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestFoxArt")) && !bWalking) || bFoxFixtureClaimed) return;
	FString Directory;
	if (!CheckPreflightIfRequested() || !GetSafeDirectory(Directory))
	{
		return;
	}
	bFoxFixtureClaimed = true;
	if (bWalking)
	{
		StartWalkingObserver(Player, Directory);
		return;
	}
	const TSharedRef<FFoxFixtureState> State = MakeShared<FFoxFixtureState>();
	State->Player = &Player;
	State->Directory = Directory;
	State->Check(Player.GetWorld() && Player.GetWorld()->WorldType == EWorldType::Game,
		TEXT("Standalone Game world required"));
	if (State->Failures) { State->Finish(); return; }
	for (const TCHAR* Action : {TEXT("Idle"), TEXT("Move"), TEXT("Attack"), TEXT("Hurt"), TEXT("Death")})
	{
		const FString Name = FString::Printf(TEXT("FB_Fox_Pixel_%s"), Action);
		UPaperFlipbook* Clip = LoadObject<UPaperFlipbook>(nullptr, *FString::Printf(
			TEXT("/Game/GAME/Asset/Monsters/desktop_pixel_v2/fox/%s.%s"), *Name, *Name));
		State->Check(Clip != nullptr, *Name);
		State->Clips.Emplace(Clip);
	}
	if (State->Failures) { State->Finish(); return; }
	const auto At = [&Player, State](const float Delay, const TFunction<void()>& Callback)
	{
		FTimerHandle Handle;
		Player.GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(&Player,
			[State, Callback] { if (!State->bFinished) Callback(); }), Delay, false);
	};
	const auto Shot = [&Player, State](const TCHAR* Name)
	{
		const APlayerController* Controller = Cast<APlayerController>(Player.GetController());
		for (AImmortalMonsterCharacter* Fox : {State->Fox.Get(), State->DeathFox.Get(), State->PetTarget.Get()})
			if (Fox && !Fox->IsHidden())
			{
				CheckControlledVisual(*Fox, Player, State, 2.0f, true);
				State->Check(SpriteInsideViewport(*Fox, Controller), TEXT("Controlled source capture includes the complete visible fox sprite bounds"));
			}
		if (AImmortalPetCharacter* Pet = State->Pet.Get())
			if (!Pet->IsHidden())
			{
				CheckControlledVisual(*Pet, Player, State, 1.25f, false);
				State->Check(SpriteInsideViewport(*Pet, Controller), TEXT("Controlled source capture includes the complete visible pet sprite bounds"));
			}
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(State->Directory,
			FString::Printf(TEXT("R07_Fox_%s.png"), Name)), true, false);
	};

	At(0.65f, [&Player, State]
	{
		State->Check(!Player.IsDead() && !Player.IsDeathCultivationRecoveryRequired(), TEXT("Fresh unlocked isolated save"));
		if (State->Failures) { State->Finish(); return; }
		Player.StopAutoAttack();
		StopControlledMovement(Player);
		Player.GetCharacterMovement()->DisableMovement();
		Player.SetActorTickEnabled(false);
		for (TActorIterator<AImmortalMonsterSpawner> It(Player.GetWorld()); It; ++It) It->StopSpawning();
		for (TActorIterator<AImmortalMonsterCharacter> It(Player.GetWorld()); It; ++It) Quarantine(*It);
		for (TActorIterator<AImmortalPetCharacter> It(Player.GetWorld()); It; ++It) Quarantine(*It);
		State->Origin = Player.GetActorLocation();
		State->Pet = Player.GetActivePetActor();
		State->Check(State->Pet.IsValid() && State->Pet->GetPetId() == TEXT("SpiritFox"), TEXT("Actual player-owned SpiritFox actor exists"));
		State->Fox = SpawnFox(Player, State->Origin + FVector(250, 0, 0), State);
		if (!State->Fox.IsValid() || !State->Pet.IsValid()) { State->Finish(); return; }
		State->bSampleIdle = true;
		State->LastPlayerHealth = Player.GetCurrentHealth();
		Player.GetWorldTimerManager().SetTimer(State->SampleTimer,
			FTimerDelegate::CreateWeakLambda(&Player, [&Player, State]
			{
				const float Now = Player.GetWorld()->GetTimeSeconds();
				if (AImmortalMonsterCharacter* Fox = State->Fox.Get())
				{
					UPaperFlipbookComponent* Sprite = Fox->GetSprite();
					if (State->bSampleIdle && IsClip(Fox, State->Clip(0)))
					{
						const float Position = Sprite->GetPlaybackPosition();
						if (State->IdleLastPosition >= 0)
						{
							if (Position > State->IdleLastPosition + 0.001f) ++State->IdleAdvances;
							else if (Position + 0.25f < State->IdleLastPosition) ++State->IdleWraps;
						}
						State->IdleLastPosition = Position;
					}
					if (State->bSampleMonsterAttack)
					{
						if (State->MonsterAttackStart < 0 && IsClip(Fox, State->Clip(2)))
						{
							State->MonsterAttackStart = Now;
							SetBool(Fox, TEXT("bAutoCombatOnBeginPlay"), false);
						}
						if (Player.GetCurrentHealth() < State->LastPlayerHealth - 0.01f)
						{
							++State->MonsterDamageEvents;
							State->MonsterContactTime = Now;
							State->ContactFrame = Sprite->GetPlaybackPositionInFrames();
						}
						State->LastPlayerHealth = Player.GetCurrentHealth();
					}
					if (State->bSampleHurt && IsClip(Fox, State->Clip(3)))
					{
						const float Position = Sprite->GetPlaybackPosition();
						if (State->HurtLastPosition >= 0)
						{
							if (Position > State->HurtLastPosition + 0.001f) ++State->HurtAdvances;
							else if (Position + 0.03f < State->HurtLastPosition) ++State->HurtRegressions;
						}
						State->HurtLastPosition = Position;
					}
				}
				if (State->bSampleDeathAttack)
				{
					AImmortalMonsterCharacter* Fox = State->DeathFox.Get();
					if (Fox && IsClip(Fox, State->Clip(2)))
					{
						if (State->DeathAttackStart < 0) State->DeathAttackStart = Now;
						if (!State->bFatalInjected && Now - State->DeathAttackStart >= 0.10f)
						{
							State->bFatalInjected = true;
							State->HealthAtFatal = Player.GetCurrentHealth();
							FDamageEvent Event;
							Fox->TakeDamage(1000000.0f, Event, nullptr, nullptr);
						}
					}
				}
				if (AImmortalPetCharacter* Pet = State->Pet.Get())
				{
					if (State->bSamplePetIdle && IsClip(Pet, State->Clip(0)))
					{
						const float Position = Pet->GetSprite()->GetPlaybackPosition();
						if (State->PetIdleLastPosition >= 0)
						{
							if (Position > State->PetIdleLastPosition + 0.001f) ++State->PetIdleAdvances;
							else if (Position + 0.25f < State->PetIdleLastPosition) ++State->PetIdleWraps;
						}
						State->PetIdleLastPosition = Position;
					}
					if (State->bSamplePetAttack && State->PetTarget.IsValid())
					{
						if (State->PetAttackStart < 0 && Pet->GetAttackCount() > State->PetAttacksBefore && IsClip(Pet, State->Clip(2)))
						{
							State->PetAttackStart = Now;
							// Component and actor-owned timers keep running; disable only new AI attacks.
							Pet->SetActorTickEnabled(false);
						}
						const float Health = State->PetTarget->GetCurrentHealth();
						if (Health < State->LastPetTargetHealth - 0.01f)
						{
							++State->PetDamageEvents;
							State->PetContactTime = Now;
						}
						State->LastPetTargetHealth = Health;
					}
				}
			}), 0.015f, true);
	});
	At(1.65f, [State, Shot]
	{
		State->Check(IsClip(State->Fox.Get(), State->Clip(0)) && State->Fox->GetSprite()->IsPlaying()
			&& State->Fox->GetSprite()->IsLooping(), TEXT("Real monster stationary Idle plays and loops"));
		Shot(TEXT("MonsterIdle"));
	});
	At(2.20f, [State]
	{
		State->Check(State->IdleAdvances > 15 && State->IdleWraps >= 1, TEXT("Real monster Idle advances through a full loop"));
		State->bSampleIdle = false;
	});
	At(2.5f, [&Player, State]
	{
		if (AImmortalMonsterCharacter* Fox = State->Fox.Get())
		{
			// Relocate only X for this horizontal locomotion probe. Keep the
			// capsule's normal initialized Z instead of lifting it to player Z.
			State->MoveOrigin = Fox->GetActorLocation();
			State->MoveOrigin.X = Player.GetActorLocation().X + 500.0f;
			Fox->SetActorLocation(State->MoveOrigin);
			StopControlledMovement(*Fox);
			SetFloat(Fox, TEXT("MovementSpeed"), 80.0f);
			SetBool(Fox, TEXT("bAutoCombatOnBeginPlay"), true);
		}
	});
	At(2.9f, [State, Shot]
	{
		AImmortalMonsterCharacter* Fox = State->Fox.Get();
		State->Check(IsClip(Fox, State->Clip(1)) && Fox->GetVelocity().X < -1.0f
			&& Fox->GetActorLocation().X < State->MoveOrigin.X - 5.0f, TEXT("Real AI moves left with looping Move"));
		State->Check(Fox && Fox->GetSprite()->GetRelativeScale3D().X > 0, TEXT("Left-facing source remains unmirrored toward the player"));
		Shot(TEXT("MonsterMoveLeft"));
	});
	At(3.10f, [State]
	{
		if (AImmortalMonsterCharacter* Fox = State->Fox.Get())
		{
			SetBool(Fox, TEXT("bAutoCombatOnBeginPlay"), false);
			StopControlledMovement(*Fox);
		}
	});
	At(3.25f, [State]
	{
		State->Check(IsClip(State->Fox.Get(), State->Clip(0)), TEXT("Monster movement stop returns to authored Idle"));
	});
	At(3.40f, [&Player, State]
	{
		if (AImmortalMonsterCharacter* Fox = State->Fox.Get())
		{
			State->MoveOrigin = Fox->GetActorLocation();
			// Keep the entire right-facing actor inside the adapted desktop strip.
			State->MoveOrigin.X = Player.GetActorLocation().X - 300.0f;
			Fox->SetActorLocation(State->MoveOrigin);
			StopControlledMovement(*Fox);
			SetBool(Fox, TEXT("bAutoCombatOnBeginPlay"), true);
		}
	});
	At(3.8f, [State, Shot]
	{
		AImmortalMonsterCharacter* Fox = State->Fox.Get();
		State->Check(IsClip(Fox, State->Clip(1)) && Fox->GetVelocity().X > 1.0f
			&& Fox->GetActorLocation().X > State->MoveOrigin.X + 5.0f, TEXT("Real AI moves right with Move"));
		State->Check(Fox && Fox->GetSprite()->GetRelativeScale3D().X < 0, TEXT("Monster mirrors only horizontal facing to the right"));
		Shot(TEXT("MonsterMoveRight"));
	});
	At(4.05f, [State]
	{
		if (AImmortalMonsterCharacter* Fox = State->Fox.Get())
		{
			SetBool(Fox, TEXT("bAutoCombatOnBeginPlay"), false);
			StopControlledMovement(*Fox);
		}
	});
	At(4.3f, [&Player, State]
	{
		// The locomotion actor must not carry attack state or cooldown into the first-contact test.
		Quarantine(State->Fox.Get());
		State->Fox = SpawnFox(Player, FoxLaneLocation(Player, 90), State);
		if (AImmortalMonsterCharacter* Fox = State->Fox.Get())
		{
			State->MonsterAttackStart = -1.0f;
			State->MonsterContactTime = -1.0f;
			State->MonsterDamageEvents = 0;
			State->ContactFrame = -1;
			State->LastPlayerHealth = Player.GetCurrentHealth();
			SetFloat(Fox, TEXT("AttackDamage"), FMath::Max(Player.GetMaxHealth() * 0.10f, 30.0f));
			State->bSampleMonsterAttack = true;
			SetBool(Fox, TEXT("bAutoCombatOnBeginPlay"), true);
		}
	});
	At(4.6f, [Shot] { Shot(TEXT("MonsterAttackContact")); });
	At(5.25f, [State]
	{
		State->bSampleMonsterAttack = false;
		const float Delta = State->MonsterContactTime - State->MonsterAttackStart;
		UE_LOG(LogTemp, Display, TEXT("R07FoxFixture monster contact: start=%.3f hit=%.3f delta=%.3f frame=%d events=%d"),
			State->MonsterAttackStart, State->MonsterContactTime, Delta, State->ContactFrame, State->MonsterDamageEvents);
		State->Check(State->MonsterAttackStart >= 0 && State->MonsterDamageEvents == 1,
			TEXT("Real monster attack causes exactly one player HP decrement"));
		State->Check(State->MonsterContactTime >= 0 && FMath::Abs(Delta - 0.25f) <= 0.10f,
			TEXT("Real damage occurs at authored .25s contact, not attack start"));
		State->Check(State->ContactFrame >= 2 && State->ContactFrame <= 4, TEXT("Damage sample aligns with the contact pose"));
		State->Check(IsClip(State->Fox.Get(), State->Clip(0)), TEXT("Attack recovery returns to Idle"));
	});
	At(5.5f, [State]
	{
		State->bSampleHurt = true;
		if (AImmortalMonsterCharacter* Fox = State->Fox.Get())
		{
			FDamageEvent Event;
			const float Before = Fox->GetCurrentHealth();
			Fox->TakeDamage(1.0f, Event, nullptr, nullptr);
			State->Check(Fox->GetCurrentHealth() < Before && IsClip(Fox, State->Clip(3)), TEXT("Real TakeDamage reduces HP and enters Hurt"));
		}
	});
	for (const float Time : {5.65f, 5.80f, 5.95f})
		At(Time, [State]
		{
			if (AImmortalMonsterCharacter* Fox = State->Fox.Get())
			{
				FDamageEvent Event;
				Fox->TakeDamage(1.0f, Event, nullptr, nullptr);
			}
		});
	At(6.35f, [State, Shot]
	{
		State->Check(State->HurtAdvances > 10 && State->HurtRegressions == 0,
			TEXT("Dense real damage does not restart or regress Hurt frames"));
		State->Check(IsLastPose(State->Fox.Get(), State->Clip(3)), TEXT("Dense Hurt reaches and holds its actual final pose"));
		Shot(TEXT("MonsterDenseHurt"));
	});
	At(6.85f, [State]
	{
		State->bSampleHurt = false;
		State->Check(IsClip(State->Fox.Get(), State->Clip(0)), TEXT("Dense Hurt eventually returns to authored Idle"));
	});
	At(7.2f, [&Player, State]
	{
		Quarantine(State->Fox.Get());
		State->DeathFox = SpawnFox(Player, Player.GetActorLocation() + FVector(90, 0, 0), State);
		if (AImmortalMonsterCharacter* Fox = State->DeathFox.Get())
		{
			State->bSampleDeathAttack = true;
			SetBool(Fox, TEXT("bAutoCombatOnBeginPlay"), true);
		}
	});
	At(7.65f, [State, Shot]
	{
		State->Check(State->bFatalInjected && State->DeathFox.IsValid() && State->DeathFox->IsDead()
			&& IsClip(State->DeathFox.Get(), State->Clip(4)), TEXT("Fatal real damage preempts an in-progress attack with Death"));
		Shot(TEXT("MonsterDeathStart"));
	});
	At(8.65f, [&Player, State, Shot]
	{
		State->bSampleDeathAttack = false;
		State->Check(State->bFatalInjected && FMath::IsNearlyEqual(Player.GetCurrentHealth(), State->HealthAtFatal, 0.01f),
			TEXT("Fatal interruption cancels the unresolved damage timer"));
		State->Check(IsLastPose(State->DeathFox.Get(), State->Clip(4)), TEXT("Real dead actor retains its final Death pose"));
		Shot(TEXT("MonsterDeathHeld"));
	});
	At(9.15f, [State] { Quarantine(State->DeathFox.Get()); });
	At(9.5f, [&Player, State]
	{
		AImmortalPetCharacter* Pet = State->Pet.Get();
		if (!Pet) { State->Check(false, TEXT("Owned pet still exists")); State->Finish(); return; }
		State->Check(Pet->InitializeForPlayer(&Player, TEXT("SpiritFox")), TEXT("Existing authority-owned pet resets only its runtime animation state"));
		FImmortalPetDefinition Definition;
		State->Check(UImmortalPetLibrary::GetPetDefinition(TEXT("SpiritFox"), Definition), TEXT("Actual pet definition is available for controlled follow speed"));
		ConfigureControlledFlying(*Pet, Definition.MovementSpeed);
		Pet->SetActorLocation(PetFollowLocation(Player, *Pet));
		StopControlledMovement(*Pet);
		State->Check(Pet == Player.GetActivePetActor(), TEXT("Fixture uses the actual authoritative pet actor"));
		State->Check(FMath::IsNearlyEqual(FMath::Abs(Pet->GetSprite()->GetRelativeScale3D().X), 1.25f), TEXT("Real pet uses scene scale 1.25"));
		State->Check(Pet->GetSprite()->GetSpriteColor().Equals(FLinearColor::White), TEXT("Real pet retains original palette"));
		State->bSamplePetIdle = true;
	});
	At(10.5f, [State, Shot]
	{
		State->Check(IsClip(State->Pet.Get(), State->Clip(0)) && State->Pet->GetSprite()->IsLooping()
			&& State->Pet->GetSprite()->IsPlaying(), TEXT("Owned pet authored Idle loops instead of freezing Move frame zero"));
		Shot(TEXT("PetIdle"));
	});
	At(11.05f, [State]
	{
		State->bSamplePetIdle = false;
		State->Check(State->PetIdleAdvances > 15 && State->PetIdleWraps >= 1, TEXT("Owned pet Idle advances across a complete loop"));
	});
	At(11.15f, [&Player, State]
	{
		if (AImmortalPetCharacter* Pet = State->Pet.Get())
		{
			State->MoveOrigin = PetFollowLocation(Player, *Pet) + FVector(320, 0, 0);
			Pet->SetActorLocation(State->MoveOrigin);
			StopControlledMovement(*Pet);
		}
	});
	At(11.45f, [State, Shot]
	{
		AImmortalPetCharacter* Pet = State->Pet.Get();
		State->Check(IsClip(Pet, State->Clip(1)) && Pet->GetVelocity().X < -1.0f
			&& Pet->GetActorLocation().X < State->MoveOrigin.X - 5, TEXT("Real pet follows left with moving animation"));
		State->Check(Pet && Pet->GetSprite()->GetRelativeScale3D().X > 0, TEXT("Pet left facing uses the original left-facing art"));
		Shot(TEXT("PetMoveLeft"));
	});
	At(12.05f, [State]
	{
		State->Check(IsClip(State->Pet.Get(), State->Clip(0)), TEXT("Pet naturally stops and returns to Idle at follow point"));
	});
	At(12.3f, [&Player, State]
	{
		if (AImmortalPetCharacter* Pet = State->Pet.Get())
		{
			State->MoveOrigin = PetFollowLocation(Player, *Pet) + FVector(-320, 0, 0);
			Pet->SetActorLocation(State->MoveOrigin);
			StopControlledMovement(*Pet);
		}
	});
	At(12.60f, [State, Shot]
	{
		AImmortalPetCharacter* Pet = State->Pet.Get();
		State->Check(IsClip(Pet, State->Clip(1)) && Pet->GetVelocity().X > 1.0f
			&& Pet->GetActorLocation().X > State->MoveOrigin.X + 5, TEXT("Real pet follows right with moving animation"));
		State->Check(Pet && Pet->GetSprite()->GetRelativeScale3D().X < 0, TEXT("Pet right facing mirrors only X"));
		Shot(TEXT("PetMoveRight"));
	});
	At(13.4f, [State]
	{
		State->Check(IsClip(State->Pet.Get(), State->Clip(0)), TEXT("Pet rightward follow settles back to Idle"));
	});
	At(14.5f, [State]
	{
		State->Check(State->Pet.IsValid() && State->Pet->PreviewHurtForDevelopment(), TEXT("Existing public pet Hurt preview accepted"));
		State->Check(IsClip(State->Pet.Get(), State->Clip(3)), TEXT("Real pet component enters Hurt preview"));
	});
	At(14.8f, [Shot] { Shot(TEXT("PetHurtPreview")); });
	At(15.3f, [State]
	{
		State->Check(IsClip(State->Pet.Get(), State->Clip(0)), TEXT("Pet Hurt preview returns to normal Idle"));
	});
	At(15.5f, [State]
	{
		State->Check(State->Pet.IsValid() && State->Pet->PreviewDeathForDevelopment(), TEXT("Existing public pet Death preview accepted"));
		State->Check(IsClip(State->Pet.Get(), State->Clip(4)), TEXT("Real pet component enters Death preview"));
	});
	At(15.8f, [Shot] { Shot(TEXT("PetDeathPreview")); });
	At(16.35f, [State]
	{
		State->Check(IsClip(State->Pet.Get(), State->Clip(0)), TEXT("Pet Death preview resumes normal follow without persistent pet death"));
	});
	At(16.5f, [&Player, State]
	{
		AImmortalPetCharacter* Pet = State->Pet.Get();
		if (!Pet) return;
		FVector TargetLocation = Player.GetActorLocation();
		TargetLocation.X = Pet->GetActorLocation().X + 100.0f;
		State->PetTarget = SpawnFox(Player, TargetLocation, State);
		if (State->PetTarget.IsValid())
		{
			State->PetAttacksBefore = Pet->GetAttackCount();
			State->PetHitsBefore = Pet->GetHitCount();
			State->LastPetTargetHealth = State->PetTarget->GetCurrentHealth();
			State->bSamplePetAttack = true;
		}
	});
	At(16.85f, [Shot] { Shot(TEXT("PetAttackContact")); });
	At(17.5f, [State]
	{
		State->bSamplePetAttack = false;
		const float Delta = State->PetContactTime - State->PetAttackStart;
		UE_LOG(LogTemp, Display, TEXT("R07FoxFixture pet contact: start=%.3f hit=%.3f delta=%.3f events=%d actorHitCount=%d"),
			State->PetAttackStart, State->PetContactTime, Delta, State->PetDamageEvents,
			State->Pet.IsValid() ? State->Pet->GetHitCount() : -1);
		State->Check(State->PetAttackStart >= 0 && State->PetDamageEvents == 1
			&& State->Pet.IsValid() && State->Pet->GetHitCount() == State->PetHitsBefore + 1, TEXT("Authoritative owned pet resolves exactly one actual target damage event"));
		State->Check(State->PetContactTime >= 0 && FMath::Abs(Delta - 0.25f) <= 0.10f,
			TEXT("Real pet damage uses authored .25s contact"));
	});
	At(17.8f, [State]
	{
		if (State->PetTarget.IsValid()) State->PetTarget->Destroy();
		if (State->Pet.IsValid()) State->Pet->SetActorTickEnabled(true);
	});
	At(18.10f, [&Player, State]
	{
		AImmortalPetCharacter* Pet = State->Pet.Get();
		if (!Pet) return;
		FVector TargetLocation = Player.GetActorLocation();
		TargetLocation.X = Pet->GetActorLocation().X + 500.0f;
		State->PetTarget = SpawnFox(Player, TargetLocation, State);
		if (State->PetTarget.IsValid())
		{
			State->PetAttackStart = -1.0f;
			State->PetContactTime = -1.0f;
			State->PetDamageEvents = 0;
			State->PetAttacksBefore = Pet->GetAttackCount();
			State->PetHitsBefore = Pet->GetHitCount();
			State->LastPetTargetHealth = State->PetTarget->GetCurrentHealth();
			State->Check(FMath::IsNearlyEqual(FMath::Abs(State->PetTarget->GetActorLocation().X - Pet->GetActorLocation().X), 500.0f, 0.1f),
				TEXT("Real ranged pet target begins five hundred world units away"));
			State->Check(FMath::Abs(State->PetTarget->GetActorLocation().X - Player.GetActorLocation().X) > ReadFloat(&Player, TEXT("AttackRange")),
				TEXT("Far pet contact is outside the player's normal melee attack range"));
			State->bSamplePetAttack = true;
		}
	});
	At(18.50f, [Shot] { Shot(TEXT("PetFarAttackContact")); });
	At(19.25f, [State]
	{
		State->bSamplePetAttack = false;
		const float Delta = State->PetContactTime - State->PetAttackStart;
		UE_LOG(LogTemp, Display, TEXT("R07FoxFixture pet far contact: start=%.3f hit=%.3f delta=%.3f events=%d actorHitCount=%d"),
			State->PetAttackStart, State->PetContactTime, Delta, State->PetDamageEvents,
			State->Pet.IsValid() ? State->Pet->GetHitCount() : -1);
		State->Check(State->PetAttackStart >= 0 && State->PetDamageEvents == 1
			&& State->Pet.IsValid() && State->Pet->GetHitCount() == State->PetHitsBefore + 1,
			TEXT("Five-hundred-unit ranged pet attack resolves one real HP decrement"));
		State->Check(State->PetContactTime >= 0 && FMath::Abs(Delta - 0.25f) <= 0.10f,
			TEXT("Far real pet damage uses authored .25s contact"));
	});
	At(19.5f, [State]
	{
		if (State->PetTarget.IsValid()) State->PetTarget->Destroy();
		if (State->Pet.IsValid()) State->Pet->SetActorTickEnabled(true);
	});
	At(20.0f, [&Player, State]
	{
		FDamageEvent Event;
		Player.TakeDamage(1000000.0f, Event, nullptr, nullptr);
		State->Check(Player.IsDead() && Player.IsDeathCultivationRecoveryRequired(), TEXT("Real owner death preserves existing forced cultivation gate"));
	});
	At(20.3f, [State, Shot]
	{
		State->Check(IsClip(State->Pet.Get(), State->Clip(4)), TEXT("Real owner death, not preview, puts pet into Death"));
		Shot(TEXT("PetOwnerDeath"));
	});
	At(21.1f, [&Player, State, Shot]
	{
		State->Check(Player.IsDead() && IsLastPose(State->Pet.Get(), State->Clip(4)), TEXT("Pet holds actual final Death pose while its owner remains dead"));
		// Existing death redirect opens management. Close only the isolated UI for the source capture.
		Player.CloseManagementInterface();
		Shot(TEXT("PetOwnerDeathHeld"));
	});
	At(23.5f, [&Player, State]
	{
		State->Check(!Player.IsDead() && Player.IsDeathCultivationRecoveryRequired(), TEXT("Existing actual auto-revive retains cultivation lock"));
		State->Check(State->Pet.IsValid() && State->Pet.Get() == Player.GetActivePetActor()
			&& State->Pet->IsActorTickEnabled() && !IsClip(State->Pet.Get(), State->Clip(4)), TEXT("Same owned pet resumes follow after actual owner revival"));
		Player.StopAutoAttack();
		StopControlledMovement(Player);
		Player.GetCharacterMovement()->DisableMovement();
		Player.CloseManagementInterface();
	});
	At(26.0f, [State, Shot]
	{
		State->Check(IsClip(State->Pet.Get(), State->Clip(0)) && State->Pet->GetSprite()->IsPlaying()
			&& State->Pet->GetSprite()->IsLooping(), TEXT("Pet returns to looping authored Idle after revival and follow settling"));
		Shot(TEXT("PetOwnerRevivedIdle"));
	});
	At(29.0f, [State]
	{
		UE_LOG(LogTemp, Display, TEXT("R07FoxFixture coverage: actual BP and owner-bound pet; isolated locomotion versus fresh first-attack actor; near/far real damage contacts; sprite foot-plane, depth, scene scale and healthbar assertions with controlled Flying movement, not Walking map validation; no monster actor Z repair and source captures only. Pet Hurt/Death preview does not imply a new pet-health gameplay system. R07F ranged projectile/impact art and multi-DPI/native desktop edge appearance remain outside this fixture."));
		State->Finish();
	});
#endif
}
