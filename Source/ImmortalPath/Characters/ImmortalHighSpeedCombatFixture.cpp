// Development-only, real-actor regression for rapid pixel-player combat.
#include "ImmortalPlayerCharacter.h"

#if !UE_BUILD_SHIPPING

#include "ImmortalMonsterCharacter.h"
#include "ImmortalPetCharacter.h"
#include "../Spawning/ImmortalMonsterSpawner.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
	bool bHighSpeedFixtureClaimed = false;
	bool bHighSpeedFixtureFinished = false;

	struct FHighSpeedFixtureState
	{
		TWeakObjectPtr<AImmortalMonsterCharacter> Target;
		FString CaseName;
		FString Directory;
		float InitialTargetHealth = 0.0f;
		float LastTargetHealth = 0.0f;
		float TotalTargetDamage = 0.0f;
		float AttackStartTime = -1.0f;
		TArray<float> HitTimes;
		float FirstIncomingHitTime = -1.0f;
		float TargetHealthAtPlayerDeath = -1.0f;
		float FirstHurtTime = -1.0f;
		float LastHurtPosition = -1.0f;
		float HurtRunStart = -1.0f;
		float LongestHurtRun = 0.0f;
		int32 DamageEvents = 0;
		int32 LateAttackFrameSamples = 0;
		int32 HurtStarts = 0;
		int32 HurtPositionRegressions = 0;
		bool bWasHurt = false;
		bool bFollowupHurtInjected = false;
		bool bInitialHurtInjected = false;
		bool bPlayerDeathInjected = false;
		FTimerHandle SampleTimer;
	};

	bool IsKnownCase(const FString& CaseName)
	{
		return CaseName == TEXT("cadence-1x")
			|| CaseName == TEXT("cadence-2x")
			|| CaseName == TEXT("cadence-4x")
			|| CaseName == TEXT("hurt")
			|| CaseName == TEXT("target-death")
			|| CaseName == TEXT("player-death");
	}

	bool GetSafeFixtureDirectory(const FString& CaseName, FString& OutDirectory)
	{
		FString UserDirectory;
		if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirectory)) return false;
		OutDirectory = FPaths::ConvertRelativePathToFull(UserDirectory);
		FString Expected = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectDir(), TEXT("Saved/Automation/HighSpeedCombat"), CaseName));
		FPaths::NormalizeDirectoryName(OutDirectory);
		FPaths::NormalizeDirectoryName(Expected);
		FPaths::CollapseRelativeDirectories(OutDirectory);
		FPaths::CollapseRelativeDirectories(Expected);
		return OutDirectory.Equals(Expected, ESearchCase::IgnoreCase)
			|| FPaths::IsUnderDirectory(OutDirectory, Expected);
	}

	void FinishHighSpeedFixture(const bool bPassed, const FString& Detail)
	{
		if (bHighSpeedFixtureFinished) return;
		bHighSpeedFixtureFinished = true;
		UE_LOG(LogTemp, Display, TEXT("R05HighSpeedFixture RESULT: %s | %s"),
			bPassed ? TEXT("PASS") : TEXT("FAIL"), *Detail);
		if (!bPassed) UE_LOG(LogTemp, Error, TEXT("R05HighSpeedFixture failed: %s"), *Detail);
		FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
	}
}

#endif

