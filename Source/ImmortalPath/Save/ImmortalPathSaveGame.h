// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Alchemy/ImmortalAlchemyTypes.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "GameFramework/SaveGame.h"
#include "ImmortalPathSaveGame.generated.h"

/** Versioned persistent data shared by the combat map and future progression systems. */
UCLASS()
class IMMORTALPATH_API UImmortalPathSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSaveVersion = 7;

	UImmortalPathSaveGame();

	static FString GetSlotName();
	static UImmortalPathSaveGame* LoadOrCreate(const UObject* WorldContextObject);
	bool SaveToDisk();

	UPROPERTY(SaveGame)
	int32 SaveVersion = CurrentSaveVersion;

	UPROPERTY(SaveGame)
	int64 LastSavedUtcTicks = 0;

	/** Audit fields for one-time offline claims and future statistics UI. */
	UPROPERTY(SaveGame)
	int64 LastOfflineClaimUtcTicks = 0;

	UPROPERTY(SaveGame)
	int64 TotalRewardedOfflineSeconds = 0;

	UPROPERTY(SaveGame)
	int32 TotalOfflineClaims = 0;

	UPROPERTY(SaveGame)
	bool bHasPlayerData = false;

	UPROPERTY(SaveGame)
	float PlayerHealth = 0.0f;

	UPROPERTY(SaveGame)
	float PlayerMana = 0.0f;

	UPROPERTY(SaveGame)
	int32 Cultivation = 0;

	UPROPERTY(SaveGame)
	bool bHasCultivationData = false;

	/** Serialized EImmortalCultivationRealm value without coupling SaveGame to the component header. */
	UPROPERTY(SaveGame)
	int32 CultivationRealmIndex = 0;

	UPROPERTY(SaveGame)
	int32 CultivationMinorStage = 1;

	/** Current physical spirit-stone currency total. */
	UPROPERTY(SaveGame)
	int32 SpiritStones = 0;

	UPROPERTY(SaveGame)
	int32 EquipmentDropCount = 0;

	UPROPERTY(SaveGame)
	TArray<FImmortalEquipmentItem> InventoryItems;

	UPROPERTY(SaveGame)
	TArray<FImmortalEquipmentItem> EquippedItems;

	/** Unlimited stack-based categorized inventory, separate from the 30 equipment slots. */
	UPROPERTY(SaveGame)
	TArray<FImmortalMaterialStack> MaterialInventory;

	UPROPERTY(SaveGame)
	TArray<FImmortalPillStack> PillInventory;

	/** Online-only悟道丹 buff pauses while the game is closed and resumes from this duration. */
	UPROPERTY(SaveGame)
	float AlchemyCultivationBoostMultiplier = 1.0f;

	UPROPERTY(SaveGame)
	float AlchemyCultivationBoostRemainingSeconds = 0.0f;

	UPROPERTY(SaveGame)
	bool bHasStageData = false;

	UPROPERTY(SaveGame)
	int32 QingyunStage = 1;

	UPROPERTY(SaveGame)
	int32 QingyunStageKills = 0;

	/** True only after the final stage 999 boss has been defeated. */
	UPROPERTY(SaveGame)
	bool bQingyunMountainCompleted = false;
};
