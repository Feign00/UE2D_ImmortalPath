#include "ImmortalPlayerCharacter.h"

#if !UE_BUILD_SHIPPING
#include "ImmortalMonsterCharacter.h"
#include "ImmortalPetCharacter.h"
#include "../Spawning/ImmortalMonsterSpawner.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInterface.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#endif

void AImmortalPlayerCharacter::RunPixelPlayerIntegrationFixture()
{
#if !UE_BUILD_SHIPPING
	if (!FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestPixelPlayerIntegration"))) return;
	FString UserDirectory;
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/Automation/PixelIntegration")));
	if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirectory)
		|| !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(UserDirectory), Root))
	{
		UE_LOG(LogTemp, Error, TEXT("Pixel integration refused: isolated PixelIntegration UserDir required."));
		return;
	}
	const auto Failures = MakeShared<int32>(0);
	const auto Check = [Failures](const TCHAR* Name, bool bPassed)
	{
		UE_LOG(LogTemp, Display, TEXT("Pixel integration check: %s passed=%s"), Name, bPassed ? TEXT("true") : TEXT("false"));
		if (!bPassed) { ++*Failures; UE_LOG(LogTemp, Error, TEXT("Pixel integration failed: %s"), Name); }
	};
	const auto At = [this](float Time, TFunction<void()> Callback)
	{
		FTimerHandle Timer;
		GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [Callback] { Callback(); }), Time, false);
	};
	const auto Shot = [](const TCHAR* Name)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),
			FString::Printf(TEXT("Screenshots/PixelIntegration_%s.png"), Name)), true, false);
	};
	const auto Target = MakeShared<TWeakObjectPtr<AImmortalMonsterCharacter>>();
	const auto TargetHealth = MakeShared<float>(0);
	const auto HurtRemaining = MakeShared<float>(0);
	At(0.5f, [this, Check, Target, TargetHealth]
	{
		StopAutoAttack();
		for (TActorIterator<AImmortalPetCharacter> It(GetWorld()); It; ++It)
		{
			GetWorldTimerManager().ClearAllTimersForObject(*It); It->SetActorTickEnabled(false);
		}
		for (TActorIterator<AImmortalMonsterSpawner> It(GetWorld()); It; ++It) It->StopSpawning();
		for (TActorIterator<AImmortalMonsterCharacter> It(GetWorld()); It; ++It)
		{
			GetWorldTimerManager().ClearAllTimersForObject(*It);
			It->SetActorHiddenInGame(true); It->SetActorEnableCollision(false); It->SetActorTickEnabled(false);
		}
		GetCharacterMovement()->StopMovementImmediately(); GetCharacterMovement()->DisableMovement();
		Check(TEXT("actual player selected complete pixel family"), bUsingDesktopPixelPlayer);
		Check(TEXT("actual player uses masked material"), GetSprite()->GetMaterial(0) && GetSprite()->GetMaterial(0)->GetBlendMode() == BLEND_Masked);
		GetCharacterMovement()->Velocity = FVector(20, 0, 0); UpdateMortalRealmLocomotionAnimation();
		Check(TEXT("movement selects new Move"), GetSprite()->GetFlipbook() == MortalRealmMoveFlipbook);
		GetCharacterMovement()->Velocity = FVector::ZeroVector; UpdateMortalRealmLocomotionAnimation();
		Check(TEXT("stopping selects new Idle"), GetSprite()->GetFlipbook() == MortalRealmIdleFlipbook);
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AImmortalMonsterCharacter* Dummy = GetWorld()->SpawnActor<AImmortalMonsterCharacter>(GetActorLocation() + FVector(100, 0, 0), FRotator::ZeroRotator, Params);
		*Target = Dummy;
		Check(TEXT("damage target spawned"), IsValid(Dummy));
		if (Dummy)
		{
			Dummy->ConfigureForStage(100); Dummy->SetActorTickEnabled(false);
			GetWorldTimerManager().ClearAllTimersForObject(Dummy);
			*TargetHealth = Dummy->GetCurrentHealth();
		}
		AttackWindup = 0.5f;
		CurrentHealth = GetMaxHealth(); ArtifactShield = TechniqueShield = CultivationPathShield = 0;
		InvulnerableUntilTime = 0;
	});
	At(0.6f, [this, Check] { TryAutoAttack(); Check(TEXT("real attack entered windup"), bAttackPending && GetSprite()->GetFlipbook() == MortalRealmAttackFlipbook); });
	At(0.7f, [this, Check]
	{
		const float Before = CurrentHealth; FDamageEvent Event; TakeDamage(1, Event, nullptr, this);
		Check(TEXT("nonlethal damage applied but swing preserved"), CurrentHealth < Before && bPixelHurtQueued && GetSprite()->GetFlipbook() == MortalRealmAttackFlipbook);
	});
	At(0.9f, [Check, Target, TargetHealth] { Check(TEXT("no premature damage"), Target->IsValid() && Target->Get()->GetCurrentHealth() == *TargetHealth); });
	At(1.15f, [this, Check, Target, TargetHealth, Shot]
	{
		Check(TEXT("damage landed at contact"), !bAttackPending && Target->IsValid() && Target->Get()->GetCurrentHealth() < *TargetHealth && GetSprite()->GetPlaybackPositionInFrames() >= 3);
		Shot(TEXT("Attack"));
	});
	At(2.05f, [this, Check, HurtRemaining, Shot]
	{
		Check(TEXT("queued hurt follows attack with normal rate"), GetSprite()->GetFlipbook() == MortalRealmHurtFlipbook && GetSprite()->GetPlayRate() == 1);
		*HurtRemaining = GetWorldTimerManager().GetTimerRemaining(MortalRealmHurtAnimationTimerHandle); Shot(TEXT("Hurt"));
	});
	At(2.2f, [this, Check, HurtRemaining]
	{
		const float Position = GetSprite()->GetPlaybackPosition(); FDamageEvent Event;
		TakeDamage(1, Event, nullptr, this); TakeDamage(1, Event, nullptr, this);
		Check(TEXT("repeated hurt preserves phase and original deadline"), GetSprite()->GetPlaybackPosition() >= Position
			&& GetWorldTimerManager().GetTimerRemaining(MortalRealmHurtAnimationTimerHandle) < *HurtRemaining);
	});
	At(2.8f, [this, Check] { Check(TEXT("hurt recovers to idle"), GetSprite()->GetFlipbook() == MortalRealmIdleFlipbook && !bMortalRealmOneShotAnimation); });
	At(3.0f, [this, Check, Target, TargetHealth]
	{
		if (Target->IsValid()) *TargetHealth = Target->Get()->GetCurrentHealth();
		AttackWindup = 0; TryAutoAttack();
		Check(TEXT("zero windup damages at visible contact"), !bAttackPending && Target->IsValid() && Target->Get()->GetCurrentHealth() < *TargetHealth && GetSprite()->GetPlaybackPositionInFrames() >= 3);
	});
	At(3.7f, [this, Target, TargetHealth]
	{
		if (Target->IsValid()) *TargetHealth = Target->Get()->GetCurrentHealth();
		AttackWindup = 0.5f; TryAutoAttack();
	});
	At(3.8f, [this, Check]
	{
		CurrentHealth = 1; ArtifactShield = TechniqueShield = CultivationPathShield = 0; FDamageEvent Event;
		TakeDamage(1000000, Event, nullptr, this);
		Check(TEXT("death cancels pending swing and queued reaction"), bDead && !bAttackPending && !bPixelHurtQueued && GetSprite()->GetFlipbook() == MortalRealmDeathFlipbook && GetSprite()->GetPlayRate() == 1);
	});
	At(4.65f, [this, Check, Shot]
	{
		Check(TEXT("death holds final frame"), bDead && !GetSprite()->IsPlaying() && GetSprite()->GetPlaybackPositionInFrames() == 7);
		Shot(TEXT("Death"));
	});
	At(5.0f, [this, Check, Target, TargetHealth]
	{
		Check(TEXT("death blocks late attack damage"), !Target->IsValid() || Target->Get()->GetCurrentHealth() == *TargetHealth);
		Check(TEXT("death forces cultivation and stops combat"), bDeathCultivationRecoveryRequired && bAdventureSuspendedForDeathRecovery
			&& bManagementInterfaceOpen && !GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle));
	});
	At(7.05f, [this, Check]
	{
		Check(TEXT("revive clears animation state without bypassing gate"), !bDead && !bPixelHurtQueued && !bMortalRealmOneShotAnimation
			&& GetSprite()->GetFlipbook() == MortalRealmIdleFlipbook && GetSprite()->GetPlayRate() == 1 && bDeathCultivationRecoveryRequired);
		CloseManagementInterface(); Check(TEXT("adventure remains locked after revival"), bManagementInterfaceOpen && !GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle));
	});
	At(7.5f, [Failures]
	{
		UE_LOG(LogTemp, Display, TEXT("Pixel integration finished: failures=%d"), *Failures);
		FPlatformMisc::RequestExitWithStatus(false, *Failures == 0 ? 0 : 1);
	});
#endif
}
