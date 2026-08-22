// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalCombatFeedbackWidget.h"

#include "../Artifacts/ImmortalArtifactTypes.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

namespace
{
	void SetFeedbackFont(UTextBlock* Text, const int32 Size, const FLinearColor& Color)
	{
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(2.0f, 2.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
		Text->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}

	FString FormatOfflineDuration(const int64 TotalSeconds)
	{
		const int64 SafeSeconds = FMath::Max<int64>(TotalSeconds, 0);
		const int64 Hours = SafeSeconds / 3600;
		const int64 Minutes = (SafeSeconds % 3600) / 60;
		const int64 Seconds = SafeSeconds % 60;
		return Hours > 0
			? FString::Printf(TEXT("%lld小时%lld分%lld秒"), Hours, Minutes, Seconds)
			: FString::Printf(TEXT("%lld分%lld秒"), Minutes, Seconds);
	}
}

void UImmortalCombatFeedbackWidget::InitializeForPlayer(AImmortalPlayerCharacter* InPlayer)
{
	Player = InPlayer;
}

void UImmortalCombatFeedbackWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CombatFeedbackCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	StageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StageProgress"));
	SetFeedbackFont(StageText, 25, FLinearColor(0.98f, 0.82f, 0.38f, 1.0f));
	if (UCanvasPanelSlot* StageSlot = RootCanvas->AddChildToCanvas(StageText))
	{
		StageSlot->SetAnchors(FAnchors(0.5f, 0.0f));
		StageSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		StageSlot->SetPosition(FVector2D(0.0f, 20.0f));
		StageSlot->SetSize(FVector2D(600.0f, 38.0f));
	}

	CultivationText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CultivationProgress"));
	SetFeedbackFont(CultivationText, 17, FLinearColor(0.5f, 0.9f, 1.0f, 1.0f));
	if (UCanvasPanelSlot* CultivationSlot = RootCanvas->AddChildToCanvas(CultivationText))
	{
		CultivationSlot->SetAnchors(FAnchors(0.5f, 0.0f));
		CultivationSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		CultivationSlot->SetPosition(FVector2D(0.0f, 55.0f));
		CultivationSlot->SetSize(FVector2D(440.0f, 30.0f));
	}

	BossAnnouncementPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BossAnnouncementPanel"));
	BossAnnouncementPanel->SetBrushColor(FLinearColor(0.08f, 0.015f, 0.015f, 0.90f));
	BossAnnouncementPanel->SetPadding(FMargin(18.0f, 7.0f));
	BossAnnouncementPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* BossSlot = RootCanvas->AddChildToCanvas(BossAnnouncementPanel))
	{
		BossSlot->SetAnchors(FAnchors(0.5f, 0.0f));
		BossSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		BossSlot->SetPosition(FVector2D(0.0f, 84.0f));
		BossSlot->SetSize(FVector2D(620.0f, 48.0f));
	}

	BossAnnouncementText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BossAnnouncementText"));
	SetFeedbackFont(BossAnnouncementText, 22, FLinearColor(1.0f, 0.35f, 0.15f, 1.0f));
	BossAnnouncementPanel->AddChild(BossAnnouncementText);

	OfflineRewardPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OfflineRewardPanel"));
	OfflineRewardPanel->SetBrushColor(FLinearColor(0.025f, 0.055f, 0.075f, 0.96f));
	OfflineRewardPanel->SetPadding(FMargin(24.0f, 14.0f));
	OfflineRewardPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* OfflineSlot = RootCanvas->AddChildToCanvas(OfflineRewardPanel))
	{
		OfflineSlot->SetAnchors(FAnchors(0.5f, 0.0f));
		OfflineSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		OfflineSlot->SetPosition(FVector2D(0.0f, 135.0f));
		OfflineSlot->SetSize(FVector2D(720.0f, 142.0f));
	}

	OfflineRewardText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("OfflineRewardText"));
	SetFeedbackFont(OfflineRewardText, 20, FLinearColor(0.65f, 0.95f, 1.0f, 1.0f));
	OfflineRewardText->SetAutoWrapText(true);
	OfflineRewardPanel->AddChild(OfflineRewardText);

	PickupPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EquipmentPickupPanel"));
	PickupPanel->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.06f, 0.92f));
	PickupPanel->SetPadding(FMargin(14.0f, 10.0f));
	PickupPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* PickupSlot = RootCanvas->AddChildToCanvas(PickupPanel))
	{
		PickupSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		PickupSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		PickupSlot->SetPosition(FVector2D(0.0f, -86.0f));
		PickupSlot->SetSize(FVector2D(520.0f, 58.0f));
	}

	PickupText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EquipmentPickupText"));
	SetFeedbackFont(PickupText, 19, FLinearColor::White);
	PickupPanel->AddChild(PickupText);
}

