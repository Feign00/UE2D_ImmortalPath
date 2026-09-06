#include "ImmortalPlayerCharacter.h"

#include "../UI/ImmortalAlchemyWidget.h"
#include "../UI/ImmortalArtifactWidget.h"
#include "../UI/ImmortalAscensionWidget.h"
#include "../UI/ImmortalCombatFeedbackWidget.h"
#include "../UI/ImmortalCraftingWidget.h"
#include "../UI/ImmortalCharacterBuildWidget.h"
#include "../UI/ImmortalCaveWidget.h"
#include "../UI/ImmortalEndlessDungeonWidget.h"
#include "../UI/ImmortalFarmingWidget.h"
#include "../UI/ImmortalInventoryWidget.h"
#include "../UI/ImmortalManagementWidget.h"
#include "../UI/ImmortalCultivationWidget.h"
#include "../UI/ImmortalMapWidget.h"
#include "../UI/ImmortalPetWidget.h"
#include "../UI/ImmortalPlayerStatusWidget.h"
#include "../UI/ImmortalQuestWidget.h"
#include "../UI/ImmortalSectWidget.h"
#include "../UI/ImmortalSettingsWidget.h"
#include "../UI/ImmortalShopWidget.h"
#include "../UI/ImmortalTechniqueWidget.h"
#include "../UI/ImmortalWorldBossWidget.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

// Management navigation is isolated here; the character retains gameplay authority.
void AImmortalPlayerCharacter::RegisterManagementPages()
{
	if (!PlayerManagementWidget)
	{
		return;
	}

	PlayerManagementWidget->RegisterFeaturePage(
		EImmortalManagementFeature::Cultivation,
		PlayerCultivationWidget);
	PlayerManagementWidget->RegisterFeaturePage(
		EImmortalManagementFeature::Inventory,
		PlayerInventoryWidget);
	PlayerManagementWidget->RegisterFeaturePage(
		EImmortalManagementFeature::Alchemy,
		PlayerAlchemyWidget);
	PlayerManagementWidget->RegisterFeaturePage(
		EImmortalManagementFeature::Crafting,
		PlayerCraftingWidget);
	PlayerManagementWidget->RegisterFeaturePage(
		EImmortalManagementFeature::Artifact,
		PlayerArtifactWidget);
	PlayerManagementWidget->RegisterFeaturePage(
		EImmortalManagementFeature::Technique,
		PlayerTechniqueWidget);
	PlayerManagementWidget->RegisterFeaturePage(
		EImmortalManagementFeature::CharacterBuild,
		PlayerCharacterBuildWidget);
	PlayerManagementWidget->RegisterFeaturePage(
		EImmortalManagementFeature::Shop,
		PlayerShopWidget);
	PlayerManagementWidget->RegisterFeaturePage(
		EImmortalManagementFeature::Map,
		PlayerMapWidget);
	PlayerManagementWidget->RegisterFeaturePage(
		EImmortalManagementFeature::Quest,
		PlayerQuestWidget);
	PlayerManagementWidget->RegisterFeaturePage(
		EImmortalManagementFeature::Cave,
		PlayerCaveWidget);
	PlayerManagementWidget->RegisterFeaturePage(
		EImmortalManagementFeature::Farming,
		PlayerFarmingWidget);
	PlayerManagementWidget->RegisterFeaturePage(
		EImmortalManagementFeature::Sect,
		PlayerSectWidget);
	PlayerManagementWidget->RegisterFeaturePage(
		EImmortalManagementFeature::WorldBoss,
		PlayerWorldBossWidget);
	PlayerManagementWidget->RegisterFeaturePage(
		EImmortalManagementFeature::EndlessDungeon,
		PlayerEndlessDungeonWidget);
	PlayerManagementWidget->RegisterFeaturePage(
		EImmortalManagementFeature::Pet,
		PlayerPetWidget);
	PlayerManagementWidget->RegisterFeaturePage(
		EImmortalManagementFeature::Settings,
		PlayerSettingsWidget);
	PlayerManagementWidget->ShowFeature(
		EImmortalManagementFeature::Home);
}

