// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Alchemy/ImmortalAlchemyTypes.h"
#include "../Artifacts/ImmortalArtifactTypes.h"
#include "../Inventory/ImmortalInventoryTypes.h"
#include "../Items/ImmortalEquipmentTypes.h"
#include "../Items/ImmortalMaterialTypes.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalInventorySlotWidget.generated.h"

class UButton;
class UImage;
class UImmortalInventoryWidget;
class UTextBlock;
class UTexture2D;

/** One 84x84 item cell with shared atlas art, readable badges and quality outlines. */
UCLASS()
class IMMORTALPATH_API UImmortalInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeSlot(
		UImmortalInventoryWidget* InOwner,
		const FImmortalEquipmentItem& InItem,
		bool bInHasItem,
		bool bInEquipped,
		bool bInSelected,
		EImmortalEquipmentSlot InPlaceholderSlot = EImmortalEquipmentSlot::MAX);

	void InitializeMaterialSlot(
		UImmortalInventoryWidget* InOwner,
		const FImmortalMaterialStack& InStack,
		bool bInSelected);

	void InitializePillSlot(
		UImmortalInventoryWidget* InOwner,
		const FImmortalPillStack& InStack,
		bool bInSelected);

	void InitializeArtifactSlot(
		UImmortalInventoryWidget* InOwner,
		const FImmortalArtifactItem& InItem,
		bool bInEquipped,
		bool bInSelected);

	void InitializeQuestItemSlot(
		UImmortalInventoryWidget* InOwner,
		const FImmortalQuestItemStack& InStack,
		bool bInSelected);

	const FImmortalEquipmentItem& GetItem() const { return Item; }
	bool HasItem() const { return bHasItem; }

protected:
	UPROPERTY(EditDefaultsOnly, Category="Art")
	TSoftObjectPtr<UTexture2D> EquipmentAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_EquipmentAtlas.T_EquipmentAtlas")));

	UPROPERTY(EditDefaultsOnly, Category="Art")
	TSoftObjectPtr<UTexture2D> ForgeAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_ForgeAtlas.T_ForgeAtlas")));

	UPROPERTY(EditDefaultsOnly, Category="Art")
	TSoftObjectPtr<UTexture2D> MaterialAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_MaterialAtlas.T_MaterialAtlas")));

	UPROPERTY(EditDefaultsOnly, Category="Art")
	TSoftObjectPtr<UTexture2D> AlchemyAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/GAME/Asset/DesktopPixelV2/UI/T_AlchemyAtlas.T_AlchemyAtlas")));

	virtual void NativeOnInitialized() override;

private:
	void RefreshAppearance();

	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<UImmortalInventoryWidget> OwnerInventory;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SlotButton;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(Transient)
	TObjectPtr<UImage> QualityFrame;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LevelText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MaterialGlyphText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SlotLabelText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LockGlyphText;

	FImmortalEquipmentItem Item;
	FImmortalMaterialStack MaterialStack;
	FImmortalPillStack PillStack;
	FImmortalArtifactItem ArtifactItem;
	FImmortalQuestItemStack QuestItemStack;
	EImmortalEquipmentSlot PlaceholderSlot = EImmortalEquipmentSlot::MAX;
	bool bMaterialItem = false;
	bool bPillItem = false;
	bool bArtifactItem = false;
	bool bQuestItem = false;
	bool bHasItem = false;
	bool bEquipped = false;
	bool bSelected = false;
};