void UImmortalCombatFeedbackWidget::ShowDamage(
	const FVector& WorldLocation,
	const float Damage,
	const bool bCritical,
	const bool bIncoming)
{
	if (Damage <= 0.0f) return;
	const FString Prefix = bCritical ? TEXT("暴击 ") : (bIncoming ? TEXT("-") : TEXT(""));
	const FLinearColor Color = bIncoming
		? FLinearColor(1.0f, 0.25f, 0.2f, 1.0f)
		: (bCritical ? FLinearColor(1.0f, 0.75f, 0.1f, 1.0f) : FLinearColor::White);
	AddFloatingText(FText::FromString(FString::Printf(TEXT("%s%.0f"), *Prefix, Damage)), WorldLocation, Color, bCritical ? 30 : 24, 1.15f);
}

void UImmortalCombatFeedbackWidget::ShowRewards(const FVector& WorldLocation, const int32 Cultivation, const int32 Gold)
{
	FString RewardString;
	if (Cultivation > 0) RewardString += FString::Printf(TEXT("修为 +%d"), Cultivation);
	if (Gold > 0) RewardString += FString::Printf(TEXT("%s灵石 +%d"), RewardString.IsEmpty() ? TEXT("") : TEXT("  "), Gold);
	if (!RewardString.IsEmpty())
	{
		AddFloatingText(FText::FromString(RewardString), WorldLocation + FVector(0.0f, 0.0f, 45.0f), FLinearColor(0.35f, 1.0f, 0.65f, 1.0f), 19, 1.8f);
	}
}

void UImmortalCombatFeedbackWidget::ShowEquipmentPickup(
	const FText& ItemName,
	const FLinearColor& QualityColor,
	const bool bAutoEquipped)
{
	if (!PickupPanel || !PickupText) return;
	PickupText->SetText(FText::FromString(FString::Printf(
		TEXT("获得装备：%s  %s"), *ItemName.ToString(), bAutoEquipped ? TEXT("[已自动装备]") : TEXT("[已放入背包]"))));
	PickupText->SetColorAndOpacity(FSlateColor(QualityColor));
	PickupPanel->SetRenderOpacity(1.0f);
	PickupPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PickupRemainingTime = 5.0f;
	UE_LOG(LogTemp, Display, TEXT("Equipment pickup message shown for 5 seconds: %s"), *ItemName.ToString());
}

void UImmortalCombatFeedbackWidget::ShowMaterialPickup(
	const FText& MaterialName,
	const FLinearColor& MaterialColor,
	const int32 Quantity)
{
	if (!PickupPanel || !PickupText) return;
	PickupText->SetText(FText::FromString(FString::Printf(
		TEXT("获得材料：%s ×%d  [已堆叠到材料背包]"),
		*MaterialName.ToString(), FMath::Max(Quantity, 1))));
	PickupText->SetColorAndOpacity(FSlateColor(MaterialColor));
	PickupPanel->SetRenderOpacity(1.0f);
	PickupPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PickupRemainingTime = 5.0f;
	UE_LOG(LogTemp, Display, TEXT("Material pickup message shown for 5 seconds: %s x%d"),
		*MaterialName.ToString(), FMath::Max(Quantity, 1));
}

