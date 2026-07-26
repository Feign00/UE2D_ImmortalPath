// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Inventory/ImmortalInventoryTypes.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalInventoryWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;
class UUniformGridPanel;
enum class EImmortalEquipmentSlot : uint8;

/** Native 1600x300 TBH backpack with five categories and safe equipment management. */
UCLASS()
class IMMORTALPATH_API UImmortalInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void ResetTransientInteraction();
	void RefreshFromPlayer();
	void HandleSlotSelected(const FGuid& ItemId);
	void HandleMaterialSelected(FName MaterialId);
	void HandlePillSelected(FName PillId, EImmortalPillQuality Quality);
	void HandleArtifactSelected(const FGuid& InstanceId);
	void HandleQuestItemSelected(FName QuestItemId);
	void ShowCategory(EImmortalInventoryCategory Category);
	void ShowMaterialTab();
	void ShowPillTab();
	void ShowArtifactTab();
	void ShowQuestItemTab();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	enum class EPendingAction : uint8
	{
		None,
		SellSelected,
		DismantleSelected,
		BatchSell,
		BatchDismantle
	};

	void RebuildEquipmentSlots();
	void RebuildBackpackSlots();
	void RefreshDetails();
	void RefreshTabAppearance();
	void RefreshCategoryOverview();
	void RefreshActionState();
	void AddEquipmentSlot(EImmortalEquipmentSlot EquipmentSlot, int32 Column, int32 Row);
	void AddArtifactEquipmentSlot(int32 Column, int32 Row);
	void SetOperationMessage(const FText& Message, bool bSuccess = false);
	void ResetPendingAction();

	UFUNCTION() void HandleCloseClicked();
	UFUNCTION() void HandleEquipmentTabClicked();
	UFUNCTION() void HandleMaterialTabClicked();
	UFUNCTION() void HandlePillTabClicked();
	UFUNCTION() void HandleArtifactTabClicked();
	UFUNCTION() void HandleQuestItemTabClicked();
	UFUNCTION() void HandleOrganizeClicked();
	UFUNCTION() void HandleLockClicked();
	UFUNCTION() void HandleMaximumQualityClicked();
	UFUNCTION() void HandleSellSelectedClicked();
	UFUNCTION() void HandleDismantleSelectedClicked();
	UFUNCTION() void HandleBatchSellClicked();
	UFUNCTION() void HandleBatchDismantleClicked();

	UPROPERTY(Transient) TWeakObjectPtr<AImmortalPlayerCharacter> Player;
	UPROPERTY(Transient) TObjectPtr<UUniformGridPanel> EquipmentGrid;
	UPROPERTY(Transient) TObjectPtr<UUniformGridPanel> BackpackGrid;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EquipmentTitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CategoryOverviewText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EquipmentTabText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MaterialTabText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PillTabText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ArtifactTabText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> QuestItemTabText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CombatPowerText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BackpackCountText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ItemNameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ItemDetailsText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ComparisonText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OperationMessageText;
	UPROPERTY(Transient) TObjectPtr<UButton> LockButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LockButtonText;
	UPROPERTY(Transient) TObjectPtr<UButton> SellSelectedButton;
	UPROPERTY(Transient) TObjectPtr<UButton> DismantleSelectedButton;
	UPROPERTY(Transient) TObjectPtr<UButton> MaximumQualityButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MaximumQualityButtonText;
	UPROPERTY(Transient) TObjectPtr<UButton> BatchSellButton;
	UPROPERTY(Transient) TObjectPtr<UButton> BatchDismantleButton;

	EImmortalInventoryCategory ActiveCategory = EImmortalInventoryCategory::Equipment;
	EImmortalEquipmentQuality MaximumBulkQuality = EImmortalEquipmentQuality::Common;
	EPendingAction PendingAction = EPendingAction::None;
	FGuid SelectedItemId;
	FName SelectedMaterialId = NAME_None;
	FName SelectedPillId = NAME_None;
	EImmortalPillQuality SelectedPillQuality = EImmortalPillQuality::Ordinary;
	FGuid SelectedArtifactInstanceId;
	FName SelectedQuestItemId = NAME_None;
	int32 LastEquipmentDropCount = INDEX_NONE;
	int32 LastEquipmentRevision = INDEX_NONE;
	int32 LastMaterialRevision = INDEX_NONE;
	int32 LastPillRevision = INDEX_NONE;
	int32 LastArtifactRevision = INDEX_NONE;
	int32 LastQuestItemRevision = INDEX_NONE;
	int32 LastGold = INDEX_NONE;
	float LastCombatPower = -1.0f;
};
