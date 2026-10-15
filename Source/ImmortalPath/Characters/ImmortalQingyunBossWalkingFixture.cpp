#include "ImmortalQingyunBossWalkingFixture.h"
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "ImmortalMonsterCharacter.h"
#include "ImmortalPlayerCharacter.h"
#include "../Spawning/ImmortalMonsterSpawner.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "TimerManager.h"
#include "UObject/ObjectKey.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"

namespace ImmortalQingyunBossWalkingPrivate
{
	constexpr const TCHAR* Flag = TEXT("ImmortalTestQingyunBossWalking");
	constexpr const TCHAR* BossClassPath = TEXT("/Game/GAME/BP_DesktopQingyunBoss.BP_DesktopQingyunBoss_C");
	constexpr const TCHAR* ClipPrefix = TEXT("/Game/GAME/Asset/DesktopPixelV2/QingyunBoss/FB_QingyunBoss_");
	constexpr float SampleInterval = 0.05f;
	constexpr float ObservationSeconds = 18.0f;
	bool bPreflightAccepted = false;
	bool bPreflightChecked = false;
	bool bClaimed = false;
	FString AcceptedDirectory;

	bool SafeDirectory(FString& Directory, FString& Reason)
	{
		const TCHAR* Cursor = FCommandLine::Get();
		FString Token;
		while (FParse::Token(Cursor, Token, false))
		{
			while (Token.StartsWith(TEXT("-")) || Token.StartsWith(TEXT("/"))) Token.RightChopInline(1);
			if (!Token.StartsWith(TEXT("ImmortalTest"), ESearchCase::IgnoreCase)) continue;
			if (!Token.Equals(TEXT("ImmortalTestQingyunBossWalking"), ESearchCase::IgnoreCase)
				&& !Token.Equals(TEXT("ImmortalTestStage=10"), ESearchCase::IgnoreCase))
			{
				Reason = FString::Printf(TEXT("additional development switch is forbidden in this untouched observer: %s"), *Token);
				return false;
			}
		}
		int32 Modes = 0;
		for (const TCHAR* Mode : {Flag, TEXT("ImmortalTestFoxArt"), TEXT("ImmortalTestFoxWalking"),
			TEXT("ImmortalTestDogArt"), TEXT("ImmortalTestDogWalking"), TEXT("ImmortalTestQingyunBossArt")})
			Modes += static_cast<int32>(FParse::Param(FCommandLine::Get(), Mode));
		int32 Stage = 0;
		if (Modes != 1) { Reason = TEXT("exactly one actor-fixture mode is required"); return false; }
		if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestDisableAutoBattle")))
		{ Reason = TEXT("DisableAutoBattle is forbidden: natural combat must remain enabled"); return false; }
		if (!FParse::Param(FCommandLine::Get(), TEXT("FixedSeed"))
			|| !FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestStage="), Stage) || Stage != 10)
		{ Reason = TEXT("FixedSeed and ImmortalTestStage=10 are required"); return false; }
		if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), Directory) || Directory.IsEmpty())
		{ Reason = TEXT("an explicit isolated UserDir is required"); return false; }
		FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/Automation/QingyunBossWalking")));
		Directory = FPaths::ConvertRelativePathToFull(Directory);
		FPaths::NormalizeDirectoryName(Directory);
		FPaths::NormalizeDirectoryName(Root);
		if (!FPaths::CollapseRelativeDirectories(Directory) || !FPaths::CollapseRelativeDirectories(Root)
			|| !FPaths::IsUnderDirectory(Directory, Root) || Directory.Equals(Root, ESearchCase::IgnoreCase))
		{ Reason = TEXT("UserDir must be a fresh child of Saved/Automation/QingyunBossWalking, never the root or formal save directory"); return false; }
		TArray<FString> PriorSaves, PriorCaptures;
		IFileManager::Get().FindFilesRecursive(PriorSaves, *Directory, TEXT("*.sav"), true, false);
		IFileManager::Get().FindFilesRecursive(PriorCaptures, *Directory, TEXT("R07_QingyunBoss_Walking_*.png"), true, false);
		if (!PriorSaves.IsEmpty() || !PriorCaptures.IsEmpty())
		{ Reason = TEXT("UserDir contains prior saves or observer captures; choose a fresh child (launcher logs are allowed)"); return false; }
		return true;
	}

	float ReadFloat(const UObject* Object, const FName Name)
	{
		const FFloatProperty* Property = Object ? FindFProperty<FFloatProperty>(Object->GetClass(), Name) : nullptr;
		return Property ? Property->GetPropertyValue_InContainer(Object) : -1.0f;
	}

	struct FActorStats
	{
		TWeakObjectPtr<AImmortalMonsterCharacter> Actor;
		FString Name;
		int32 Samples = 0;
		int32 DeadSamples = 0;
		int32 GroundedSamples = 0;
		int32 GroundStreak = 0;
		int32 MaxGroundStreak = 0;
		int32 MoveSamples = 0;
		int32 MoveAdvances = 0;
		int32 IdleSamples = 0;
		int32 IdleAdvances = 0;
		int32 AttackSamples = 0;
		int32 HurtSamples = 0;
		int32 DeathSamples = 0;
		int32 HealthDecreaseSamples = 0;
		int32 AuthoredSamples = 0;
		int32 JointVisibleSamples = 0;
		int32 VisualAlignedSamples = 0;
		int32 VisualAlignedStreak = 0;
		int32 MaxVisualAlignedStreak = 0;
		int32 VisualMismatchStreak = 0;
		int32 MaxVisualMismatchStreak = 0;
		int32 ScaleMismatchSamples = 0;
		int32 UnexpectedLiveModeSamples = 0;
		int32 SharedGroundSamples = 0;
		int32 DestroyingSamples = 0;
		bool bLastObservedDead = false;
		bool bDisappearanceLogged = false;
		TSet<int32> MovementModes;
		TSet<int32> MovePoses;
		TSet<int32> IdlePoses;
		float LastMovePosition = -1.0f;
		float LastIdlePosition = -1.0f;
		float LastHealth = -1.0f;
		double FirstMoveX = 0;
		double MaximumMoveDisplacement = 0;
		double MaximumTowardPlayerProgress = 0;
		double MoveDirection = 0;
		double LargestGroundedFootError = 0;
		double LargestGroundedDepthError = 0;
		double LargestSharedCapsuleBottomGap = 0;
		double LargestSharedSpriteFootGap = 0;
		float NextLogAt = 0;
	};

	struct FVisualProbe
	{
		double CapsuleBottom = 0;
		double PlayerReferenceOffset = 0;
		double ExpectedFoot = 0;
		double FootError = BIG_NUMBER;
		double DepthError = BIG_NUMBER;
		FVector Scale = FVector::ZeroVector;
		FVector Foot = FVector::ZeroVector;
		bool bHasComponents = false;
	};

	FVisualProbe MeasureVisual(const APaperCharacter& Actor, const AImmortalPlayerCharacter& Player)
	{
		FVisualProbe Probe;
		const UCapsuleComponent* Capsule = Actor.GetCapsuleComponent();
		const UCapsuleComponent* PlayerCapsule = Player.GetCapsuleComponent();
		const UPaperFlipbookComponent* Sprite = Actor.GetSprite();
		const UPaperFlipbookComponent* PlayerSprite = Player.GetSprite();
		if (!Capsule || !PlayerCapsule || !Sprite || !PlayerSprite) return Probe;
		Probe.bHasComponents = true;
		Probe.CapsuleBottom = Capsule->GetComponentLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
		// Independent reference: do not call the production visual-placement helper.
		Probe.PlayerReferenceOffset = PlayerSprite->GetComponentLocation().Z
			- (PlayerCapsule->GetComponentLocation().Z - PlayerCapsule->GetScaledCapsuleHalfHeight());
		Probe.ExpectedFoot = Probe.CapsuleBottom + Probe.PlayerReferenceOffset;
		Probe.Foot = Sprite->GetComponentLocation();
		Probe.FootError = FMath::Abs(Probe.Foot.Z - Probe.ExpectedFoot);
		Probe.DepthError = FMath::Abs(Probe.Foot.Y - PlayerSprite->GetComponentLocation().Y);
		Probe.Scale = Sprite->GetComponentScale();
		return Probe;
	}

	bool InsideViewport(const APaperCharacter& Actor, const APlayerController* Controller)
	{
		const UPaperFlipbookComponent* Sprite = Actor.GetSprite();
		if (!Sprite || !Sprite->IsVisible() || Actor.IsHidden() || !Controller) return false;
		int32 Width = 0, Height = 0;
		Controller->GetViewportSize(Width, Height);
		if (Width <= 0 || Height <= 0) return false;
		const FBoxSphereBounds Bounds = Sprite->CalcBounds(Sprite->GetComponentTransform());
		if (Bounds.BoxExtent.X <= 0 || Bounds.BoxExtent.Z <= 0) return false;
		for (const double X : {-1.0, 1.0}) for (const double Z : {-1.0, 1.0})
		{
			FVector2D Screen;
			if (!UGameplayStatics::ProjectWorldToScreen(Controller,
				Bounds.Origin + FVector(X * Bounds.BoxExtent.X, 0, Z * Bounds.BoxExtent.Z), Screen, false)
				|| Screen.X < 0 || Screen.Y < 0 || Screen.X > Width || Screen.Y > Height) return false;
		}
		return true;
	}

	struct FCapture
	{
		FString Name;
		uint64 EarliestFrame = 0;
		TWeakObjectPtr<AImmortalMonsterCharacter> Boss;
	};

	struct FState
	{
		TWeakObjectPtr<AImmortalPlayerCharacter> Player;
		TWeakObjectPtr<UWorld> World;
		FString Directory;
		TMap<FObjectKey, FActorStats> BossStats;
		TArray<FCapture> PendingCaptures;
		TSet<FString> QueuedCaptures;
		FTimerHandle SampleTimer;
		float StartedAt = 0;
		float LastCaptureAt = -10;
		uint64 LastCaptureFrame = 0;
		int32 Samples = 0;
		int32 PlayerDeadSamples = 0;
		int32 PausedSamples = 0;
		int32 PlayerGroundStreak = 0;
		int32 MaxPlayerGroundStreak = 0;
		int32 PlayerUnexpectedLiveModeSamples = 0;
		int32 PlayerHealthDecreaseSamples = 0;
		float LastPlayerHealth = -1;
		float NextPlayerLogAt = 0;
		int32 IdentityMismatchSamples = 0;
		int32 Checks = 0;
		int32 Failures = 0;
		int32 ScreenshotsRequested = 0;
		bool bFinished = false;

		void Check(const bool bPassed, const TCHAR* Label)
		{
			++Checks;
			if (!bPassed) ++Failures;
			UE_LOG(LogTemp, Display, TEXT("R07QingyunBossWalking check %03d: %s | %s"),
				Checks, bPassed ? TEXT("PASS") : TEXT("FAIL"), Label);
		}

		void QueueCapture(const TCHAR* Name, AImmortalMonsterCharacter* Boss = nullptr)
		{
			if (QueuedCaptures.Contains(Name)) return;
			QueuedCaptures.Add(Name);
			PendingCaptures.Add({Name, GFrameCounter + 1, Boss});
			UE_LOG(LogTemp, Display, TEXT("R07QingyunBossWalking capture queued=%s observedFrame=%llu earliestFrame=%llu"),
				Name, static_cast<unsigned long long>(GFrameCounter), static_cast<unsigned long long>(GFrameCounter + 1));
		}

		void RequestOneCapture(const float Elapsed)
		{
			if (PendingCaptures.IsEmpty() || GFrameCounter <= LastCaptureFrame + 1 || Elapsed - LastCaptureAt < 0.25f
				|| FScreenshotRequest::IsScreenshotRequested()) return;
			int32 CaptureIndex = INDEX_NONE;
			for (int32 Index = 0; Index < PendingCaptures.Num(); ++Index)
			{
				const FCapture& Capture = PendingCaptures[Index];
				if (GFrameCounter < Capture.EarliestFrame) continue;
				if (Capture.Name == TEXT("Final")) { CaptureIndex = Index; break; }
				AImmortalMonsterCharacter* Boss = Capture.Boss.Get();
				AImmortalPlayerCharacter* Owner = Player.Get();
				if (!Boss || !Owner || Boss->IsDead() || Boss->IsActorBeingDestroyed()) continue;
				const APlayerController* Controller = Cast<APlayerController>(Owner->GetController());
				UPaperFlipbookComponent* Sprite = Boss->GetSprite();
				const FString Clip = Sprite && Sprite->GetFlipbook() ? Sprite->GetFlipbook()->GetPathName() : TEXT("");
				if (!Clip.StartsWith(ClipPrefix) || !InsideViewport(*Boss, Controller) || !InsideViewport(*Owner, Controller)) continue;
				const UCharacterMovementComponent* Movement = Boss->GetCharacterMovement();
				const bool bMove = Movement && Movement->IsMovingOnGround() && Movement->CurrentFloor.IsWalkableFloor()
					&& FMath::Abs(Boss->GetVelocity().X) > 5.0f && Clip.EndsWith(TEXT("FB_QingyunBoss_Move"))
					&& Sprite->IsPlaying() && Sprite->IsLooping();
				const bool bBattle = Clip.EndsWith(TEXT("FB_QingyunBoss_Attack")) || Clip.EndsWith(TEXT("FB_QingyunBoss_Hurt"));
				if ((Capture.Name == TEXT("Move") && bMove) || (Capture.Name == TEXT("Battle") && bBattle))
				{
					CaptureIndex = Index;
					UE_LOG(LogTemp, Display, TEXT("R07QingyunBossWalking capture revalidated=%s actor=%s clip=%s pose=%d jointVisible=true"),
						*Capture.Name, *Boss->GetName(), *Clip, Sprite->GetPlaybackPositionInFrames());
					break;
				}
			}
			// A stale queued pose never becomes evidence merely through its filename.
			// Wait for this same actor's next natural matching state, without forcing it.
			if (CaptureIndex == INDEX_NONE) return;
			const FString Name = PendingCaptures[CaptureIndex].Name;
			PendingCaptures.RemoveAt(CaptureIndex);
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory,
				FString::Printf(TEXT("R07_QingyunBoss_Walking_%s.png"), *Name)), true, false);
			LastCaptureAt = Elapsed;
			LastCaptureFrame = GFrameCounter;
			++ScreenshotsRequested;
			UE_LOG(LogTemp, Display, TEXT("R07QingyunBossWalking capture requested=%s elapsed=%.3f frame=%llu"),
				*Name, Elapsed, static_cast<unsigned long long>(GFrameCounter));
		}

		void Finish()
		{
			if (bFinished) return;
			bFinished = true;
			if (World.IsValid()) World->GetTimerManager().ClearTimer(SampleTimer);
			int32 MaxGround = 0, MaxMove = 0, MaxMoveAdvances = 0, MaxMovePoses = 0;
			int32 MaxIdle = 0, MaxIdleAdvances = 0, MaxIdlePoses = 0, Joint = 0;
			int32 Attack = 0, Hurt = 0, Death = 0, HealthDecreases = 0, Aligned = 0, Misaligned = 0;
			int32 UnexpectedModes = 0, ScaleMismatch = 0, SharedGroundSamples = 0;
			double FootError = 0, DepthError = 0, Progress = 0;
			double SharedCapsuleGap = 0, SharedFootGap = 0;
			bool bOneActorGroundedMoveEvidence = false;
			bool bOneActorIdleEvidence = false;
			for (const TPair<FObjectKey, FActorStats>& Pair : BossStats)
			{
				const FActorStats& S = Pair.Value;
				MaxGround = FMath::Max(MaxGround, S.MaxGroundStreak);
				MaxMove = FMath::Max(MaxMove, S.MoveSamples);
				MaxMoveAdvances = FMath::Max(MaxMoveAdvances, S.MoveAdvances);
				MaxMovePoses = FMath::Max(MaxMovePoses, S.MovePoses.Num());
				MaxIdle = FMath::Max(MaxIdle, S.IdleSamples);
				MaxIdleAdvances = FMath::Max(MaxIdleAdvances, S.IdleAdvances);
				MaxIdlePoses = FMath::Max(MaxIdlePoses, S.IdlePoses.Num());
				Joint += S.JointVisibleSamples;
				Attack += S.AttackSamples; Hurt += S.HurtSamples; Death += S.DeathSamples;
				HealthDecreases += S.HealthDecreaseSamples;
				Aligned = FMath::Max(Aligned, S.MaxVisualAlignedStreak);
				Misaligned = FMath::Max(Misaligned, S.MaxVisualMismatchStreak);
				UnexpectedModes += S.UnexpectedLiveModeSamples;
				ScaleMismatch += S.ScaleMismatchSamples;
				FootError = FMath::Max(FootError, S.LargestGroundedFootError);
				DepthError = FMath::Max(DepthError, S.LargestGroundedDepthError);
				Progress = FMath::Max(Progress, S.MaximumTowardPlayerProgress);
				SharedGroundSamples += S.SharedGroundSamples;
				SharedCapsuleGap = FMath::Max(SharedCapsuleGap, S.LargestSharedCapsuleBottomGap);
				SharedFootGap = FMath::Max(SharedFootGap, S.LargestSharedSpriteFootGap);
				bOneActorGroundedMoveEvidence |= S.MaxGroundStreak >= 6 && S.MoveSamples >= 3
					&& S.MoveAdvances >= 2 && S.MovePoses.Num() >= 3 && S.MaximumTowardPlayerProgress > 5.0;
				bOneActorIdleEvidence |= S.IdleSamples >= 3 && S.IdleAdvances >= 2 && S.IdlePoses.Num() >= 3;
				UE_LOG(LogTemp, Display, TEXT("R07QingyunBossWalking actor=%s samples=%d deadSamples=%d grounded=%d maxGroundStreak=%d moveSamples=%d moveAdvances=%d movePoses=%d moveDisplacement=%.3f towardPlayerProgress=%.3f idleSamples=%d idleAdvances=%d idlePoses=%d attackSamples=%d hurtSamples=%d deathSamples=%d healthDecreases=%d jointVisible=%d alignedSamples=%d maxAlignedStreak=%d maxMismatchStreak=%d largestGroundedFootError=%.3f largestGroundedDepthError=%.3f scaleMismatch=%d unexpectedLiveModes=%d"),
					*S.Name, S.Samples, S.DeadSamples, S.GroundedSamples, S.MaxGroundStreak,
					S.MoveSamples, S.MoveAdvances, S.MovePoses.Num(), S.MaximumMoveDisplacement, S.MaximumTowardPlayerProgress,
					S.IdleSamples, S.IdleAdvances, S.IdlePoses.Num(), S.AttackSamples, S.HurtSamples,
					S.DeathSamples, S.HealthDecreaseSamples, S.JointVisibleSamples, S.VisualAlignedSamples,
					S.MaxVisualAlignedStreak, S.MaxVisualMismatchStreak, S.LargestGroundedFootError,
					S.LargestGroundedDepthError, S.ScaleMismatchSamples, S.UnexpectedLiveModeSamples);
				for (const int32 Mode : S.MovementModes)
					UE_LOG(LogTemp, Display, TEXT("R07QingyunBossWalking actor=%s observedMovementMode=%d"), *S.Name, Mode);
				UE_LOG(LogTemp, Display, TEXT("R07QingyunBossWalking actor=%s sharedGroundSamples=%d largestSharedCapsuleBottomGap=%.3f largestSharedSpriteFootGap=%.3f destroyingSamples=%d disappeared=%s lastObservedDead=%s. Disappearance while alive is not a Boss death observation."),
					*S.Name, S.SharedGroundSamples, S.LargestSharedCapsuleBottomGap, S.LargestSharedSpriteFootGap,
					S.DestroyingSamples, S.bDisappearanceLogged ? TEXT("true") : TEXT("false"), S.bLastObservedDead ? TEXT("true") : TEXT("false"));
			}
			Check(Samples >= 150, TEXT("Observer collected an extended natural-combat timeline from early settling onward"));
			Check(BossStats.Num() > 0 && IdentityMismatchSamples == 0, TEXT("Actual new Qingyun map Boss is naturally owned by the real map Spawner"));
			Check(MaxPlayerGroundStreak >= 6, TEXT("Unmodified player repeatedly settles on a walkable floor"));
			Check(MaxGround >= 6, TEXT("At least one real Boss has six consecutive grounded Walking/NavWalking samples"));
			Check(MaxMove >= 3 && MaxMoveAdvances >= 2 && MaxMovePoses >= 3, TEXT("Grounded natural Move really plays and advances through distinct poses"));
			Check(Progress > 5.0 && bOneActorGroundedMoveEvidence, TEXT("The same naturally grounded moving Boss makes real X progress toward the player"));
			Check(MaxIdle >= 3 && MaxIdleAdvances >= 2 && MaxIdlePoses >= 3 && bOneActorIdleEvidence,
				TEXT("The same naturally grounded Boss Idle actually advances through distinct poses"));
			Check(Attack + Hurt > 0, TEXT("At least one actual Attack or Hurt clip occurs with normal combat enabled"));
			Check(Joint >= 5, TEXT("Complete Boss and player sprite bounds are jointly inside the source viewport repeatedly"));
			Check(Aligned >= 6 && FootError <= 1.5 && DepthError <= 1.5 && Misaligned == 0,
				TEXT("Grounded Boss foot/depth remains aligned to independent capsule/player reference without persistent mismatch"));
			Check(SharedGroundSamples >= 6 && SharedCapsuleGap <= 1.5 && SharedFootGap <= 1.5,
				TEXT("After settling, jointly grounded Boss/player capsule bottoms and visible sprite feet share the actual ground lane"));
			Check(ScaleMismatch == 0, TEXT("Observed promoted Boss visual scale is 2.7 without accumulated scaling"));
			Check(UnexpectedModes == 0 && PlayerUnexpectedLiveModeSamples == 0,
				TEXT("No live Boss/player samples use Flying, Swimming, Custom or disabled movement"));
			Check(PausedSamples == 0, TEXT("Observed timeline is not paused"));
			Check(ScreenshotsRequested == 3 && QueuedCaptures.Num() == 3 && PendingCaptures.IsEmpty(),
				TEXT("Three distinct delayed Move/Battle/Final captures were requested on separate frames"));
			UE_LOG(LogTemp, Display, TEXT("R07QingyunBossWalking coverage: bosses=%d samples=%d playerDeadSamples=%d playerHealthDecreases=%d bossHealthDecreases=%d attackObserved=%s hurtObserved=%s deathObserved=%s. Attack/Hurt/Death counts are observations, not guaranteed coverage when zero. HP changes and clip names do not identify a damage causer; no player-versus-pet damage attribution is inferred. No damage-contact timing, skill art, balance, native desktop transparency or multi-DPI claim. All evidence accumulated across death/destruction; no dead-player early return. Fixture only reads actors, Spawner, floor, movement, animation and camera; it never spawns, teleports, changes stats/modes/collision/floor/camera, advances stages or suppresses battle."),
				BossStats.Num(), Samples, PlayerDeadSamples, PlayerHealthDecreaseSamples, HealthDecreases,
				Attack > 0 ? TEXT("true") : TEXT("false"), Hurt > 0 ? TEXT("true") : TEXT("false"), Death > 0 ? TEXT("true") : TEXT("false"));
			UE_LOG(LogTemp, Display, TEXT("R07QingyunBossWalking finished: checks=%d failures=%d screenshotsRequested=%d RESULT: %s"),
				Checks, Failures, ScreenshotsRequested, Failures == 0 ? TEXT("PASS") : TEXT("FAIL"));
			if (Failures > 0) UE_LOG(LogTemp, Error, TEXT("R07QingyunBossWalking failed %d natural-map checks"), Failures);
			FPlatformMisc::RequestExitWithStatus(false, Failures == 0 ? 0 : 1);
		}
	};

	bool Grounded(const UCharacterMovementComponent* Movement)
	{
		return Movement && (Movement->MovementMode == MOVE_Walking || Movement->MovementMode == MOVE_NavWalking)
			&& Movement->IsMovingOnGround() && Movement->CurrentFloor.IsWalkableFloor();
	}

	bool UnexpectedLiveMode(const UCharacterMovementComponent* Movement)
	{
		return !Movement || (Movement->MovementMode != MOVE_Walking && Movement->MovementMode != MOVE_NavWalking
			&& Movement->MovementMode != MOVE_Falling);
	}

	// Own a strong reference for the complete callback. Finish clears its own
	// timer/delegate, so borrowing the delegate's last reference would destroy
	// the evidence maps before their final assertions were evaluated.
	void Sample(TSharedRef<FState> State)
	{
		UWorld* World = State->World.Get();
		AImmortalPlayerCharacter* Player = State->Player.Get();
		if (!World || !Player || State->bFinished)
		{
			if (!State->bFinished) { State->Check(false, TEXT("Observed world/player remains available")); State->Finish(); }
			return;
		}
		const float Elapsed = World->GetTimeSeconds() - State->StartedAt;
		++State->Samples;
		if (UGameplayStatics::IsGamePaused(World)) ++State->PausedSamples;
		if (Player->IsDead()) ++State->PlayerDeadSamples;
		const bool bPlayerGrounded = Grounded(Player->GetCharacterMovement());
		State->PlayerGroundStreak = bPlayerGrounded ? State->PlayerGroundStreak + 1 : 0;
		State->MaxPlayerGroundStreak = FMath::Max(State->MaxPlayerGroundStreak, State->PlayerGroundStreak);
		if (!Player->IsDead() && UnexpectedLiveMode(Player->GetCharacterMovement())) ++State->PlayerUnexpectedLiveModeSamples;
		if (State->LastPlayerHealth >= 0 && Player->GetCurrentHealth() < State->LastPlayerHealth - 0.001f) ++State->PlayerHealthDecreaseSamples;
		State->LastPlayerHealth = Player->GetCurrentHealth();
		const APlayerController* Controller = Cast<APlayerController>(Player->GetController());
		const bool bPlayerVisible = InsideViewport(*Player, Controller);
		if (Elapsed >= State->NextPlayerLogAt)
		{
			State->NextPlayerLogAt = Elapsed + 0.5f;
			const UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
			UPaperFlipbookComponent* Sprite = Player->GetSprite();
			UE_LOG(LogTemp, Display, TEXT("R07QingyunBossWalking player elapsed=%.3f settling=%s dead=%s mode=%d grounded=%s location=%s health=%.3f actualAttackRange=%.3f clip=%s pose=%d"),
				Elapsed, Elapsed < 2 ? TEXT("true") : TEXT("false"), Player->IsDead() ? TEXT("true") : TEXT("false"),
				Movement ? static_cast<int32>(Movement->MovementMode) : -1, bPlayerGrounded ? TEXT("true") : TEXT("false"),
				*Player->GetActorLocation().ToString(), Player->GetCurrentHealth(), ReadFloat(Player, TEXT("AttackRange")),
				Sprite && Sprite->GetFlipbook() ? *Sprite->GetFlipbook()->GetPathName() : TEXT("none"), Sprite ? Sprite->GetPlaybackPositionInFrames() : -1);
		}
		for (TActorIterator<AImmortalMonsterCharacter> It(World); It; ++It)
		{
			AImmortalMonsterCharacter* Boss = *It;
			if (Boss->GetClass()->GetPathName() != BossClassPath) continue;
			const AImmortalMonsterSpawner* Spawner = Cast<AImmortalMonsterSpawner>(Boss->GetOwner());
			if (!Spawner || !Boss->IsBoss() || Boss->IsWorldBoss() || Boss->IsEndlessEnemy()
				|| Boss->GetConfiguredMapId() != UImmortalMapLibrary::GetQingyunMountainId()
				|| Spawner->GetActiveMapId() != UImmortalMapLibrary::GetQingyunMountainId())
			{ ++State->IdentityMismatchSamples; continue; }
			FActorStats& S = State->BossStats.FindOrAdd(FObjectKey(Boss));
			S.Actor = Boss;
			S.Name = Boss->GetName();
			++S.Samples;
			S.bLastObservedDead = Boss->IsDead();
			if (Boss->IsDead()) ++S.DeadSamples;
			if (Boss->IsActorBeingDestroyed()) { ++S.DestroyingSamples; continue; }
			const UCharacterMovementComponent* Movement = Boss->GetCharacterMovement();
			UPaperFlipbookComponent* Sprite = Boss->GetSprite();
			const bool bGrounded = Grounded(Movement);
			S.MovementModes.Add(Movement ? static_cast<int32>(Movement->MovementMode) : -1);
			if (!Boss->IsDead() && UnexpectedLiveMode(Movement)) ++S.UnexpectedLiveModeSamples;
			S.GroundedSamples += bGrounded ? 1 : 0;
			S.GroundStreak = bGrounded ? S.GroundStreak + 1 : 0;
			S.MaxGroundStreak = FMath::Max(S.MaxGroundStreak, S.GroundStreak);
			const FString ClipName = Sprite && Sprite->GetFlipbook() ? Sprite->GetFlipbook()->GetPathName() : TEXT("none");
			const bool bAuthored = ClipName.StartsWith(ClipPrefix);
			S.AuthoredSamples += bAuthored ? 1 : 0;
			const bool bMove = bGrounded && bAuthored && ClipName.EndsWith(TEXT("FB_QingyunBoss_Move"))
				&& FMath::Abs(Boss->GetVelocity().X) > 5 && Sprite->IsPlaying() && Sprite->IsLooping();
			const bool bIdle = bGrounded && bAuthored && ClipName.EndsWith(TEXT("FB_QingyunBoss_Idle"))
				&& Sprite->IsPlaying() && Sprite->IsLooping();
			const bool bAttack = bAuthored && ClipName.EndsWith(TEXT("FB_QingyunBoss_Attack"));
			const bool bHurt = bAuthored && ClipName.EndsWith(TEXT("FB_QingyunBoss_Hurt"));
			const bool bDeath = bAuthored && ClipName.EndsWith(TEXT("FB_QingyunBoss_Death"));
			S.AttackSamples += bAttack ? 1 : 0; S.HurtSamples += bHurt ? 1 : 0; S.DeathSamples += bDeath ? 1 : 0;
			if (S.LastHealth >= 0 && Boss->GetCurrentHealth() < S.LastHealth - 0.001f) ++S.HealthDecreaseSamples;
			S.LastHealth = Boss->GetCurrentHealth();
			if (bMove)
			{
				if (S.MoveSamples == 0)
				{
					S.FirstMoveX = Boss->GetActorLocation().X;
					S.MoveDirection = FMath::Sign(Player->GetActorLocation().X - S.FirstMoveX);
				}
				++S.MoveSamples;
				S.MovePoses.Add(Sprite->GetPlaybackPositionInFrames());
				const float Position = Sprite->GetPlaybackPosition();
				if (S.LastMovePosition >= 0 && !FMath::IsNearlyEqual(Position, S.LastMovePosition, 0.001f)) ++S.MoveAdvances;
				S.LastMovePosition = Position;
				const double Delta = Boss->GetActorLocation().X - S.FirstMoveX;
				S.MaximumMoveDisplacement = FMath::Max(S.MaximumMoveDisplacement, FMath::Abs(Delta));
				S.MaximumTowardPlayerProgress = FMath::Max(S.MaximumTowardPlayerProgress, Delta * S.MoveDirection);
			}
			else S.LastMovePosition = -1;
			if (bIdle)
			{
				++S.IdleSamples;
				S.IdlePoses.Add(Sprite->GetPlaybackPositionInFrames());
				const float Position = Sprite->GetPlaybackPosition();
				if (S.LastIdlePosition >= 0 && !FMath::IsNearlyEqual(Position, S.LastIdlePosition, 0.001f)) ++S.IdleAdvances;
				S.LastIdlePosition = Position;
			}
			else S.LastIdlePosition = -1;
			const FVisualProbe Probe = MeasureVisual(*Boss, *Player);
			const bool bScaleCorrect = Probe.bHasComponents && FMath::IsNearlyEqual(FMath::Abs(Probe.Scale.X), 2.7, 0.01)
				&& FMath::IsNearlyEqual(Probe.Scale.Z, 2.7, 0.01);
			if (!bScaleCorrect) ++S.ScaleMismatchSamples;
			const bool bAligned = Probe.FootError <= 1.5 && Probe.DepthError <= 1.5 && bScaleCorrect;
			S.VisualMismatchStreak = bGrounded && !bAligned ? S.VisualMismatchStreak + 1 : 0;
			S.MaxVisualMismatchStreak = FMath::Max(S.MaxVisualMismatchStreak, S.VisualMismatchStreak);
			S.VisualAlignedStreak = bGrounded && bAligned ? S.VisualAlignedStreak + 1 : 0;
			S.MaxVisualAlignedStreak = FMath::Max(S.MaxVisualAlignedStreak, S.VisualAlignedStreak);
			if (bGrounded)
			{
				S.VisualAlignedSamples += bAligned ? 1 : 0;
				S.LargestGroundedFootError = FMath::Max(S.LargestGroundedFootError, Probe.FootError);
				S.LargestGroundedDepthError = FMath::Max(S.LargestGroundedDepthError, Probe.DepthError);
			}
			if (Elapsed >= 2.0f && bGrounded && bPlayerGrounded && Player->GetCapsuleComponent() && Player->GetSprite())
			{
				++S.SharedGroundSamples;
				const double PlayerBottom = Player->GetCapsuleComponent()->GetComponentLocation().Z - Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
				S.LargestSharedCapsuleBottomGap = FMath::Max(S.LargestSharedCapsuleBottomGap, FMath::Abs(Probe.CapsuleBottom - PlayerBottom));
				S.LargestSharedSpriteFootGap = FMath::Max(S.LargestSharedSpriteFootGap, FMath::Abs(Probe.Foot.Z - Player->GetSprite()->GetComponentLocation().Z));
			}
			const bool bJointVisible = bPlayerVisible && InsideViewport(*Boss, Controller);
			if (bJointVisible) ++S.JointVisibleSamples;
			if (bMove && bJointVisible) State->QueueCapture(TEXT("Move"), Boss);
			if ((bAttack || bHurt) && bJointVisible) State->QueueCapture(TEXT("Battle"), Boss);
			if (Elapsed >= S.NextLogAt)
			{
				S.NextLogAt = Elapsed + 0.5f;
				UE_LOG(LogTemp, Display, TEXT("R07QingyunBossWalking sample elapsed=%.3f settling=%s actor=%s owner=%s stage=%d playerDead=%s bossDead=%s mode=%d grounded=%s walkable=%s floorDistance=%.3f location=%s vx=%.3f capsuleBottom=%.3f playerReferenceOffset=%.3f expectedFoot=%.3f foot=%s footError=%.3f depthError=%.3f scale=%s jointVisible=%s clip=%s pose=%d playing=%s bossHealth=%.3f bossMaxHealth=%.3f bossAttackRange=%.3f playerHealth=%.3f playerAttackRange=%.3f horizontalDistance=%.3f distance3D=%.3f combatTarget=%s damageAttribution=not_inferred"),
					Elapsed, Elapsed < 2 ? TEXT("true") : TEXT("false"), *S.Name, *Spawner->GetName(), Spawner->GetCurrentStage(),
					Player->IsDead() ? TEXT("true") : TEXT("false"), Boss->IsDead() ? TEXT("true") : TEXT("false"),
					Movement ? static_cast<int32>(Movement->MovementMode) : -1, bGrounded ? TEXT("true") : TEXT("false"),
					Movement && Movement->CurrentFloor.IsWalkableFloor() ? TEXT("true") : TEXT("false"), Movement ? Movement->CurrentFloor.FloorDist : -1,
					*Boss->GetActorLocation().ToString(), Boss->GetVelocity().X, Probe.CapsuleBottom, Probe.PlayerReferenceOffset,
					Probe.ExpectedFoot, *Probe.Foot.ToString(), Probe.FootError, Probe.DepthError, *Probe.Scale.ToString(),
					bJointVisible ? TEXT("true") : TEXT("false"), *ClipName, Sprite ? Sprite->GetPlaybackPositionInFrames() : -1,
					Sprite && Sprite->IsPlaying() ? TEXT("true") : TEXT("false"), Boss->GetCurrentHealth(), Boss->GetMaxHealth(),
					ReadFloat(Boss, TEXT("AttackRange")), Player->GetCurrentHealth(), ReadFloat(Player, TEXT("AttackRange")),
					FMath::Abs(Boss->GetActorLocation().X - Player->GetActorLocation().X),
					FVector::Distance(Boss->GetActorLocation(), Player->GetActorLocation()), *GetNameSafe(Boss->GetCombatTarget()));
			}
		}
		for (TPair<FObjectKey, FActorStats>& Pair : State->BossStats)
		{
			FActorStats& S = Pair.Value;
			if (!S.Actor.IsValid() && !S.bDisappearanceLogged)
			{
				S.bDisappearanceLogged = true;
				UE_LOG(LogTemp, Display, TEXT("R07QingyunBossWalking actor disappeared elapsed=%.3f actor=%s lastObservedDead=%s playerDead=%s. Removal/destruction does not count as death unless IsDead was actually observed."),
					Elapsed, *S.Name, S.bLastObservedDead ? TEXT("true") : TEXT("false"), Player->IsDead() ? TEXT("true") : TEXT("false"));
			}
		}
		if (Elapsed >= 16.5f) State->QueueCapture(TEXT("Final"));
		State->RequestOneCapture(Elapsed);
		if (Elapsed >= ObservationSeconds) State->Finish();
	}
}
#endif