void UImmortalCombatFeedbackWidget::SetStageProgress(
	const FText& MapDisplayName,
	const int32 MaximumStage,
	const int32 Stage,
	const int32 Kills,
	const int32 RequiredKills,
	const bool bBossStage,
	const bool bMapCompleted)
{
	if (StageText)
	{
		const int32 SafeMaximumStage = FMath::Max(MaximumStage, 1);
		const int32 SafeStage = FMath::Clamp(Stage, 1, SafeMaximumStage);
		const FString SafeMapName = MapDisplayName.IsEmpty()
			? TEXT("历练地图")
			: MapDisplayName.ToString();
		if (bMapCompleted)
		{
			StageText->SetText(FText::FromString(FString::Printf(
				TEXT("%s  第 %d 关    已通关"), *SafeMapName, SafeMaximumStage)));
			StageText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.78f, 0.18f, 1.0f)));
		}
		else if (bBossStage)
		{
			StageText->SetText(FText::FromString(FString::Printf(
				TEXT("%s  第 %d / %d 关    [守关 BOSS]"), *SafeMapName, SafeStage, SafeMaximumStage)));
			StageText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.30f, 0.15f, 1.0f)));
		}
		else
		{
			StageText->SetText(FText::FromString(FString::Printf(
				TEXT("%s  第 %d / %d 关    %d / %d"), *SafeMapName, SafeStage, SafeMaximumStage,
				FMath::Max(Kills, 0), FMath::Max(RequiredKills, 1))));
			StageText->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.82f, 0.38f, 1.0f)));
		}
	}
}

void UImmortalCombatFeedbackWidget::SetStageProgress(
	const int32 Stage,
	const int32 Kills,
	const int32 RequiredKills,
	const bool bBossStage,
	const bool bMapCompleted)
{
	SetStageProgress(FText::FromString(TEXT("青云山")), 999, Stage, Kills, RequiredKills, bBossStage, bMapCompleted);
}

void UImmortalCombatFeedbackWidget::ShowBossAnnouncement(
	const FText& Message,
	const FLinearColor& Color,
	const float Duration)
{
	if (!BossAnnouncementPanel || !BossAnnouncementText)
	{
		return;
	}
	BossAnnouncementText->SetText(Message);
	BossAnnouncementText->SetColorAndOpacity(FSlateColor(Color));
	BossAnnouncementPanel->SetBrushColor(FLinearColor(0.08f, 0.015f, 0.015f, 0.90f));
	BossAnnouncementPanel->SetRenderOpacity(1.0f);
	BossAnnouncementPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BossAnnouncementRemainingTime = FMath::Max(Duration, 0.1f);
	UE_LOG(LogTemp, Display, TEXT("Boss announcement: %s"), *Message.ToString());
}

void UImmortalCombatFeedbackWidget::SetWorldBossProgress(
	const FText& BossName,
	const int32 Phase,
	const float CurrentHealth,
	const float MaximumHealth,
	const float RemainingSeconds)
{
	if (!StageText)
	{
		return;
	}
	const int32 Minutes = FMath::FloorToInt(FMath::Max(RemainingSeconds, 0.0f) / 60.0f);
	const int32 Seconds = FMath::FloorToInt(FMath::Max(RemainingSeconds, 0.0f)) % 60;
	const float HealthPercent = MaximumHealth > 0.0f
		? FMath::Clamp(CurrentHealth / MaximumHealth, 0.0f, 1.0f) * 100.0f
		: 0.0f;
	StageText->SetText(FText::FromString(FString::Printf(
		TEXT("世界妖王 · %s    阶段 %d/3    生命 %.0f%%    剩余 %02d:%02d"),
		*BossName.ToString(), FMath::Clamp(Phase, 1, 3),
		HealthPercent, Minutes, Seconds)));
	StageText->SetColorAndOpacity(FSlateColor(
		Phase >= 3
			? FLinearColor(1.0f, 0.18f, 0.10f, 1.0f)
			: FLinearColor(1.0f, 0.62f, 0.18f, 1.0f)));
}

