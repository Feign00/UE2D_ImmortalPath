// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Crafting/ImmortalCraftingTypes.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/SoftObjectPtr.h"
#include "ImmortalPetTypes.generated.h"

class UPaperFlipbook;

UENUM(BlueprintType)
enum class EImmortalPetAttackStyle : uint8
{
	Melee UMETA(DisplayName = "Melee"),
	Ranged UMETA(DisplayName = "Ranged")
};

/**
 * Data-driven definition for one combat pet.
 *
 * The native Fox/Dog catalog is always available. A matching row in
 * /Game/GAME/Data/DT_Pets can replace presentation and balance values without
 * changing the persistent PetId.
 */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalPetDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet")
	FText IconGlyph;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet")
	EImmortalPetAttackStyle AttackStyle = EImmortalPetAttackStyle::Melee;

	/** The first owned pet is equipped automatically on a new or migrated save. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet")
	bool bInitiallyOwned = false;

	/** Paid only once when a locked pet is tamed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Growth")
	FImmortalCraftingCost UnlockCost;

	/** Fraction of the owner's current attack used by each pet strike. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Combat", meta = (ClampMin = "0.01"))
	float BaseDamageRatio = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Combat", meta = (ClampMin = "0.0"))
	float DamageRatioPerLevel = 0.006f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Combat", meta = (ClampMin = "0.0"))
	float DamageRatioPerStar = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Combat", meta = (ClampMin = "0.1", Units = "s"))
	float AttackInterval = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Combat", meta = (ClampMin = "1.0", Units = "cm"))
	float AttackRange = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Combat", meta = (ClampMin = "1.0", Units = "cm"))
	float SearchRange = 920.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CriticalChance = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Movement", meta = (ClampMin = "1.0", Units = "cm/s"))
	float MovementSpeed = 520.0f;

	/** Horizontal resting offset relative to the owner. Negative stays behind the player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Movement", meta = (Units = "cm"))
	float FollowOffsetX = -145.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Movement", meta = (ClampMin = "100.0", Units = "cm"))
	float MaximumLeashDistance = 760.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Presentation", meta = (ClampMin = "0.1"))
	float VisualScale = 2.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Presentation")
	FLinearColor DisplayColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Presentation")
	TSoftObjectPtr<UPaperFlipbook> MoveFlipbook;

	/** Optional idle art; legacy catalogs keep a stationary move-frame fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Presentation")
	TSoftObjectPtr<UPaperFlipbook> IdleFlipbook;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Presentation")
	TSoftObjectPtr<UPaperFlipbook> AttackFlipbook;

	/** Reserved for later pet-health gameplay and available to development previews now. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Presentation")
	TSoftObjectPtr<UPaperFlipbook> HurtFlipbook;

	/** Played while the owner is dead; the pet resumes following after auto-revive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|Presentation")
	TSoftObjectPtr<UPaperFlipbook> DeathFlipbook;

	bool IsValid() const;
};

/** Persistent growth for one catalog entry. Runtime actors and targets are never serialized. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalPetProgress
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Pet")
	FName PetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Pet")
	bool bOwned = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Pet", meta = (ClampMin = "1", ClampMax = "50"))
	int32 Level = 1;

	/** Experience inside the current level. It is zero at level 50. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Pet", meta = (ClampMin = "0"))
	int32 Experience = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Pet", meta = (ClampMin = "0", ClampMax = "5"))
	int32 Stars = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Pet", meta = (ClampMin = "0"))
	int64 TotalCombatKills = 0;

	bool IsValid() const { return !PetId.IsNone(); }
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalPetState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Pet")
	bool bInitialized = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Pet")
	int32 Revision = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Pet")
	FName ActivePetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Pet")
	TArray<FImmortalPetProgress> Pets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Pet", meta = (ClampMin = "0"))
	int64 TotalCombatKills = 0;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalPetExperienceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Pet")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Pet")
	FName PetId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Pet")
	int32 ExperienceGranted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Pet")
	int32 PreviousLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Pet")
	int32 CurrentLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Pet")
	int32 CurrentExperience = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Pet")
	int32 ExperienceToNextLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Pet")
	int32 LevelsGained = 0;
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalPetOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Pet")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Pet")
	bool bAlreadyOwned = false;

	UPROPERTY(BlueprintReadOnly, Category = "Pet")
	bool bAffordable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Pet")
	FName PetId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Pet")
	FText Message;
};

UCLASS()
class IMMORTALPATH_API UImmortalPetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static constexpr int32 MaximumPetLevel = 50;
	static constexpr int32 MaximumPetStars = 5;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Pet")
	static TArray<FName> GetKnownPetIds();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Pet")
	static bool GetPetDefinition(FName PetId, FImmortalPetDefinition& OutDefinition);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Pet")
	static FImmortalPetState CreateDefaultState();

	/** Repairs malformed values, adds missing catalog entries and returns whether state changed. */
	static bool NormalizeState(FImmortalPetState& State);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Pet")
	static bool GetPetProgress(
		const FImmortalPetState& State,
		FName PetId,
		FImmortalPetProgress& OutProgress);

	static FImmortalPetProgress* FindMutablePetProgress(
		FImmortalPetState& State,
		FName PetId);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Pet")
	static int32 GetExperienceRequiredForLevel(int32 Level);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Pet")
	static float CalculateDamageRatio(
		const FImmortalPetDefinition& Definition,
		const FImmortalPetProgress& Progress);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Pet")
	static float CalculateCombatPower(
		const FImmortalPetDefinition& Definition,
		const FImmortalPetProgress& Progress,
		float OwnerAttack);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Pet")
	static FImmortalCraftingCost GetStarUpCost(const FImmortalPetProgress& Progress);

	/** Pure state mutation. Resource payment and disk rollback are owned by the player. */
	static bool UnlockPet(FImmortalPetState& State, FName PetId);

	/** Pure state mutation. The pet must already be owned. */
	static bool SetActivePet(FImmortalPetState& State, FName PetId);

	/** Pure state mutation. Resource payment and disk rollback are owned by the player. */
	static bool RaiseStar(FImmortalPetState& State, FName PetId);

	/** Grants experience only to the currently active owned pet. */
	static FImmortalPetExperienceResult GrantActivePetExperience(
		FImmortalPetState& State,
		int32 Experience,
		int32 CombatKills = 1);

	/** Stable reward curve shared by map, World Boss and Endless Dungeon settlement. */
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Pet")
	static int32 CalculateCombatExperienceReward(
		int32 DifficultyIndex,
		bool bElite,
		bool bBoss,
		bool bWorldBoss);
};
