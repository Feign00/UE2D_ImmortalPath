#include "ImmortalQingyunBossAnimationFixture.h"
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "ImmortalMonsterCharacter.h"
#include "ImmortalPetCharacter.h"
#include "ImmortalPlayerCharacter.h"
#include "../Spawning/ImmortalMonsterSpawner.h"
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
#include "PaperSprite.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"

namespace ImmortalQingyunBossFixturePrivate
{
	constexpr const TCHAR* Flag = TEXT("ImmortalTestQingyunBossArt");
	constexpr const TCHAR* BossClassPath = TEXT("/Game/GAME/BP_DesktopQingyunBoss.BP_DesktopQingyunBoss_C");
	constexpr const TCHAR* ClipDirectory = TEXT("/Game/GAME/Asset/DesktopPixelV2/QingyunBoss");
	bool bClaimed = false;

	bool SafeDirectory(FString& Directory)
	{
		int32 Modes = 0;
		for (const TCHAR* Mode : {Flag, TEXT("ImmortalTestFoxArt"), TEXT("ImmortalTestFoxWalking"),
			TEXT("ImmortalTestDogArt"), TEXT("ImmortalTestDogWalking")})
			Modes += static_cast<int32>(FParse::Param(FCommandLine::Get(), Mode));
		int32 Stage = 0;
		if (Modes != 1 || !FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestDisableAutoBattle"))
			|| !FParse::Param(FCommandLine::Get(), TEXT("FixedSeed"))
			|| !FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestStage="), Stage) || Stage != 10
			|| !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), Directory) || Directory.IsEmpty()) return false;
		FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/Automation/QingyunBossArt")));
		Directory = FPaths::ConvertRelativePathToFull(Directory);
		FPaths::NormalizeDirectoryName(Directory);
		FPaths::NormalizeDirectoryName(Root);
		return FPaths::CollapseRelativeDirectories(Directory) && FPaths::CollapseRelativeDirectories(Root)
			&& FPaths::IsUnderDirectory(Directory, Root) && !Directory.Equals(Root, ESearchCase::IgnoreCase);
	}

	float ReadFloat(const UObject* Object, const FName Name)
	{
		const FFloatProperty* P = Object ? FindFProperty<FFloatProperty>(Object->GetClass(), Name) : nullptr;
		return P ? P->GetPropertyValue_InContainer(Object) : -1000.0f;
	}
	bool SetFloat(UObject* Object, const FName Name, const float Value)
	{
		FFloatProperty* P = Object ? FindFProperty<FFloatProperty>(Object->GetClass(), Name) : nullptr;
		if (!P) return false;
		P->SetPropertyValue_InContainer(Object, Value);
		return true;
	}
	bool SetBool(UObject* Object, const FName Name, const bool Value)
	{
		FBoolProperty* P = Object ? FindFProperty<FBoolProperty>(Object->GetClass(), Name) : nullptr;
		if (!P) return false;
		P->SetPropertyValue_InContainer(Object, Value);
		return true;
	}
	bool ClearObject(UObject* Object, const FName Name)
	{
		FObjectPropertyBase* P = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name) : nullptr;
		if (!P) return false;
		P->SetObjectPropertyValue_InContainer(Object, nullptr);
		return true;
	}
	UPaperFlipbook* ReadClip(const UObject* Object, const FName Name)
	{
		const FObjectPropertyBase* P = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name) : nullptr;
		return P ? Cast<UPaperFlipbook>(P->GetObjectPropertyValue_InContainer(Object)) : nullptr;
	}
	void StopMovement(ACharacter& Actor)
	{
		Actor.ConsumeMovementInputVector();
		Actor.GetCharacterMovement()->StopMovementImmediately();
	}
	void Quarantine(AActor* Actor)
	{
		if (!Actor) return;
		Actor->GetWorldTimerManager().ClearAllTimersForObject(Actor);
		Actor->SetActorHiddenInGame(true);
		Actor->SetActorEnableCollision(false);
		Actor->SetActorTickEnabled(false);
		if (ACharacter* Character = Cast<ACharacter>(Actor)) StopMovement(*Character);
	}
	void ControlledMovement(AImmortalMonsterCharacter& Actor)
	{
		// This explicitly isolates terrain physics. It never claims natural Walking coverage.
		Actor.GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		Actor.GetCharacterMovement()->MaxFlySpeed = 80.0f;
		Actor.GetCharacterMovement()->BrakingDecelerationFlying = 2048.0f;
		Actor.GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
		Actor.GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		StopMovement(Actor);
	}
	bool IsClip(const AImmortalMonsterCharacter* Boss, const UPaperFlipbook* Clip)
	{
		return Boss && Boss->GetSprite() && Boss->GetSprite()->GetFlipbook() == Clip;
	}
	bool LastPose(const AImmortalMonsterCharacter* Boss, UPaperFlipbook* Clip)
	{
		return IsClip(Boss, Clip) && Clip && Clip->GetNumKeyFrames() > 0
			&& !Boss->GetSprite()->IsLooping() && !Boss->GetSprite()->IsPlaying()
			&& Clip->GetSpriteAtTime(Boss->GetSprite()->GetPlaybackPosition(), true)
				== Clip->GetKeyFrameChecked(Clip->GetNumKeyFrames() - 1).Sprite;
	}

	struct FState
	{
		TWeakObjectPtr<AImmortalPlayerCharacter> Player;
		TWeakObjectPtr<AImmortalMonsterCharacter> Boss;
		TWeakObjectPtr<AImmortalMonsterCharacter> DeathBoss;
		TWeakObjectPtr<AImmortalMonsterCharacter> RoutedBoss;
		TWeakObjectPtr<AImmortalMonsterSpawner> Spawner;
		TArray<TStrongObjectPtr<UPaperFlipbook>> Clips;
		FString Directory;
		FTimerHandle SampleTimer;
		FVector MoveOrigin = FVector::ZeroVector;
		TSet<int32> IdleFrames;
		float IdleLast = -1.0f;
		int32 IdleAdvances = 0;
		int32 IdleWraps = 0;
		float HurtLast = -1.0f;
		int32 HurtAdvances = 0;
		int32 HurtRegressions = 0;
		float AttackStart = -1.0f;
		float ContactTime = -1.0f;
		int32 ContactFrame = -1;
		int32 DamageEvents = 0;
		float LastPlayerHealth = 0.0f;
		float DeathAttackStart = -1.0f;
		float HealthBeforeDeathAttack = 0.0f;
		float FatalInjectionElapsed = -1.0f;
		float HealthAtFatal = 0.0f;
		bool bFatalInjected = false;
		float PhaseAttack = 0.0f;
		float PhaseSpeed = 0.0f;
		float PhaseAttackSpeed = 0.0f;
		float PhaseCritical = 0.0f;
		float LegacyHurtSeconds = 0.0f;
		bool bIdle = false;
		bool bAttack = false;
		bool bHurt = false;
		bool bDeathAttack = false;
		bool bFinished = false;
		int32 Checks = 0;
		int32 Failures = 0;
		int32 Screenshots = 0;
		UPaperFlipbook* Clip(const int32 Index) const { return Clips.IsValidIndex(Index) ? Clips[Index].Get() : nullptr; }
		void Check(const bool bPass, const TCHAR* Label)
		{
			++Checks;
			if (!bPass) ++Failures;
			UE_LOG(LogTemp, Display, TEXT("R07QingyunBossFixture check %03d: %s | %s"), Checks, bPass ? TEXT("PASS") : TEXT("FAIL"), Label);
		}
		void Finish()
		{
			if (bFinished) return;
			bFinished = true;
			if (Player.IsValid()) Player->GetWorldTimerManager().ClearTimer(SampleTimer);
			UE_LOG(LogTemp, Display, TEXT("R07QingyunBossFixture finished: checks=%d failures=%d screenshotsRequested=%d RESULT: %s"),
				Checks, Failures, Screenshots, Failures == 0 ? TEXT("PASS") : TEXT("FAIL"));
			UE_LOG(LogTemp, Display, TEXT("R07QingyunBossFixture coverage: real independent BP, ConfigureAsMapBoss(QingyunMountain,10), real attack HP change and .35s contact, dense Hurt, fatal cancellation, and actual map Spawner phase delegates. Controlled Flying/source viewport screenshots only; no natural Walking, native transparency, Boss skill/transition animation or complete combat balance acceptance."));
			FPlatformMisc::RequestExitWithStatus(false, Failures == 0 ? 0 : 1);
		}
	};

	void CheckVisual(AImmortalMonsterCharacter& Boss, AImmortalPlayerCharacter& Player, const TSharedRef<FState>& State)
	{
		const UCapsuleComponent* Capsule = Boss.GetCapsuleComponent();
		const UCapsuleComponent* PlayerCapsule = Player.GetCapsuleComponent();
		const UPaperFlipbookComponent* Sprite = Boss.GetSprite();
		const UPaperFlipbookComponent* PlayerSprite = Player.GetSprite();
		if (!Capsule || !PlayerCapsule || !Sprite || !PlayerSprite)
		{
			State->Check(false, TEXT("Boss/player physical and visual components exist"));
			return;
		}
		const double GroundOffset = PlayerSprite->GetComponentLocation().Z
			- (PlayerCapsule->GetComponentLocation().Z - PlayerCapsule->GetScaledCapsuleHalfHeight());
		const double ExpectedFoot = Capsule->GetComponentLocation().Z - Capsule->GetScaledCapsuleHalfHeight() + GroundOffset;
		const FVector Foot = Sprite->GetComponentLocation();
		State->Check(FMath::Abs(Foot.Z - ExpectedFoot) <= 1.5 && FMath::Abs(Foot.Y - PlayerSprite->GetComponentLocation().Y) <= 1.5,
			TEXT("Actual Boss foot/depth matches its own capsule bottom plus player visual lane"));
		const double ExpectedScale = ReadFloat(&Boss, TEXT("VisualScale")) * ReadFloat(&Boss, TEXT("BossVisualScaleMultiplier"));
		const FVector Scale = Sprite->GetComponentScale();
		State->Check(ExpectedScale > 0 && FMath::IsNearlyEqual(FMath::Abs(Scale.X), ExpectedScale, 0.01)
			&& FMath::IsNearlyEqual(Scale.Z, ExpectedScale, 0.01), TEXT("Boss promotion multiplies authored visual scale once"));
		const UWidgetComponent* Bar = Boss.FindComponentByClass<UWidgetComponent>();
		FVector ExpectedBar = Boss.GetRootComponent()->GetComponentTransform().TransformPosition(
			FVector(0, ReadFloat(&Boss, TEXT("VisualDepthOffset")), ReadFloat(&Boss, TEXT("HealthBarHeight")) + 35.0f));
		ExpectedBar.Z += GroundOffset;
		ExpectedBar.Y = PlayerSprite->GetComponentLocation().Y;
		State->Check(Bar && Bar->GetComponentLocation().Equals(ExpectedBar, 1.5), TEXT("Actual Boss bar retains baseline and +35 promotion offset without accumulation"));
		const APlayerController* Controller = Cast<APlayerController>(Player.GetController());
		int32 Width = 0, Height = 0;
		if (Controller) Controller->GetViewportSize(Width, Height);
		bool bInside = Controller && Width > 0 && Height > 0 && Sprite->IsVisible() && !Boss.IsHidden();
		const FBoxSphereBounds Bounds = Sprite->CalcBounds(Sprite->GetComponentTransform());
		for (const float X : {-1.0f, 1.0f}) for (const float Z : {-1.0f, 1.0f})
		{
			FVector2D Screen;
			bInside &= Controller && UGameplayStatics::ProjectWorldToScreen(Controller,
				Bounds.Origin + FVector(X * Bounds.BoxExtent.X, 0, Z * Bounds.BoxExtent.Z), Screen, false)
				&& Screen.X >= 0 && Screen.Y >= 0 && Screen.X <= Width && Screen.Y <= Height;
		}
		State->Check(bInside, TEXT("Source viewport contains complete promoted Boss bounds"));
		UE_LOG(LogTemp, Display, TEXT("R07QingyunBossVisual actor=%s location=%s foot=%s expectedFootZ=%.3f footError=%.3f depthError=%.3f scale=%s movementMode=%d viewport=%dx%d inside=%s phase=%d"),
			*Boss.GetName(), *Boss.GetActorLocation().ToString(), *Foot.ToString(), ExpectedFoot,
			FMath::Abs(Foot.Z - ExpectedFoot), FMath::Abs(Foot.Y - PlayerSprite->GetComponentLocation().Y), *Scale.ToString(),
			static_cast<int32>(Boss.GetCharacterMovement()->MovementMode), Width, Height, bInside ? TEXT("true") : TEXT("false"), Boss.GetBossPhase());
	}

	AImmortalMonsterCharacter* SpawnControlledBoss(AImmortalPlayerCharacter& Player, const float OffsetX, const TSharedRef<FState>& State)
	{
		UClass* Class = LoadClass<AImmortalMonsterCharacter>(nullptr, BossClassPath);
		State->Check(Class != nullptr, TEXT("Independent authored Qingyun Boss class loads"));
		if (!Class) return nullptr;
		const FTransform Transform(FRotator::ZeroRotator, Player.GetActorLocation() + FVector(OffsetX, 0, 0));
		AImmortalMonsterCharacter* Boss = Player.GetWorld()->SpawnActorDeferred<AImmortalMonsterCharacter>(
			Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		State->Check(Boss != nullptr, TEXT("Independent real Boss actor spawns"));
		if (!Boss) return nullptr;
		State->Check(FMath::IsNearlyEqual(ReadFloat(Boss, TEXT("AttackWindup")), 0.35f), TEXT("Production Boss retains original .35s attack windup"));
		State->Check(FMath::IsNearlyEqual(ReadFloat(Boss, TEXT("HurtAnimationDuration")), 0.35f), TEXT("Production Hurt fallback remains .35s; actual clip duration is checked separately"));
		State->Check(FMath::IsNearlyEqual(ReadFloat(Boss, TEXT("MaxHealth")), 45.0f)
			&& FMath::IsNearlyEqual(ReadFloat(Boss, TEXT("MovementSpeed")), 100.0f)
			&& FMath::IsNearlyEqual(ReadFloat(Boss, TEXT("AttackDamage")), 6.0f)
			&& FMath::IsNearlyEqual(ReadFloat(Boss, TEXT("AttackRange")), 320.0f)
			&& FMath::IsNearlyEqual(ReadFloat(Boss, TEXT("AttackInterval")), 1.7f, 0.001f), TEXT("Authored BP retains pre-promotion legacy gameplay baseline"));
		State->Check(SetBool(Boss, TEXT("bAutoCombatOnBeginPlay"), false) && SetBool(Boss, TEXT("bDestroyAfterDeath"), false)
			&& SetFloat(Boss, TEXT("MaxHealth"), 10000.0f) && SetFloat(Boss, TEXT("AttackInterval"), 30.0f)
			&& SetFloat(Boss, TEXT("CriticalChance"), 0.0f), TEXT("Only controlled instance health/cooldown/random critical controls are set"));
		UGameplayStatics::FinishSpawningActor(Boss, Transform);
		Boss->ConfigureAsMapBoss(UImmortalMapLibrary::GetQingyunMountainId(), 10);
		State->Check(Boss->IsBoss() && Boss->GetBossPhase() == 1 && !Boss->IsWorldBoss() && !Boss->IsEndlessEnemy()
			&& Boss->GetConfiguredMapId() == UImmortalMapLibrary::GetQingyunMountainId(), TEXT("Real ConfigureAsMapBoss creates phase-one Qingyun map Boss, not an ordinary dog"));
		State->Check(Boss->GetMonsterDisplayName().ToString() == TEXT("青云妖王") && Boss->GetCultivationReward() == 0 && Boss->GetGoldReward() == 0,
			TEXT("Real map Boss identity and no-kill-cultivation policy retained"));
		for (int32 I = 0; I < 5; ++I)
		{
			const TCHAR* Actions[] = {TEXT("Idle"), TEXT("Move"), TEXT("Attack"), TEXT("Hurt"), TEXT("Death")};
			State->Check(ReadClip(Boss, FName(*FString::Printf(TEXT("%sFlipbook"), Actions[I]))) == State->Clip(I), TEXT("Actual promoted Boss binds its authored action"));
		}
		State->Check(ClearObject(Boss, TEXT("EquipmentDropClass")) && ClearObject(Boss, TEXT("SpiritStoneDropClass"))
			&& ClearObject(Boss, TEXT("MaterialDropClass")), TEXT("Controlled death cannot create loot entities; production definitions untouched"));
		UE_LOG(LogTemp, Display, TEXT("R07QingyunBossFixture isolated locomotion range=100; production range=320 and .35s contact unchanged. Actor requested=%s actual=%s; no Z repair."),
			*Transform.GetLocation().ToString(), *Boss->GetActorLocation().ToString());
		SetFloat(Boss, TEXT("AttackRange"), 100.0f);
		const FTransform Initialized = Boss->GetActorTransform();
		ControlledMovement(*Boss);
		State->Check(Boss->GetActorTransform().Equals(Initialized, 0.1f), TEXT("Controlled physics does not move the normally initialized actor or repair Z"));
		return Boss;
	}

	int32 CheckAndQuarantineMinions(AImmortalPlayerCharacter& Player, AImmortalMonsterCharacter& Boss, const TSharedRef<FState>& State)
	{
		int32 Count = 0;
		for (TActorIterator<AImmortalMonsterCharacter> It(Player.GetWorld()); It; ++It)
		{
			AImmortalMonsterCharacter* Minion = *It;
			if (Minion->GetOwner() != &Boss || !Minion->ActorHasTag(TEXT("BossMinion"))) continue;
			++Count;
			const FString Path = Minion->GetClass()->GetPathName();
			State->Check((Path == TEXT("/Game/GAME/BP_Monster.BP_Monster_C") || Path == TEXT("/Game/GAME/BP_DesktopDogMonster.BP_DesktopDogMonster_C"))
				&& !Minion->IsBoss() && !Minion->IsWorldBoss() && !Minion->IsEndlessEnemy()
				&& Minion->GetConfiguredMapId() == UImmortalMapLibrary::GetQingyunMountainId(), TEXT("Real phase delegate summons ordinary authored map minion, not a copied Boss"));
			UE_LOG(LogTemp, Display, TEXT("R07QingyunBossMinion phase=%d actor=%s class=%s owner=%s"), Boss.GetBossPhase(), *Minion->GetName(), *Path, *GetNameSafe(Minion->GetOwner()));
			Quarantine(Minion);
		}
		return Count;
	}
}
#endif