void UImmortalCombatFeedbackWidget::ShowWorldBossRewardSummary(
	const int32 EquipmentCount,
	const int32 SpiritStones,
	const TArray<FImmortalMaterialStack>& Materials,
	const FName ArtifactId)
{
	if (!OfflineRewardPanel || !OfflineRewardText)
	{
		return;
	}
	TArray<FString> MaterialParts;
	for (const FImmortalMaterialStack& Stack : Materials)
	{
		FImmortalMaterialDefinition Definition;
		const FString Name = UImmortalMaterialLibrary::GetMaterialDefinition(
			Stack.MaterialId, Definition)
			? Definition.DisplayName.ToString()
			: Stack.MaterialId.ToString();
		MaterialParts.Add(FString::Printf(TEXT("%s×%d"), *Name, Stack.Quantity));
	}
	FString ArtifactPart;
	if (!ArtifactId.IsNone())
	{
		FImmortalArtifactDefinition Definition;
		ArtifactPart = UImmortalArtifactLibrary::GetArtifactDefinition(
			ArtifactId, Definition)
			? FString::Printf(TEXT("\n首通法宝：%s"), *Definition.DisplayName.ToString())
			: FString::Printf(TEXT("\n首通法宝：%s"), *ArtifactId.ToString());
	}
	OfflineRewardText->SetText(FText::FromString(FString::Printf(
		TEXT("世界妖王独立掉落已自动领取并保存\n高阶装备 ×%d    灵石 +%d    %s%s\n本次奖励不包含修为"),
		FMath::Max(EquipmentCount, 0),
		FMath::Max(SpiritStones, 0),
		*FString::Join(MaterialParts, TEXT("、")),
		*ArtifactPart)));
	OfflineRewardText->SetColorAndOpacity(
		FSlateColor(FLinearColor(1.0f, 0.78f, 0.26f, 1.0f)));
	OfflineRewardPanel->SetBrushColor(FLinearColor(0.08f, 0.035f, 0.012f, 0.96f));
	OfflineRewardPanel->SetRenderOpacity(1.0f);
	OfflineRewardPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	OfflineRewardRemainingTime = 5.0f;
}

void UImmortalCombatFeedbackWidget::SetEndlessDungeonProgress(
	const int32 Floor,
	const int32 Kills,
	const int32 RequiredKills,
	const bool bElite,
	const bool bBoss,
	const int32 BossPhase,
	const float CurrentHealth,
	const float MaximumHealth)
{
	if (!StageText)
	{
		return;
	}
	if (bBoss)
	{
		const float HealthPercent = MaximumHealth > 0.0f
			? FMath::Clamp(
				CurrentHealth / MaximumHealth,
				0.0f,
				1.0f) * 100.0f
			: 0.0f;
		StageText->SetText(FText::FromString(FString::Printf(
			TEXT("\u65E0\u5C3D\u79D8\u5883 \u00B7 \u7B2C %d \u5C42\u9996\u9886    \u9636\u6BB5 %d/3    \u751F\u547D %.0f%%"),
			FMath::Max(Floor, 1),
			FMath::Clamp(BossPhase, 1, 3),
			HealthPercent)));
		StageText->SetColorAndOpacity(FSlateColor(
			BossPhase >= 3
				? FLinearColor(1.0f, 0.16f, 0.10f, 1.0f)
				: FLinearColor(1.0f, 0.42f, 0.18f, 1.0f)));
		return;
	}

	const int32 SafeFloor = FMath::Max(Floor, 1);
	const int32 SafeRequiredKills = FMath::Max(RequiredKills, 1);
	const int32 SafeKills = FMath::Clamp(
		Kills, 0, SafeRequiredKills);
	const FString ProgressText = bElite
		? FString::Printf(
			TEXT("\u65E0\u5C3D\u79D8\u5883 \u00B7 \u7B2C %d \u5C42\u7CBE\u82F1    %d / %d"),
			SafeFloor,
			SafeKills,
			SafeRequiredKills)
		: FString::Printf(
			TEXT("\u65E0\u5C3D\u79D8\u5883 \u00B7 \u7B2C %d \u5C42    %d / %d"),
			SafeFloor,
			SafeKills,
			SafeRequiredKills);
	StageText->SetText(FText::FromString(ProgressText));
	StageText->SetColorAndOpacity(FSlateColor(
		bElite
			? FLinearColor(0.82f, 0.52f, 1.0f, 1.0f)
			: FLinearColor(0.42f, 0.86f, 1.0f, 1.0f)));
}