void AImmortalPlayerCharacter::RunHighSpeedCombatFixture()
{
#if !UE_BUILD_SHIPPING
	FString CaseName;
	if (!FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestHighSpeedCombatCase="), CaseName)
		|| bHighSpeedFixtureClaimed)
	{
		return;
	}
	FString Directory;
	if (!IsKnownCase(CaseName)
		|| !GetSafeFixtureDirectory(CaseName, Directory)
		|| !FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestDisableAutoBattle")))
	{
		UE_LOG(LogTemp, Error, TEXT("R05HighSpeedFixture refused: known case, -ImmortalTestDisableAutoBattle and isolated -UserDir under Saved/Automation/HighSpeedCombat/<case> are required"));
		return;
	}
	bHighSpeedFixtureClaimed = true;
	if (!GetWorld() || GetWorld()->WorldType != EWorldType::Game)
	{
		FinishHighSpeedFixture(false, TEXT("requires a standalone game world"));
		return;
	}

	const TSharedRef<FHighSpeedFixtureState> State = MakeShared<FHighSpeedFixtureState>();
	State->CaseName = CaseName;
	State->Directory = Directory;
	const auto At = [this](const float Delay, const TFunction<void()>& Callback)
	{
		FTimerHandle Timer;
		GetWorldTimerManager().SetTimer(Timer,
			FTimerDelegate::CreateWeakLambda(this, [Callback] { Callback(); }), Delay, false);
	};
	const auto Shot = [State](const TCHAR* Name)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(State->Directory,
			FString::Printf(TEXT("R05_%s_%s.png"), *State->CaseName, Name)), true, false);
	};

	At(0.65f, [this, State, Shot]
	{
		if (bDead || bDeathCultivationRecoveryRequired || bAdventureSuspendedForDeathRecovery
			|| !bUsingDesktopPixelPlayer || !GetSprite() || !MortalRealmAttackFlipbook
			|| !MortalRealmHurtFlipbook || !MortalRealmDeathFlipbook)
		{
			FinishHighSpeedFixture(false, TEXT("fresh, unlocked pixel player with all action clips required"));
			return;
		}
		StopAutoAttack();
		bPixelHurtQueued = false;
		FinishMortalRealmOneShotAnimation();
		for (TActorIterator<AImmortalMonsterSpawner> It(GetWorld()); It; ++It)
		{
			It->StopSpawning();
		}
		for (TActorIterator<AImmortalPetCharacter> It(GetWorld()); It; ++It)
		{
			GetWorldTimerManager().ClearAllTimersForObject(*It);
			It->SetActorTickEnabled(false);
			It->SetActorEnableCollision(false);
		}
		for (TActorIterator<AImmortalMonsterCharacter> It(GetWorld()); It; ++It)
		{
			GetWorldTimerManager().ClearAllTimersForObject(*It);
			It->SetActorHiddenInGame(true);
			It->SetActorEnableCollision(false);
			It->SetActorTickEnabled(false);
		}
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
		CurrentHealth = GetMaxHealth();
		ArtifactShield = TechniqueShield = CultivationPathShield = 0.0f;
		InvulnerableUntilTime = 0.0f;
		AttackInterval = 1.0f;
		AttackSpeedMultiplier = State->CaseName == TEXT("cadence-4x") ? 4.0f :
			(State->CaseName == TEXT("cadence-1x") ? 1.0f : 2.0f);
		EquippedAttackSpeedBonus = ArtifactAttackSpeedBonus =
			TechniqueAttackSpeedBonus = CharacterPathAttackSpeedBonus = 0.0f;
		AttackWindup = State->CaseName == TEXT("cadence-4x") ? 0.15f : 0.25f;
		AttackDamage = State->CaseName == TEXT("target-death") ? 1000.0f : 100.0f;
		CriticalChance = EquippedCriticalChanceBonus = ArtifactCriticalChanceBonus =
			TechniqueCriticalChanceBonus = CharacterPathCriticalChanceBonus = 0.0f;

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AImmortalMonsterCharacter* Dummy = GetWorld()->SpawnActor<AImmortalMonsterCharacter>(
			GetActorLocation() + FVector(100.0f, 0.0f, 0.0f), FRotator::ZeroRotator, Params);
		if (!Dummy)
		{
			FinishHighSpeedFixture(false, TEXT("unable to spawn real monster target"));
			return;
		}
		Dummy->ConfigureForStage(State->CaseName == TEXT("target-death") ? 1 : 999);
		Dummy->SetActorTickEnabled(false);
		GetWorldTimerManager().ClearAllTimersForObject(Dummy);
		State->Target = Dummy;
		State->InitialTargetHealth = Dummy->GetCurrentHealth();
		State->LastTargetHealth = State->InitialTargetHealth;
		if (State->CaseName != TEXT("target-death")
			&& State->InitialTargetHealth < GetTotalAttackDamage() * 18.0f)
		{
			FinishHighSpeedFixture(false, TEXT("test target would die before sustained cadence completes"));
			return;
		}
		UE_LOG(LogTemp, Display,
			TEXT("R05HighSpeedFixture start: case=%s interval=%.3f windup=%.3f clip=%.3f targetHP=%.1f damage=%.1f"),
			*State->CaseName, GetEffectiveAttackInterval(), AttackWindup,
			MortalRealmAttackFlipbook->GetTotalDuration(), State->InitialTargetHealth,
			GetTotalAttackDamage());
		Shot(TEXT("ready"));

		GetWorldTimerManager().SetTimer(State->SampleTimer,
			FTimerDelegate::CreateWeakLambda(this, [this, State]
			{
				const float Now = GetWorld()->GetTimeSeconds();
				if (AImmortalMonsterCharacter* Target = State->Target.Get())
				{
					const float Health = Target->GetCurrentHealth();
					if (Health < State->LastTargetHealth - 0.01f)
					{
						++State->DamageEvents;
						State->TotalTargetDamage += State->LastTargetHealth - Health;
						State->HitTimes.Add(Now);
					}
					State->LastTargetHealth = Health;
				}
				UPaperFlipbookComponent* Sprite = GetSprite();
				if (!Sprite) return;
				if (State->CaseName == TEXT("hurt") && !State->bInitialHurtInjected
					&& bAttackPending
					&& GetWorldTimerManager().IsTimerActive(MortalRealmAttackAnimationTimerHandle))
				{
					State->bInitialHurtInjected = true;
					State->FirstIncomingHitTime = Now;
					const float Before = CurrentHealth;
					FDamageEvent Event;
					TakeDamage(5.0f, Event, nullptr, this);
					if (!(CurrentHealth < Before && bPixelHurtQueued
						&& Sprite->GetFlipbook() == MortalRealmAttackFlipbook))
					{
						FinishHighSpeedFixture(false,
							TEXT("nonlethal damage did not queue its visual during a real swing"));
						return;
					}
				}
				if (State->CaseName == TEXT("player-death")
					&& !State->bPlayerDeathInjected && bAttackPending
					&& GetWorldTimerManager().IsTimerActive(MortalRealmAttackAnimationTimerHandle))
				{
					State->bPlayerDeathInjected = true;
					State->TargetHealthAtPlayerDeath = State->Target.IsValid()
						? State->Target->GetCurrentHealth() : -1.0f;
					CurrentHealth = 1.0f;
					ArtifactShield = TechniqueShield = CultivationPathShield = 0.0f;
					FDamageEvent Event;
					TakeDamage(1000000.0f, Event, nullptr, this);
					if (!bDead || bAttackPending || bPixelHurtQueued
						|| Sprite->GetFlipbook() != MortalRealmDeathFlipbook
						|| GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle))
					{
						FinishHighSpeedFixture(false,
							TEXT("death did not preempt the pending swing"));
						return;
					}
				}
				if (Sprite->GetFlipbook() == MortalRealmAttackFlipbook
					&& Sprite->GetPlaybackPositionInFrames() >= 7)
				{
					++State->LateAttackFrameSamples;
				}
				const bool bHurt = Sprite->GetFlipbook() == MortalRealmHurtFlipbook;
				if (bHurt)
				{
					if (!State->bWasHurt)
					{
						++State->HurtStarts;
						State->HurtRunStart = Now;
						if (State->FirstHurtTime < 0.0f) State->FirstHurtTime = Now;
						if (State->CaseName == TEXT("hurt"))
						{
							FScreenshotRequest::RequestScreenshot(FPaths::Combine(
								State->Directory, TEXT("R05_hurt_actual.png")), true, false);
						}
					}
					const float Position = Sprite->GetPlaybackPosition();
					if (State->bWasHurt && Position + 0.05f < State->LastHurtPosition)
					{
						++State->HurtPositionRegressions;
					}
					State->LastHurtPosition = Position;
					State->LongestHurtRun = FMath::Max(State->LongestHurtRun,
						Now - State->HurtRunStart);
					if (State->CaseName == TEXT("hurt") && !State->bFollowupHurtInjected
						&& Now - State->HurtRunStart >= 0.10f)
					{
						State->bFollowupHurtInjected = true;
						FDamageEvent Event;
						TakeDamage(5.0f, Event, nullptr, this);
					}
				}
				else State->LastHurtPosition = -1.0f;
				State->bWasHurt = bHurt;
			}), 0.015f, true);
	});

	At(0.85f, [this, State]
	{
		if (!State->Target.IsValid()) return;
		State->AttackStartTime = GetWorld()->GetTimeSeconds();
		StartAutoAttack();
	});

	if (CaseName == TEXT("hurt"))
	{
		At(1.55f, [State, Shot]
		{
			if (State->FirstHurtTime >= 0.0f) Shot(TEXT("hurt"));
		});
	}
	else if (CaseName == TEXT("player-death"))
	{
		At(1.45f, [Shot] { Shot(TEXT("death")); });
	}
	else if (CaseName == TEXT("target-death"))
	{
		At(1.50f, [this, State, Shot]
		{
			if (!State->Target.IsValid() || !State->Target->IsDead() || bAttackPending)
			{
				FinishHighSpeedFixture(false, TEXT("target did not die on resolved high-speed hit"));
				return;
			}
			Shot(TEXT("target_death"));
		});
	}
	else
	{
		At(1.60f, [Shot] { Shot(TEXT("cadence")); });
	}

	At(CaseName == TEXT("player-death") ? 4.55f
		: (CaseName == TEXT("target-death") ? 1.70f : 4.20f),
		[this, State]
		{
			GetWorldTimerManager().ClearTimer(State->SampleTimer);
			const AImmortalMonsterCharacter* Target = State->Target.Get();
			if (!Target)
			{
				FinishHighSpeedFixture(false, TEXT("real monster target disappeared before audit"));
				return;
			}
			const float ObservedDamage = State->InitialTargetHealth - Target->GetCurrentHealth();
			UE_LOG(LogTemp, Display,
				TEXT("R05HighSpeedFixture audit: case=%s interval=%.3f damageEvents=%d targetDamage=%.1f sampledDamage=%.1f lateFrameSamples=%d firstHurt=%.3f hurtStarts=%d longestHurt=%.3f hurtRegressions=%d dead=%s gate=%s"),
				*State->CaseName, GetEffectiveAttackInterval(), State->DamageEvents,
				ObservedDamage, State->TotalTargetDamage, State->LateAttackFrameSamples,
				State->FirstHurtTime, State->HurtStarts, State->LongestHurtRun,
				State->HurtPositionRegressions, bDead ? TEXT("true") : TEXT("false"),
				bDeathCultivationRecoveryRequired ? TEXT("true") : TEXT("false"));

			bool bPassed = FMath::IsNearlyEqual(ObservedDamage, State->TotalTargetDamage, 0.1f);
			FString Detail;
			if (State->CaseName == TEXT("cadence-1x")
				|| State->CaseName == TEXT("cadence-2x")
				|| State->CaseName == TEXT("cadence-4x"))
			{
				const int32 MinimumHits = State->CaseName == TEXT("cadence-1x") ? 3
					: (State->CaseName == TEXT("cadence-2x") ? 6 : 11);
				bPassed = bPassed && State->DamageEvents >= MinimumHits
					&& ObservedDamage >= static_cast<float>(MinimumHits)
					&& !Target->IsDead() && !bDead;
				// A dropped or duplicate strike changes the inter-hit period even
				// when a loose minimum-hit count still passes. Compare actual damage
				// events rather than counting animation starts.
				bPassed = bPassed && State->AttackStartTime >= 0.0f
					&& State->HitTimes.Num() == State->DamageEvents;
				for (int32 Index = 1; Index < State->HitTimes.Num(); ++Index)
				{
					const float Gap = State->HitTimes[Index] - State->HitTimes[Index - 1];
					bPassed = bPassed && FMath::Abs(Gap - GetEffectiveAttackInterval()) <= 0.11f;
				}
				if (State->CaseName != TEXT("cadence-1x"))
				{
					bPassed = bPassed && State->LateAttackFrameSamples > 0;
				}
				Detail = FString::Printf(TEXT("cadence hits=%d minimum=%d lateFrames=%d damage=%.1f"),
					State->DamageEvents, MinimumHits, State->LateAttackFrameSamples, ObservedDamage);
			}
			else if (State->CaseName == TEXT("hurt"))
			{
				bPassed = bPassed && State->DamageEvents >= 6 && !bDead
					&& State->bInitialHurtInjected
					&& State->FirstHurtTime >= 0.0f
					&& State->FirstIncomingHitTime >= 0.0f
					&& State->FirstHurtTime - State->FirstIncomingHitTime <= 1.0f
					&& State->HurtStarts == 1 && State->LongestHurtRun >= 0.40f
					&& State->HurtPositionRegressions == 0 && State->bFollowupHurtInjected;
				Detail = FString::Printf(TEXT("hurt first=%.3f starts=%d longest=%.3f followup=%s hits=%d"),
						State->FirstHurtTime, State->HurtStarts, State->LongestHurtRun,
						State->bFollowupHurtInjected ? TEXT("true") : TEXT("false"), State->DamageEvents);
			}
			else if (State->CaseName == TEXT("target-death"))
			{
				bPassed = bPassed && Target->IsDead() && !bAttackPending
					&& !CurrentAttackTarget.IsValid() && !bDead
					&& !bMortalRealmOneShotAnimation
					&& GetSprite()->GetFlipbook() == MortalRealmIdleFlipbook;
				Detail = TEXT("dead target released; player returned to idle");
			}
			else
			{
				bPassed = bPassed && State->bPlayerDeathInjected
					&& !bDead && bDeathCultivationRecoveryRequired
					&& bAdventureSuspendedForDeathRecovery
					&& !GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)
					&& FMath::IsNearlyEqual(Target->GetCurrentHealth(),
						State->TargetHealthAtPlayerDeath, 0.01f);
				Detail = TEXT("fatal strike preempted attack; revive kept cultivation gate");
			}
			FinishHighSpeedFixture(bPassed, Detail);
		});
#endif
}
