// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "PaperCharacter.h"
#include "ImmortalPlayerCharacter.generated.h"

class AController;
class UCameraComponent;
class UDamageType;
class UImmortalInventoryWidget;
class UImmortalPlayerStatusWidget;
class UInputComponent;
class USpringArmComponent;

/**
 * C++ base for the 2D player character.
 *
 * Reparent the existing Player Blueprint to this class to keep its flipbook,
 * transform and other Blueprint setup while gaining automatic attacks.
 */
UCLASS(Blueprintable)
class IMMORTALPATH_API AImmortalPlayerCharacter : public APaperCharacter
{
	GENERATED_BODY()

public:
	AImmortalPlayerCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(
		float DamageAmount,
		const FDamageEvent& DamageEvent,
		AController* EventInstigator,
		AActor* DamageCauser) override;

	/** Start searching for targets and attacking. Safe to call more than once. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Combat")
	void StartAutoAttack();

	/** Stop attacking and clear any pending strike. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Combat")
	void StopAutoAttack();

	/** Immediately perform one target search/attack attempt. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Combat")
	void TryAutoAttack();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Combat")
	AActor* GetCurrentAttackTarget() const { return CurrentAttackTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Attributes")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Attributes")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Attributes")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Attributes")
	float GetCurrentMana() const { return CurrentMana; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Attributes")
	float GetMaxMana() const { return MaxMana; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Attributes")
	float GetManaPercent() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Combat")
	float GetEffectiveAttackInterval() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Combat")
	float GetTotalAttackDamage() const { return AttackDamage + EquippedAttackBonus; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Attributes")
	float GetTotalDefense() const { return Defense + EquippedDefenseBonus; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Combat")
	float GetTotalAttackSpeedMultiplier() const { return AttackSpeedMultiplier + EquippedAttackSpeedBonus; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Combat")
	float GetTotalCriticalChance() const { return FMath::Clamp(CriticalChance + EquippedCriticalChanceBonus, 0.0f, 1.0f); }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Progression")
	float GetCombatPower() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Progression")
	int32 GetCultivation() const { return CurrentCultivation; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Progression")
	int32 GetGold() const { return CurrentGold; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Progression")
	int32 GetEquipmentDropCount() const { return EquipmentDropCount; }

	/** Adds the rewards granted by a killed monster. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Progression")
	void ReceiveKillRewards(int32 CultivationGained, int32 GoldGained);

	/** Converts an automatically collected equipment orb into a pending equipment entry. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Progression")
	void ReceiveEquipmentDrop(int32 Amount = 1);

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Equipment")
	bool ReceiveEquipmentItem(const FImmortalEquipmentItem& Item);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	TArray<FImmortalEquipmentItem> GetInventoryItems() const { return InventoryItems; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	TArray<FImmortalEquipmentItem> GetEquippedItems() const { return EquippedItems; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	int32 GetInventoryCapacity() const { return FMath::Max(InventoryCapacity, 1); }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	bool GetEquippedItemForSlot(EImmortalEquipmentSlot Slot, FImmortalEquipmentItem& OutItem) const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Attributes")
	bool IsDead() const { return bDead; }

	/** Opens or closes the native equipment/backpack screen. Bound to the I key. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|UI")
	void ToggleInventory();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|UI")
	bool IsInventoryOpen() const { return bInventoryOpen; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Attributes", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Attributes", meta = (ClampMin = "0.0"))
	float MaxMana = 50.0f;

	/** Flat damage reduction applied after Unreal accepts incoming damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Attributes", meta = (ClampMin = "0.0"))
	float Defense = 2.0f;

	/** Base damage passed to the target's AnyDamage event / TakeDamage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 10.0f;

	/** Maximum world-space distance from the player to a target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat", meta = (ClampMin = "1.0", Units = "cm"))
	float AttackRange = 220.0f;

	/** Seconds between attack attempts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat", meta = (ClampMin = "0.05", Units = "s"))
	float AttackInterval = 1.0f;

	/** Multiplies attack frequency. 2.0 means twice as many attacks per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat", meta = (ClampMin = "0.1"))
	float AttackSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CriticalChance = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat", meta = (ClampMin = "1.0"))
	float CriticalDamageMultiplier = 1.5f;

	/** Delay between starting the animation and applying damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat", meta = (ClampMin = "0.0", Units = "s"))
	float AttackWindup = 0.25f;

	/** Automatically enable combat when play begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat")
	bool bAutoAttackOnBeginPlay = true;

	/** Draw the search radius while playing for quick editor verification. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat|Debug")
	bool bDrawAttackRange = false;

	/** Shift and widen the existing Blueprint camera for idle combat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Camera")
	bool bConfigureCombatCamera = true;

	/** Moves the camera centre toward monsters so the player stays left of centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Camera", meta = (EditCondition = "bConfigureCombatCamera", Units = "cm"))
	float CameraLeadDistance = 70.0f;

	/** Width used when the Blueprint camera is orthographic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Camera", meta = (EditCondition = "bConfigureCombatCamera", ClampMin = "100.0", Units = "cm"))
	float CombatOrthoWidth = 1600.0f;

	/** Field of view used when the Blueprint camera is perspective. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Camera", meta = (EditCondition = "bConfigureCombatCamera", ClampMin = "5.0", ClampMax = "170.0", Units = "deg"))
	float CombatPerspectiveFOV = 115.0f;

	/** Optional custom damage type for later elemental/skill expansion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat")
	TSubclassOf<UDamageType> DamageTypeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Progression", meta = (ClampMin = "0"))
	int32 StartingCultivation = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Progression", meta = (ClampMin = "0"))
	int32 StartingGold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Equipment", meta = (ClampMin = "1"))
	int32 InventoryCapacity = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Equipment")
	bool bAutoEquipNewItems = true;

	/** Called when an attack begins. Override this in Player BP to play the attack flipbook. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Combat", meta = (DisplayName = "On Auto Attack Started"))
	void BP_OnAutoAttackStarted(AActor* Target);

	/** Called after the strike resolves; DamageDealt is zero if the target became invalid. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Combat", meta = (DisplayName = "On Auto Attack Resolved"))
	void BP_OnAutoAttackResolved(AActor* Target, float DamageDealt);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Combat", meta = (DisplayName = "On Player Critical Hit"))
	void BP_OnPlayerCriticalHit(AActor* Target, float DamageDealt);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Attributes", meta = (DisplayName = "On Player Damaged"))
	void BP_OnPlayerDamaged(float DamageApplied, float RemainingHealth, AActor* DamageCauser);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Attributes", meta = (DisplayName = "On Player Died"))
	void BP_OnPlayerDied(AActor* DamageCauser);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Progression", meta = (DisplayName = "On Rewards Changed"))
	void BP_OnRewardsChanged(int32 NewCultivation, int32 NewGold, int32 CultivationGained, int32 GoldGained);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Progression", meta = (DisplayName = "On Equipment Picked Up"))
	void BP_OnEquipmentPickedUp(int32 NewEquipmentCount, int32 AmountPickedUp);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Equipment", meta = (DisplayName = "On Inventory Changed"))
	void BP_OnInventoryChanged(int32 InventoryCount, int32 Capacity);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Equipment", meta = (DisplayName = "On Equipment Changed"))
	void BP_OnEquipmentChanged(EImmortalEquipmentSlot Slot, const FImmortalEquipmentItem& Item, bool bAutoEquipped, float NewCombatPower);

private:
	void ConfigureCombatCamera();
	AActor* FindNearestTarget() const;
	bool IsTargetAttackable(const AActor* Target, bool bCheckRange) const;
	FVector GetAutoAttackLocation(const AActor* Target) const;
	void ResolvePendingAttack();
	void RecalculateEquipmentBonuses();
	bool AddItemToInventory(const FImmortalEquipmentItem& Item);

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentAttackTarget;

	FTimerHandle AutoAttackTimerHandle;
	FTimerHandle AttackWindupTimerHandle;
	bool bAttackPending = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Attributes", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Attributes", meta = (AllowPrivateAccess = "true"))
	float CurrentMana = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Progression", meta = (AllowPrivateAccess = "true"))
	int32 CurrentCultivation = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Progression", meta = (AllowPrivateAccess = "true"))
	int32 CurrentGold = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Progression", meta = (AllowPrivateAccess = "true"))
	int32 EquipmentDropCount = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Equipment", meta = (AllowPrivateAccess = "true"))
	TArray<FImmortalEquipmentItem> InventoryItems;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Equipment", meta = (AllowPrivateAccess = "true"))
	TArray<FImmortalEquipmentItem> EquippedItems;

	UPROPERTY(Transient)
	float EquippedAttackBonus = 0.0f;

	UPROPERTY(Transient)
	float EquippedDefenseBonus = 0.0f;

	UPROPERTY(Transient)
	float EquippedHealthBonus = 0.0f;

	UPROPERTY(Transient)
	float EquippedAttackSpeedBonus = 0.0f;

	UPROPERTY(Transient)
	float EquippedCriticalChanceBonus = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UImmortalPlayerStatusWidget> PlayerStatusWidget;

	UPROPERTY(Transient)
	TObjectPtr<UImmortalInventoryWidget> PlayerInventoryWidget;

	bool bInventoryOpen = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Attributes", meta = (AllowPrivateAccess = "true"))
	bool bDead = false;
};