void UImmortalCombatFeedbackWidget::ShowEndlessDungeonRewardSummary(
	const int32 ClearedFloor,
	const int32 EquipmentCount,
	const int32 SpiritStones,
	const TArray<FImmortalMaterialStack>& Materials)
{
	if (!OfflineRewardPanel || !OfflineRewardText)
	{
		return;
	}
	TArray<FString> MaterialParts;
	for (const FImmortalMaterialStack& Stack : Materials)
	{
		FImmortalMaterialDefinition Definition;
		const FString Name =
			UImmortalMaterialLibrary::GetMaterialDefinition(
				Stack.MaterialId,
				Definition)
				? Definition.DisplayName.ToString()
				: Stack.MaterialId.ToString();
		MaterialParts.Add(FString::Printf(
			TEXT("%s\u00D7%d"),
			*Name,
			Stack.Quantity));
	}
	OfflineRewardText->SetText(FText::FromString(FString::Printf(
		TEXT("\u65E0\u5C3D\u79D8\u5883\u7B2C %d \u5C42\u5956\u52B1\u5DF2\u81EA\u52A8\u9886\u53D6\u5E76\u4FDD\u5B58\n\u88C5\u5907 \u00D7%d    \u7075\u77F3 +%d    %s\n\u672C\u6B21\u5956\u52B1\u4E0D\u5305\u542B\u4FEE\u4E3A"),
		FMath::Max(ClearedFloor, 1),
		FMath::Max(EquipmentCount, 0),
		FMath::Max(SpiritStones, 0),
		*FString::Join(MaterialParts, TEXT("  ")))));
	OfflineRewardText->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.48f, 0.92f, 1.0f, 1.0f)));
	OfflineRewardPanel->SetBrushColor(
		FLinearColor(0.025f, 0.045f, 0.085f, 0.97f));
	OfflineRewardPanel->SetRenderOpacity(1.0f);
	OfflineRewardPanel->SetVisibility(
		ESlateVisibility::SelfHitTestInvisible);
	OfflineRewardRemainingTime = 5.0f;
}

void UImmortalCombatFeedbackWidget::SetCultivationProgress(
	const FText& RealmName,
	const int32 Current,
	const int32 Required,
	const float PerSecond,
	const bool bAscended)
{
	if (CultivationText)
	{
		CultivationText->SetText(bAscended
			? FText::FromString(FString::Printf(TEXT("境界：%s    已完成修行"), *RealmName.ToString()))
			: FText::FromString(FString::Printf(
				TEXT("境界：%s    修为 %d / %d    +%.1f/秒"),
				*RealmName.ToString(), FMath::Max(Current, 0), FMath::Max(Required, 1), FMath::Max(PerSecond, 0.0f))));
	}
}

void UImmortalCombatFeedbackWidget::ShowCultivationBreakthrough(
	const FText& NewRealmName,
	const float TotalHealthBonus,
	const float TotalManaBonus,
	const float TotalAttackBonus,
	const float TotalDefenseBonus)
{
	if (!BossAnnouncementPanel || !BossAnnouncementText)
	{
		return;
	}
	BossAnnouncementText->SetText(FText::FromString(FString::Printf(
		TEXT("突破成功：%s    累计 生命+%.0f  灵力+%.0f  攻击+%.1f  防御+%.1f"),
		*NewRealmName.ToString(), TotalHealthBonus, TotalManaBonus, TotalAttackBonus, TotalDefenseBonus)));
	BossAnnouncementText->SetColorAndOpacity(FSlateColor(FLinearColor(0.35f, 1.0f, 0.85f, 1.0f)));
	BossAnnouncementPanel->SetBrushColor(FLinearColor(0.015f, 0.08f, 0.065f, 0.92f));
	BossAnnouncementPanel->SetRenderOpacity(1.0f);
	BossAnnouncementPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BossAnnouncementRemainingTime = 5.0f;
	UE_LOG(LogTemp, Display, TEXT("Cultivation breakthrough announcement: %s"), *NewRealmName.ToString());
}

void UImmortalCombatFeedbackWidget::ShowOfflineRewardSummary(
	const int64 RewardedSeconds,
	const int32 Cultivation,
	const int32 SpiritStones,
	const int32 EquipmentCount,
	const int32 MaterialCount,
	const float MaximumHours,
	const bool bCappedByMaximum)
{
	if (!OfflineRewardPanel || !OfflineRewardText)
	{
		return;
	}
	const FString CapText = bCappedByMaximum
		? FString::Printf(TEXT("（已按 %.0f 小时上限结算）"), FMath::Max(MaximumHours, 0.0f))
		: TEXT("");
	OfflineRewardText->SetText(FText::FromString(FString::Printf(
		TEXT("离线挂机收益  %s%s\n修为 +%d    灵石 +%d    装备 +%d    材料 +%d\n奖励已自动领取并保存，角色继续在线修炼与战斗"),
		*FormatOfflineDuration(RewardedSeconds), *CapText,
		FMath::Max(Cultivation, 0), FMath::Max(SpiritStones, 0), FMath::Max(EquipmentCount, 0),
		FMath::Max(MaterialCount, 0))));
	OfflineRewardPanel->SetRenderOpacity(1.0f);
	OfflineRewardPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	OfflineRewardRemainingTime = 8.0f;
	UE_LOG(LogTemp, Display, TEXT("Offline reward summary shown for 8 seconds"));
}

