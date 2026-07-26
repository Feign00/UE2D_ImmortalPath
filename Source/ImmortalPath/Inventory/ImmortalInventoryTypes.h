// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Alchemy/ImmortalAlchemyTypes.h"
#include "../Artifacts/ImmortalArtifactTypes.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImmortalInventoryTypes.generated.h"

/** Stable backpack categories used by the native TBH inventory screen. */
UENUM(BlueprintType)
enum class EImmortalInventoryCategory : uint8
{
	Equipment UMETA(DisplayName = "Equipment"),
	Material UMETA(DisplayName = "Material"),
	Pill UMETA(DisplayName = "Pill"),
	Artifact UMETA(DisplayName = "Artifact"),
	QuestItem UMETA(DisplayName = "Quest Item")
};

/** One data-driven task item. Task items cannot be sold or dismantled. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalQuestItemDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest Item") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest Item", meta = (MultiLine = "true")) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest Item") FText IconGlyph;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest Item") FLinearColor DisplayColor = FLinearColor(0.95f, 0.78f, 0.28f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest Item", meta = (ClampMin = "1")) int32 MaximumStack = 999;
};

/** Stable task-item ID plus quantity, serialized independently of equipment capacity. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalQuestItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest Item") FName QuestItemId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Quest Item", meta = (ClampMin = "0")) int32 Quantity = 0;

	bool IsValid() const { return !QuestItemId.IsNone() && Quantity > 0; }
};

/** Shared result for organize, lock, sale and dismantle transactions. */
USTRUCT(BlueprintType)
struct IMMORTALPATH_API FImmortalInventoryOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory") bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Inventory") bool bPersistenceFailed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Inventory") int32 AffectedItemCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Inventory") int32 SkippedLockedItemCount = 0;
	/** Positive when a sale grants spirit stones. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory") int32 SpiritStoneDelta = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Inventory") TArray<FImmortalMaterialStack> MaterialRewards;
	UPROPERTY(BlueprintReadOnly, Category = "Inventory") FText Message;
};

/** Pure inventory catalog, sorting, filtering and deterministic dismantle rules. */
UCLASS()
class IMMORTALPATH_API UImmortalInventoryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Inventory|Quest Items")
	static TArray<FName> GetKnownQuestItemIds();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Inventory|Quest Items")
	static bool GetQuestItemDefinition(FName QuestItemId, FImmortalQuestItemDefinition& OutDefinition);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Inventory|Quest Items")
	static int32 GetQuestItemQuantity(const TArray<FImmortalQuestItemStack>& Inventory, FName QuestItemId);

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Inventory|Quest Items")
	static int32 AddQuestItemStack(
		UPARAM(ref) TArray<FImmortalQuestItemStack>& Inventory,
		FName QuestItemId,
		int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Inventory|Quest Items")
	static bool RemoveQuestItemStack(
		UPARAM(ref) TArray<FImmortalQuestItemStack>& Inventory,
		FName QuestItemId,
		int32 Amount);

	static void NormalizeQuestItemInventory(TArray<FImmortalQuestItemStack>& Inventory);

	/** Repairs invalid/duplicate IDs and duplicate equipped slots without deleting a distinct valid item. */
	static bool NormalizeEquipmentCollections(
		TArray<FImmortalEquipmentItem>& Inventory,
		TArray<FImmortalEquipmentItem>& Equipped);

	/** Locked first, then slot, quality, level, power, name and stable instance ID. */
	static bool SortEquipmentInventory(TArray<FImmortalEquipmentItem>& Inventory);

	/** Equipped artifact first, followed by quality, stars, level, name and stable instance ID. */
	static bool SortArtifactInventory(TArray<FImmortalArtifactItem>& Inventory, FGuid EquippedInstanceId);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Inventory|Equipment")
	static bool IsEligibleForBulkAction(
		const FImmortalEquipmentItem& Item,
		EImmortalEquipmentQuality MaximumQuality);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Inventory|Equipment")
	static int32 GetBulkEligibleEquipmentCount(
		const TArray<FImmortalEquipmentItem>& Inventory,
		EImmortalEquipmentQuality MaximumQuality);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Inventory|Equipment")
	static int32 GetBulkEquipmentSellValue(
		const TArray<FImmortalEquipmentItem>& Inventory,
		EImmortalEquipmentQuality MaximumQuality);

	/** Deterministic yield: ore for every item, spirit iron for higher quality and fragments for epic/legendary. */
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Inventory|Equipment")
	static TArray<FImmortalMaterialStack> GetEquipmentDismantleYield(const FImmortalEquipmentItem& Item);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Inventory|Equipment")
	static TArray<FImmortalMaterialStack> GetBulkEquipmentDismantleYield(
		const TArray<FImmortalEquipmentItem>& Inventory,
		EImmortalEquipmentQuality MaximumQuality);

	/** Applies every reward to a candidate copy and commits only when every stack fits in full. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Inventory|Safety")
	static bool TryAddMaterialRewards(
		UPARAM(ref) TArray<FImmortalMaterialStack>& Inventory,
		const TArray<FImmortalMaterialStack>& Rewards);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Inventory|Safety")
	static bool CanReceiveSpiritStones(int32 CurrentSpiritStones, int64 Amount);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Inventory|Display")
	static FText GetCategoryText(EImmortalInventoryCategory Category);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Inventory|Display")
	static FText GetMaximumQualityText(EImmortalEquipmentQuality MaximumQuality);
};