bool ImmortalQingyunBossAnimationFixture::CheckPreflightIfRequested()
{
#if !UE_BUILD_SHIPPING
	using namespace ImmortalQingyunBossFixturePrivate;
	if (!FParse::Param(FCommandLine::Get(), Flag)) return true;
	FString Directory;
	if (!SafeDirectory(Directory))
	{
		UE_LOG(LogTemp, Error, TEXT("R07QingyunBossFixture refused before player initialization: exactly one actor-fixture mode, DisableAutoBattle, FixedSeed, stage 10, and a unique child -UserDir under Saved/Automation/QingyunBossArt are required"));
		FPlatformMisc::RequestExitWithStatus(false, 2);
		return false;
	}
#endif
	return true;
}

void ImmortalQingyunBossAnimationFixture::StartIfRequested(AImmortalPlayerCharacter& Player)
{
#if !UE_BUILD_SHIPPING
	using namespace ImmortalQingyunBossFixturePrivate;
	if (!FParse::Param(FCommandLine::Get(), Flag) || bClaimed) return;
	FString Directory;
	if (!SafeDirectory(Directory)) return;
	bClaimed = true;
	const TSharedRef<FState> State = MakeShared<FState>();
	State->Player = &Player;
	State->Directory = Directory;
	State->Check(Player.GetWorld() && Player.GetWorld()->WorldType == EWorldType::Game, TEXT("Standalone Game world required"));
	if (State->Failures) { State->Finish(); return; }
	for (const TCHAR* Action : {TEXT("Idle"), TEXT("Move"), TEXT("Attack"), TEXT("Hurt"), TEXT("Death")})
	{
		const FString Name = FString::Printf(TEXT("FB_QingyunBoss_%s"), Action);
		UPaperFlipbook* Clip = LoadObject<UPaperFlipbook>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), ClipDirectory, *Name, *Name));
		State->Clips.Emplace(Clip);
		State->Check(Clip && Clip->GetNumKeyFrames() == 8, TEXT("Authored Boss action has eight real keyframes"));
		if (!Clip) continue;
		TSet<const UPaperSprite*> Sprites;
		for (int32 I = 0; I < Clip->GetNumKeyFrames(); ++I)
		{
			const FPaperFlipbookKeyFrame& Key = Clip->GetKeyFrameChecked(I);
			const UPaperSprite* Sprite = Key.Sprite;
			Sprites.Add(Sprite);
			State->Check(Sprite && Key.FrameRun == 1 && FMath::IsNearlyEqual(Sprite->GetPixelsPerUnrealUnit(), 2.56f),
				TEXT("Boss frame retains authored density and has no repeated playback run"));
#if WITH_EDITOR
			// Source rectangle/pivot metadata is editor-only. The actual Game target
			// still checks the runtime rendered geometry below, not a fabricated metadata pass.
			State->Check(Sprite && Sprite->GetSourceSize().Equals(FVector2D(512, 384), 0.01)
				&& (Sprite->GetPivotPosition() - Sprite->GetSourceUV()).Equals(FVector2D(256, 336), 0.01),
				TEXT("Editor source metadata contains full uniform Boss cell and exact authored foot pivot"));
#endif
		}
		State->Check(Sprites.Num() == 8 && !Sprites.Contains(nullptr), TEXT("Eight separate sprite regions bound; pixel uniqueness remains source-art QA"));
	}
	UPaperFlipbook* LegacyHurt = LoadObject<UPaperFlipbook>(nullptr, TEXT("/Game/GAME/Asset/Player/doghurt/doghurt.doghurt"));
	State->LegacyHurtSeconds = LegacyHurt ? LegacyHurt->GetTotalDuration() : 0.0f;
	State->Check(LegacyHurt && State->Clip(3) && State->LegacyHurtSeconds > 0
		&& FMath::IsNearlyEqual(State->Clip(3)->GetTotalDuration(), State->LegacyHurtSeconds, 0.001f), TEXT("New eight-frame Hurt retains the actual protected old clip duration, not the .35 fallback"));
	State->Check(State->Clip(0) && FMath::IsNearlyEqual(State->Clip(0)->GetFramesPerSecond(), 8.0f)
		&& State->Clip(1) && FMath::IsNearlyEqual(State->Clip(1)->GetFramesPerSecond(), 8.0f)
		&& State->Clip(2) && FMath::IsNearlyEqual(State->Clip(2)->GetFramesPerSecond(), 12.0f)
		&& State->Clip(4) && FMath::IsNearlyEqual(State->Clip(4)->GetFramesPerSecond(), 10.0f), TEXT("Boss playback rates match authored action family"));
	if (State->Failures) { State->Finish(); return; }
	Player.StopAutoAttack();
	// The owned pet is already created by player initialization. Quarantine before its first attack, not after observing damage.
	for (TActorIterator<AImmortalPetCharacter> It(Player.GetWorld()); It; ++It) Quarantine(*It);
	for (TActorIterator<AImmortalMonsterCharacter> It(Player.GetWorld()); It; ++It) SetBool(*It, TEXT("bAutoCombatOnBeginPlay"), false);
	const auto At = [&Player, State](const float Delay, const TFunction<void()>& Callback)
	{
		FTimerHandle Handle;
		Player.GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(&Player,
			[State, Callback] { if (!State->bFinished) Callback(); }), Delay, false);
	};
	const auto Shot = [&Player, State](const TCHAR* Name)
	{
		for (AImmortalMonsterCharacter* Boss : {State->Boss.Get(), State->DeathBoss.Get(), State->RoutedBoss.Get()})
			if (Boss && !Boss->IsHidden()) CheckVisual(*Boss, Player, State);
		++State->Screenshots;
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(State->Directory, FString::Printf(TEXT("R07_QingyunBoss_%s.png"), Name)), true, false);
	};
	At(0.65f, [&Player, State, Shot]
	{
		State->Check(!Player.IsDead() && !Player.IsDeathCultivationRecoveryRequired(), TEXT("Fresh isolated player is alive and not death-gated"));
		Player.StopAutoAttack();
		StopMovement(Player);
		Player.GetCharacterMovement()->DisableMovement();
		Player.SetActorTickEnabled(false);
		for (TActorIterator<AImmortalMonsterSpawner> It(Player.GetWorld()); It; ++It)
		{
			It->StopSpawning();
			if (It->GetActiveMapId() == UImmortalMapLibrary::GetQingyunMountainId()) State->Spawner = *It;
		}
		State->Check(State->Spawner.IsValid() && State->Spawner->GetCurrentStage() == 10 && State->Spawner->IsCurrentStageBossStage(), TEXT("Actual map BP Spawner is at Qingyun stage-ten Boss gate"));
		int32 RoutedBossCount = 0;
		for (TActorIterator<AImmortalMonsterCharacter> It(Player.GetWorld()); It; ++It)
		{
			if (It->IsBoss() && !It->IsWorldBoss() && !It->IsEndlessEnemy() && It->GetOwner() == State->Spawner.Get())
			{
				++RoutedBossCount;
				State->RoutedBoss = *It;
			}
			Quarantine(*It);
		}
		for (TActorIterator<AImmortalPetCharacter> It(Player.GetWorld()); It; ++It) Quarantine(*It);
		State->Check(RoutedBossCount == 1 && State->RoutedBoss.IsValid() && State->RoutedBoss->GetClass()->GetPathName() == BossClassPath,
			TEXT("Actual map Spawner naturally routed exactly one new dedicated Boss"));
		if (State->RoutedBoss.IsValid())
		{
			State->Check(State->RoutedBoss->GetBossPhase() == 1 && FMath::IsNearlyEqual(State->RoutedBoss->GetHealthPercent(), 1.0f, 0.001f), TEXT("Real routed Boss enters untouched phase one"));
			State->PhaseAttack = ReadFloat(State->RoutedBoss.Get(), TEXT("AttackDamage"));
			State->PhaseSpeed = ReadFloat(State->RoutedBoss.Get(), TEXT("MovementSpeed"));
			State->PhaseAttackSpeed = ReadFloat(State->RoutedBoss.Get(), TEXT("AttackSpeedMultiplier"));
			State->PhaseCritical = ReadFloat(State->RoutedBoss.Get(), TEXT("CriticalChance"));
		}
		if (State->Failures) { State->Finish(); return; }
		State->Boss = SpawnControlledBoss(Player, 250.0f, State);
		if (!State->Boss.IsValid() || State->Failures) { State->Finish(); return; }
		State->bIdle = true;
		State->LastPlayerHealth = Player.GetCurrentHealth();
		Player.GetWorldTimerManager().SetTimer(State->SampleTimer, FTimerDelegate::CreateWeakLambda(&Player, [&Player, State, Shot]
		{
			const float Now = Player.GetWorld()->GetTimeSeconds();
			if (AImmortalMonsterCharacter* Boss = State->Boss.Get())
			{
				UPaperFlipbookComponent* Sprite = Boss->GetSprite();
				if (State->bIdle && IsClip(Boss, State->Clip(0)))
				{
					State->IdleFrames.Add(Sprite->GetPlaybackPositionInFrames());
					const float Position = Sprite->GetPlaybackPosition();
					if (State->IdleLast >= 0 && Position > State->IdleLast + 0.001f) ++State->IdleAdvances;
					if (State->IdleLast >= 0 && Position + 0.25f < State->IdleLast) ++State->IdleWraps;
					State->IdleLast = Position;
				}
				if (State->bAttack)
				{
					if (State->AttackStart < 0 && IsClip(Boss, State->Clip(2)))
					{
						State->AttackStart = Now;
						SetBool(Boss, TEXT("bAutoCombatOnBeginPlay"), false);
					}
					if (Player.GetCurrentHealth() < State->LastPlayerHealth - 0.01f)
					{
						++State->DamageEvents;
						State->ContactTime = Now;
						State->ContactFrame = Sprite->GetPlaybackPositionInFrames();
						if (State->DamageEvents == 1) Shot(TEXT("AttackContact"));
					}
					State->LastPlayerHealth = Player.GetCurrentHealth();
				}
				if (State->bHurt && IsClip(Boss, State->Clip(3)))
				{
					const float Position = Sprite->GetPlaybackPosition();
					if (State->HurtLast >= 0 && Position > State->HurtLast + 0.001f) ++State->HurtAdvances;
					if (State->HurtLast >= 0 && Position + 0.03f < State->HurtLast) ++State->HurtRegressions;
					State->HurtLast = Position;
				}
			}
			if (State->bDeathAttack && State->DeathBoss.IsValid() && IsClip(State->DeathBoss.Get(), State->Clip(2)))
			{
				if (State->DeathAttackStart < 0)
				{
					// Infer the actual animation onset instead of treating the first
					// delayed polling sample as a new attack start.
					const UPaperFlipbookComponent* Sprite = State->DeathBoss->GetSprite();
					const float Playback = Sprite->GetPlaybackPosition();
					State->DeathAttackStart = Now - Playback;
					State->Check(Sprite->IsPlaying() && !Sprite->IsLooping() && Playback >= 0.0f
						&& Playback < ReadFloat(State->DeathBoss.Get(), TEXT("AttackWindup")),
						TEXT("Death cancellation observes the real attack onset before its damage contact"));
				}
				if (!State->bFatalInjected && Now - State->DeathAttackStart >= 0.10f)
				{
					State->bFatalInjected = true;
					State->FatalInjectionElapsed = Now - State->DeathAttackStart;
					State->HealthAtFatal = Player.GetCurrentHealth();
					State->Check(State->DeathAttackStart >= 0.0f
						&& State->FatalInjectionElapsed >= 0.10f && State->FatalInjectionElapsed < 0.35f
						&& State->FatalInjectionElapsed < ReadFloat(State->DeathBoss.Get(), TEXT("AttackWindup")),
						TEXT("Fatal Boss damage is injected inside the original .35s windup before contact"));
					State->Check(FMath::IsNearlyEqual(State->HealthAtFatal, State->HealthBeforeDeathAttack, 0.01f),
						TEXT("Death-scene player HP stays unchanged from attack enablement until the pre-contact fatal injection"));
					UE_LOG(LogTemp, Display,
						TEXT("R07QingyunBossFatalCancellation start=%.3f fatal=%.3f elapsed=%.3f windup=%.3f playerHPBefore=%.3f playerHPAtFatal=%.3f"),
						State->DeathAttackStart, Now, State->FatalInjectionElapsed,
						ReadFloat(State->DeathBoss.Get(), TEXT("AttackWindup")),
						State->HealthBeforeDeathAttack, State->HealthAtFatal);
					FDamageEvent Event;
					State->DeathBoss->TakeDamage(1000000.0f, Event, nullptr, nullptr);
				}
			}
		}), 0.015f, true);
	});
	At(2.20f, [State]
	{
		State->bIdle = false;
		UE_LOG(LogTemp, Display, TEXT("R07QingyunBossIdle frames=%d advances=%d wraps=%d last=%.3f"),
			State->IdleFrames.Num(), State->IdleAdvances, State->IdleWraps, State->IdleLast);
		State->Check(State->IdleFrames.Num() == 8 && State->IdleAdvances > 15 && State->IdleWraps >= 1, TEXT("Real Boss Idle visits all eight authored poses and wraps a complete loop"));
	});
	// First screenshot encoding can stall a cold render pipeline. Keep it outside
	// the complete-loop sampling window; never lower the eight-pose requirement.
	At(2.25f, [State, Shot]
	{
		State->Check(IsClip(State->Boss.Get(), State->Clip(0)) && State->Boss->GetSprite()->IsPlaying() && State->Boss->GetSprite()->IsLooping(), TEXT("Promoted Boss stationary Idle really plays a loop"));
		Shot(TEXT("Idle"));
	});
	At(2.50f, [&Player, State]
	{
		if (AImmortalMonsterCharacter* Boss = State->Boss.Get())
		{
			State->MoveOrigin = Boss->GetActorLocation();
			State->MoveOrigin.X = Player.GetActorLocation().X + 500.0f;
			Boss->SetActorLocation(State->MoveOrigin);
			StopMovement(*Boss);
			SetBool(Boss, TEXT("bAutoCombatOnBeginPlay"), true);
		}
	});
	At(2.95f, [State, Shot]
	{
		AImmortalMonsterCharacter* Boss = State->Boss.Get();
		State->Check(IsClip(Boss, State->Clip(1)) && Boss->GetVelocity().X < -1.0f && Boss->GetActorLocation().X < State->MoveOrigin.X - 5.0f, TEXT("Real Boss AI moves left with actual forward progress"));
		State->Check(Boss && Boss->GetSprite()->GetRelativeScale3D().X > 0, TEXT("Authored left-facing Boss stays unmirrored toward left target"));
		Shot(TEXT("MoveLeft"));
	});
	At(3.10f, [State] { if (State->Boss.IsValid()) { SetBool(State->Boss.Get(), TEXT("bAutoCombatOnBeginPlay"), false); StopMovement(*State->Boss); } });
	At(3.25f, [State] { State->Check(IsClip(State->Boss.Get(), State->Clip(0)), TEXT("Boss locomotion stop returns to authored Idle")); });
	At(3.40f, [&Player, State]
	{
		if (AImmortalMonsterCharacter* Boss = State->Boss.Get())
		{
			State->MoveOrigin = Boss->GetActorLocation();
			State->MoveOrigin.X = Player.GetActorLocation().X - 280.0f;
			Boss->SetActorLocation(State->MoveOrigin);
			StopMovement(*Boss);
			SetBool(Boss, TEXT("bAutoCombatOnBeginPlay"), true);
		}
	});
	At(3.85f, [State, Shot]
	{
		AImmortalMonsterCharacter* Boss = State->Boss.Get();
		State->Check(IsClip(Boss, State->Clip(1)) && Boss->GetVelocity().X > 1.0f && Boss->GetActorLocation().X > State->MoveOrigin.X + 5.0f, TEXT("Real Boss AI moves right with actual forward progress"));
		State->Check(Boss && Boss->GetSprite()->GetRelativeScale3D().X < 0, TEXT("Only horizontal facing is mirrored for rightward Boss"));
		Shot(TEXT("MoveRight"));
	});
	At(4.05f, [State] { if (State->Boss.IsValid()) { SetBool(State->Boss.Get(), TEXT("bAutoCombatOnBeginPlay"), false); StopMovement(*State->Boss); } });
	At(4.30f, [&Player, State]
	{
		Quarantine(State->Boss.Get());
		State->Boss = SpawnControlledBoss(Player, 90.0f, State);
		if (!State->Boss.IsValid() || State->Failures) { State->Finish(); return; }
		SetFloat(State->Boss.Get(), TEXT("AttackDamage"), FMath::Max(Player.GetMaxHealth() * 0.10f, 30.0f));
		State->LastPlayerHealth = Player.GetCurrentHealth();
		State->bAttack = true;
		SetBool(State->Boss.Get(), TEXT("bAutoCombatOnBeginPlay"), true);
	});
	At(5.35f, [State, Shot]
	{
		State->bAttack = false;
		const float Delta = State->ContactTime - State->AttackStart;
		UE_LOG(LogTemp, Display, TEXT("R07QingyunBossContact start=%.3f hit=%.3f delta=%.3f frame=%d damageEvents=%d"), State->AttackStart, State->ContactTime, Delta, State->ContactFrame, State->DamageEvents);
		State->Check(State->AttackStart >= 0 && State->DamageEvents == 1 && State->ContactTime >= 0, TEXT("Real Boss attack causes exactly one player HP decrement"));
		State->Check(FMath::Abs(Delta - 0.35f) <= 0.10f && State->ContactFrame >= 3 && State->ContactFrame <= 5, TEXT("Actual damage retains .35s authored contact near zero-based frame four"));
		State->Check(IsClip(State->Boss.Get(), State->Clip(0)), TEXT("Boss attack recovers to Idle"));
		Shot(TEXT("AttackRecovery"));
	});
	At(5.70f, [State]
	{
		State->bHurt = true;
		if (AImmortalMonsterCharacter* Boss = State->Boss.Get())
		{
			FDamageEvent Event;
			const float Before = Boss->GetCurrentHealth();
			Boss->TakeDamage(1.0f, Event, nullptr, nullptr);
			State->Check(Boss->GetCurrentHealth() < Before && IsClip(Boss, State->Clip(3)) && Boss->GetBossPhase() == 1, TEXT("Real Boss TakeDamage changes HP and enters authored Hurt without premature phase change"));
		}
	});
	for (const float Time : {5.82f, 5.94f, 6.06f}) At(Time, [State]
	{
		if (State->Boss.IsValid()) { FDamageEvent Event; State->Boss->TakeDamage(1.0f, Event, nullptr, nullptr); }
	});
	At(5.96f, [Shot] { Shot(TEXT("HurtMid")); });
	At(6.30f, [State, Shot]
	{
		State->Check(State->HurtAdvances > 10 && State->HurtRegressions == 0, TEXT("Dense real Boss hits do not reset or regress Hurt playback"));
		State->Check(LastPose(State->Boss.Get(), State->Clip(3)), TEXT("Dense Hurt reaches and holds the actual eighth pose"));
		Shot(TEXT("HurtHeld"));
	});
	At(6.65f, [State]
	{
		State->bHurt = false;
		State->Check(IsClip(State->Boss.Get(), State->Clip(0)), TEXT("Hurt recovers after the preserved actual legacy clip window"));
	});
	At(7.00f, [&Player, State]
	{
		Quarantine(State->Boss.Get());
		State->DeathBoss = SpawnControlledBoss(Player, 90.0f, State);
		if (!State->DeathBoss.IsValid() || State->Failures) { State->Finish(); return; }
		State->HealthBeforeDeathAttack = Player.GetCurrentHealth();
		State->Check(State->HealthBeforeDeathAttack > 0.0f && !Player.IsDead(), TEXT("Death cancellation starts with a live player and records HP before enabling the Boss attack"));
		State->bDeathAttack = true;
		SetBool(State->DeathBoss.Get(), TEXT("bAutoCombatOnBeginPlay"), true);
	});
	At(7.40f, [State, Shot]
	{
		State->Check(State->bFatalInjected && State->DeathBoss.IsValid() && State->DeathBoss->IsDead() && IsClip(State->DeathBoss.Get(), State->Clip(4)), TEXT("Fatal real damage during Boss windup immediately enters Death"));
		Shot(TEXT("DeathEarly"));
	});
	At(8.10f, [State, Shot]
	{
		State->Check(LastPose(State->DeathBoss.Get(), State->Clip(4)), TEXT("Real Boss Death holds eighth pose without looping or destruction"));
		State->Check(State->bFatalInjected && State->Player.IsValid() && FMath::IsNearlyEqual(State->Player->GetCurrentHealth(), State->HealthAtFatal, 0.01f), TEXT("Fatal Boss hit cancels pending attack damage and cannot harm player later"));
		Shot(TEXT("DeathHeld"));
	});
	At(8.65f, [State]
	{
		State->bDeathAttack = false;
		State->Check(LastPose(State->DeathBoss.Get(), State->Clip(4)), TEXT("Boss remains on Death terminal pose across a later sample"));
		Quarantine(State->DeathBoss.Get());
	});
	At(9.00f, [&Player, State, Shot]
	{
		AImmortalMonsterCharacter* Boss = State->RoutedBoss.Get();
		State->Check(Boss && Boss->IsBoss() && Boss->GetBossPhase() == 1, TEXT("Actual Spawner-owned Boss retained live phase-one state"));
		if (!Boss) { State->Finish(); return; }
		SetBool(Boss, TEXT("bAutoCombatOnBeginPlay"), false);
		Boss->SetActorHiddenInGame(false);
		Boss->SetActorEnableCollision(true);
		Boss->SetActorTickEnabled(true);
		FVector Location = Boss->GetActorLocation();
		Location.X = Player.GetActorLocation().X + 300.0f;
		Boss->SetActorLocation(Location); // controlled X-only framing; keep its actual initialized Z.
		ControlledMovement(*Boss);
		Shot(TEXT("PhaseOne"));
	});
	At(9.25f, [&Player, State]
	{
		if (AImmortalMonsterCharacter* Boss = State->RoutedBoss.Get())
		{
			FDamageEvent Event;
			const float Damage = Boss->GetCurrentHealth() - Boss->GetMaxHealth() * 0.60f + FMath::Max(ReadFloat(Boss, TEXT("Defense")), 0.0f);
			Boss->TakeDamage(Damage, Event, nullptr, nullptr);
			State->Check(Boss->GetBossPhase() == 2 && !Boss->IsDead(), TEXT("Real damage crosses 66% threshold into phase two"));
			State->Check(FMath::IsNearlyEqual(ReadFloat(Boss, TEXT("AttackDamage")), State->PhaseAttack * 1.18f, 0.01f)
				&& FMath::IsNearlyEqual(ReadFloat(Boss, TEXT("AttackSpeedMultiplier")), State->PhaseAttackSpeed * 1.12f, 0.001f)
				&& FMath::IsNearlyEqual(ReadFloat(Boss, TEXT("MovementSpeed")), State->PhaseSpeed * 1.05f, 0.01f), TEXT("Actual map Boss phase-two attack/attack-speed/movement multipliers remain unchanged"));
			State->Check(CheckAndQuarantineMinions(Player, *Boss, State) == 1, TEXT("Actual map Spawner phase-two delegate summons exactly one minion"));
		}
	});
	At(9.95f, [State, Shot]
	{
		State->Check(State->RoutedBoss.IsValid() && IsClip(State->RoutedBoss.Get(), State->Clip(0)), TEXT("Real phase-two Hurt finishes into same authored Idle family"));
		Shot(TEXT("PhaseTwo"));
	});
	At(10.25f, [&Player, State]
	{
		if (AImmortalMonsterCharacter* Boss = State->RoutedBoss.Get())
		{
			FDamageEvent Event;
			const float Damage = Boss->GetCurrentHealth() - Boss->GetMaxHealth() * 0.25f + FMath::Max(ReadFloat(Boss, TEXT("Defense")), 0.0f);
			Boss->TakeDamage(Damage, Event, nullptr, nullptr);
			State->Check(Boss->GetBossPhase() == 3 && !Boss->IsDead(), TEXT("Real damage crosses 33% threshold into phase three"));
			State->Check(FMath::IsNearlyEqual(ReadFloat(Boss, TEXT("AttackDamage")), State->PhaseAttack * 1.18f * 1.25f, 0.01f)
				&& FMath::IsNearlyEqual(ReadFloat(Boss, TEXT("AttackSpeedMultiplier")), State->PhaseAttackSpeed * 1.12f * 1.18f, 0.001f)
				&& FMath::IsNearlyEqual(ReadFloat(Boss, TEXT("MovementSpeed")), State->PhaseSpeed * 1.05f * 1.08f, 0.01f)
				&& FMath::IsNearlyEqual(ReadFloat(Boss, TEXT("CriticalChance")), FMath::Clamp(State->PhaseCritical + 0.08f, 0.0f, 1.0f), 0.001f), TEXT("Actual map Boss phase-three cumulative multipliers and critical increase remain unchanged"));
			State->Check(CheckAndQuarantineMinions(Player, *Boss, State) == 3, TEXT("Actual map Spawner phase-three delegate adds exactly two minions to the previous one"));
		}
	});
	At(10.95f, [State, Shot]
	{
		State->Check(State->RoutedBoss.IsValid() && IsClip(State->RoutedBoss.Get(), State->Clip(0)), TEXT("Real phase-three Hurt recovers to authored Idle"));
		Shot(TEXT("PhaseThree"));
	});
	At(11.75f, [State]
	{
		State->Check(State->Spawner.IsValid() && State->Spawner->GetCurrentStage() == 10 && State->Spawner->GetCurrentStageKills() == 0,
			TEXT("Phase testing does not kill the routed Boss or advance the stage gate"));
		State->Check(State->Screenshots == 12, TEXT("All twelve distinct source captures were requested"));
		State->Finish();
	});
#endif
}