void UImmortalCombatFeedbackWidget::AddFloatingText(
	const FText& Text,
	const FVector& WorldLocation,
	const FLinearColor& Color,
	const int32 FontSize,
	const float Duration)
{
	if (!RootCanvas || !GetOwningPlayer()) return;
	FVector2D ScreenPosition;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), WorldLocation, ScreenPosition, false)) return;

	UTextBlock* FloatingText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FloatingText->SetText(Text);
	SetFeedbackFont(FloatingText, FontSize, Color);
	if (UCanvasPanelSlot* FloatingCanvasSlot = RootCanvas->AddChildToCanvas(FloatingText))
	{
		FloatingCanvasSlot->SetPosition(ScreenPosition - FVector2D(90.0f, 20.0f));
		FloatingCanvasSlot->SetSize(FVector2D(180.0f, 44.0f));
	}

	FImmortalFloatingTextEntry& Entry = FloatingTexts.AddDefaulted_GetRef();
	Entry.Text = FloatingText;
	Entry.StartPosition = ScreenPosition - FVector2D(90.0f, 20.0f);
	Entry.Duration = FMath::Max(Duration, 0.1f);
}

void UImmortalCombatFeedbackWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	for (int32 Index = FloatingTexts.Num() - 1; Index >= 0; --Index)
	{
		FImmortalFloatingTextEntry& Entry = FloatingTexts[Index];
		Entry.Age += InDeltaTime;
		const float Alpha = FMath::Clamp(Entry.Age / Entry.Duration, 0.0f, 1.0f);
		if (!Entry.Text || Alpha >= 1.0f)
		{
			if (Entry.Text) Entry.Text->RemoveFromParent();
			FloatingTexts.RemoveAt(Index);
			continue;
		}
		Entry.Text->SetRenderOpacity(1.0f - Alpha);
		if (UCanvasPanelSlot* FloatingCanvasSlot = Cast<UCanvasPanelSlot>(Entry.Text->Slot))
		{
			FloatingCanvasSlot->SetPosition(Entry.StartPosition + FVector2D(0.0f, -70.0f * Alpha));
		}
	}

	if (PickupRemainingTime > 0.0f && PickupPanel)
	{
		PickupRemainingTime = FMath::Max(PickupRemainingTime - InDeltaTime, 0.0f);
		PickupPanel->SetRenderOpacity(PickupRemainingTime < 0.5f ? PickupRemainingTime / 0.5f : 1.0f);
		if (PickupRemainingTime <= 0.0f)
		{
			PickupPanel->SetVisibility(ESlateVisibility::Collapsed);
			UE_LOG(LogTemp, Display, TEXT("Pickup message hidden after 5 seconds"));
		}
	}

	if (BossAnnouncementRemainingTime > 0.0f && BossAnnouncementPanel)
	{
		BossAnnouncementRemainingTime = FMath::Max(BossAnnouncementRemainingTime - InDeltaTime, 0.0f);
		BossAnnouncementPanel->SetRenderOpacity(
			BossAnnouncementRemainingTime < 0.4f ? BossAnnouncementRemainingTime / 0.4f : 1.0f);
		if (BossAnnouncementRemainingTime <= 0.0f)
		{
			BossAnnouncementPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (OfflineRewardRemainingTime > 0.0f && OfflineRewardPanel)
	{
		OfflineRewardRemainingTime = FMath::Max(OfflineRewardRemainingTime - InDeltaTime, 0.0f);
		OfflineRewardPanel->SetRenderOpacity(
			OfflineRewardRemainingTime < 0.5f ? OfflineRewardRemainingTime / 0.5f : 1.0f);
		if (OfflineRewardRemainingTime <= 0.0f)
		{
			OfflineRewardPanel->SetVisibility(ESlateVisibility::Collapsed);
			UE_LOG(LogTemp, Display, TEXT("Reward summary hidden after its configured display duration"));
		}
	}
}
