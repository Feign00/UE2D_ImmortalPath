// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Pets/ImmortalPetTypes.h"
#include "PaperCharacter.h"
#include "ImmortalPetCharacter.generated.h"

class AImmortalPlayerCharacter;
class UPaperFlipbook;

/**
 * Runtime-only combat companion owned by the local player.
 *
 * Pet growth lives in FImmortalPetState. This actor deliberately has no
 * Monster tag, target interface, health bar, reward logic or serialized
 * combat state.
 */
UCLASS(Blueprintable)
class IMMORTALPATH_API AImmortalPetCharacter : public APaperCharacter
{
	GENERATED_BODY()

public:
	AImmortalPetCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Binds this runtime actor to one owned pet in the player's persistent state. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Pet")
	bool InitializeForPlayer(
		AImmortalPlayerCharacter* InPlayer,
		FName InPetId);

	/** Reloads the catalog definition and presentation for the current PetId. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Pet")
	void RefreshFromPersistentState();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Pet")
	FName GetPetId() const { return PetId; }

	/** Number of attack animations that reached their windup state this session. */
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Pet|Combat")
	int32 GetAttackCount() const { return AttackCount; }

	/** Number of real damage frames accepted by the player's damage pipeline. */
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Pet|Combat")
	int32 GetHitCount() const { return HitCount; }

	/** Non-shipping animation fixture. It never changes persistent pet health. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Pet|Development")
	bool PreviewHurtForDevelopment();

	/** Non-shipping animation fixture. The actor resumes normally after playback. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Pet|Development")
	bool PreviewDeathForDevelopment();

private:
	void UpdateFollowAndCombat(float DeltaSeconds);
	void ReturnToFollowPoint(bool bTeleport);
	void MoveHorizontally(float Direction);
	FVector GetFollowLocation() const;

	AActor* FindNearestTarget() const;
	bool IsTargetAttackable(
		const AActor* Target,
		bool bCheckSearchRange) const;
	FVector GetPetTargetLocation(const AActor* Target) const;

	void BeginAttack(AActor* Target);
	void ResolveAttack();
	void FinishAttack();
	void CancelActiveAttack(bool bClearTarget);

	void EnterOwnerDeathState();
	void ExitOwnerDeathState();
	void FinishHurtPreview();
	void FinishDevelopmentDeathPreview();

	void UpdateFacing(float HorizontalDirection);
	void PlayMoveAnimation();
	void PlayOneShotAnimation(UPaperFlipbook* Flipbook);
	float GetAnimationDuration(
		UPaperFlipbook* Flipbook,
		float FallbackDuration) const;
	void DisableInvalidPet();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> PlayerOwner;

	UPROPERTY(Transient)
	FName PetId = NAME_None;

	UPROPERTY(Transient)
	FImmortalPetDefinition ActiveDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> MoveFlipbook;

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> AttackFlipbook;

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> HurtFlipbook;

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> DeathFlipbook;

	TWeakObjectPtr<AActor> CurrentTarget;

	FVector BaseSpriteRelativeLocation = FVector::ZeroVector;

	int32 AttackCount = 0;
	int32 HitCount = 0;
	float NextAttackTime = 0.0f;
	float NextTargetSearchTime = 0.0f;

	bool bAttackPending = false;
	bool bHurtPreviewActive = false;
	bool bDevelopmentDeathPreviewActive = false;
	bool bOwnerDeathState = false;
	bool bRuntimeDefinitionValid = false;

	FTimerHandle AttackWindupTimerHandle;
	FTimerHandle AttackFinishTimerHandle;
	FTimerHandle HurtPreviewTimerHandle;
	FTimerHandle DevelopmentDeathPreviewTimerHandle;
};
