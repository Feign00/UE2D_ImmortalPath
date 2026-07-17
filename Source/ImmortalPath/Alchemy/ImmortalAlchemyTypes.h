// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalAlchemyTypes.generated.h"

UENUM(BlueprintType)
enum class EImmortalPillQuality : uint8
{
	Ordinary UMETA(DisplayName = "Ordinary"),
	Exceptional UMETA(DisplayName = "Exceptional")
};

UENUM(BlueprintType)
enum class EImmortalAlchemyOutcome : uint8
{
	Failure UMETA(DisplayName = "Failure"),
	Ordinary UMETA(DisplayName = "Ordinary"),
	Exceptional UMETA(DisplayName = "Exceptional")
};

UENUM(BlueprintType)
enum class EImmortalPillEffect : uint8
{
	RestoreHealth UMETA(DisplayName = "Restore Health"),
	GrantCultivationPercent UMETA(DisplayName = "Grant Cultivation"),
	CultivationRateBoost UMETA(DisplayName = "Cultivation Rate Boost"),
	CompleteCurrentStage UMETA(DisplayName = "Complete Current Stage")
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalAlchemyIngredient
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alchemy")
	FName MaterialId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alchemy", meta = (ClampMin = "1"))
	int32 Quantity = 1;
};

/** One row in the optional /Game/GAME/Data/DT_AlchemyRecipes data table. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalPillDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pill")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pill", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pill")
	FText IconGlyph;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pill")
	FLinearColor DisplayColor = FLinearColor(0.3f, 1.0f, 0.65f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	TArray<FImmortalAlchemyIngredient> Ingredients;

	/** Absolute chance of producing either ordinary or exceptional pills. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BaseSuccessChance = 0.8f;

	/** Absolute chance inside the success band that becomes exceptional quality. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ExceptionalChance = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unlock", meta = (ClampMin = "0", ClampMax = "9"))
	int32 MinimumRealmIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unlock", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MinimumMinorStage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	EImmortalPillEffect Effect = EImmortalPillEffect::RestoreHealth;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0.0"))
	float OrdinaryMagnitude = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0.0"))
	float ExceptionalMagnitude = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0.0", Units = "s"))
	float OrdinaryDurationSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0.0", Units = "s"))
	float ExceptionalDurationSeconds = 0.0f;
};

/** Pills stack by stable recipe ID and quality. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalPillStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Pill")
	FName PillId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Pill")
	EImmortalPillQuality Quality = EImmortalPillQuality::Ordinary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Pill", meta = (ClampMin = "0"))
	int32 Quantity = 0;

	bool IsValid() const { return !PillId.IsNone() && Quantity > 0; }
};

USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalAlchemyCraftResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Alchemy")
	FName RecipeId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Alchemy")
	EImmortalAlchemyOutcome Outcome = EImmortalAlchemyOutcome::Failure;

	UPROPERTY(BlueprintReadOnly, Category = "Alchemy")
	bool bRecipeFound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Alchemy")
	bool bUnlocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Alchemy")
	bool bHadIngredients = false;

	UPROPERTY(BlueprintReadOnly, Category = "Alchemy")
	bool bMaterialsConsumed = false;

	UPROPERTY(BlueprintReadOnly, Category = "Alchemy")
	int32 PillQuantityGranted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Alchemy")
	FText Message;
};

UCLASS()
class IMMORTALPATH_API UImmortalAlchemyLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	static TArray<FName> GetKnownRecipeIds();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	static bool GetPillDefinition(FName PillId, FImmortalPillDefinition& OutDefinition);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	static bool IsRecipeUnlocked(const FImmortalPillDefinition& Definition, int32 RealmIndex, int32 MinorStage);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	static bool CanCraft(const TArray<FImmortalMaterialStack>& Materials, const FImmortalPillDefinition& Definition);

	/** Transactional: checks the complete recipe before changing any material stack. */
	static bool ConsumeIngredients(TArray<FImmortalMaterialStack>& Materials, const FImmortalPillDefinition& Definition);

	/** Pure deterministic outcome used by runtime random rolls and automation tests. */
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	static EImmortalAlchemyOutcome CalculateOutcome(
		const FImmortalPillDefinition& Definition,
		float Roll,
		float SuccessChanceBonus = 0.0f,
		float ExceptionalChanceBonus = 0.0f);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	static int32 GetPillQuantity(
		const TArray<FImmortalPillStack>& Inventory,
		FName PillId,
		EImmortalPillQuality Quality);

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Alchemy")
	static int32 AddPillStack(
		UPARAM(ref) TArray<FImmortalPillStack>& Inventory,
		FName PillId,
		EImmortalPillQuality Quality,
		int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Alchemy")
	static bool RemovePill(
		UPARAM(ref) TArray<FImmortalPillStack>& Inventory,
		FName PillId,
		EImmortalPillQuality Quality,
		int32 Amount = 1);

	static void NormalizePillInventory(TArray<FImmortalPillStack>& Inventory);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	static FText GetQualityText(EImmortalPillQuality Quality);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	static FLinearColor GetQualityColor(EImmortalPillQuality Quality);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	static FText GetEffectText(const FImmortalPillDefinition& Definition, EImmortalPillQuality Quality);
};