bool ImmortalQingyunBossWalkingFixture::CheckPreflightIfRequested()
{
#if !UE_BUILD_SHIPPING
	using namespace ImmortalQingyunBossWalkingPrivate;
	if (!FParse::Param(FCommandLine::Get(), Flag)) return true;
	// Spawners can initialize before the player. Validate once before either
	// caller reads a save, then retain the result for this process.
	if (bPreflightChecked) return bPreflightAccepted;
	bPreflightChecked = true;
	FString Directory, Reason;
	if (!SafeDirectory(Directory, Reason))
	{
		UE_LOG(LogTemp, Error, TEXT("R07QingyunBossWalking refused before player initialization: %s"), *Reason);
		FPlatformMisc::RequestExitWithStatus(false, 2);
		return false;
	}
	bPreflightAccepted = true;
	AcceptedDirectory = Directory;
	UE_LOG(LogTemp, Display, TEXT("R07QingyunBossWalking preflight accepted: independent UserDir=%s, stage=10, FixedSeed, normal battle enabled"), *Directory);
#endif
	return true;
}

void ImmortalQingyunBossWalkingFixture::StartIfRequested(AImmortalPlayerCharacter& Player)
{
#if !UE_BUILD_SHIPPING
	using namespace ImmortalQingyunBossWalkingPrivate;
	if (!FParse::Param(FCommandLine::Get(), Flag) || bClaimed) return;
	if (!bPreflightAccepted)
	{
		UE_LOG(LogTemp, Error, TEXT("R07QingyunBossWalking refused: mandatory pre-initialization preflight was not accepted"));
		FPlatformMisc::RequestExitWithStatus(false, 2);
		return;
	}
	bClaimed = true;
	const TSharedRef<FState> State = MakeShared<FState>();
	State->Player = &Player;
	State->World = Player.GetWorld();
	State->Directory = AcceptedDirectory;
	State->Check(Player.GetWorld() && Player.GetWorld()->WorldType == EWorldType::Game, TEXT("Observer requires standalone Game world"));
	if (State->Failures) { State->Finish(); return; }
	State->StartedAt = Player.GetWorld()->GetTimeSeconds();
	UE_LOG(LogTemp, Display, TEXT("R07QingyunBossWalking started: read-only 18-second natural map observation; sampling=0.05s from early startup. First two seconds labelled settling, not discarded. No actor/Spawner/ground/camera/movement mutation. Source captures are delayed and serialized; absent natural Move/Battle is a coverage failure, never a substituted capture."));
	Player.GetWorldTimerManager().SetTimer(State->SampleTimer, FTimerDelegate::CreateLambda([State] { Sample(State); }), SampleInterval, true);
#endif
}
