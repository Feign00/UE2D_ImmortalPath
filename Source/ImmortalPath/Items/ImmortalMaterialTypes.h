// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalMaterialTypes.generated.h"

UENUM(BlueprintType)
enum class EImmortalMaterialCategory : uint8
{
	Herb UMETA(DisplayName = "Herb"),
	Monster UMETA(DisplayName = "Monster"),
	Essence UMETA(DisplayName = "Essence"),
	Mineral UMETA(DisplayName = "Mineral"),
	Artifact UMETA(DisplayName = "Artifact")
};

/** One row in the optional /Game/GAME/Data/DT_Materials data table. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalMaterialDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	EImmortalMaterialCategory Category = EImmortalMaterialCategory::Herb;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	FLinearColor DisplayColor = FLinearColor::White;

	/** Single-character fallback used until final material icons are supplied. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	FText IconGlyph;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material", meta = (ClampMin = "1"))
	int32 MaximumStack = 999999;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = "1", ClampMax = "999"))
	int32 MinimumQingyunStage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = "0.0"))
	float DropWeight = 1.0f;
};

/** Stable ID plus quantity; safe to serialize even when new materials are added later. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalMaterialStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Material")
	FName MaterialId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Material", meta = (ClampMin = "0"))
	int32 Quantity = 0;

	bool IsValid() const { return !MaterialId.IsNone() && Quantity > 0; }
};

/** Material catalog, stack operations and stage-aware drop generation. */
UCLASS()
class IMMORTALPATH_API UImmortalMaterialLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Materials")
	static TArray<FName> GetKnownMaterialIds();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Materials")
	static bool GetMaterialDefinition(FName MaterialId, FImmortalMaterialDefinition& OutDefinition);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Materials")
	static FText GetCategoryText(EImmortalMaterialCategory Category);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Materials")
	static int32 GetMaterialQuantity(const TArray<FImmortalMaterialStack>& Inventory, FName MaterialId);

	/** Adds to an existing stack or creates one. Returns the amount actually added. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Materials")
	static int32 AddMaterialStack(UPARAM(ref) TArray<FImmortalMaterialStack>& Inventory, FName MaterialId, int32 Amount);

	/** Transactional removal used by selling and future recipes. No stack changes when the full amount is unavailable. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Materials")
	static bool RemoveMaterialStack(UPARAM(ref) TArray<FImmortalMaterialStack>& Inventory, FName MaterialId, int32 Amount);

	/** Removes invalid entries and merges duplicate IDs after loading old or manually edited saves. */
	static void NormalizeInventory(TArray<FImmortalMaterialStack>& Inventory);

	/** Generates one valid physical/offline drop for the supplied Qingyun Mountain stage. */
	static FImmortalMaterialStack GenerateStageDrop(int32 QingyunStage, bool bBossDrop, int32 DropIndex = 0);
};
