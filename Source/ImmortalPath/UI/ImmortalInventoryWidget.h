// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Alchemy/ImmortalAlchemyTypes.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalInventoryWidget.generated.h"

class AImmortalPlayerCharacter;
class UButton;
class UTextBlock;
class UUniformGridPanel;
enum class EImmortalEquipmentSlot : uint8;

/** Native 900x600 equipment and backpack screen. */
UCLASS()
class IMMORTALPATH_API UImmortalInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void RefreshFromPlayer();
	void HandleSlotSelected(const FGuid& ItemId);
	void HandleMaterialSelected(FName MaterialId);
	void HandlePillSelected(FName PillId, EImmortalPillQuality Quality);
	void ShowMaterialTab();
	void ShowPillTab();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RebuildEquipmentSlots();
	void RebuildBackpackSlots();
	void RefreshDetails();
	void RefreshTabAppearance();
	void RefreshMaterialOverview();
	void AddEquipmentSlot(EImmortalEquipmentSlot EquipmentSlot, int32 Column, int32 Row);

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleEquipmentTabClicked();

	UFUNCTION()
	void HandleMaterialTabClicked();

	UFUNCTION()
	void HandlePillTabClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> EquipmentGrid;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> BackpackGrid;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EquipmentTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MaterialOverviewText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EquipmentTabText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MaterialTabText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PillTabText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CombatPowerText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BackpackCountText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ItemDetailsText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ComparisonText;

	FGuid SelectedItemId;
	FName SelectedMaterialId = NAME_None;
	FName SelectedPillId = NAME_None;
	EImmortalPillQuality SelectedPillQuality = EImmortalPillQuality::Ordinary;
	bool bShowingMaterials = false;
	bool bShowingPills = false;
	int32 LastEquipmentDropCount = INDEX_NONE;
	int32 LastInventoryCount = INDEX_NONE;
	int32 LastMaterialRevision = INDEX_NONE;
	int32 LastPillRevision = INDEX_NONE;
	float LastCombatPower = -1.0f;
};
