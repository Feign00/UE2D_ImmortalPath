// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ImmortalCombatFeedbackWidget.generated.h"

class AImmortalPlayerCharacter;
class UBorder;
class UCanvasPanel;
class UTextBlock;

USTRUCT()
struct FImmortalFloatingTextEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text = nullptr;

	FVector2D StartPosition = FVector2D::ZeroVector;
	float Age = 0.0f;
	float Duration = 1.0f;
};

/** Full-screen native HUD layer for combat numbers, stage data and pickup messages. */
UCLASS()
class IMMORTALPATH_API UImmortalCombatFeedbackWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(AImmortalPlayerCharacter* InPlayer);
	void ShowDamage(const FVector& WorldLocation, float Damage, bool bCritical, bool bIncoming);
	void ShowRewards(const FVector& WorldLocation, int32 Cultivation, int32 Gold);
	void ShowEquipmentPickup(const FText& ItemName, const FLinearColor& QualityColor, bool bAutoEquipped);
	void ShowMaterialPickup(const FText& MaterialName, const FLinearColor& MaterialColor, int32 Quantity);
	void SetStageProgress(int32 Stage, int32 Kills, int32 RequiredKills, bool bBossStage = false, bool bMapCompleted = false);
	void ShowBossAnnouncement(const FText& Message, const FLinearColor& Color, float Duration = 3.0f);
	void SetCultivationProgress(
		const FText& RealmName,
		int32 Current,
		int32 Required,
		float PerSecond = 0.0f,
		bool bAscended = false);
	void ShowCultivationBreakthrough(
		const FText& NewRealmName,
		float TotalHealthBonus,
		float TotalManaBonus,
		float TotalAttackBonus,
		float TotalDefenseBonus);
	void ShowOfflineRewardSummary(
		int64 RewardedSeconds,
		int32 Cultivation,
		int32 SpiritStones,
		int32 EquipmentCount,
		int32 MaterialCount,
		float MaximumHours,
		bool bCappedByMaximum);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void AddFloatingText(const FText& Text, const FVector& WorldLocation, const FLinearColor& Color, int32 FontSize, float Duration);

	UPROPERTY(Transient)
	TWeakObjectPtr<AImmortalPlayerCharacter> Player;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StageText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CultivationText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> BossAnnouncementPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BossAnnouncementText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> OfflineRewardPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OfflineRewardText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PickupPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PickupText;

	UPROPERTY(Transient)
	TArray<FImmortalFloatingTextEntry> FloatingTexts;

	float PickupRemainingTime = 0.0f;
	float BossAnnouncementRemainingTime = 0.0f;
	float OfflineRewardRemainingTime = 0.0f;
};