void AImmortalPlayerCharacter::QueueManagementNotification(
	const FText& Message,
	const FLinearColor& Color,
	const float DurationSeconds)
{
	if (PlayerManagementWidget)
	{
		PlayerManagementWidget->QueueNotification(
			Message,
			Color,
			FMath::Max(DurationSeconds, 0.1f));
	}
}

void AImmortalPlayerCharacter::OpenManagementInterface()
{
	OpenManagementFeature(EImmortalManagementFeature::Home);
}

void AImmortalPlayerCharacter::CloseManagementInterface()
{
	if (bDeathCultivationRecoveryRequired)
	{
		OpenManagementFeature(
			EImmortalManagementFeature::Cultivation);
		QueueManagementNotification(
			FText::FromString(TEXT(
				"历练通道仍处于关闭状态：完成下一次修炼突破后才能重返历练。")),
			FLinearColor(1.0f, 0.58f, 0.32f, 1.0f),
			5.0f);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Return to adventure blocked by death cultivation recovery"));
		return;
	}

	const bool bShouldResumeAdventure =
		bAdventureSuspendedForDeathRecovery;
	if (PlayerManagementWidget)
	{
		PlayerManagementWidget->SetVisibility(
			ESlateVisibility::Collapsed);
		PlayerManagementWidget->ShowFeature(
			EImmortalManagementFeature::Home);
	}
	bManagementInterfaceOpen = false;
	ActiveManagementFeature = EImmortalManagementFeature::Home;
	SetManagementFeatureOpenFlags(EImmortalManagementFeature::Home);
	if (PlayerStatusWidget)
	{
		PlayerStatusWidget->SetVisibility(
			ESlateVisibility::Visible);
	}
	ConfigureModalWidget(nullptr, false);
	if (bShouldResumeAdventure)
	{
		ResumeAdventureAfterDeathRecovery();
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Management interface closed: combatPaused=%s autoAttackActive=%s"),
		UGameplayStatics::IsGamePaused(this) ? TEXT("true") : TEXT("false"),
		GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)
			? TEXT("true")
			: TEXT("false"));
}

