// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Alchemy/ImmortalAlchemyTypes.h"
#include "../Artifacts/ImmortalArtifactTypes.h"
#include "../Crafting/ImmortalCraftingTypes.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "../Progression/ImmortalCultivationComponent.h"
#include "../Progression/ImmortalCharacterPathTypes.h"
#include "../Progression/ImmortalOfflineRewardTypes.h"
#include "../Shop/ImmortalShopTypes.h"
#include "../Techniques/ImmortalTechniqueTypes.h"
#include "PaperCharacter.h"
#include "ImmortalPlayerCharacter.generated.h"

class AController;
class UCameraComponent;
class UDamageType;
class UImmortalCombatFeedbackWidget;
class UImmortalAlchemyWidget;
class UImmortalArtifactWidget;
class UImmortalCraftingWidget;
class UImmortalInventoryWidget;
class UImmortalPlayerStatusWidget;
class UImmortalTechniqueWidget;
class UImmortalCharacterBuildWidget;
class UImmortalShopWidget;
class UInputComponent;
class USpringArmComponent;
class UUserWidget;
class UImmortalPathSaveGame;

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
	float GetMaxMana() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Attributes")
	float GetManaPercent() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Combat")
	float GetEffectiveAttackInterval() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Combat")
	float GetTotalAttackDamage() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Attributes")
	float GetTotalDefense() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Combat")
	float GetTotalAttackSpeedMultiplier() const { return AttackSpeedMultiplier + EquippedAttackSpeedBonus + ArtifactAttackSpeedBonus + TechniqueAttackSpeedBonus + CharacterPathAttackSpeedBonus; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Combat")
	float GetTotalCriticalChance() const { return FMath::Clamp(CriticalChance + EquippedCriticalChanceBonus + ArtifactCriticalChanceBonus + TechniqueCriticalChanceBonus + CharacterPathCriticalChanceBonus, 0.0f, 1.0f); }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Progression")
	float GetCombatPower() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Progression")
	int32 GetCultivation() const { return CurrentCultivation; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	EImmortalCultivationRealm GetCultivationRealm() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	int32 GetCultivationMinorStage() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	FText GetFullCultivationRealmName() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	int32 GetRequiredCultivation() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	float GetCultivationPerSecond() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	UImmortalCultivationComponent* GetCultivationComponent() const { return CultivationComponent; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Offline")
	FImmortalOfflineRewardResult GetLastOfflineReward() const { return LastOfflineRewardResult; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Progression")
	int32 GetGold() const { return CurrentGold; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Progression")
	int32 GetEquipmentDropCount() const { return EquipmentDropCount; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Progression")
	int32 GetQingyunStage() const { return DisplayedStage; }

	/** Adds the rewards granted by a killed monster. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Progression")
	void ReceiveKillRewards(int32 CultivationGained, int32 GoldGained);

	/** Adds automatically collected physical spirit-stone drops. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Progression")
	void ReceiveSpiritStones(int32 Amount, FVector PickupWorldLocation);

	/** Updates the persistent Qingyun Mountain stage banner. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Progression")
	void UpdateStageProgress(
		int32 Stage,
		int32 Kills,
		int32 RequiredKills,
		bool bBossStage = false,
		bool bMapCompleted = false);

	/** Displays a temporary boss spawn, phase or victory announcement. */
	void ShowBossMessage(const FText& Message, const FLinearColor& Color);

	void ShowRewardFeedback(const FVector& WorldLocation, int32 Cultivation, int32 SpiritStones);

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

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Materials")
	TArray<FImmortalMaterialStack> GetMaterialInventory() const { return MaterialInventory; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Materials")
	int32 GetMaterialQuantity(FName MaterialId) const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Materials")
	int32 GetMaterialInventoryRevision() const { return MaterialInventoryRevision; }

	/** Adds a physical or scripted material pickup to the categorized backpack. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Materials")
	int32 ReceiveMaterial(FName MaterialId, int32 Amount, FVector PickupWorldLocation);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	TArray<FImmortalPillStack> GetPillInventory() const { return PillInventory; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	int32 GetPillQuantity(FName PillId, EImmortalPillQuality Quality) const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	int32 GetPillInventoryRevision() const { return PillInventoryRevision; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	bool IsAlchemyRecipeUnlocked(FName RecipeId) const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	bool CanCraftPill(FName RecipeId) const;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Alchemy")
	FImmortalAlchemyCraftResult CraftPill(FName RecipeId);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	bool CanUsePill(FName PillId, EImmortalPillQuality Quality) const;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Alchemy")
	bool UsePill(FName PillId, EImmortalPillQuality Quality);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	float GetPillEffectMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	FText GetEffectivePillEffectText(FName PillId, EImmortalPillQuality Quality) const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	float GetAlchemyBoostMultiplier() const { return AlchemyCultivationBoostMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Alchemy")
	float GetAlchemyBoostRemainingSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Crafting")
	int32 GetEquipmentInventoryRevision() const { return EquipmentInventoryRevision; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Crafting")
	bool GetEquipmentItemById(FGuid ItemId, FImmortalEquipmentItem& OutItem, bool& bOutEquipped) const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Crafting")
	bool IsCraftingRecipeUnlocked(FName RecipeId) const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Crafting")
	bool CanCraftEquipment(FName RecipeId) const;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Crafting")
	FImmortalCraftingResult CraftEquipment(FName RecipeId);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Crafting")
	bool CanEnhanceEquipment(FGuid ItemId) const;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Crafting")
	FImmortalCraftingResult EnhanceEquipment(FGuid ItemId);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Crafting")
	bool CanRefineEquipment(FGuid ItemId) const;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Crafting")
	FImmortalCraftingResult RefineEquipment(FGuid ItemId);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact")
	TArray<FImmortalArtifactItem> GetArtifactInventory() const { return ArtifactInventory; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact")
	int32 GetArtifactInventoryRevision() const { return ArtifactInventoryRevision; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact")
	bool GetEquippedArtifact(FImmortalArtifactItem& OutArtifact) const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact")
	bool IsArtifactUnlocked(FName ArtifactId) const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact")
	bool CanCraftArtifact(FName ArtifactId) const;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Artifact")
	FImmortalArtifactOperationResult CraftArtifact(FName ArtifactId);

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Artifact")
	FImmortalArtifactOperationResult EquipArtifact(FGuid InstanceId);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact")
	bool CanUpgradeArtifact(FGuid InstanceId) const;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Artifact")
	FImmortalArtifactOperationResult UpgradeArtifact(FGuid InstanceId);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact")
	bool CanStarUpArtifact(FGuid InstanceId) const;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Artifact")
	FImmortalArtifactOperationResult StarUpArtifact(FGuid InstanceId);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Artifact")
	float GetArtifactShield() const { return ArtifactShield; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique")
	TArray<FImmortalTechniqueProgress> GetTechniqueLibrary() const { return TechniqueLibrary; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique")
	TArray<FName> GetEquippedTechniqueIds() const { return EquippedTechniqueIds; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique")
	int32 GetTechniqueRevision() const { return TechniqueRevision; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique")
	int32 GetTechniqueInsightPoints() const { return TechniqueInsightPoints; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique")
	float GetTechniqueShield() const { return TechniqueShield; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique")
	bool GetTechniqueProgress(FName TechniqueId, FImmortalTechniqueProgress& OutProgress) const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique")
	bool IsTechniqueLearned(FName TechniqueId) const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique")
	bool IsTechniqueUnlocked(FName TechniqueId) const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique")
	bool IsTechniqueEquipped(FName TechniqueId, int32& OutSlotIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Technique")
	FImmortalTechniqueOperationResult LearnTechnique(FName TechniqueId);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique")
	bool CanUpgradeTechnique(FName TechniqueId) const;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Technique")
	FImmortalTechniqueOperationResult UpgradeTechnique(FName TechniqueId);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique")
	bool CanBreakthroughTechnique(FName TechniqueId) const;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Technique")
	FImmortalTechniqueOperationResult BreakthroughTechnique(FName TechniqueId);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Technique")
	bool CanAllocateTechniquePoint(FName TechniqueId, EImmortalTechniquePointBranch Branch) const;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Technique")
	FImmortalTechniqueOperationResult AllocateTechniquePoint(FName TechniqueId, EImmortalTechniquePointBranch Branch);

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Technique")
	FImmortalTechniqueOperationResult EquipTechnique(FName TechniqueId, int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Character Build")
	FImmortalSpiritRootState GetSpiritRootState() const { return SpiritRootState; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Character Build")
	FImmortalCultivationPathState GetCultivationPathState() const { return CultivationPathState; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Character Build")
	int32 GetCharacterBuildRevision() const { return CharacterBuildRevision; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Character Build")
	float GetElementDamageMultiplier(EImmortalElementType Element) const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Character Build")
	bool IsEquipmentCompatibleWithPath(const FImmortalEquipmentItem& Item) const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Character Build")
	bool CanSelectCultivationPath(EImmortalCultivationPath NewPath) const;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Character Build")
	FImmortalCharacterPathOperationResult SelectCultivationPath(EImmortalCultivationPath NewPath);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop")
	FImmortalShopState GetShopState() const { return ShopState; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop")
	int32 GetShopRevision() const { return ShopRevision; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop")
	int64 GetShopSecondsUntilRefresh() const;

	/** Applies the free daily refresh when the configured calendar day advances. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Shop")
	bool EnsureDailyShopRefresh();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop")
	bool CanBuyShopListing(FGuid ListingId) const;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Shop")
	FImmortalShopTransactionResult BuyShopListing(FGuid ListingId);

	/** Paid same-day reroll. The first costs 100 stones and subsequent rerolls increase by 100. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Shop")
	FImmortalShopTransactionResult RefreshShopInventory();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop")
	int32 GetEquipmentShopSellPrice(FGuid ItemId) const;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Shop")
	FImmortalShopTransactionResult SellEquipmentToShop(FGuid ItemId);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Shop")
	int32 GetMaterialShopSellPrice(FName MaterialId, int32 Amount = 1) const;

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Shop")
	FImmortalShopTransactionResult SellMaterialToShop(FName MaterialId, int32 Amount = 1);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Equipment")
	bool GetEquippedItemForSlot(EImmortalEquipmentSlot Slot, FImmortalEquipmentItem& OutItem) const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Attributes")
	bool IsDead() const { return bDead; }

	/** Opens or closes the native equipment/backpack screen. Bound to the I key. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|UI")
	void ToggleInventory();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|UI")
	bool IsInventoryOpen() const { return bInventoryOpen; }

	/** Opens or closes the native alchemy furnace. Bound to the L key. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|UI")
	void ToggleAlchemy();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|UI")
	bool IsAlchemyOpen() const { return bAlchemyOpen; }

	/** Opens or closes the native crafting furnace. Bound to the K key. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|UI")
	void ToggleCrafting();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|UI")
	bool IsCraftingOpen() const { return bCraftingOpen; }

	/** Opens or closes the independent artifact screen. Bound to the F key. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|UI")
	void ToggleArtifacts();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|UI")
	bool IsArtifactScreenOpen() const { return bArtifactOpen; }

	/** Opens or closes the independent technique screen. Bound to the G key. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|UI")
	void ToggleTechniques();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|UI")
	bool IsTechniqueScreenOpen() const { return bTechniqueOpen; }

	/** Opens or closes the combined spirit-root and cultivation-path screen. Bound to H. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|UI")
	void ToggleCharacterBuild();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|UI")
	bool IsCharacterBuildScreenOpen() const { return bCharacterBuildOpen; }

	/** Opens or closes the Treasure Pavilion. Bound to B. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|UI")
	void ToggleShop();

	UFUNCTION(BlueprintPure, Category = "Immortal Path|UI")
	bool IsShopOpen() const { return bShopOpen; }

	/** Writes attributes, spirit stones, backpack and equipped items to the main slot. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Save")
	bool SaveProgress();

	/** Restores player data from the main slot. Returns false when no player save exists yet. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Save")
	bool LoadProgress();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Immortal Path|Cultivation")
	TObjectPtr<UImmortalCultivationComponent> CultivationComponent;

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

	/** Horizontal world-space lead used by the TBH strip so the player stays near the left side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Camera", meta = (EditCondition = "bConfigureCombatCamera", Units = "cm"))
	float CameraLeadDistance = 850.0f;

	/** Local boom depth needed to keep the existing perspective camera on the Paper2D plane. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Camera", meta = (EditCondition = "bConfigureCombatCamera", Units = "cm"))
	float CameraDepthOffset = 70.0f;

	/** Width used when the Blueprint camera is orthographic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Camera", meta = (EditCondition = "bConfigureCombatCamera", ClampMin = "100.0", Units = "cm"))
	float CombatOrthoWidth = 1600.0f;

	/** Field of view used when the Blueprint camera is perspective. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Camera", meta = (EditCondition = "bConfigureCombatCamera", ClampMin = "5.0", ClampMax = "170.0", Units = "deg"))
	float CombatPerspectiveFOV = 115.0f;

	/** TBH-style borderless strip docked above the Windows taskbar in standalone play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Camera|Taskbar")
	bool bEnableTaskbarWindowMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Camera|Taskbar", meta = (EditCondition = "bEnableTaskbarWindowMode", ClampMin = "180", ClampMax = "600"))
	int32 TaskbarWindowHeight = 320;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Camera|Taskbar", meta = (EditCondition = "bEnableTaskbarWindowMode"))
	bool bTaskbarWindowAlwaysOnTop = true;

	/** Idle-game recovery delay after the player's death animation begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat|Recovery", meta = (ClampMin = "0.1", Units = "s"))
	float AutoReviveDelay = 3.0f;

	/** Prevents an immediate chain death while the revived player resumes attacking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat|Recovery", meta = (ClampMin = "0.0", Units = "s"))
	float ReviveInvulnerabilityDuration = 2.0f;

	/** Optional custom damage type for later elemental/skill expansion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Combat")
	TSubclassOf<UDamageType> DamageTypeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Progression", meta = (ClampMin = "0"))
	int32 StartingCultivation = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Progression", meta = (ClampMin = "0"))
	int32 StartingGold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Offline", meta = (ClampMin = "0"))
	int32 MinimumOfflineSeconds = 60;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Offline", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float MaximumOfflineHours = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Offline", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OfflineCultivationEfficiency = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Offline", meta = (ClampMin = "0.0"))
	float OfflineSpiritStonesPerMinute = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Offline", meta = (ClampMin = "1.0", Units = "s"))
	float OfflineEquipmentIntervalSeconds = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Offline", meta = (ClampMin = "0"))
	int32 MaximumOfflineEquipmentCount = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Offline", meta = (ClampMin = "1.0", Units = "s"))
	float OfflineMaterialIntervalSeconds = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Offline", meta = (ClampMin = "0"))
	int32 MaximumOfflineMaterialBundles = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Equipment", meta = (ClampMin = "1"))
	int32 InventoryCapacity = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Equipment")
	bool bAutoEquipNewItems = true;

	/** Fixed calendar offset used for daily shop refreshes. 480 means China Standard Time (UTC+8). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immortal Path|Shop", meta = (ClampMin = "-720", ClampMax = "840"))
	int32 ShopUtcOffsetMinutes = 480;

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

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Attributes", meta = (DisplayName = "On Player Auto Revived"))
	void BP_OnPlayerAutoRevived();

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Progression", meta = (DisplayName = "On Rewards Changed"))
	void BP_OnRewardsChanged(int32 NewCultivation, int32 NewGold, int32 CultivationGained, int32 GoldGained);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Cultivation", meta = (DisplayName = "On Cultivation Breakthrough"))
	void BP_OnCultivationBreakthrough(
		const FText& PreviousRealmName,
		const FText& NewRealmName,
		EImmortalCultivationRealm NewRealm,
		int32 NewMinorStage);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Offline", meta = (DisplayName = "On Offline Rewards Claimed"))
	void BP_OnOfflineRewardsClaimed(
		int64 RewardedSeconds,
		int32 CultivationGained,
		int32 SpiritStonesGained,
		int32 EquipmentGained,
		bool bCappedByMaximum);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Progression", meta = (DisplayName = "On Equipment Picked Up"))
	void BP_OnEquipmentPickedUp(int32 NewEquipmentCount, int32 AmountPickedUp);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Equipment", meta = (DisplayName = "On Inventory Changed"))
	void BP_OnInventoryChanged(int32 InventoryCount, int32 Capacity);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Materials", meta = (DisplayName = "On Material Inventory Changed"))
	void BP_OnMaterialInventoryChanged(FName MaterialId, int32 NewQuantity, int32 AmountAdded);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Offline", meta = (DisplayName = "On Offline Materials Granted"))
	void BP_OnOfflineMaterialsGranted(int32 MaterialCount);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Alchemy", meta = (DisplayName = "On Alchemy Completed"))
	void BP_OnAlchemyCompleted(FName RecipeId, EImmortalAlchemyOutcome Outcome, const FText& Message);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Alchemy", meta = (DisplayName = "On Pill Used"))
	void BP_OnPillUsed(FName PillId, EImmortalPillQuality Quality, const FText& EffectText);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Crafting", meta = (DisplayName = "On Crafting Completed"))
	void BP_OnCraftingCompleted(const FImmortalCraftingResult& Result);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Artifact", meta = (DisplayName = "On Artifact Changed"))
	void BP_OnArtifactChanged(const FImmortalArtifactItem& Artifact, bool bEquipped);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Artifact", meta = (DisplayName = "On Artifact Skill Triggered"))
	void BP_OnArtifactSkillTriggered(FName ArtifactId, EImmortalArtifactActiveEffect Effect, AActor* Target, float Magnitude);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Technique", meta = (DisplayName = "On Technique Changed"))
	void BP_OnTechniqueChanged(const FImmortalTechniqueProgress& Technique, bool bEquipped);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Technique", meta = (DisplayName = "On Technique Skill Triggered"))
	void BP_OnTechniqueSkillTriggered(FName TechniqueId, EImmortalTechniqueActiveEffect Effect, bool bUltimate, AActor* Target, float AppliedValue);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Character Build", meta = (DisplayName = "On Spirit Root Awakened"))
	void BP_OnSpiritRootAwakened(const FImmortalSpiritRootState& SpiritRoot);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Character Build", meta = (DisplayName = "On Cultivation Path Changed"))
	void BP_OnCultivationPathChanged(const FImmortalCultivationPathState& PathState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Character Build", meta = (DisplayName = "On Cultivation Path Skill Triggered"))
	void BP_OnCultivationPathSkillTriggered(EImmortalCultivationPath Path, EImmortalPathSkillEffect Effect, AActor* Target, float AppliedValue);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Shop", meta = (DisplayName = "On Shop Transaction"))
	void BP_OnShopTransaction(const FImmortalShopTransactionResult& Result);

	UFUNCTION(BlueprintImplementableEvent, Category = "Immortal Path|Equipment", meta = (DisplayName = "On Equipment Changed"))
	void BP_OnEquipmentChanged(EImmortalEquipmentSlot Slot, const FImmortalEquipmentItem& Item, bool bAutoEquipped, float NewCombatPower);

private:
	void ConfigureCombatCamera();
	void ConfigureTaskbarWindow();
	void ApplyTaskbarWindowPlacement();
	AActor* FindNearestTarget() const;
	bool IsTargetAttackable(const AActor* Target, bool bCheckRange) const;
	FVector GetAutoAttackLocation(const AActor* Target) const;
	void ResolvePendingAttack();
	void AutoRevive();
	void RecalculateEquipmentBonuses();
	void RecalculateArtifactBonuses();
	void RecalculateTechniqueBonuses();
	void RecalculateCharacterPathBonuses();
	void TryTriggerEquippedArtifact(AActor* PrimaryTarget);
	void TryTriggerEquippedTechniques(AActor* PrimaryTarget);
	void TryTriggerCultivationPathSkill(AActor* PrimaryTarget);
	float ExecuteTechniqueSkill(const FImmortalTechniqueProgress& Technique, AActor* PrimaryTarget, bool bUltimate);
	float ExecuteCultivationPathSkill(const FImmortalCultivationPathDefinition& Definition, AActor* PrimaryTarget);
	bool ReconcileEquipmentForPath(EImmortalCultivationPath NewPath, bool bApplyChanges);
	void AwakenSpiritRootIfNeeded();
	bool AddItemToInventory(const FImmortalEquipmentItem& Item);
	int32 AddMaterialInternal(FName MaterialId, int32 Amount);
	int32 AddPillInternal(FName PillId, EImmortalPillQuality Quality, int32 Amount);
	FImmortalAlchemyCraftResult CraftPillInternal(FName RecipeId, TOptional<float> ForcedRoll);
	FImmortalEquipmentItem* FindMutableEquipmentItem(FGuid ItemId, bool& bOutEquipped);
	FImmortalArtifactItem* FindMutableArtifact(FGuid InstanceId);
	FImmortalTechniqueProgress* FindMutableTechnique(FName TechniqueId);
	void ApplyAlchemyCultivationBoost(float Multiplier, float DurationSeconds);
	void ClearAlchemyCultivationBoost();
	void RestoreAlchemyCultivationBoost(float Multiplier, float RemainingSeconds);
	void ConfigureModalWidget(UUserWidget* Widget, bool bOpen);
	void CloseAllModalWidgetsExcept(const UUserWidget* ExceptWidget);
	bool ProcessEquipmentItem(const FImmortalEquipmentItem& Item, bool bShowFeedback, bool bSaveAfter);
	bool RefreshShopForDay(int32 DayKey, int32 QingyunStage, bool bResetManualRefreshes);
	void CheckDailyShopRefresh();
	void RefreshCultivationHud() const;
	void AutosaveCultivationProgress();
	void ApplyOfflineRewards(UImmortalPathSaveGame* SaveGame);

	UFUNCTION()
	void HandleCultivationProgressChanged(int32 NewCultivation, int32 RequiredCultivation, FText FullRealmName);

	UFUNCTION()
	void HandleCultivationBreakthrough(
		FText PreviousRealmName,
		FText NewRealmName,
		EImmortalCultivationRealm NewRealm,
		int32 NewMinorStage);

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentAttackTarget;

	FTimerHandle AutoAttackTimerHandle;
	FTimerHandle AttackWindupTimerHandle;
	FTimerHandle TaskbarWindowTimerHandle;
	FTimerHandle AutoReviveTimerHandle;
	FTimerHandle CultivationAutosaveTimerHandle;
	FTimerHandle CultivationBreakthroughSaveTimerHandle;
	FTimerHandle AlchemyBoostTimerHandle;
	FTimerHandle ShopDailyRefreshTimerHandle;
	bool bAttackPending = false;
	float InvulnerableUntilTime = 0.0f;
	FVector InitialSpawnLocation = FVector::ZeroVector;
	int32 DisplayedStage = 1;
	int32 DisplayedStageKills = 0;
	int32 DisplayedStageRequiredKills = 10;
	bool bDisplayedBossStage = false;
	bool bDisplayedMapCompleted = false;
	bool bHasUnshownOfflineReward = false;

	UPROPERTY(Transient)
	FImmortalOfflineRewardResult LastOfflineRewardResult;

	int64 LastOfflineClaimUtcTicks = 0;
	int64 TotalRewardedOfflineSeconds = 0;
	int32 TotalOfflineClaims = 0;

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
	int32 EquipmentInventoryRevision = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Equipment", meta = (AllowPrivateAccess = "true"))
	TArray<FImmortalEquipmentItem> InventoryItems;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Equipment", meta = (AllowPrivateAccess = "true"))
	TArray<FImmortalEquipmentItem> EquippedItems;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Materials", meta = (AllowPrivateAccess = "true"))
	TArray<FImmortalMaterialStack> MaterialInventory;

	int32 MaterialInventoryRevision = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Alchemy", meta = (AllowPrivateAccess = "true"))
	TArray<FImmortalPillStack> PillInventory;

	int32 PillInventoryRevision = 0;
	float AlchemyCultivationBoostMultiplier = 1.0f;
	float AlchemyBoostEndWorldTime = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Artifact", meta = (AllowPrivateAccess = "true"))
	TArray<FImmortalArtifactItem> ArtifactInventory;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Artifact", meta = (AllowPrivateAccess = "true"))
	FGuid EquippedArtifactInstanceId;

	int32 ArtifactInventoryRevision = 0;
	int32 ArtifactAttackCounter = 0;
	float ArtifactShield = 0.0f;
	float ArtifactAttackMultiplier = 1.0f;
	float ArtifactDefenseMultiplier = 1.0f;
	float ArtifactHealthMultiplier = 1.0f;
	float ArtifactAttackSpeedBonus = 0.0f;
	float ArtifactCriticalChanceBonus = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Technique", meta = (AllowPrivateAccess = "true"))
	TArray<FImmortalTechniqueProgress> TechniqueLibrary;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Technique", meta = (AllowPrivateAccess = "true"))
	TArray<FName> EquippedTechniqueIds;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Technique", meta = (AllowPrivateAccess = "true"))
	int32 TechniqueInsightPoints = 0;

	int32 TechniqueRevision = 0;
	TMap<FName, int32> TechniqueAttackCounters;
	TMap<FName, int32> TechniqueActiveCounters;
	float TechniqueShield = 0.0f;
	float TechniqueAttackMultiplier = 1.0f;
	float TechniqueDefenseMultiplier = 1.0f;
	float TechniqueHealthMultiplier = 1.0f;
	float TechniqueAttackSpeedBonus = 0.0f;
	float TechniqueCriticalChanceBonus = 0.0f;
	float TechniqueCultivationRateMultiplier = 1.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Character Build", meta = (AllowPrivateAccess = "true"))
	FImmortalSpiritRootState SpiritRootState;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Character Build", meta = (AllowPrivateAccess = "true"))
	FImmortalCultivationPathState CultivationPathState;

	int32 CharacterBuildRevision = 0;
	int32 CultivationPathAttackCounter = 0;
	float CultivationPathShield = 0.0f;
	float CharacterPathAttackMultiplier = 1.0f;
	float CharacterPathDefenseMultiplier = 1.0f;
	float CharacterPathHealthMultiplier = 1.0f;
	float CharacterPathManaMultiplier = 1.0f;
	float CharacterPathAttackSpeedBonus = 0.0f;
	float CharacterPathCriticalChanceBonus = 0.0f;
	float CharacterPathDamageReduction = 0.0f;
	float CharacterPathCultivationRateMultiplier = 1.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Shop", meta = (AllowPrivateAccess = "true"))
	FImmortalShopState ShopState;

	int32 ShopRevision = 0;

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

	UPROPERTY(Transient)
	TObjectPtr<UImmortalAlchemyWidget> PlayerAlchemyWidget;

	UPROPERTY(Transient)
	TObjectPtr<UImmortalCraftingWidget> PlayerCraftingWidget;

	UPROPERTY(Transient)
	TObjectPtr<UImmortalArtifactWidget> PlayerArtifactWidget;

	UPROPERTY(Transient)
	TObjectPtr<UImmortalTechniqueWidget> PlayerTechniqueWidget;

	UPROPERTY(Transient)
	TObjectPtr<UImmortalCharacterBuildWidget> PlayerCharacterBuildWidget;

	UPROPERTY(Transient)
	TObjectPtr<UImmortalShopWidget> PlayerShopWidget;

	UPROPERTY(Transient)
	TObjectPtr<UImmortalCombatFeedbackWidget> CombatFeedbackWidget;

	bool bInventoryOpen = false;
	bool bAlchemyOpen = false;
	bool bCraftingOpen = false;
	bool bArtifactOpen = false;
	bool bTechniqueOpen = false;
	bool bCharacterBuildOpen = false;
	bool bShopOpen = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Immortal Path|Attributes", meta = (AllowPrivateAccess = "true"))
	bool bDead = false;
};