void AImmortalPlayerCharacter::OpenManagementFeature(
	const EImmortalManagementFeature Feature)
{
	if (!PlayerManagementWidget)
	{
		return;
	}
	const EImmortalManagementFeature EffectiveFeature =
		bDeathCultivationRecoveryRequired
		&& Feature != EImmortalManagementFeature::Cultivation
			? EImmortalManagementFeature::Cultivation
			: Feature;

	if (bAscensionOpen && PlayerAscensionWidget)
	{
		PlayerAscensionWidget->SetVisibility(
			ESlateVisibility::Collapsed);
	}
	bAscensionOpen = false;
	bManagementInterfaceOpen = true;
	ActiveManagementFeature = EffectiveFeature;
	SetManagementFeatureOpenFlags(EffectiveFeature);

	switch (EffectiveFeature)
	{
	case EImmortalManagementFeature::Cultivation:
		if (PlayerCultivationWidget)
		{
			PlayerCultivationWidget->RefreshFromPlayer();
		}
		break;
	case EImmortalManagementFeature::Inventory:
		if (PlayerInventoryWidget)
		{
			PlayerInventoryWidget->ResetTransientInteraction();
			PlayerInventoryWidget->RefreshFromPlayer();
		}
		break;
	case EImmortalManagementFeature::Alchemy:
		if (PlayerAlchemyWidget) PlayerAlchemyWidget->RefreshFromPlayer();
		break;
	case EImmortalManagementFeature::Crafting:
		if (PlayerCraftingWidget) PlayerCraftingWidget->RefreshFromPlayer();
		break;
	case EImmortalManagementFeature::Artifact:
		if (PlayerArtifactWidget) PlayerArtifactWidget->RefreshFromPlayer();
		break;
	case EImmortalManagementFeature::Technique:
		if (PlayerTechniqueWidget) PlayerTechniqueWidget->RefreshFromPlayer();
		break;
	case EImmortalManagementFeature::CharacterBuild:
		if (PlayerCharacterBuildWidget) PlayerCharacterBuildWidget->RefreshFromPlayer();
		break;
	case EImmortalManagementFeature::Shop:
		EnsureDailyShopRefresh();
		if (PlayerShopWidget) PlayerShopWidget->RefreshFromPlayer();
		break;
	case EImmortalManagementFeature::Map:
		if (PlayerMapWidget) PlayerMapWidget->SelectMap(GetActiveMapId());
		break;
	case EImmortalManagementFeature::Quest:
		EnsureQuestDailyState();
		if (PlayerQuestWidget) PlayerQuestWidget->RefreshFromPlayer();
		break;
	case EImmortalManagementFeature::Cave:
		SettleCaveProduction();
		if (PlayerCaveWidget) PlayerCaveWidget->RefreshFromPlayer();
		break;
	case EImmortalManagementFeature::Farming:
		SettleFarmingGrowth();
		if (PlayerFarmingWidget) PlayerFarmingWidget->RefreshFromPlayer();
		break;
	case EImmortalManagementFeature::Sect:
		EnsureSectDailyState();
		if (PlayerSectWidget) PlayerSectWidget->RefreshFromPlayer();
		break;
	case EImmortalManagementFeature::WorldBoss:
		RetryPendingWorldBossRewards();
		if (PlayerWorldBossWidget) PlayerWorldBossWidget->RefreshFromPlayer();
		break;
	case EImmortalManagementFeature::EndlessDungeon:
		RetryPendingEndlessDungeonRewards();
		if (PlayerEndlessDungeonWidget)
		{
			PlayerEndlessDungeonWidget->RefreshFromPlayer();
		}
		break;
	case EImmortalManagementFeature::Pet:
		if (PlayerPetWidget) PlayerPetWidget->RefreshFromPlayer();
		break;
	case EImmortalManagementFeature::Settings:
		if (PlayerSettingsWidget) PlayerSettingsWidget->RefreshFromPlayer();
		break;
	default:
		break;
	}

	PlayerManagementWidget->ShowFeature(EffectiveFeature);
	PlayerManagementWidget->SetVisibility(ESlateVisibility::Visible);
	if (PlayerStatusWidget)
	{
		PlayerStatusWidget->SetVisibility(
			ESlateVisibility::Collapsed);
	}
	ConfigureModalWidget(PlayerManagementWidget, true);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Management feature opened: page=%d combatPaused=%s autoAttackActive=%s cultivationRate=%.2f"),
		static_cast<int32>(EffectiveFeature),
		UGameplayStatics::IsGamePaused(this) ? TEXT("true") : TEXT("false"),
		GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)
			? TEXT("true")
			: TEXT("false"),
		GetCultivationPerSecond());
	if (EffectiveFeature != Feature)
	{
		QueueManagementNotification(
			FText::FromString(TEXT(
				"历练失败后必须先修炼：完成下一次突破即可解锁其他功能和重返历练。")),
			FLinearColor(1.0f, 0.58f, 0.32f, 1.0f),
			5.0f);
	}
}

void AImmortalPlayerCharacter::ToggleManagementFeature(
	const EImmortalManagementFeature Feature)
{
	if (bManagementInterfaceOpen
		&& ActiveManagementFeature == Feature)
	{
		OpenManagementFeature(EImmortalManagementFeature::Home);
		return;
	}
	OpenManagementFeature(Feature);
}

void AImmortalPlayerCharacter::HandleManagementToggleInput()
{
	if (bAscensionOpen)
	{
		OpenManagementFeature(EImmortalManagementFeature::Cultivation);
	}
	else if (bManagementInterfaceOpen)
	{
		CloseManagementInterface();
	}
	else
	{
		OpenManagementInterface();
	}
}
