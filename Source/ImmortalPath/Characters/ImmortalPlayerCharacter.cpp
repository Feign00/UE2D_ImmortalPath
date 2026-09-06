// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPlayerCharacter.h"

#include "../Combat/AutoAttackTarget.h"
#include "../Save/ImmortalPathSaveGame.h"
#include "../Settings/ImmortalDesktopSettings.h"
#include "../Settings/ImmortalDesktopWindow.h"
#include "../Spawning/ImmortalMonsterSpawner.h"
#include "ImmortalMonsterCharacter.h"
#include "ImmortalPetCharacter.h"
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
#include "../UI/ImmortalDesktopGroundWidget.h"
#include "../UI/ImmortalQuestWidget.h"
#include "../UI/ImmortalSectWidget.h"
#include "../UI/ImmortalSettingsWidget.h"
#include "../UI/ImmortalShopWidget.h"
#include "../UI/ImmortalTechniqueWidget.h"
#include "../UI/ImmortalWorldBossWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/PlatformMisc.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
	bool HaveSameMaterialQuantities(
		const TArray<FImmortalMaterialStack>& Left,
		const TArray<FImmortalMaterialStack>& Right)
	{
		TSet<FName> MaterialIds;
		for (const FImmortalMaterialStack& Stack : Left) if (!Stack.MaterialId.IsNone()) MaterialIds.Add(Stack.MaterialId);
		for (const FImmortalMaterialStack& Stack : Right) if (!Stack.MaterialId.IsNone()) MaterialIds.Add(Stack.MaterialId);
		for (const FName MaterialId : MaterialIds)
		{
			if (UImmortalMaterialLibrary::GetMaterialQuantity(Left, MaterialId)
				!= UImmortalMaterialLibrary::GetMaterialQuantity(Right, MaterialId))
			{
				return false;
			}
		}
		return true;
	}

	bool HaveSameMapProgress(
		const FImmortalMapSystemState& Left,
		const FImmortalMapSystemState& Right)
	{
		if (Left.ActiveMapId != Right.ActiveMapId
			|| Left.MapProgress.Num()
				!= Right.MapProgress.Num())
		{
			return false;
		}
		for (const FName MapId :
			UImmortalMapLibrary::GetKnownMapIds())
		{
			FImmortalMapProgress LeftProgress;
			FImmortalMapProgress RightProgress;
			if (!UImmortalMapLibrary::GetMapProgress(
					Left, MapId, LeftProgress)
				|| !UImmortalMapLibrary::GetMapProgress(
					Right, MapId, RightProgress)
				|| LeftProgress.Stage != RightProgress.Stage
				|| LeftProgress.StageKills
					!= RightProgress.StageKills
				|| LeftProgress.bCompleted
					!= RightProgress.bCompleted)
			{
				return false;
			}
		}
		return true;
	}

	bool IsAscensionCycleStartState(
		const FImmortalMapSystemState& State)
	{
		if (State.ActiveMapId
			!= UImmortalMapLibrary::GetQingyunMountainId()
			|| State.MapProgress.Num()
				!= UImmortalMapLibrary::GetKnownMapIds().Num())
		{
			return false;
		}
		for (const FName MapId :
			UImmortalMapLibrary::GetKnownMapIds())
		{
			FImmortalMapProgress Progress;
			if (!UImmortalMapLibrary::GetMapProgress(
					State, MapId, Progress)
				|| Progress.Stage != 1
				|| Progress.StageKills != 0
				|| Progress.bCompleted)
			{
				return false;
			}
		}
		return true;
	}

	bool HaveMergedLifetimeMapRecords(
		const FImmortalAscensionState& PreviousState,
		const FImmortalAscensionState& CurrentState,
		const FImmortalMapSystemState& CompletedCycle)
	{
		for (const FName MapId :
			UImmortalMapLibrary::GetKnownMapIds())
		{
			FImmortalAscensionMapLegacy PreviousRecord;
			FImmortalAscensionMapLegacy CurrentRecord;
			FImmortalMapProgress CycleProgress;
			if (!UImmortalAscensionLibrary::GetLifetimeMapRecord(
					PreviousState, MapId, PreviousRecord)
				|| !UImmortalAscensionLibrary::GetLifetimeMapRecord(
					CurrentState, MapId, CurrentRecord)
				|| !UImmortalMapLibrary::GetMapProgress(
					CompletedCycle, MapId, CycleProgress))
			{
				return false;
			}
			const int32 ExpectedCompletions =
				FMath::Min(
					PreviousRecord.TimesCompleted
						+ (CycleProgress.bCompleted ? 1 : 0),
					UImmortalAscensionLibrary
						::MaximumAscensionCount);
			if (CurrentRecord.HighestStage
					< FMath::Max(
						PreviousRecord.HighestStage,
						CycleProgress.Stage)
				|| CurrentRecord.TimesCompleted
					!= ExpectedCompletions)
			{
				return false;
			}
		}
		return true;
	}

	bool HaveSameLifetimeMapRecords(
		const FImmortalAscensionState& Left,
		const FImmortalAscensionState& Right)
	{
		for (const FName MapId :
			UImmortalMapLibrary::GetKnownMapIds())
		{
			FImmortalAscensionMapLegacy LeftRecord;
			FImmortalAscensionMapLegacy RightRecord;
			if (!UImmortalAscensionLibrary::GetLifetimeMapRecord(
					Left, MapId, LeftRecord)
				|| !UImmortalAscensionLibrary::GetLifetimeMapRecord(
					Right, MapId, RightRecord)
				|| LeftRecord.HighestStage
					!= RightRecord.HighestStage
				|| LeftRecord.TimesCompleted
					!= RightRecord.TimesCompleted)
			{
				return false;
			}
		}
		return true;
	}

	bool ShouldForceFarmingPersistenceFailure(const TCHAR* Operation)
	{
#if !UE_BUILD_SHIPPING
		FString ForcedOperation;
		return Operation
			&& FParse::Value(
				FCommandLine::Get(), TEXT("ImmortalTestForceFarmingSaveFailure="), ForcedOperation)
			&& ForcedOperation.Equals(Operation, ESearchCase::IgnoreCase);
#else
		return false;
#endif
	}

	bool ShouldForceSectPersistenceFailure(const TCHAR* Operation)
	{
#if !UE_BUILD_SHIPPING
		FString ForcedOperation;
		return Operation
			&& FParse::Value(
				FCommandLine::Get(), TEXT("ImmortalTestForceSectSaveFailure="), ForcedOperation)
			&& ForcedOperation.Equals(Operation, ESearchCase::IgnoreCase);
#else
		return false;
#endif
	}

	bool ShouldForceWorldBossPersistenceFailure(const TCHAR* Operation)
	{
#if !UE_BUILD_SHIPPING
		FString ForcedOperation;
		return Operation
			&& FParse::Value(
				FCommandLine::Get(),
				TEXT("ImmortalTestForceWorldBossSaveFailure="),
				ForcedOperation)
			&& ForcedOperation.Equals(Operation, ESearchCase::IgnoreCase);
#else
		return false;
#endif
	}

	bool ShouldForceEndlessDungeonPersistenceFailure(const TCHAR* Operation)
	{
#if !UE_BUILD_SHIPPING
		FString ForcedOperation;
		return Operation
			&& FParse::Value(
				FCommandLine::Get(),
				TEXT("ImmortalTestForceEndlessSaveFailure="),
				ForcedOperation)
			&& ForcedOperation.Equals(Operation, ESearchCase::IgnoreCase);
#else
		return false;
#endif
	}

	bool ShouldForcePetPersistenceFailure(const TCHAR* Operation)
	{
#if !UE_BUILD_SHIPPING
		FString ForcedOperation;
		return Operation
			&& FParse::Value(
				FCommandLine::Get(),
				TEXT("ImmortalTestForcePetSaveFailure="),
				ForcedOperation)
			&& ForcedOperation.Equals(
				Operation, ESearchCase::IgnoreCase);
#else
		return false;
#endif
	}

	bool ShouldForceAscensionPersistenceFailure(
		const TCHAR* Operation)
	{
#if !UE_BUILD_SHIPPING
		FString ForcedOperation;
		return Operation
			&& FParse::Value(
				FCommandLine::Get(),
				TEXT("ImmortalTestForceAscensionSaveFailure="),
				ForcedOperation)
			&& ForcedOperation.Equals(
				Operation, ESearchCase::IgnoreCase);
#else
		return false;
#endif
	}

	bool ShouldForceInventoryPersistenceFailure(const TCHAR* Operation)
	{
#if !UE_BUILD_SHIPPING
		FString ForcedOperation;
		return Operation
			&& FParse::Value(
				FCommandLine::Get(), TEXT("ImmortalTestForceInventorySaveFailure="), ForcedOperation)
			&& ForcedOperation.Equals(Operation, ESearchCase::IgnoreCase);
#else
		return false;
#endif
	}

	bool HaveSameEquipmentOrder(
		const TArray<FImmortalEquipmentItem>& Left,
		const TArray<FImmortalEquipmentItem>& Right)
	{
		if (Left.Num() != Right.Num()) return false;
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index].ItemId != Right[Index].ItemId || Left[Index].bLocked != Right[Index].bLocked) return false;
		}
		return true;
	}

	bool HaveSameMaterialOrder(
		const TArray<FImmortalMaterialStack>& Left,
		const TArray<FImmortalMaterialStack>& Right)
	{
		if (Left.Num() != Right.Num()) return false;
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index].MaterialId != Right[Index].MaterialId || Left[Index].Quantity != Right[Index].Quantity) return false;
		}
		return true;
	}

	bool HaveSamePillOrder(
		const TArray<FImmortalPillStack>& Left,
		const TArray<FImmortalPillStack>& Right)
	{
		if (Left.Num() != Right.Num()) return false;
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index].PillId != Right[Index].PillId
				|| Left[Index].Quality != Right[Index].Quality
				|| Left[Index].Quantity != Right[Index].Quantity) return false;
		}
		return true;
	}

	bool HaveSameArtifactOrder(
		const TArray<FImmortalArtifactItem>& Left,
		const TArray<FImmortalArtifactItem>& Right)
	{
		if (Left.Num() != Right.Num()) return false;
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index].InstanceId != Right[Index].InstanceId || Left[Index].bLocked != Right[Index].bLocked) return false;
		}
		return true;
	}

	bool HaveSameQuestItemOrder(
		const TArray<FImmortalQuestItemStack>& Left,
		const TArray<FImmortalQuestItemStack>& Right)
	{
		if (Left.Num() != Right.Num()) return false;
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index].QuestItemId != Right[Index].QuestItemId || Left[Index].Quantity != Right[Index].Quantity) return false;
		}
		return true;
	}

	FString FormatInventoryMaterials(const TArray<FImmortalMaterialStack>& Materials)
	{
		TArray<FString> Parts;
		for (const FImmortalMaterialStack& Stack : Materials)
		{
			FImmortalMaterialDefinition Definition;
			const FString Name = UImmortalMaterialLibrary::GetMaterialDefinition(Stack.MaterialId, Definition)
				? Definition.DisplayName.ToString()
				: Stack.MaterialId.ToString();
			Parts.Add(FString::Printf(TEXT("%s×%d"), *Name, Stack.Quantity));
		}
		return FString::Join(Parts, TEXT("、"));
	}
}

AImmortalPlayerCharacter::AImmortalPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	DamageTypeClass = UDamageType::StaticClass();
	CultivationComponent = CreateDefaultSubobject<UImmortalCultivationComponent>(TEXT("CultivationComponent"));
	MortalRealmIdleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(
		TEXT("/Game/GAME/Asset/Player/mortal/generated/FB_Player_Mortal_Idle.FB_Player_Mortal_Idle")));
	MortalRealmMoveFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(
		TEXT("/Game/GAME/Asset/Player/mortal/generated/FB_Player_Mortal_Move.FB_Player_Mortal_Move")));
	MortalRealmAttackFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(
		TEXT("/Game/GAME/Asset/Player/mortal/generated/FB_Player_Mortal_Attack.FB_Player_Mortal_Attack")));
	MortalRealmHurtFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(
		TEXT("/Game/GAME/Asset/Player/mortal/generated/FB_Player_Mortal_Hurt.FB_Player_Mortal_Hurt")));
	MortalRealmDeathFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(
		TEXT("/Game/GAME/Asset/Player/mortal/generated/FB_Player_Mortal_Death.FB_Player_Mortal_Death")));
}

void AImmortalPlayerCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateMortalRealmLocomotionAnimation();
}

void AImmortalPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	LoadMortalRealmAnimationSet();
	InventoryItems.Reset();
	EquippedItems.Reset();
	MaterialInventory.Reset();
	MaterialInventoryRevision = 0;
	PillInventory.Reset();
	PillInventoryRevision = 0;
	ArtifactInventory.Reset();
	EquippedArtifactInstanceId.Invalidate();
	ArtifactInventoryRevision = 0;
	QuestItemInventory.Reset();
	QuestItemInventoryRevision = 0;
	ArtifactAttackCounter = 0;
	ArtifactShield = 0.0f;
	ArtifactAttackMultiplier = 1.0f;
	ArtifactDefenseMultiplier = 1.0f;
	ArtifactHealthMultiplier = 1.0f;
	ArtifactAttackSpeedBonus = 0.0f;
	ArtifactCriticalChanceBonus = 0.0f;
	TechniqueLibrary.Reset();
	EquippedTechniqueIds.Reset();
	TechniqueInsightPoints = 0;
	TechniqueRevision = 0;
	TechniqueAttackCounters.Reset();
	TechniqueActiveCounters.Reset();
	TechniqueShield = 0.0f;
	TechniqueAttackMultiplier = 1.0f;
	TechniqueDefenseMultiplier = 1.0f;
	TechniqueHealthMultiplier = 1.0f;
	TechniqueAttackSpeedBonus = 0.0f;
	TechniqueCriticalChanceBonus = 0.0f;
	TechniqueCultivationRateMultiplier = 1.0f;
	SpiritRootState = FImmortalSpiritRootState();
	CultivationPathState = FImmortalCultivationPathState();
	CharacterBuildRevision = 0;
	CultivationPathAttackCounter = 0;
	CultivationPathShield = 0.0f;
	CharacterPathAttackMultiplier = 1.0f;
	CharacterPathDefenseMultiplier = 1.0f;
	CharacterPathHealthMultiplier = 1.0f;
	CharacterPathManaMultiplier = 1.0f;
	CharacterPathAttackSpeedBonus = 0.0f;
	CharacterPathCriticalChanceBonus = 0.0f;
	CharacterPathDamageReduction = 0.0f;
	CharacterPathCultivationRateMultiplier = 1.0f;
	ShopState = FImmortalShopState();
	ShopRevision = 0;
	DisplayedMapId = UImmortalMapLibrary::GetQingyunMountainId();
	FImmortalMapDefinition InitialMapDefinition;
	if (UImmortalMapLibrary::GetMapDefinition(DisplayedMapId, InitialMapDefinition))
	{
		DisplayedMapName = InitialMapDefinition.DisplayName;
		DisplayedMapMaximumStage = InitialMapDefinition.MaximumStage;
	}
	CachedMapSystemState = UImmortalMapLibrary::CreateMigratedState(1, 0, false);
	CaveState = UImmortalCaveLibrary::CreateDefaultState(FDateTime::UtcNow().GetTicks());
	FarmingState = UImmortalFarmingLibrary::CreateDefaultState(FDateTime::UtcNow().GetTicks());
	SectState = UImmortalSectLibrary::CreateDefaultState(FDateTime::UtcNow().GetTicks(), SectUtcOffsetMinutes);
	QuestState = UImmortalQuestLibrary::CreateDefaultState(
		FDateTime::UtcNow().GetTicks(), SectUtcOffsetMinutes);
	WorldBossState = UImmortalWorldBossLibrary::CreateDefaultState();
	EndlessDungeonState =
		UImmortalEndlessDungeonLibrary::CreateDefaultState();
	PetState = UImmortalPetLibrary::CreateDefaultState();
	AscensionState =
		UImmortalAscensionLibrary::CreateDefaultState();
	AlchemyCultivationBoostMultiplier = 1.0f;
	AlchemyBoostEndWorldTime = 0.0f;
	RecalculateEquipmentBonuses();
	CurrentHealth = FMath::Max(GetMaxHealth(), 1.0f);
	CurrentMana = FMath::Max(MaxMana, 0.0f);
	CurrentCultivation = FMath::Max(StartingCultivation, 0);
	CurrentGold = FMath::Max(StartingGold, 0);
	EquipmentDropCount = 0;
	EquipmentInventoryRevision = 0;
	bDead = false;
	bDeathCultivationRecoveryRequired = false;
	bAdventureSuspendedForDeathRecovery = false;
	InitialSpawnLocation = GetActorLocation();
	InvulnerableUntilTime = 0.0f;
	if (CultivationComponent)
	{
		CultivationComponent->OnCultivationProgressChanged.AddDynamic(
			this, &AImmortalPlayerCharacter::HandleCultivationProgressChanged);
		CultivationComponent->OnCultivationBreakthrough.AddDynamic(
			this, &AImmortalPlayerCharacter::HandleCultivationBreakthrough);
		CultivationComponent->InitializeProgress(
			EImmortalCultivationRealm::QiRefining, 1, FMath::Max(StartingCultivation, 0));
	}
	const bool bLoadedProgress = LoadProgress();
	RecalculateCaveBonuses();
	AwakenSpiritRootIfNeeded();
	RecalculateEquipmentBonuses();
	if (!bLoadedProgress)
	{
		RefreshShopForDay(
			UImmortalShopLibrary::GetDayKeyFromUtcTicks(FDateTime::UtcNow().GetTicks(), ShopUtcOffsetMinutes),
			1,
			true);
		CurrentHealth = GetMaxHealth();
		CurrentMana = GetMaxMana();
		SaveProgress();
	}
	ApplyDesktopSettings();
	ConfigureCombatCamera();
	ConfigureTaskbarWindow();

	if (APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PlayerStatusWidget = CreateWidget<UImmortalPlayerStatusWidget>(PlayerController, UImmortalPlayerStatusWidget::StaticClass());
		DesktopGroundWidget = CreateWidget<UImmortalDesktopGroundWidget>(PlayerController);
		if (DesktopGroundWidget)
		{
			DesktopGroundWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			DesktopGroundWidget->ForceVolatile(true);
			DesktopGroundWidget->AddToViewport(0);
		}
		if (PlayerStatusWidget)
		{
			PlayerStatusWidget->InitializeForPlayer(this);
			PlayerStatusWidget->AddToViewport(10);
			PlayerStatusWidget->SetPositionInViewport(FVector2D(24.0f, 16.0f), false);
			PlayerStatusWidget->SetDesiredSizeInViewport(FVector2D(512.0f, 64.0f));
		}
		ConfigureModalWidget(nullptr, false);

		PlayerManagementWidget = CreateWidget<UImmortalManagementWidget>(
			PlayerController,
			UImmortalManagementWidget::StaticClass());
		if (PlayerManagementWidget)
		{
			PlayerManagementWidget->InitializeForPlayer(this);
			PlayerManagementWidget->AddToViewport(100);
			PlayerManagementWidget->SetDesiredSizeInViewport(
				FVector2D(1707.0f, 320.0f));
			PlayerManagementWidget->SetVisibility(
				ESlateVisibility::Collapsed);
		}

		PlayerCultivationWidget = CreateWidget<UImmortalCultivationWidget>(
			PlayerController,
			UImmortalCultivationWidget::StaticClass());
		if (PlayerCultivationWidget)
		{
			PlayerCultivationWidget->InitializeForPlayer(this);
		}

		PlayerInventoryWidget = CreateWidget<UImmortalInventoryWidget>(PlayerController, UImmortalInventoryWidget::StaticClass());
		if (PlayerInventoryWidget)
		{
			PlayerInventoryWidget->InitializeForPlayer(this);
		}

		PlayerAlchemyWidget = CreateWidget<UImmortalAlchemyWidget>(PlayerController, UImmortalAlchemyWidget::StaticClass());
		if (PlayerAlchemyWidget)
		{
			PlayerAlchemyWidget->InitializeForPlayer(this);
		}

		PlayerCraftingWidget = CreateWidget<UImmortalCraftingWidget>(PlayerController, UImmortalCraftingWidget::StaticClass());
		if (PlayerCraftingWidget)
		{
			PlayerCraftingWidget->InitializeForPlayer(this);
		}

		PlayerArtifactWidget = CreateWidget<UImmortalArtifactWidget>(PlayerController, UImmortalArtifactWidget::StaticClass());
		if (PlayerArtifactWidget)
		{
			PlayerArtifactWidget->InitializeForPlayer(this);
		}

		PlayerTechniqueWidget = CreateWidget<UImmortalTechniqueWidget>(PlayerController, UImmortalTechniqueWidget::StaticClass());
		if (PlayerTechniqueWidget)
		{
			PlayerTechniqueWidget->InitializeForPlayer(this);
		}

		PlayerCharacterBuildWidget = CreateWidget<UImmortalCharacterBuildWidget>(PlayerController, UImmortalCharacterBuildWidget::StaticClass());
		if (PlayerCharacterBuildWidget)
		{
			PlayerCharacterBuildWidget->InitializeForPlayer(this);
		}

		PlayerShopWidget = CreateWidget<UImmortalShopWidget>(PlayerController, UImmortalShopWidget::StaticClass());
		if (PlayerShopWidget)
		{
			PlayerShopWidget->InitializeForPlayer(this);
		}

		PlayerMapWidget = CreateWidget<UImmortalMapWidget>(PlayerController, UImmortalMapWidget::StaticClass());
		if (PlayerMapWidget)
		{
			PlayerMapWidget->InitializeForPlayer(this);
		}

		PlayerQuestWidget = CreateWidget<UImmortalQuestWidget>(
			PlayerController, UImmortalQuestWidget::StaticClass());
		if (PlayerQuestWidget)
		{
			PlayerQuestWidget->InitializeForPlayer(this);
		}

		PlayerCaveWidget = CreateWidget<UImmortalCaveWidget>(PlayerController, UImmortalCaveWidget::StaticClass());
		if (PlayerCaveWidget)
		{
			PlayerCaveWidget->InitializeForPlayer(this);
		}

		PlayerFarmingWidget = CreateWidget<UImmortalFarmingWidget>(PlayerController, UImmortalFarmingWidget::StaticClass());
		if (PlayerFarmingWidget)
		{
			PlayerFarmingWidget->InitializeForPlayer(this);
		}

		PlayerSectWidget = CreateWidget<UImmortalSectWidget>(PlayerController, UImmortalSectWidget::StaticClass());
		if (PlayerSectWidget)
		{
			PlayerSectWidget->InitializeForPlayer(this);
		}

		PlayerWorldBossWidget = CreateWidget<UImmortalWorldBossWidget>(
			PlayerController, UImmortalWorldBossWidget::StaticClass());
		if (PlayerWorldBossWidget)
		{
			PlayerWorldBossWidget->InitializeForPlayer(this);
		}

		PlayerEndlessDungeonWidget =
			CreateWidget<UImmortalEndlessDungeonWidget>(
				PlayerController,
				UImmortalEndlessDungeonWidget::StaticClass());
		if (PlayerEndlessDungeonWidget)
		{
			PlayerEndlessDungeonWidget->InitializeForPlayer(this);
		}

		PlayerPetWidget = CreateWidget<UImmortalPetWidget>(
			PlayerController,
			UImmortalPetWidget::StaticClass());
		if (PlayerPetWidget)
		{
			PlayerPetWidget->InitializeForPlayer(this);
		}

		PlayerAscensionWidget =
			CreateWidget<UImmortalAscensionWidget>(
				PlayerController,
				UImmortalAscensionWidget::StaticClass());
		if (PlayerAscensionWidget)
		{
			PlayerAscensionWidget->InitializeForPlayer(this);
			PlayerAscensionWidget->AddToViewport(200);
			PlayerAscensionWidget->SetDesiredSizeInViewport(
				FVector2D(1600.0f, 300.0f));
			PlayerAscensionWidget->SetVisibility(
				ESlateVisibility::Collapsed);
		}

		PlayerSettingsWidget =
			CreateWidget<UImmortalSettingsWidget>(
				PlayerController,
				UImmortalSettingsWidget::StaticClass());
		if (PlayerSettingsWidget)
		{
			PlayerSettingsWidget->InitializeForPlayer(this);
		}

		RegisterManagementPages();

		CombatFeedbackWidget = CreateWidget<UImmortalCombatFeedbackWidget>(PlayerController, UImmortalCombatFeedbackWidget::StaticClass());
		if (CombatFeedbackWidget)
		{
			CombatFeedbackWidget->InitializeForPlayer(this);
			CombatFeedbackWidget->AddToViewport(50);
			CombatFeedbackWidget->SetVisibility(
				ESlateVisibility::Collapsed);
			CombatFeedbackWidget->SetStageProgress(
				DisplayedMapName,
				DisplayedMapMaximumStage,
				DisplayedStage,
				DisplayedStageKills,
				DisplayedStageRequiredKills,
				bDisplayedBossStage,
				bDisplayedMapCompleted);
			if (bDisplayedBossStage)
			{
				CombatFeedbackWidget->ShowBossAnnouncement(
					FText::FromString(FString::Printf(TEXT("第 %d 关：守关妖王出现"), DisplayedStage)),
					FLinearColor(1.0f, 0.28f, 0.12f, 1.0f));
			}
			if (bHasUnshownOfflineReward)
			{
				CombatFeedbackWidget->ShowOfflineRewardSummary(
					LastOfflineRewardResult.RewardedOfflineSeconds,
					LastOfflineRewardResult.Cultivation,
					LastOfflineRewardResult.SpiritStones,
					LastOfflineRewardResult.EquipmentCount,
					LastOfflineRewardResult.MaterialCount,
					MaximumOfflineHours,
					LastOfflineRewardResult.bCappedByMaximum);
				bHasUnshownOfflineReward = false;
			}
		}
	}

#if !UE_BUILD_SHIPPING
	if (FParse::Param(
		FCommandLine::Get(),
		TEXT("ImmortalTestScreenshotSettings")))
	{
		FTimerHandle SettingsFixtureTimer;
		GetWorldTimerManager().SetTimer(
			SettingsFixtureTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this]
				{
					if (!bSettingsOpen)
					{
						ToggleSettings();
					}
					UE_LOG(
						LogTemp,
						Display,
						TEXT("Desktop settings runtime audit: topmost=%s muted=%s fps=%d height=%d"),
						IsDesktopAlwaysOnTopEnabled()
							? TEXT("true")
							: TEXT("false"),
						IsDesktopMuted()
							? TEXT("true")
							: TEXT("false"),
						GetDesktopFrameRateLimit(),
						GetDesktopWindowHeight());

					FTimerHandle ScreenshotTimer;
					GetWorldTimerManager().SetTimer(
						ScreenshotTimer,
						FTimerDelegate::CreateWeakLambda(
							this,
							[]
							{
								const FString ScreenshotPath =
									FPaths::Combine(
										FPaths::ProjectSavedDir(),
										TEXT("Screenshots/Step34_Settings_TBH.png"));
								FScreenshotRequest::RequestScreenshot(
									ScreenshotPath,
									true,
									false);
								UE_LOG(
									LogTemp,
									Display,
									TEXT("Desktop settings verification screenshot requested: %s"),
									*ScreenshotPath);
							}),
						0.75f,
						false);

					FTimerHandle ExitTimer;
					GetWorldTimerManager().SetTimer(
						ExitTimer,
						FTimerDelegate::CreateWeakLambda(
							this,
							[]
							{
								UE_LOG(
									LogTemp,
									Display,
									TEXT("Desktop settings verification complete; requesting clean exit"));
								FPlatformMisc::RequestExit(false);
							}),
						2.2f,
						false);
				}),
			1.20f,
			false);
	}
#endif

#if !UE_BUILD_SHIPPING
	if (FParse::Param(
		FCommandLine::Get(),
		TEXT("ImmortalTestPreviewAscensionAnimation")))
	{
		FTimerHandle AscensionPreviewTimer;
		GetWorldTimerManager().SetTimer(
			AscensionPreviewTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this]
				{
					OpenAscensionInterface();
					if (PlayerAscensionWidget)
					{
						PlayerAscensionWidget->PlayAscensionSequence();
					}

					FTimerHandle ScreenshotTimer;
					GetWorldTimerManager().SetTimer(
						ScreenshotTimer,
						FTimerDelegate::CreateWeakLambda(
							this,
							[]
							{
								const FString ScreenshotPath =
									FPaths::Combine(
										FPaths::ProjectSavedDir(),
										TEXT("Screenshots/AscensionInterface_TBH.png"));
								FScreenshotRequest::RequestScreenshot(
									ScreenshotPath,
									true,
									false);
								UE_LOG(
									LogTemp,
									Display,
									TEXT("Independent ascension interface screenshot requested: %s"),
									*ScreenshotPath);
							}),
						0.65f,
						false);

					FTimerHandle ExitTimer;
					GetWorldTimerManager().SetTimer(
						ExitTimer,
						FTimerDelegate::CreateWeakLambda(
							this,
							[]
							{
								UE_LOG(
									LogTemp,
									Display,
									TEXT("Player ascension animation verification complete; requesting clean exit"));
								FPlatformMisc::RequestExit(false);
							}),
						2.2f,
						false);
				}),
			1.2f,
			false);
	}
#endif

#if !UE_BUILD_SHIPPING
	if (FParse::Param(
		FCommandLine::Get(),
		TEXT("ImmortalTestManagementInterface")))
	{
		FTimerHandle ManagementFixtureTimer;
		GetWorldTimerManager().SetTimer(
			ManagementFixtureTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this]
				{
					const int32 StartingKills = DisplayedStageKills;
					const int32 StartingCultivation = CurrentCultivation;
					OpenManagementFeature(
						EImmortalManagementFeature::Cultivation);
					QueueManagementNotification(
						FText::FromString(TEXT(
							"\u9752\u4E91\u5251\uFF08\u5DF2\u81EA\u52A8\u62FE\u53D6\u81F3\u50A8\u7269\u6212\uFF09")),
						FLinearColor(0.55f, 0.78f, 1.0f, 1.0f),
						5.0f);

					FTimerHandle CultivationShotTimer;
					GetWorldTimerManager().SetTimer(
						CultivationShotTimer,
						FTimerDelegate::CreateWeakLambda(
							this,
							[]
							{
								const FString ScreenshotPath =
									FPaths::Combine(
										FPaths::ProjectSavedDir(),
										TEXT("Screenshots/Management_Cultivation_TBH.png"));
								FScreenshotRequest::RequestScreenshot(
									ScreenshotPath, true, false);
							}),
						0.55f,
						false);

					FTimerHandle SwitchPageTimer;
					GetWorldTimerManager().SetTimer(
						SwitchPageTimer,
						FTimerDelegate::CreateWeakLambda(
							this,
							[this, StartingKills, StartingCultivation]
							{
								OpenManagementFeature(
									EImmortalManagementFeature::Sect);
								UE_LOG(
									LogTemp,
									Display,
									TEXT("Management background combat audit: worldPaused=%s autoAttackActive=%s kills=%d->%d cultivation=%d->%d"),
									UGameplayStatics::IsGamePaused(this)
										? TEXT("true") : TEXT("false"),
									GetWorldTimerManager().IsTimerActive(
										AutoAttackTimerHandle)
										? TEXT("true") : TEXT("false"),
									StartingKills,
									DisplayedStageKills,
									StartingCultivation,
									CurrentCultivation);

								FTimerHandle SectShotTimer;
								GetWorldTimerManager().SetTimer(
									SectShotTimer,
									FTimerDelegate::CreateWeakLambda(
										this,
										[]
										{
											const FString ScreenshotPath =
												FPaths::Combine(
													FPaths::ProjectSavedDir(),
													TEXT("Screenshots/Management_Sect_TBH.png"));
											FScreenshotRequest::RequestScreenshot(
												ScreenshotPath, true, false);
										}),
									0.55f,
									false);
							}),
						2.0f,
						false);

					FTimerHandle QuestPageTimer;
					GetWorldTimerManager().SetTimer(
						QuestPageTimer,
						FTimerDelegate::CreateWeakLambda(
							this,
							[this]
							{
								OpenManagementFeature(
									EImmortalManagementFeature::Quest);
								UE_LOG(
									LogTemp,
									Display,
									TEXT("Unified quest page audit: worldPaused=%s autoAttackActive=%s topLevelManagementOpen=%s"),
									UGameplayStatics::IsGamePaused(this)
										? TEXT("true") : TEXT("false"),
									GetWorldTimerManager().IsTimerActive(
										AutoAttackTimerHandle)
										? TEXT("true") : TEXT("false"),
									bManagementInterfaceOpen
										? TEXT("true") : TEXT("false"));

								FTimerHandle QuestShotTimer;
								GetWorldTimerManager().SetTimer(
									QuestShotTimer,
									FTimerDelegate::CreateWeakLambda(
										this,
										[]
										{
											const FString ScreenshotPath =
												FPaths::Combine(
													FPaths::ProjectSavedDir(),
													TEXT("Screenshots/Management_Quest_TBH.png"));
											FScreenshotRequest::RequestScreenshot(
												ScreenshotPath, true, false);
										}),
									0.55f,
									false);
							}),
						3.5f,
						false);

					FTimerHandle CombatReturnTimer;
					GetWorldTimerManager().SetTimer(
						CombatReturnTimer,
						FTimerDelegate::CreateWeakLambda(
							this,
							[this]
							{
								CloseManagementInterface();
								const FString ScreenshotPath =
									FPaths::Combine(
										FPaths::ProjectSavedDir(),
										TEXT("Screenshots/Combat_HealthOnly_TBH.png"));
								FScreenshotRequest::RequestScreenshot(
									ScreenshotPath, true, false);
							}),
						5.0f,
						false);

					FTimerHandle ExitTimer;
					GetWorldTimerManager().SetTimer(
						ExitTimer,
						FTimerDelegate::CreateWeakLambda(
							this,
							[] { FPlatformMisc::RequestExit(false); }),
						6.5f,
						false);
				}),
			1.2f,
			false);
	}
#endif

#if !UE_BUILD_SHIPPING
	if (FParse::Param(
		FCommandLine::Get(),
		TEXT("ImmortalTestManagementScenes")))
	{
		FTimerHandle SceneFixtureTimer;
		GetWorldTimerManager().SetTimer(
			SceneFixtureTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this]
				{
					const int32 StartingKills = DisplayedStageKills;
					OpenManagementInterface();
					auto CaptureScene = [this, StartingKills](
						const EImmortalManagementScene Scene,
						const TCHAR* Filename,
						const TCHAR* Label)
					{
						if (PlayerManagementWidget)
						{
							PlayerManagementWidget->ShowScene(Scene);
						}
						UE_LOG(
							LogTemp,
							Display,
							TEXT("Management scene audit: label=%s scene=%d feature=%d worldPaused=%s autoAttack=%s kills=%d->%d"),
							Label,
							PlayerManagementWidget
								? static_cast<int32>(PlayerManagementWidget->GetActiveScene())
								: -1,
							PlayerManagementWidget
								? static_cast<int32>(PlayerManagementWidget->GetActiveFeature())
								: -1,
							UGameplayStatics::IsGamePaused(this)
								? TEXT("true") : TEXT("false"),
							GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)
								? TEXT("true") : TEXT("false"),
							StartingKills,
							DisplayedStageKills);
						const FString DeferredFilename(Filename);
						FTimerHandle ScreenshotTimer;
						GetWorldTimerManager().SetTimer(
							ScreenshotTimer,
							FTimerDelegate::CreateWeakLambda(
								this,
								[this, DeferredFilename]
								{
									FScreenshotRequest::RequestScreenshot(
										FPaths::Combine(
											FPaths::ProjectSavedDir(),
											FString::Printf(
												TEXT("Screenshots/%s"),
												*DeferredFilename)),
										true,
										false);
								}),
							0.30f,
							false);
					};

					CaptureScene(
						EImmortalManagementScene::SectSanctuary,
						TEXT("ManagementScene_SectSanctuary_TBH.png"),
						TEXT("SectSanctuary"));

					FTimerHandle MarketTimer;
					GetWorldTimerManager().SetTimer(
						MarketTimer,
						FTimerDelegate::CreateWeakLambda(
							this,
							[this, CaptureScene]
							{
								CaptureScene(
									EImmortalManagementScene::MarketTown,
									TEXT("ManagementScene_MarketTown_TBH.png"),
									TEXT("MarketTown"));
							}),
						1.0f,
						false);

					FTimerHandle CaveTimer;
					GetWorldTimerManager().SetTimer(
						CaveTimer,
						FTimerDelegate::CreateWeakLambda(
							this,
							[this, CaptureScene]
							{
								CaptureScene(
									EImmortalManagementScene::CaveEstate,
									TEXT("ManagementScene_CaveEstate_TBH.png"),
									TEXT("CaveEstate"));
							}),
						2.0f,
						false);

					FTimerHandle AdventureTimer;
					GetWorldTimerManager().SetTimer(
						AdventureTimer,
						FTimerDelegate::CreateWeakLambda(
							this,
							[this, CaptureScene]
							{
								CaptureScene(
									EImmortalManagementScene::AdventureHall,
									TEXT("ManagementScene_AdventureHall_TBH.png"),
									TEXT("AdventureHall"));
							}),
						3.0f,
						false);

					FTimerHandle FeatureTimer;
					GetWorldTimerManager().SetTimer(
						FeatureTimer,
						FTimerDelegate::CreateWeakLambda(
							this,
							[this]
							{
								OpenManagementFeature(
									EImmortalManagementFeature::Cultivation);
								UE_LOG(
									LogTemp,
									Display,
									TEXT("Management scene feature mapping audit: feature=%d scene=%d"),
									PlayerManagementWidget
										? static_cast<int32>(PlayerManagementWidget->GetActiveFeature()) : -1,
									PlayerManagementWidget
										? static_cast<int32>(PlayerManagementWidget->GetActiveScene()) : -1);
							}),
						4.0f,
						false);

					FTimerHandle ReturnSceneTimer;
					GetWorldTimerManager().SetTimer(
						ReturnSceneTimer,
						FTimerDelegate::CreateWeakLambda(
							this,
							[this]
							{
								OpenManagementFeature(EImmortalManagementFeature::Home);
								UE_LOG(
									LogTemp,
									Display,
									TEXT("Management scene return audit: feature=%d scene=%d"),
									PlayerManagementWidget
										? static_cast<int32>(PlayerManagementWidget->GetActiveFeature()) : -1,
									PlayerManagementWidget
										? static_cast<int32>(PlayerManagementWidget->GetActiveScene()) : -1);
							}),
						4.8f,
						false);

					FTimerHandle CloseTimer;
					GetWorldTimerManager().SetTimer(
						CloseTimer,
						FTimerDelegate::CreateWeakLambda(
							this,
							[this] { CloseManagementInterface(); }),
						5.6f,
						false);

					FTimerHandle ExitTimer;
					GetWorldTimerManager().SetTimer(
						ExitTimer,
						FTimerDelegate::CreateWeakLambda(
							this,
							[] { FPlatformMisc::RequestExit(false); }),
						6.5f,
						false);
				}),
			1.2f,
			false);
	}
#endif

	SpawnActivePetActor();

	// Retry durable rewards after the combat HUD exists so a successful
	// recovery on startup still shows the required five-second summary.
	if (!WorldBossState.PendingRewards.IsEmpty())
	{
		RetryPendingWorldBossRewards();
	}
	if (!EndlessDungeonState.PendingRewards.IsEmpty())
	{
		RetryPendingEndlessDungeonRewards();
	}

#if !UE_BUILD_SHIPPING
	FString TestWorldBossId;
	const bool bHasTestWorldBoss = FParse::Value(
		FCommandLine::Get(), TEXT("ImmortalTestWorldBoss="), TestWorldBossId);
	const bool bTestOpenWorldBoss =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestOpenWorldBoss"));
	const bool bTestWorldBossPhaseThree =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestWorldBossPhaseThree"));
	const bool bTestDefeatWorldBoss =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestDefeatWorldBoss"));
	const bool bTestCancelWorldBoss =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestCancelWorldBoss"));
	const bool bTestTimeoutWorldBoss =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestTimeoutWorldBoss"));
	const bool bTestWorldBossPlayerDeath =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestWorldBossPlayerDeath"));
	const bool bTestScreenshotWorldBossUi =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotWorldBossUI"));
	const bool bTestScreenshotWorldBossBattle =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotWorldBossBattle"));
	const bool bTestScreenshotWorldBossReward =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotWorldBossReward"));
	const bool bTestLogWorldBoss =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestLogWorldBoss"));
	if (bHasTestWorldBoss || bTestOpenWorldBoss || bTestWorldBossPhaseThree
		|| bTestDefeatWorldBoss || bTestCancelWorldBoss || bTestTimeoutWorldBoss
		|| bTestWorldBossPlayerDeath
		|| bTestScreenshotWorldBossUi
		|| bTestScreenshotWorldBossBattle || bTestScreenshotWorldBossReward
		|| bTestLogWorldBoss)
	{
		if (!bHasTestWorldBoss)
		{
			TestWorldBossId = TEXT("AzureScaleDragon");
		}
		FTimerHandle WorldBossFixtureTimer;
		GetWorldTimerManager().SetTimer(
			WorldBossFixtureTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this,
					TestWorldBossId,
					bHasTestWorldBoss,
					bTestOpenWorldBoss,
					bTestWorldBossPhaseThree,
					bTestDefeatWorldBoss,
					bTestCancelWorldBoss,
					bTestTimeoutWorldBoss,
					bTestWorldBossPlayerDeath,
					bTestScreenshotWorldBossUi,
					bTestScreenshotWorldBossBattle,
					bTestScreenshotWorldBossReward,
					bTestLogWorldBoss]
				{
					AImmortalMonsterSpawner* FixtureSpawner = FindMapSpawner();
					const FName BaselineMapId = FixtureSpawner
						? FixtureSpawner->GetActiveMapId() : NAME_None;
					const int32 BaselineStage = FixtureSpawner
						? FixtureSpawner->GetCurrentStage() : 0;
					const int32 BaselineStageKills = FixtureSpawner
						? FixtureSpawner->GetCurrentStageKills() : 0;
					const int32 BaselineCultivation = CurrentCultivation;
					const int32 BaselineGold = CurrentGold;
					const int32 BaselineSectRevision = SectState.Revision;
					UE_LOG(LogTemp, Display,
						TEXT("World Boss fixture baseline: map=%s stage=%d kills=%d cultivation=%d stones=%d sectRevision=%d"),
						*BaselineMapId.ToString(), BaselineStage, BaselineStageKills,
						BaselineCultivation, BaselineGold, BaselineSectRevision);
					const bool bNeedsBattle = bHasTestWorldBoss
						|| bTestWorldBossPhaseThree
						|| bTestDefeatWorldBoss
						|| bTestCancelWorldBoss
						|| bTestTimeoutWorldBoss
						|| bTestWorldBossPlayerDeath
						|| bTestScreenshotWorldBossBattle;
					if ((bTestOpenWorldBoss || bTestScreenshotWorldBossUi)
						&& PlayerWorldBossWidget && !bWorldBossOpen)
					{
						ToggleWorldBoss();
					}
					if (bNeedsBattle)
					{
						if (bWorldBossOpen)
						{
							ToggleWorldBoss();
						}
						const FImmortalWorldBossChallengeResult StartResult =
							StartWorldBossChallenge(FName(TestWorldBossId));
						UE_LOG(LogTemp, Display,
							TEXT("World Boss development start: id=%s success=%s message=%s"),
							*TestWorldBossId,
							StartResult.bSucceeded ? TEXT("true") : TEXT("false"),
							*StartResult.Message.ToString());
					}
					if (bTestWorldBossPhaseThree || bTestScreenshotWorldBossBattle)
					{
						FTimerHandle PhaseTimer;
						GetWorldTimerManager().SetTimer(
							PhaseTimer,
							FTimerDelegate::CreateWeakLambda(this, [this]
							{
								if (AImmortalMonsterSpawner* Spawner = FindMapSpawner())
								{
									Spawner->DamageActiveWorldBossForDevelopment(0.72f);
								}
							}),
							1.0f,
							false);
					}
					if (bTestDefeatWorldBoss)
					{
						FTimerHandle DefeatTimer;
						GetWorldTimerManager().SetTimer(
							DefeatTimer,
							FTimerDelegate::CreateWeakLambda(this, [this]
							{
								if (AImmortalMonsterSpawner* Spawner = FindMapSpawner())
								{
									Spawner->DefeatActiveWorldBossForDevelopment();
								}
							}),
							2.4f,
							false);
					}
					if (bTestCancelWorldBoss || bTestTimeoutWorldBoss
						|| bTestWorldBossPlayerDeath)
					{
						FTimerHandle EndEncounterTimer;
						GetWorldTimerManager().SetTimer(
							EndEncounterTimer,
							FTimerDelegate::CreateWeakLambda(
								this,
								[this,
									bTestTimeoutWorldBoss,
									bTestWorldBossPlayerDeath]
								{
									if (AImmortalMonsterSpawner* Spawner = FindMapSpawner())
									{
										if (bTestWorldBossPlayerDeath)
										{
											UGameplayStatics::ApplyDamage(
												this,
												GetMaxHealth() + 1000000.0f,
												nullptr,
												Spawner,
												UDamageType::StaticClass());
										}
										else if (bTestTimeoutWorldBoss)
										{
											Spawner->TimeoutActiveWorldBossForDevelopment();
										}
										else
										{
											Spawner->CancelWorldBossChallenge();
										}
									}
								}),
							1.35f,
							false);
					}
					if (bTestScreenshotWorldBossUi || bTestScreenshotWorldBossBattle)
					{
						FTimerHandle ScreenshotTimer;
						GetWorldTimerManager().SetTimer(
							ScreenshotTimer,
							FTimerDelegate::CreateWeakLambda(
								this,
								[bTestScreenshotWorldBossBattle]
								{
									const FString FileName = bTestScreenshotWorldBossBattle
										? TEXT("Screenshots/Step30_WorldBoss_Battle_Phase3.png")
										: TEXT("Screenshots/Step30_WorldBoss_UI.png");
									const FString ScreenshotPath = FPaths::Combine(
										FPaths::ProjectSavedDir(), FileName);
									FScreenshotRequest::RequestScreenshot(
										ScreenshotPath, true, false);
									UE_LOG(LogTemp, Display,
										TEXT("World Boss verification screenshot requested: %s"),
										*ScreenshotPath);
								}),
							bTestScreenshotWorldBossBattle ? 2.1f : 1.0f,
							false);
					}
					if (bTestScreenshotWorldBossReward)
					{
						FTimerHandle RewardScreenshotTimer;
						GetWorldTimerManager().SetTimer(
							RewardScreenshotTimer,
							FTimerDelegate::CreateWeakLambda(this, []
							{
								const FString ScreenshotPath = FPaths::Combine(
									FPaths::ProjectSavedDir(),
									TEXT("Screenshots/Step30_WorldBoss_Reward.png"));
								FScreenshotRequest::RequestScreenshot(
									ScreenshotPath, true, false);
								UE_LOG(LogTemp, Display,
									TEXT("World Boss reward screenshot requested: %s"),
									*ScreenshotPath);
							}),
							bTestDefeatWorldBoss ? 3.1f : 1.0f,
							false);
					}
					if (bTestDefeatWorldBoss || bTestCancelWorldBoss
						|| bTestTimeoutWorldBoss || bTestWorldBossPlayerDeath)
					{
						FTimerHandle AuditTimer;
						GetWorldTimerManager().SetTimer(
							AuditTimer,
							FTimerDelegate::CreateWeakLambda(
								this,
								[this,
									BaselineMapId,
									BaselineStage,
									BaselineStageKills,
									BaselineCultivation,
									BaselineGold,
									BaselineSectRevision]
								{
									const AImmortalMonsterSpawner* Spawner =
										FindMapSpawner();
									const bool bMapUnchanged = Spawner
										&& Spawner->GetActiveMapId() == BaselineMapId
										&& Spawner->GetCurrentStage() == BaselineStage
										&& Spawner->GetCurrentStageKills() == BaselineStageKills;
									UE_LOG(LogTemp, Display,
										TEXT("World Boss fixture invariant audit: mapUnchanged=%s cultivationDelta=%d(independent-training) sectUnchanged=%s active=%s stonesDelta=%d pending=%d"),
										bMapUnchanged ? TEXT("true") : TEXT("false"),
										CurrentCultivation - BaselineCultivation,
										SectState.Revision == BaselineSectRevision
											? TEXT("true") : TEXT("false"),
										Spawner && Spawner->IsWorldBossChallengeActive()
											? TEXT("true") : TEXT("false"),
										CurrentGold - BaselineGold,
										WorldBossState.PendingRewards.Num());
								}),
							bTestDefeatWorldBoss ? 2.55f : 1.50f,
							false);

						FTimerHandle ResumeAuditTimer;
						GetWorldTimerManager().SetTimer(
							ResumeAuditTimer,
							FTimerDelegate::CreateWeakLambda(this, [this]
							{
								const AImmortalMonsterSpawner* Spawner =
									FindMapSpawner();
								UE_LOG(LogTemp, Display,
									TEXT("World Boss fixture resume audit: active=%s normalMonsters=%d"),
									Spawner && Spawner->IsWorldBossChallengeActive()
										? TEXT("true") : TEXT("false"),
									Spawner ? Spawner->GetAliveMonsterCount() : -1);
							}),
							bTestDefeatWorldBoss ? 4.0f : 2.40f,
							false);
					}
					if (bTestLogWorldBoss)
					{
						const FImmortalWorldBossRuntimeSnapshot Runtime =
							GetWorldBossRuntimeSnapshot();
						UE_LOG(LogTemp, Display,
							TEXT("World Boss development state: initialized=%s bosses=%d pending=%d revision=%d active=%s id=%s phase=%d hp=%.0f/%.0f remaining=%.1f"),
							WorldBossState.bInitialized ? TEXT("true") : TEXT("false"),
							WorldBossState.BossProgress.Num(),
							WorldBossState.PendingRewards.Num(),
							WorldBossState.Revision,
							Runtime.bActive ? TEXT("true") : TEXT("false"),
							*Runtime.BossId.ToString(),
							Runtime.Phase,
							Runtime.CurrentHealth,
							Runtime.MaximumHealth,
							Runtime.RemainingSeconds);
						for (const FImmortalWorldBossProgress& Progress :
							WorldBossState.BossProgress)
						{
							UE_LOG(LogTemp, Display,
								TEXT("World Boss persistence: id=%s defeats=%d best=%.2f last=%lld firstArtifact=%s"),
								*Progress.BossId.ToString(),
								Progress.DefeatCount,
								Progress.BestClearSeconds,
								Progress.LastDefeatedUtcTicks,
								Progress.bFirstClearArtifactClaimed
									? TEXT("true") : TEXT("false"));
						}
					}
					if (FParse::Param(
						FCommandLine::Get(), TEXT("ImmortalTestExitAfterWorldBoss")))
					{
						FTimerHandle ExitTimer;
						GetWorldTimerManager().SetTimer(
							ExitTimer,
							FTimerDelegate::CreateWeakLambda(this, []
							{
								UE_LOG(LogTemp, Display,
									TEXT("World Boss runtime verification complete; requesting clean exit"));
								FPlatformMisc::RequestExit(false);
							}),
							5.0f,
							false);
					}
				}),
			0.80f,
			false);
	}
#endif

#if !UE_BUILD_SHIPPING
	int32 TestEndlessFloor = 0;
	const bool bHasTestEndlessFloor = FParse::Value(
		FCommandLine::Get(),
		TEXT("ImmortalTestEndlessFloor="),
		TestEndlessFloor);
	const bool bTestPrepareEndless =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestPrepareEndless"));
	const bool bTestStartEndless =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestStartEndless"));
	const bool bTestOpenEndless =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestOpenEndless"));
	const bool bTestEndlessPhaseThree =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestEndlessPhaseThree"));
	const bool bTestClearEndlessFloor =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestClearEndlessFloor"));
	const bool bTestFailEndless =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestFailEndless"));
	const bool bTestEndlessPlayerDeath =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestEndlessPlayerDeath"));
	const bool bTestScreenshotEndlessUi =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotEndlessUI"));
	const bool bTestScreenshotEndlessBattle =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotEndlessBattle"));
	const bool bTestScreenshotEndlessReward =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotEndlessReward"));
	const bool bTestLogEndless =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestLogEndless"));
	const bool bHasEndlessFixture =
		bHasTestEndlessFloor || bTestPrepareEndless || bTestStartEndless
		|| bTestOpenEndless || bTestEndlessPhaseThree
		|| bTestClearEndlessFloor || bTestFailEndless
		|| bTestEndlessPlayerDeath || bTestScreenshotEndlessUi
		|| bTestScreenshotEndlessBattle || bTestScreenshotEndlessReward
		|| bTestLogEndless;
	if (bHasEndlessFixture)
	{
		if (TestEndlessFloor < 1)
		{
			TestEndlessFloor =
				bTestEndlessPhaseThree || bTestScreenshotEndlessBattle
					|| bTestScreenshotEndlessReward
				? 10
				: 1;
		}
		const bool bNeedsEndlessBattle =
			bHasTestEndlessFloor || bTestStartEndless
			|| bTestEndlessPhaseThree || bTestClearEndlessFloor
			|| bTestFailEndless || bTestEndlessPlayerDeath
			|| bTestScreenshotEndlessBattle
			|| bTestScreenshotEndlessReward;
		const bool bShouldClearEndlessFloor =
			bTestClearEndlessFloor || bTestScreenshotEndlessReward;
		FTimerHandle EndlessFixtureTimer;
		GetWorldTimerManager().SetTimer(
			EndlessFixtureTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this,
					TestEndlessFloor,
					bTestPrepareEndless,
					bTestOpenEndless,
					bTestEndlessPhaseThree,
					bTestFailEndless,
					bTestEndlessPlayerDeath,
					bTestScreenshotEndlessUi,
					bTestScreenshotEndlessBattle,
					bTestScreenshotEndlessReward,
					bTestLogEndless,
					bNeedsEndlessBattle,
					bShouldClearEndlessFloor]
				{
					AImmortalMonsterSpawner* FixtureSpawner = FindMapSpawner();
					const FName BaselineMapId = FixtureSpawner
						? FixtureSpawner->GetActiveMapId() : NAME_None;
					const int32 BaselineStage = FixtureSpawner
						? FixtureSpawner->GetCurrentStage() : 0;
					const int32 BaselineStageKills = FixtureSpawner
						? FixtureSpawner->GetCurrentStageKills() : 0;
					const int32 BaselineMapRevision = FixtureSpawner
						? FixtureSpawner->GetMapRevision() : 0;
					const int32 BaselineCultivation = CurrentCultivation;
					const int32 BaselineGold = CurrentGold;
					const int32 BaselineSectRevision = SectState.Revision;
					const int32 BaselineInventoryItems = InventoryItems.Num();
					const int32 BaselineEquipmentDrops = EquipmentDropCount;
					UE_LOG(
						LogTemp,
						Display,
						TEXT("Endless Dungeon fixture baseline: map=%s stage=%d kills=%d mapRevision=%d cultivation=%d stones=%d sectRevision=%d inventory=%d equipmentDrops=%d"),
						*BaselineMapId.ToString(),
						BaselineStage,
						BaselineStageKills,
						BaselineMapRevision,
						BaselineCultivation,
						BaselineGold,
						BaselineSectRevision,
						BaselineInventoryItems,
						BaselineEquipmentDrops);

					if (bTestPrepareEndless)
					{
						EndlessDungeonState =
							UImmortalEndlessDungeonLibrary::CreateDefaultState();
						EndlessDungeonState.HighestClearedFloor =
							FMath::Max(TestEndlessFloor - 1, 0);
						EndlessDungeonState.TotalFloorsCleared =
							EndlessDungeonState.HighestClearedFloor;
						UImmortalEndlessDungeonLibrary::NormalizeState(
							EndlessDungeonState);
						const bool bPreparedSave = SaveProgress();
						UE_LOG(
							LogTemp,
							Display,
							TEXT("Endless Dungeon fixture prepared: targetFloor=%d highest=%d checkpoint=%d saved=%s"),
							TestEndlessFloor,
							EndlessDungeonState.HighestClearedFloor,
							UImmortalEndlessDungeonLibrary::GetCheckpointStartFloor(
								EndlessDungeonState),
							bPreparedSave ? TEXT("true") : TEXT("false"));
					}

					if ((bTestOpenEndless || bTestScreenshotEndlessUi)
						&& PlayerEndlessDungeonWidget && !bEndlessDungeonOpen)
					{
						ToggleEndlessDungeon();
					}

					bool bStarted = false;
					if (bNeedsEndlessBattle)
					{
						if (bEndlessDungeonOpen)
						{
							ToggleEndlessDungeon();
						}
						const FImmortalEndlessDungeonStartResult StartResult =
							StartEndlessDungeon(TestEndlessFloor);
						bStarted = StartResult.bSucceeded;
						UE_LOG(
							LogTemp,
							Display,
							TEXT("Endless Dungeon development start: floor=%d success=%s message=%s"),
							TestEndlessFloor,
							StartResult.bSucceeded ? TEXT("true") : TEXT("false"),
							*StartResult.Message.ToString());
						if (FixtureSpawner && bStarted)
						{
							const FImmortalMapTravelResult TravelProbe =
								FixtureSpawner->TravelToMap(BaselineMapId);
							const FImmortalWorldBossChallengeResult BossProbe =
								FixtureSpawner->StartWorldBossChallenge(
									TEXT("AzureScaleDragon"));
							UE_LOG(
								LogTemp,
								Display,
								TEXT("Endless Dungeon mutual exclusion audit: mapTravelBlocked=%s worldBossBlocked=%s mapMessage=%s bossMessage=%s"),
								TravelProbe.bSucceeded
									? TEXT("false") : TEXT("true"),
								BossProbe.bSucceeded
									? TEXT("false") : TEXT("true"),
								*TravelProbe.Message.ToString(),
								*BossProbe.Message.ToString());
						}
					}

					if (bStarted
						&& (bTestEndlessPhaseThree
							|| bTestScreenshotEndlessBattle))
					{
						FTimerHandle PhaseTimer;
						GetWorldTimerManager().SetTimer(
							PhaseTimer,
							FTimerDelegate::CreateWeakLambda(this, [this]
							{
								if (AImmortalMonsterSpawner* Spawner =
									FindMapSpawner())
								{
									const bool bDamaged =
										Spawner
											->DamageActiveEndlessBossForDevelopment(
												0.72f);
									UE_LOG(
										LogTemp,
										Display,
										TEXT("Endless Dungeon phase-three fixture damage applied=%s"),
										bDamaged ? TEXT("true") : TEXT("false"));
								}
							}),
							1.0f,
							false);
					}

					if (bStarted && bShouldClearEndlessFloor)
					{
						FTimerHandle ClearTimer;
						GetWorldTimerManager().SetTimer(
							ClearTimer,
							FTimerDelegate::CreateWeakLambda(this, [this]
							{
								if (AImmortalMonsterSpawner* Spawner =
									FindMapSpawner())
								{
									const bool bCleared =
										Spawner
											->ClearActiveEndlessFloorForDevelopment();
									UE_LOG(
										LogTemp,
										Display,
										TEXT("Endless Dungeon development clear issued: success=%s"),
										bCleared ? TEXT("true") : TEXT("false"));
								}
							}),
							2.35f,
							false);
					}

					if (bStarted
						&& (bTestFailEndless || bTestEndlessPlayerDeath))
					{
						FTimerHandle FailureTimer;
						GetWorldTimerManager().SetTimer(
							FailureTimer,
							FTimerDelegate::CreateWeakLambda(
								this,
								[this, bTestEndlessPlayerDeath]
								{
									if (AImmortalMonsterSpawner* Spawner =
										FindMapSpawner())
									{
										if (bTestEndlessPlayerDeath)
										{
											UGameplayStatics::ApplyDamage(
												this,
												GetMaxHealth() + 1000000.0f,
												nullptr,
												Spawner,
												UDamageType::StaticClass());
										}
										else
										{
											Spawner
												->FailActiveEndlessDungeonForDevelopment();
										}
									}
								}),
							1.35f,
							false);
					}

					if (bTestScreenshotEndlessUi
						|| bTestScreenshotEndlessBattle)
					{
						FTimerHandle ScreenshotTimer;
						GetWorldTimerManager().SetTimer(
							ScreenshotTimer,
							FTimerDelegate::CreateWeakLambda(
								this,
								[bTestScreenshotEndlessBattle]
								{
									const FString FileName =
										bTestScreenshotEndlessBattle
										? TEXT("Screenshots/Step31_EndlessDungeon_Battle_BossPhase3.png")
										: TEXT("Screenshots/Step31_EndlessDungeon_UI.png");
									const FString ScreenshotPath =
										FPaths::Combine(
											FPaths::ProjectSavedDir(),
											FileName);
									FScreenshotRequest::RequestScreenshot(
										ScreenshotPath,
										true,
										false);
									UE_LOG(
										LogTemp,
										Display,
										TEXT("Endless Dungeon verification screenshot requested: %s"),
										*ScreenshotPath);
								}),
							bTestScreenshotEndlessBattle ? 1.85f : 1.0f,
							false);
					}

					if (bTestScreenshotEndlessReward)
					{
						FTimerHandle RewardScreenshotTimer;
						GetWorldTimerManager().SetTimer(
							RewardScreenshotTimer,
							FTimerDelegate::CreateWeakLambda(this, []
							{
								const FString ScreenshotPath =
									FPaths::Combine(
										FPaths::ProjectSavedDir(),
										TEXT("Screenshots/Step31_EndlessDungeon_Reward.png"));
								FScreenshotRequest::RequestScreenshot(
									ScreenshotPath,
									true,
									false);
								UE_LOG(
									LogTemp,
									Display,
									TEXT("Endless Dungeon reward screenshot requested: %s"),
									*ScreenshotPath);
							}),
							2.85f,
							false);
					}

					if (bStarted && !bTestFailEndless
						&& !bTestEndlessPlayerDeath)
					{
						FTimerHandle CleanupTimer;
						GetWorldTimerManager().SetTimer(
							CleanupTimer,
							FTimerDelegate::CreateWeakLambda(this, [this]
							{
								if (AImmortalMonsterSpawner* Spawner =
									FindMapSpawner();
									Spawner
										&& Spawner->IsEndlessDungeonActive())
								{
									Spawner->CancelEndlessDungeon();
								}
							}),
							bTestScreenshotEndlessReward ? 3.65f : 2.65f,
							false);
					}

					if (bNeedsEndlessBattle || bTestLogEndless)
					{
						FTimerHandle RuntimeAuditTimer;
						GetWorldTimerManager().SetTimer(
							RuntimeAuditTimer,
							FTimerDelegate::CreateWeakLambda(this, [this]
							{
								const FImmortalEndlessDungeonRuntimeSnapshot Runtime =
									GetEndlessDungeonRuntimeSnapshot();
								UE_LOG(
									LogTemp,
									Display,
									TEXT("Endless Dungeon runtime audit: active=%s floor=%d kind=%s kills=%d/%d phase=%d hp=%.0f/%.0f elapsed=%.2f"),
									Runtime.bActive ? TEXT("true") : TEXT("false"),
									Runtime.Floor,
									Runtime.bBoss
										? TEXT("boss")
										: (Runtime.bElite
											? TEXT("elite") : TEXT("normal")),
									Runtime.Kills,
									Runtime.RequiredKills,
									Runtime.BossPhase,
									Runtime.CurrentHealth,
									Runtime.MaximumHealth,
									Runtime.ElapsedSeconds);
							}),
							1.95f,
							false);

						FTimerHandle InvariantAuditTimer;
						GetWorldTimerManager().SetTimer(
							InvariantAuditTimer,
							FTimerDelegate::CreateWeakLambda(
								this,
								[this,
									BaselineMapId,
									BaselineStage,
									BaselineStageKills,
									BaselineMapRevision,
									BaselineCultivation,
									BaselineGold,
									BaselineSectRevision,
									BaselineInventoryItems,
									BaselineEquipmentDrops]
								{
									const AImmortalMonsterSpawner* Spawner =
										FindMapSpawner();
									const bool bMapUnchanged = Spawner
										&& Spawner->GetActiveMapId()
											== BaselineMapId
										&& Spawner->GetCurrentStage()
											== BaselineStage
										&& Spawner->GetCurrentStageKills()
											== BaselineStageKills
										&& Spawner->GetMapRevision()
											== BaselineMapRevision;
									UE_LOG(
										LogTemp,
										Display,
										TEXT("Endless Dungeon invariant audit: mapUnchanged=%s cultivationDelta=%d(independent-training) sectUnchanged=%s active=%s stonesDelta=%d inventoryDelta=%d equipmentDropDelta=%d highest=%d totalFloors=%lld runs=%d pending=%d revision=%d"),
										bMapUnchanged
											? TEXT("true") : TEXT("false"),
										CurrentCultivation
											- BaselineCultivation,
										SectState.Revision
												== BaselineSectRevision
											? TEXT("true") : TEXT("false"),
										Spawner
												&& Spawner
													->IsEndlessDungeonActive()
											? TEXT("true") : TEXT("false"),
										CurrentGold - BaselineGold,
										InventoryItems.Num()
											- BaselineInventoryItems,
										EquipmentDropCount
											- BaselineEquipmentDrops,
										EndlessDungeonState
											.HighestClearedFloor,
										EndlessDungeonState
											.TotalFloorsCleared,
										EndlessDungeonState.TotalRuns,
										EndlessDungeonState
											.PendingRewards.Num(),
										EndlessDungeonState.Revision);
								}),
							bTestScreenshotEndlessReward ? 4.20f : 3.20f,
							false);
					}

					if (FParse::Param(
						FCommandLine::Get(),
						TEXT("ImmortalTestExitAfterEndless")))
					{
						FTimerHandle ExitTimer;
						GetWorldTimerManager().SetTimer(
							ExitTimer,
							FTimerDelegate::CreateWeakLambda(this, []
							{
								UE_LOG(
									LogTemp,
									Display,
									TEXT("Endless Dungeon runtime verification complete; requesting clean exit"));
								FPlatformMisc::RequestExit(false);
							}),
							6.20f,
							false);
					}
				}),
			0.85f,
			false);
	}
#endif

#if !UE_BUILD_SHIPPING
	FString TestPetValue;
	const bool bHasTestPet = FParse::Value(
		FCommandLine::Get(), TEXT("ImmortalTestPet="), TestPetValue);
	const bool bTestGrantPetResources =
		FParse::Param(
			FCommandLine::Get(), TEXT("ImmortalTestGrantPetResources"));
	const bool bTestUnlockPet =
		FParse::Param(
			FCommandLine::Get(), TEXT("ImmortalTestUnlockPet"));
	const bool bTestRaisePetStar =
		FParse::Param(
			FCommandLine::Get(), TEXT("ImmortalTestRaisePetStar"));
	const bool bTestOpenPet =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestOpenPet"));
	const bool bTestPetForceKill =
		FParse::Param(
			FCommandLine::Get(), TEXT("ImmortalTestPetForceKill"));
	const bool bTestPetOwnerDeath =
		FParse::Param(
			FCommandLine::Get(), TEXT("ImmortalTestPetOwnerDeath"));
	const bool bTestPetPreviewHurt =
		FParse::Param(
			FCommandLine::Get(), TEXT("ImmortalTestPetPreviewHurt"));
	const bool bTestPetPreviewDeath =
		FParse::Param(
			FCommandLine::Get(), TEXT("ImmortalTestPetPreviewDeath"));
	const bool bTestScreenshotPetUi =
		FParse::Param(
			FCommandLine::Get(), TEXT("ImmortalTestScreenshotPetUI"));
	const bool bTestScreenshotPetBattle =
		FParse::Param(
			FCommandLine::Get(), TEXT("ImmortalTestScreenshotPetBattle"));
	const bool bTestScreenshotPetNotification =
		FParse::Param(
			FCommandLine::Get(),
			TEXT("ImmortalTestScreenshotPetNotification"));
	const bool bTestLogPet =
		FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestLogPet"));
	const bool bTestSavePetFixture =
		FParse::Param(
			FCommandLine::Get(), TEXT("ImmortalTestSavePetFixture"));
	const bool bHasPetFixture =
		bHasTestPet || bTestGrantPetResources || bTestUnlockPet
		|| bTestRaisePetStar || bTestOpenPet || bTestPetForceKill
		|| bTestPetOwnerDeath || bTestPetPreviewHurt
		|| bTestPetPreviewDeath || bTestScreenshotPetUi
		|| bTestScreenshotPetBattle
		|| bTestScreenshotPetNotification || bTestLogPet
		|| bTestSavePetFixture;
	if (bHasPetFixture)
	{
		FTimerHandle PetFixtureTimer;
		GetWorldTimerManager().SetTimer(
			PetFixtureTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this,
					TestPetValue,
					bHasTestPet,
					bTestGrantPetResources,
					bTestUnlockPet,
					bTestRaisePetStar,
					bTestOpenPet,
					bTestPetForceKill,
					bTestPetOwnerDeath,
					bTestPetPreviewHurt,
					bTestPetPreviewDeath,
					bTestScreenshotPetUi,
					bTestScreenshotPetBattle,
					bTestScreenshotPetNotification,
					bTestLogPet,
					bTestSavePetFixture]
				{
					FName RequestedPetId = PetState.ActivePetId;
					if (bHasTestPet)
					{
						if (TestPetValue.Equals(
							TEXT("Fox"), ESearchCase::IgnoreCase)
							|| TestPetValue.Equals(
								TEXT("SpiritFox"),
								ESearchCase::IgnoreCase))
						{
							RequestedPetId = TEXT("SpiritFox");
						}
						else if (TestPetValue.Equals(
							TEXT("Dog"), ESearchCase::IgnoreCase)
							|| TestPetValue.Equals(
								TEXT("Hound"), ESearchCase::IgnoreCase)
							|| TestPetValue.Equals(
								TEXT("SpiritHound"),
								ESearchCase::IgnoreCase))
						{
							RequestedPetId = TEXT("SpiritHound");
						}
						else
						{
							RequestedPetId = FName(*TestPetValue);
						}
					}

					AImmortalMonsterSpawner* FixtureSpawner =
						FindMapSpawner();
					const FName BaselineMapId = FixtureSpawner
						? FixtureSpawner->GetActiveMapId() : NAME_None;
					const int32 BaselineStage = FixtureSpawner
						? FixtureSpawner->GetCurrentStage() : 0;
					const int32 BaselineStageKills = FixtureSpawner
						? FixtureSpawner->GetCurrentStageKills() : 0;
					const int32 BaselineMapRevision = FixtureSpawner
						? FixtureSpawner->GetMapRevision() : 0;
					const int32 BaselineCultivation =
						CurrentCultivation;
					const int32 BaselineGold = CurrentGold;
					const int32 BaselineInventory =
						InventoryItems.Num();
					const int64 BaselinePetKills =
						PetState.TotalCombatKills;
					const int32 BaselinePetRevision =
						PetState.Revision;
					UE_LOG(LogTemp, Display,
						TEXT("Pet fixture baseline: requested=%s active=%s actor=%s map=%s stage=%d stageKills=%d mapRevision=%d cultivation=%d stones=%d inventory=%d petKills=%lld petRevision=%d"),
						*RequestedPetId.ToString(),
						*PetState.ActivePetId.ToString(),
						*GetNameSafe(ActivePetActor),
						*BaselineMapId.ToString(),
						BaselineStage,
						BaselineStageKills,
						BaselineMapRevision,
						BaselineCultivation,
						BaselineGold,
						BaselineInventory,
						BaselinePetKills,
						BaselinePetRevision);

					if (bTestGrantPetResources)
					{
						const TArray<FImmortalMaterialStack>
							PreviousMaterials = MaterialInventory;
						const int32 AddedCores =
							AddMaterialInternal(TEXT("DemonCore"), 100);
						CurrentGold = static_cast<int32>(
							FMath::Min<int64>(
								static_cast<int64>(CurrentGold)
									+ 10000,
								MAX_int32));
						PublishMaterialInventoryDiff(
							PreviousMaterials);
						UE_LOG(LogTemp, Display,
							TEXT("Pet fixture resources granted: demonCore=+%d stones=%d"),
							AddedCores,
							CurrentGold);
					}

					FImmortalPetProgress RequestedProgress;
					bool bHasProgress = GetPetProgress(
						RequestedPetId, RequestedProgress);
					if (bHasProgress && !RequestedProgress.bOwned
						&& (bTestUnlockPet
							|| bTestGrantPetResources))
					{
						const FImmortalPetOperationResult Unlock =
							UnlockPet(RequestedPetId);
						UE_LOG(LogTemp, Display,
							TEXT("Pet fixture unlock: id=%s success=%s affordable=%s message=%s"),
							*RequestedPetId.ToString(),
							Unlock.bSucceeded
								? TEXT("true") : TEXT("false"),
							Unlock.bAffordable
								? TEXT("true") : TEXT("false"),
							*Unlock.Message.ToString());
						bHasProgress = GetPetProgress(
							RequestedPetId, RequestedProgress);
					}
					if (bHasProgress && RequestedProgress.bOwned
						&& PetState.ActivePetId
							!= RequestedPetId)
					{
						const FImmortalPetOperationResult Equip =
							SetActivePet(RequestedPetId);
						UE_LOG(LogTemp, Display,
							TEXT("Pet fixture equip: id=%s success=%s message=%s"),
							*RequestedPetId.ToString(),
							Equip.bSucceeded
								? TEXT("true") : TEXT("false"),
							*Equip.Message.ToString());
					}
					if (bTestRaisePetStar)
					{
						const FImmortalPetOperationResult Star =
							RaisePetStar(RequestedPetId);
						UE_LOG(LogTemp, Display,
							TEXT("Pet fixture star: id=%s success=%s affordable=%s message=%s"),
							*RequestedPetId.ToString(),
							Star.bSucceeded
								? TEXT("true") : TEXT("false"),
							Star.bAffordable
								? TEXT("true") : TEXT("false"),
							*Star.Message.ToString());
					}

					if ((bTestOpenPet || bTestScreenshotPetUi)
						&& PlayerPetWidget && !bPetOpen)
					{
						TogglePet();
					}
					if (bTestScreenshotPetNotification
						&& bPetOpen)
					{
						TogglePet();
					}
					if (bTestScreenshotPetNotification
						&& CombatFeedbackWidget)
					{
						FImmortalPetDefinition NoticeDefinition;
						UImmortalPetLibrary::GetPetDefinition(
							PetState.ActivePetId,
							NoticeDefinition);
						CombatFeedbackWidget->ShowBossAnnouncement(
							FText::FromString(FString::Printf(
								TEXT("%s已出战，将自动跟随并协助攻击"),
								*NoticeDefinition.DisplayName.ToString())),
							NoticeDefinition.DisplayColor,
							5.0f);
					}

					if (bTestPetPreviewHurt
						&& ActivePetActor)
					{
						ActivePetActor
							->PreviewHurtForDevelopment();
					}
					if (bTestPetPreviewDeath
						&& ActivePetActor)
					{
						ActivePetActor
							->PreviewDeathForDevelopment();
					}

					if (bTestPetOwnerDeath)
					{
						FTimerHandle OwnerDeathTimer;
						GetWorldTimerManager().SetTimer(
							OwnerDeathTimer,
							FTimerDelegate::CreateWeakLambda(
								this,
								[this]
								{
									UGameplayStatics::ApplyDamage(
										this,
										GetMaxHealth()
											+ 1000000.0f,
										nullptr,
										ActivePetActor,
										UDamageType::StaticClass());
									UE_LOG(LogTemp, Display,
										TEXT("Pet owner-death fixture applied: playerDead=%s pet=%s"),
										bDead
											? TEXT("true")
											: TEXT("false"),
										*GetNameSafe(
											ActivePetActor));
								}),
							1.20f,
							false);
					}

					if (bTestPetForceKill && FixtureSpawner)
					{
						AImmortalMonsterCharacter* Target =
							FixtureSpawner->SpawnMonster();
						if (!Target)
						{
							for (TActorIterator<
								AImmortalMonsterCharacter> It(
									GetWorld());
								It;
								++It)
							{
								if (!It->IsDead())
								{
									Target = *It;
									break;
								}
							}
						}
						const TWeakObjectPtr<
							AImmortalMonsterCharacter>
							WeakTarget(Target);
						FTimerHandle LethalTimer;
						GetWorldTimerManager().SetTimer(
							LethalTimer,
							FTimerDelegate::CreateWeakLambda(
								this,
								[this, WeakTarget]
								{
									AImmortalMonsterCharacter*
										LiveTarget =
											WeakTarget.Get();
									if (!LiveTarget
										|| LiveTarget->IsDead()
										|| !ActivePetActor)
									{
										UE_LOG(LogTemp, Error,
											TEXT("Pet lethal fixture could not find a live target"));
										return;
									}
									LiveTarget->SetActorLocation(
										ActivePetActor
											->GetActorLocation()
											+ FVector(
												120.0f,
												0.0f,
												0.0f),
										false,
										nullptr,
										ETeleportType
											::TeleportPhysics);
									const float Applied =
										ResolvePetAttack(
											ActivePetActor,
											LiveTarget,
											1000000000.0f);
									UE_LOG(LogTemp, Display,
										TEXT("Pet lethal fixture issued: target=%s applied=%.1f dead=%s"),
										*GetNameSafe(
											LiveTarget),
										Applied,
										LiveTarget->IsDead()
											? TEXT("true")
											: TEXT("false"));
								}),
							2.20f,
							false);
					}

					if (bTestScreenshotPetUi
						|| bTestScreenshotPetBattle
						|| bTestScreenshotPetNotification)
					{
						FTimerHandle ScreenshotTimer;
						GetWorldTimerManager().SetTimer(
							ScreenshotTimer,
							FTimerDelegate::CreateWeakLambda(
								this,
								[bTestScreenshotPetUi,
									bTestScreenshotPetBattle,
									RequestedPetId]
								{
									FString FileName;
									if (bTestScreenshotPetUi)
									{
										FileName =
											TEXT("Screenshots/Step32_Pet_UI.png");
									}
									else if (
										bTestScreenshotPetBattle)
									{
										FileName =
											RequestedPetId
												== FName(
													TEXT("SpiritHound"))
											? TEXT("Screenshots/Step32_Pet_Battle_Dog.png")
											: TEXT("Screenshots/Step32_Pet_Battle_Fox.png");
									}
									else
									{
										FileName =
											TEXT("Screenshots/Step32_Pet_Notification.png");
									}
									const FString Path =
										FPaths::Combine(
											FPaths::ProjectSavedDir(),
											FileName);
									FScreenshotRequest
										::RequestScreenshot(
											Path,
											true,
											false);
									UE_LOG(LogTemp, Display,
										TEXT("Pet verification screenshot requested: %s"),
										*Path);
								}),
							bTestScreenshotPetBattle
								? 1.80f : 1.0f,
							false);
					}

					if (bTestSavePetFixture)
					{
						const bool bSaved = SaveProgress();
						UE_LOG(LogTemp, Display,
							TEXT("Pet fixture explicit save: success=%s version=%d marker=%s"),
							bSaved ? TEXT("true") : TEXT("false"),
							UImmortalPathSaveGame
								::CurrentSaveVersion,
							PetState.bInitialized
								? TEXT("true")
								: TEXT("false"));
					}

					if (bTestLogPet || bTestPetForceKill
						|| bTestPetOwnerDeath)
					{
						FTimerHandle AuditTimer;
						GetWorldTimerManager().SetTimer(
							AuditTimer,
							FTimerDelegate::CreateWeakLambda(
								this,
								[this,
									BaselineStage,
									BaselineStageKills,
									BaselineMapRevision,
									BaselineCultivation,
									BaselineGold,
									BaselineInventory,
									BaselinePetKills,
									BaselinePetRevision]
								{
									const AImmortalMonsterSpawner*
										Spawner = FindMapSpawner();
									FImmortalPetProgress ActiveProgress;
									GetPetProgress(
										PetState.ActivePetId,
										ActiveProgress);
									UE_LOG(LogTemp, Display,
										TEXT("Pet runtime audit: active=%s actor=%s level=%d exp=%d/%d stars=%d attacks=%d hits=%d petKillDelta=%lld petRevisionDelta=%d stage=%d->%d stageKills=%d->%d mapRevisionDelta=%d cultivationDelta=%d(independent-training-only) stonesDelta=%d inventoryDelta=%d playerDead=%s"),
										*PetState.ActivePetId
											.ToString(),
										*GetNameSafe(
											ActivePetActor),
										ActiveProgress.Level,
										ActiveProgress.Experience,
										UImmortalPetLibrary
											::GetExperienceRequiredForLevel(
												ActiveProgress.Level),
										ActiveProgress.Stars,
										ActivePetActor
											? ActivePetActor
												->GetAttackCount()
											: 0,
										ActivePetActor
											? ActivePetActor
												->GetHitCount()
											: 0,
										PetState.TotalCombatKills
											- BaselinePetKills,
										PetState.Revision
											- BaselinePetRevision,
										BaselineStage,
										Spawner
											? Spawner
												->GetCurrentStage()
											: 0,
										BaselineStageKills,
										Spawner
											? Spawner
												->GetCurrentStageKills()
											: 0,
										Spawner
											? Spawner
												->GetMapRevision()
												- BaselineMapRevision
											: 0,
										CurrentCultivation
											- BaselineCultivation,
										CurrentGold
											- BaselineGold,
										InventoryItems.Num()
											- BaselineInventory,
										bDead
											? TEXT("true")
											: TEXT("false"));
								}),
							3.35f,
							false);
					}

					if (FParse::Param(
						FCommandLine::Get(),
						TEXT("ImmortalTestExitAfterPet")))
					{
						FTimerHandle ExitTimer;
						GetWorldTimerManager().SetTimer(
							ExitTimer,
							FTimerDelegate::CreateWeakLambda(
								this,
								[]
								{
									UE_LOG(LogTemp, Display,
										TEXT("Pet runtime verification complete; requesting clean exit"));
									FPlatformMisc::RequestExit(
										false);
								}),
							5.80f,
							false);
					}
				}),
			0.90f,
			false);
	}
#endif

#if !UE_BUILD_SHIPPING
	const bool bTestOpenAscension =
		FParse::Param(
			FCommandLine::Get(),
			TEXT("ImmortalTestOpenAscension"));
	const bool bTestPrepareAscension =
		FParse::Param(
			FCommandLine::Get(),
			TEXT("ImmortalTestPrepareAscension"));
	const bool bTestPerformAscension =
		FParse::Param(
			FCommandLine::Get(),
			TEXT("ImmortalTestPerformAscension"));
	const bool bTestLogAscension =
		FParse::Param(
			FCommandLine::Get(),
			TEXT("ImmortalTestLogAscension"));
	const bool bTestScreenshotAscension =
		FParse::Param(
			FCommandLine::Get(),
			TEXT("ImmortalTestScreenshotAscension"));
	const bool bTestScreenshotAscensionStatus =
		FParse::Param(
			FCommandLine::Get(),
			TEXT("ImmortalTestScreenshotAscensionStatus"));
	const bool bTestSaveAscensionFixture =
		FParse::Param(
			FCommandLine::Get(),
			TEXT("ImmortalTestSaveAscensionFixture"));
	FString TestAscensionPath;
	const bool bTestInvestAscension =
		FParse::Value(
			FCommandLine::Get(),
			TEXT("ImmortalTestInvestAscension="),
			TestAscensionPath);
	if (bTestOpenAscension || bTestPrepareAscension
		|| bTestPerformAscension || bTestLogAscension
		|| bTestScreenshotAscension
		|| bTestScreenshotAscensionStatus
		|| bTestSaveAscensionFixture
		|| bTestInvestAscension)
	{
		FTimerHandle AscensionFixtureTimer;
		GetWorldTimerManager().SetTimer(
			AscensionFixtureTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this,
					bTestOpenAscension,
					bTestPrepareAscension,
					bTestPerformAscension,
					bTestLogAscension,
					bTestScreenshotAscension,
					bTestScreenshotAscensionStatus,
					bTestSaveAscensionFixture,
					bTestInvestAscension,
					TestAscensionPath]
				{
					AImmortalMonsterSpawner* Spawner =
						FindMapSpawner();
					if (bTestPrepareAscension && Spawner)
					{
						if (Spawner->GetActiveMapId()
							!= UImmortalMapLibrary
								::GetQingyunMountainId())
						{
							Spawner->TravelToMap(
								UImmortalMapLibrary
									::GetQingyunMountainId());
						}
						if (CultivationComponent)
						{
							CultivationComponent
								->InitializeProgress(
									EImmortalCultivationRealm
										::Ascension,
									1,
									0);
							CurrentCultivation = 0;
							CurrentHealth = GetMaxHealth();
							CurrentMana = GetMaxMana();
						}
						int32 PreparedMapCount = 0;
						for (const FName MapId :
							UImmortalMapLibrary
								::GetKnownMapIds())
						{
							PreparedMapCount +=
								Spawner
									->CompleteMapForDevelopment(
										MapId)
									? 1
									: 0;
						}
						FImmortalMapProgress FinalMapProgress;
						const bool bFinalMapPrepared =
							Spawner->GetProgressForMap(
								UImmortalMapLibrary
									::GetImmortalPalaceRuinsId(),
								FinalMapProgress)
							&& FinalMapProgress.bCompleted;
						const bool bPreparedSaved =
							SaveProgress();
						UE_LOG(LogTemp, Display,
							TEXT("Ascension fixture prepared: completedMaps=%d finalMap=%s realm=%s activeMap=%s save=%s"),
							PreparedMapCount,
							bFinalMapPrepared
								? TEXT("true")
								: TEXT("false"),
							*GetFullCultivationRealmName()
								.ToString(),
							*GetActiveMapId().ToString(),
							bPreparedSaved
								? TEXT("true")
								: TEXT("false"));
					}

					if (bTestPerformAscension
						|| bTestLogAscension
						|| bTestInvestAscension)
					{
						// Freeze ordinary map settlement while the exact
						// reset/preserve matrix is audited.
						StopAutoAttack();
						DespawnActivePetActor();
					}

					const FImmortalAscensionState
						BaselineAscension = AscensionState;
					const int32 BaselineGold = CurrentGold;
					const int32 BaselineInventory =
						InventoryItems.Num();
					const int32 BaselineEquipment =
						EquippedItems.Num();
					const int32 BaselineMaterials =
						MaterialInventory.Num();
					const FImmortalMapSystemState BaselineMaps =
						GetMapSystemState();
					const FImmortalCaveState BaselineCave =
						CaveState;
					const FImmortalFarmingState BaselineFarming =
						FarmingState;
					const FImmortalSectState BaselineSect =
						SectState;
					const FImmortalPetState BaselinePet =
						PetState;
					const FImmortalWorldBossState BaselineBoss =
						WorldBossState;
					const FImmortalEndlessDungeonState
						BaselineEndless = EndlessDungeonState;

					if (bTestOpenAscension
						|| bTestScreenshotAscension)
					{
						if (!bAscensionOpen)
						{
							ToggleAscension();
						}
					}

					if (bTestPerformAscension)
					{
						const FImmortalAscensionOperationResult
							Result = PerformAscension();
						UE_LOG(LogTemp, Display,
							TEXT("Ascension development perform: success=%s count=%d sealsGranted=%d seals=%d message=%s"),
							Result.bSucceeded
								? TEXT("true")
								: TEXT("false"),
							Result.AscensionCount,
							Result.ImmortalSealsGranted,
							Result.ImmortalSeals,
							*Result.Message.ToString());
					}

					if (bTestInvestAscension)
					{
						EImmortalAscensionPath Path =
							EImmortalAscensionPath::Battle;
						if (TestAscensionPath.Equals(
							TEXT("Enlightenment"),
							ESearchCase::IgnoreCase))
						{
							Path = EImmortalAscensionPath
								::Enlightenment;
						}
						else if (TestAscensionPath.Equals(
							TEXT("Fortune"),
							ESearchCase::IgnoreCase))
						{
							Path = EImmortalAscensionPath
								::Fortune;
						}
						const FImmortalAscensionPathResult
							Result = InvestAscensionPath(Path);
						UE_LOG(LogTemp, Display,
							TEXT("Ascension development invest: path=%s success=%s rank=%d cost=%d seals=%d message=%s"),
							*TestAscensionPath,
							Result.bSucceeded
								? TEXT("true")
								: TEXT("false"),
							Result.CurrentRank,
							Result.ImmortalSealsSpent,
							Result.ImmortalSeals,
							*Result.Message.ToString());
					}

					if (bTestPerformAscension)
					{
						const bool bCommitted =
							AscensionState.AscensionCount
								> BaselineAscension
									.AscensionCount;
						const bool bCycleInvariant =
							bCommitted
								? IsAscensionCycleStartState(
									GetMapSystemState())
								: HaveSameMapProgress(
									GetMapSystemState(),
									BaselineMaps);
						const bool bLifetimeInvariant =
							bCommitted
								? HaveMergedLifetimeMapRecords(
									BaselineAscension,
									AscensionState,
									BaselineMaps)
								: HaveSameLifetimeMapRecords(
									AscensionState,
									BaselineAscension);
						const bool bImmediatePreserved =
							CurrentGold == BaselineGold
							&& InventoryItems.Num()
								== BaselineInventory
							&& EquippedItems.Num()
								== BaselineEquipment
							&& MaterialInventory.Num()
								== BaselineMaterials
							&& bCycleInvariant
							&& bLifetimeInvariant
							&& CaveState.Revision
								>= BaselineCave.Revision
							&& FarmingState.Revision
								>= BaselineFarming.Revision
							&& SectState.SectId
								== BaselineSect.SectId
							&& PetState.ActivePetId
								== BaselinePet.ActivePetId
							&& WorldBossState.BossProgress.Num()
								== BaselineBoss.BossProgress.Num()
							&& EndlessDungeonState
								.HighestClearedFloor
								== BaselineEndless
									.HighestClearedFloor;
						UE_LOG(LogTemp, Display,
							TEXT("Ascension immediate preservation audit: preserved=%s committed=%s mapInvariant=%s lifetimeInvariant=%s stones=%d inventory=%d equipped=%d materials=%d maps=%d caveRevision=%d farmingRevision=%d sect=%s pet=%s bossRecords=%d endlessHighest=%d"),
							bImmediatePreserved
								? TEXT("true")
								: TEXT("false"),
							bCommitted
								? TEXT("true")
								: TEXT("false"),
							bCycleInvariant
								? TEXT("true")
								: TEXT("false"),
							bLifetimeInvariant
								? TEXT("true")
								: TEXT("false"),
							CurrentGold,
							InventoryItems.Num(),
							EquippedItems.Num(),
							MaterialInventory.Num(),
							GetMapSystemState()
								.MapProgress.Num(),
							CaveState.Revision,
							FarmingState.Revision,
							*SectState.SectId.ToString(),
							*PetState.ActivePetId.ToString(),
							WorldBossState.BossProgress.Num(),
							EndlessDungeonState
								.HighestClearedFloor);
					}

					if (bTestSaveAscensionFixture)
					{
						UE_LOG(LogTemp, Display,
							TEXT("Ascension fixture explicit save: success=%s version=%d marker=%s"),
							SaveProgress()
								? TEXT("true")
								: TEXT("false"),
							UImmortalPathSaveGame
								::CurrentSaveVersion,
							AscensionState.bInitialized
								? TEXT("true")
								: TEXT("false"));
					}

					if (bTestScreenshotAscension
						|| bTestScreenshotAscensionStatus)
					{
						FTimerHandle ScreenshotTimer;
						GetWorldTimerManager().SetTimer(
							ScreenshotTimer,
							FTimerDelegate::CreateWeakLambda(
								this,
								[bTestScreenshotAscensionStatus]
								{
									const FString Path =
										FPaths::Combine(
											FPaths::ProjectSavedDir(),
											bTestScreenshotAscensionStatus
												? TEXT("Screenshots/Step33_Ascension_StatusButton.png")
												: TEXT("Screenshots/Step33_Ascension_UI.png"));
									FScreenshotRequest
										::RequestScreenshot(
											Path,
											true,
											false);
									UE_LOG(LogTemp, Display,
										TEXT("Ascension verification screenshot requested: %s"),
										*Path);
								}),
							0.8f,
							false);
					}

					if (bTestLogAscension
						|| bTestPerformAscension
						|| bTestInvestAscension)
					{
						FTimerHandle AuditTimer;
						GetWorldTimerManager().SetTimer(
							AuditTimer,
							FTimerDelegate::CreateWeakLambda(
								this,
								[this,
									BaselineAscension,
									BaselineGold,
									BaselineInventory,
									BaselineEquipment,
									BaselineMaterials,
									BaselineMaps,
									BaselineCave,
									BaselineFarming,
									BaselineSect,
									BaselinePet,
									BaselineBoss,
									BaselineEndless]
								{
									const FImmortalMapSystemState
										CurrentMaps =
											GetMapSystemState();
									FImmortalMapProgress FinalMap;
									const bool
										bFinalMapCompleted =
											UImmortalMapLibrary
												::GetMapProgress(
													CurrentMaps,
													UImmortalMapLibrary
														::GetImmortalPalaceRuinsId(),
													FinalMap)
											&& FinalMap.bCompleted;
									const bool bCommitted =
										AscensionState
											.AscensionCount
											> BaselineAscension
												.AscensionCount;
									const bool bCycleInvariant =
										bCommitted
											? IsAscensionCycleStartState(
												CurrentMaps)
											: HaveSameMapProgress(
												CurrentMaps,
												BaselineMaps);
									const bool bLifetimeInvariant =
										bCommitted
											? HaveMergedLifetimeMapRecords(
												BaselineAscension,
												AscensionState,
												BaselineMaps)
											: HaveSameLifetimeMapRecords(
												AscensionState,
												BaselineAscension);
									const bool bPreserved =
										CurrentGold == BaselineGold
										&& InventoryItems.Num()
											== BaselineInventory
										&& EquippedItems.Num()
											== BaselineEquipment
										&& MaterialInventory.Num()
											== BaselineMaterials
										&& bCycleInvariant
										&& bLifetimeInvariant
										&& CaveState.Revision
											>= BaselineCave.Revision
										&& FarmingState.Revision
											>= BaselineFarming.Revision
										&& SectState.SectId
											== BaselineSect.SectId
										&& PetState.ActivePetId
											== BaselinePet.ActivePetId
										&& WorldBossState
											.BossProgress.Num()
											== BaselineBoss
												.BossProgress.Num()
										&& EndlessDungeonState
											.HighestClearedFloor
											== BaselineEndless
												.HighestClearedFloor;
									FImmortalAscensionMapLegacy
										FinalMapLegacy;
									UImmortalAscensionLibrary
										::GetLifetimeMapRecord(
											AscensionState,
											UImmortalMapLibrary
												::GetImmortalPalaceRuinsId(),
											FinalMapLegacy);
									UE_LOG(LogTemp, Display,
										TEXT("Ascension runtime audit: count=%d(delta=%d) seals=%d paths=%d/%d/%d realm=%s cultivation=%d battle=x%.2f cultivationRate=x%.2f loot=x%.2f activeMap=%s finalMapCompleted=%s cycleInvariant=%s lifetimeInvariant=%s lifetimeCompleted=%d/%d finalLegacy=%d/%d preserved=%s aliveMonsters=%d worldBossActive=%s endlessActive=%s revision=%d"),
										AscensionState
											.AscensionCount,
										AscensionState
											.AscensionCount
											- BaselineAscension
												.AscensionCount,
										AscensionState
											.ImmortalSeals,
										AscensionState
											.BattlePathRank,
										AscensionState
											.EnlightenmentPathRank,
										AscensionState
											.FortunePathRank,
										*GetFullCultivationRealmName()
											.ToString(),
										CurrentCultivation,
										GetAscensionBattleMultiplier(),
										GetAscensionCultivationMultiplier(),
										GetAscensionEquipmentDropMultiplier(),
										*CurrentMaps.ActiveMapId
											.ToString(),
										bFinalMapCompleted
											? TEXT("true")
											: TEXT("false"),
										bCycleInvariant
											? TEXT("true")
											: TEXT("false"),
										bLifetimeInvariant
											? TEXT("true")
											: TEXT("false"),
										UImmortalAscensionLibrary
											::GetLifetimeCompletedMapCount(
												AscensionState),
										UImmortalMapLibrary
											::GetKnownMapIds().Num(),
										FinalMapLegacy.HighestStage,
										FinalMapLegacy.TimesCompleted,
										bPreserved
											? TEXT("true")
											: TEXT("false"),
										FindMapSpawner()
											? FindMapSpawner()
												->GetAliveMonsterCount()
											: -1,
										GetWorldBossRuntimeSnapshot()
											.bActive
											? TEXT("true")
											: TEXT("false"),
										GetEndlessDungeonRuntimeSnapshot()
											.bActive
											? TEXT("true")
											: TEXT("false"),
										AscensionState.Revision);
									SpawnActivePetActor();
									StartAutoAttack();
								}),
							1.7f,
							false);
					}

					if (FParse::Param(
						FCommandLine::Get(),
						TEXT("ImmortalTestExitAfterAscension")))
					{
						FTimerHandle ExitTimer;
						GetWorldTimerManager().SetTimer(
							ExitTimer,
							FTimerDelegate::CreateWeakLambda(
								this,
								[]
								{
									UE_LOG(LogTemp, Display,
										TEXT("Ascension runtime verification complete; requesting clean exit"));
									FPlatformMisc::RequestExit(
										false);
								}),
							4.6f,
							false);
					}
				}),
			0.95f,
			false);
	}
#endif

#if !UE_BUILD_SHIPPING
	if (PlayerInventoryWidget && FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestOpenMaterials")))
	{
		ToggleInventory();
		PlayerInventoryWidget->ShowMaterialTab();
		UE_LOG(LogTemp, Display, TEXT("Material inventory opened by development test parameter"));
		if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotMaterials")))
		{
			FTimerHandle ScreenshotTimer;
			GetWorldTimerManager().SetTimer(
				ScreenshotTimer,
				FTimerDelegate::CreateWeakLambda(this, []
				{
					const FString ScreenshotPath = FPaths::Combine(
						FPaths::ProjectSavedDir(), TEXT("Screenshots/MaterialInventoryTest.png"));
					FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
					UE_LOG(LogTemp, Display, TEXT("Material inventory verification screenshot requested: %s"), *ScreenshotPath);
				}),
				1.5f,
				false);
		}
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestGrantAlchemyMaterials")))
	{
		for (const FName MaterialId : { FName(TEXT("SpiritGrass")), FName(TEXT("DemonCore")), FName(TEXT("SpiritLiquid")), FName(TEXT("Ore")), FName(TEXT("ImmortalFruit")) })
		{
			AddMaterialInternal(MaterialId, 20);
		}
		++MaterialInventoryRevision;
		UE_LOG(LogTemp, Display, TEXT("Alchemy development materials granted: five recipe materials x20"));
	}

	FString TestAlchemyRecipe;
	if (FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestAlchemyRecipe="), TestAlchemyRecipe))
	{
		float ForcedAlchemyRoll = 0.0f;
		const bool bHasForcedRoll = FParse::Value(
			FCommandLine::Get(), TEXT("ImmortalTestAlchemyRoll="), ForcedAlchemyRoll);
		const FImmortalAlchemyCraftResult TestResult = CraftPillInternal(
			FName(*TestAlchemyRecipe),
			bHasForcedRoll ? TOptional<float>(ForcedAlchemyRoll) : TOptional<float>());
		UE_LOG(LogTemp, Display, TEXT("Alchemy runtime verification: recipe %s | outcome %d | granted %d | message %s"),
			*TestAlchemyRecipe,
			static_cast<int32>(TestResult.Outcome),
			TestResult.PillQuantityGranted,
			*TestResult.Message.ToString());

		if (TestResult.PillQuantityGranted > 0
			&& FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestUseCraftedPill")))
		{
			const EImmortalPillQuality Quality = TestResult.Outcome == EImmortalAlchemyOutcome::Exceptional
				? EImmortalPillQuality::Exceptional
				: EImmortalPillQuality::Ordinary;
			const bool bUsed = UsePill(FName(*TestAlchemyRecipe), Quality);
			UE_LOG(LogTemp, Display, TEXT("Alchemy runtime verification pill use: %s | boost %.2fx | remaining %.1fs"),
				bUsed ? TEXT("true") : TEXT("false"),
				GetAlchemyBoostMultiplier(),
				GetAlchemyBoostRemainingSeconds());
		}
	}

	if (PlayerAlchemyWidget && FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestOpenAlchemy")))
	{
		ToggleAlchemy();
		UE_LOG(LogTemp, Display, TEXT("Alchemy furnace opened by development test parameter"));
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotAlchemy")))
	{
		FTimerHandle ScreenshotTimer;
		GetWorldTimerManager().SetTimer(
			ScreenshotTimer,
			FTimerDelegate::CreateWeakLambda(this, []
			{
				const FString ScreenshotPath = FPaths::Combine(
					FPaths::ProjectSavedDir(), TEXT("Screenshots/AlchemyTest.png"));
				FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
				UE_LOG(LogTemp, Display, TEXT("Alchemy verification screenshot requested: %s"), *ScreenshotPath);
			}),
			1.5f,
			false);

		if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestExitAfterScreenshot")))
		{
			FTimerHandle ExitTimer;
			GetWorldTimerManager().SetTimer(
				ExitTimer,
				FTimerDelegate::CreateWeakLambda(this, []
				{
					UE_LOG(LogTemp, Display, TEXT("Alchemy runtime verification complete; requesting clean exit"));
					FPlatformMisc::RequestExit(false);
				}),
				4.0f,
				false);
		}
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestGrantCraftingResources")))
	{
		for (const FName MaterialId :
			{ FName(TEXT("Ore")), FName(TEXT("DemonBone")), FName(TEXT("SpiritIron")), FName(TEXT("ArtifactFragment")), FName(TEXT("SpiritWood")) })
		{
			AddMaterialInternal(MaterialId, 50);
		}
		CurrentGold = FMath::Min(CurrentGold + 5000, MAX_int32);
		++MaterialInventoryRevision;
		BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, 0);
		UE_LOG(LogTemp, Display, TEXT("Crafting development resources granted: five materials x50 | spirit stones +5000"));
	}

	FString TestCraftingRecipe;
	if (FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestCraftingRecipe="), TestCraftingRecipe))
	{
		const FImmortalCraftingResult TestCraftResult = CraftEquipment(FName(*TestCraftingRecipe));
		UE_LOG(LogTemp, Display, TEXT("Crafting runtime verification: recipe %s | success %s | item %s | message %s"),
			*TestCraftingRecipe,
			TestCraftResult.bSucceeded ? TEXT("true") : TEXT("false"),
			*TestCraftResult.ItemId.ToString(),
			*TestCraftResult.Message.ToString());
		if (TestCraftResult.bSucceeded
			&& FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestEnhanceCraftedEquipment")))
		{
			const FImmortalCraftingResult EnhanceResult = EnhanceEquipment(TestCraftResult.ItemId);
			UE_LOG(LogTemp, Display, TEXT("Crafting runtime enhancement: success %s | %s"),
				EnhanceResult.bSucceeded ? TEXT("true") : TEXT("false"), *EnhanceResult.Message.ToString());
		}
		if (TestCraftResult.bSucceeded
			&& FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestRefineCraftedEquipment")))
		{
			const FImmortalCraftingResult RefineResult = RefineEquipment(TestCraftResult.ItemId);
			UE_LOG(LogTemp, Display, TEXT("Crafting runtime refinement: success %s | %s"),
				RefineResult.bSucceeded ? TEXT("true") : TEXT("false"), *RefineResult.Message.ToString());
		}
	}

	if (PlayerCraftingWidget && FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestOpenCrafting")))
	{
		ToggleCrafting();
		UE_LOG(LogTemp, Display, TEXT("Crafting furnace opened by development test parameter"));
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotCrafting")))
	{
		FTimerHandle ScreenshotTimer;
		GetWorldTimerManager().SetTimer(
			ScreenshotTimer,
			FTimerDelegate::CreateWeakLambda(this, []
			{
				const FString ScreenshotPath = FPaths::Combine(
					FPaths::ProjectSavedDir(), TEXT("Screenshots/CraftingTest.png"));
				FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
				UE_LOG(LogTemp, Display, TEXT("Crafting verification screenshot requested: %s"), *ScreenshotPath);
			}),
			1.5f,
			false);
		if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestExitAfterScreenshot")))
		{
			FTimerHandle ExitTimer;
			GetWorldTimerManager().SetTimer(
				ExitTimer,
				FTimerDelegate::CreateWeakLambda(this, []
				{
					UE_LOG(LogTemp, Display, TEXT("Crafting runtime verification complete; requesting clean exit"));
					FPlatformMisc::RequestExit(false);
				}),
				4.0f,
				false);
		}
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestLogCraftingInventory")))
	{
		int32 ForgedItemCount = 0;
		auto LogForgedItems = [&ForgedItemCount](const TArray<FImmortalEquipmentItem>& Items, const TCHAR* Location)
		{
			for (const FImmortalEquipmentItem& Item : Items)
			{
				if (Item.EnhancementLevel <= 0 && Item.RefinementCount <= 0) continue;
				++ForgedItemCount;
				UE_LOG(LogTemp, Display, TEXT("Persisted forged equipment: %s | %s | +%d | refinements %d | affixes %d | power %.2f"),
					Location, *Item.DisplayName.ToString(), Item.EnhancementLevel, Item.RefinementCount,
					Item.Affixes.Num(), UImmortalEquipmentLibrary::CalculateEquipmentPower(Item));
			}
		};
		LogForgedItems(EquippedItems, TEXT("equipped"));
		LogForgedItems(InventoryItems, TEXT("backpack"));
		UE_LOG(LogTemp, Display, TEXT("Crafting persistence audit: forged items %d | save version %d"),
			ForgedItemCount, UImmortalPathSaveGame::CurrentSaveVersion);
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestGrantArtifactResources")))
	{
		for (const FName MaterialId :
			{ FName(TEXT("ArtifactFragment")), FName(TEXT("SpiritIron")), FName(TEXT("DemonCore")), FName(TEXT("DemonBone")) })
		{
			AddMaterialInternal(MaterialId, 100);
		}
		CurrentGold = FMath::Min(CurrentGold + 20000, MAX_int32);
		++MaterialInventoryRevision;
		BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, 0);
		UE_LOG(LogTemp, Display, TEXT("Artifact development resources granted: four materials x100 | spirit stones +20000"));
	}

	FString TestArtifactId;
	if (FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestArtifact="), TestArtifactId))
	{
		const FImmortalArtifactOperationResult CraftResult = CraftArtifact(FName(*TestArtifactId));
		UE_LOG(LogTemp, Display, TEXT("Artifact runtime crafting: %s | success %s | instance %s | %s"),
			*TestArtifactId, CraftResult.bSucceeded ? TEXT("true") : TEXT("false"),
			*CraftResult.InstanceId.ToString(), *CraftResult.Message.ToString());
		if (CraftResult.bSucceeded)
		{
			const FImmortalArtifactOperationResult EquipResult = EquipArtifact(CraftResult.InstanceId);
			UE_LOG(LogTemp, Display, TEXT("Artifact runtime equip: success %s | %s"),
				EquipResult.bSucceeded ? TEXT("true") : TEXT("false"), *EquipResult.Message.ToString());
			if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestUpgradeArtifact")))
			{
				const FImmortalArtifactOperationResult UpgradeResult = UpgradeArtifact(CraftResult.InstanceId);
				UE_LOG(LogTemp, Display, TEXT("Artifact runtime upgrade: success %s | %s"),
					UpgradeResult.bSucceeded ? TEXT("true") : TEXT("false"), *UpgradeResult.Message.ToString());
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestStarArtifact")))
			{
				const FImmortalArtifactOperationResult StarResult = StarUpArtifact(CraftResult.InstanceId);
				UE_LOG(LogTemp, Display, TEXT("Artifact runtime star-up: success %s | %s"),
					StarResult.bSucceeded ? TEXT("true") : TEXT("false"), *StarResult.Message.ToString());
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestPrimeArtifactTrigger")))
			{
				FImmortalArtifactItem Equipped;
				if (GetEquippedArtifact(Equipped))
				{
					ArtifactAttackCounter = FMath::Max(UImmortalArtifactLibrary::CalculateTriggerAttackCount(Equipped) - 1, 0);
					UE_LOG(LogTemp, Display, TEXT("Artifact trigger primed: %s | counter %d"),
						*Equipped.ArtifactId.ToString(), ArtifactAttackCounter);
				}
			}
		}
	}

	if (PlayerArtifactWidget && FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestOpenArtifacts")))
	{
		ToggleArtifacts();
		UE_LOG(LogTemp, Display, TEXT("Artifact furnace opened by development test parameter"));
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotArtifacts")))
	{
		FTimerHandle ScreenshotTimer;
		GetWorldTimerManager().SetTimer(
			ScreenshotTimer,
			FTimerDelegate::CreateWeakLambda(this, []
			{
				const FString ScreenshotPath = FPaths::Combine(
					FPaths::ProjectSavedDir(), TEXT("Screenshots/ArtifactTest.png"));
				FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
				UE_LOG(LogTemp, Display, TEXT("Artifact verification screenshot requested: %s"), *ScreenshotPath);
			}),
			1.5f,
			false);
		if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestExitAfterScreenshot")))
		{
			FTimerHandle ExitTimer;
			GetWorldTimerManager().SetTimer(
				ExitTimer,
				FTimerDelegate::CreateWeakLambda(this, []
				{
					UE_LOG(LogTemp, Display, TEXT("Artifact runtime verification complete; requesting clean exit"));
					FPlatformMisc::RequestExit(false);
				}),
				6.0f,
				false);
		}
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestLogArtifactInventory")))
	{
		for (const FImmortalArtifactItem& Item : ArtifactInventory)
		{
			UE_LOG(LogTemp, Display, TEXT("Persisted artifact: %s | instance %s | level %d | stars %d | equipped %s"),
				*Item.ArtifactId.ToString(), *Item.InstanceId.ToString(), Item.Level, Item.Stars,
				Item.InstanceId == EquippedArtifactInstanceId ? TEXT("true") : TEXT("false"));
		}
		UE_LOG(LogTemp, Display, TEXT("Artifact persistence audit: artifacts %d | equipped valid %s | save version %d"),
			ArtifactInventory.Num(), EquippedArtifactInstanceId.IsValid() ? TEXT("true") : TEXT("false"),
			UImmortalPathSaveGame::CurrentSaveVersion);
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestGrantTechniqueResources")))
	{
		for (const FName MaterialId :
			{ FName(TEXT("SpiritGrass")), FName(TEXT("SpiritLiquid")), FName(TEXT("DemonCore")),
			  FName(TEXT("SpiritIron")), FName(TEXT("ArtifactFragment")) })
		{
			AddMaterialInternal(MaterialId, 100);
		}
		CurrentGold = FMath::Min(CurrentGold + 20000, MAX_int32);
		if (CultivationComponent) CultivationComponent->AddCultivation(2000);
		++MaterialInventoryRevision;
		BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, 0);
		UE_LOG(LogTemp, Display, TEXT("Technique development resources granted: five materials x100 | spirit stones +20000 | cultivation +2000"));
	}

	FString TestTechniqueId;
	if (FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestTechnique="), TestTechniqueId))
	{
		const FName TechniqueId(*TestTechniqueId);
		FImmortalTechniqueOperationResult LearnResult;
		if (!IsTechniqueLearned(TechniqueId)) LearnResult = LearnTechnique(TechniqueId);
		else
		{
			LearnResult.bSucceeded = true;
			LearnResult.TechniqueId = TechniqueId;
			LearnResult.Message = FText::FromString(TEXT("功法已在功法库中"));
		}
		UE_LOG(LogTemp, Display, TEXT("Technique runtime learning: %s | success %s | %s"),
			*TestTechniqueId, LearnResult.bSucceeded ? TEXT("true") : TEXT("false"), *LearnResult.Message.ToString());
		if (LearnResult.bSucceeded)
		{
			const FImmortalTechniqueOperationResult EquipResult = EquipTechnique(TechniqueId, 0);
			UE_LOG(LogTemp, Display, TEXT("Technique runtime equip: success %s | %s"),
				EquipResult.bSucceeded ? TEXT("true") : TEXT("false"), *EquipResult.Message.ToString());
			if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestUpgradeTechnique")))
			{
				const FImmortalTechniqueOperationResult UpgradeResult = UpgradeTechnique(TechniqueId);
				UE_LOG(LogTemp, Display, TEXT("Technique runtime upgrade: success %s | %s"),
					UpgradeResult.bSucceeded ? TEXT("true") : TEXT("false"), *UpgradeResult.Message.ToString());
				if (UpgradeResult.bSucceeded)
				{
					const FImmortalTechniqueOperationResult PointResult = AllocateTechniquePoint(
						TechniqueId, EImmortalTechniquePointBranch::Active);
					UE_LOG(LogTemp, Display, TEXT("Technique runtime point allocation: success %s | %s"),
						PointResult.bSucceeded ? TEXT("true") : TEXT("false"), *PointResult.Message.ToString());
				}
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestPrimeTechniqueTrigger")))
			{
				FImmortalTechniqueProgress Progress;
				if (GetTechniqueProgress(TechniqueId, Progress))
				{
					TechniqueAttackCounters.FindOrAdd(TechniqueId) =
						FMath::Max(UImmortalTechniqueLibrary::CalculateTriggerAttackCount(Progress) - 1, 0);
					if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestPrimeTechniqueUltimate")))
					{
						TechniqueActiveCounters.FindOrAdd(TechniqueId) =
							FMath::Max(UImmortalTechniqueLibrary::CalculateUltimateActiveCount(Progress) - 1, 0);
					}
					UE_LOG(LogTemp, Display, TEXT("Technique trigger primed: %s | attack counter %d | active counter %d"),
						*TestTechniqueId, TechniqueAttackCounters.FindRef(TechniqueId), TechniqueActiveCounters.FindRef(TechniqueId));
				}
			}
		}
	}

	if (PlayerTechniqueWidget && FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestOpenTechniques")))
	{
		ToggleTechniques();
		UE_LOG(LogTemp, Display, TEXT("Technique library opened by development test parameter"));
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotTechniques")))
	{
		FTimerHandle ScreenshotTimer;
		GetWorldTimerManager().SetTimer(
			ScreenshotTimer,
			FTimerDelegate::CreateWeakLambda(this, []
			{
				const FString ScreenshotPath = FPaths::Combine(
					FPaths::ProjectSavedDir(), TEXT("Screenshots/TechniqueTest.png"));
				FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
				UE_LOG(LogTemp, Display, TEXT("Technique verification screenshot requested: %s"), *ScreenshotPath);
			}),
			1.5f,
			false);
		if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestExitAfterScreenshot")))
		{
			FTimerHandle ExitTimer;
			GetWorldTimerManager().SetTimer(
				ExitTimer,
				FTimerDelegate::CreateWeakLambda(this, []
				{
					UE_LOG(LogTemp, Display, TEXT("Technique runtime verification complete; requesting clean exit"));
					FPlatformMisc::RequestExit(false);
				}),
				6.0f,
				false);
		}
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestLogTechniqueLibrary")))
	{
		for (const FImmortalTechniqueProgress& Progress : TechniqueLibrary)
		{
			int32 Slot = INDEX_NONE;
			const bool bEquipped = IsTechniqueEquipped(Progress.TechniqueId, Slot);
			UE_LOG(LogTemp, Display, TEXT("Persisted technique: %s | level %d | breakthrough %d | points %d/%d/%d | equipped %s | slot %d"),
				*Progress.TechniqueId.ToString(), Progress.Level, Progress.BreakthroughRank,
				Progress.ActivePoints, Progress.PassivePoints, Progress.SpecialPoints,
				bEquipped ? TEXT("true") : TEXT("false"), bEquipped ? Slot + 1 : 0);
		}
		UE_LOG(LogTemp, Display, TEXT("Technique persistence audit: learned %d | equipped %d | insight %d | save version %d"),
			TechniqueLibrary.Num(), EquippedTechniqueIds.Num(), TechniqueInsightPoints,
			UImmortalPathSaveGame::CurrentSaveVersion);
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestGrantCharacterBuildResources")))
	{
		AddMaterialInternal(TEXT("DemonCore"), 100);
		AddMaterialInternal(TEXT("SpiritLiquid"), 100);
		CurrentGold = FMath::Min(CurrentGold + 20000, MAX_int32);
		++MaterialInventoryRevision;
		BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, 0);
		UE_LOG(LogTemp, Display, TEXT("Character-build development resources granted: DemonCore/SpiritLiquid x100 | spirit stones +20000"));
	}

	FString TestSpiritRoot;
	if (FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestSpiritRoot="), TestSpiritRoot))
	{
		const TMap<FString, EImmortalSpiritRoot> RootMap =
		{
			{TEXT("Metal"), EImmortalSpiritRoot::Metal}, {TEXT("Wood"), EImmortalSpiritRoot::Wood},
			{TEXT("Water"), EImmortalSpiritRoot::Water}, {TEXT("Fire"), EImmortalSpiritRoot::Fire},
			{TEXT("Earth"), EImmortalSpiritRoot::Earth}, {TEXT("Wind"), EImmortalSpiritRoot::Wind},
			{TEXT("Thunder"), EImmortalSpiritRoot::Thunder}, {TEXT("Ice"), EImmortalSpiritRoot::Ice},
			{TEXT("Mutated"), EImmortalSpiritRoot::Mutated}
		};
		if (const EImmortalSpiritRoot* ForcedRoot = RootMap.Find(TestSpiritRoot))
		{
			float ForcedPurity = 1.0f;
			FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestSpiritRootPurity="), ForcedPurity);
			SpiritRootState.Root = *ForcedRoot;
			SpiritRootState.Purity = ForcedPurity;
			UImmortalCharacterPathLibrary::NormalizeSpiritRoot(SpiritRootState);
			++CharacterBuildRevision;
			RecalculateEquipmentBonuses();
			SaveProgress();
			UE_LOG(LogTemp, Display, TEXT("Spirit root forced for development: %s | type %d | purity %.2f | cultivation x%.3f | pill x%.3f"),
				*TestSpiritRoot, static_cast<int32>(SpiritRootState.Root), SpiritRootState.Purity,
				UImmortalCharacterPathLibrary::CalculateCultivationRateMultiplier(SpiritRootState), GetPillEffectMultiplier());
		}
	}

	FString TestCultivationPath;
	if (FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestCultivationPath="), TestCultivationPath))
	{
		const TMap<FString, EImmortalCultivationPath> PathMap =
		{
			{TEXT("Body"), EImmortalCultivationPath::Body}, {TEXT("Dharma"), EImmortalCultivationPath::Dharma},
			{TEXT("Sword"), EImmortalCultivationPath::Sword}, {TEXT("Poison"), EImmortalCultivationPath::Poison},
			{TEXT("Thunder"), EImmortalCultivationPath::Thunder}
		};
		if (const EImmortalCultivationPath* ForcedPath = PathMap.Find(TestCultivationPath))
		{
			const FImmortalCharacterPathOperationResult PathResult = SelectCultivationPath(*ForcedPath);
			UE_LOG(LogTemp, Display, TEXT("Cultivation path runtime selection: %s | success %s | %s"),
				*TestCultivationPath, PathResult.bSucceeded ? TEXT("true") : TEXT("false"), *PathResult.Message.ToString());
		}
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestPrimeCultivationPathSkill")))
	{
		FImmortalCultivationPathDefinition Definition;
		if (UImmortalCharacterPathLibrary::GetCultivationPathDefinition(CultivationPathState.Path, Definition))
		{
			CultivationPathAttackCounter = FMath::Max(Definition.AttacksPerSkill - 1, 0);
			UE_LOG(LogTemp, Display, TEXT("Cultivation path skill primed: path %d | counter %d/%d"),
				static_cast<int32>(CultivationPathState.Path), CultivationPathAttackCounter, Definition.AttacksPerSkill);
		}
	}

	if (PlayerCharacterBuildWidget && FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestOpenCharacterBuild")))
	{
		ToggleCharacterBuild();
		UE_LOG(LogTemp, Display, TEXT("Character build screen opened by development test parameter"));
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotCharacterBuild")))
	{
		FTimerHandle ScreenshotTimer;
		GetWorldTimerManager().SetTimer(
			ScreenshotTimer,
			FTimerDelegate::CreateWeakLambda(this, []
			{
				const FString ScreenshotPath = FPaths::Combine(
					FPaths::ProjectSavedDir(), TEXT("Screenshots/CharacterBuildTest.png"));
				FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
				UE_LOG(LogTemp, Display, TEXT("Character-build verification screenshot requested: %s"), *ScreenshotPath);
			}),
			1.5f,
			false);
		if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestExitAfterScreenshot")))
		{
			FTimerHandle ExitTimer;
			GetWorldTimerManager().SetTimer(
				ExitTimer,
				FTimerDelegate::CreateWeakLambda(this, []
				{
					UE_LOG(LogTemp, Display, TEXT("Character-build runtime verification complete; requesting clean exit"));
					FPlatformMisc::RequestExit(false);
				}),
				6.0f,
				false);
		}
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestLogCharacterBuild")))
	{
		FImmortalSpiritRootDefinition RootDefinition;
		FImmortalCultivationPathDefinition PathDefinition;
		UImmortalCharacterPathLibrary::GetSpiritRootDefinition(SpiritRootState.Root, RootDefinition);
		UImmortalCharacterPathLibrary::GetCultivationPathDefinition(CultivationPathState.Path, PathDefinition);
		int32 CompatibleEquipment = 0;
		for (const FImmortalEquipmentItem& Item : EquippedItems)
		{
			if (IsEquipmentCompatibleWithPath(Item)) ++CompatibleEquipment;
			UE_LOG(LogTemp, Display, TEXT("Persisted equipped discipline: item %s | discipline %d | compatible %s"),
				*Item.DisplayName.ToString(), static_cast<int32>(Item.Discipline),
				IsEquipmentCompatibleWithPath(Item) ? TEXT("true") : TEXT("false"));
		}
		UE_LOG(LogTemp, Display, TEXT("Character-build persistence audit: root %s/type %d/purity %.2f | path %s/type %d/switches %d | equipment compatible %d/%d | root rate x%.3f | path rate x%.3f | pill x%.3f | save version %d"),
			*RootDefinition.DisplayName.ToString(), static_cast<int32>(SpiritRootState.Root), SpiritRootState.Purity,
			*PathDefinition.DisplayName.ToString(), static_cast<int32>(CultivationPathState.Path), CultivationPathState.SwitchCount,
			CompatibleEquipment, EquippedItems.Num(),
			UImmortalCharacterPathLibrary::CalculateCultivationRateMultiplier(SpiritRootState),
			CharacterPathCultivationRateMultiplier, GetPillEffectMultiplier(), UImmortalPathSaveGame::CurrentSaveVersion);
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestGrantShopResources")))
	{
		CurrentGold = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(CurrentGold) + 100000, MAX_int32));
		for (const FName MaterialId : UImmortalMaterialLibrary::GetKnownMaterialIds())
		{
			AddMaterialInternal(MaterialId, 30);
		}
		++MaterialInventoryRevision;
		for (int32 Index = 0; Index < 2 && InventoryItems.Num() < FMath::Max(InventoryCapacity, 1); ++Index)
		{
			InventoryItems.Add(UImmortalEquipmentLibrary::GenerateRandomEquipment(20 + Index));
			++EquipmentInventoryRevision;
		}
		// A fixed high-progression catalog guarantees all four product categories
		// are present for the temporary runtime verification save.
		const int32 DayKey = UImmortalShopLibrary::GetDayKeyFromUtcTicks(
			FDateTime::UtcNow().GetTicks(), ShopUtcOffsetMinutes);
		ShopState = UImmortalShopLibrary::GenerateStock(999, 9, 10, DayKey, 0);
		++ShopRevision;
		SaveProgress();
		UE_LOG(LogTemp, Display, TEXT("Shop development resources granted: spirit stones %d | backpack %d | materials %d | listings %d"),
			CurrentGold, InventoryItems.Num(), MaterialInventory.Num(), ShopState.Listings.Num());
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestFillShopBackpack")))
	{
		while (InventoryItems.Num() < FMath::Max(InventoryCapacity, 1))
		{
			InventoryItems.Add(UImmortalEquipmentLibrary::GenerateRandomEquipment(1));
		}
		++EquipmentInventoryRevision;
		const FImmortalShopListing* EquipmentListing = ShopState.Listings.FindByPredicate([](const FImmortalShopListing& Listing)
		{
			return Listing.ProductType == EImmortalShopProductType::Equipment && !Listing.bSoldOut;
		});
		if (EquipmentListing)
		{
			const FGuid ListingId = EquipmentListing->ListingId;
			const int32 GoldBefore = CurrentGold;
			const int32 CountBefore = InventoryItems.Num();
			const FImmortalShopTransactionResult TestResult = BuyShopListing(ListingId);
			const FImmortalShopListing* AfterListing = ShopState.Listings.FindByPredicate([ListingId](const FImmortalShopListing& Listing)
			{
				return Listing.ListingId == ListingId;
			});
			UE_LOG(LogTemp, Display, TEXT("Shop full-backpack audit: success %s | gold unchanged %s | count unchanged %s | listing available %s | message %s"),
				TestResult.bSucceeded ? TEXT("true") : TEXT("false"),
				CurrentGold == GoldBefore ? TEXT("true") : TEXT("false"),
				InventoryItems.Num() == CountBefore ? TEXT("true") : TEXT("false"),
				AfterListing && !AfterListing->bSoldOut ? TEXT("true") : TEXT("false"),
				*TestResult.Message.ToString());
		}
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestRunShopTransactions")))
	{
		for (const EImmortalShopProductType Type : {
			EImmortalShopProductType::Equipment,
			EImmortalShopProductType::Material,
			EImmortalShopProductType::Pill,
			EImmortalShopProductType::Artifact })
		{
			const FImmortalShopListing* Listing = ShopState.Listings.FindByPredicate([Type](const FImmortalShopListing& Entry)
			{
				return Entry.ProductType == Type && !Entry.bSoldOut;
			});
			if (!Listing) continue;
			const FImmortalShopTransactionResult BuyResult = BuyShopListing(Listing->ListingId);
			UE_LOG(LogTemp, Display, TEXT("Shop runtime category purchase: type %d | success %s | delta %d | message %s"),
				static_cast<int32>(Type), BuyResult.bSucceeded ? TEXT("true") : TEXT("false"),
				BuyResult.SpiritStoneDelta, *BuyResult.Message.ToString());
		}
		const FImmortalShopTransactionResult RefreshResult = RefreshShopInventory();
		UE_LOG(LogTemp, Display, TEXT("Shop runtime paid refresh: success %s | delta %d | message %s"),
			RefreshResult.bSucceeded ? TEXT("true") : TEXT("false"), RefreshResult.SpiritStoneDelta,
			*RefreshResult.Message.ToString());
		const FImmortalShopListing* RefreshedEquipment = ShopState.Listings.FindByPredicate([](const FImmortalShopListing& Entry)
		{
			return Entry.ProductType == EImmortalShopProductType::Equipment && !Entry.bSoldOut;
		});
		if (RefreshedEquipment)
		{
			const FImmortalShopTransactionResult BuyAfterRefresh = BuyShopListing(RefreshedEquipment->ListingId);
			UE_LOG(LogTemp, Display, TEXT("Shop runtime post-refresh purchase: success %s | delta %d | message %s"),
				BuyAfterRefresh.bSucceeded ? TEXT("true") : TEXT("false"),
				BuyAfterRefresh.SpiritStoneDelta, *BuyAfterRefresh.Message.ToString());
		}
		if (!InventoryItems.IsEmpty())
		{
			const FImmortalShopTransactionResult SellEquipmentResult = SellEquipmentToShop(InventoryItems[0].ItemId);
			UE_LOG(LogTemp, Display, TEXT("Shop runtime equipment sale: success %s | delta %d | message %s"),
				SellEquipmentResult.bSucceeded ? TEXT("true") : TEXT("false"),
				SellEquipmentResult.SpiritStoneDelta, *SellEquipmentResult.Message.ToString());
		}
		if (!MaterialInventory.IsEmpty())
		{
			const FName MaterialId = MaterialInventory[0].MaterialId;
			const FImmortalShopTransactionResult SellMaterialResult = SellMaterialToShop(MaterialId, 1);
			UE_LOG(LogTemp, Display, TEXT("Shop runtime material sale: %s | success %s | delta %d | message %s"),
				*MaterialId.ToString(), SellMaterialResult.bSucceeded ? TEXT("true") : TEXT("false"),
				SellMaterialResult.SpiritStoneDelta, *SellMaterialResult.Message.ToString());
		}
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestLogShop")))
	{
		int32 CategoryCounts[4] = {0, 0, 0, 0};
		int32 SoldOut = 0;
		for (const FImmortalShopListing& Listing : ShopState.Listings)
		{
			const int32 TypeIndex = static_cast<int32>(Listing.ProductType);
			if (TypeIndex >= 0 && TypeIndex < 4) ++CategoryCounts[TypeIndex];
			SoldOut += Listing.bSoldOut ? 1 : 0;
			UE_LOG(LogTemp, Display, TEXT("Persisted shop listing: %s | type %d | product %s | quantity %d | price %d | sold %s"),
				*Listing.ListingId.ToString(), TypeIndex, *Listing.ProductId.ToString(), Listing.BundleQuantity,
				Listing.BundlePrice, Listing.bSoldOut ? TEXT("true") : TEXT("false"));
		}
		UE_LOG(LogTemp, Display, TEXT("Shop persistence audit: day %d | serial %d | manual %d | listings %d | categories %d/%d/%d/%d | sold %d | stones %d | backpack %d | materials %d | pills %d | artifacts %d | save version %d"),
			ShopState.RefreshDayKey, ShopState.RefreshSerial, ShopState.ManualRefreshCount, ShopState.Listings.Num(),
			CategoryCounts[0], CategoryCounts[1], CategoryCounts[2], CategoryCounts[3], SoldOut,
			CurrentGold, InventoryItems.Num(), MaterialInventory.Num(), PillInventory.Num(), ArtifactInventory.Num(),
			UImmortalPathSaveGame::CurrentSaveVersion);
	}

	if (PlayerShopWidget && FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestOpenShop")))
	{
		ToggleShop();
		UE_LOG(LogTemp, Display, TEXT("Treasure Pavilion opened by development test parameter"));
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotShop")))
	{
		FTimerHandle ScreenshotTimer;
		GetWorldTimerManager().SetTimer(
			ScreenshotTimer,
			FTimerDelegate::CreateWeakLambda(this, []
			{
				const FString ScreenshotPath = FPaths::Combine(
					FPaths::ProjectSavedDir(), TEXT("Screenshots/ShopTest.png"));
				FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
				UE_LOG(LogTemp, Display, TEXT("Shop verification screenshot requested: %s"), *ScreenshotPath);
			}),
			1.5f,
			false);
		if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestExitAfterScreenshot")))
		{
			FTimerHandle ExitTimer;
			GetWorldTimerManager().SetTimer(
				ExitTimer,
				FTimerDelegate::CreateWeakLambda(this, []
				{
					UE_LOG(LogTemp, Display, TEXT("Shop runtime verification complete; requesting clean exit"));
					FPlatformMisc::RequestExit(false);
				}),
				6.0f,
				false);
		}
	}

	FString TestTravelMapId;
	const bool bHasTestTravelMap = FParse::Value(
		FCommandLine::Get(), TEXT("ImmortalTestTravelMap="), TestTravelMapId);
	const bool bTestOpenMaps = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestOpenMaps"));
	const bool bTestScreenshotMaps = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotMaps"));
	const bool bTestLogMaps = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestLogMaps"));
	int32 TestMapRealm = INDEX_NONE;
	if (FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestMapRealm="), TestMapRealm)
		&& CultivationComponent)
	{
		const int32 SafeRealm = FMath::Clamp(
			TestMapRealm, 0, static_cast<int32>(EImmortalCultivationRealm::Ascension));
		CultivationComponent->InitializeProgress(
			static_cast<EImmortalCultivationRealm>(SafeRealm), 1, 0);
		CurrentCultivation = CultivationComponent->GetCurrentCultivation();
		UE_LOG(LogTemp, Display, TEXT("Map runtime realm override applied: %d"), SafeRealm);
	}
	if (bHasTestTravelMap || bTestOpenMaps || bTestScreenshotMaps || bTestLogMaps)
	{
		FTimerHandle MapVerificationTimer;
		GetWorldTimerManager().SetTimer(
			MapVerificationTimer,
			FTimerDelegate::CreateWeakLambda(this,
				[this, TestTravelMapId, bHasTestTravelMap, bTestOpenMaps, bTestScreenshotMaps, bTestLogMaps]
				{
					if (bHasTestTravelMap)
					{
						const FImmortalMapTravelResult TravelResult = TravelToMap(FName(*TestTravelMapId));
						UE_LOG(LogTemp, Display,
							TEXT("Map runtime travel audit: destination=%s success=%s unlocked=%s alreadyActive=%s message=%s"),
							*TestTravelMapId,
							TravelResult.bSucceeded ? TEXT("true") : TEXT("false"),
							TravelResult.bUnlocked ? TEXT("true") : TEXT("false"),
							TravelResult.bAlreadyActive ? TEXT("true") : TEXT("false"),
							*TravelResult.Message.ToString());
					}
					if (bTestLogMaps)
					{
						const FImmortalMapSystemState State = GetMapSystemState();
						for (const FImmortalMapProgress& Progress : State.MapProgress)
						{
							UE_LOG(LogTemp, Display,
								TEXT("Map persistence audit: active=%s map=%s stage=%d kills=%d completed=%s"),
								*State.ActiveMapId.ToString(), *Progress.MapId.ToString(), Progress.Stage,
								Progress.StageKills, Progress.bCompleted ? TEXT("true") : TEXT("false"));
						}
					}
					if (bTestOpenMaps && PlayerMapWidget && !bMapSelectionOpen)
					{
						ToggleMapSelection();
						UE_LOG(LogTemp, Display, TEXT("Adventure map selector opened by development test parameter"));
					}
					if (bTestScreenshotMaps)
					{
						FTimerHandle ScreenshotTimer;
						GetWorldTimerManager().SetTimer(
							ScreenshotTimer,
							FTimerDelegate::CreateWeakLambda(this, []
							{
								const FString ScreenshotPath = FPaths::Combine(
									FPaths::ProjectSavedDir(), TEXT("Screenshots/MapSelectionTest.png"));
								FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
								UE_LOG(LogTemp, Display, TEXT("Map selector verification screenshot requested: %s"), *ScreenshotPath);
							}),
							1.5f,
							false);
					}
				}),
			1.0f,
			false);

		if (bTestScreenshotMaps && FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestExitAfterScreenshot")))
		{
			FTimerHandle ExitTimer;
			GetWorldTimerManager().SetTimer(
				ExitTimer,
				FTimerDelegate::CreateWeakLambda(this, []
				{
					UE_LOG(LogTemp, Display, TEXT("Map runtime verification complete; requesting clean exit"));
					FPlatformMisc::RequestExit(false);
				}),
				6.0f,
				false);
		}
	}

	int32 TestCaveSeconds = 0;
	const bool bHasTestCaveSeconds = FParse::Value(
		FCommandLine::Get(), TEXT("ImmortalTestCaveSeconds="), TestCaveSeconds);
	FString TestCaveUpgrade;
	const bool bHasTestCaveUpgrade = FParse::Value(
		FCommandLine::Get(), TEXT("ImmortalTestCaveUpgrade="), TestCaveUpgrade);
	const bool bTestGrantCaveResources = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestGrantCaveResources"));
	const bool bTestCollectCave = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestCollectCave"));
	const bool bTestOpenCave = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestOpenCave"));
	const bool bTestScreenshotCave = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotCave"));
	const bool bTestLogCave = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestLogCave"));
	if (bHasTestCaveSeconds || bHasTestCaveUpgrade || bTestGrantCaveResources
		|| bTestCollectCave || bTestOpenCave || bTestScreenshotCave || bTestLogCave)
	{
		FTimerHandle CaveVerificationTimer;
		GetWorldTimerManager().SetTimer(
			CaveVerificationTimer,
			FTimerDelegate::CreateWeakLambda(this,
				[this, TestCaveSeconds, bHasTestCaveSeconds, TestCaveUpgrade, bHasTestCaveUpgrade,
					bTestGrantCaveResources, bTestCollectCave, bTestOpenCave, bTestScreenshotCave, bTestLogCave]
				{
					if (bTestGrantCaveResources)
					{
						CurrentGold = static_cast<int32>(FMath::Min<int64>(
							static_cast<int64>(CurrentGold) + 1000000, MAX_int32));
						for (const FName MaterialId : {FName(TEXT("SpiritGrass")), FName(TEXT("SpiritLiquid")),
							FName(TEXT("Ore")), FName(TEXT("DemonBone")), FName(TEXT("SpiritIron"))})
						{
							AddMaterialInternal(MaterialId, 10000);
						}
						BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, 1000000);
						UE_LOG(LogTemp, Display, TEXT("Cave development resources granted"));
					}
					if (bHasTestCaveSeconds && TestCaveSeconds > 0)
					{
						const int64 NowTicks = FDateTime::UtcNow().GetTicks();
						const int64 SafeSeconds = FMath::Clamp<int64>(TestCaveSeconds, 1, 60LL * 24LL * 60LL * 60LL);
						CaveState.LastSettlementUtcTicks = FMath::Max<int64>(
							NowTicks - SafeSeconds * ETimespan::TicksPerSecond, 1);
						const FImmortalCaveSettlementResult Settlement = SettleCaveProduction(NowTicks);
						UE_LOG(LogTemp, Display,
							TEXT("Cave development settlement: seconds=%.0f stones=%d grass=%d ore=%d discarded=%lld/%lld/%lld"),
							Settlement.ElapsedSeconds, Settlement.AddedSpiritStones, Settlement.AddedSpiritGrass,
							Settlement.AddedOre, Settlement.DiscardedSpiritStones,
							Settlement.DiscardedSpiritGrass, Settlement.DiscardedOre);
					}
					if (bHasTestCaveUpgrade)
					{
						EImmortalCaveBuildingType Type = EImmortalCaveBuildingType::CaveHeart;
						bool bKnownType = TestCaveUpgrade.Equals(TEXT("CaveHeart"), ESearchCase::IgnoreCase);
						if (TestCaveUpgrade.Equals(TEXT("MeditationRoom"), ESearchCase::IgnoreCase)) { Type = EImmortalCaveBuildingType::MeditationRoom; bKnownType = true; }
						else if (TestCaveUpgrade.Equals(TEXT("SpiritVein"), ESearchCase::IgnoreCase)) Type = EImmortalCaveBuildingType::SpiritVein;
						else if (TestCaveUpgrade.Equals(TEXT("StoragePavilion"), ESearchCase::IgnoreCase)) Type = EImmortalCaveBuildingType::StoragePavilion;
						else if (TestCaveUpgrade.Equals(TEXT("AlchemyRoom"), ESearchCase::IgnoreCase)) Type = EImmortalCaveBuildingType::AlchemyRoom;
						else if (TestCaveUpgrade.Equals(TEXT("ForgeRoom"), ESearchCase::IgnoreCase)) Type = EImmortalCaveBuildingType::ForgeRoom;
						else if (TestCaveUpgrade.Equals(TEXT("SpiritField"), ESearchCase::IgnoreCase)) Type = EImmortalCaveBuildingType::SpiritField;
						bKnownType = bKnownType
							|| TestCaveUpgrade.Equals(TEXT("SpiritVein"), ESearchCase::IgnoreCase)
							|| TestCaveUpgrade.Equals(TEXT("StoragePavilion"), ESearchCase::IgnoreCase)
							|| TestCaveUpgrade.Equals(TEXT("AlchemyRoom"), ESearchCase::IgnoreCase)
							|| TestCaveUpgrade.Equals(TEXT("ForgeRoom"), ESearchCase::IgnoreCase)
							|| TestCaveUpgrade.Equals(TEXT("SpiritField"), ESearchCase::IgnoreCase);
						if (!bKnownType)
						{
							UE_LOG(LogTemp, Error, TEXT("Unknown cave development upgrade id ignored: %s"), *TestCaveUpgrade);
						}
						else
						{
							const FImmortalCaveUpgradeResult Upgrade = UpgradeCaveBuilding(Type);
							UE_LOG(LogTemp, Display, TEXT("Cave development upgrade: %s success=%s target=%d message=%s"),
								*TestCaveUpgrade, Upgrade.bSucceeded ? TEXT("true") : TEXT("false"),
								Upgrade.TargetLevel, *Upgrade.Message.ToString());
						}
					}
					if (bTestCollectCave)
					{
						const FImmortalCaveCollectionResult Collection = CollectCaveResources();
						UE_LOG(LogTemp, Display, TEXT("Cave development collection: success=%s stones=%d grass=%d ore=%d"),
							Collection.bCollectedAnything ? TEXT("true") : TEXT("false"),
							Collection.SpiritStonesCollected, Collection.SpiritGrassCollected, Collection.OreCollected);
					}
					if (bTestLogCave)
					{
						const FImmortalCaveProductionSnapshot Snapshot = GetCaveProductionSnapshot();
						UE_LOG(LogTemp, Display,
							TEXT("Cave persistence audit: revision=%d last=%lld stored=%d/%d/%d rates=%.2f/%.2f/%.2f cap=%d/%d/%d cultivate=%.3f alchemy=%.3f/%.3f forge=%.3f"),
							CaveState.Revision, CaveState.LastSettlementUtcTicks,
							CaveState.StoredSpiritStones, CaveState.StoredSpiritGrass, CaveState.StoredOre,
							Snapshot.SpiritStonesPerHour, Snapshot.SpiritGrassPerHour, Snapshot.OrePerHour,
							Snapshot.SpiritStoneCapacity, Snapshot.SpiritGrassCapacity, Snapshot.OreCapacity,
							Snapshot.CultivationRateMultiplier, Snapshot.AlchemySuccessChanceBonus,
							Snapshot.AlchemyExceptionalChanceBonus, Snapshot.ForgeSpiritStoneDiscount);
					}
					if ((bTestOpenCave || bTestScreenshotCave) && PlayerCaveWidget && !bCaveOpen)
					{
						ToggleCave();
					}
					if (bTestScreenshotCave)
					{
						FTimerHandle ScreenshotTimer;
						GetWorldTimerManager().SetTimer(
							ScreenshotTimer,
							FTimerDelegate::CreateWeakLambda(this, []
							{
								const FString ScreenshotPath = FPaths::Combine(
									FPaths::ProjectSavedDir(), TEXT("Screenshots/CaveTest.png"));
								FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
								UE_LOG(LogTemp, Display, TEXT("Cave verification screenshot requested: %s"), *ScreenshotPath);
							}),
							1.5f,
							false);
					}
				}),
			1.0f,
			false);

		if (bTestScreenshotCave && FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestExitAfterScreenshot")))
		{
			FTimerHandle ExitTimer;
			GetWorldTimerManager().SetTimer(
				ExitTimer,
				FTimerDelegate::CreateWeakLambda(this, [] { FPlatformMisc::RequestExit(false); }),
				6.0f,
				false);
		}
	}

	int32 TestFarmingSeconds = 0;
	const bool bHasTestFarmingSeconds = FParse::Value(
		FCommandLine::Get(), TEXT("ImmortalTestFarmingSeconds="), TestFarmingSeconds);
	int32 TestFarmingPlot = 0;
	FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestFarmingPlot="), TestFarmingPlot);
	int32 TestFarmingFieldLevel = 0;
	const bool bHasTestFarmingFieldLevel = FParse::Value(
		FCommandLine::Get(), TEXT("ImmortalTestFarmingFieldLevel="), TestFarmingFieldLevel);
	FString TestFarmingCrop(TEXT("SpiritGrassCrop"));
	FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestFarmingCrop="), TestFarmingCrop);
	const bool bTestGrantFarmingResources = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestGrantFarmingResources"));
	const bool bTestPlantFarming = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestPlantFarming"));
	const bool bTestPlantAllFarming = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestPlantAllFarming"));
	const bool bTestHarvestFarming = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestHarvestFarming"));
	const bool bTestHarvestAllFarming = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestHarvestAllFarming"));
	const bool bTestOpenFarming = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestOpenFarming"));
	const bool bTestScreenshotFarming = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotFarming"));
	const bool bTestLogFarming = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestLogFarming"));
	if (bHasTestFarmingSeconds || bHasTestFarmingFieldLevel || bTestGrantFarmingResources
		|| bTestPlantFarming || bTestPlantAllFarming || bTestHarvestFarming || bTestHarvestAllFarming
		|| bTestOpenFarming || bTestScreenshotFarming || bTestLogFarming)
	{
		FTimerHandle FarmingVerificationTimer;
		GetWorldTimerManager().SetTimer(
			FarmingVerificationTimer,
			FTimerDelegate::CreateWeakLambda(this,
				[this, TestFarmingSeconds, bHasTestFarmingSeconds, TestFarmingPlot,
					TestFarmingFieldLevel, bHasTestFarmingFieldLevel, TestFarmingCrop,
					bTestGrantFarmingResources, bTestPlantFarming, bTestPlantAllFarming,
					bTestHarvestFarming, bTestHarvestAllFarming, bTestOpenFarming,
					bTestScreenshotFarming, bTestLogFarming]
				{
					const int64 NowTicks = FDateTime::UtcNow().GetTicks();
					if (bHasTestFarmingFieldLevel)
					{
						const int32 SafeLevel = FMath::Clamp(TestFarmingFieldLevel, 1, 20);
						for (FImmortalCaveBuildingProgress& Building : CaveState.Buildings)
						{
							if (Building.Type == EImmortalCaveBuildingType::CaveHeart)
							{
								Building.Level = FMath::Max(Building.Level, SafeLevel);
							}
							else if (Building.Type == EImmortalCaveBuildingType::SpiritField)
							{
								Building.Level = SafeLevel;
							}
						}
						UImmortalCaveLibrary::NormalizeState(CaveState, NowTicks);
						UE_LOG(LogTemp, Display, TEXT("Farming development field level applied: %d"), SafeLevel);
					}
					if (bTestGrantFarmingResources)
					{
						const int32 PreviousGold = CurrentGold;
						CurrentGold = static_cast<int32>(FMath::Min<int64>(
							static_cast<int64>(CurrentGold) + 10000, MAX_int32));
						AddMaterialInternal(TEXT("SpiritGrass"), 1000);
						AddMaterialInternal(TEXT("Ore"), 1000);
						BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, CurrentGold - PreviousGold);
						UE_LOG(LogTemp, Display, TEXT("Farming development resources granted: stones +10000 grass/ore +1000"));
					}

					FImmortalFarmingCropDefinition TestCropDefinition;
					const FName CropId(*TestFarmingCrop);
					const bool bKnownCrop = UImmortalFarmingLibrary::GetCropDefinition(CropId, TestCropDefinition);
					if (!bKnownCrop && (bTestPlantFarming || bTestPlantAllFarming))
					{
						UE_LOG(LogTemp, Error, TEXT("Unknown farming development crop id ignored: %s"), *TestFarmingCrop);
					}
					else if (bTestPlantAllFarming)
					{
						const FImmortalFarmingBatchPlantResult PlantResult = PlantCropInAllEmptyPlots(CropId);
						UE_LOG(LogTemp, Display,
							TEXT("Farming development batch plant: crop=%s success=%s planted=%d/%d message=%s"),
							*TestFarmingCrop, PlantResult.bSucceeded ? TEXT("true") : TEXT("false"),
							PlantResult.PlantedPlotCount, PlantResult.EligiblePlotCount, *PlantResult.Message.ToString());
					}
					else if (bTestPlantFarming)
					{
						const FImmortalFarmingPlantResult PlantResult = PlantCrop(TestFarmingPlot, CropId);
						UE_LOG(LogTemp, Display,
							TEXT("Farming development plant: plot=%d crop=%s success=%s yield=%d message=%s"),
							TestFarmingPlot, *TestFarmingCrop, PlantResult.bSucceeded ? TEXT("true") : TEXT("false"),
							PlantResult.FrozenYield, *PlantResult.Message.ToString());
					}

					if (bHasTestFarmingSeconds && TestFarmingSeconds > 0)
					{
						const int64 SafeSeconds = FMath::Clamp<int64>(
							TestFarmingSeconds, 1, 60LL * 24LL * 60LL * 60LL);
						FarmingState.LastSettlementUtcTicks = FMath::Max<int64>(
							NowTicks - SafeSeconds * ETimespan::TicksPerSecond, 1);
						const FImmortalFarmingSettlementResult Settlement = SettleFarmingGrowth(NowTicks);
						UE_LOG(LogTemp, Display,
							TEXT("Farming development settlement: seconds=%.0f growing=%d matured=%d rollback=%s"),
							Settlement.ElapsedSeconds, Settlement.GrowingPlotCount, Settlement.MaturedPlotCount,
							Settlement.bClockRollbackDetected ? TEXT("true") : TEXT("false"));
					}

					if (bTestHarvestAllFarming)
					{
						const FImmortalFarmingBatchHarvestResult HarvestResult = HarvestAllReadyCrops();
						UE_LOG(LogTemp, Display,
							TEXT("Farming development batch harvest: success=%s ready=%d full=%d partial=%d items=%d message=%s"),
							HarvestResult.bSucceeded ? TEXT("true") : TEXT("false"), HarvestResult.ReadyPlotCount,
							HarvestResult.FullyHarvestedPlotCount, HarvestResult.PartiallyHarvestedPlotCount,
							HarvestResult.HarvestedItemCount, *HarvestResult.Message.ToString());
					}
					else if (bTestHarvestFarming)
					{
						const FImmortalFarmingHarvestResult HarvestResult = HarvestCrop(TestFarmingPlot);
						UE_LOG(LogTemp, Display,
							TEXT("Farming development harvest: plot=%d success=%s material=%s amount=%d remaining=%d message=%s"),
							TestFarmingPlot, HarvestResult.bSucceeded ? TEXT("true") : TEXT("false"),
							*HarvestResult.OutputMaterialId.ToString(), HarvestResult.HarvestedQuantity,
							HarvestResult.RemainingQuantity, *HarvestResult.Message.ToString());
					}

					if (bTestLogFarming)
					{
						for (int32 PlotIndex = 0; PlotIndex < UImmortalFarmingLibrary::GetMaximumPlotCount(); ++PlotIndex)
						{
							const FImmortalFarmingPlotView View = UImmortalFarmingLibrary::GetPlotView(
								FarmingState, PlotIndex, GetSpiritFieldLevel(), NowTicks);
							UE_LOG(LogTemp, Display,
								TEXT("Farming persistence plot: index=%d unlocked=%s crop=%s stage=%d remaining=%lld yield=%d"),
								PlotIndex, View.bUnlocked ? TEXT("true") : TEXT("false"), *View.CropId.ToString(),
								static_cast<int32>(View.GrowthStage), View.RemainingSeconds, View.PendingYield);
						}
						UE_LOG(LogTemp, Display,
							TEXT("Farming persistence audit: field=%d revision=%d last=%lld planted=%lld harvested=%lld items=%lld materials=%d/%d/%d saveVersion=%d"),
							GetSpiritFieldLevel(), FarmingState.Revision, FarmingState.LastSettlementUtcTicks,
							FarmingState.TotalCropsPlanted, FarmingState.TotalCropsHarvested, FarmingState.TotalItemsHarvested,
							GetMaterialQuantity(TEXT("SpiritGrass")), GetMaterialQuantity(TEXT("ImmortalFruit")),
							GetMaterialQuantity(TEXT("SpiritWood")), UImmortalPathSaveGame::CurrentSaveVersion);
					}
					SaveProgress();
					if ((bTestOpenFarming || bTestScreenshotFarming) && PlayerFarmingWidget && !bFarmingOpen)
					{
						ToggleFarming();
					}
					if (bTestScreenshotFarming)
					{
						FTimerHandle ScreenshotTimer;
						GetWorldTimerManager().SetTimer(
							ScreenshotTimer,
							FTimerDelegate::CreateWeakLambda(this, []
							{
								const FString ScreenshotPath = FPaths::Combine(
									FPaths::ProjectSavedDir(), TEXT("Screenshots/FarmingTest.png"));
								FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
								UE_LOG(LogTemp, Display, TEXT("Farming verification screenshot requested: %s"), *ScreenshotPath);
							}),
							1.5f,
							false);
					}
				}),
			1.2f,
			false);

		if (bTestScreenshotFarming && FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestExitAfterScreenshot")))
		{
			FTimerHandle ExitTimer;
			GetWorldTimerManager().SetTimer(
				ExitTimer,
				FTimerDelegate::CreateWeakLambda(this, [] { FPlatformMisc::RequestExit(false); }),
				6.0f,
				false);
		}
	}

	FString TestSectId;
	const bool bHasTestSect = FParse::Value(
		FCommandLine::Get(), TEXT("ImmortalTestSect="), TestSectId);
	int32 TestSectContribution = 0;
	const bool bHasTestSectContribution = FParse::Value(
		FCommandLine::Get(), TEXT("ImmortalTestGrantSectContribution="), TestSectContribution);
	FString TestSectExchange;
	const bool bHasTestSectExchange = FParse::Value(
		FCommandLine::Get(), TEXT("ImmortalTestSectExchange="), TestSectExchange);
	const bool bTestCompleteSectTasks = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestCompleteSectTasks"));
	const bool bTestClaimSectTasks = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestClaimSectTasks"));
	const bool bTestOpenSect = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestOpenSect"));
	const bool bTestScreenshotSect = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotSect"));
	const bool bTestLogSect = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestLogSect"));
	if (bHasTestSect || bHasTestSectContribution || bHasTestSectExchange
		|| bTestCompleteSectTasks || bTestClaimSectTasks || bTestOpenSect
		|| bTestScreenshotSect || bTestLogSect)
	{
		FTimerHandle SectVerificationTimer;
		GetWorldTimerManager().SetTimer(
			SectVerificationTimer,
			FTimerDelegate::CreateWeakLambda(this,
				[this, TestSectId, bHasTestSect, TestSectContribution, bHasTestSectContribution,
					TestSectExchange, bHasTestSectExchange, bTestCompleteSectTasks,
					bTestClaimSectTasks, bTestOpenSect, bTestScreenshotSect, bTestLogSect]
				{
					if (bHasTestSect)
					{
						const FImmortalSectJoinResult JoinResult = JoinSect(FName(*TestSectId));
						UE_LOG(LogTemp, Display,
							TEXT("Sect development join: id=%s success=%s requirements=%s locked=%s persistenceFailed=%s message=%s"),
							*TestSectId, JoinResult.bSucceeded ? TEXT("true") : TEXT("false"),
							JoinResult.bRequirementsMet ? TEXT("true") : TEXT("false"),
							JoinResult.bLockedToOtherSect ? TEXT("true") : TEXT("false"),
							JoinResult.bPersistenceFailed ? TEXT("true") : TEXT("false"),
							*JoinResult.Message.ToString());
					}
					if (bHasTestSectContribution && TestSectContribution > 0 && SectState.HasJoined())
					{
						const int32 SafeGrant = FMath::Clamp(TestSectContribution, 1, 1000000);
						const int32 Applied = static_cast<int32>(FMath::Min<int64>(
							SafeGrant, static_cast<int64>(MAX_int32) - SectState.Contribution));
						SectState.Contribution += Applied;
						SectState.TotalContributionEarned = FMath::Min<int64>(
							SectState.TotalContributionEarned + Applied, MAX_int64);
						SectState.Revision = SectState.Revision >= MAX_int32 ? MAX_int32 : SectState.Revision + 1;
						const bool bSaved = SaveProgress();
						if (bSaved) BP_OnSectStateChanged(SectState);
						UE_LOG(LogTemp, Display,
							TEXT("Sect development contribution grant: requested=%d applied=%d total=%d earned=%lld saved=%s"),
							TestSectContribution, Applied, SectState.Contribution,
							SectState.TotalContributionEarned, bSaved ? TEXT("true") : TEXT("false"));
					}
					if (bTestCompleteSectTasks)
					{
						NotifySectCombatProgress(20, 3, 1);
					}
					if (bTestClaimSectTasks)
					{
						for (const FImmortalSectTaskDefinition& Definition : UImmortalSectLibrary::GetDailyTaskDefinitions())
						{
							const FImmortalSectTaskClaimResult Claim = ClaimSectTask(Definition.TaskId);
							UE_LOG(LogTemp, Display,
								TEXT("Sect development task claim: id=%s success=%s awarded=%d contribution=%d message=%s"),
								*Definition.TaskId.ToString(), Claim.bSucceeded ? TEXT("true") : TEXT("false"),
								Claim.ContributionAwarded, Claim.ContributionAfter, *Claim.Message.ToString());
						}
					}
					if (bHasTestSectExchange)
					{
						const int32 ContributionBefore = SectState.Contribution;
						const TArray<FImmortalMaterialStack> MaterialsBefore = MaterialInventory;
						const int32 TechniqueCountBefore = TechniqueLibrary.Num();
						const FImmortalSectExchangeResult Exchange = ExchangeSectOffer(FName(*TestSectExchange));
						UE_LOG(LogTemp, Display,
							TEXT("Sect development exchange: id=%s success=%s persistenceFailed=%s contribution=%d->%d materialUnchanged=%s techniques=%d->%d reward=%d/%s x%d message=%s"),
							*TestSectExchange, Exchange.bSucceeded ? TEXT("true") : TEXT("false"),
							Exchange.bPersistenceFailed ? TEXT("true") : TEXT("false"),
							ContributionBefore, SectState.Contribution,
							HaveSameMaterialQuantities(MaterialsBefore, MaterialInventory) ? TEXT("true") : TEXT("false"),
							TechniqueCountBefore, TechniqueLibrary.Num(), static_cast<int32>(Exchange.RewardType),
							*Exchange.RewardId.ToString(), Exchange.RewardQuantity, *Exchange.Message.ToString());
					}
					if (bTestLogSect)
					{
						UE_LOG(LogTemp, Display,
							TEXT("Sect persistence audit: initialized=%s sect=%s contribution=%d earned=%lld spent=%lld day=%d last=%lld claimed=%lld revision=%d saveVersion=%d"),
							SectState.bInitialized ? TEXT("true") : TEXT("false"), *SectState.SectId.ToString(),
							SectState.Contribution, SectState.TotalContributionEarned,
							SectState.TotalContributionSpent, SectState.TaskDayKey,
							SectState.LastObservedUtcTicks, SectState.TotalTasksClaimed,
							SectState.Revision, UImmortalPathSaveGame::CurrentSaveVersion);
						for (const FImmortalSectTaskProgress& Progress : SectState.DailyTasks)
						{
							UE_LOG(LogTemp, Display, TEXT("Sect persistence task: id=%s progress=%d claimed=%s"),
								*Progress.TaskId.ToString(), Progress.Progress,
								Progress.bClaimed ? TEXT("true") : TEXT("false"));
						}
						for (const FImmortalSectOfferProgress& Progress : SectState.OfferProgress)
						{
							UE_LOG(LogTemp, Display, TEXT("Sect persistence offer: id=%s daily=%d total=%lld"),
								*Progress.OfferId.ToString(), Progress.DailyPurchaseCount, Progress.TotalPurchaseCount);
						}
					}
					if ((bTestOpenSect || bTestScreenshotSect) && PlayerSectWidget && !bSectOpen)
					{
						ToggleSect();
					}
					if (bTestScreenshotSect)
					{
						FTimerHandle ScreenshotTimer;
						GetWorldTimerManager().SetTimer(
							ScreenshotTimer,
							FTimerDelegate::CreateWeakLambda(this, []
							{
								const FString ScreenshotPath = FPaths::Combine(
									FPaths::ProjectSavedDir(), TEXT("Screenshots/SectTest.png"));
								FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
								UE_LOG(LogTemp, Display, TEXT("Sect verification screenshot requested: %s"), *ScreenshotPath);
							}),
							1.5f,
							false);
					}
				}),
			1.4f,
			false);

		if (bTestScreenshotSect && FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestExitAfterScreenshot")))
		{
			FTimerHandle ExitTimer;
			GetWorldTimerManager().SetTimer(
				ExitTimer,
				FTimerDelegate::CreateWeakLambda(this, [] { FPlatformMisc::RequestExit(false); }),
				6.5f,
				false);
		}
	}

	const bool bTestPrepareInventory = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestPrepareInventory"));
	const bool bTestPrepareEquipmentExpansion = FParse::Param(
		FCommandLine::Get(), TEXT("ImmortalTestPrepareEquipmentExpansion"));
	const bool bTestFillInventoryLocked = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestFillInventoryLocked"));
	const bool bTestProbeFullInventory = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestProbeFullInventory"));
	const bool bTestProbeMixedLockedInventory = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestProbeMixedLockedInventory"));
	const bool bTestProtectLockedEquipped = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestProtectLockedEquipped"));
	const bool bTestLockFirstInventory = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestLockFirstInventory"));
	const bool bTestLockFirstArtifact = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestLockFirstArtifact"));
	const bool bTestSortInventory = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestSortInventory"));
	const bool bTestDismantleFirst = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestDismantleFirst"));
	const bool bTestGrantQuestItems = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestGrantQuestItems"));
	const bool bTestOpenInventory = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestOpenInventory"));
	const bool bTestScreenshotInventory = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestScreenshotInventory"));
	const bool bTestLogInventory = FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestLogInventory"));
	FString TestInventoryBatchSell;
	const bool bHasTestInventoryBatchSell = FParse::Value(
		FCommandLine::Get(), TEXT("ImmortalTestInventoryBatchSell="), TestInventoryBatchSell);
	FString TestInventoryBatchDismantle;
	const bool bHasTestInventoryBatchDismantle = FParse::Value(
		FCommandLine::Get(), TEXT("ImmortalTestInventoryBatchDismantle="), TestInventoryBatchDismantle);
	FString TestInventoryTab;
	const bool bHasTestInventoryTab = FParse::Value(
		FCommandLine::Get(), TEXT("ImmortalTestInventoryTab="), TestInventoryTab);
	if (bTestPrepareInventory || bTestPrepareEquipmentExpansion || bTestFillInventoryLocked || bTestProbeFullInventory || bTestProbeMixedLockedInventory
		|| bTestProtectLockedEquipped || bTestLockFirstInventory || bTestLockFirstArtifact || bTestSortInventory || bTestDismantleFirst
		|| bTestGrantQuestItems || bHasTestInventoryBatchSell || bHasTestInventoryBatchDismantle
		|| bHasTestInventoryTab || bTestOpenInventory || bTestScreenshotInventory || bTestLogInventory)
	{
		FTimerHandle InventoryVerificationTimer;
		GetWorldTimerManager().SetTimer(
			InventoryVerificationTimer,
			FTimerDelegate::CreateWeakLambda(this,
				[this, bTestPrepareInventory, bTestPrepareEquipmentExpansion, bTestFillInventoryLocked, bTestProbeFullInventory, bTestProbeMixedLockedInventory,
					bTestProtectLockedEquipped, bTestLockFirstInventory, bTestLockFirstArtifact, bTestSortInventory, bTestDismantleFirst,
					bTestGrantQuestItems, bHasTestInventoryBatchSell, TestInventoryBatchSell,
					bHasTestInventoryBatchDismantle, TestInventoryBatchDismantle,
					bHasTestInventoryTab, TestInventoryTab, bTestOpenInventory,
					bTestScreenshotInventory, bTestLogInventory]
				{
						auto ParseQuality = [](const FString& Text)
					{
						if (Text.Equals(TEXT("Uncommon"), ESearchCase::IgnoreCase)
							|| Text.Equals(TEXT("Spirit"), ESearchCase::IgnoreCase)) return EImmortalEquipmentQuality::Uncommon;
						if (Text.Equals(TEXT("Rare"), ESearchCase::IgnoreCase)
							|| Text.Equals(TEXT("Mystic"), ESearchCase::IgnoreCase)) return EImmortalEquipmentQuality::Rare;
						if (Text.Equals(TEXT("Epic"), ESearchCase::IgnoreCase)
							|| Text.Equals(TEXT("Earth"), ESearchCase::IgnoreCase)) return EImmortalEquipmentQuality::Epic;
						if (Text.Equals(TEXT("Legendary"), ESearchCase::IgnoreCase)
							|| Text.Equals(TEXT("Heaven"), ESearchCase::IgnoreCase)) return EImmortalEquipmentQuality::Legendary;
						if (Text.Equals(TEXT("Immortal"), ESearchCase::IgnoreCase)) return EImmortalEquipmentQuality::Immortal;
						if (Text.Equals(TEXT("Divine"), ESearchCase::IgnoreCase)) return EImmortalEquipmentQuality::Divine;
						return EImmortalEquipmentQuality::Common;
					};

					if (bTestPrepareEquipmentExpansion)
					{
						InventoryItems.Reset();
						EquippedItems.Reset();
						const EImmortalEquipmentSlot DisplaySlots[] =
						{
							EImmortalEquipmentSlot::Weapon, EImmortalEquipmentSlot::Head,
							EImmortalEquipmentSlot::Chest, EImmortalEquipmentSlot::Bracers,
							EImmortalEquipmentSlot::Belt, EImmortalEquipmentSlot::Boots,
							EImmortalEquipmentSlot::RingLeft, EImmortalEquipmentSlot::RingRight,
							EImmortalEquipmentSlot::Accessory
						};
						const FName DisplaySets[] =
						{
							TEXT("QingyunSet"), TEXT("QingyunSet"), TEXT("QingyunSet"),
							TEXT("QingyunSet"), TEXT("QingyunSet"), TEXT("QingyunSet"),
							TEXT("HeavenlySwordSet"), TEXT("MyriadThunderSet"), TEXT("BlackTortoiseSet")
						};
						for (int32 Index = 0; Index < UE_ARRAY_COUNT(DisplaySlots); ++Index)
						{
							const EImmortalEquipmentQuality Quality = static_cast<EImmortalEquipmentQuality>(
								FMath::Clamp(Index / 2 + 2, 2, static_cast<int32>(EImmortalEquipmentQuality::Divine)));
							EquippedItems.Add(UImmortalEquipmentLibrary::GenerateCraftedEquipment(
								120 + Index * 10, DisplaySlots[Index], Quality,
								EImmortalEquipmentDiscipline::Universal, DisplaySets[Index]));
						}
						for (int32 QualityIndex = 0; QualityIndex <= static_cast<int32>(EImmortalEquipmentQuality::Divine); ++QualityIndex)
						{
							InventoryItems.Add(UImmortalEquipmentLibrary::GenerateCraftedEquipment(
								40 + QualityIndex * 15,
								DisplaySlots[QualityIndex],
								static_cast<EImmortalEquipmentQuality>(QualityIndex),
								EImmortalEquipmentDiscipline::Universal,
								QualityIndex >= static_cast<int32>(EImmortalEquipmentQuality::Rare)
									? DisplaySets[QualityIndex] : NAME_None));
						}
						ArtifactInventory.Reset();
						FImmortalArtifactItem Artifact = UImmortalArtifactLibrary::CreateArtifact(TEXT("XuanGuangSword"));
						if (Artifact.IsValid())
						{
							ArtifactInventory.Add(Artifact);
							EquippedArtifactInstanceId = Artifact.InstanceId;
						}
						++EquipmentInventoryRevision;
						++ArtifactInventoryRevision;
						RecalculateEquipmentBonuses();
						const bool bSaved = SaveProgress();
						FString SetSummary = GetEquipmentSetSummaryText().ToString();
						SetSummary.ReplaceInline(TEXT("\n"), TEXT("; "));
						UE_LOG(LogTemp, Display,
							TEXT("Equipment expansion development sample prepared: equipped=%d backpack=%d artifact=%s saved=%s setSummary=%s"),
							EquippedItems.Num(), InventoryItems.Num(), EquippedArtifactInstanceId.IsValid() ? TEXT("true") : TEXT("false"),
							bSaved ? TEXT("true") : TEXT("false"), *SetSummary);
					}

					if (bTestPrepareInventory)
					{
						InventoryItems.Reset();
						const EImmortalEquipmentSlot Slots[] =
						{
							EImmortalEquipmentSlot::Weapon, EImmortalEquipmentSlot::Head,
							EImmortalEquipmentSlot::Chest, EImmortalEquipmentSlot::Boots,
							EImmortalEquipmentSlot::Accessory, EImmortalEquipmentSlot::Weapon
						};
						const EImmortalEquipmentQuality Qualities[] =
						{
							EImmortalEquipmentQuality::Common, EImmortalEquipmentQuality::Common,
							EImmortalEquipmentQuality::Uncommon, EImmortalEquipmentQuality::Rare,
							EImmortalEquipmentQuality::Epic, EImmortalEquipmentQuality::Legendary
						};
						for (int32 Index = 0; Index < 6; ++Index)
						{
							FImmortalEquipmentItem Item = UImmortalEquipmentLibrary::GenerateCraftedEquipment(
								5 + Index * 7, Slots[Index], Qualities[Index]);
							Item.bLocked = false;
							InventoryItems.Add(Item);
						}
						++EquipmentInventoryRevision;
						const bool bSaved = SaveProgress();
						UE_LOG(LogTemp, Display, TEXT("Inventory development sample prepared: items=%d saved=%s"),
							InventoryItems.Num(), bSaved ? TEXT("true") : TEXT("false"));
					}
					if (bTestFillInventoryLocked)
					{
						InventoryItems.Reset();
						for (int32 Index = 0; Index < GetInventoryCapacity(); ++Index)
						{
							FImmortalEquipmentItem Item = UImmortalEquipmentLibrary::GenerateCraftedEquipment(
								1 + Index, static_cast<EImmortalEquipmentSlot>(Index % static_cast<int32>(EImmortalEquipmentSlot::MAX)),
								EImmortalEquipmentQuality::Common);
							Item.bLocked = true;
							InventoryItems.Add(Item);
						}
						++EquipmentInventoryRevision;
						const bool bSaved = SaveProgress();
						UE_LOG(LogTemp, Display, TEXT("Inventory development full locked backpack prepared: items=%d/%d saved=%s"),
							InventoryItems.Num(), GetInventoryCapacity(), bSaved ? TEXT("true") : TEXT("false"));
					}
					if (bTestProbeFullInventory)
					{
						const int32 BeforeCount = InventoryItems.Num();
						int32 LockedBefore = 0;
						for (const FImmortalEquipmentItem& Item : InventoryItems) LockedBefore += Item.bLocked ? 1 : 0;
						const FImmortalEquipmentItem Probe = UImmortalEquipmentLibrary::GenerateCraftedEquipment(
							999, EImmortalEquipmentSlot::Weapon, EImmortalEquipmentQuality::Legendary);
						const bool bStored = AddItemToInventory(Probe);
						int32 LockedAfter = 0;
						for (const FImmortalEquipmentItem& Item : InventoryItems) LockedAfter += Item.bLocked ? 1 : 0;
						UE_LOG(LogTemp, Display,
							TEXT("Inventory full-lock replacement probe: stored=%s count=%d->%d locked=%d->%d"),
							bStored ? TEXT("true") : TEXT("false"), BeforeCount, InventoryItems.Num(), LockedBefore, LockedAfter);
					}
					if (bTestProbeMixedLockedInventory)
					{
						InventoryItems.Reset();
						FImmortalEquipmentItem LockedWeakest = UImmortalEquipmentLibrary::GenerateCraftedEquipment(
							1, EImmortalEquipmentSlot::Weapon, EImmortalEquipmentQuality::Common);
						LockedWeakest.BaseAttackBonus = 0.0f;
						LockedWeakest.BaseDefenseBonus = 0.0f;
						LockedWeakest.BaseHealthBonus = 0.0f;
						LockedWeakest.BaseAttackSpeedBonus = 0.0f;
						LockedWeakest.BaseCriticalChanceBonus = 0.0f;
						LockedWeakest.Affixes.Reset();
						LockedWeakest.bLocked = true;
						UImmortalEquipmentLibrary::RebuildEquipmentStats(LockedWeakest);
						InventoryItems.Add(LockedWeakest);
						for (int32 Index = 1; Index < GetInventoryCapacity(); ++Index)
						{
							InventoryItems.Add(UImmortalEquipmentLibrary::GenerateCraftedEquipment(
								100 + Index, static_cast<EImmortalEquipmentSlot>(Index % static_cast<int32>(EImmortalEquipmentSlot::MAX)),
								EImmortalEquipmentQuality::Rare));
						}
						const int32 ReplaceableIndex = FindWeakestReplaceableInventoryItem();
						const FGuid ExpectedReplacedId = ReplaceableIndex == INDEX_NONE ? FGuid() : InventoryItems[ReplaceableIndex].ItemId;
						const float ReplaceablePower = ReplaceableIndex == INDEX_NONE ? -1.0f
							: UImmortalEquipmentLibrary::CalculateEquipmentPower(InventoryItems[ReplaceableIndex]);
						FImmortalEquipmentItem Probe = UImmortalEquipmentLibrary::GenerateCraftedEquipment(
							999, EImmortalEquipmentSlot::Weapon, EImmortalEquipmentQuality::Legendary);
						const bool bStored = AddItemToInventory(Probe);
						const bool bLockedPreserved = InventoryItems.ContainsByPredicate([&LockedWeakest](const FImmortalEquipmentItem& Item)
						{
							return Item.ItemId == LockedWeakest.ItemId && Item.bLocked;
						});
						const bool bExpectedRemoved = ExpectedReplacedId.IsValid() && !InventoryItems.ContainsByPredicate([ExpectedReplacedId](const FImmortalEquipmentItem& Item)
						{
							return Item.ItemId == ExpectedReplacedId;
						});
						if (bStored) ++EquipmentInventoryRevision;
						const bool bSaved = bStored && SaveProgress();
						UE_LOG(LogTemp, Display,
							TEXT("Inventory mixed-lock replacement probe: stored=%s saved=%s lockedPreserved=%s expectedRemoved=%s lockedPower=%.2f replaceablePower=%.2f lockedId=%s replacedId=%s probeId=%s"),
							bStored ? TEXT("true") : TEXT("false"), bSaved ? TEXT("true") : TEXT("false"),
							bLockedPreserved ? TEXT("true") : TEXT("false"), bExpectedRemoved ? TEXT("true") : TEXT("false"),
							UImmortalEquipmentLibrary::CalculateEquipmentPower(LockedWeakest), ReplaceablePower,
							*LockedWeakest.ItemId.ToString(), *ExpectedReplacedId.ToString(), *Probe.ItemId.ToString());
					}
					if (bTestProtectLockedEquipped)
					{
						if (EquippedItems.IsEmpty())
						{
							EquippedItems.Add(UImmortalEquipmentLibrary::GenerateCraftedEquipment(
								1, EImmortalEquipmentSlot::Weapon, EImmortalEquipmentQuality::Common,
								EImmortalEquipmentDiscipline::Universal));
						}
						FImmortalEquipmentItem& ProtectedItem = EquippedItems[0];
						if (!ProtectedItem.IsValid())
						{
							ProtectedItem = UImmortalEquipmentLibrary::GenerateCraftedEquipment(
								1, EImmortalEquipmentSlot::Weapon, EImmortalEquipmentQuality::Common,
								EImmortalEquipmentDiscipline::Universal);
						}
						ProtectedItem.Discipline = EImmortalEquipmentDiscipline::Universal;
						ProtectedItem.bLocked = true;
						const FGuid ProtectedId = ProtectedItem.ItemId;
						const EImmortalEquipmentSlot ProtectedSlot = ProtectedItem.Slot;
						FImmortalEquipmentItem StrongerItem = UImmortalEquipmentLibrary::GenerateCraftedEquipment(
							999, ProtectedSlot, EImmortalEquipmentQuality::Legendary,
							EImmortalEquipmentDiscipline::Universal);
						StrongerItem.AttackBonus += 100000.0f;
						StrongerItem.BaseAttackBonus += 100000.0f;
						const bool bStored = ProcessEquipmentItem(StrongerItem, false, true, false);
						const int32 ProtectedIndex = EquippedItems.IndexOfByPredicate([ProtectedId](const FImmortalEquipmentItem& Item)
						{
							return Item.ItemId == ProtectedId && Item.bLocked;
						});
						const bool bStrongerInBackpack = InventoryItems.ContainsByPredicate([&StrongerItem](const FImmortalEquipmentItem& Item)
						{
							return Item.ItemId == StrongerItem.ItemId;
						});
						UE_LOG(LogTemp, Display,
							TEXT("Inventory locked-equipped protection probe: stored=%s protected=%s strongerInBackpack=%s equippedId=%s probeId=%s"),
							bStored ? TEXT("true") : TEXT("false"), ProtectedIndex != INDEX_NONE ? TEXT("true") : TEXT("false"),
							bStrongerInBackpack ? TEXT("true") : TEXT("false"), *ProtectedId.ToString(), *StrongerItem.ItemId.ToString());
					}
					if (bTestGrantQuestItems)
					{
						ReceiveQuestItem(TEXT("QingyunTrialJadeSlip"), 2);
						ReceiveQuestItem(TEXT("AncientMapFragment"), 3);
						ReceiveQuestItem(TEXT("DemonKingSeal"), 1);
					}
					if (bTestLockFirstInventory && !InventoryItems.IsEmpty())
					{
						const FImmortalInventoryOperationResult LockResult = SetEquipmentLocked(InventoryItems[0].ItemId, true);
						UE_LOG(LogTemp, Display, TEXT("Inventory development lock: success=%s persistenceFailed=%s message=%s"),
							LockResult.bSucceeded ? TEXT("true") : TEXT("false"),
							LockResult.bPersistenceFailed ? TEXT("true") : TEXT("false"), *LockResult.Message.ToString());
					}
					if (bTestLockFirstArtifact && !ArtifactInventory.IsEmpty())
					{
						const FImmortalInventoryOperationResult LockResult = SetArtifactLocked(ArtifactInventory[0].InstanceId, true);
						UE_LOG(LogTemp, Display, TEXT("Inventory development artifact lock: success=%s persistenceFailed=%s message=%s"),
							LockResult.bSucceeded ? TEXT("true") : TEXT("false"),
							LockResult.bPersistenceFailed ? TEXT("true") : TEXT("false"), *LockResult.Message.ToString());
					}
					if (bTestSortInventory)
					{
						const FImmortalInventoryOperationResult SortResult = OrganizeInventory();
						UE_LOG(LogTemp, Display, TEXT("Inventory development sort: success=%s persistenceFailed=%s affected=%d message=%s"),
							SortResult.bSucceeded ? TEXT("true") : TEXT("false"),
							SortResult.bPersistenceFailed ? TEXT("true") : TEXT("false"),
							SortResult.AffectedItemCount, *SortResult.Message.ToString());
					}
					if (bTestDismantleFirst && !InventoryItems.IsEmpty())
					{
						const FImmortalInventoryOperationResult Salvage = DismantleEquipment(InventoryItems[0].ItemId);
						UE_LOG(LogTemp, Display, TEXT("Inventory development single dismantle: success=%s persistenceFailed=%s count=%d rewards=%s message=%s"),
							Salvage.bSucceeded ? TEXT("true") : TEXT("false"), Salvage.bPersistenceFailed ? TEXT("true") : TEXT("false"),
							Salvage.AffectedItemCount, *FormatInventoryMaterials(Salvage.MaterialRewards), *Salvage.Message.ToString());
					}
					if (bHasTestInventoryBatchSell)
					{
						const int32 GoldBefore = CurrentGold;
						const int32 CountBefore = InventoryItems.Num();
						const FImmortalInventoryOperationResult Sale = BatchSellEquipment(ParseQuality(TestInventoryBatchSell));
						UE_LOG(LogTemp, Display,
							TEXT("Inventory development batch sale: threshold=%s success=%s persistenceFailed=%s count=%d->%d stones=%d->%d delta=%d lockedSkipped=%d message=%s"),
							*TestInventoryBatchSell, Sale.bSucceeded ? TEXT("true") : TEXT("false"),
							Sale.bPersistenceFailed ? TEXT("true") : TEXT("false"), CountBefore, InventoryItems.Num(),
							GoldBefore, CurrentGold, Sale.SpiritStoneDelta, Sale.SkippedLockedItemCount, *Sale.Message.ToString());
					}
					if (bHasTestInventoryBatchDismantle)
					{
						const int32 CountBefore = InventoryItems.Num();
						const TArray<FImmortalMaterialStack> MaterialsBefore = MaterialInventory;
						const FImmortalInventoryOperationResult Salvage = BatchDismantleEquipment(ParseQuality(TestInventoryBatchDismantle));
						UE_LOG(LogTemp, Display,
							TEXT("Inventory development batch dismantle: threshold=%s success=%s persistenceFailed=%s count=%d->%d materialsUnchanged=%s rewards=%s lockedSkipped=%d message=%s"),
							*TestInventoryBatchDismantle, Salvage.bSucceeded ? TEXT("true") : TEXT("false"),
							Salvage.bPersistenceFailed ? TEXT("true") : TEXT("false"), CountBefore, InventoryItems.Num(),
							HaveSameMaterialQuantities(MaterialsBefore, MaterialInventory) ? TEXT("true") : TEXT("false"),
							*FormatInventoryMaterials(Salvage.MaterialRewards), Salvage.SkippedLockedItemCount, *Salvage.Message.ToString());
					}

					if (bTestLogInventory)
					{
						int32 LockedEquipment = 0;
						for (int32 Index = 0; Index < InventoryItems.Num(); ++Index)
						{
							const FImmortalEquipmentItem& Item = InventoryItems[Index];
							LockedEquipment += Item.bLocked ? 1 : 0;
							UE_LOG(LogTemp, Display,
								TEXT("Inventory persistence equipment: index=%d id=%s slot=%d quality=%d set=%s affixes=%d level=%d enhancement=%d locked=%s power=%.2f"),
								Index, *Item.ItemId.ToString(), static_cast<int32>(Item.Slot), static_cast<int32>(Item.Quality),
								*Item.SetId.ToString(), Item.Affixes.Num(), Item.ItemLevel, Item.EnhancementLevel, Item.bLocked ? TEXT("true") : TEXT("false"),
								UImmortalEquipmentLibrary::CalculateEquipmentPower(Item));
						}
						for (int32 Index = 0; Index < EquippedItems.Num(); ++Index)
						{
							const FImmortalEquipmentItem& Item = EquippedItems[Index];
							UE_LOG(LogTemp, Display,
								TEXT("Inventory persistence equipped: index=%d id=%s slot=%d quality=%d set=%s affixes=%d level=%d enhancement=%d locked=%s power=%.2f"),
								Index, *Item.ItemId.ToString(), static_cast<int32>(Item.Slot), static_cast<int32>(Item.Quality),
								*Item.SetId.ToString(), Item.Affixes.Num(), Item.ItemLevel, Item.EnhancementLevel, Item.bLocked ? TEXT("true") : TEXT("false"),
								UImmortalEquipmentLibrary::CalculateEquipmentPower(Item));
						}
						for (const FImmortalMaterialStack& Stack : MaterialInventory)
						{
							UE_LOG(LogTemp, Display, TEXT("Inventory persistence material: id=%s quantity=%d"),
								*Stack.MaterialId.ToString(), Stack.Quantity);
						}
						for (const FImmortalPillStack& Stack : PillInventory)
						{
							UE_LOG(LogTemp, Display, TEXT("Inventory persistence pill: id=%s quality=%d quantity=%d"),
								*Stack.PillId.ToString(), static_cast<int32>(Stack.Quality), Stack.Quantity);
						}
						int32 LockedArtifacts = 0;
						for (const FImmortalArtifactItem& Item : ArtifactInventory)
						{
							LockedArtifacts += Item.bLocked ? 1 : 0;
							UE_LOG(LogTemp, Display,
								TEXT("Inventory persistence artifact: id=%s artifact=%s level=%d stars=%d locked=%s equipped=%s"),
								*Item.InstanceId.ToString(), *Item.ArtifactId.ToString(), Item.Level, Item.Stars,
								Item.bLocked ? TEXT("true") : TEXT("false"),
								Item.InstanceId == EquippedArtifactInstanceId ? TEXT("true") : TEXT("false"));
						}
						UE_LOG(LogTemp, Display,
							TEXT("Inventory persistence audit: backpack=%d/%d locked=%d equipmentRevision=%d materials=%d/%d pills=%d/%d artifacts=%d/locked%d/rev%d quest=%d/rev%d stones=%d saveVersion=%d"),
							InventoryItems.Num(), GetInventoryCapacity(), LockedEquipment, EquipmentInventoryRevision,
							MaterialInventory.Num(), MaterialInventoryRevision, PillInventory.Num(), PillInventoryRevision,
							ArtifactInventory.Num(), LockedArtifacts, ArtifactInventoryRevision,
							QuestItemInventory.Num(), QuestItemInventoryRevision, CurrentGold,
							UImmortalPathSaveGame::CurrentSaveVersion);
						for (const FImmortalQuestItemStack& Stack : QuestItemInventory)
						{
							UE_LOG(LogTemp, Display, TEXT("Inventory persistence quest item: id=%s quantity=%d"),
								*Stack.QuestItemId.ToString(), Stack.Quantity);
						}
					}
					if ((bHasTestInventoryTab || bTestOpenInventory || bTestScreenshotInventory) && PlayerInventoryWidget)
					{
						if (!bInventoryOpen) ToggleInventory();
						if (bHasTestInventoryTab)
						{
							if (TestInventoryTab.Equals(TEXT("Material"), ESearchCase::IgnoreCase)) PlayerInventoryWidget->ShowMaterialTab();
							else if (TestInventoryTab.Equals(TEXT("Pill"), ESearchCase::IgnoreCase)) PlayerInventoryWidget->ShowPillTab();
							else if (TestInventoryTab.Equals(TEXT("Artifact"), ESearchCase::IgnoreCase)) PlayerInventoryWidget->ShowArtifactTab();
							else if (TestInventoryTab.Equals(TEXT("Quest"), ESearchCase::IgnoreCase)) PlayerInventoryWidget->ShowQuestItemTab();
							else PlayerInventoryWidget->ShowCategory(EImmortalInventoryCategory::Equipment);
						}
					}
					if (bTestScreenshotInventory)
					{
						FTimerHandle ScreenshotTimer;
						GetWorldTimerManager().SetTimer(
							ScreenshotTimer,
							FTimerDelegate::CreateWeakLambda(this, []
							{
								const FString ScreenshotPath = FPaths::Combine(
									FPaths::ProjectSavedDir(), TEXT("Screenshots/InventoryTest.png"));
								FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
								UE_LOG(LogTemp, Display, TEXT("Inventory verification screenshot requested: %s"), *ScreenshotPath);
							}),
							1.5f,
							false);
					}
				}),
			1.6f,
			false);

		if (bTestScreenshotInventory && FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestExitAfterScreenshot")))
		{
			FTimerHandle ExitTimer;
			GetWorldTimerManager().SetTimer(
				ExitTimer,
				FTimerDelegate::CreateWeakLambda(this, [] { FPlatformMisc::RequestExit(false); }),
				6.5f,
				false);
		}
	}
#endif

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestDisableAutoBattle")))
	{
		bAutoAttackOnBeginPlay = false;
		GetWorldTimerManager().ClearTimer(AutoAttackTimerHandle);
		UE_LOG(LogTemp, Display, TEXT("Development verification disabled automatic battle"));
	}
#endif
	if (bAdventureSuspendedForDeathRecovery)
	{
		bAdventureSuspendedForDeathRecovery = true;
		StopAutoAttack();
		GetWorldTimerManager().SetTimer(
			DeathCultivationStartupTimerHandle,
			this,
			&AImmortalPlayerCharacter::ApplyPersistedDeathCultivationRecovery,
			0.15f,
			false);
	}
	else if (bAutoAttackOnBeginPlay)
	{
		StartAutoAttack();
	}

	RefreshCultivationHud();
	if (CultivationComponent)
	{
#if !UE_BUILD_SHIPPING
		float TestRateMultiplier = 0.0f;
		if (FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestCultivationRate="), TestRateMultiplier))
		{
			CultivationComponent->SetRuntimeRateMultiplier(FMath::Max(TestRateMultiplier, 0.0f));
			UE_LOG(LogTemp, Display, TEXT("Cultivation development rate override applied: %.1fx"), TestRateMultiplier);
		}
#endif
		CultivationComponent->StartCultivating();
		GetWorldTimerManager().SetTimer(
			CultivationAutosaveTimerHandle,
			this,
			&AImmortalPlayerCharacter::AutosaveCultivationProgress,
			15.0f,
			true);
	}
	GetWorldTimerManager().SetTimer(
		ShopDailyRefreshTimerHandle,
		this,
		&AImmortalPlayerCharacter::CheckDailyShopRefresh,
		60.0f,
		true);
	GetWorldTimerManager().SetTimer(
		CaveProductionTimerHandle,
		this,
		&AImmortalPlayerCharacter::HandleCaveProductionTick,
		1.0f,
		true);

#if !UE_BUILD_SHIPPING
	const bool bTestDeathCultivationGate = FParse::Param(
		FCommandLine::Get(), TEXT("ImmortalTestDeathCultivationGate"));
	const bool bTestDeathCultivationPersistOnly = FParse::Param(
		FCommandLine::Get(), TEXT("ImmortalTestDeathCultivationPersistOnly"));
	const bool bTestDeathCultivationStartupAudit = FParse::Param(
		FCommandLine::Get(), TEXT("ImmortalTestDeathCultivationStartupAudit"));
	if (bTestDeathCultivationGate || bTestDeathCultivationPersistOnly)
	{
		FTimerHandle FatalDamageTimer;
		GetWorldTimerManager().SetTimer(
			FatalDamageTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this]
				{
					ArtifactShield = 0.0f;
					TechniqueShield = 0.0f;
					CultivationPathShield = 0.0f;
					CurrentHealth = 1.0f;
					FDamageEvent FatalDamageEvent;
					TakeDamage(
						1000000.0f,
						FatalDamageEvent,
						nullptr,
						this);
					UE_LOG(
						LogTemp,
						Display,
						TEXT("Death cultivation fixture fatal hit: dead=%s recoveryRequired=%s"),
						bDead ? TEXT("true") : TEXT("false"),
						bDeathCultivationRecoveryRequired
							? TEXT("true") : TEXT("false"));
				}),
			0.80f,
			false);

		FTimerHandle DeathShotTimer;
		GetWorldTimerManager().SetTimer(
			DeathShotTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[]
				{
					FScreenshotRequest::RequestScreenshot(
						FPaths::Combine(
							FPaths::ProjectSavedDir(),
							TEXT("Screenshots/DeathRecovery_Death_TBH.png")),
						true,
						false);
				}),
			1.25f,
			false);

		FTimerHandle LockedAuditTimer;
		GetWorldTimerManager().SetTimer(
			LockedAuditTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this]
				{
					AImmortalMonsterSpawner* Spawner = FindMapSpawner();
					UE_LOG(
						LogTemp,
						Display,
						TEXT("Death cultivation fixture locked audit: required=%s suspended=%s managementOpen=%s activeFeature=%d autoAttack=%s spawning=%s alive=%d"),
						bDeathCultivationRecoveryRequired ? TEXT("true") : TEXT("false"),
						bAdventureSuspendedForDeathRecovery ? TEXT("true") : TEXT("false"),
						bManagementInterfaceOpen ? TEXT("true") : TEXT("false"),
						static_cast<int32>(ActiveManagementFeature),
						GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)
							? TEXT("true") : TEXT("false"),
						Spawner && Spawner->IsAdventureSpawningRequested()
							? TEXT("true") : TEXT("false"),
						Spawner ? Spawner->GetAliveMonsterCount() : -1);
					FScreenshotRequest::RequestScreenshot(
						FPaths::Combine(
							FPaths::ProjectSavedDir(),
							TEXT("Screenshots/DeathRecovery_CultivationLocked_TBH.png")),
						true,
						false);
				}),
			2.10f,
			false);

		FTimerHandle BlockedReturnTimer;
		GetWorldTimerManager().SetTimer(
			BlockedReturnTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this]
				{
					CloseManagementInterface();
					UE_LOG(
						LogTemp,
						Display,
						TEXT("Death cultivation fixture blocked return audit: managementOpen=%s activeFeature=%d required=%s"),
						bManagementInterfaceOpen ? TEXT("true") : TEXT("false"),
						static_cast<int32>(ActiveManagementFeature),
						bDeathCultivationRecoveryRequired
							? TEXT("true") : TEXT("false"));
				}),
			2.55f,
			false);

		if (bTestDeathCultivationPersistOnly)
		{
			FTimerHandle PersistExitTimer;
			GetWorldTimerManager().SetTimer(
				PersistExitTimer,
				FTimerDelegate::CreateWeakLambda(
					this,
					[] { FPlatformMisc::RequestExit(false); }),
				2.90f,
				false);
		}
		else
		{
			FTimerHandle BreakthroughTimer;
			GetWorldTimerManager().SetTimer(
				BreakthroughTimer,
				FTimerDelegate::CreateWeakLambda(
					this,
					[this]
					{
						if (CultivationComponent
							&& !CultivationComponent->HasReachedAscension())
						{
							CultivationComponent->AddCultivation(
								FMath::Max(
									CultivationComponent->GetRequiredCultivation()
									- CultivationComponent->GetCurrentCultivation(),
									1));
						}
					}),
				4.20f,
				false);

			FTimerHandle ResumeAuditTimer;
			GetWorldTimerManager().SetTimer(
				ResumeAuditTimer,
				FTimerDelegate::CreateWeakLambda(
					this,
					[this]
					{
						const bool bUnlocked =
							!bDeathCultivationRecoveryRequired;
						CloseManagementInterface();
						AImmortalMonsterSpawner* Spawner = FindMapSpawner();
						UE_LOG(
							LogTemp,
							Display,
							TEXT("Death cultivation fixture resume audit: unlocked=%s managementOpen=%s suspended=%s autoAttack=%s spawning=%s alive=%d"),
							bUnlocked ? TEXT("true") : TEXT("false"),
							bManagementInterfaceOpen ? TEXT("true") : TEXT("false"),
							bAdventureSuspendedForDeathRecovery ? TEXT("true") : TEXT("false"),
							GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)
								? TEXT("true") : TEXT("false"),
							Spawner && Spawner->IsAdventureSpawningRequested()
								? TEXT("true") : TEXT("false"),
							Spawner ? Spawner->GetAliveMonsterCount() : -1);
						FScreenshotRequest::RequestScreenshot(
							FPaths::Combine(
								FPaths::ProjectSavedDir(),
								TEXT("Screenshots/DeathRecovery_AdventureResumed_TBH.png")),
							true,
							false);
					}),
				4.80f,
				false);

			FTimerHandle GateExitTimer;
			GetWorldTimerManager().SetTimer(
				GateExitTimer,
				FTimerDelegate::CreateWeakLambda(
					this,
					[] { FPlatformMisc::RequestExit(false); }),
				6.10f,
				false);
		}
	}

	if (bTestDeathCultivationStartupAudit)
	{
		FTimerHandle StartupAuditTimer;
		GetWorldTimerManager().SetTimer(
			StartupAuditTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this]
				{
					AImmortalMonsterSpawner* Spawner = FindMapSpawner();
					UE_LOG(
						LogTemp,
						Display,
						TEXT("Death cultivation restart audit: required=%s suspended=%s managementOpen=%s activeFeature=%d autoAttack=%s spawning=%s alive=%d"),
						bDeathCultivationRecoveryRequired ? TEXT("true") : TEXT("false"),
						bAdventureSuspendedForDeathRecovery ? TEXT("true") : TEXT("false"),
						bManagementInterfaceOpen ? TEXT("true") : TEXT("false"),
						static_cast<int32>(ActiveManagementFeature),
						GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)
							? TEXT("true") : TEXT("false"),
						Spawner && Spawner->IsAdventureSpawningRequested()
							? TEXT("true") : TEXT("false"),
						Spawner ? Spawner->GetAliveMonsterCount() : -1);
					FScreenshotRequest::RequestScreenshot(
						FPaths::Combine(
							FPaths::ProjectSavedDir(),
							TEXT("Screenshots/DeathRecovery_RestartLocked_TBH.png")),
						true,
						false);
				}),
			1.20f,
			false);

		FTimerHandle RestartExitTimer;
		GetWorldTimerManager().SetTimer(
			RestartExitTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[] { FPlatformMisc::RequestExit(false); }),
			2.20f,
			false);
	}
#endif

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestMortalPlayerAnimations")))
	{
		StopAutoAttack();
		auto RequestAnimationScreenshot = [this](const TCHAR* Filename, const TCHAR* State)
		{
			const FString ScreenshotPath = FPaths::Combine(
				FPaths::ProjectSavedDir(),
				FString::Printf(TEXT("Screenshots/%s"), Filename));
			FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
			UPaperFlipbookComponent* SpriteComponent = GetSprite();
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Mortal player runtime animation audit: state=%s flipbook=%s looping=%s screenshot=%s"),
				State,
				SpriteComponent && SpriteComponent->GetFlipbook()
					? *SpriteComponent->GetFlipbook()->GetPathName()
					: TEXT("none"),
				SpriteComponent && SpriteComponent->IsLooping() ? TEXT("true") : TEXT("false"),
				*ScreenshotPath);
		};

		FTimerHandle IdleScreenshotTimer;
		GetWorldTimerManager().SetTimer(
			IdleScreenshotTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this, RequestAnimationScreenshot]
				{
					FinishMortalRealmOneShotAnimation();
					RequestAnimationScreenshot(TEXT("MortalPlayer_Idle_TBH.png"), TEXT("Idle"));
				}),
			0.8f,
			false);

		FTimerHandle AttackPreviewTimer;
		GetWorldTimerManager().SetTimer(
			AttackPreviewTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this] { PlayMortalRealmAttackAnimation(); }),
			1.2f,
			false);
		FTimerHandle AttackScreenshotTimer;
		GetWorldTimerManager().SetTimer(
			AttackScreenshotTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[RequestAnimationScreenshot]
				{
					RequestAnimationScreenshot(TEXT("MortalPlayer_Attack_TBH.png"), TEXT("Attack"));
				}),
			1.45f,
			false);

		FTimerHandle HurtPreviewTimer;
		GetWorldTimerManager().SetTimer(
			HurtPreviewTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this] { PlayMortalRealmHurtAnimation(); }),
			2.0f,
			false);
		FTimerHandle HurtScreenshotTimer;
		GetWorldTimerManager().SetTimer(
			HurtScreenshotTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[RequestAnimationScreenshot]
				{
					RequestAnimationScreenshot(TEXT("MortalPlayer_Hurt_TBH.png"), TEXT("Hurt"));
				}),
			2.25f,
			false);

		FTimerHandle DeathPreviewTimer;
		GetWorldTimerManager().SetTimer(
			DeathPreviewTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this] { PlayMortalRealmDeathAnimation(); }),
			2.8f,
			false);
		FTimerHandle DeathScreenshotTimer;
		GetWorldTimerManager().SetTimer(
			DeathScreenshotTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[RequestAnimationScreenshot]
				{
					RequestAnimationScreenshot(TEXT("MortalPlayer_Death_TBH.png"), TEXT("Death"));
				}),
			3.35f,
			false);

		FTimerHandle RestoreTimer;
		GetWorldTimerManager().SetTimer(
			RestoreTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this, RequestAnimationScreenshot]
				{
					FinishMortalRealmOneShotAnimation();
					RequestAnimationScreenshot(TEXT("MortalPlayer_RestoredIdle_TBH.png"), TEXT("RestoredIdle"));
				}),
			4.25f,
			false);

		FTimerHandle ExitTimer;
		GetWorldTimerManager().SetTimer(
			ExitTimer,
			FTimerDelegate::CreateWeakLambda(
				this,
				[] { FPlatformMisc::RequestExit(false); }),
			5.5f,
			false);
	}
#endif
}

void AImmortalPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (PlayerInputComponent)
	{
		PlayerInputComponent->BindKey(EKeys::I, IE_Pressed, this, &AImmortalPlayerCharacter::ToggleInventory);
		PlayerInputComponent->BindKey(EKeys::L, IE_Pressed, this, &AImmortalPlayerCharacter::ToggleAlchemy);
		PlayerInputComponent->BindKey(EKeys::K, IE_Pressed, this, &AImmortalPlayerCharacter::ToggleCrafting);
		PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AImmortalPlayerCharacter::ToggleArtifacts);
		PlayerInputComponent->BindKey(EKeys::G, IE_Pressed, this, &AImmortalPlayerCharacter::ToggleTechniques);
		PlayerInputComponent->BindKey(EKeys::H, IE_Pressed, this, &AImmortalPlayerCharacter::ToggleCharacterBuild);
		PlayerInputComponent->BindKey(EKeys::B, IE_Pressed, this, &AImmortalPlayerCharacter::ToggleShop);
		PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &AImmortalPlayerCharacter::ToggleMapSelection);
		PlayerInputComponent->BindKey(EKeys::C, IE_Pressed, this, &AImmortalPlayerCharacter::ToggleCave);
		PlayerInputComponent->BindKey(EKeys::J, IE_Pressed, this, &AImmortalPlayerCharacter::ToggleSect);
		PlayerInputComponent->BindKey(EKeys::V, IE_Pressed, this, &AImmortalPlayerCharacter::ToggleWorldBoss);
		PlayerInputComponent->BindKey(EKeys::N, IE_Pressed, this, &AImmortalPlayerCharacter::ToggleEndlessDungeon);
		PlayerInputComponent->BindKey(EKeys::P, IE_Pressed, this, &AImmortalPlayerCharacter::TogglePet);
		PlayerInputComponent->BindKey(EKeys::U, IE_Pressed, this, &AImmortalPlayerCharacter::ToggleAscension);
		PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AImmortalPlayerCharacter::HandleManagementToggleInput);
		PlayerInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AImmortalPlayerCharacter::HandleEscapePressed);
	}
}

void AImmortalPlayerCharacter::SetManagementFeatureOpenFlags(
	const EImmortalManagementFeature Feature)
{
	bInventoryOpen = Feature == EImmortalManagementFeature::Inventory;
	bAlchemyOpen = Feature == EImmortalManagementFeature::Alchemy;
	bCraftingOpen = Feature == EImmortalManagementFeature::Crafting;
	bArtifactOpen = Feature == EImmortalManagementFeature::Artifact;
	bTechniqueOpen = Feature == EImmortalManagementFeature::Technique;
	bCharacterBuildOpen =
		Feature == EImmortalManagementFeature::CharacterBuild;
	bShopOpen = Feature == EImmortalManagementFeature::Shop;
	bMapSelectionOpen = Feature == EImmortalManagementFeature::Map;
	bQuestOpen = Feature == EImmortalManagementFeature::Quest;
	bCaveOpen = Feature == EImmortalManagementFeature::Cave;
	bFarmingOpen = Feature == EImmortalManagementFeature::Farming;
	bSectOpen = Feature == EImmortalManagementFeature::Sect;
	bWorldBossOpen = Feature == EImmortalManagementFeature::WorldBoss;
	bEndlessDungeonOpen =
		Feature == EImmortalManagementFeature::EndlessDungeon;
	bPetOpen = Feature == EImmortalManagementFeature::Pet;
	bSettingsOpen = Feature == EImmortalManagementFeature::Settings;
}


void AImmortalPlayerCharacter::OpenAscensionInterface()
{
	if (bDeathCultivationRecoveryRequired)
	{
		OpenManagementFeature(
			EImmortalManagementFeature::Cultivation);
		QueueManagementNotification(
			FText::FromString(TEXT(
				"历练失败后需先完成修炼恢复，当前不能离开修炼界面。")),
			FLinearColor(1.0f, 0.58f, 0.32f, 1.0f),
			5.0f);
		return;
	}
	if (!PlayerAscensionWidget)
	{
		return;
	}
	if (PlayerManagementWidget)
	{
		PlayerManagementWidget->SetVisibility(
			ESlateVisibility::Collapsed);
	}
	bManagementInterfaceOpen = false;
	SetManagementFeatureOpenFlags(EImmortalManagementFeature::Home);
	bAscensionOpen = true;
	PlayerAscensionWidget->RefreshFromPlayer();
	PlayerAscensionWidget->SetVisibility(ESlateVisibility::Visible);
	if (PlayerStatusWidget)
	{
		PlayerStatusWidget->SetVisibility(
			ESlateVisibility::Collapsed);
	}
	ConfigureModalWidget(PlayerAscensionWidget, true);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Independent ascension interface opened: combatPaused=%s autoAttackActive=%s"),
		UGameplayStatics::IsGamePaused(this) ? TEXT("true") : TEXT("false"),
		GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)
			? TEXT("true")
			: TEXT("false"));
}

void AImmortalPlayerCharacter::ToggleInventory()
{
	if (PlayerManagementWidget)
	{
		ToggleManagementFeature(EImmortalManagementFeature::Inventory);
		return;
	}
	if (!PlayerInventoryWidget)
	{
		return;
	}

	const bool bWantsOpen = !bInventoryOpen;
	if (bWantsOpen) CloseAllModalWidgetsExcept(PlayerInventoryWidget);
	PlayerInventoryWidget->ResetTransientInteraction();
	bInventoryOpen = bWantsOpen;
	PlayerInventoryWidget->SetVisibility(bInventoryOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bInventoryOpen) PlayerInventoryWidget->RefreshFromPlayer();
	ConfigureModalWidget(PlayerInventoryWidget, bInventoryOpen);
	if (bInventoryOpen)
	{
		UE_LOG(LogTemp, Display, TEXT("Inventory opened: equipped %d | backpack %d/%d | material types %d | pill stacks %d | artifacts %d | quest types %d | combat power %.2f"),
			EquippedItems.Num(), InventoryItems.Num(), GetInventoryCapacity(), MaterialInventory.Num(), PillInventory.Num(),
			ArtifactInventory.Num(), QuestItemInventory.Num(), GetCombatPower());
	}
}

void AImmortalPlayerCharacter::ToggleAlchemy()
{
	if (PlayerManagementWidget)
	{
		ToggleManagementFeature(EImmortalManagementFeature::Alchemy);
		return;
	}
	if (!PlayerAlchemyWidget)
	{
		return;
	}
	const bool bWantsOpen = !bAlchemyOpen;
	if (bWantsOpen) CloseAllModalWidgetsExcept(PlayerAlchemyWidget);
	bAlchemyOpen = bWantsOpen;
	PlayerAlchemyWidget->SetVisibility(bAlchemyOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bAlchemyOpen) PlayerAlchemyWidget->RefreshFromPlayer();
	ConfigureModalWidget(PlayerAlchemyWidget, bAlchemyOpen);
	if (bAlchemyOpen)
	{
		UE_LOG(LogTemp, Display, TEXT("Alchemy furnace opened: material types %d | pill stacks %d | boost x%.2f for %.0fs"),
			MaterialInventory.Num(), PillInventory.Num(), GetAlchemyBoostMultiplier(), GetAlchemyBoostRemainingSeconds());
	}
}

void AImmortalPlayerCharacter::ToggleCrafting()
{
	if (PlayerManagementWidget)
	{
		ToggleManagementFeature(EImmortalManagementFeature::Crafting);
		return;
	}
	if (!PlayerCraftingWidget) return;
	const bool bWantsOpen = !bCraftingOpen;
	if (bWantsOpen) CloseAllModalWidgetsExcept(PlayerCraftingWidget);
	bCraftingOpen = bWantsOpen;
	PlayerCraftingWidget->SetVisibility(bCraftingOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bCraftingOpen) PlayerCraftingWidget->RefreshFromPlayer();
	ConfigureModalWidget(PlayerCraftingWidget, bCraftingOpen);
	if (bCraftingOpen)
	{
		UE_LOG(LogTemp, Display, TEXT("Crafting furnace opened: stage %d | spirit stones %d | materials %d | equipment %d"),
			DisplayedStage, CurrentGold, MaterialInventory.Num(), EquippedItems.Num() + InventoryItems.Num());
	}
}

void AImmortalPlayerCharacter::ToggleArtifacts()
{
	if (PlayerManagementWidget)
	{
		ToggleManagementFeature(EImmortalManagementFeature::Artifact);
		return;
	}
	if (!PlayerArtifactWidget) return;
	const bool bWantsOpen = !bArtifactOpen;
	if (bWantsOpen) CloseAllModalWidgetsExcept(PlayerArtifactWidget);
	bArtifactOpen = bWantsOpen;
	PlayerArtifactWidget->SetVisibility(bArtifactOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bArtifactOpen) PlayerArtifactWidget->RefreshFromPlayer();
	ConfigureModalWidget(PlayerArtifactWidget, bArtifactOpen);
	if (bArtifactOpen)
	{
		UE_LOG(LogTemp, Display, TEXT("Artifact furnace opened: stage %d | spirit stones %d | artifacts %d | materials %d"),
			DisplayedStage, CurrentGold, ArtifactInventory.Num(), MaterialInventory.Num());
	}
}

void AImmortalPlayerCharacter::ToggleTechniques()
{
	if (PlayerManagementWidget)
	{
		ToggleManagementFeature(EImmortalManagementFeature::Technique);
		return;
	}
	if (!PlayerTechniqueWidget) return;
	const bool bWantsOpen = !bTechniqueOpen;
	if (bWantsOpen) CloseAllModalWidgetsExcept(PlayerTechniqueWidget);
	bTechniqueOpen = bWantsOpen;
	PlayerTechniqueWidget->SetVisibility(bTechniqueOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bTechniqueOpen) PlayerTechniqueWidget->RefreshFromPlayer();
	ConfigureModalWidget(PlayerTechniqueWidget, bTechniqueOpen);
	if (bTechniqueOpen)
	{
		UE_LOG(LogTemp, Display, TEXT("Technique library opened: learned %d | equipped %d | insight %d | cultivation %d"),
			TechniqueLibrary.Num(), EquippedTechniqueIds.Num(), TechniqueInsightPoints, CurrentCultivation);
	}
}

void AImmortalPlayerCharacter::ToggleCharacterBuild()
{
	if (PlayerManagementWidget)
	{
		ToggleManagementFeature(
			EImmortalManagementFeature::CharacterBuild);
		return;
	}
	if (!PlayerCharacterBuildWidget) return;
	const bool bWantsOpen = !bCharacterBuildOpen;
	if (bWantsOpen) CloseAllModalWidgetsExcept(PlayerCharacterBuildWidget);
	bCharacterBuildOpen = bWantsOpen;
	PlayerCharacterBuildWidget->SetVisibility(bCharacterBuildOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bCharacterBuildOpen) PlayerCharacterBuildWidget->RefreshFromPlayer();
	ConfigureModalWidget(PlayerCharacterBuildWidget, bCharacterBuildOpen);
	if (bCharacterBuildOpen)
	{
		UE_LOG(LogTemp, Display, TEXT("Character build screen opened: root %d purity %.2f | path %d switches %d | cultivation rate x%.3f"),
			static_cast<int32>(SpiritRootState.Root), SpiritRootState.Purity,
			static_cast<int32>(CultivationPathState.Path), CultivationPathState.SwitchCount,
			CharacterPathCultivationRateMultiplier);
	}
}

void AImmortalPlayerCharacter::ToggleShop()
{
	if (PlayerManagementWidget)
	{
		ToggleManagementFeature(EImmortalManagementFeature::Shop);
		return;
	}
	if (!PlayerShopWidget) return;
	const bool bWantsOpen = !bShopOpen;
	if (bWantsOpen)
	{
		CloseAllModalWidgetsExcept(PlayerShopWidget);
		EnsureDailyShopRefresh();
	}
	bShopOpen = bWantsOpen;
	PlayerShopWidget->SetVisibility(bShopOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bShopOpen) PlayerShopWidget->RefreshFromPlayer();
	ConfigureModalWidget(PlayerShopWidget, bShopOpen);
	if (bShopOpen)
	{
		int32 SoldOut = 0;
		for (const FImmortalShopListing& Listing : ShopState.Listings) SoldOut += Listing.bSoldOut ? 1 : 0;
		UE_LOG(LogTemp, Display, TEXT("Treasure Pavilion opened: day %d | serial %d | listings %d | sold out %d | spirit stones %d"),
			ShopState.RefreshDayKey, ShopState.RefreshSerial, ShopState.Listings.Num(), SoldOut, CurrentGold);
	}
}

void AImmortalPlayerCharacter::ToggleMapSelection()
{
	if (PlayerManagementWidget)
	{
		ToggleManagementFeature(EImmortalManagementFeature::Map);
		return;
	}
	if (!PlayerMapWidget) return;
	const bool bWantsOpen = !bMapSelectionOpen;
	if (bWantsOpen) CloseAllModalWidgetsExcept(PlayerMapWidget);
	bMapSelectionOpen = bWantsOpen;
	PlayerMapWidget->SetVisibility(bMapSelectionOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bMapSelectionOpen) PlayerMapWidget->SelectMap(GetActiveMapId());
	ConfigureModalWidget(PlayerMapWidget, bMapSelectionOpen);
	if (bMapSelectionOpen)
	{
		UE_LOG(LogTemp, Display, TEXT("Adventure map selector opened: active %s | stage %d | realm %d"),
			*GetActiveMapId().ToString(), GetActiveMapStage(), static_cast<int32>(GetCultivationRealm()));
	}
}

void AImmortalPlayerCharacter::ToggleCave()
{
	if (PlayerManagementWidget)
	{
		ToggleManagementFeature(EImmortalManagementFeature::Cave);
		return;
	}
	if (!PlayerCaveWidget) return;
	const bool bWantsOpen = !bCaveOpen;
	if (bWantsOpen)
	{
		SettleCaveProduction();
		CloseAllModalWidgetsExcept(PlayerCaveWidget);
	}
	bCaveOpen = bWantsOpen;
	PlayerCaveWidget->SetVisibility(bCaveOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bCaveOpen) PlayerCaveWidget->RefreshFromPlayer();
	ConfigureModalWidget(PlayerCaveWidget, bCaveOpen);
	if (bCaveOpen)
	{
		const FImmortalCaveProductionSnapshot Snapshot = GetCaveProductionSnapshot();
		UE_LOG(LogTemp, Display,
			TEXT("Cave opened: revision %d | stored stones=%d grass=%d ore=%d | cultivation x%.2f | forge discount %.1f%%"),
			CaveState.Revision, CaveState.StoredSpiritStones, CaveState.StoredSpiritGrass, CaveState.StoredOre,
			Snapshot.CultivationRateMultiplier, Snapshot.ForgeSpiritStoneDiscount * 100.0f);
	}
}

void AImmortalPlayerCharacter::ToggleFarming()
{
	if (PlayerManagementWidget)
	{
		ToggleManagementFeature(EImmortalManagementFeature::Farming);
		return;
	}
	if (!PlayerFarmingWidget) return;
	const bool bWantsOpen = !bFarmingOpen;
	if (bWantsOpen)
	{
		SettleFarmingGrowth();
		CloseAllModalWidgetsExcept(PlayerFarmingWidget);
	}
	bFarmingOpen = bWantsOpen;
	PlayerFarmingWidget->SetVisibility(bFarmingOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bFarmingOpen) PlayerFarmingWidget->RefreshFromPlayer();
	ConfigureModalWidget(PlayerFarmingWidget, bFarmingOpen);
	if (bFarmingOpen)
	{
		UE_LOG(LogTemp, Display,
			TEXT("Spirit field opened: level=%d unlocked=%d/%d revision=%d"),
			GetSpiritFieldLevel(),
			UImmortalFarmingLibrary::GetUnlockedPlotCount(GetSpiritFieldLevel()),
			UImmortalFarmingLibrary::GetMaximumPlotCount(),
			FarmingState.Revision);
	}
}

void AImmortalPlayerCharacter::ToggleSect()
{
	if (PlayerManagementWidget)
	{
		ToggleManagementFeature(EImmortalManagementFeature::Sect);
		return;
	}
	if (!PlayerSectWidget) return;
	const bool bWantsOpen = !bSectOpen;
	if (bWantsOpen)
	{
		EnsureSectDailyState();
		CloseAllModalWidgetsExcept(PlayerSectWidget);
	}
	bSectOpen = bWantsOpen;
	PlayerSectWidget->SetVisibility(bSectOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bSectOpen) PlayerSectWidget->RefreshFromPlayer();
	ConfigureModalWidget(PlayerSectWidget, bSectOpen);
	if (bSectOpen)
	{
		UE_LOG(LogTemp, Display,
			TEXT("Sect screen opened: sect=%s contribution=%d earned=%lld tasks=%d revision=%d"),
			*SectState.SectId.ToString(), SectState.Contribution, SectState.TotalContributionEarned,
			SectState.DailyTasks.Num(), SectState.Revision);
	}
}

void AImmortalPlayerCharacter::ToggleWorldBoss()
{
	if (PlayerManagementWidget)
	{
		ToggleManagementFeature(EImmortalManagementFeature::WorldBoss);
		return;
	}
	if (!PlayerWorldBossWidget)
	{
		return;
	}
	const bool bWantsOpen = !bWorldBossOpen;
	if (bWantsOpen)
	{
		CloseAllModalWidgetsExcept(PlayerWorldBossWidget);
		RetryPendingWorldBossRewards();
	}
	bWorldBossOpen = bWantsOpen;
	PlayerWorldBossWidget->SetVisibility(
		bWorldBossOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bWorldBossOpen)
	{
		PlayerWorldBossWidget->RefreshFromPlayer();
	}
	ConfigureModalWidget(PlayerWorldBossWidget, bWorldBossOpen);
	if (bWorldBossOpen)
	{
		const FImmortalWorldBossRuntimeSnapshot Runtime = GetWorldBossRuntimeSnapshot();
		UE_LOG(LogTemp, Display,
			TEXT("World Boss screen opened: bosses=%d pendingRewards=%d active=%s revision=%d"),
			UImmortalWorldBossLibrary::GetKnownWorldBossIds().Num(),
			WorldBossState.PendingRewards.Num(),
			Runtime.bActive ? TEXT("true") : TEXT("false"),
			WorldBossState.Revision);
	}
}

void AImmortalPlayerCharacter::ToggleEndlessDungeon()
{
	if (PlayerManagementWidget)
	{
		ToggleManagementFeature(
			EImmortalManagementFeature::EndlessDungeon);
		return;
	}
	if (!PlayerEndlessDungeonWidget)
	{
		return;
	}
	const bool bWantsOpen = !bEndlessDungeonOpen;
	if (bWantsOpen)
	{
		CloseAllModalWidgetsExcept(PlayerEndlessDungeonWidget);
		RetryPendingEndlessDungeonRewards();
	}
	bEndlessDungeonOpen = bWantsOpen;
	PlayerEndlessDungeonWidget->SetVisibility(
		bEndlessDungeonOpen
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	if (bEndlessDungeonOpen)
	{
		PlayerEndlessDungeonWidget->RefreshFromPlayer();
	}
	ConfigureModalWidget(
		PlayerEndlessDungeonWidget,
		bEndlessDungeonOpen);
	if (bEndlessDungeonOpen)
	{
		const FImmortalEndlessDungeonRuntimeSnapshot Runtime =
			GetEndlessDungeonRuntimeSnapshot();
		UE_LOG(LogTemp, Display,
			TEXT("Endless Dungeon screen opened: highest=%d totalFloors=%lld runs=%d pendingRewards=%d active=%s revision=%d"),
			EndlessDungeonState.HighestClearedFloor,
			EndlessDungeonState.TotalFloorsCleared,
			EndlessDungeonState.TotalRuns,
			EndlessDungeonState.PendingRewards.Num(),
			Runtime.bActive ? TEXT("true") : TEXT("false"),
			EndlessDungeonState.Revision);
	}
}

void AImmortalPlayerCharacter::TogglePet()
{
	if (PlayerManagementWidget)
	{
		ToggleManagementFeature(EImmortalManagementFeature::Pet);
		return;
	}
	if (!PlayerPetWidget)
	{
		return;
	}
	const bool bWantsOpen = !bPetOpen;
	if (bWantsOpen)
	{
		CloseAllModalWidgetsExcept(PlayerPetWidget);
	}
	bPetOpen = bWantsOpen;
	PlayerPetWidget->SetVisibility(
		bPetOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bPetOpen)
	{
		PlayerPetWidget->RefreshFromPlayer();
	}
	ConfigureModalWidget(PlayerPetWidget, bPetOpen);
	if (bPetOpen)
	{
		UE_LOG(LogTemp, Display,
			TEXT("Pet screen opened: active=%s pets=%d kills=%lld revision=%d"),
			*PetState.ActivePetId.ToString(),
			PetState.Pets.Num(),
			PetState.TotalCombatKills,
			PetState.Revision);
	}
}

void AImmortalPlayerCharacter::ToggleAscension()
{
	if (PlayerManagementWidget)
	{
		if (bAscensionOpen)
		{
			OpenManagementFeature(
				EImmortalManagementFeature::Cultivation);
		}
		else
		{
			OpenAscensionInterface();
		}
		return;
	}
	if (!PlayerAscensionWidget)
	{
		return;
	}
	const bool bWantsOpen = !bAscensionOpen;
	if (bWantsOpen)
	{
		CloseAllModalWidgetsExcept(PlayerAscensionWidget);
	}
	bAscensionOpen = bWantsOpen;
	PlayerAscensionWidget->SetVisibility(
		bAscensionOpen
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	if (bAscensionOpen)
	{
		PlayerAscensionWidget->RefreshFromPlayer();
	}
	ConfigureModalWidget(
		PlayerAscensionWidget, bAscensionOpen);
	if (bAscensionOpen)
	{
		const FImmortalAscensionEligibility Eligibility =
			EvaluateAscensionEligibility();
		UE_LOG(LogTemp, Display,
			TEXT("Ascension screen opened: count=%d seals=%d paths=%d/%d/%d eligible=%s revision=%d"),
			AscensionState.AscensionCount,
			AscensionState.ImmortalSeals,
			AscensionState.BattlePathRank,
			AscensionState.EnlightenmentPathRank,
			AscensionState.FortunePathRank,
			Eligibility.bEligible ? TEXT("true") : TEXT("false"),
			AscensionState.Revision);
	}
}

void AImmortalPlayerCharacter::HandleEscapePressed()
{
	if (bAscensionOpen && PlayerManagementWidget)
	{
		OpenManagementFeature(
			EImmortalManagementFeature::Cultivation);
		return;
	}
	if (bManagementInterfaceOpen && PlayerManagementWidget)
	{
		if (ActiveManagementFeature
			!= EImmortalManagementFeature::Home)
		{
			OpenManagementFeature(
				EImmortalManagementFeature::Home);
		}
		else
		{
			CloseManagementInterface();
		}
		return;
	}
	if (bSettingsOpen)
	{
		ToggleSettings();
		return;
	}
	const bool bAnyGameplayModalOpen =
		bInventoryOpen
		|| bAlchemyOpen
		|| bCraftingOpen
		|| bArtifactOpen
		|| bTechniqueOpen
		|| bCharacterBuildOpen
		|| bShopOpen
		|| bMapSelectionOpen
		|| bCaveOpen
		|| bFarmingOpen
		|| bSectOpen
		|| bWorldBossOpen
		|| bEndlessDungeonOpen
		|| bPetOpen
		|| bAscensionOpen;
	if (bAnyGameplayModalOpen)
	{
		CloseAllModalWidgetsExcept(nullptr);
		ConfigureModalWidget(nullptr, false);
		return;
	}
	ToggleSettings();
}

void AImmortalPlayerCharacter::ToggleSettings()
{
	if (PlayerManagementWidget)
	{
		ToggleManagementFeature(EImmortalManagementFeature::Settings);
		return;
	}
	if (!PlayerSettingsWidget)
	{
		return;
	}
	const bool bWantsOpen = !bSettingsOpen;
	if (bWantsOpen)
	{
		CloseAllModalWidgetsExcept(PlayerSettingsWidget);
	}
	bSettingsOpen = bWantsOpen;
	PlayerSettingsWidget->SetVisibility(
		bSettingsOpen
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	if (bSettingsOpen)
	{
		PlayerSettingsWidget->RefreshFromPlayer();
	}
	ConfigureModalWidget(PlayerSettingsWidget, bSettingsOpen);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Desktop settings %s: topmost=%s muted=%s fps=%d height=%d"),
		bSettingsOpen ? TEXT("opened") : TEXT("closed"),
		IsDesktopAlwaysOnTopEnabled() ? TEXT("true") : TEXT("false"),
		IsDesktopMuted() ? TEXT("true") : TEXT("false"),
		GetDesktopFrameRateLimit(),
		GetDesktopWindowHeight());
}

bool AImmortalPlayerCharacter::IsDesktopAlwaysOnTopEnabled() const
{
	const UImmortalDesktopSettings* Settings =
		GetDefault<UImmortalDesktopSettings>();
	return Settings
		? Settings->bAlwaysOnTop
		: bTaskbarWindowAlwaysOnTop;
}

bool AImmortalPlayerCharacter::IsDesktopMuted() const
{
	const UImmortalDesktopSettings* Settings =
		GetDefault<UImmortalDesktopSettings>();
	return Settings ? Settings->bMuted : false;
}

int32 AImmortalPlayerCharacter::GetDesktopFrameRateLimit() const
{
	const UImmortalDesktopSettings* Settings =
		GetDefault<UImmortalDesktopSettings>();
	return Settings
		? Settings->FrameRateLimit
		: UImmortalDesktopSettings::HighFrameRateLimit;
}

int32 AImmortalPlayerCharacter::GetDesktopWindowHeight() const
{
	const UImmortalDesktopSettings* Settings =
		GetDefault<UImmortalDesktopSettings>();
	return Settings
		? Settings->WindowHeight
		: TaskbarWindowHeight;
}

void AImmortalPlayerCharacter::ToggleDesktopAlwaysOnTop()
{
	UImmortalDesktopSettings* Settings =
		UImmortalDesktopSettings::GetMutable();
	if (!Settings) return;
	Settings->bAlwaysOnTop = !Settings->bAlwaysOnTop;
	Settings->SaveToDisk();
	bTaskbarWindowAlwaysOnTop = Settings->bAlwaysOnTop;
	ApplyTaskbarWindowPlacement();
}

void AImmortalPlayerCharacter::ToggleDesktopMute()
{
	UImmortalDesktopSettings* Settings =
		UImmortalDesktopSettings::GetMutable();
	if (!Settings) return;
	Settings->bMuted = !Settings->bMuted;
	Settings->SaveToDisk();
	FApp::SetVolumeMultiplier(
		Settings->bMuted ? 0.0f : 1.0f);
}

bool AImmortalPlayerCharacter::IsDesktopTransparent() const
{
	return UImmortalDesktopSettings::GetMutable()->bTransparentBackground;
}

bool AImmortalPlayerCharacter::ToggleDesktopTransparency()
{
	UImmortalDesktopSettings* Settings = UImmortalDesktopSettings::GetMutable();
	const bool bNewValue = !Settings->bTransparentBackground;
	if (!ImmortalDesktopWindow::ApplyTransparency(GetWorld(), bNewValue)) return false;
	Settings->bTransparentBackground = bNewValue;
	Settings->SaveToDisk();
	return true;
}

void AImmortalPlayerCharacter::CycleDesktopFrameRateLimit()
{
	UImmortalDesktopSettings* Settings =
		UImmortalDesktopSettings::GetMutable();
	if (!Settings) return;
	Settings->CycleFrameRateLimit();
	Settings->SaveToDisk();
	if (GEngine)
	{
		GEngine->SetMaxFPS(
			static_cast<float>(Settings->FrameRateLimit));
	}
}

bool AImmortalPlayerCharacter::MinimizeDesktopWindow()
{
#if PLATFORM_WINDOWS
	if (!GetWorld()) return false;
	const EWorldType::Type WorldType = GetWorld()->WorldType;
	if (WorldType == EWorldType::PIE
		|| WorldType == EWorldType::Editor
		|| WorldType == EWorldType::EditorPreview)
	{
		return false;
	}
	if (!GEngine || !GEngine->GameViewport) return false;
	const TSharedPtr<SWindow> GameWindow =
		GEngine->GameViewport->GetWindow();
	if (!GameWindow.IsValid()
		|| !GameWindow->GetNativeWindow().IsValid())
	{
		return false;
	}
	HWND WindowHandle = static_cast<HWND>(
		GameWindow->GetNativeWindow()->GetOSWindowHandle());
	if (!WindowHandle) return false;
	ShowWindow(WindowHandle, SW_MINIMIZE);
	UE_LOG(LogTemp, Display, TEXT("TBH window minimized"));
	return true;
#else
	return false;
#endif
}

bool AImmortalPlayerCharacter::SaveAndQuitDesktop()
{
	if (!GetWorld()) return false;
	const EWorldType::Type WorldType = GetWorld()->WorldType;
	if (WorldType == EWorldType::PIE
		|| WorldType == EWorldType::Editor
		|| WorldType == EWorldType::EditorPreview)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Save-and-quit is disabled in editor preview"));
		return false;
	}
	FImmortalMapSystemState CurrentMapState =
		GetMapSystemState();
	UImmortalMapLibrary::NormalizeState(CurrentMapState);
	if (!SaveProgressWithMapOverride(&CurrentMapState))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Save-and-quit aborted because the atomic player/map write failed"));
		return false;
	}
	bSaveAndQuitRequested = true;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Save-and-quit committed: map=%s stage=%d"),
		*CurrentMapState.ActiveMapId.ToString(),
		GetActiveMapStage());
	ImmortalDesktopWindow::PrepareForExit(GetWorld());
	FPlatformMisc::RequestExit(false);
	return true;
}

bool AImmortalPlayerCharacter::GetPetProgress(
	const FName PetId,
	FImmortalPetProgress& OutProgress) const
{
	return UImmortalPetLibrary::GetPetProgress(
		PetState, PetId, OutProgress);
}

FImmortalPetOperationResult AImmortalPlayerCharacter::UnlockPet(
	const FName PetId)
{
	FImmortalPetOperationResult Result;
	Result.PetId = PetId;
	FImmortalPetDefinition Definition;
	FImmortalPetProgress Progress;
	if (!UImmortalPetLibrary::GetPetDefinition(PetId, Definition)
		|| !GetPetProgress(PetId, Progress))
	{
		Result.Message = FText::FromString(TEXT("灵宠目录中不存在该灵兽"));
		return Result;
	}
	if (Progress.bOwned)
	{
		Result.bSucceeded = true;
		Result.bAlreadyOwned = true;
		Result.bAffordable = true;
		Result.Message = FText::FromString(FString::Printf(
			TEXT("%s已经认主"),
			*Definition.DisplayName.ToString()));
		return Result;
	}
	Result.bAffordable = UImmortalCraftingLibrary::CanAfford(
		MaterialInventory, CurrentGold, Definition.UnlockCost);
	if (!Result.bAffordable)
	{
		Result.Message = FText::FromString(FString::Printf(
			TEXT("驯服%s所需资源不足：%s"),
			*Definition.DisplayName.ToString(),
			*UImmortalCraftingLibrary::FormatCost(
				Definition.UnlockCost,
				MaterialInventory,
				CurrentGold).ToString()));
		return Result;
	}

	const FImmortalPetState PreviousPetState = PetState;
	const TArray<FImmortalMaterialStack> PreviousMaterials =
		MaterialInventory;
	const int32 PreviousGold = CurrentGold;
	const int32 PreviousMaterialRevision = MaterialInventoryRevision;
	if (!UImmortalCraftingLibrary::ConsumeCost(
		MaterialInventory, CurrentGold, Definition.UnlockCost)
		|| !UImmortalPetLibrary::UnlockPet(PetState, PetId))
	{
		PetState = PreviousPetState;
		MaterialInventory = PreviousMaterials;
		CurrentGold = PreviousGold;
		Result.Message = FText::FromString(TEXT("灵宠驯服事务未能完成"));
		return Result;
	}
	++MaterialInventoryRevision;
	if (ShouldForcePetPersistenceFailure(TEXT("Unlock"))
		|| !SaveProgress())
	{
		PetState = PreviousPetState;
		MaterialInventory = PreviousMaterials;
		CurrentGold = PreviousGold;
		MaterialInventoryRevision = PreviousMaterialRevision;
		Result.Message = FText::FromString(TEXT("存档写入失败，驯服消耗已全部回滚"));
		return Result;
	}

	Result.bSucceeded = true;
	Result.Message = FText::FromString(FString::Printf(
		TEXT("%s已认主，可在灵宠界面选择出战"),
		*Definition.DisplayName.ToString()));
	PublishMaterialInventoryDiff(PreviousMaterials);
	BP_OnPetStateChanged(PetState);
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->ShowBossAnnouncement(
			Result.Message,
			Definition.DisplayColor,
			5.0f);
	}
	UE_LOG(LogTemp, Display,
		TEXT("Pet unlocked atomically: %s | stones %d -> %d | pets=%d revision=%d"),
		*PetId.ToString(),
		PreviousGold,
		CurrentGold,
		PetState.Pets.Num(),
		PetState.Revision);
	return Result;
}

FImmortalPetOperationResult AImmortalPlayerCharacter::SetActivePet(
	const FName PetId)
{
	FImmortalPetOperationResult Result;
	Result.PetId = PetId;
	FImmortalPetDefinition Definition;
	FImmortalPetProgress Progress;
	if (!UImmortalPetLibrary::GetPetDefinition(PetId, Definition)
		|| !GetPetProgress(PetId, Progress))
	{
		Result.Message = FText::FromString(TEXT("灵宠目录中不存在该灵兽"));
		return Result;
	}
	if (!Progress.bOwned)
	{
		Result.Message = FText::FromString(TEXT("该灵宠尚未认主"));
		return Result;
	}
	Result.bAffordable = true;
	if (PetState.ActivePetId == PetId)
	{
		Result.bSucceeded = true;
		Result.bAlreadyOwned = true;
		Result.Message = FText::FromString(FString::Printf(
			TEXT("%s正在出战"),
			*Definition.DisplayName.ToString()));
		return Result;
	}

	const FImmortalPetState PreviousState = PetState;
	if (!UImmortalPetLibrary::SetActivePet(PetState, PetId)
		|| ShouldForcePetPersistenceFailure(TEXT("Equip"))
		|| !SaveProgress())
	{
		PetState = PreviousState;
		Result.Message = FText::FromString(TEXT("存档写入失败，出战灵宠未改变"));
		return Result;
	}
	RefreshActivePetActor();
	Result.bSucceeded = true;
	Result.Message = FText::FromString(FString::Printf(
		TEXT("%s已出战"),
		*Definition.DisplayName.ToString()));
	BP_OnPetStateChanged(PetState);
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->ShowBossAnnouncement(
			Result.Message,
			Definition.DisplayColor,
			5.0f);
	}
	UE_LOG(LogTemp, Display,
		TEXT("Active pet switched atomically: %s -> %s | revision=%d"),
		*PreviousState.ActivePetId.ToString(),
		*PetState.ActivePetId.ToString(),
		PetState.Revision);
	return Result;
}

FImmortalPetOperationResult AImmortalPlayerCharacter::RaisePetStar(
	const FName PetId)
{
	FImmortalPetOperationResult Result;
	Result.PetId = PetId;
	FImmortalPetDefinition Definition;
	FImmortalPetProgress Progress;
	if (!UImmortalPetLibrary::GetPetDefinition(PetId, Definition)
		|| !GetPetProgress(PetId, Progress))
	{
		Result.Message = FText::FromString(TEXT("灵宠目录中不存在该灵兽"));
		return Result;
	}
	if (!Progress.bOwned)
	{
		Result.Message = FText::FromString(TEXT("未认主的灵宠无法升星"));
		return Result;
	}
	if (Progress.Stars >= UImmortalPetLibrary::MaximumPetStars)
	{
		Result.bAffordable = true;
		Result.Message = FText::FromString(TEXT("该灵宠已经达到五星"));
		return Result;
	}
	const FImmortalCraftingCost Cost =
		UImmortalPetLibrary::GetStarUpCost(Progress);
	Result.bAffordable = UImmortalCraftingLibrary::CanAfford(
		MaterialInventory, CurrentGold, Cost);
	if (!Result.bAffordable)
	{
		Result.Message = FText::FromString(FString::Printf(
			TEXT("%s升星资源不足：%s"),
			*Definition.DisplayName.ToString(),
			*UImmortalCraftingLibrary::FormatCost(
				Cost, MaterialInventory, CurrentGold).ToString()));
		return Result;
	}

	const FImmortalPetState PreviousPetState = PetState;
	const TArray<FImmortalMaterialStack> PreviousMaterials =
		MaterialInventory;
	const int32 PreviousGold = CurrentGold;
	const int32 PreviousMaterialRevision = MaterialInventoryRevision;
	if (!UImmortalCraftingLibrary::ConsumeCost(
		MaterialInventory, CurrentGold, Cost)
		|| !UImmortalPetLibrary::RaiseStar(PetState, PetId))
	{
		PetState = PreviousPetState;
		MaterialInventory = PreviousMaterials;
		CurrentGold = PreviousGold;
		Result.Message = FText::FromString(TEXT("灵宠升星事务未能完成"));
		return Result;
	}
	++MaterialInventoryRevision;
	if (ShouldForcePetPersistenceFailure(TEXT("Star"))
		|| !SaveProgress())
	{
		PetState = PreviousPetState;
		MaterialInventory = PreviousMaterials;
		CurrentGold = PreviousGold;
		MaterialInventoryRevision = PreviousMaterialRevision;
		Result.Message = FText::FromString(TEXT("存档写入失败，升星消耗已全部回滚"));
		return Result;
	}
	RefreshActivePetActor();
	FImmortalPetProgress NewProgress;
	GetPetProgress(PetId, NewProgress);
	Result.bSucceeded = true;
	Result.Message = FText::FromString(FString::Printf(
		TEXT("%s提升至 %d 星"),
		*Definition.DisplayName.ToString(),
		NewProgress.Stars));
	PublishMaterialInventoryDiff(PreviousMaterials);
	BP_OnPetStateChanged(PetState);
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->ShowBossAnnouncement(
			Result.Message,
			Definition.DisplayColor,
			5.0f);
	}
	UE_LOG(LogTemp, Display,
		TEXT("Pet star raised atomically: %s %d -> %d | stones %d -> %d | revision=%d"),
		*PetId.ToString(),
		Progress.Stars,
		NewProgress.Stars,
		PreviousGold,
		CurrentGold,
		PetState.Revision);
	return Result;
}

bool AImmortalPlayerCharacter::SpawnActivePetActor()
{
	DespawnActivePetActor();
	if (!GetWorld() || PetState.ActivePetId.IsNone())
	{
		return false;
	}
	FImmortalPetProgress Progress;
	FImmortalPetDefinition Definition;
	if (!GetPetProgress(PetState.ActivePetId, Progress)
		|| !Progress.bOwned
		|| !UImmortalPetLibrary::GetPetDefinition(
			PetState.ActivePetId, Definition))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector SpawnLocation = GetActorLocation()
		+ FVector(Definition.FollowOffsetX, 0.0f, 6.0f);
	ActivePetActor = GetWorld()->SpawnActor<AImmortalPetCharacter>(
		AImmortalPetCharacter::StaticClass(),
		SpawnLocation,
		GetActorRotation(),
		SpawnParameters);
	if (!ActivePetActor
		|| !ActivePetActor->InitializeForPlayer(
			this, PetState.ActivePetId))
	{
		DespawnActivePetActor();
		UE_LOG(LogTemp, Error,
			TEXT("Failed to spawn active pet actor: %s"),
			*PetState.ActivePetId.ToString());
		return false;
	}
	UE_LOG(LogTemp, Display,
		TEXT("Active pet actor spawned: %s at %s"),
		*PetState.ActivePetId.ToString(),
		*SpawnLocation.ToCompactString());
	return true;
}

void AImmortalPlayerCharacter::DespawnActivePetActor()
{
	if (IsValid(ActivePetActor))
	{
		ActivePetActor->Destroy();
	}
	ActivePetActor = nullptr;
}

void AImmortalPlayerCharacter::RefreshActivePetActor()
{
	if (!IsValid(ActivePetActor)
		|| ActivePetActor->GetPetId() != PetState.ActivePetId)
	{
		SpawnActivePetActor();
		return;
	}
	ActivePetActor->RefreshFromPersistentState();
}

float AImmortalPlayerCharacter::ResolvePetAttack(
	AImmortalPetCharacter* SourcePet,
	AActor* Target,
	const float RequestedDamageOverride)
{
	if (bDead || !IsValid(SourcePet) || SourcePet != ActivePetActor
		|| SourcePet->GetOwner() != this
		|| SourcePet->GetPetId() != PetState.ActivePetId
		|| !IsTargetAttackable(Target, false))
	{
		return 0.0f;
	}
	FImmortalPetDefinition Definition;
	FImmortalPetProgress Progress;
	if (!UImmortalPetLibrary::GetPetDefinition(
			PetState.ActivePetId, Definition)
		|| !GetPetProgress(PetState.ActivePetId, Progress)
		|| !Progress.bOwned)
	{
		return 0.0f;
	}

	const bool bCritical =
		FMath::FRand() < FMath::Clamp(
			Definition.CriticalChance, 0.0f, 1.0f);
	const AImmortalMonsterCharacter* TargetMonster =
		Cast<AImmortalMonsterCharacter>(Target);
	const bool bTargetWasAlive =
		TargetMonster && !TargetMonster->IsDead();
	const float BaseRequestedDamage = RequestedDamageOverride > 0.0f
		? RequestedDamageOverride
		: GetTotalAttackDamage()
			* UImmortalPetLibrary::CalculateDamageRatio(
				Definition, Progress);
	const float AppliedDamage = ApplyOutgoingDamage(
		Target,
		FMath::Max(BaseRequestedDamage, 0.0f)
			* (bCritical ? 1.5f : 1.0f));
	if (AppliedDamage > 0.0f && CombatFeedbackWidget)
	{
		CombatFeedbackWidget->ShowDamage(
			Target->GetActorLocation()
				+ FVector(0.0f, 0.0f, 92.0f),
			AppliedDamage,
			bCritical,
			false);
	}
	if (AppliedDamage > 0.0f)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("Pet attack resolved: pet=%s target=%s damage=%.1f critical=%s damageCauser=Player"),
			*SourcePet->GetPetId().ToString(),
			*GetNameSafe(Target),
			AppliedDamage,
			bCritical ? TEXT("true") : TEXT("false"));
		if (bTargetWasAlive && TargetMonster->IsDead())
		{
			UE_LOG(LogTemp, Display,
				TEXT("Pet lethal strike settled through player attribution: pet=%s target=%s damageCauser=Player duplicateRewards=0"),
				*SourcePet->GetPetId().ToString(),
				*GetNameSafe(Target));
		}
	}
	return AppliedDamage;
}

void AImmortalPlayerCharacter::NotifyPetCombatKill(
	AImmortalMonsterCharacter* DefeatedMonster)
{
	if (!DefeatedMonster || PetState.ActivePetId.IsNone())
	{
		return;
	}

	int32 DifficultyIndex = GetActiveMapStage();
	bool bElite = false;
	bool bBoss = DefeatedMonster->IsBoss();
	const bool bWorldBoss = DefeatedMonster->IsWorldBoss();
	if (DefeatedMonster->IsEndlessEnemy())
	{
		DifficultyIndex = DefeatedMonster->GetEndlessFloor();
		bElite = DefeatedMonster->IsEndlessElite();
	}
	else if (bWorldBoss)
	{
		FImmortalWorldBossDefinition Definition;
		if (UImmortalWorldBossLibrary::GetWorldBossDefinition(
			DefeatedMonster->GetWorldBossId(), Definition))
		{
			DifficultyIndex = Definition.RecommendedStage;
		}
	}
	const int32 Experience =
		UImmortalPetLibrary::CalculateCombatExperienceReward(
			DifficultyIndex, bElite, bBoss, bWorldBoss);
	const FImmortalPetExperienceResult Growth =
		UImmortalPetLibrary::GrantActivePetExperience(
			PetState, Experience, 1);
	if (!Growth.bSucceeded)
	{
		return;
	}
	// Real damage always reads the latest persistent level/star values. Do
	// not rebuild the runtime actor on each kill: doing so would clear a
	// different target that is already inside the pet's attack windup.
	BP_OnPetStateChanged(PetState);
	if (Growth.LevelsGained > 0 && CombatFeedbackWidget)
	{
		FImmortalPetDefinition Definition;
		UImmortalPetLibrary::GetPetDefinition(
			Growth.PetId, Definition);
		CombatFeedbackWidget->ShowBossAnnouncement(
			FText::FromString(FString::Printf(
				TEXT("%s提升至 %d 级（+%d 灵兽历练）"),
				*Definition.DisplayName.ToString(),
				Growth.CurrentLevel,
				Growth.ExperienceGranted)),
			Definition.DisplayColor,
			5.0f);
	}
	UE_LOG(LogTemp, Display,
		TEXT("Pet combat growth recorded: pet=%s xp=+%d level=%d exp=%d/%d kills=%lld revision=%d cultivationReward=0"),
		*Growth.PetId.ToString(),
		Growth.ExperienceGranted,
		Growth.CurrentLevel,
		Growth.CurrentExperience,
		Growth.ExperienceToNextLevel,
		PetState.TotalCombatKills,
		PetState.Revision);
}

FImmortalAscensionEligibility
AImmortalPlayerCharacter::EvaluateAscensionEligibility() const
{
	const bool bReachedAscension =
		CultivationComponent
		&& CultivationComponent->HasReachedAscension();
	const FImmortalMapSystemState MapState =
		GetMapSystemState();
	FImmortalMapProgress FinalMapProgress;
	const bool bFinalMapCompleted =
		UImmortalMapLibrary::GetMapProgress(
			MapState,
			UImmortalMapLibrary::GetImmortalPalaceRuinsId(),
			FinalMapProgress)
		&& FinalMapProgress.bCompleted
		&& FinalMapProgress.Stage >= 999;
	const bool bAtQingyun =
		MapState.ActiveMapId
			== UImmortalMapLibrary::GetQingyunMountainId();
	FImmortalAscensionEligibility Result =
		UImmortalAscensionLibrary::EvaluateEligibility(
		AscensionState,
		bReachedAscension,
		bFinalMapCompleted,
		bAtQingyun,
		GetWorldBossRuntimeSnapshot().bActive,
		GetEndlessDungeonRuntimeSnapshot().bActive);
	if (Result.bEligible && bDead)
	{
		Result.bEligible = false;
		Result.Message = FText::FromString(
			TEXT("重伤状态不可飞升，请等待自动复苏"));
	}
	if (Result.bEligible)
	{
		const AImmortalMonsterSpawner* Spawner =
			FindMapSpawner();
		if (!Spawner
			|| !Spawner->CanApplyAscensionCycleReset())
		{
			Result.bEligible = false;
			Result.Message = FText::FromString(
				TEXT("地图或战利品正在结算，请稍后再飞升"));
		}
	}
	return Result;
}

FImmortalAscensionOperationResult
AImmortalPlayerCharacter::PerformAscension()
{
	const FImmortalAscensionEligibility Eligibility =
		EvaluateAscensionEligibility();
	if (!Eligibility.bEligible)
	{
		FImmortalAscensionOperationResult Result;
		Result.AscensionCount =
			AscensionState.AscensionCount;
		Result.ImmortalSeals = AscensionState.ImmortalSeals;
		Result.Message = Eligibility.Message;
		return Result;
	}
	if (!CultivationComponent)
	{
		FImmortalAscensionOperationResult Result;
		Result.Message =
			FText::FromString(TEXT("修炼组件不可用，飞升已取消"));
		return Result;
	}
	AImmortalMonsterSpawner* Spawner =
		FindMapSpawner();
	if (!Spawner
		|| !Spawner->CanApplyAscensionCycleReset())
	{
		FImmortalAscensionOperationResult Result;
		Result.AscensionCount =
			AscensionState.AscensionCount;
		Result.ImmortalSeals = AscensionState.ImmortalSeals;
		Result.Message = FText::FromString(
			TEXT("地图或战利品正在结算，飞升已取消"));
		return Result;
	}

	const FImmortalAscensionState PreviousState =
		AscensionState;
	const FImmortalQuestState PreviousQuestState = QuestState;
	const EImmortalCultivationRealm PreviousRealm =
		CultivationComponent->GetCurrentRealm();
	const int32 PreviousMinorStage =
		CultivationComponent->GetCurrentMinorStage();
	const int32 PreviousCultivation =
		CultivationComponent->GetCurrentCultivation();
	const float PreviousHealth = CurrentHealth;
	const float PreviousMana = CurrentMana;
	FImmortalMapSystemState CompletedCycleMapState =
		Spawner->GetMapSystemState();
	UImmortalMapLibrary::NormalizeState(
		CompletedCycleMapState);
	const FImmortalMapSystemState NewCycleMapState =
		UImmortalAscensionLibrary::CreateNewCycleMapState();
	if (!Spawner->IsCanonicalAscensionCycleState(
		NewCycleMapState))
	{
		FImmortalAscensionOperationResult Result;
		Result.AscensionCount =
			AscensionState.AscensionCount;
		Result.ImmortalSeals = AscensionState.ImmortalSeals;
		Result.Message = FText::FromString(
			TEXT("新轮回地图状态无效，飞升已取消"));
		return Result;
	}

	FImmortalAscensionOperationResult Result =
		UImmortalAscensionLibrary::GrantAscension(
			AscensionState,
			CompletedCycleMapState,
			FDateTime::UtcNow().GetTicks());
	if (!Result.bSucceeded)
	{
		return Result;
	}
	const bool bQuestChanged = RecordQuestProgressWithoutSave(
		EImmortalQuestMetric::Ascensions, 1);

	CultivationComponent->StopCultivating();
	CultivationComponent->InitializeProgress(
		EImmortalCultivationRealm::QiRefining, 1, 0);
	CurrentCultivation = 0;
	RecalculateAscensionBonuses();
	CurrentHealth = FMath::Clamp(
		PreviousHealth, 0.0f, GetMaxHealth());
	CurrentMana = FMath::Clamp(
		PreviousMana, 0.0f, GetMaxMana());

	bool bAscensionSaved = false;
#if !UE_BUILD_SHIPPING
	const bool bInjectAscensionWriteFailure =
		ShouldForceAscensionPersistenceFailure(TEXT("Ascend"));
	UImmortalPathSaveGame::SetDevelopmentWriteFailure(
		bInjectAscensionWriteFailure);
#endif
	bAscensionSaved = SaveProgressWithMapOverride(
		&NewCycleMapState);
#if !UE_BUILD_SHIPPING
	UImmortalPathSaveGame::SetDevelopmentWriteFailure(false);
#endif
	if (!bAscensionSaved)
	{
		AscensionState = PreviousState;
		QuestState = PreviousQuestState;
		CultivationComponent->InitializeProgress(
			PreviousRealm,
			PreviousMinorStage,
			PreviousCultivation);
		CurrentCultivation = PreviousCultivation;
		RecalculateAscensionBonuses();
		CurrentHealth = FMath::Clamp(
			PreviousHealth, 0.0f, GetMaxHealth());
		CurrentMana = FMath::Clamp(
			PreviousMana, 0.0f, GetMaxMana());
		CultivationComponent->StartCultivating();
		Result.bSucceeded = false;
		Result.ImmortalSealsGranted = 0;
		Result.AscensionCount =
			AscensionState.AscensionCount;
		Result.ImmortalSeals = AscensionState.ImmortalSeals;
		Result.Message = FText::FromString(
			TEXT("存档失败，飞升与境界重置均已回滚"));
		UE_LOG(LogTemp, Error,
			TEXT("Ascension transaction rolled back because persistence failed"));
		return Result;
	}

	Spawner->ApplyPersistedAscensionCycleState(
		NewCycleMapState);
	CachedMapSystemState = NewCycleMapState;
	CultivationComponent->StartCultivating();
	BP_OnAscensionStateChanged(AscensionState);
	if (bQuestChanged) BP_OnQuestStateChanged(QuestState);
	if (!bAscensionOpen)
	{
		OpenAscensionInterface();
	}
	if (PlayerAscensionWidget)
	{
		PlayerAscensionWidget->PlayAscensionSequence();
	}
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->ShowBossAnnouncement(
			FText::FromString(FString::Printf(
				TEXT("羽化飞升 · 第 %d 次轮回开启 · 仙印 +%d"),
				Result.AscensionCount,
				Result.ImmortalSealsGranted)),
			FLinearColor(0.84f, 0.58f, 1.0f, 1.0f),
			5.0f);
	}
	UE_LOG(LogTemp, Display,
		TEXT("Ascension committed: count=%d sealsGranted=%d seals=%d realmReset=QiRefining1 cultivation=0 cycleReset=QingyunMountain/1/0 lifetimeMaps=%d preserved=equipment,inventory,stones,mapLegacy,cave,farming,sect,pet,boss,endless revision=%d"),
		AscensionState.AscensionCount,
		Result.ImmortalSealsGranted,
		AscensionState.ImmortalSeals,
		AscensionState.LifetimeMapRecords.Num(),
		AscensionState.Revision);
	return Result;
}

FImmortalAscensionPathResult
AImmortalPlayerCharacter::InvestAscensionPath(
	const EImmortalAscensionPath Path)
{
	const FImmortalAscensionState PreviousState =
		AscensionState;
	FImmortalAscensionPathResult Result =
		UImmortalAscensionLibrary::InvestPath(
			AscensionState, Path);
	if (!Result.bSucceeded)
	{
		return Result;
	}

	RecalculateAscensionBonuses();
	bool bPathSaved = false;
#if !UE_BUILD_SHIPPING
	const bool bInjectPathWriteFailure =
		ShouldForceAscensionPersistenceFailure(TEXT("Path"));
	UImmortalPathSaveGame::SetDevelopmentWriteFailure(
		bInjectPathWriteFailure);
#endif
	bPathSaved = SaveProgress();
#if !UE_BUILD_SHIPPING
	UImmortalPathSaveGame::SetDevelopmentWriteFailure(false);
#endif
	if (!bPathSaved)
	{
		AscensionState = PreviousState;
		RecalculateAscensionBonuses();
		Result.bSucceeded = false;
		Result.CurrentRank = Result.PreviousRank;
		Result.ImmortalSeals = AscensionState.ImmortalSeals;
		Result.ImmortalSealsSpent = 0;
		Result.Message = FText::FromString(
			TEXT("存档失败，本次仙途加点已回滚"));
		UE_LOG(LogTemp, Error,
			TEXT("Ascension path transaction rolled back because persistence failed"));
		return Result;
	}

	BP_OnAscensionStateChanged(AscensionState);
	UE_LOG(LogTemp, Display,
		TEXT("Ascension path invested: path=%d rank=%d cost=%d seals=%d battle=x%.2f cultivation=x%.2f loot=x%.2f revision=%d"),
		static_cast<int32>(Path),
		Result.CurrentRank,
		Result.ImmortalSealsSpent,
		AscensionState.ImmortalSeals,
		GetAscensionBattleMultiplier(),
		GetAscensionCultivationMultiplier(),
		GetAscensionEquipmentDropMultiplier(),
		AscensionState.Revision);
	return Result;
}

float AImmortalPlayerCharacter::GetAscensionBattleMultiplier() const
{
	return UImmortalAscensionLibrary::
		CalculateBattleDamageMultiplier(AscensionState);
}

float AImmortalPlayerCharacter::GetAscensionCultivationMultiplier() const
{
	return UImmortalAscensionLibrary::
		CalculateCultivationRateMultiplier(AscensionState);
}

float AImmortalPlayerCharacter::GetAscensionEquipmentDropMultiplier() const
{
	return UImmortalAscensionLibrary::
		CalculateEquipmentDropMultiplier(AscensionState);
}

bool AImmortalPlayerCharacter::EnsureQuestDailyState(
	const int64 CurrentUtcTicks)
{
	const int64 EffectiveTicks = CurrentUtcTicks > 0
		? CurrentUtcTicks : FDateTime::UtcNow().GetTicks();
	const FImmortalQuestState PreviousState = QuestState;
	const FImmortalQuestDailyRefreshResult Result =
		UImmortalQuestLibrary::EnsureDailyState(
			QuestState, EffectiveTicks, SectUtcOffsetMinutes);
	if (!Result.bStateChanged)
	{
		return Result.bSucceeded && !Result.bClockRollbackDetected;
	}
	if (!SaveProgress())
	{
		QuestState = PreviousState;
		UE_LOG(LogTemp, Error,
			TEXT("Quest daily refresh rolled back because persistence failed"));
		return false;
	}
	BP_OnQuestStateChanged(QuestState);
	return true;
}

bool AImmortalPlayerCharacter::RecordQuestProgressWithoutSave(
	const EImmortalQuestMetric Metric,
	const int64 Amount)
{
	const FImmortalQuestRecordResult Result =
		UImmortalQuestLibrary::RecordProgress(
			QuestState,
			Metric,
			Amount,
			FDateTime::UtcNow().GetTicks(),
			SectUtcOffsetMinutes);
	return Result.bSucceeded && Result.bStateChanged;
}

bool AImmortalPlayerCharacter::RecordQuestProgress(
	const EImmortalQuestMetric Metric,
	const int64 Amount)
{
	if (Amount <= 0)
	{
		return false;
	}
	const FImmortalQuestState PreviousState = QuestState;
	if (!RecordQuestProgressWithoutSave(Metric, Amount))
	{
		return false;
	}
	if (!SaveProgress())
	{
		QuestState = PreviousState;
		UE_LOG(LogTemp, Error,
			TEXT("Quest progress rolled back because persistence failed: metric=%d amount=%lld"),
			static_cast<int32>(Metric), Amount);
		return false;
	}
	BP_OnQuestStateChanged(QuestState);
	return true;
}

FImmortalQuestClaimResult AImmortalPlayerCharacter::EvaluateQuestClaim(
	const FName QuestId) const
{
	return UImmortalQuestLibrary::EvaluateClaim(
		QuestState,
		QuestId,
		FDateTime::UtcNow().GetTicks(),
		SectUtcOffsetMinutes);
}

FImmortalQuestClaimResult AImmortalPlayerCharacter::ClaimQuest(
	const FName QuestId)
{
	FImmortalQuestClaimResult Result = EvaluateQuestClaim(QuestId);
	if (!Result.bCanClaim)
	{
		return Result;
	}
	if (Result.Reward.SpiritStones < 0
		|| CurrentGold > MAX_int32 - Result.Reward.SpiritStones)
	{
		Result.bCanClaim = false;
		Result.Message = FText::FromString(
			TEXT("灵石已达到上限，暂时无法领取任务奖励"));
		return Result;
	}
	if (Result.Reward.TechniqueInsight < 0
		|| TechniqueInsightPoints > 9999 - Result.Reward.TechniqueInsight)
	{
		Result.bCanClaim = false;
		Result.Message = FText::FromString(
			TEXT("悟道点已达到上限，暂时无法领取任务奖励"));
		return Result;
	}
	if (!Result.Reward.MaterialId.IsNone()
		&& Result.Reward.MaterialQuantity > 0)
	{
		TArray<FImmortalMaterialStack> CandidateMaterials = MaterialInventory;
		if (UImmortalMaterialLibrary::AddMaterialStack(
			CandidateMaterials,
			Result.Reward.MaterialId,
			Result.Reward.MaterialQuantity)
			!= Result.Reward.MaterialQuantity)
		{
			Result.bCanClaim = false;
			Result.Message = FText::FromString(
				TEXT("材料堆叠已满，暂时无法领取任务奖励"));
			return Result;
		}
	}

	const FImmortalQuestState PreviousQuestState = QuestState;
	const TArray<FImmortalMaterialStack> PreviousMaterials = MaterialInventory;
	const int32 PreviousGold = CurrentGold;
	const int32 PreviousInsight = TechniqueInsightPoints;
	const int32 PreviousMaterialRevision = MaterialInventoryRevision;
	const int32 PreviousTechniqueRevision = TechniqueRevision;
	Result = UImmortalQuestLibrary::TryClaim(
		QuestState,
		QuestId,
		FDateTime::UtcNow().GetTicks(),
		SectUtcOffsetMinutes);
	if (!Result.bSucceeded)
	{
		QuestState = PreviousQuestState;
		return Result;
	}

	CurrentGold += Result.Reward.SpiritStones;
	if (Result.Reward.TechniqueInsight > 0)
	{
		TechniqueInsightPoints += Result.Reward.TechniqueInsight;
		++TechniqueRevision;
	}
	bool bRewardApplied = true;
	if (!Result.Reward.MaterialId.IsNone()
		&& Result.Reward.MaterialQuantity > 0)
	{
		bRewardApplied = AddMaterialInternal(
			Result.Reward.MaterialId,
			Result.Reward.MaterialQuantity)
			== Result.Reward.MaterialQuantity;
	}
	if (!bRewardApplied || !SaveProgress())
	{
		QuestState = PreviousQuestState;
		MaterialInventory = PreviousMaterials;
		CurrentGold = PreviousGold;
		TechniqueInsightPoints = PreviousInsight;
		MaterialInventoryRevision = PreviousMaterialRevision;
		TechniqueRevision = PreviousTechniqueRevision;
		Result.bSucceeded = false;
		Result.bCanClaim = false;
		Result.bPersistenceFailed = true;
		Result.Message = bRewardApplied
			? FText::FromString(TEXT("领取失败：存档未写入，任务状态与奖励已完整回滚"))
			: FText::FromString(TEXT("领取失败：奖励无法写入，任务状态未改变"));
		return Result;
	}

	if (Result.Reward.SpiritStones > 0)
	{
		BP_OnRewardsChanged(
			CurrentCultivation,
			CurrentGold,
			0,
			CurrentGold - PreviousGold);
	}
	if (!Result.Reward.MaterialId.IsNone()
		&& Result.Reward.MaterialQuantity > 0)
	{
		PublishMaterialInventoryDiff(PreviousMaterials);
	}
	BP_OnQuestStateChanged(QuestState);
	QueueManagementNotification(
		Result.Message,
		FLinearColor(0.58f, 1.0f, 0.66f, 1.0f),
		5.0f);
	UE_LOG(LogTemp, Display,
		TEXT("Quest reward claimed: %s | stones +%d | insight +%d | material %s x%d | totalClaims=%lld"),
		*QuestId.ToString(),
		Result.Reward.SpiritStones,
		Result.Reward.TechniqueInsight,
		*Result.Reward.MaterialId.ToString(),
		Result.Reward.MaterialQuantity,
		QuestState.TotalClaims);
	return Result;
}

bool AImmortalPlayerCharacter::EnsureSectDailyState(const int64 CurrentUtcTicks)
{
	const int64 EffectiveTicks = CurrentUtcTicks > 0 ? CurrentUtcTicks : FDateTime::UtcNow().GetTicks();
	const FImmortalSectState PreviousState = SectState;
	const FImmortalSectDailyRefreshResult Result = UImmortalSectLibrary::EnsureDailyState(
		SectState, EffectiveTicks, SectUtcOffsetMinutes);
	if (!Result.bStateChanged)
	{
		return Result.bSucceeded && !Result.bClockRollbackDetected;
	}
	if (!SaveProgress())
	{
		SectState = PreviousState;
		UE_LOG(LogTemp, Error, TEXT("Sect daily refresh rolled back because persistence failed"));
		return false;
	}
	BP_OnSectStateChanged(SectState);
	return true;
}

FImmortalSectJoinResult AImmortalPlayerCharacter::EvaluateJoinSect(const FName SectId) const
{
	return UImmortalSectLibrary::EvaluateJoin(
		SectState,
		SectId,
		static_cast<int32>(GetCultivationRealm()),
		GetMapSystemState(),
		FDateTime::UtcNow().GetTicks(),
		SectUtcOffsetMinutes);
}

FImmortalSectJoinResult AImmortalPlayerCharacter::JoinSect(const FName SectId)
{
	const FImmortalSectState PreviousState = SectState;
	FImmortalSectJoinResult Result = UImmortalSectLibrary::TryJoin(
		SectState,
		SectId,
		static_cast<int32>(GetCultivationRealm()),
		GetMapSystemState(),
		FDateTime::UtcNow().GetTicks(),
		SectUtcOffsetMinutes);
	if (!Result.bSucceeded)
	{
		return Result;
	}
	if (!SaveProgress())
	{
		SectState = PreviousState;
		Result.bSucceeded = false;
		Result.bCanJoin = false;
		Result.bPersistenceFailed = true;
		Result.Message = FText::FromString(TEXT("加入宗门失败：存档未写入，所有变更已回滚"));
		return Result;
	}
	BP_OnSectStateChanged(SectState);
	UE_LOG(LogTemp, Display, TEXT("Sect joined: %s | day=%d | revision=%d"),
		*SectState.SectId.ToString(), SectState.TaskDayKey, SectState.Revision);
	return Result;
}

FImmortalSectTaskClaimResult AImmortalPlayerCharacter::EvaluateSectTaskClaim(const FName TaskId) const
{
	return UImmortalSectLibrary::EvaluateTaskClaim(
		SectState, TaskId, FDateTime::UtcNow().GetTicks(), SectUtcOffsetMinutes);
}

FImmortalSectTaskClaimResult AImmortalPlayerCharacter::ClaimSectTask(const FName TaskId)
{
	const FImmortalSectState PreviousState = SectState;
	const FImmortalQuestState PreviousQuestState = QuestState;
	FImmortalSectTaskClaimResult Result = UImmortalSectLibrary::TryClaimTask(
		SectState, TaskId, FDateTime::UtcNow().GetTicks(), SectUtcOffsetMinutes);
	if (!Result.bSucceeded)
	{
		return Result;
	}
	const bool bQuestChanged = RecordQuestProgressWithoutSave(
		EImmortalQuestMetric::SectTasksClaimed, 1);
	if (!SaveProgress())
	{
		SectState = PreviousState;
		QuestState = PreviousQuestState;
		Result.bSucceeded = false;
		Result.bCanClaim = false;
		Result.bPersistenceFailed = true;
		Result.ContributionAwarded = 0;
		Result.ContributionAfter = SectState.Contribution;
		Result.Message = FText::FromString(TEXT("领取失败：存档未写入，贡献与任务状态已回滚"));
		return Result;
	}
	BP_OnSectStateChanged(SectState);
	if (bQuestChanged) BP_OnQuestStateChanged(QuestState);
	UE_LOG(LogTemp, Display, TEXT("Sect task claimed: %s | contribution +%d => %d"),
		*TaskId.ToString(), Result.ContributionAwarded, SectState.Contribution);
	return Result;
}

FImmortalSectExchangeResult AImmortalPlayerCharacter::EvaluateSectExchange(const FName OfferId) const
{
	FImmortalSectExchangeResult Result = UImmortalSectLibrary::EvaluateExchange(
		SectState, OfferId, FDateTime::UtcNow().GetTicks(), SectUtcOffsetMinutes);
	if (!Result.bCanExchange)
	{
		return Result;
	}

	switch (Result.RewardType)
	{
	case EImmortalSectRewardType::Material:
		{
			TArray<FImmortalMaterialStack> CandidateInventory = MaterialInventory;
			if (UImmortalMaterialLibrary::AddMaterialStack(
				CandidateInventory, Result.RewardId, Result.RewardQuantity) != Result.RewardQuantity)
			{
				Result.bCanExchange = false;
				Result.Message = FText::FromString(TEXT("材料堆叠已满，无法兑换"));
			}
			break;
		}
	case EImmortalSectRewardType::SpiritStones:
		if (Result.RewardQuantity <= 0 || CurrentGold > MAX_int32 - Result.RewardQuantity)
		{
			Result.bCanExchange = false;
			Result.Message = FText::FromString(TEXT("灵石已达到上限，无法兑换"));
		}
		break;
	case EImmortalSectRewardType::TechniqueInsight:
		if (Result.RewardQuantity <= 0 || TechniqueInsightPoints > 9999 - Result.RewardQuantity)
		{
			Result.bCanExchange = false;
			Result.Message = FText::FromString(TEXT("悟道点已达到上限，无法兑换"));
		}
		break;
	case EImmortalSectRewardType::Technique:
		if (IsTechniqueLearned(Result.RewardId))
		{
			Result.bCanExchange = false;
			Result.bOneTimePurchased = true;
			Result.Message = FText::FromString(TEXT("该宗门功法已经领悟，不会重复扣除贡献"));
		}
		break;
	default:
		Result.bCanExchange = false;
		Result.Message = FText::FromString(TEXT("未知宗门奖励"));
		break;
	}
	return Result;
}

FImmortalSectExchangeResult AImmortalPlayerCharacter::ExchangeSectOffer(const FName OfferId)
{
	FImmortalSectExchangeResult Preflight = EvaluateSectExchange(OfferId);
	if (!Preflight.bCanExchange)
	{
		return Preflight;
	}

	const FImmortalSectState PreviousSectState = SectState;
	const TArray<FImmortalMaterialStack> PreviousMaterials = MaterialInventory;
	const TArray<FImmortalTechniqueProgress> PreviousTechniques = TechniqueLibrary;
	const int32 PreviousGold = CurrentGold;
	const int32 PreviousInsight = TechniqueInsightPoints;
	const int32 PreviousMaterialRevision = MaterialInventoryRevision;
	const int32 PreviousTechniqueRevision = TechniqueRevision;

	FImmortalSectExchangeResult Result = UImmortalSectLibrary::TryExchange(
		SectState, OfferId, FDateTime::UtcNow().GetTicks(), SectUtcOffsetMinutes);
	if (!Result.bSucceeded)
	{
		SectState = PreviousSectState;
		return Result;
	}

	bool bRewardApplied = false;
	FImmortalTechniqueProgress GrantedTechnique;
	switch (Result.RewardType)
	{
	case EImmortalSectRewardType::Material:
		bRewardApplied = AddMaterialInternal(Result.RewardId, Result.RewardQuantity) == Result.RewardQuantity;
		break;
	case EImmortalSectRewardType::SpiritStones:
		CurrentGold += Result.RewardQuantity;
		bRewardApplied = true;
		break;
	case EImmortalSectRewardType::TechniqueInsight:
		TechniqueInsightPoints += Result.RewardQuantity;
		++TechniqueRevision;
		bRewardApplied = true;
		break;
	case EImmortalSectRewardType::Technique:
		GrantedTechnique = UImmortalTechniqueLibrary::CreateTechnique(Result.RewardId);
		if (!GrantedTechnique.TechniqueId.IsNone() && !IsTechniqueLearned(Result.RewardId))
		{
			TechniqueLibrary.Add(GrantedTechnique);
			UImmortalTechniqueLibrary::NormalizeLibrary(
				TechniqueLibrary, EquippedTechniqueIds, TechniqueInsightPoints);
			++TechniqueRevision;
			bRewardApplied = IsTechniqueLearned(Result.RewardId);
		}
		break;
	default:
		break;
	}

	const bool bPersistenceFailed = !bRewardApplied
		|| ShouldForceSectPersistenceFailure(TEXT("Exchange"))
		|| !SaveProgress();
	if (bPersistenceFailed)
	{
		SectState = PreviousSectState;
		MaterialInventory = PreviousMaterials;
		TechniqueLibrary = PreviousTechniques;
		CurrentGold = PreviousGold;
		TechniqueInsightPoints = PreviousInsight;
		MaterialInventoryRevision = PreviousMaterialRevision;
		TechniqueRevision = PreviousTechniqueRevision;
		Result.bSucceeded = false;
		Result.bCanExchange = false;
		Result.bPersistenceFailed = true;
		Result.ContributionSpent = 0;
		Result.ContributionAfter = SectState.Contribution;
		Result.RewardQuantity = 0;
		Result.Message = bRewardApplied
			? FText::FromString(TEXT("兑换失败：存档未写入，贡献与奖励已完整回滚"))
			: FText::FromString(TEXT("兑换失败：奖励无法写入，贡献未扣除"));
		return Result;
	}

	if (Result.RewardType == EImmortalSectRewardType::Material)
	{
		PublishMaterialInventoryDiff(PreviousMaterials);
	}
	else if (Result.RewardType == EImmortalSectRewardType::SpiritStones)
	{
		BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, CurrentGold - PreviousGold);
	}
	else if (Result.RewardType == EImmortalSectRewardType::Technique)
	{
		FImmortalTechniqueProgress PersistedTechnique;
		if (GetTechniqueProgress(Result.RewardId, PersistedTechnique))
		{
			BP_OnTechniqueChanged(PersistedTechnique, false);
		}
	}
	BP_OnSectStateChanged(SectState);
	UE_LOG(LogTemp, Display,
		TEXT("Sect exchange: %s | reward=%d/%s x%d | contribution -%d => %d"),
		*OfferId.ToString(), static_cast<int32>(Result.RewardType), *Result.RewardId.ToString(),
		Result.RewardQuantity, Result.ContributionSpent, SectState.Contribution);
	return Result;
}

void AImmortalPlayerCharacter::NotifySectCombatProgress(
	const int32 MonsterKills,
	const int32 StageClears,
	const int32 BossKills,
	const int32 MapCompletions)
{
	const int32 SafeKills = FMath::Max(MonsterKills, 0);
	const int32 SafeStages = FMath::Max(StageClears, 0);
	const int32 SafeBosses = FMath::Max(BossKills, 0);
	const int32 SafeMaps = FMath::Max(MapCompletions, 0);
	if (SafeKills <= 0 && SafeStages <= 0
		&& SafeBosses <= 0 && SafeMaps <= 0)
	{
		return;
	}

	const FImmortalQuestState PreviousQuestState = QuestState;
	const FImmortalSectState PreviousSectState = SectState;
	bool bQuestChanged = false;
	bQuestChanged |= RecordQuestProgressWithoutSave(
		EImmortalQuestMetric::MonsterKills, SafeKills);
	bQuestChanged |= RecordQuestProgressWithoutSave(
		EImmortalQuestMetric::StageClears, SafeStages);
	bQuestChanged |= RecordQuestProgressWithoutSave(
		EImmortalQuestMetric::BossKills, SafeBosses);
	bQuestChanged |= RecordQuestProgressWithoutSave(
		EImmortalQuestMetric::MapCompletions, SafeMaps);

	bool bSectChanged = false;
	if (SectState.HasJoined())
	{
		const FImmortalSectTaskProgressResult SectResult =
			UImmortalSectLibrary::RecordCombatProgress(
				SectState,
				SafeKills,
				SafeStages,
				SafeBosses,
				FDateTime::UtcNow().GetTicks(),
				SectUtcOffsetMinutes);
		bSectChanged = SectResult.bStateChanged;
	}
	if (!bQuestChanged && !bSectChanged)
	{
		return;
	}
	if (!SaveProgress())
	{
		QuestState = PreviousQuestState;
		SectState = PreviousSectState;
		UE_LOG(LogTemp, Error,
			TEXT("Combat quest/sect progress rolled back because persistence failed"));
		return;
	}
	if (bQuestChanged) BP_OnQuestStateChanged(QuestState);
	if (bSectChanged) BP_OnSectStateChanged(SectState);
}

FImmortalCaveProductionSnapshot AImmortalPlayerCharacter::GetCaveProductionSnapshot() const
{
	return UImmortalCaveLibrary::GetProductionSnapshot(CaveState);
}

float AImmortalPlayerCharacter::GetCaveCultivationMultiplier() const
{
	return GetCaveProductionSnapshot().CultivationRateMultiplier;
}

float AImmortalPlayerCharacter::GetCaveAlchemySuccessBonus() const
{
	return GetCaveProductionSnapshot().AlchemySuccessChanceBonus;
}

float AImmortalPlayerCharacter::GetCaveAlchemyExceptionalBonus() const
{
	return GetCaveProductionSnapshot().AlchemyExceptionalChanceBonus;
}

FImmortalCraftingCost AImmortalPlayerCharacter::ApplyCaveForgeDiscount(const FImmortalCraftingCost& Cost) const
{
	return UImmortalCaveLibrary::ApplyForgeDiscount(Cost, CaveState);
}

bool AImmortalPlayerCharacter::CanUpgradeCaveBuilding(const EImmortalCaveBuildingType BuildingType) const
{
	return UImmortalCaveLibrary::EvaluateUpgrade(CaveState, BuildingType, MaterialInventory, CurrentGold).bCanUpgrade;
}

FImmortalCaveSettlementResult AImmortalPlayerCharacter::SettleCaveProduction(const int64 CurrentUtcTicks)
{
	const int64 SettlementTicks = CurrentUtcTicks > 0 ? CurrentUtcTicks : FDateTime::UtcNow().GetTicks();
	const FImmortalCaveSettlementResult Result = UImmortalCaveLibrary::SettleProduction(CaveState, SettlementTicks);
	if (Result.bClockRollbackDetected)
	{
		UE_LOG(LogTemp, VeryVerbose,
			TEXT("Cave production paused because UTC moved backwards: saved=%lld current=%lld"),
			CaveState.LastSettlementUtcTicks, SettlementTicks);
	}
	return Result;
}

int32 AImmortalPlayerCharacter::GetSpiritFieldLevel() const
{
	return UImmortalCaveLibrary::GetBuildingLevel(CaveState, EImmortalCaveBuildingType::SpiritField);
}

FImmortalFarmingSettlementResult AImmortalPlayerCharacter::SettleFarmingGrowth(const int64 CurrentUtcTicks)
{
	const int64 SettlementTicks = CurrentUtcTicks > 0 ? CurrentUtcTicks : FDateTime::UtcNow().GetTicks();
	const FImmortalFarmingSettlementResult Result = UImmortalFarmingLibrary::SettleGrowth(
		FarmingState, GetSpiritFieldLevel(), SettlementTicks);
	if (Result.bClockRollbackDetected)
	{
		UE_LOG(LogTemp, VeryVerbose,
			TEXT("Farming growth paused because UTC moved backwards: saved=%lld current=%lld"),
			FarmingState.LastSettlementUtcTicks, SettlementTicks);
	}
	return Result;
}

void AImmortalPlayerCharacter::HandleCaveProductionTick()
{
	SettleCaveProduction();
}

void AImmortalPlayerCharacter::RecalculateCaveBonuses()
{
	if (CultivationComponent)
	{
		CultivationComponent->SetCaveRateMultiplier(GetCaveCultivationMultiplier());
	}
}

FImmortalCaveUpgradeResult AImmortalPlayerCharacter::UpgradeCaveBuilding(
	const EImmortalCaveBuildingType BuildingType)
{
	const int64 OperationUtcTicks = FDateTime::UtcNow().GetTicks();
	SettleCaveProduction(OperationUtcTicks);
	// Settle existing crops with the old spirit-field level so an upgrade only
	// accelerates growth after the moment it succeeds.
	SettleFarmingGrowth(OperationUtcTicks);
	const FImmortalCaveState PreviousCaveState = CaveState;
	const TArray<FImmortalMaterialStack> PreviousMaterials = MaterialInventory;
	const int32 PreviousGold = CurrentGold;
	const int32 PreviousMaterialRevision = MaterialInventoryRevision;

	FImmortalCaveUpgradeResult Result = UImmortalCaveLibrary::TryUpgradeBuilding(
		CaveState, BuildingType, MaterialInventory, CurrentGold);
	if (!Result.bSucceeded)
	{
		return Result;
	}

	++MaterialInventoryRevision;
	RecalculateCaveBonuses();
	if (!SaveProgress())
	{
		CaveState = PreviousCaveState;
		MaterialInventory = PreviousMaterials;
		CurrentGold = PreviousGold;
		MaterialInventoryRevision = PreviousMaterialRevision;
		RecalculateCaveBonuses();
		Result.bSucceeded = false;
		Result.bCanUpgrade = false;
		Result.Message = FText::FromString(TEXT("洞府升级存档失败，资源与建筑等级已完整回滚"));
		return Result;
	}

	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, CurrentGold - PreviousGold);
	for (const FImmortalCraftingMaterialCost& MaterialCost : Result.Cost.Materials)
	{
		BP_OnMaterialInventoryChanged(
			MaterialCost.MaterialId,
			GetMaterialQuantity(MaterialCost.MaterialId),
			-MaterialCost.Quantity);
	}
	UE_LOG(LogTemp, Display,
		TEXT("Cave building upgraded: type=%d level=%d->%d | stones=%d | revision=%d"),
		static_cast<int32>(BuildingType), Result.CurrentLevel, Result.TargetLevel, CurrentGold, CaveState.Revision);
	return Result;
}

FImmortalCaveCollectionResult AImmortalPlayerCharacter::CollectCaveResources()
{
	SettleCaveProduction();
	const FImmortalCaveState PreviousCaveState = CaveState;
	const TArray<FImmortalMaterialStack> PreviousMaterials = MaterialInventory;
	const int32 PreviousGold = CurrentGold;
	const int32 PreviousMaterialRevision = MaterialInventoryRevision;

	FImmortalCaveCollectionResult Result = UImmortalCaveLibrary::CollectStoredResources(
		CaveState, MaterialInventory, CurrentGold);
	if (!Result.bCollectedAnything)
	{
		return Result;
	}

	if (Result.SpiritGrassCollected > 0 || Result.OreCollected > 0)
	{
		++MaterialInventoryRevision;
	}
	if (!SaveProgress())
	{
		CaveState = PreviousCaveState;
		MaterialInventory = PreviousMaterials;
		CurrentGold = PreviousGold;
		MaterialInventoryRevision = PreviousMaterialRevision;
		FImmortalCaveCollectionResult Failure;
		Failure.bPersistenceFailed = true;
		Failure.Message = FText::FromString(TEXT("洞府资源收取存档失败，所有资源已安全回滚"));
		return Failure;
	}

	if (Result.SpiritGrassCollected > 0)
	{
		BP_OnMaterialInventoryChanged(TEXT("SpiritGrass"), GetMaterialQuantity(TEXT("SpiritGrass")), Result.SpiritGrassCollected);
	}
	if (Result.OreCollected > 0)
	{
		BP_OnMaterialInventoryChanged(TEXT("Ore"), GetMaterialQuantity(TEXT("Ore")), Result.OreCollected);
	}
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, Result.SpiritStonesCollected);
	UE_LOG(LogTemp, Display,
		TEXT("Cave resources collected: stones=%d grass=%d ore=%d | remaining=%d/%d/%d | revision=%d"),
		Result.SpiritStonesCollected, Result.SpiritGrassCollected, Result.OreCollected,
		CaveState.StoredSpiritStones, CaveState.StoredSpiritGrass, CaveState.StoredOre, CaveState.Revision);
	return Result;
}

FImmortalFarmingPlantResult AImmortalPlayerCharacter::EvaluatePlantCrop(
	const int32 PlotIndex,
	const FName CropId) const
{
	return UImmortalFarmingLibrary::EvaluatePlant(
		FarmingState,
		PlotIndex,
		CropId,
		GetSpiritFieldLevel(),
		MaterialInventory,
		CurrentGold,
		FDateTime::UtcNow().GetTicks());
}

FImmortalFarmingPlantResult AImmortalPlayerCharacter::PlantCrop(
	const int32 PlotIndex,
	const FName CropId)
{
	const int64 OperationUtcTicks = FDateTime::UtcNow().GetTicks();
	SettleCaveProduction(OperationUtcTicks);
	SettleFarmingGrowth(OperationUtcTicks);
	const FImmortalCaveState PreviousCaveState = CaveState;
	const FImmortalFarmingState PreviousFarmingState = FarmingState;
	const TArray<FImmortalMaterialStack> PreviousMaterials = MaterialInventory;
	const int32 PreviousGold = CurrentGold;
	const int32 PreviousMaterialRevision = MaterialInventoryRevision;

	FImmortalFarmingPlantResult Result = UImmortalFarmingLibrary::TryPlantCrop(
		FarmingState,
		PlotIndex,
		CropId,
		GetSpiritFieldLevel(),
		MaterialInventory,
		CurrentGold,
		OperationUtcTicks);
	if (!Result.bSucceeded) return Result;

	const bool bMaterialsChanged = !HaveSameMaterialQuantities(PreviousMaterials, MaterialInventory);
	if (bMaterialsChanged) ++MaterialInventoryRevision;
	if (ShouldForceFarmingPersistenceFailure(TEXT("Plant")) || !SaveProgress())
	{
		CaveState = PreviousCaveState;
		FarmingState = PreviousFarmingState;
		MaterialInventory = PreviousMaterials;
		CurrentGold = PreviousGold;
		MaterialInventoryRevision = PreviousMaterialRevision;
		Result.bSucceeded = false;
		Result.bPersistenceFailed = true;
		Result.Message = FText::FromString(TEXT("播种存档失败，灵石、材料和田块均已安全回滚"));
		return Result;
	}

	if (bMaterialsChanged) PublishMaterialInventoryDiff(PreviousMaterials);
	if (CurrentGold != PreviousGold)
	{
		BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, CurrentGold - PreviousGold);
	}
	UE_LOG(LogTemp, Display,
		TEXT("Farming crop planted: plot=%d crop=%s yield=%d stones=%d revision=%d"),
		PlotIndex, *CropId.ToString(), Result.FrozenYield, CurrentGold, FarmingState.Revision);
	return Result;
}

FImmortalFarmingBatchPlantResult AImmortalPlayerCharacter::PlantCropInAllEmptyPlots(const FName CropId)
{
	const int64 OperationUtcTicks = FDateTime::UtcNow().GetTicks();
	SettleCaveProduction(OperationUtcTicks);
	SettleFarmingGrowth(OperationUtcTicks);
	const FImmortalCaveState PreviousCaveState = CaveState;
	const FImmortalFarmingState PreviousFarmingState = FarmingState;
	const TArray<FImmortalMaterialStack> PreviousMaterials = MaterialInventory;
	const int32 PreviousGold = CurrentGold;
	const int32 PreviousMaterialRevision = MaterialInventoryRevision;

	FImmortalFarmingBatchPlantResult Result = UImmortalFarmingLibrary::TryPlantAllEmpty(
		FarmingState,
		CropId,
		GetSpiritFieldLevel(),
		MaterialInventory,
		CurrentGold,
		OperationUtcTicks);
	if (!Result.bSucceeded) return Result;

	const bool bMaterialsChanged = !HaveSameMaterialQuantities(PreviousMaterials, MaterialInventory);
	if (bMaterialsChanged) ++MaterialInventoryRevision;
	if (ShouldForceFarmingPersistenceFailure(TEXT("Plant")) || !SaveProgress())
	{
		CaveState = PreviousCaveState;
		FarmingState = PreviousFarmingState;
		MaterialInventory = PreviousMaterials;
		CurrentGold = PreviousGold;
		MaterialInventoryRevision = PreviousMaterialRevision;
		Result.bSucceeded = false;
		Result.bPersistenceFailed = true;
		Result.bAllEligiblePlotsPlanted = false;
		Result.PlantedPlotCount = 0;
		for (FImmortalFarmingPlantResult& PlantResult : Result.PlantResults)
		{
			if (PlantResult.bSucceeded)
			{
				PlantResult.bSucceeded = false;
				PlantResult.bPersistenceFailed = true;
				PlantResult.Message = FText::FromString(TEXT("存档失败，本次播种已回滚"));
			}
		}
		Result.Message = FText::FromString(TEXT("批量播种存档失败，全部操作已安全回滚"));
		return Result;
	}

	if (bMaterialsChanged) PublishMaterialInventoryDiff(PreviousMaterials);
	if (CurrentGold != PreviousGold)
	{
		BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, CurrentGold - PreviousGold);
	}
	UE_LOG(LogTemp, Display,
		TEXT("Farming batch planted: crop=%s plots=%d/%d stones=%d revision=%d"),
		*CropId.ToString(), Result.PlantedPlotCount, Result.EligiblePlotCount, CurrentGold, FarmingState.Revision);
	return Result;
}

FImmortalFarmingHarvestResult AImmortalPlayerCharacter::HarvestCrop(const int32 PlotIndex)
{
	const int64 OperationUtcTicks = FDateTime::UtcNow().GetTicks();
	SettleCaveProduction(OperationUtcTicks);
	SettleFarmingGrowth(OperationUtcTicks);
	const FImmortalCaveState PreviousCaveState = CaveState;
	const FImmortalFarmingState PreviousFarmingState = FarmingState;
	const TArray<FImmortalMaterialStack> PreviousMaterials = MaterialInventory;
	const int32 PreviousMaterialRevision = MaterialInventoryRevision;

	FImmortalFarmingHarvestResult Result = UImmortalFarmingLibrary::TryHarvestPlot(
		FarmingState,
		PlotIndex,
		GetSpiritFieldLevel(),
		MaterialInventory,
		OperationUtcTicks);
	if (!Result.bSucceeded) return Result;

	const bool bMaterialsChanged = !HaveSameMaterialQuantities(PreviousMaterials, MaterialInventory);
	if (bMaterialsChanged) ++MaterialInventoryRevision;
	if (ShouldForceFarmingPersistenceFailure(TEXT("Harvest")) || !SaveProgress())
	{
		CaveState = PreviousCaveState;
		FarmingState = PreviousFarmingState;
		MaterialInventory = PreviousMaterials;
		MaterialInventoryRevision = PreviousMaterialRevision;
		Result.bSucceeded = false;
		Result.bPersistenceFailed = true;
		Result.bFullyHarvested = false;
		Result.bPartiallyHarvested = false;
		Result.HarvestedQuantity = 0;
		Result.RemainingQuantity = Result.RequestedQuantity;
		Result.Message = FText::FromString(TEXT("收获存档失败，作物和材料均已安全回滚"));
		return Result;
	}

	if (bMaterialsChanged) PublishMaterialInventoryDiff(PreviousMaterials);
	UE_LOG(LogTemp, Display,
		TEXT("Farming crop harvested: plot=%d crop=%s material=%s amount=%d remaining=%d revision=%d"),
		PlotIndex, *Result.CropId.ToString(), *Result.OutputMaterialId.ToString(),
		Result.HarvestedQuantity, Result.RemainingQuantity, FarmingState.Revision);
	return Result;
}

FImmortalFarmingBatchHarvestResult AImmortalPlayerCharacter::HarvestAllReadyCrops()
{
	const int64 OperationUtcTicks = FDateTime::UtcNow().GetTicks();
	SettleCaveProduction(OperationUtcTicks);
	SettleFarmingGrowth(OperationUtcTicks);
	const FImmortalCaveState PreviousCaveState = CaveState;
	const FImmortalFarmingState PreviousFarmingState = FarmingState;
	const TArray<FImmortalMaterialStack> PreviousMaterials = MaterialInventory;
	const int32 PreviousMaterialRevision = MaterialInventoryRevision;

	FImmortalFarmingBatchHarvestResult Result = UImmortalFarmingLibrary::TryHarvestAllReady(
		FarmingState,
		GetSpiritFieldLevel(),
		MaterialInventory,
		OperationUtcTicks);
	if (!Result.bSucceeded) return Result;

	const bool bMaterialsChanged = !HaveSameMaterialQuantities(PreviousMaterials, MaterialInventory);
	if (bMaterialsChanged) ++MaterialInventoryRevision;
	if (ShouldForceFarmingPersistenceFailure(TEXT("Harvest")) || !SaveProgress())
	{
		CaveState = PreviousCaveState;
		FarmingState = PreviousFarmingState;
		MaterialInventory = PreviousMaterials;
		MaterialInventoryRevision = PreviousMaterialRevision;
		Result.bSucceeded = false;
		Result.bPersistenceFailed = true;
		Result.FullyHarvestedPlotCount = 0;
		Result.PartiallyHarvestedPlotCount = 0;
		Result.HarvestedItemCount = 0;
		for (FImmortalFarmingHarvestResult& HarvestResult : Result.HarvestResults)
		{
			if (HarvestResult.bSucceeded)
			{
				HarvestResult.bSucceeded = false;
				HarvestResult.bPersistenceFailed = true;
				HarvestResult.bFullyHarvested = false;
				HarvestResult.bPartiallyHarvested = false;
				HarvestResult.HarvestedQuantity = 0;
				HarvestResult.RemainingQuantity = HarvestResult.RequestedQuantity;
				HarvestResult.Message = FText::FromString(TEXT("存档失败，本次收获已回滚"));
			}
		}
		Result.Message = FText::FromString(TEXT("批量收获存档失败，全部作物和材料均已安全回滚"));
		return Result;
	}

	if (bMaterialsChanged) PublishMaterialInventoryDiff(PreviousMaterials);
	UE_LOG(LogTemp, Display,
		TEXT("Farming batch harvested: ready=%d full=%d partial=%d items=%d revision=%d"),
		Result.ReadyPlotCount, Result.FullyHarvestedPlotCount,
		Result.PartiallyHarvestedPlotCount, Result.HarvestedItemCount, FarmingState.Revision);
	return Result;
}

AImmortalMonsterSpawner* AImmortalPlayerCharacter::FindMapSpawner() const
{
	if (CachedMapSpawner.IsValid())
	{
		return CachedMapSpawner.Get();
	}
	AImmortalMonsterSpawner* Found = GetWorld()
		? Cast<AImmortalMonsterSpawner>(UGameplayStatics::GetActorOfClass(
			GetWorld(), AImmortalMonsterSpawner::StaticClass()))
		: nullptr;
	CachedMapSpawner = Found;
	return Found;
}

FImmortalMapSystemState AImmortalPlayerCharacter::GetMapSystemState() const
{
	if (const AImmortalMonsterSpawner* Spawner = FindMapSpawner())
	{
		const FImmortalMapSystemState SpawnerState = Spawner->GetMapSystemState();
		if (SpawnerState.bInitialized)
		{
			return SpawnerState;
		}
	}
	FImmortalMapSystemState Fallback = CachedMapSystemState;
	if (!Fallback.bInitialized)
	{
		Fallback = UImmortalMapLibrary::CreateMigratedState(1, 0, false);
	}
	UImmortalMapLibrary::NormalizeState(Fallback);
	return Fallback;
}

int32 AImmortalPlayerCharacter::GetMapRevision() const
{
	if (const AImmortalMonsterSpawner* Spawner = FindMapSpawner())
	{
		return Spawner->GetMapRevision();
	}
	return 0;
}

FName AImmortalPlayerCharacter::GetActiveMapId() const
{
	if (const AImmortalMonsterSpawner* Spawner = FindMapSpawner())
	{
		if (Spawner->GetMapSystemState().bInitialized)
		{
			return Spawner->GetActiveMapId();
		}
	}
	return CachedMapSystemState.bInitialized && !CachedMapSystemState.ActiveMapId.IsNone()
		? CachedMapSystemState.ActiveMapId
		: (DisplayedMapId.IsNone() ? UImmortalMapLibrary::GetQingyunMountainId() : DisplayedMapId);
}

int32 AImmortalPlayerCharacter::GetActiveMapStage() const
{
	FImmortalMapProgress Progress;
	return UImmortalMapLibrary::GetMapProgress(GetMapSystemState(), GetActiveMapId(), Progress)
		? Progress.Stage
		: FMath::Max(DisplayedStage, 1);
}

int32 AImmortalPlayerCharacter::GetQingyunStage() const
{
	FImmortalMapProgress Progress;
	return UImmortalMapLibrary::GetMapProgress(
		GetMapSystemState(), UImmortalMapLibrary::GetQingyunMountainId(), Progress)
		? Progress.Stage
		: (DisplayedMapId == UImmortalMapLibrary::GetQingyunMountainId() ? FMath::Max(DisplayedStage, 1) : 1);
}

bool AImmortalPlayerCharacter::IsMapUnlocked(const FName MapId) const
{
	return UImmortalMapLibrary::IsMapUnlocked(MapId, static_cast<int32>(GetCultivationRealm()));
}

FImmortalMapTravelResult AImmortalPlayerCharacter::TravelToMap(const FName MapId)
{
	if (AImmortalMonsterSpawner* Spawner = FindMapSpawner())
	{
		FImmortalMapTravelResult Result = Spawner->TravelToMap(MapId);
		if (Result.bSucceeded)
		{
			CachedMapSystemState = Spawner->GetMapSystemState();
			if (PlayerMapWidget) PlayerMapWidget->RefreshFromPlayer();
		}
		return Result;
	}
	FImmortalMapTravelResult Result;
	Result.DestinationMapId = MapId;
	Result.Message = FText::FromString(TEXT("地图控制器尚未就绪，请稍后再试"));
	return Result;
}

bool AImmortalPlayerCharacter::GetWorldBossProgress(
	const FName BossId,
	FImmortalWorldBossProgress& OutProgress) const
{
	return UImmortalWorldBossLibrary::GetProgress(
		WorldBossState, BossId, OutProgress);
}

bool AImmortalPlayerCharacter::IsWorldBossUnlocked(const FName BossId) const
{
	FImmortalWorldBossDefinition Definition;
	return UImmortalWorldBossLibrary::GetWorldBossDefinition(BossId, Definition)
		&& UImmortalWorldBossLibrary::IsUnlocked(
			Definition, static_cast<int32>(GetCultivationRealm()));
}

FImmortalWorldBossChallengeResult AImmortalPlayerCharacter::StartWorldBossChallenge(
	const FName BossId)
{
	if (AImmortalMonsterSpawner* Spawner = FindMapSpawner())
	{
		return Spawner->StartWorldBossChallenge(BossId);
	}
	FImmortalWorldBossChallengeResult Result;
	Result.BossId = BossId;
	Result.Message = FText::FromString(TEXT("世界妖王控制器尚未就绪，请稍后再试"));
	return Result;
}

FImmortalWorldBossChallengeResult AImmortalPlayerCharacter::CancelWorldBossChallenge()
{
	if (AImmortalMonsterSpawner* Spawner = FindMapSpawner())
	{
		return Spawner->CancelWorldBossChallenge();
	}
	FImmortalWorldBossChallengeResult Result;
	Result.Message = FText::FromString(TEXT("世界妖王控制器尚未就绪"));
	return Result;
}

FImmortalWorldBossRuntimeSnapshot AImmortalPlayerCharacter::GetWorldBossRuntimeSnapshot() const
{
	if (const AImmortalMonsterSpawner* Spawner = FindMapSpawner())
	{
		return Spawner->GetWorldBossRuntimeSnapshot();
	}
	return FImmortalWorldBossRuntimeSnapshot();
}

FImmortalWorldBossVictoryResult AImmortalPlayerCharacter::CommitWorldBossVictory(
	const FName BossId,
	const float ClearSeconds)
{
	FImmortalWorldBossVictoryResult Result;
	FImmortalWorldBossDefinition Definition;
	if (!UImmortalWorldBossLibrary::GetWorldBossDefinition(BossId, Definition)
		|| ClearSeconds <= 0.0f)
	{
		Result.Message = FText::FromString(TEXT("世界妖王胜利数据无效，未结算奖励"));
		return Result;
	}

	const FImmortalWorldBossState PreviousState = WorldBossState;
	const int64 CurrentUtcTicks = FDateTime::UtcNow().GetTicks();
	const FImmortalWorldBossRecordResult Record =
		UImmortalWorldBossLibrary::RecordDefeat(
			WorldBossState, BossId, ClearSeconds, CurrentUtcTicks);
	if (!Record.bSucceeded)
	{
		Result.Message = FText::FromString(TEXT("世界妖王胜利记录生成失败"));
		return Result;
	}
	const int32 EquipmentItemLevel = FMath::Max(
		1 + (Definition.RecommendedStage - 1) / 5 + Definition.EquipmentLevelBonus,
		1);
	const FImmortalWorldBossRewardBundle Reward =
		UImmortalWorldBossLibrary::CreateRewardBundle(
			Definition,
			EquipmentItemLevel,
			Record.bFirstClear,
			CurrentUtcTicks);
	if (!Reward.IsValid())
	{
		WorldBossState = PreviousState;
		Result.Message = FText::FromString(TEXT("世界妖王独立掉落池生成失败"));
		return Result;
	}
	WorldBossState.PendingRewards.Add(Reward);
	++WorldBossState.Revision;
	if (ShouldForceWorldBossPersistenceFailure(TEXT("Commit")) || !SaveProgress())
	{
		WorldBossState = PreviousState;
		Result.Message = FText::FromString(TEXT("世界妖王胜利首次写盘失败，记录与奖励均已回滚"));
		UE_LOG(LogTemp, Error,
			TEXT("World Boss victory rolled back before delivery: %s"),
			*BossId.ToString());
		return Result;
	}

	Result.bSucceeded = true;
	Result.bFirstClear = Record.bFirstClear;
	Result.bNewBestTime = Record.bNewBestTime;
	Result.RewardId = Reward.RewardId;
	Result.bRewardDelivered = TryDeliverPendingWorldBossReward(Reward.RewardId);
	Result.bRewardPending = WorldBossState.PendingRewards.ContainsByPredicate(
		[&Reward](const FImmortalWorldBossRewardBundle& Pending)
		{
			return Pending.RewardId == Reward.RewardId;
		});
	Result.Message = Result.bRewardDelivered
		? FText::FromString(TEXT("世界妖王奖励已自动发放并保存"))
		: FText::FromString(TEXT("世界妖王胜利已保存；奖励等待背包空间或下次写盘"));
	UE_LOG(LogTemp, Display,
		TEXT("World Boss victory committed: %s | clear %.2fs | first=%s | best=%s | delivered=%s | pending=%s | revision=%d"),
		*BossId.ToString(), ClearSeconds,
		Result.bFirstClear ? TEXT("true") : TEXT("false"),
		Result.bNewBestTime ? TEXT("true") : TEXT("false"),
		Result.bRewardDelivered ? TEXT("true") : TEXT("false"),
		Result.bRewardPending ? TEXT("true") : TEXT("false"),
		WorldBossState.Revision);
	return Result;
}

bool AImmortalPlayerCharacter::TryDeliverPendingWorldBossReward(
	const FGuid RewardId)
{
	const FImmortalWorldBossRewardBundle* Pending =
		WorldBossState.PendingRewards.FindByPredicate(
			[RewardId](const FImmortalWorldBossRewardBundle& Reward)
			{
				return Reward.RewardId == RewardId;
			});
	if (!Pending || !Pending->IsValid())
	{
		return false;
	}
	const FImmortalWorldBossRewardBundle Reward = *Pending;

	const TArray<FImmortalEquipmentItem> PreviousInventory = InventoryItems;
	const TArray<FImmortalEquipmentItem> PreviousEquipped = EquippedItems;
	const TArray<FImmortalMaterialStack> PreviousMaterials = MaterialInventory;
	const TArray<FImmortalArtifactItem> PreviousArtifacts = ArtifactInventory;
	const FGuid PreviousEquippedArtifact = EquippedArtifactInstanceId;
	const FImmortalWorldBossState PreviousWorldBossState = WorldBossState;
	const int32 PreviousGold = CurrentGold;
	const int32 PreviousDropCount = EquipmentDropCount;
	const int32 PreviousEquipmentRevision = EquipmentInventoryRevision;
	const int32 PreviousMaterialRevision = MaterialInventoryRevision;
	const int32 PreviousArtifactRevision = ArtifactInventoryRevision;
	const float PreviousHealth = CurrentHealth;
	const float PreviousMana = CurrentMana;

	auto Rollback = [this,
		&PreviousInventory,
		&PreviousEquipped,
		&PreviousMaterials,
		&PreviousArtifacts,
		PreviousEquippedArtifact,
		&PreviousWorldBossState,
		PreviousGold,
		PreviousDropCount,
		PreviousEquipmentRevision,
		PreviousMaterialRevision,
		PreviousArtifactRevision,
		PreviousHealth,
		PreviousMana]
	{
		InventoryItems = PreviousInventory;
		EquippedItems = PreviousEquipped;
		MaterialInventory = PreviousMaterials;
		ArtifactInventory = PreviousArtifacts;
		EquippedArtifactInstanceId = PreviousEquippedArtifact;
		WorldBossState = PreviousWorldBossState;
		CurrentGold = PreviousGold;
		EquipmentDropCount = PreviousDropCount;
		EquipmentInventoryRevision = PreviousEquipmentRevision;
		MaterialInventoryRevision = PreviousMaterialRevision;
		ArtifactInventoryRevision = PreviousArtifactRevision;
		RecalculateEquipmentBonuses();
		CurrentHealth = FMath::Clamp(PreviousHealth, 0.0f, GetMaxHealth());
		CurrentMana = FMath::Clamp(PreviousMana, 0.0f, GetMaxMana());
	};

	for (const FImmortalEquipmentItem& Item : Reward.EquipmentItems)
	{
		if (!ProcessEquipmentItem(Item, false, false, false))
		{
			Rollback();
			UE_LOG(LogTemp, Warning,
				TEXT("World Boss reward remains pending: equipment backpack cannot accept all %d items"),
				Reward.EquipmentItems.Num());
			return false;
		}
	}
	for (const FImmortalMaterialStack& Material : Reward.Materials)
	{
		if (AddMaterialInternal(Material.MaterialId, Material.Quantity)
			!= Material.Quantity)
		{
			Rollback();
			UE_LOG(LogTemp, Warning,
				TEXT("World Boss reward remains pending: material %s could not be added in full"),
				*Material.MaterialId.ToString());
			return false;
		}
	}
	if (Reward.SpiritStones > 0)
	{
		const int64 NewTotal = static_cast<int64>(CurrentGold) + Reward.SpiritStones;
		if (NewTotal > MAX_int32)
		{
			Rollback();
			UE_LOG(LogTemp, Warning,
				TEXT("World Boss reward remains pending: spirit-stone total would overflow"));
			return false;
		}
		CurrentGold = static_cast<int32>(NewTotal);
	}

	FImmortalArtifactItem GrantedArtifact;
	if (!Reward.ArtifactId.IsNone())
	{
		GrantedArtifact = UImmortalArtifactLibrary::CreateArtifact(Reward.ArtifactId);
		if (!GrantedArtifact.IsValid())
		{
			Rollback();
			return false;
		}
		ArtifactInventory.Add(GrantedArtifact);
		++ArtifactInventoryRevision;
	}

	WorldBossState.PendingRewards.RemoveAll(
		[RewardId](const FImmortalWorldBossRewardBundle& Entry)
		{
			return Entry.RewardId == RewardId;
		});
	++WorldBossState.Revision;
	if (ShouldForceWorldBossPersistenceFailure(TEXT("Delivery")) || !SaveProgress())
	{
		Rollback();
		UE_LOG(LogTemp, Error,
			TEXT("World Boss delivery second write failed; the durable pending bundle was restored"));
		return false;
	}

	BP_OnEquipmentPickedUp(EquipmentDropCount, Reward.EquipmentItems.Num());
	BP_OnInventoryChanged(InventoryItems.Num(), GetInventoryCapacity());
	PublishMaterialInventoryDiff(PreviousMaterials);
	if (Reward.SpiritStones > 0)
	{
		BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, Reward.SpiritStones);
	}
	if (GrantedArtifact.IsValid())
	{
		BP_OnArtifactChanged(GrantedArtifact, false);
	}
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->ShowWorldBossRewardSummary(
			Reward.EquipmentItems.Num(),
			Reward.SpiritStones,
			Reward.Materials,
			GrantedArtifact.IsValid() ? GrantedArtifact.ArtifactId : NAME_None);
	}
	UE_LOG(LogTemp, Display,
		TEXT("World Boss reward delivered atomically: %s | equipment=%d stones=%d materials=%d artifact=%s cultivationReward=0"),
		*Reward.BossId.ToString(), Reward.EquipmentItems.Num(), Reward.SpiritStones,
		Reward.Materials.Num(),
		Reward.ArtifactId.IsNone() ? TEXT("none") : *Reward.ArtifactId.ToString());
	return true;
}

bool AImmortalPlayerCharacter::RetryPendingWorldBossRewards()
{
	TArray<FGuid> RewardIds;
	for (const FImmortalWorldBossRewardBundle& Reward : WorldBossState.PendingRewards)
	{
		RewardIds.Add(Reward.RewardId);
	}
	bool bAllDelivered = true;
	for (const FGuid RewardId : RewardIds)
	{
		bAllDelivered = TryDeliverPendingWorldBossReward(RewardId) && bAllDelivered;
	}
	return bAllDelivered && WorldBossState.PendingRewards.IsEmpty();
}

FImmortalEndlessDungeonStartResult
AImmortalPlayerCharacter::StartEndlessDungeon(const int32 RequestedFloor)
{
	FImmortalEndlessDungeonStartResult Result;
	UImmortalEndlessDungeonLibrary::NormalizeState(EndlessDungeonState);
	const FImmortalEndlessDungeonRules Rules =
		UImmortalEndlessDungeonLibrary::GetRules();
	const int32 CheckpointFloor =
		UImmortalEndlessDungeonLibrary::GetCheckpointStartFloor(
			EndlessDungeonState);
	const int32 MaximumUnlockedFloor = FMath::Clamp(
		EndlessDungeonState.HighestClearedFloor + 1,
		1,
		FMath::Max(Rules.MaximumFloor, 1));
	const int32 StartFloor = RequestedFloor <= 0
		? CheckpointFloor
		: RequestedFloor;
	Result.StartFloor = StartFloor;
	if (StartFloor < CheckpointFloor
		|| StartFloor > MaximumUnlockedFloor)
	{
		Result.Message = FText::FromString(FString::Printf(
			TEXT("Endless Dungeon start floor must be between %d and %d"),
			CheckpointFloor,
			MaximumUnlockedFloor));
		return Result;
	}

	AImmortalMonsterSpawner* Spawner = FindMapSpawner();
	if (!Spawner)
	{
		Result.Message = FText::FromString(
			TEXT("Endless Dungeon controller is not ready"));
		return Result;
	}
	const FImmortalEndlessDungeonState PreviousState = EndlessDungeonState;
	Result = Spawner->StartEndlessDungeon(StartFloor);
	if (!Result.bSucceeded)
	{
		return Result;
	}

	EndlessDungeonState.TotalRuns =
		EndlessDungeonState.TotalRuns < MAX_int32
			? EndlessDungeonState.TotalRuns + 1
			: MAX_int32;
	if (EndlessDungeonState.Revision < MAX_int32)
	{
		++EndlessDungeonState.Revision;
	}
	if (ShouldForceEndlessDungeonPersistenceFailure(TEXT("Start"))
		|| !SaveProgress())
	{
		Spawner->CancelEndlessDungeon();
		EndlessDungeonState = PreviousState;
		Result.bSucceeded = false;
		Result.Message = FText::FromString(
			TEXT("Endless Dungeon run save failed; entry was rolled back"));
		UE_LOG(LogTemp, Error,
			TEXT("Endless Dungeon start rolled back before combat: floor=%d"),
			StartFloor);
		return Result;
	}
	UE_LOG(LogTemp, Display,
		TEXT("Endless Dungeon run persisted: start=%d checkpoint=%d highest=%d runs=%d revision=%d"),
		StartFloor,
		CheckpointFloor,
		EndlessDungeonState.HighestClearedFloor,
		EndlessDungeonState.TotalRuns,
		EndlessDungeonState.Revision);
	return Result;
}

FImmortalEndlessDungeonStartResult
AImmortalPlayerCharacter::CancelEndlessDungeon()
{
	if (AImmortalMonsterSpawner* Spawner = FindMapSpawner())
	{
		return Spawner->CancelEndlessDungeon();
	}
	FImmortalEndlessDungeonStartResult Result;
	Result.Message = FText::FromString(
		TEXT("Endless Dungeon controller is not ready"));
	return Result;
}

FImmortalEndlessDungeonRuntimeSnapshot
AImmortalPlayerCharacter::GetEndlessDungeonRuntimeSnapshot() const
{
	if (const AImmortalMonsterSpawner* Spawner = FindMapSpawner())
	{
		return Spawner->GetEndlessDungeonRuntimeSnapshot();
	}
	return FImmortalEndlessDungeonRuntimeSnapshot();
}

FImmortalEndlessDungeonFloorClearResult
AImmortalPlayerCharacter::CommitEndlessDungeonFloorClear(
	const int32 ClearedFloor,
	const float ClearSeconds)
{
	FImmortalEndlessDungeonFloorClearResult Result;
	if (ClearSeconds <= 0.0f)
	{
		Result.Message = FText::FromString(
			TEXT("Endless Dungeon clear time is invalid"));
		return Result;
	}
	UImmortalEndlessDungeonLibrary::NormalizeState(EndlessDungeonState);

	// Floors inside the current checkpoint segment are replays after a failed
	// run. They must be cleared again, but never create duplicate rewards.
	if (ClearedFloor >= 1
		&& ClearedFloor <= EndlessDungeonState.HighestClearedFloor)
	{
		Result.bSucceeded = true;
		Result.bRewardDelivered = true;
		Result.bRewardPending = false;
		Result.bNewRecord = false;
		Result.Message = FText::FromString(
			TEXT("Checkpoint replay cleared; no duplicate reward was generated"));
		UE_LOG(LogTemp, Display,
			TEXT("Endless Dungeon replay floor cleared without reward: floor=%d highest=%d elapsed=%.2f"),
			ClearedFloor,
			EndlessDungeonState.HighestClearedFloor,
			ClearSeconds);
		return Result;
	}

	FImmortalEndlessFloorDescriptor Descriptor;
	if (!UImmortalEndlessDungeonLibrary::GetFloorDescriptor(
		ClearedFloor, Descriptor))
	{
		Result.Message = FText::FromString(
			TEXT("Endless Dungeon floor reward definition is invalid"));
		return Result;
	}
	const FImmortalEndlessDungeonState PreviousState =
		EndlessDungeonState;
	const int64 CurrentUtcTicks = FDateTime::UtcNow().GetTicks();
	const FImmortalEndlessRecordResult Record =
		UImmortalEndlessDungeonLibrary::RecordFloorClear(
			EndlessDungeonState,
			ClearedFloor,
			CurrentUtcTicks);
	if (!Record.bSucceeded)
	{
		Result.Message = FText::FromString(
			TEXT("Endless Dungeon rejected a duplicate or skipped floor clear"));
		return Result;
	}
	const FImmortalEndlessRewardBundle Reward =
		UImmortalEndlessDungeonLibrary::CreateRewardBundle(
			Descriptor,
			CurrentUtcTicks);
	if (!Reward.IsValid())
	{
		EndlessDungeonState = PreviousState;
		Result.Message = FText::FromString(
			TEXT("Endless Dungeon reward generation failed"));
		return Result;
	}
	EndlessDungeonState.PendingRewards.Add(Reward);
	if (EndlessDungeonState.Revision < MAX_int32)
	{
		++EndlessDungeonState.Revision;
	}
	if (ShouldForceEndlessDungeonPersistenceFailure(TEXT("Commit"))
		|| !SaveProgress())
	{
		EndlessDungeonState = PreviousState;
		Result.Message = FText::FromString(
			TEXT("Endless Dungeon first reward write failed; floor and reward rolled back"));
		UE_LOG(LogTemp, Error,
			TEXT("Endless Dungeon floor commit rolled back: floor=%d"),
			ClearedFloor);
		return Result;
	}

	Result.bSucceeded = true;
	Result.bNewRecord = Record.bNewHighest;
	Result.RewardId = Reward.RewardId;
	Result.bRewardDelivered =
		TryDeliverPendingEndlessDungeonReward(Reward.RewardId);
	Result.bRewardPending =
		EndlessDungeonState.PendingRewards.ContainsByPredicate(
			[&Reward](const FImmortalEndlessRewardBundle& Pending)
			{
				return Pending.RewardId == Reward.RewardId;
			});
	Result.Message = Result.bRewardDelivered
		? FText::FromString(
			TEXT("Endless Dungeon floor reward was auto-collected and saved"))
		: FText::FromString(
			TEXT("Endless Dungeon progress is saved; reward remains pending"));
	UE_LOG(LogTemp, Display,
		TEXT("Endless Dungeon floor committed: floor=%d elapsed=%.2f delivered=%s pending=%s highest=%d total=%lld revision=%d"),
		ClearedFloor,
		ClearSeconds,
		Result.bRewardDelivered ? TEXT("true") : TEXT("false"),
		Result.bRewardPending ? TEXT("true") : TEXT("false"),
		EndlessDungeonState.HighestClearedFloor,
		EndlessDungeonState.TotalFloorsCleared,
		EndlessDungeonState.Revision);
	return Result;
}

bool AImmortalPlayerCharacter::TryDeliverPendingEndlessDungeonReward(
	const FGuid RewardId)
{
	const FImmortalEndlessRewardBundle* Pending =
		EndlessDungeonState.PendingRewards.FindByPredicate(
			[RewardId](const FImmortalEndlessRewardBundle& Reward)
			{
				return Reward.RewardId == RewardId;
			});
	if (!Pending || !Pending->IsValid())
	{
		return false;
	}
	const FImmortalEndlessRewardBundle Reward = *Pending;

	const TArray<FImmortalEquipmentItem> PreviousInventory =
		InventoryItems;
	const TArray<FImmortalEquipmentItem> PreviousEquipped =
		EquippedItems;
	const TArray<FImmortalMaterialStack> PreviousMaterials =
		MaterialInventory;
	const FImmortalEndlessDungeonState PreviousEndlessState =
		EndlessDungeonState;
	const int32 PreviousGold = CurrentGold;
	const int32 PreviousDropCount = EquipmentDropCount;
	const int32 PreviousEquipmentRevision =
		EquipmentInventoryRevision;
	const int32 PreviousMaterialRevision = MaterialInventoryRevision;
	const float PreviousHealth = CurrentHealth;
	const float PreviousMana = CurrentMana;

	auto Rollback = [this,
		&PreviousInventory,
		&PreviousEquipped,
		&PreviousMaterials,
		&PreviousEndlessState,
		PreviousGold,
		PreviousDropCount,
		PreviousEquipmentRevision,
		PreviousMaterialRevision,
		PreviousHealth,
		PreviousMana]
	{
		InventoryItems = PreviousInventory;
		EquippedItems = PreviousEquipped;
		MaterialInventory = PreviousMaterials;
		EndlessDungeonState = PreviousEndlessState;
		CurrentGold = PreviousGold;
		EquipmentDropCount = PreviousDropCount;
		EquipmentInventoryRevision = PreviousEquipmentRevision;
		MaterialInventoryRevision = PreviousMaterialRevision;
		RecalculateEquipmentBonuses();
		CurrentHealth = FMath::Clamp(
			PreviousHealth, 0.0f, GetMaxHealth());
		CurrentMana = FMath::Clamp(
			PreviousMana, 0.0f, GetMaxMana());
	};

	for (const FImmortalEquipmentItem& Item : Reward.EquipmentItems)
	{
		if (!ProcessEquipmentItem(Item, false, false, false))
		{
			Rollback();
			UE_LOG(LogTemp, Warning,
				TEXT("Endless Dungeon reward remains pending: equipment could not be stored (%d items)"),
				Reward.EquipmentItems.Num());
			return false;
		}
	}
	for (const FImmortalMaterialStack& Material : Reward.Materials)
	{
		if (AddMaterialInternal(
			Material.MaterialId,
			Material.Quantity) != Material.Quantity)
		{
			Rollback();
			UE_LOG(LogTemp, Warning,
				TEXT("Endless Dungeon reward remains pending: material %s could not be added in full"),
				*Material.MaterialId.ToString());
			return false;
		}
	}
	if (Reward.SpiritStones > 0)
	{
		const int64 NewTotal =
			static_cast<int64>(CurrentGold)
			+ static_cast<int64>(Reward.SpiritStones);
		if (NewTotal > MAX_int32)
		{
			Rollback();
			UE_LOG(LogTemp, Warning,
				TEXT("Endless Dungeon reward remains pending: spirit-stone total would overflow"));
			return false;
		}
		CurrentGold = static_cast<int32>(NewTotal);
	}

	EndlessDungeonState.PendingRewards.RemoveAll(
		[RewardId](const FImmortalEndlessRewardBundle& Entry)
		{
			return Entry.RewardId == RewardId;
		});
	if (EndlessDungeonState.Revision < MAX_int32)
	{
		++EndlessDungeonState.Revision;
	}
	if (ShouldForceEndlessDungeonPersistenceFailure(TEXT("Delivery"))
		|| !SaveProgress())
	{
		Rollback();
		UE_LOG(LogTemp, Error,
			TEXT("Endless Dungeon delivery second write failed; durable pending bundle restored"));
		return false;
	}

	if (!Reward.EquipmentItems.IsEmpty())
	{
		BP_OnEquipmentPickedUp(
			EquipmentDropCount,
			Reward.EquipmentItems.Num());
		BP_OnInventoryChanged(
			InventoryItems.Num(),
			GetInventoryCapacity());
	}
	PublishMaterialInventoryDiff(PreviousMaterials);
	if (Reward.SpiritStones > 0)
	{
		BP_OnRewardsChanged(
			CurrentCultivation,
			CurrentGold,
			0,
			Reward.SpiritStones);
	}
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->ShowEndlessDungeonRewardSummary(
			Reward.ClearedFloor,
			Reward.EquipmentItems.Num(),
			Reward.SpiritStones,
			Reward.Materials);
	}
	UE_LOG(LogTemp, Display,
		TEXT("Endless Dungeon reward delivered atomically: floor=%d equipment=%d stones=%d materials=%d cultivationReward=0"),
		Reward.ClearedFloor,
		Reward.EquipmentItems.Num(),
		Reward.SpiritStones,
		Reward.Materials.Num());
	return true;
}

bool AImmortalPlayerCharacter::RetryPendingEndlessDungeonRewards()
{
	TArray<FGuid> RewardIds;
	for (const FImmortalEndlessRewardBundle& Reward :
		EndlessDungeonState.PendingRewards)
	{
		RewardIds.Add(Reward.RewardId);
	}
	bool bAllDelivered = true;
	for (const FGuid RewardId : RewardIds)
	{
		bAllDelivered =
			TryDeliverPendingEndlessDungeonReward(RewardId)
			&& bAllDelivered;
	}
	return bAllDelivered
		&& EndlessDungeonState.PendingRewards.IsEmpty();
}

void AImmortalPlayerCharacter::CloseAllModalWidgetsExcept(const UUserWidget* ExceptWidget)
{
	auto Close = [ExceptWidget](UUserWidget* Widget, bool& bOpen)
	{
		if (Widget && Widget != ExceptWidget)
		{
			bOpen = false;
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
	};
	Close(PlayerInventoryWidget, bInventoryOpen);
	Close(PlayerAlchemyWidget, bAlchemyOpen);
	Close(PlayerCraftingWidget, bCraftingOpen);
	Close(PlayerArtifactWidget, bArtifactOpen);
	Close(PlayerTechniqueWidget, bTechniqueOpen);
	Close(PlayerCharacterBuildWidget, bCharacterBuildOpen);
	Close(PlayerShopWidget, bShopOpen);
	Close(PlayerMapWidget, bMapSelectionOpen);
	Close(PlayerCaveWidget, bCaveOpen);
	Close(PlayerFarmingWidget, bFarmingOpen);
	Close(PlayerSectWidget, bSectOpen);
	Close(PlayerWorldBossWidget, bWorldBossOpen);
	Close(PlayerEndlessDungeonWidget, bEndlessDungeonOpen);
	Close(PlayerPetWidget, bPetOpen);
	Close(PlayerAscensionWidget, bAscensionOpen);
	Close(PlayerSettingsWidget, bSettingsOpen);
}

void AImmortalPlayerCharacter::ConfigureModalWidget(UUserWidget* Widget, const bool bOpen)
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController && GetWorld())
	{
		PlayerController = GetWorld()->GetFirstPlayerController();
	}
	if (!PlayerController)
	{
		return;
	}

	// The battle HUD is interactive too; GameOnly capture swallows its clicks.
	PlayerController->bShowMouseCursor = true;
	if (bOpen && Widget)
	{
		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
		const bool bIsFullViewportScene = Widget == PlayerManagementWidget;
		const bool bIsTaskbarStripWidget = Widget == PlayerInventoryWidget
			|| Widget == PlayerManagementWidget
			|| Widget == PlayerFarmingWidget || Widget == PlayerSectWidget
			|| Widget == PlayerWorldBossWidget
			|| Widget == PlayerEndlessDungeonWidget
			|| Widget == PlayerPetWidget
			|| Widget == PlayerAscensionWidget
			|| Widget == PlayerSettingsWidget;
		const FVector2D InventorySize = bIsFullViewportScene
			? FVector2D(1707.0f, 320.0f)
			: bIsTaskbarStripWidget
				? FVector2D(1600.0f, 300.0f)
			: FVector2D(900.0f, 600.0f);
		const float AvailableWidth = FMath::Max(static_cast<float>(ViewportWidth) - 16.0f, 1.0f);
		const float AvailableHeight = FMath::Max(static_cast<float>(ViewportHeight) - 16.0f, 1.0f);
		const float FitScale = bIsFullViewportScene
			? 1.0f
			: ViewportWidth > 0 && ViewportHeight > 0
			? FMath::Clamp(FMath::Min(
				AvailableWidth / InventorySize.X,
				AvailableHeight / InventorySize.Y), 0.1f, 1.0f)
			: 1.0f;
		const float DpiScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
		// Compensate UMG DPI against the scene's fixed design canvas, not a
		// viewport-sized slot whose root SizeBox still measures 1707x320.
		const float RenderScale = FitScale / DpiScale;
		const FVector2D SceneRenderScale = bIsFullViewportScene
			? FVector2D(FMath::Max(ViewportWidth, 1) / InventorySize.X,
				FMath::Max(ViewportHeight, 1) / InventorySize.Y) / DpiScale
			: FVector2D(RenderScale);
		const FVector2D RenderedSize = InventorySize * FitScale;
		const FVector2D CentredPosition = bIsFullViewportScene
			? FVector2D::ZeroVector
			: FVector2D(
				FMath::Max((static_cast<float>(ViewportWidth) - RenderedSize.X) * 0.5f, 0.0f),
				FMath::Max((static_cast<float>(ViewportHeight) - RenderedSize.Y) * 0.5f, 0.0f));
		// Apply layout only after AddToViewport has registered the widget with
		// UE 5.7's GameViewportSubsystem; early consecutive setters can replace
		// one another while the viewport slot is still unmanaged.
		Widget->SetDesiredSizeInViewport(InventorySize);
		Widget->SetRenderTransformPivot(FVector2D::ZeroVector);
		Widget->SetRenderScale(SceneRenderScale);
		Widget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f));
		Widget->SetAlignmentInViewport(FVector2D::ZeroVector);
		Widget->SetPositionInViewport(CentredPosition, true);
		UE_LOG(LogTemp, Display,
			TEXT("Modal viewport fit applied: taskbarStrip=%s logical=%.0fx%.0f viewport=%dx%d fit=%.3f dpi=%.3f render=%.3f"),
			bIsTaskbarStripWidget ? TEXT("true") : TEXT("false"), InventorySize.X, InventorySize.Y,
			ViewportWidth, ViewportHeight, FitScale, DpiScale, RenderScale);
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(Widget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
	}
	else
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
	}
}

int64 AImmortalPlayerCharacter::GetShopSecondsUntilRefresh() const
{
	return UImmortalShopLibrary::GetSecondsUntilNextRefresh(
		FDateTime::UtcNow().GetTicks(), ShopUtcOffsetMinutes);
}

bool AImmortalPlayerCharacter::RefreshShopForDay(
	const int32 DayKey,
	const int32 QingyunStage,
	const bool bResetManualRefreshes)
{
	if (DayKey <= 0) return false;
	FImmortalShopState Generated = UImmortalShopLibrary::GenerateStock(
		QingyunStage,
		static_cast<int32>(GetCultivationRealm()),
		GetCultivationMinorStage(),
		DayKey,
		bResetManualRefreshes ? 0 : ShopState.RefreshSerial + 1);
	if (Generated.Listings.IsEmpty()) return false;
	if (!bResetManualRefreshes)
	{
		Generated.ManualRefreshCount = ShopState.ManualRefreshCount + 1;
	}
	ShopState = MoveTemp(Generated);
	++ShopRevision;
	UE_LOG(LogTemp, Display, TEXT("Treasure Pavilion stock generated: day %d | serial %d | listings %d | stage %d"),
		ShopState.RefreshDayKey, ShopState.RefreshSerial, ShopState.Listings.Num(), FMath::Clamp(QingyunStage, 1, 999));
	return true;
}

bool AImmortalPlayerCharacter::EnsureDailyShopRefresh()
{
	const int32 CurrentDayKey = UImmortalShopLibrary::GetDayKeyFromUtcTicks(
		FDateTime::UtcNow().GetTicks(), ShopUtcOffsetMinutes);
	if (!UImmortalShopLibrary::NeedsDailyRefresh(ShopState, CurrentDayKey)) return false;
	if (!RefreshShopForDay(CurrentDayKey,
		UImmortalMapLibrary::GetEffectiveAdventureStage(GetActiveMapId(), GetActiveMapStage()), true)) return false;
	SaveProgress();
	return true;
}

void AImmortalPlayerCharacter::CheckDailyShopRefresh()
{
	EnsureDailyShopRefresh();
	const int64 CurrentUtcTicks = FDateTime::UtcNow().GetTicks();
	const int32 CurrentSectDayKey = UImmortalSectLibrary::GetDayKeyFromUtcTicks(
		CurrentUtcTicks, SectUtcOffsetMinutes);
	if (CurrentSectDayKey > SectState.TaskDayKey)
	{
		EnsureSectDailyState(CurrentUtcTicks);
	}
}

bool AImmortalPlayerCharacter::CanBuyShopListing(const FGuid ListingId) const
{
	const FImmortalShopListing* Listing = ShopState.Listings.FindByPredicate([ListingId](const FImmortalShopListing& Entry)
	{
		return Entry.ListingId == ListingId;
	});
	if (!Listing || !Listing->IsValid() || Listing->bSoldOut || CurrentGold < Listing->BundlePrice) return false;
	switch (Listing->ProductType)
	{
	case EImmortalShopProductType::Equipment:
		if (InventoryItems.Num() < FMath::Max(InventoryCapacity, 1)) return true;
		return bAutoEquipNewItems
			&& IsEquipmentCompatibleWithPath(Listing->EquipmentItem)
			&& !EquippedItems.ContainsByPredicate([Listing](const FImmortalEquipmentItem& Item)
			{
				return Item.Slot == Listing->EquipmentItem.Slot;
			});
	case EImmortalShopProductType::Material:
	{
		FImmortalMaterialDefinition Definition;
		return UImmortalMaterialLibrary::GetMaterialDefinition(Listing->ProductId, Definition)
			&& GetMaterialQuantity(Listing->ProductId)
			<= FMath::Max(Definition.MaximumStack, 1) - Listing->BundleQuantity;
	}
	case EImmortalShopProductType::Pill:
		return GetPillQuantity(Listing->ProductId, Listing->PillQuality) <= 9999 - Listing->BundleQuantity;
	case EImmortalShopProductType::Artifact:
	{
		FImmortalArtifactDefinition Definition;
		return UImmortalArtifactLibrary::GetArtifactDefinition(Listing->ProductId, Definition);
	}
	default:
		return false;
	}
}

FImmortalShopTransactionResult AImmortalPlayerCharacter::BuyShopListing(const FGuid ListingId)
{
	FImmortalShopTransactionResult Result;
	Result.ListingId = ListingId;
	FImmortalShopListing* Listing = ShopState.Listings.FindByPredicate([ListingId](const FImmortalShopListing& Entry)
	{
		return Entry.ListingId == ListingId;
	});
	if (!Listing || !Listing->IsValid())
	{
		Result.Message = FText::FromString(TEXT("商品不存在或数据已失效"));
		return Result;
	}
	if (Listing->bSoldOut)
	{
		Result.Message = FText::FromString(TEXT("该商品今日已经售罄"));
		return Result;
	}
	Result.bAffordable = CurrentGold >= Listing->BundlePrice;
	if (!Result.bAffordable)
	{
		Result.Message = FText::FromString(TEXT("灵石不足，无法购买"));
		return Result;
	}
	Result.bHadCapacity = CanBuyShopListing(ListingId);
	if (!Result.bHadCapacity)
	{
		Result.Message = Listing->ProductType == EImmortalShopProductType::Equipment
			? FText::FromString(TEXT("装备背包已满，请先出售一件装备"))
			: FText::FromString(TEXT("该物品堆叠已达上限"));
		return Result;
	}

	const FImmortalShopState PreviousShop = ShopState;
	const TArray<FImmortalEquipmentItem> PreviousInventory = InventoryItems;
	const TArray<FImmortalEquipmentItem> PreviousEquipped = EquippedItems;
	const TArray<FImmortalMaterialStack> PreviousMaterials = MaterialInventory;
	const TArray<FImmortalPillStack> PreviousPills = PillInventory;
	const TArray<FImmortalArtifactItem> PreviousArtifacts = ArtifactInventory;
	const int32 PreviousGold = CurrentGold;
	const float PreviousHealth = CurrentHealth;
	const float PreviousMana = CurrentMana;
	const int32 PreviousEquipmentRevision = EquipmentInventoryRevision;
	const int32 PreviousMaterialRevision = MaterialInventoryRevision;
	const int32 PreviousPillRevision = PillInventoryRevision;
	const int32 PreviousArtifactRevision = ArtifactInventoryRevision;
	const int32 PreviousShopRevision = ShopRevision;
	const FImmortalShopListing PurchasedListing = *Listing;
	bool bAdded = false;
	bool bAutoEquipped = false;

	switch (PurchasedListing.ProductType)
	{
	case EImmortalShopProductType::Equipment:
	{
		const FImmortalEquipmentItem PurchasedItem = PurchasedListing.EquipmentItem;
		InventoryItems.Add(PurchasedItem);
		bAdded = true;
		const int32 EquippedIndex = EquippedItems.IndexOfByPredicate([&PurchasedItem](const FImmortalEquipmentItem& Item)
		{
			return Item.Slot == PurchasedItem.Slot;
		});
		const bool bCompatible = UImmortalCharacterPathLibrary::IsEquipmentCompatible(
			CultivationPathState.Path, PurchasedItem.Discipline);
		auto CalculateCompatibleLoadoutPower = [this](const TArray<FImmortalEquipmentItem>& Loadout)
		{
			TArray<FImmortalEquipmentItem> CompatibleLoadout;
			for (const FImmortalEquipmentItem& Candidate : Loadout)
			{
				if (IsEquipmentCompatibleWithPath(Candidate)) CompatibleLoadout.Add(Candidate);
			}
			const float CultivationAttack = CultivationComponent ? CultivationComponent->GetAttackBonus() : 0.0f;
			const float CultivationDefense = CultivationComponent ? CultivationComponent->GetDefenseBonus() : 0.0f;
			const float CultivationHealth = CultivationComponent ? CultivationComponent->GetHealthBonus() : 0.0f;
			return UImmortalEquipmentLibrary::CalculateLoadoutPowerWithBaseStats(
				CompatibleLoadout,
				AttackDamage + CultivationAttack,
				Defense + CultivationDefense,
				MaxHealth + CultivationHealth,
				AttackSpeedMultiplier + ArtifactAttackSpeedBonus + TechniqueAttackSpeedBonus + CharacterPathAttackSpeedBonus,
				CriticalChance + ArtifactCriticalChanceBonus + TechniqueCriticalChanceBonus + CharacterPathCriticalChanceBonus,
				CriticalDamageMultiplier);
		};
		const float ExistingLoadoutPower = CalculateCompatibleLoadoutPower(EquippedItems);
		TArray<FImmortalEquipmentItem> CandidateLoadout = EquippedItems;
		if (EquippedIndex == INDEX_NONE) CandidateLoadout.Add(PurchasedItem);
		else CandidateLoadout[EquippedIndex] = PurchasedItem;
		const float CandidateLoadoutPower = CalculateCompatibleLoadoutPower(CandidateLoadout);
		if (bAutoEquipNewItems && bCompatible
			&& (EquippedIndex == INDEX_NONE || !EquippedItems[EquippedIndex].bLocked)
			&& CandidateLoadoutPower > ExistingLoadoutPower + KINDA_SMALL_NUMBER)
		{
			const int32 PurchasedIndex = InventoryItems.IndexOfByPredicate([&PurchasedItem](const FImmortalEquipmentItem& Item)
			{
				return Item.ItemId == PurchasedItem.ItemId;
			});
			if (PurchasedIndex != INDEX_NONE) InventoryItems.RemoveAt(PurchasedIndex);
			if (EquippedIndex == INDEX_NONE) EquippedItems.Add(PurchasedItem);
			else
			{
				InventoryItems.Add(EquippedItems[EquippedIndex]);
				EquippedItems[EquippedIndex] = PurchasedItem;
			}
			bAutoEquipped = true;
		}
		++EquipmentInventoryRevision;
		RecalculateEquipmentBonuses();
		break;
	}
	case EImmortalShopProductType::Material:
		bAdded = UImmortalMaterialLibrary::AddMaterialStack(
			MaterialInventory, PurchasedListing.ProductId, PurchasedListing.BundleQuantity) == PurchasedListing.BundleQuantity;
		if (bAdded) ++MaterialInventoryRevision;
		break;
	case EImmortalShopProductType::Pill:
		bAdded = UImmortalAlchemyLibrary::AddPillStack(
			PillInventory, PurchasedListing.ProductId, PurchasedListing.PillQuality,
			PurchasedListing.BundleQuantity) == PurchasedListing.BundleQuantity;
		if (bAdded) ++PillInventoryRevision;
		break;
	case EImmortalShopProductType::Artifact:
	{
		const FImmortalArtifactItem Artifact = UImmortalArtifactLibrary::CreateArtifact(PurchasedListing.ProductId);
		bAdded = Artifact.IsValid();
		if (bAdded)
		{
			ArtifactInventory.Add(Artifact);
			++ArtifactInventoryRevision;
		}
		break;
	}
	default:
		break;
	}

	if (!bAdded)
	{
		InventoryItems = PreviousInventory;
		EquippedItems = PreviousEquipped;
		MaterialInventory = PreviousMaterials;
		PillInventory = PreviousPills;
		ArtifactInventory = PreviousArtifacts;
		EquipmentInventoryRevision = PreviousEquipmentRevision;
		MaterialInventoryRevision = PreviousMaterialRevision;
		PillInventoryRevision = PreviousPillRevision;
		ArtifactInventoryRevision = PreviousArtifactRevision;
		RecalculateEquipmentBonuses();
		CurrentHealth = PreviousHealth;
		CurrentMana = PreviousMana;
		Result.Message = FText::FromString(TEXT("物品写入背包失败，交易未扣款"));
		return Result;
	}

	CurrentGold -= PurchasedListing.BundlePrice;
	Listing->bSoldOut = true;
	++ShopRevision;
	if (!SaveProgress())
	{
		ShopState = PreviousShop;
		InventoryItems = PreviousInventory;
		EquippedItems = PreviousEquipped;
		MaterialInventory = PreviousMaterials;
		PillInventory = PreviousPills;
		ArtifactInventory = PreviousArtifacts;
		CurrentGold = PreviousGold;
		EquipmentInventoryRevision = PreviousEquipmentRevision;
		MaterialInventoryRevision = PreviousMaterialRevision;
		PillInventoryRevision = PreviousPillRevision;
		ArtifactInventoryRevision = PreviousArtifactRevision;
		ShopRevision = PreviousShopRevision;
		RecalculateEquipmentBonuses();
		CurrentHealth = PreviousHealth;
		CurrentMana = PreviousMana;
		Result.Message = FText::FromString(TEXT("存档写入失败，交易已回滚"));
		return Result;
	}

	Result.bSucceeded = true;
	Result.SpiritStoneDelta = -PurchasedListing.BundlePrice;
	Result.Message = FText::FromString(FString::Printf(TEXT("购买成功：%s ×%d，消耗灵石 %d"),
		*UImmortalShopLibrary::GetListingDisplayName(PurchasedListing).ToString(),
		PurchasedListing.BundleQuantity, PurchasedListing.BundlePrice));
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, Result.SpiritStoneDelta);
	if (PurchasedListing.ProductType == EImmortalShopProductType::Equipment)
	{
		BP_OnInventoryChanged(InventoryItems.Num(), FMath::Max(InventoryCapacity, 1));
		if (bAutoEquipped)
		{
			BP_OnEquipmentChanged(PurchasedListing.EquipmentItem.Slot, PurchasedListing.EquipmentItem, true, GetCombatPower());
			if (!bDead && bAutoAttackOnBeginPlay) StartAutoAttack();
		}
	}
	else if (PurchasedListing.ProductType == EImmortalShopProductType::Material)
	{
		BP_OnMaterialInventoryChanged(PurchasedListing.ProductId,
			GetMaterialQuantity(PurchasedListing.ProductId), PurchasedListing.BundleQuantity);
	}
	else if (PurchasedListing.ProductType == EImmortalShopProductType::Artifact)
	{
		const FImmortalArtifactItem& AddedArtifact = ArtifactInventory.Last();
		BP_OnArtifactChanged(AddedArtifact, false);
	}
	BP_OnShopTransaction(Result);
	UE_LOG(LogTemp, Display, TEXT("Shop purchase succeeded: type %d | listing %s | price %d | stones %d | sold out true"),
		static_cast<int32>(PurchasedListing.ProductType), *ListingId.ToString(), PurchasedListing.BundlePrice, CurrentGold);
	return Result;
}

FImmortalShopTransactionResult AImmortalPlayerCharacter::RefreshShopInventory()
{
	FImmortalShopTransactionResult Result;
	EnsureDailyShopRefresh();
	const int32 Cost = UImmortalShopLibrary::GetManualRefreshCost(ShopState);
	Result.bAffordable = CurrentGold >= Cost;
	Result.bHadCapacity = true;
	if (!Result.bAffordable)
	{
		Result.Message = FText::FromString(FString::Printf(TEXT("刷新需要 %d 灵石"), Cost));
		return Result;
	}
	const int32 DayKey = FMath::Max(ShopState.RefreshDayKey,
		UImmortalShopLibrary::GetDayKeyFromUtcTicks(FDateTime::UtcNow().GetTicks(), ShopUtcOffsetMinutes));
	FImmortalShopState Candidate = UImmortalShopLibrary::GenerateStock(
		UImmortalMapLibrary::GetEffectiveAdventureStage(GetActiveMapId(), GetActiveMapStage()),
		static_cast<int32>(GetCultivationRealm()), GetCultivationMinorStage(),
		DayKey, ShopState.RefreshSerial + 1);
	if (Candidate.Listings.IsEmpty())
	{
		Result.Message = FText::FromString(TEXT("今日货源生成失败，未扣除灵石"));
		return Result;
	}
	Candidate.ManualRefreshCount = ShopState.ManualRefreshCount + 1;
	const FImmortalShopState Previous = ShopState;
	const int32 PreviousGold = CurrentGold;
	const int32 PreviousRevision = ShopRevision;
	ShopState = MoveTemp(Candidate);
	CurrentGold -= Cost;
	++ShopRevision;
	if (!SaveProgress())
	{
		ShopState = Previous;
		CurrentGold = PreviousGold;
		ShopRevision = PreviousRevision;
		Result.Message = FText::FromString(TEXT("存档写入失败，刷新已回滚"));
		return Result;
	}
	Result.bSucceeded = true;
	Result.SpiritStoneDelta = -Cost;
	Result.Message = FText::FromString(FString::Printf(TEXT("百宝阁已换新货，消耗灵石 %d"), Cost));
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, Result.SpiritStoneDelta);
	BP_OnShopTransaction(Result);
	UE_LOG(LogTemp, Display, TEXT("Shop manual refresh succeeded: day %d | serial %d | cost %d | listings %d"),
		ShopState.RefreshDayKey, ShopState.RefreshSerial, Cost, ShopState.Listings.Num());
	return Result;
}

int32 AImmortalPlayerCharacter::GetEquipmentShopSellPrice(const FGuid ItemId) const
{
	const FImmortalEquipmentItem* Item = InventoryItems.FindByPredicate([ItemId](const FImmortalEquipmentItem& Entry)
	{
		return Entry.ItemId == ItemId;
	});
	return Item && !Item->bLocked ? UImmortalShopLibrary::GetEquipmentSellPrice(*Item) : 0;
}

FImmortalShopTransactionResult AImmortalPlayerCharacter::SellEquipmentToShop(const FGuid ItemId)
{
	FImmortalShopTransactionResult Result;
	const int32 Index = InventoryItems.IndexOfByPredicate([ItemId](const FImmortalEquipmentItem& Entry)
	{
		return Entry.ItemId == ItemId;
	});
	if (Index == INDEX_NONE)
	{
		Result.Message = FText::FromString(TEXT("只能出售背包中的装备，已装备物品不会被回收"));
		return Result;
	}
	const FImmortalEquipmentItem SoldItem = InventoryItems[Index];
	if (SoldItem.bLocked)
	{
		Result.Message = FText::FromString(TEXT("该装备已锁定，请先在背包中解锁"));
		return Result;
	}
	const int32 Price = UImmortalShopLibrary::GetEquipmentSellPrice(SoldItem);
	if (Price <= 0)
	{
		Result.Message = FText::FromString(TEXT("该装备无法估价"));
		return Result;
	}
	if (!UImmortalInventoryLibrary::CanReceiveSpiritStones(CurrentGold, Price))
	{
		Result.Message = FText::FromString(TEXT("灵石接近上限，无法完整接收售价；装备未出售"));
		return Result;
	}
	const TArray<FImmortalEquipmentItem> PreviousInventory = InventoryItems;
	const int32 PreviousGold = CurrentGold;
	const int32 PreviousRevision = EquipmentInventoryRevision;
	InventoryItems.RemoveAt(Index);
	CurrentGold += Price;
	++EquipmentInventoryRevision;
	if (ShouldForceInventoryPersistenceFailure(TEXT("Sell")) || !SaveProgress())
	{
		InventoryItems = PreviousInventory;
		CurrentGold = PreviousGold;
		EquipmentInventoryRevision = PreviousRevision;
		Result.Message = FText::FromString(TEXT("存档写入失败，出售已回滚"));
		return Result;
	}
	Result.bSucceeded = true;
	Result.bAffordable = true;
	Result.bHadCapacity = true;
	Result.SpiritStoneDelta = CurrentGold - PreviousGold;
	Result.Message = FText::FromString(FString::Printf(TEXT("已出售 %s，获得灵石 %d"),
		*SoldItem.DisplayName.ToString(), Result.SpiritStoneDelta));
	BP_OnInventoryChanged(InventoryItems.Num(), FMath::Max(InventoryCapacity, 1));
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, Result.SpiritStoneDelta);
	BP_OnShopTransaction(Result);
	UE_LOG(LogTemp, Display, TEXT("Shop equipment sale succeeded: item %s | price %d | stones %d | backpack %d"),
		*ItemId.ToString(), Result.SpiritStoneDelta, CurrentGold, InventoryItems.Num());
	return Result;
}

int32 AImmortalPlayerCharacter::GetMaterialShopSellPrice(const FName MaterialId, const int32 Amount) const
{
	if (Amount <= 0 || GetMaterialQuantity(MaterialId) < Amount) return 0;
	return static_cast<int32>(FMath::Min<int64>(
		static_cast<int64>(UImmortalShopLibrary::GetMaterialUnitSellPrice(MaterialId)) * Amount, MAX_int32));
}

FImmortalShopTransactionResult AImmortalPlayerCharacter::SellMaterialToShop(
	const FName MaterialId,
	const int32 Amount)
{
	FImmortalShopTransactionResult Result;
	const int32 Price = GetMaterialShopSellPrice(MaterialId, Amount);
	if (Price <= 0)
	{
		Result.Message = FText::FromString(TEXT("材料数量不足或无法估价"));
		return Result;
	}
	const TArray<FImmortalMaterialStack> PreviousMaterials = MaterialInventory;
	const int32 PreviousGold = CurrentGold;
	const int32 PreviousRevision = MaterialInventoryRevision;
	if (!UImmortalMaterialLibrary::RemoveMaterialStack(MaterialInventory, MaterialId, Amount))
	{
		Result.Message = FText::FromString(TEXT("材料出售事务失败"));
		return Result;
	}
	CurrentGold = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(CurrentGold) + Price, MAX_int32));
	++MaterialInventoryRevision;
	if (!SaveProgress())
	{
		MaterialInventory = PreviousMaterials;
		CurrentGold = PreviousGold;
		MaterialInventoryRevision = PreviousRevision;
		Result.Message = FText::FromString(TEXT("存档写入失败，出售已回滚"));
		return Result;
	}
	FImmortalMaterialDefinition Definition;
	UImmortalMaterialLibrary::GetMaterialDefinition(MaterialId, Definition);
	Result.bSucceeded = true;
	Result.bAffordable = true;
	Result.bHadCapacity = true;
	Result.SpiritStoneDelta = CurrentGold - PreviousGold;
	Result.Message = FText::FromString(FString::Printf(TEXT("已出售 %s ×%d，获得灵石 %d"),
		*Definition.DisplayName.ToString(), Amount, Result.SpiritStoneDelta));
	BP_OnMaterialInventoryChanged(MaterialId, GetMaterialQuantity(MaterialId), -Amount);
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, Result.SpiritStoneDelta);
	BP_OnShopTransaction(Result);
	UE_LOG(LogTemp, Display, TEXT("Shop material sale succeeded: %s x%d | price %d | stones %d | remaining %d"),
		*MaterialId.ToString(), Amount, Result.SpiritStoneDelta, CurrentGold, GetMaterialQuantity(MaterialId));
	return Result;
}

void AImmortalPlayerCharacter::ConfigureCombatCamera()
{
	if (!bConfigureCombatCamera)
	{
		return;
	}

	if (USpringArmComponent* SpringArm = FindComponentByClass<USpringArmComponent>())
	{
		// Crowded monsters can otherwise shorten the boom and make the taskbar
		// view suddenly zoom into a sprite during long unattended sessions.
		SpringArm->bDoCollisionTest = false;
		// Keep the camera on the Paper2D plane, then move the world-space view
		// centre toward the incoming wave. These axes were calibrated in the
		// independent 320 px taskbar strip rather than inferred from the boom.
		FVector CameraOffset = SpringArm->SocketOffset;
		CameraOffset.Y += CameraDepthOffset;
		SpringArm->SocketOffset = CameraOffset;
		FVector ViewTargetOffset = SpringArm->TargetOffset;
		ViewTargetOffset.X += CameraLeadDistance;
		SpringArm->TargetOffset = ViewTargetOffset;
		UE_LOG(LogTemp, Display, TEXT("TBH camera horizontal lead applied: %.1f cm"), CameraLeadDistance);
	}

	if (UCameraComponent* Camera = FindComponentByClass<UCameraComponent>())
	{
		// TBH mode uses the actual ultra-wide taskbar strip aspect ratio.
		Camera->SetConstraintAspectRatio(false);
		if (Camera->ProjectionMode == ECameraProjectionMode::Orthographic)
		{
			Camera->SetOrthoWidth(FMath::Max(CombatOrthoWidth, 100.0f));
		}
		else
		{
			Camera->SetFieldOfView(FMath::Clamp(CombatPerspectiveFOV, 5.0f, 170.0f));
		}
		UE_LOG(LogTemp, Display, TEXT("Combat camera ready for taskbar aspect: projection %s | ortho width %.1f | FOV %.1f"),
			Camera->ProjectionMode == ECameraProjectionMode::Orthographic ? TEXT("Orthographic") : TEXT("Perspective"),
			Camera->OrthoWidth, Camera->FieldOfView);
	}
}

void AImmortalPlayerCharacter::ApplyDesktopSettings()
{
	UImmortalDesktopSettings* Settings =
		UImmortalDesktopSettings::GetMutable();
	if (!Settings)
	{
		return;
	}
	Settings->LoadFromDisk();
	bTaskbarWindowAlwaysOnTop = Settings->bAlwaysOnTop;
	TaskbarWindowHeight = Settings->WindowHeight;
	FApp::SetVolumeMultiplier(
		Settings->bMuted ? 0.0f : 1.0f);
	if (GEngine)
	{
		GEngine->SetMaxFPS(
			static_cast<float>(Settings->FrameRateLimit));
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Desktop settings applied: topmost=%s muted=%s fps=%d height=%d"),
		Settings->bAlwaysOnTop ? TEXT("true") : TEXT("false"),
		Settings->bMuted ? TEXT("true") : TEXT("false"),
		Settings->FrameRateLimit,
		Settings->WindowHeight);
}

void AImmortalPlayerCharacter::ConfigureTaskbarWindow()
{
	if (!bEnableTaskbarWindowMode || !GetWorld()) return;
	const EWorldType::Type WorldType = GetWorld()->WorldType;
	if (WorldType == EWorldType::PIE || WorldType == EWorldType::Editor || WorldType == EWorldType::EditorPreview)
	{
		UE_LOG(LogTemp, Display, TEXT("TBH taskbar window mode prepared; window docking is skipped in PIE."));
		return;
	}

	GetWorldTimerManager().SetTimer(
		TaskbarWindowTimerHandle,
		this,
		&AImmortalPlayerCharacter::ApplyTaskbarWindowPlacement,
		0.75f,
		false);
}

void AImmortalPlayerCharacter::ApplyTaskbarWindowPlacement()
{
#if PLATFORM_WINDOWS
	if (!GetWorld()) return;
	const EWorldType::Type WorldType = GetWorld()->WorldType;
	if (WorldType == EWorldType::PIE
		|| WorldType == EWorldType::Editor
		|| WorldType == EWorldType::EditorPreview)
	{
		return;
	}
	if (!GEngine || !GEngine->GameViewport) return;
	const TSharedPtr<SWindow> GameWindow = GEngine->GameViewport->GetWindow();
	if (!GameWindow.IsValid() || !GameWindow->GetNativeWindow().IsValid()) return;

	HWND WindowHandle = static_cast<HWND>(GameWindow->GetNativeWindow()->GetOSWindowHandle());
	if (!WindowHandle) return;
	MONITORINFO MonitorInfo = {};
	MonitorInfo.cbSize = sizeof(MonitorInfo);
	if (!GetMonitorInfo(MonitorFromWindow(WindowHandle, MONITOR_DEFAULTTONEAREST), &MonitorInfo)) return;
	const RECT WorkArea = MonitorInfo.rcWork;

	const int32 WorkWidth = static_cast<int32>(WorkArea.right - WorkArea.left);
	const int32 WorkHeight = static_cast<int32>(WorkArea.bottom - WorkArea.top);
	const int32 Width = FMath::Max(WorkWidth, 640);
	const int32 Height = FMath::Clamp(TaskbarWindowHeight, 180, FMath::Max(WorkHeight / 2, 180));
	const int32 Top = WorkArea.bottom - Height;
	LONG_PTR Style = GetWindowLongPtr(WindowHandle, GWL_STYLE);
	Style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
	Style |= WS_POPUP;
	SetWindowLongPtr(WindowHandle, GWL_STYLE, Style);
	SetWindowPos(
		WindowHandle,
		bTaskbarWindowAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
		WorkArea.left,
		Top,
		Width,
		Height,
		SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOACTIVATE);
	ImmortalDesktopWindow::ApplyTransparency(GetWorld(),
		UImmortalDesktopSettings::GetMutable()->bTransparentBackground);
	UE_LOG(LogTemp, Display, TEXT("TBH taskbar window applied: %dx%d at %d,%d | topmost=%s"),
		Width, Height, WorkArea.left, Top, bTaskbarWindowAlwaysOnTop ? TEXT("true") : TEXT("false"));
	// The viewport scale changes after the native TBH window is resized. Reapply
	// the active modal's viewport geometry so it remains centered and fully visible.
	if (bAscensionOpen) ConfigureModalWidget(PlayerAscensionWidget, true);
	else if (bManagementInterfaceOpen) ConfigureModalWidget(PlayerManagementWidget, true);
	else if (bInventoryOpen) ConfigureModalWidget(PlayerInventoryWidget, true);
	else if (bAlchemyOpen) ConfigureModalWidget(PlayerAlchemyWidget, true);
	else if (bCraftingOpen) ConfigureModalWidget(PlayerCraftingWidget, true);
	else if (bArtifactOpen) ConfigureModalWidget(PlayerArtifactWidget, true);
	else if (bTechniqueOpen) ConfigureModalWidget(PlayerTechniqueWidget, true);
	else if (bCharacterBuildOpen) ConfigureModalWidget(PlayerCharacterBuildWidget, true);
	else if (bShopOpen) ConfigureModalWidget(PlayerShopWidget, true);
	else if (bMapSelectionOpen) ConfigureModalWidget(PlayerMapWidget, true);
	else if (bCaveOpen) ConfigureModalWidget(PlayerCaveWidget, true);
	else if (bFarmingOpen) ConfigureModalWidget(PlayerFarmingWidget, true);
	else if (bSectOpen) ConfigureModalWidget(PlayerSectWidget, true);
	else if (bWorldBossOpen) ConfigureModalWidget(PlayerWorldBossWidget, true);
	else if (bEndlessDungeonOpen) ConfigureModalWidget(PlayerEndlessDungeonWidget, true);
	else if (bPetOpen) ConfigureModalWidget(PlayerPetWidget, true);
	else if (bSettingsOpen) ConfigureModalWidget(PlayerSettingsWidget, true);
#endif
}

float AImmortalPlayerCharacter::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	if (bDead || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}
	if (GetWorld() && GetWorld()->GetTimeSeconds() < InvulnerableUntilTime)
	{
		return 0.0f;
	}

	const float EngineAcceptedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	const float RequestedDamage = EngineAcceptedDamage > 0.0f ? EngineAcceptedDamage : DamageAmount;
	const float AfterDefense = FMath::Max(RequestedDamage - FMath::Max(GetTotalDefense(), 0.0f), 0.0f);
	const float CombinedReduction = 1.0f
		- (1.0f - FMath::Clamp(CharacterPathDamageReduction, 0.0f, 0.75f))
		* (1.0f - FMath::Clamp(EquipmentDamageReduction, 0.0f, 0.75f));
	const float ReducedDamage = FMath::Max(
		AfterDefense * (1.0f - FMath::Clamp(CombinedReduction, 0.0f, 0.85f)), 1.0f);
	const float ArtifactShieldAbsorbed = FMath::Min(FMath::Max(ArtifactShield, 0.0f), ReducedDamage);
	ArtifactShield = FMath::Max(ArtifactShield - ArtifactShieldAbsorbed, 0.0f);
	const float AfterArtifactShield = FMath::Max(ReducedDamage - ArtifactShieldAbsorbed, 0.0f);
	const float TechniqueShieldAbsorbed = FMath::Min(FMath::Max(TechniqueShield, 0.0f), AfterArtifactShield);
	TechniqueShield = FMath::Max(TechniqueShield - TechniqueShieldAbsorbed, 0.0f);
	const float AfterTechniqueShield = FMath::Max(AfterArtifactShield - TechniqueShieldAbsorbed, 0.0f);
	const float PathShieldAbsorbed = FMath::Min(FMath::Max(CultivationPathShield, 0.0f), AfterTechniqueShield);
	CultivationPathShield = FMath::Max(CultivationPathShield - PathShieldAbsorbed, 0.0f);
	const float DamageApplied = FMath::Min(FMath::Max(AfterTechniqueShield - PathShieldAbsorbed, 0.0f), CurrentHealth);
	CurrentHealth = FMath::Max(CurrentHealth - DamageApplied, 0.0f);
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->ShowDamage(GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), DamageApplied, false, true);
	}
	BP_OnPlayerDamaged(DamageApplied, CurrentHealth, DamageCauser);

	if (CurrentHealth <= 0.0f)
	{
		bDead = true;
		StopAutoAttack();
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BP_OnPlayerDied(DamageCauser);
		PlayMortalRealmDeathAnimation();
		BeginDeathCultivationRecovery();
		if (GetWorld())
		{
			GetWorldTimerManager().SetTimer(
				AutoReviveTimerHandle,
				this,
				&AImmortalPlayerCharacter::AutoRevive,
				FMath::Max(AutoReviveDelay, 0.1f),
				false);
		}
	}
	else if (DamageApplied > 0.0f)
	{
		PlayMortalRealmHurtAnimation();
	}

	return DamageApplied + ArtifactShieldAbsorbed + TechniqueShieldAbsorbed + PathShieldAbsorbed;
}

float AImmortalPlayerCharacter::GetHealthPercent() const
{
	return GetMaxHealth() > 0.0f ? CurrentHealth / GetMaxHealth() : 0.0f;
}

float AImmortalPlayerCharacter::GetMaxHealth() const
{
	const float CultivationBonus = CultivationComponent ? CultivationComponent->GetHealthBonus() : 0.0f;
	return FMath::Max((MaxHealth + EquippedHealthBonus + CultivationBonus)
		* EquipmentHealthMultiplier * ArtifactHealthMultiplier * TechniqueHealthMultiplier * CharacterPathHealthMultiplier, 1.0f);
}

float AImmortalPlayerCharacter::GetMaxMana() const
{
	const float CultivationBonus = CultivationComponent ? CultivationComponent->GetManaBonus() : 0.0f;
	return FMath::Max((MaxMana + CultivationBonus) * CharacterPathManaMultiplier, 0.0f);
}

float AImmortalPlayerCharacter::GetManaPercent() const
{
	return GetMaxMana() > 0.0f ? CurrentMana / GetMaxMana() : 0.0f;
}

float AImmortalPlayerCharacter::GetTotalAttackDamage() const
{
	const float CultivationBonus = CultivationComponent ? CultivationComponent->GetAttackBonus() : 0.0f;
	return (AttackDamage + EquippedAttackBonus + CultivationBonus)
		* EquipmentAttackMultiplier * ArtifactAttackMultiplier
		* TechniqueAttackMultiplier * CharacterPathAttackMultiplier
		* GetAscensionBattleMultiplier();
}

float AImmortalPlayerCharacter::GetTotalDefense() const
{
	const float CultivationBonus = CultivationComponent ? CultivationComponent->GetDefenseBonus() : 0.0f;
	return (Defense + EquippedDefenseBonus + CultivationBonus)
		* EquipmentDefenseMultiplier * ArtifactDefenseMultiplier * TechniqueDefenseMultiplier * CharacterPathDefenseMultiplier;
}

float AImmortalPlayerCharacter::GetEquipmentDropChanceMultiplier() const
{
	return FMath::Clamp(
		(1.0f + EquippedLootFindBonus)
			* GetAscensionEquipmentDropMultiplier(),
		0.0f,
		5.0f);
}

float AImmortalPlayerCharacter::GetEffectiveAttackInterval() const
{
	return FMath::Max(AttackInterval, 0.05f) / FMath::Max(GetTotalAttackSpeedMultiplier(), 0.1f);
}

float AImmortalPlayerCharacter::GetCombatPower() const
{
	return GetMaxHealth() * 0.2f
		+ GetTotalAttackDamage() * 5.0f
		+ GetTotalDefense() * 4.0f
		+ GetTotalAttackSpeedMultiplier() * 20.0f
		+ GetTotalCriticalChance() * 100.0f
		+ (GetTotalCriticalDamageMultiplier() - 1.0f) * 70.0f
		+ (EquippedFireDamageBonus + EquippedThunderDamageBonus + EquippedIceDamageBonus) * 55.0f
		+ EquippedLifeStealBonus * 160.0f + EquippedCultivationGainBonus * 60.0f
		+ EquippedLootFindBonus * 80.0f + EquippedBossDamageBonus * 70.0f
		+ EquipmentFinalDamageBonus * 90.0f + EquipmentDamageReduction * 120.0f
		+ (GetAscensionCultivationMultiplier() - 1.0f) * 120.0f
		+ (GetAscensionEquipmentDropMultiplier() - 1.0f) * 100.0f;
}

FText AImmortalPlayerCharacter::GetEquipmentSetSummaryText() const
{
	TArray<FImmortalEquipmentItem> CompatibleItems;
	for (const FImmortalEquipmentItem& Item : EquippedItems)
	{
		if (IsEquipmentCompatibleWithPath(Item)) CompatibleItems.Add(Item);
	}
	return UImmortalEquipmentLibrary::GetSetSummaryText(CompatibleItems);
}

EImmortalCultivationRealm AImmortalPlayerCharacter::GetCultivationRealm() const
{
	return CultivationComponent
		? CultivationComponent->GetCurrentRealm()
		: EImmortalCultivationRealm::QiRefining;
}

int32 AImmortalPlayerCharacter::GetCultivationMinorStage() const
{
	return CultivationComponent ? CultivationComponent->GetCurrentMinorStage() : 1;
}

FText AImmortalPlayerCharacter::GetFullCultivationRealmName() const
{
	return CultivationComponent
		? CultivationComponent->GetFullRealmName()
		: FText::FromString(TEXT("炼气一层"));
}

int32 AImmortalPlayerCharacter::GetRequiredCultivation() const
{
	return CultivationComponent ? CultivationComponent->GetRequiredCultivation() : 1;
}

float AImmortalPlayerCharacter::GetCultivationPerSecond() const
{
	return CultivationComponent ? CultivationComponent->GetCultivationPerSecond() : 0.0f;
}

void AImmortalPlayerCharacter::ReceiveKillRewards(const int32 CultivationGained, const int32 GoldGained)
{
	// Kill rewards no longer drive cultivation or currency. Both systems use
	// their own physical/independent progression paths from this version on.
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, 0);
}

void AImmortalPlayerCharacter::ReceiveSpiritStones(const int32 Amount, const FVector PickupWorldLocation)
{
	const int32 SafeAmount = FMath::Max(Amount, 0);
	if (SafeAmount <= 0) return;
	const int32 PreviousGold = CurrentGold;
	CurrentGold = static_cast<int32>(FMath::Min<int64>(
		static_cast<int64>(CurrentGold) + SafeAmount, MAX_int32));
	const int32 GrantedAmount = CurrentGold - PreviousGold;
	if (GrantedAmount <= 0) return;
	const bool bQuestChanged = RecordQuestProgressWithoutSave(
		EImmortalQuestMetric::SpiritStonesCollected,
		GrantedAmount);
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->ShowRewards(PickupWorldLocation, 0, GrantedAmount);
	}
	QueueManagementNotification(
		FText::FromString(FString::Printf(
			TEXT("\u7075\u77F3 +%d\uFF08\u5DF2\u81EA\u52A8\u62FE\u53D6\uFF09"),
			GrantedAmount)),
		FLinearColor(1.0f, 0.82f, 0.32f, 1.0f),
		5.0f);
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, GrantedAmount);
	UE_LOG(LogTemp, Display, TEXT("Spirit stones collected: +%d | total %d"), GrantedAmount, CurrentGold);
	if (SaveProgress() && bQuestChanged)
	{
		BP_OnQuestStateChanged(QuestState);
	}
}

void AImmortalPlayerCharacter::UpdateStageProgress(
	const FName MapId,
	const FText& MapDisplayName,
	const int32 MaximumStage,
	const int32 Stage,
	const int32 Kills,
	const int32 RequiredKills,
	const bool bBossStage,
	const bool bMapCompleted)
{
	DisplayedMapId = MapId.IsNone() ? UImmortalMapLibrary::GetQingyunMountainId() : MapId;
	DisplayedMapName = MapDisplayName.IsEmpty() ? FText::FromName(DisplayedMapId) : MapDisplayName;
	DisplayedMapMaximumStage = FMath::Clamp(MaximumStage, 1, 999);
	DisplayedStage = FMath::Clamp(Stage, 1, DisplayedMapMaximumStage);
	DisplayedStageKills = FMath::Max(Kills, 0);
	DisplayedStageRequiredKills = FMath::Max(RequiredKills, 1);
	bDisplayedBossStage = bBossStage;
	bDisplayedMapCompleted = bMapCompleted;
	CachedMapSystemState.ActiveMapId = DisplayedMapId;
	FImmortalMapProgress CachedProgress;
	CachedProgress.MapId = DisplayedMapId;
	CachedProgress.Stage = DisplayedStage;
	CachedProgress.StageKills = DisplayedStageKills;
	CachedProgress.bCompleted = bDisplayedMapCompleted;
	UImmortalMapLibrary::SetMapProgress(CachedMapSystemState, CachedProgress);
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->SetStageProgress(
			DisplayedMapName,
			DisplayedMapMaximumStage,
			DisplayedStage,
			DisplayedStageKills,
			DisplayedStageRequiredKills,
			bDisplayedBossStage,
			bDisplayedMapCompleted);
	}
}

void AImmortalPlayerCharacter::ShowBossMessage(const FText& Message, const FLinearColor& Color)
{
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->ShowBossAnnouncement(Message, Color);
	}
}

void AImmortalPlayerCharacter::UpdateWorldBossProgress(
	const FImmortalWorldBossRuntimeSnapshot& Snapshot)
{
	if (CombatFeedbackWidget && Snapshot.bActive)
	{
		CombatFeedbackWidget->SetWorldBossProgress(
			Snapshot.DisplayName,
			Snapshot.Phase,
			Snapshot.CurrentHealth,
			Snapshot.MaximumHealth,
			Snapshot.RemainingSeconds);
	}
}

void AImmortalPlayerCharacter::UpdateEndlessDungeonProgress(
	const FImmortalEndlessDungeonRuntimeSnapshot& Snapshot)
{
	if (CombatFeedbackWidget && Snapshot.bActive)
	{
		CombatFeedbackWidget->SetEndlessDungeonProgress(
			Snapshot.Floor,
			Snapshot.Kills,
			Snapshot.RequiredKills,
			Snapshot.bElite,
			Snapshot.bBoss,
			Snapshot.BossPhase,
			Snapshot.CurrentHealth,
			Snapshot.MaximumHealth);
	}
}

void AImmortalPlayerCharacter::ShowRewardFeedback(const FVector& WorldLocation, const int32 Cultivation, const int32 SpiritStones)
{
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->ShowRewards(WorldLocation, Cultivation, SpiritStones);
	}
}

void AImmortalPlayerCharacter::HandleCultivationProgressChanged(
	const int32 NewCultivation,
	const int32 RequiredCultivation,
	const FText FullRealmName)
{
	CurrentCultivation = FMath::Max(NewCultivation, 0);
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->SetCultivationProgress(
			FullRealmName,
			CurrentCultivation,
			FMath::Max(RequiredCultivation, 1),
			GetCultivationPerSecond(),
			CultivationComponent && CultivationComponent->HasReachedAscension());
	}
}

void AImmortalPlayerCharacter::HandleCultivationBreakthrough(
	const FText PreviousRealmName,
	const FText NewRealmName,
	const EImmortalCultivationRealm NewRealm,
	const int32 NewMinorStage)
{
	CurrentCultivation = CultivationComponent ? CultivationComponent->GetCurrentCultivation() : 0;
	const bool bQuestChanged = RecordQuestProgressWithoutSave(
		EImmortalQuestMetric::CultivationBreakthroughs,
		1);
	CurrentHealth = GetMaxHealth();
	CurrentMana = GetMaxMana();
	if (CombatFeedbackWidget && CultivationComponent)
	{
		CombatFeedbackWidget->ShowCultivationBreakthrough(
			NewRealmName,
			CultivationComponent->GetHealthBonus(),
			CultivationComponent->GetManaBonus(),
			CultivationComponent->GetAttackBonus(),
			CultivationComponent->GetDefenseBonus());
	}
	BP_OnCultivationBreakthrough(PreviousRealmName, NewRealmName, NewRealm, NewMinorStage);
	if (bQuestChanged)
	{
		BP_OnQuestStateChanged(QuestState);
	}
	if (bDeathCultivationRecoveryRequired)
	{
		UnlockDeathCultivationRecovery(
			TEXT("cultivation breakthrough"));
	}
	// A high cultivation rate can cross several stages inside one AddCultivation call.
	// Defer and coalesce the save so only the final valid stage/progress pair is written.
	GetWorldTimerManager().SetTimer(
		CultivationBreakthroughSaveTimerHandle,
		this,
		&AImmortalPlayerCharacter::AutosaveCultivationProgress,
		0.05f,
		false);
}

void AImmortalPlayerCharacter::RefreshCultivationHud() const
{
	if (CombatFeedbackWidget && CultivationComponent)
	{
		CombatFeedbackWidget->SetCultivationProgress(
			CultivationComponent->GetFullRealmName(),
			CultivationComponent->GetCurrentCultivation(),
			CultivationComponent->GetRequiredCultivation(),
			CultivationComponent->GetCultivationPerSecond(),
			CultivationComponent->HasReachedAscension());
	}
}

void AImmortalPlayerCharacter::AutosaveCultivationProgress()
{
	SaveProgress();
}

void AImmortalPlayerCharacter::ReceiveEquipmentDrop(const int32 Amount)
{
	const int32 SafeAmount = FMath::Max(Amount, 0);
	if (SafeAmount <= 0)
	{
		return;
	}

	EquipmentDropCount += SafeAmount;
	BP_OnEquipmentPickedUp(EquipmentDropCount, SafeAmount);
	SaveProgress();
}

bool AImmortalPlayerCharacter::ReceiveEquipmentItem(const FImmortalEquipmentItem& Item)
{
	return ProcessEquipmentItem(Item, true, true);
}

bool AImmortalPlayerCharacter::IsEquipmentLocked(const FGuid ItemId) const
{
	FImmortalEquipmentItem Item;
	bool bEquipped = false;
	return GetEquipmentItemById(ItemId, Item, bEquipped) && Item.bLocked;
}

FImmortalInventoryOperationResult AImmortalPlayerCharacter::SetEquipmentLocked(
	const FGuid ItemId,
	const bool bLocked)
{
	FImmortalInventoryOperationResult Result;
	bool bEquipped = false;
	FImmortalEquipmentItem* Item = FindMutableEquipmentItem(ItemId, bEquipped);
	if (!Item)
	{
		Result.Message = FText::FromString(TEXT("未找到需要锁定的装备"));
		return Result;
	}
	if (Item->bLocked == bLocked)
	{
		Result.bSucceeded = true;
		Result.Message = FText::FromString(bLocked ? TEXT("装备已经锁定") : TEXT("装备已经解锁"));
		return Result;
	}

	const int32 PreviousRevision = EquipmentInventoryRevision;
	Item->bLocked = bLocked;
	++EquipmentInventoryRevision;
	if (ShouldForceInventoryPersistenceFailure(TEXT("Lock")) || !SaveProgress())
	{
		Item->bLocked = !bLocked;
		EquipmentInventoryRevision = PreviousRevision;
		Result.bPersistenceFailed = true;
		Result.Message = FText::FromString(TEXT("存档写入失败，装备锁定状态已回滚"));
		return Result;
	}

	Result.bSucceeded = true;
	Result.AffectedItemCount = 1;
	Result.Message = FText::FromString(bLocked ? TEXT("装备已锁定，不会被出售、分解或自动替换") : TEXT("装备已解锁"));
	BP_OnInventoryChanged(InventoryItems.Num(), GetInventoryCapacity());
	if (bEquipped) BP_OnEquipmentChanged(Item->Slot, *Item, false, GetCombatPower());
	BP_OnInventoryOperation(Result);
	UE_LOG(LogTemp, Display, TEXT("Inventory equipment lock changed: %s | locked=%s | equipped=%s | revision=%d"),
		*ItemId.ToString(), bLocked ? TEXT("true") : TEXT("false"), bEquipped ? TEXT("true") : TEXT("false"),
		EquipmentInventoryRevision);
	return Result;
}

bool AImmortalPlayerCharacter::IsArtifactLocked(const FGuid InstanceId) const
{
	const FImmortalArtifactItem* Item = ArtifactInventory.FindByPredicate([InstanceId](const FImmortalArtifactItem& Entry)
	{
		return Entry.InstanceId == InstanceId;
	});
	return Item && Item->bLocked;
}

FImmortalInventoryOperationResult AImmortalPlayerCharacter::SetArtifactLocked(
	const FGuid InstanceId,
	const bool bLocked)
{
	FImmortalInventoryOperationResult Result;
	FImmortalArtifactItem* Item = FindMutableArtifact(InstanceId);
	if (!Item)
	{
		Result.Message = FText::FromString(TEXT("未找到需要锁定的法宝"));
		return Result;
	}
	if (Item->bLocked == bLocked)
	{
		Result.bSucceeded = true;
		Result.Message = FText::FromString(bLocked ? TEXT("法宝已经锁定") : TEXT("法宝已经解锁"));
		return Result;
	}

	const int32 PreviousRevision = ArtifactInventoryRevision;
	Item->bLocked = bLocked;
	++ArtifactInventoryRevision;
	if (ShouldForceInventoryPersistenceFailure(TEXT("Lock")) || !SaveProgress())
	{
		Item->bLocked = !bLocked;
		ArtifactInventoryRevision = PreviousRevision;
		Result.bPersistenceFailed = true;
		Result.Message = FText::FromString(TEXT("存档写入失败，法宝锁定状态已回滚"));
		return Result;
	}

	Result.bSucceeded = true;
	Result.AffectedItemCount = 1;
	Result.Message = FText::FromString(bLocked ? TEXT("法宝已锁定") : TEXT("法宝已解锁"));
	BP_OnArtifactChanged(*Item, Item->InstanceId == EquippedArtifactInstanceId);
	BP_OnInventoryOperation(Result);
	UE_LOG(LogTemp, Display, TEXT("Inventory artifact lock changed: %s | locked=%s | equipped=%s | revision=%d"),
		*InstanceId.ToString(), bLocked ? TEXT("true") : TEXT("false"),
		InstanceId == EquippedArtifactInstanceId ? TEXT("true") : TEXT("false"), ArtifactInventoryRevision);
	return Result;
}

FImmortalInventoryOperationResult AImmortalPlayerCharacter::OrganizeInventory()
{
	FImmortalInventoryOperationResult Result;
	const TArray<FImmortalEquipmentItem> PreviousEquipment = InventoryItems;
	const TArray<FImmortalMaterialStack> PreviousMaterials = MaterialInventory;
	const TArray<FImmortalPillStack> PreviousPills = PillInventory;
	const TArray<FImmortalArtifactItem> PreviousArtifacts = ArtifactInventory;
	const TArray<FImmortalQuestItemStack> PreviousQuestItems = QuestItemInventory;
	const int32 PreviousEquipmentRevision = EquipmentInventoryRevision;
	const int32 PreviousMaterialRevision = MaterialInventoryRevision;
	const int32 PreviousPillRevision = PillInventoryRevision;
	const int32 PreviousArtifactRevision = ArtifactInventoryRevision;
	const int32 PreviousQuestRevision = QuestItemInventoryRevision;

	UImmortalInventoryLibrary::SortEquipmentInventory(InventoryItems);
	UImmortalMaterialLibrary::NormalizeInventory(MaterialInventory);
	UImmortalAlchemyLibrary::NormalizePillInventory(PillInventory);
	UImmortalInventoryLibrary::SortArtifactInventory(ArtifactInventory, EquippedArtifactInstanceId);
	UImmortalInventoryLibrary::NormalizeQuestItemInventory(QuestItemInventory);
	const bool bEquipmentChanged = !HaveSameEquipmentOrder(PreviousEquipment, InventoryItems);
	const bool bMaterialsChanged = !HaveSameMaterialOrder(PreviousMaterials, MaterialInventory);
	const bool bPillsChanged = !HaveSamePillOrder(PreviousPills, PillInventory);
	const bool bArtifactsChanged = !HaveSameArtifactOrder(PreviousArtifacts, ArtifactInventory);
	const bool bQuestItemsChanged = !HaveSameQuestItemOrder(PreviousQuestItems, QuestItemInventory);
	if (!bEquipmentChanged && !bMaterialsChanged && !bPillsChanged && !bArtifactsChanged && !bQuestItemsChanged)
	{
		Result.bSucceeded = true;
		Result.Message = FText::FromString(TEXT("背包已经整理完毕"));
		return Result;
	}

	if (bEquipmentChanged) ++EquipmentInventoryRevision;
	if (bMaterialsChanged) ++MaterialInventoryRevision;
	if (bPillsChanged) ++PillInventoryRevision;
	if (bArtifactsChanged) ++ArtifactInventoryRevision;
	if (bQuestItemsChanged) ++QuestItemInventoryRevision;
	if (ShouldForceInventoryPersistenceFailure(TEXT("Sort")) || !SaveProgress())
	{
		InventoryItems = PreviousEquipment;
		MaterialInventory = PreviousMaterials;
		PillInventory = PreviousPills;
		ArtifactInventory = PreviousArtifacts;
		QuestItemInventory = PreviousQuestItems;
		EquipmentInventoryRevision = PreviousEquipmentRevision;
		MaterialInventoryRevision = PreviousMaterialRevision;
		PillInventoryRevision = PreviousPillRevision;
		ArtifactInventoryRevision = PreviousArtifactRevision;
		QuestItemInventoryRevision = PreviousQuestRevision;
		Result.bPersistenceFailed = true;
		Result.Message = FText::FromString(TEXT("存档写入失败，背包顺序已回滚"));
		return Result;
	}

	Result.bSucceeded = true;
	Result.AffectedItemCount =
		(bEquipmentChanged ? InventoryItems.Num() : 0)
		+ (bMaterialsChanged ? MaterialInventory.Num() : 0)
		+ (bPillsChanged ? PillInventory.Num() : 0)
		+ (bArtifactsChanged ? ArtifactInventory.Num() : 0)
		+ (bQuestItemsChanged ? QuestItemInventory.Num() : 0);
	Result.Message = FText::FromString(TEXT("背包已按锁定、类型、品质和等级整理"));
	BP_OnInventoryChanged(InventoryItems.Num(), GetInventoryCapacity());
	BP_OnInventoryOperation(Result);
	UE_LOG(LogTemp, Display,
		TEXT("Inventory organized: equipment=%s materials=%s pills=%s artifacts=%s quest=%s | affected=%d"),
		bEquipmentChanged ? TEXT("true") : TEXT("false"), bMaterialsChanged ? TEXT("true") : TEXT("false"),
		bPillsChanged ? TEXT("true") : TEXT("false"), bArtifactsChanged ? TEXT("true") : TEXT("false"),
		bQuestItemsChanged ? TEXT("true") : TEXT("false"), Result.AffectedItemCount);
	return Result;
}

int32 AImmortalPlayerCharacter::GetBulkEquipmentCount(const EImmortalEquipmentQuality MaximumQuality) const
{
	return UImmortalInventoryLibrary::GetBulkEligibleEquipmentCount(InventoryItems, MaximumQuality);
}

int32 AImmortalPlayerCharacter::GetBulkEquipmentSellValue(const EImmortalEquipmentQuality MaximumQuality) const
{
	return UImmortalInventoryLibrary::GetBulkEquipmentSellValue(InventoryItems, MaximumQuality);
}

TArray<FImmortalMaterialStack> AImmortalPlayerCharacter::GetBulkEquipmentDismantleYield(
	const EImmortalEquipmentQuality MaximumQuality) const
{
	return UImmortalInventoryLibrary::GetBulkEquipmentDismantleYield(InventoryItems, MaximumQuality);
}

FImmortalInventoryOperationResult AImmortalPlayerCharacter::BatchSellEquipment(
	const EImmortalEquipmentQuality MaximumQuality)
{
	FImmortalInventoryOperationResult Result;
	int64 TotalPrice = 0;
	for (const FImmortalEquipmentItem& Item : InventoryItems)
	{
		const bool bWithinThreshold = Item.IsValid()
			&& static_cast<int32>(Item.Quality) <= static_cast<int32>(MaximumQuality);
		if (bWithinThreshold && Item.bLocked) ++Result.SkippedLockedItemCount;
		if (UImmortalInventoryLibrary::IsEligibleForBulkAction(Item, MaximumQuality))
		{
			TotalPrice += UImmortalShopLibrary::GetEquipmentSellPrice(Item);
			++Result.AffectedItemCount;
		}
	}
	if (Result.AffectedItemCount <= 0 || TotalPrice <= 0)
	{
		Result.AffectedItemCount = 0;
		Result.Message = FText::FromString(Result.SkippedLockedItemCount > 0
			? TEXT("符合品质的装备都已锁定，没有出售任何物品")
			: TEXT("没有符合当前品质条件的可出售装备"));
		return Result;
	}
	if (!UImmortalInventoryLibrary::CanReceiveSpiritStones(CurrentGold, TotalPrice))
	{
		Result.AffectedItemCount = 0;
		Result.Message = FText::FromString(TEXT("灵石接近上限，无法完整接收出售所得；本次未出售"));
		return Result;
	}

	const TArray<FImmortalEquipmentItem> PreviousInventory = InventoryItems;
	const int32 PreviousGold = CurrentGold;
	const int32 PreviousRevision = EquipmentInventoryRevision;
	InventoryItems.RemoveAll([MaximumQuality](const FImmortalEquipmentItem& Item)
	{
		return UImmortalInventoryLibrary::IsEligibleForBulkAction(Item, MaximumQuality);
	});
	CurrentGold += static_cast<int32>(TotalPrice);
	++EquipmentInventoryRevision;
	if (ShouldForceInventoryPersistenceFailure(TEXT("Sell")) || !SaveProgress())
	{
		InventoryItems = PreviousInventory;
		CurrentGold = PreviousGold;
		EquipmentInventoryRevision = PreviousRevision;
		Result.bPersistenceFailed = true;
		Result.SpiritStoneDelta = 0;
		Result.AffectedItemCount = 0;
		Result.Message = FText::FromString(TEXT("存档写入失败，批量出售已完整回滚"));
		return Result;
	}

	Result.bSucceeded = true;
	Result.SpiritStoneDelta = static_cast<int32>(TotalPrice);
	Result.Message = FText::FromString(FString::Printf(TEXT("已批量出售 %d 件装备，获得灵石 %d；跳过锁定 %d 件"),
		Result.AffectedItemCount, Result.SpiritStoneDelta, Result.SkippedLockedItemCount));
	BP_OnInventoryChanged(InventoryItems.Num(), GetInventoryCapacity());
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, Result.SpiritStoneDelta);
	BP_OnInventoryOperation(Result);
	UE_LOG(LogTemp, Display, TEXT("Inventory batch sale succeeded: count=%d lockedSkipped=%d stones=+%d => %d backpack=%d"),
		Result.AffectedItemCount, Result.SkippedLockedItemCount, Result.SpiritStoneDelta, CurrentGold, InventoryItems.Num());
	return Result;
}

FImmortalInventoryOperationResult AImmortalPlayerCharacter::DismantleEquipment(const FGuid ItemId)
{
	return DismantleEquipmentInternal(ItemId, EImmortalEquipmentQuality::Divine, false);
}

FImmortalInventoryOperationResult AImmortalPlayerCharacter::BatchDismantleEquipment(
	const EImmortalEquipmentQuality MaximumQuality)
{
	return DismantleEquipmentInternal(FGuid(), MaximumQuality, true);
}

FImmortalInventoryOperationResult AImmortalPlayerCharacter::DismantleEquipmentInternal(
	const FGuid ItemId,
	const EImmortalEquipmentQuality MaximumQuality,
	const bool bBatch)
{
	FImmortalInventoryOperationResult Result;
	TSet<FGuid> RemovedIds;
	TArray<FImmortalMaterialStack> Rewards;
	for (const FImmortalEquipmentItem& Item : InventoryItems)
	{
		const bool bSelected = bBatch
			? UImmortalInventoryLibrary::IsEligibleForBulkAction(Item, MaximumQuality)
			: Item.ItemId == ItemId && Item.IsValid() && !Item.bLocked;
		if (bBatch && Item.IsValid()
			&& static_cast<int32>(Item.Quality) <= static_cast<int32>(MaximumQuality)
			&& Item.bLocked)
		{
			++Result.SkippedLockedItemCount;
		}
		if (!bSelected) continue;
		RemovedIds.Add(Item.ItemId);
		for (const FImmortalMaterialStack& Stack : UImmortalInventoryLibrary::GetEquipmentDismantleYield(Item))
		{
			UImmortalMaterialLibrary::AddMaterialStack(Rewards, Stack.MaterialId, Stack.Quantity);
		}
	}
	if (!bBatch && RemovedIds.IsEmpty())
	{
		const FImmortalEquipmentItem* Locked = InventoryItems.FindByPredicate([ItemId](const FImmortalEquipmentItem& Item)
		{
			return Item.ItemId == ItemId;
		});
		const bool bEquipped = EquippedItems.ContainsByPredicate([ItemId](const FImmortalEquipmentItem& Item)
		{
			return Item.ItemId == ItemId;
		});
		Result.Message = FText::FromString(Locked && Locked->bLocked
			? TEXT("该装备已锁定，请先解锁")
			: (bEquipped ? TEXT("已装备物品不能分解，请先更换装备") : TEXT("未在背包中找到该装备")));
		return Result;
	}
	if (bBatch && RemovedIds.IsEmpty())
	{
		Result.Message = FText::FromString(Result.SkippedLockedItemCount > 0
			? TEXT("符合品质的装备都已锁定，没有分解任何物品")
			: TEXT("没有符合当前品质条件的可分解装备"));
		return Result;
	}

	TArray<FImmortalMaterialStack> CandidateMaterials = MaterialInventory;
	if (!UImmortalInventoryLibrary::TryAddMaterialRewards(CandidateMaterials, Rewards))
	{
		Result.Message = FText::FromString(TEXT("分解材料堆叠空间不足，本次没有消耗装备"));
		return Result;
	}

	const TArray<FImmortalEquipmentItem> PreviousInventory = InventoryItems;
	const TArray<FImmortalMaterialStack> PreviousMaterials = MaterialInventory;
	const int32 PreviousEquipmentRevision = EquipmentInventoryRevision;
	const int32 PreviousMaterialRevision = MaterialInventoryRevision;
	InventoryItems.RemoveAll([&RemovedIds](const FImmortalEquipmentItem& Item)
	{
		return RemovedIds.Contains(Item.ItemId);
	});
	MaterialInventory = MoveTemp(CandidateMaterials);
	++EquipmentInventoryRevision;
	++MaterialInventoryRevision;
	if (ShouldForceInventoryPersistenceFailure(TEXT("Salvage")) || !SaveProgress())
	{
		InventoryItems = PreviousInventory;
		MaterialInventory = PreviousMaterials;
		EquipmentInventoryRevision = PreviousEquipmentRevision;
		MaterialInventoryRevision = PreviousMaterialRevision;
		Result.bPersistenceFailed = true;
		Result.Message = FText::FromString(TEXT("存档写入失败，装备与分解材料已完整回滚"));
		return Result;
	}

	Result.bSucceeded = true;
	Result.AffectedItemCount = RemovedIds.Num();
	Result.MaterialRewards = Rewards;
	Result.Message = FText::FromString(FString::Printf(TEXT("已分解 %d 件装备：%s；跳过锁定 %d 件"),
		Result.AffectedItemCount, *FormatInventoryMaterials(Rewards), Result.SkippedLockedItemCount));
	BP_OnInventoryChanged(InventoryItems.Num(), GetInventoryCapacity());
	PublishMaterialInventoryDiff(PreviousMaterials);
	BP_OnInventoryOperation(Result);
	UE_LOG(LogTemp, Display, TEXT("Inventory dismantle succeeded: batch=%s count=%d lockedSkipped=%d rewards=%s backpack=%d"),
		bBatch ? TEXT("true") : TEXT("false"), Result.AffectedItemCount, Result.SkippedLockedItemCount,
		*FormatInventoryMaterials(Rewards), InventoryItems.Num());
	return Result;
}

int32 AImmortalPlayerCharacter::GetQuestItemQuantity(const FName QuestItemId) const
{
	return UImmortalInventoryLibrary::GetQuestItemQuantity(QuestItemInventory, QuestItemId);
}

int32 AImmortalPlayerCharacter::AddQuestItemInternal(const FName QuestItemId, const int32 Amount)
{
	const int32 Added = UImmortalInventoryLibrary::AddQuestItemStack(QuestItemInventory, QuestItemId, Amount);
	if (Added > 0) ++QuestItemInventoryRevision;
	return Added;
}

int32 AImmortalPlayerCharacter::ReceiveQuestItem(const FName QuestItemId, const int32 Amount)
{
	const TArray<FImmortalQuestItemStack> PreviousInventory = QuestItemInventory;
	const int32 PreviousRevision = QuestItemInventoryRevision;
	const int32 Added = AddQuestItemInternal(QuestItemId, Amount);
	if (Added <= 0) return 0;
	if (ShouldForceInventoryPersistenceFailure(TEXT("Quest")) || !SaveProgress())
	{
		QuestItemInventory = PreviousInventory;
		QuestItemInventoryRevision = PreviousRevision;
		return 0;
	}
	const int32 NewQuantity = GetQuestItemQuantity(QuestItemId);
	BP_OnQuestItemInventoryChanged(QuestItemId, NewQuantity, Added);
	UE_LOG(LogTemp, Display, TEXT("Quest item received: %s x%d => %d | task types=%d"),
		*QuestItemId.ToString(), Added, NewQuantity, QuestItemInventory.Num());
	return Added;
}

int32 AImmortalPlayerCharacter::GetMaterialQuantity(const FName MaterialId) const
{
	return UImmortalMaterialLibrary::GetMaterialQuantity(MaterialInventory, MaterialId);
}

int32 AImmortalPlayerCharacter::AddMaterialInternal(const FName MaterialId, const int32 Amount)
{
	const int32 Added = UImmortalMaterialLibrary::AddMaterialStack(MaterialInventory, MaterialId, Amount);
	if (Added > 0)
	{
		++MaterialInventoryRevision;
	}
	return Added;
}

void AImmortalPlayerCharacter::PublishMaterialInventoryDiff(
	const TArray<FImmortalMaterialStack>& PreviousInventory)
{
	TSet<FName> MaterialIds;
	for (const FImmortalMaterialStack& Stack : PreviousInventory)
	{
		if (!Stack.MaterialId.IsNone()) MaterialIds.Add(Stack.MaterialId);
	}
	for (const FImmortalMaterialStack& Stack : MaterialInventory)
	{
		if (!Stack.MaterialId.IsNone()) MaterialIds.Add(Stack.MaterialId);
	}
	for (const FName MaterialId : MaterialIds)
	{
		const int32 PreviousQuantity = UImmortalMaterialLibrary::GetMaterialQuantity(PreviousInventory, MaterialId);
		const int32 NewQuantity = GetMaterialQuantity(MaterialId);
		if (NewQuantity != PreviousQuantity)
		{
			BP_OnMaterialInventoryChanged(MaterialId, NewQuantity, NewQuantity - PreviousQuantity);
		}
	}
}

int32 AImmortalPlayerCharacter::ReceiveMaterial(
	const FName MaterialId,
	const int32 Amount,
	const FVector PickupWorldLocation)
{
	const int32 Added = AddMaterialInternal(MaterialId, Amount);
	if (Added <= 0)
	{
		return 0;
	}

	FImmortalMaterialDefinition Definition;
	UImmortalMaterialLibrary::GetMaterialDefinition(MaterialId, Definition);
	const int32 NewQuantity = GetMaterialQuantity(MaterialId);
	BP_OnMaterialInventoryChanged(MaterialId, NewQuantity, Added);
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->ShowMaterialPickup(Definition.DisplayName, Definition.DisplayColor, Added);
		ShowRewardFeedback(PickupWorldLocation, 0, 0);
	}
	QueueManagementNotification(
		FText::FromString(FString::Printf(
			TEXT("%s x%d\uFF08\u5DF2\u81EA\u52A8\u62FE\u53D6\uFF09"),
			*Definition.DisplayName.ToString(),
			Added)),
		Definition.DisplayColor,
		5.0f);
	SaveProgress();
	UE_LOG(LogTemp, Display, TEXT("Material collected: %s x%d | total %d | material types %d"),
		*Definition.DisplayName.ToString(), Added, NewQuantity, MaterialInventory.Num());
	return Added;
}

int32 AImmortalPlayerCharacter::GetPillQuantity(
	const FName PillId,
	const EImmortalPillQuality Quality) const
{
	return UImmortalAlchemyLibrary::GetPillQuantity(PillInventory, PillId, Quality);
}

bool AImmortalPlayerCharacter::IsAlchemyRecipeUnlocked(const FName RecipeId) const
{
	FImmortalPillDefinition Definition;
	return CultivationComponent
		&& UImmortalAlchemyLibrary::GetPillDefinition(RecipeId, Definition)
		&& UImmortalAlchemyLibrary::IsRecipeUnlocked(
			Definition,
			static_cast<int32>(CultivationComponent->GetCurrentRealm()),
			CultivationComponent->GetCurrentMinorStage());
}

bool AImmortalPlayerCharacter::CanCraftPill(const FName RecipeId) const
{
	FImmortalPillDefinition Definition;
	return IsAlchemyRecipeUnlocked(RecipeId)
		&& UImmortalAlchemyLibrary::GetPillDefinition(RecipeId, Definition)
		&& UImmortalAlchemyLibrary::CanCraft(MaterialInventory, Definition);
}

int32 AImmortalPlayerCharacter::AddPillInternal(
	const FName PillId,
	const EImmortalPillQuality Quality,
	const int32 Amount)
{
	const int32 Added = UImmortalAlchemyLibrary::AddPillStack(PillInventory, PillId, Quality, Amount);
	if (Added > 0) ++PillInventoryRevision;
	return Added;
}

FImmortalAlchemyCraftResult AImmortalPlayerCharacter::CraftPill(const FName RecipeId)
{
	return CraftPillInternal(RecipeId, TOptional<float>());
}

FImmortalAlchemyCraftResult AImmortalPlayerCharacter::CraftPillInternal(
	const FName RecipeId,
	const TOptional<float> ForcedRoll)
{
	FImmortalAlchemyCraftResult Result;
	Result.RecipeId = RecipeId;
	FImmortalPillDefinition Definition;
	Result.bRecipeFound = UImmortalAlchemyLibrary::GetPillDefinition(RecipeId, Definition);
	if (!Result.bRecipeFound)
	{
		Result.Message = FText::FromString(TEXT("未找到丹方"));
		return Result;
	}

	Result.bUnlocked = IsAlchemyRecipeUnlocked(RecipeId);
	if (!Result.bUnlocked)
	{
		Result.Message = FText::FromString(TEXT("当前境界尚未解锁此丹方"));
		return Result;
	}
	Result.bHadIngredients = UImmortalAlchemyLibrary::CanCraft(MaterialInventory, Definition);
	if (!Result.bHadIngredients)
	{
		Result.Message = FText::FromString(TEXT("炼丹材料不足"));
		return Result;
	}

	Result.bMaterialsConsumed = UImmortalAlchemyLibrary::ConsumeIngredients(MaterialInventory, Definition);
	if (!Result.bMaterialsConsumed)
	{
		Result.Message = FText::FromString(TEXT("材料扣除失败，炼制已取消"));
		return Result;
	}
	++MaterialInventoryRevision;

	const float Roll = ForcedRoll.IsSet() ? ForcedRoll.GetValue() : FMath::FRand();
	Result.Outcome = UImmortalAlchemyLibrary::CalculateOutcome(
		Definition,
		Roll,
		GetCaveAlchemySuccessBonus(),
		GetCaveAlchemyExceptionalBonus());
	if (Result.Outcome == EImmortalAlchemyOutcome::Failure)
	{
		Result.Message = FText::FromString(FString::Printf(
			TEXT("%s炼制失败，材料化为药渣"), *Definition.DisplayName.ToString()));
	}
	else
	{
		const EImmortalPillQuality Quality = Result.Outcome == EImmortalAlchemyOutcome::Exceptional
			? EImmortalPillQuality::Exceptional
			: EImmortalPillQuality::Ordinary;
		Result.PillQuantityGranted = AddPillInternal(RecipeId, Quality, 1);
		Result.Message = FText::FromString(FString::Printf(
			TEXT("炼制成功：%s·%s ×%d"),
			*UImmortalAlchemyLibrary::GetQualityText(Quality).ToString(),
			*Definition.DisplayName.ToString(),
			Result.PillQuantityGranted));
	}

	const bool bQuestChanged = RecordQuestProgressWithoutSave(
		EImmortalQuestMetric::AlchemyCrafts,
		1);
	if (SaveProgress() && bQuestChanged)
	{
		BP_OnQuestStateChanged(QuestState);
	}
	BP_OnAlchemyCompleted(RecipeId, Result.Outcome, Result.Message);
	UE_LOG(LogTemp, Display, TEXT("Alchemy completed: recipe %s | roll %.4f | outcome %d | consumed=true | pills +%d | material types %d"),
		*RecipeId.ToString(), Roll, static_cast<int32>(Result.Outcome), Result.PillQuantityGranted, MaterialInventory.Num());
	return Result;
}

bool AImmortalPlayerCharacter::CanUsePill(
	const FName PillId,
	const EImmortalPillQuality Quality) const
{
	if (GetPillQuantity(PillId, Quality) <= 0) return false;
	FImmortalPillDefinition Definition;
	if (!UImmortalAlchemyLibrary::GetPillDefinition(PillId, Definition)) return false;
	switch (Definition.Effect)
	{
	case EImmortalPillEffect::RestoreHealth:
		return !bDead && CurrentHealth < GetMaxHealth() - KINDA_SMALL_NUMBER;
	case EImmortalPillEffect::GrantCultivationPercent:
	case EImmortalPillEffect::CompleteCurrentStage:
		return CultivationComponent && !CultivationComponent->HasReachedAscension();
	case EImmortalPillEffect::CultivationRateBoost:
		return CultivationComponent && !CultivationComponent->HasReachedAscension();
	default:
		return false;
	}
}

bool AImmortalPlayerCharacter::UsePill(
	const FName PillId,
	const EImmortalPillQuality Quality)
{
	FImmortalPillDefinition Definition;
	if (!CanUsePill(PillId, Quality)
		|| !UImmortalAlchemyLibrary::GetPillDefinition(PillId, Definition)
		|| !UImmortalAlchemyLibrary::RemovePill(PillInventory, PillId, Quality, 1))
	{
		return false;
	}
	++PillInventoryRevision;

	const bool bExceptional = Quality == EImmortalPillQuality::Exceptional;
	const float BaseMagnitude = bExceptional ? Definition.ExceptionalMagnitude : Definition.OrdinaryMagnitude;
	const float PillMultiplier = GetPillEffectMultiplier();
	const float Magnitude = UImmortalCharacterPathLibrary::CalculateEffectivePillMagnitude(
		SpiritRootState, BaseMagnitude, Definition.Effect == EImmortalPillEffect::CultivationRateBoost);
	const float Duration = bExceptional ? Definition.ExceptionalDurationSeconds : Definition.OrdinaryDurationSeconds;
	switch (Definition.Effect)
	{
	case EImmortalPillEffect::RestoreHealth:
		CurrentHealth = FMath::Clamp(CurrentHealth + GetMaxHealth() * Magnitude, 0.0f, GetMaxHealth());
		break;
	case EImmortalPillEffect::GrantCultivationPercent:
		CultivationComponent->AddCultivation(FMath::Max(FMath::RoundToInt(CultivationComponent->GetRequiredCultivation() * Magnitude), 1));
		CurrentCultivation = CultivationComponent->GetCurrentCultivation();
		break;
	case EImmortalPillEffect::CultivationRateBoost:
		ApplyAlchemyCultivationBoost(Magnitude, Duration);
		break;
	case EImmortalPillEffect::CompleteCurrentStage:
	{
		const int32 Needed = FMath::Max(
			CultivationComponent->GetRequiredCultivation() - CultivationComponent->GetCurrentCultivation(), 1);
		CultivationComponent->AddCultivation(Needed);
		if (bExceptional && !CultivationComponent->HasReachedAscension() && Magnitude > 0.0f)
		{
			CultivationComponent->AddCultivation(FMath::Max(
				FMath::RoundToInt(CultivationComponent->GetRequiredCultivation() * Magnitude), 1));
		}
		CurrentCultivation = CultivationComponent->GetCurrentCultivation();
		break;
	}
	default:
		break;
	}

	const FText EffectText = GetEffectivePillEffectText(PillId, Quality);
	SaveProgress();
	BP_OnPillUsed(PillId, Quality, EffectText);
	UE_LOG(LogTemp, Display, TEXT("Pill used: %s %s | effect %s | remaining %d"),
		*UImmortalAlchemyLibrary::GetQualityText(Quality).ToString(),
		*Definition.DisplayName.ToString(),
		*EffectText.ToString(),
		GetPillQuantity(PillId, Quality));
	return true;
}

float AImmortalPlayerCharacter::GetPillEffectMultiplier() const
{
	return UImmortalCharacterPathLibrary::CalculatePillEffectMultiplier(SpiritRootState);
}

FText AImmortalPlayerCharacter::GetEffectivePillEffectText(
	const FName PillId,
	const EImmortalPillQuality Quality) const
{
	FImmortalPillDefinition Definition;
	if (!UImmortalAlchemyLibrary::GetPillDefinition(PillId, Definition)) return FText::GetEmpty();
	const bool bExceptional = Quality == EImmortalPillQuality::Exceptional;
	const float BaseMagnitude = bExceptional ? Definition.ExceptionalMagnitude : Definition.OrdinaryMagnitude;
	const float Duration = bExceptional ? Definition.ExceptionalDurationSeconds : Definition.OrdinaryDurationSeconds;
	const float Multiplier = GetPillEffectMultiplier();
	switch (Definition.Effect)
	{
	case EImmortalPillEffect::RestoreHealth:
		return FText::FromString(FString::Printf(TEXT("恢复最大生命的 %.0f%%（灵根药效 ×%.2f）"), BaseMagnitude * Multiplier * 100.0f, Multiplier));
	case EImmortalPillEffect::GrantCultivationPercent:
		return FText::FromString(FString::Printf(TEXT("获得当前突破需求 %.0f%% 的修为（灵根药效 ×%.2f）"), BaseMagnitude * Multiplier * 100.0f, Multiplier));
	case EImmortalPillEffect::CultivationRateBoost:
		return FText::FromString(FString::Printf(TEXT("在线修炼速度 ×%.2f，持续 %.0f 秒（灵根强化增益部分）"),
			UImmortalCharacterPathLibrary::CalculateEffectivePillMagnitude(SpiritRootState, BaseMagnitude, true), Duration));
	case EImmortalPillEffect::CompleteCurrentStage:
		return bExceptional
			? FText::FromString(FString::Printf(TEXT("立即突破，并获得下一层需求 %.0f%% 的修为（灵根药效 ×%.2f）"), BaseMagnitude * Multiplier * 100.0f, Multiplier))
			: FText::FromString(TEXT("补足当前修为并立即突破"));
	default:
		return FText::GetEmpty();
	}
}

float AImmortalPlayerCharacter::GetAlchemyBoostRemainingSeconds() const
{
	return GetWorld() && AlchemyCultivationBoostMultiplier > 1.0f
		? FMath::Max(AlchemyBoostEndWorldTime - GetWorld()->GetTimeSeconds(), 0.0f)
		: 0.0f;
}

void AImmortalPlayerCharacter::ApplyAlchemyCultivationBoost(
	const float Multiplier,
	const float DurationSeconds)
{
	const float ExistingRemaining = GetAlchemyBoostRemainingSeconds();
	AlchemyCultivationBoostMultiplier = FMath::Max(
		AlchemyCultivationBoostMultiplier, FMath::Max(Multiplier, 1.0f));
	const float CombinedDuration = FMath::Clamp(ExistingRemaining + FMath::Max(DurationSeconds, 0.0f), 0.0f, 3600.0f);
	RestoreAlchemyCultivationBoost(AlchemyCultivationBoostMultiplier, CombinedDuration);
}

void AImmortalPlayerCharacter::RestoreAlchemyCultivationBoost(
	const float Multiplier,
	const float RemainingSeconds)
{
	GetWorldTimerManager().ClearTimer(AlchemyBoostTimerHandle);
	if (!GetWorld() || Multiplier <= 1.0f || RemainingSeconds <= 0.0f)
	{
		ClearAlchemyCultivationBoost();
		return;
	}
	AlchemyCultivationBoostMultiplier = FMath::Max(Multiplier, 1.0f);
	AlchemyBoostEndWorldTime = GetWorld()->GetTimeSeconds() + FMath::Min(RemainingSeconds, 3600.0f);
	if (CultivationComponent) CultivationComponent->SetAlchemyRateMultiplier(AlchemyCultivationBoostMultiplier);
	GetWorldTimerManager().SetTimer(
		AlchemyBoostTimerHandle,
		this,
		&AImmortalPlayerCharacter::ClearAlchemyCultivationBoost,
		FMath::Min(RemainingSeconds, 3600.0f),
		false);
	UE_LOG(LogTemp, Display, TEXT("Alchemy cultivation boost active: x%.2f for %.1f seconds"),
		AlchemyCultivationBoostMultiplier, GetAlchemyBoostRemainingSeconds());
}

void AImmortalPlayerCharacter::ClearAlchemyCultivationBoost()
{
	GetWorldTimerManager().ClearTimer(AlchemyBoostTimerHandle);
	AlchemyCultivationBoostMultiplier = 1.0f;
	AlchemyBoostEndWorldTime = 0.0f;
	if (CultivationComponent) CultivationComponent->SetAlchemyRateMultiplier(1.0f);
	UE_LOG(LogTemp, Display, TEXT("Alchemy cultivation boost ended"));
}

bool AImmortalPlayerCharacter::ProcessEquipmentItem(
	const FImmortalEquipmentItem& Item,
	const bool bShowFeedback,
	const bool bSaveAfter,
	const bool bNotifyChanges)
{
	bLastEquipmentReceivePersistenceFailure = false;
	if (!Item.IsValid())
	{
		return false;
	}
	TArray<FImmortalEquipmentItem> PreviousInventory;
	TArray<FImmortalEquipmentItem> PreviousEquipped;
	int32 PreviousDropCount = EquipmentDropCount;
	int32 PreviousRevision = EquipmentInventoryRevision;
	FImmortalQuestState PreviousQuestState = QuestState;
	float PreviousHealth = CurrentHealth;
	float PreviousMana = CurrentMana;
	if (bSaveAfter)
	{
		PreviousInventory = InventoryItems;
		PreviousEquipped = EquippedItems;
	}
	auto RollbackAcquisition = [this, bSaveAfter, &PreviousInventory, &PreviousEquipped,
		PreviousDropCount, PreviousRevision, PreviousQuestState,
		PreviousHealth, PreviousMana]
	{
		if (!bSaveAfter) return;
		InventoryItems = PreviousInventory;
		EquippedItems = PreviousEquipped;
		EquipmentDropCount = PreviousDropCount;
		EquipmentInventoryRevision = PreviousRevision;
		QuestState = PreviousQuestState;
		RecalculateEquipmentBonuses();
		CurrentHealth = FMath::Clamp(PreviousHealth, 0.0f, GetMaxHealth());
		CurrentMana = FMath::Clamp(PreviousMana, 0.0f, GetMaxMana());
	};
	auto FinalizeAcquisition = [this, bSaveAfter, bNotifyChanges, &RollbackAcquisition]()
	{
		++EquipmentDropCount;
		const bool bQuestChanged = bSaveAfter
			&& RecordQuestProgressWithoutSave(
				EImmortalQuestMetric::EquipmentPickups, 1);
		if (bSaveAfter && !SaveProgress())
		{
			bLastEquipmentReceivePersistenceFailure = true;
			RollbackAcquisition();
			UE_LOG(LogTemp, Error, TEXT("Equipment acquisition rolled back because the save failed"));
			return false;
		}
		if (bNotifyChanges) BP_OnEquipmentPickedUp(EquipmentDropCount, 1);
		if (bQuestChanged) BP_OnQuestStateChanged(QuestState);
		return true;
	};

	const float NewPower = UImmortalEquipmentLibrary::CalculateEquipmentPower(Item);
	const int32 EquippedIndex = EquippedItems.IndexOfByPredicate([&Item](const FImmortalEquipmentItem& Existing)
	{
		return Existing.Slot == Item.Slot;
	});
	const bool bHasEquippedItem = EquippedIndex != INDEX_NONE;
	const float ExistingPower = bHasEquippedItem
		? UImmortalEquipmentLibrary::CalculateEquipmentPower(EquippedItems[EquippedIndex])
		: -1.0f;
	const bool bCompatible = UImmortalCharacterPathLibrary::IsEquipmentCompatible(
		CultivationPathState.Path, Item.Discipline);
	const bool bCurrentEquipmentProtected = bHasEquippedItem && EquippedItems[EquippedIndex].bLocked;
	auto CalculateCompatibleLoadoutPower = [this](const TArray<FImmortalEquipmentItem>& Loadout)
	{
		TArray<FImmortalEquipmentItem> CompatibleLoadout;
		for (const FImmortalEquipmentItem& Candidate : Loadout)
		{
			if (IsEquipmentCompatibleWithPath(Candidate)) CompatibleLoadout.Add(Candidate);
		}
		const float CultivationAttack = CultivationComponent ? CultivationComponent->GetAttackBonus() : 0.0f;
		const float CultivationDefense = CultivationComponent ? CultivationComponent->GetDefenseBonus() : 0.0f;
		const float CultivationHealth = CultivationComponent ? CultivationComponent->GetHealthBonus() : 0.0f;
		return UImmortalEquipmentLibrary::CalculateLoadoutPowerWithBaseStats(
			CompatibleLoadout,
			AttackDamage + CultivationAttack,
			Defense + CultivationDefense,
			MaxHealth + CultivationHealth,
			AttackSpeedMultiplier + ArtifactAttackSpeedBonus + TechniqueAttackSpeedBonus + CharacterPathAttackSpeedBonus,
			CriticalChance + ArtifactCriticalChanceBonus + TechniqueCriticalChanceBonus + CharacterPathCriticalChanceBonus,
			CriticalDamageMultiplier);
	};
	const float ExistingLoadoutPower = CalculateCompatibleLoadoutPower(EquippedItems);
	TArray<FImmortalEquipmentItem> CandidateLoadout = EquippedItems;
	if (bHasEquippedItem) CandidateLoadout[EquippedIndex] = Item;
	else CandidateLoadout.Add(Item);
	const float CandidateLoadoutPower = CalculateCompatibleLoadoutPower(CandidateLoadout);
	bool bShouldEquip = bAutoEquipNewItems && bCompatible && !bCurrentEquipmentProtected
		&& CandidateLoadoutPower > ExistingLoadoutPower + KINDA_SMALL_NUMBER;
	if (bShouldEquip && bHasEquippedItem)
	{
		// Replacing an equipped item needs one real backpack slot. Never discard the
		// previous item just because a stronger drop arrived while the backpack is full.
		bShouldEquip = AddItemToInventory(EquippedItems[EquippedIndex]);
	}

	if (bShouldEquip)
	{
		if (bHasEquippedItem)
		{
			EquippedItems[EquippedIndex] = Item;
		}
		else
		{
			EquippedItems.Add(Item);
		}

		++EquipmentInventoryRevision;
		RecalculateEquipmentBonuses();
		if (!FinalizeAcquisition()) return false;
		if (bShowFeedback && !bDead && bAutoAttackOnBeginPlay)
		{
			StartAutoAttack();
		}
		UE_LOG(LogTemp, Display, TEXT("Equipment auto-equipped: %s | item power %.2f vs %.2f | loadout %.2f -> %.2f | combat power %.2f"),
			*Item.DisplayName.ToString(), NewPower, ExistingPower, ExistingLoadoutPower, CandidateLoadoutPower, GetCombatPower());
		if (bNotifyChanges)
		{
			BP_OnEquipmentChanged(Item.Slot, Item, true, GetCombatPower());
			BP_OnInventoryChanged(InventoryItems.Num(), FMath::Max(InventoryCapacity, 1));
		}
		if (bShowFeedback && CombatFeedbackWidget)
		{
			CombatFeedbackWidget->ShowEquipmentPickup(FText::FromName(Item.DisplayName), UImmortalEquipmentLibrary::GetQualityColor(Item.Quality), true);
		}
		if (bShowFeedback)
		{
			QueueManagementNotification(
				FText::FromString(FString::Printf(
					TEXT("%s\uFF08\u5DF2\u81EA\u52A8\u62FE\u53D6\u5E76\u6362\u88C5\uFF09"),
					*Item.DisplayName.ToString())),
				UImmortalEquipmentLibrary::GetQualityColor(Item.Quality),
				5.0f);
		}
		return true;
	}

	const bool bStored = AddItemToInventory(Item);
	if (bStored) ++EquipmentInventoryRevision;
	if (bStored && !FinalizeAcquisition()) return false;
	UE_LOG(LogTemp, Display, TEXT("Equipment stored=%s: %s | compatible %s | item power %.2f | backpack %d/%d"),
		bStored ? TEXT("true") : TEXT("false"), *Item.DisplayName.ToString(),
		bCompatible ? TEXT("true") : TEXT("false"), NewPower, InventoryItems.Num(), FMath::Max(InventoryCapacity, 1));
	if (bStored && bNotifyChanges) BP_OnInventoryChanged(InventoryItems.Num(), FMath::Max(InventoryCapacity, 1));
	if (bStored && bShowFeedback && CombatFeedbackWidget)
	{
		CombatFeedbackWidget->ShowEquipmentPickup(FText::FromName(Item.DisplayName), UImmortalEquipmentLibrary::GetQualityColor(Item.Quality), false);
	}
	if (bStored && bShowFeedback)
	{
		QueueManagementNotification(
			FText::FromString(FString::Printf(
				TEXT("%s\uFF08\u5DF2\u81EA\u52A8\u62FE\u53D6\u81F3\u50A8\u7269\u6212\uFF09"),
				*Item.DisplayName.ToString())),
			UImmortalEquipmentLibrary::GetQualityColor(Item.Quality),
			5.0f);
	}
	return bStored;
}

bool AImmortalPlayerCharacter::SaveProgress()
{
	return SaveProgressWithMapOverride(nullptr);
}

bool AImmortalPlayerCharacter::SaveProgressWithMapOverride(
	const FImmortalMapSystemState* MapStateOverride)
{
	const FImmortalCaveState CaveBeforeSettlement = CaveState;
	const FImmortalFarmingState FarmingBeforeSettlement = FarmingState;
	SettleCaveProduction();
	SettleFarmingGrowth();
	UImmortalPathSaveGame* SaveGame = UImmortalPathSaveGame::LoadOrCreate(this);
	if (!SaveGame)
	{
		CaveState = CaveBeforeSettlement;
		FarmingState = FarmingBeforeSettlement;
		return false;
	}

	FImmortalMapSystemState MapStateToSave;
	if (MapStateOverride)
	{
		MapStateToSave = *MapStateOverride;
		UImmortalMapLibrary::NormalizeState(
			MapStateToSave);
		SaveGame->MapSystemState = MapStateToSave;
		FImmortalMapProgress QingyunProgress;
		if (UImmortalMapLibrary::GetMapProgress(
			MapStateToSave,
			UImmortalMapLibrary::GetQingyunMountainId(),
			QingyunProgress))
		{
			SaveGame->bHasStageData = true;
			SaveGame->QingyunStage =
				QingyunProgress.Stage;
			SaveGame->QingyunStageKills =
				QingyunProgress.StageKills;
			SaveGame->bQingyunMountainCompleted =
				QingyunProgress.bCompleted;
		}
	}

	SaveGame->bHasPlayerData = true;
	SaveGame->PlayerHealth = bDead ? GetMaxHealth() : FMath::Clamp(CurrentHealth, 0.0f, GetMaxHealth());
	SaveGame->PlayerMana = FMath::Clamp(CurrentMana, 0.0f, GetMaxMana());
	if (CultivationComponent)
	{
		CurrentCultivation = CultivationComponent->GetCurrentCultivation();
		SaveGame->bHasCultivationData = true;
		SaveGame->CultivationRealmIndex = static_cast<int32>(CultivationComponent->GetCurrentRealm());
		SaveGame->CultivationMinorStage = CultivationComponent->GetCurrentMinorStage();
	}
	SaveGame->Cultivation = FMath::Max(CurrentCultivation, 0);
	SaveGame->SpiritStones = FMath::Max(CurrentGold, 0);
	SaveGame->EquipmentDropCount = FMath::Max(EquipmentDropCount, 0);
	SaveGame->InventoryItems = InventoryItems;
	SaveGame->EquippedItems = EquippedItems;
	SaveGame->MaterialInventory = MaterialInventory;
	SaveGame->PillInventory = PillInventory;
	SaveGame->ArtifactInventory = ArtifactInventory;
	SaveGame->EquippedArtifactInstanceId = EquippedArtifactInstanceId;
	SaveGame->QuestItemInventory = QuestItemInventory;
	SaveGame->bInventoryManagementInitialized = true;
	SaveGame->bEquipmentExpansionInitialized = true;
	UImmortalWorldBossLibrary::NormalizeState(WorldBossState);
	SaveGame->bWorldBossInitialized = true;
	SaveGame->WorldBossState = WorldBossState;
	UImmortalEndlessDungeonLibrary::NormalizeState(
		EndlessDungeonState);
	SaveGame->bEndlessDungeonInitialized = true;
	SaveGame->EndlessDungeonState = EndlessDungeonState;
	UImmortalPetLibrary::NormalizeState(PetState);
	SaveGame->bPetSystemInitialized = true;
	SaveGame->PetState = PetState;
	UImmortalAscensionLibrary::NormalizeState(AscensionState);
	SaveGame->bAscensionSystemInitialized = true;
	SaveGame->AscensionState = AscensionState;
	UImmortalQuestLibrary::NormalizeState(
		QuestState,
		FDateTime::UtcNow().GetTicks(),
		SectUtcOffsetMinutes);
	SaveGame->bQuestSystemInitialized = true;
	SaveGame->QuestState = QuestState;
	SaveGame->bDeathCultivationRecoveryRequired =
		bDeathCultivationRecoveryRequired;
	SaveGame->TechniqueLibrary = TechniqueLibrary;
	SaveGame->EquippedTechniqueIds = EquippedTechniqueIds;
	SaveGame->TechniqueInsightPoints = TechniqueInsightPoints;
	SaveGame->SpiritRootState = SpiritRootState;
	SaveGame->CultivationPathState = CultivationPathState;
	SaveGame->ShopState = ShopState;
	SaveGame->CaveState = CaveState;
	SaveGame->FarmingState = FarmingState;
	SaveGame->SectState = SectState;
	SaveGame->AlchemyCultivationBoostMultiplier = AlchemyCultivationBoostMultiplier;
	SaveGame->AlchemyCultivationBoostRemainingSeconds = GetAlchemyBoostRemainingSeconds();
	SaveGame->LastOfflineClaimUtcTicks = LastOfflineClaimUtcTicks;
	SaveGame->TotalRewardedOfflineSeconds = FMath::Max<int64>(TotalRewardedOfflineSeconds, 0);
	SaveGame->TotalOfflineClaims = FMath::Max(TotalOfflineClaims, 0);
	const bool bSaved = SaveGame->SaveToDisk();
	if (bSaved)
	{
		int32 SoldOutListings = 0;
		for (const FImmortalShopListing& Listing : ShopState.Listings) SoldOutListings += Listing.bSoldOut ? 1 : 0;
		int32 LockedEquipment = 0;
		for (const FImmortalEquipmentItem& Item : InventoryItems) LockedEquipment += Item.bLocked ? 1 : 0;
		int32 LockedArtifacts = 0;
		for (const FImmortalArtifactItem& Item : ArtifactInventory) LockedArtifacts += Item.bLocked ? 1 : 0;
		UE_LOG(LogTemp, Display, TEXT("Player progress saved: realm %s | cultivation %d/%d | spirit stones %d | equipped %d | backpack %d/locked%d | material types %d | pill stacks %d | artifacts %d/locked%d | quest types %d | artifact equipped %s | techniques %d/%d | insight %d | root %d/%.2f | path %d/switches %d | shop %d/%d/%d/%d | alchemy boost %.0fs"),
			*GetFullCultivationRealmName().ToString(), CurrentCultivation, GetRequiredCultivation(),
			CurrentGold, EquippedItems.Num(), InventoryItems.Num(), LockedEquipment, MaterialInventory.Num(), PillInventory.Num(),
			ArtifactInventory.Num(), LockedArtifacts, QuestItemInventory.Num(), EquippedArtifactInstanceId.IsValid() ? TEXT("true") : TEXT("false"),
			TechniqueLibrary.Num(), EquippedTechniqueIds.Num(), TechniqueInsightPoints,
			static_cast<int32>(SpiritRootState.Root), SpiritRootState.Purity,
			static_cast<int32>(CultivationPathState.Path), CultivationPathState.SwitchCount,
			ShopState.RefreshDayKey, ShopState.RefreshSerial, ShopState.Listings.Num(), SoldOutListings,
			GetAlchemyBoostRemainingSeconds());
		FString SetSummary = GetEquipmentSetSummaryText().ToString();
		SetSummary.ReplaceInline(TEXT("\n"), TEXT("; "));
		UE_LOG(LogTemp, Display,
			TEXT("Equipment expansion saved: version=%d initialized=%s ordinarySlots=%d equipped=%d artifact=%s sets=%s"),
			SaveGame->SaveVersion, SaveGame->bEquipmentExpansionInitialized ? TEXT("true") : TEXT("false"),
			static_cast<int32>(EImmortalEquipmentSlot::MAX), EquippedItems.Num(),
			EquippedArtifactInstanceId.IsValid() ? TEXT("equipped") : TEXT("empty"), *SetSummary);
		UE_LOG(LogTemp, Display,
			TEXT("World Boss state saved: initialized=%s bosses=%d pendingRewards=%d revision=%d"),
			WorldBossState.bInitialized ? TEXT("true") : TEXT("false"),
			WorldBossState.BossProgress.Num(),
			WorldBossState.PendingRewards.Num(),
			WorldBossState.Revision);
		UE_LOG(LogTemp, Display,
			TEXT("Endless Dungeon state saved: initialized=%s highest=%d totalFloors=%lld runs=%d pendingRewards=%d revision=%d"),
			EndlessDungeonState.bInitialized
				? TEXT("true") : TEXT("false"),
			EndlessDungeonState.HighestClearedFloor,
			EndlessDungeonState.TotalFloorsCleared,
			EndlessDungeonState.TotalRuns,
			EndlessDungeonState.PendingRewards.Num(),
			EndlessDungeonState.Revision);
		UE_LOG(LogTemp, Display,
			TEXT("Pet state saved: initialized=%s active=%s pets=%d totalKills=%lld revision=%d"),
			PetState.bInitialized ? TEXT("true") : TEXT("false"),
			*PetState.ActivePetId.ToString(),
			PetState.Pets.Num(),
			PetState.TotalCombatKills,
			PetState.Revision);
		UE_LOG(LogTemp, Display,
			TEXT("Ascension state saved: initialized=%s count=%d seals=%d earned=%lld paths=%d/%d/%d lifetimeMaps=%d revision=%d"),
			AscensionState.bInitialized ? TEXT("true") : TEXT("false"),
			AscensionState.AscensionCount,
			AscensionState.ImmortalSeals,
			AscensionState.TotalImmortalSealsEarned,
			AscensionState.BattlePathRank,
			AscensionState.EnlightenmentPathRank,
			AscensionState.FortunePathRank,
			AscensionState.LifetimeMapRecords.Num(),
			AscensionState.Revision);
		UE_LOG(LogTemp, Display,
			TEXT("Quest state saved: initialized=%s day=%d claims=%lld kills=%lld stages=%lld bosses=%lld revision=%d"),
			QuestState.bInitialized ? TEXT("true") : TEXT("false"),
			QuestState.DailyDayKey,
			QuestState.TotalClaims,
			QuestState.LifetimeCounters.MonsterKills,
			QuestState.LifetimeCounters.StageClears,
			QuestState.LifetimeCounters.BossKills,
			QuestState.Revision);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Death cultivation recovery saved: required=%s adventureSuspended=%s"),
			bDeathCultivationRecoveryRequired ? TEXT("true") : TEXT("false"),
			bAdventureSuspendedForDeathRecovery ? TEXT("true") : TEXT("false"));
		if (MapStateOverride)
		{
			UE_LOG(LogTemp, Display,
				TEXT("Player map override saved atomically: active=%s maps=%d"),
				*MapStateToSave.ActiveMapId.ToString(),
				MapStateToSave.MapProgress.Num());
		}
	}
	else
	{
		// Settlement is part of the persisted snapshot. If the write fails, keep
		// the old high-water marks so the next successful save can settle the
		// complete interval instead of losing cave or crop growth.
		CaveState = CaveBeforeSettlement;
		FarmingState = FarmingBeforeSettlement;
	}
	return bSaved;
}

bool AImmortalPlayerCharacter::LoadProgress()
{
	UImmortalPathSaveGame* SaveGame = UImmortalPathSaveGame::LoadOrCreate(this);
	if (!SaveGame || !SaveGame->bHasPlayerData)
	{
		UE_LOG(LogTemp, Display, TEXT("No saved player progress found; starting with defaults"));
		return false;
	}
	const int32 LoadedSaveVersion = SaveGame->SaveVersion;
	const int64 CurrentUtcTicks = FDateTime::UtcNow().GetTicks();

	InventoryItems = SaveGame->InventoryItems;
	EquippedItems = SaveGame->EquippedItems;
	const bool bEquipmentCollectionsNormalized = UImmortalInventoryLibrary::NormalizeEquipmentCollections(
		InventoryItems, EquippedItems);
	++EquipmentInventoryRevision;
	MaterialInventory = SaveGame->MaterialInventory;
	UImmortalMaterialLibrary::NormalizeInventory(MaterialInventory);
	++MaterialInventoryRevision;
	PillInventory = SaveGame->PillInventory;
	UImmortalAlchemyLibrary::NormalizePillInventory(PillInventory);
	++PillInventoryRevision;
	ArtifactInventory = SaveGame->ArtifactInventory;
	EquippedArtifactInstanceId = SaveGame->EquippedArtifactInstanceId;
	UImmortalArtifactLibrary::NormalizeInventory(ArtifactInventory, EquippedArtifactInstanceId);
	++ArtifactInventoryRevision;
	const bool bNeedsWorldBossInitialization =
		LoadedSaveVersion < 18
		|| !SaveGame->bWorldBossInitialized
		|| !SaveGame->WorldBossState.bInitialized;
	WorldBossState = bNeedsWorldBossInitialization
		? UImmortalWorldBossLibrary::CreateDefaultState()
		: SaveGame->WorldBossState;
	const bool bWorldBossStateNormalized =
		UImmortalWorldBossLibrary::NormalizeState(WorldBossState);
	const bool bNeedsWorldBossMigration =
		bNeedsWorldBossInitialization || bWorldBossStateNormalized;
	const bool bLegacyEndlessDungeonSave = LoadedSaveVersion < 19;
	const bool bNeedsEndlessDungeonMarkerRepair =
		!SaveGame->bEndlessDungeonInitialized
		|| !SaveGame->EndlessDungeonState.bInitialized;
	EndlessDungeonState = bLegacyEndlessDungeonSave
		? UImmortalEndlessDungeonLibrary::CreateDefaultState()
		: SaveGame->EndlessDungeonState;
	const bool bEndlessDungeonStateNormalized =
		UImmortalEndlessDungeonLibrary::NormalizeState(
			EndlessDungeonState);
	const bool bNeedsEndlessDungeonMigration =
		bLegacyEndlessDungeonSave
		|| bNeedsEndlessDungeonMarkerRepair
		|| bEndlessDungeonStateNormalized;
	const bool bLegacyPetSave = LoadedSaveVersion < 20;
	const bool bNeedsPetMarkerRepair =
		!SaveGame->bPetSystemInitialized
		|| !SaveGame->PetState.bInitialized;
	PetState = bLegacyPetSave
		? UImmortalPetLibrary::CreateDefaultState()
		: SaveGame->PetState;
	const bool bPetStateNormalized =
		UImmortalPetLibrary::NormalizeState(PetState);
	const bool bNeedsPetMigration =
		bLegacyPetSave
		|| bNeedsPetMarkerRepair
		|| bPetStateNormalized;
	const bool bLegacyAscensionSave =
		LoadedSaveVersion < 21;
	const bool bNeedsAscensionMarkerRepair =
		!SaveGame->bAscensionSystemInitialized
		|| !SaveGame->AscensionState.bInitialized;
	AscensionState = bLegacyAscensionSave
		? UImmortalAscensionLibrary::CreateDefaultState()
		: SaveGame->AscensionState;
	const bool bAscensionStateNormalized =
		UImmortalAscensionLibrary::NormalizeState(
			AscensionState);
	const bool bNeedsAscensionMigration =
		bLegacyAscensionSave
		|| bNeedsAscensionMarkerRepair
		|| bAscensionStateNormalized;
	RecalculateAscensionBonuses();
	const bool bLegacyQuestSave = LoadedSaveVersion < 22;
	const bool bNeedsQuestMarkerRepair =
		!SaveGame->bQuestSystemInitialized
		|| !SaveGame->QuestState.bInitialized;
	QuestState = bLegacyQuestSave
		? UImmortalQuestLibrary::CreateDefaultState(
			CurrentUtcTicks, SectUtcOffsetMinutes)
		: SaveGame->QuestState;
	const bool bQuestStateNormalized =
		UImmortalQuestLibrary::NormalizeState(
			QuestState, CurrentUtcTicks, SectUtcOffsetMinutes);
	const int32 PreviousQuestDayKey = QuestState.DailyDayKey;
	const FImmortalQuestDailyRefreshResult QuestRefresh =
		UImmortalQuestLibrary::EnsureDailyState(
			QuestState, CurrentUtcTicks, SectUtcOffsetMinutes);
	const bool bNeedsQuestMigration =
		bLegacyQuestSave
		|| bNeedsQuestMarkerRepair
		|| bQuestStateNormalized
		|| (QuestRefresh.bStateChanged
			&& QuestState.DailyDayKey != PreviousQuestDayKey);
	const bool bNeedsDeathRecoveryMigration = LoadedSaveVersion < 23;
	bDeathCultivationRecoveryRequired =
		bNeedsDeathRecoveryMigration
			? false
			: SaveGame->bDeathCultivationRecoveryRequired;
	bAdventureSuspendedForDeathRecovery =
		bDeathCultivationRecoveryRequired;
	QuestItemInventory = SaveGame->QuestItemInventory;
	UImmortalInventoryLibrary::NormalizeQuestItemInventory(QuestItemInventory);
	++QuestItemInventoryRevision;
	ArtifactAttackCounter = 0;
	ArtifactShield = 0.0f;
	TechniqueLibrary = SaveGame->TechniqueLibrary;
	EquippedTechniqueIds = SaveGame->EquippedTechniqueIds;
	TechniqueInsightPoints = SaveGame->TechniqueInsightPoints;
	UImmortalTechniqueLibrary::NormalizeLibrary(TechniqueLibrary, EquippedTechniqueIds, TechniqueInsightPoints);
	++TechniqueRevision;
	TechniqueAttackCounters.Reset();
	TechniqueActiveCounters.Reset();
	TechniqueShield = 0.0f;
	SpiritRootState = SaveGame->SpiritRootState;
	const bool bNeedsInventoryMigration =
		LoadedSaveVersion < 16 || !SaveGame->bInventoryManagementInitialized;
	const bool bNeedsEquipmentExpansionMigration =
		LoadedSaveVersion < 17 || !SaveGame->bEquipmentExpansionInitialized || bEquipmentCollectionsNormalized;
	const bool bNeedsCharacterBuildMigration =
		LoadedSaveVersion < 10
		|| !SpiritRootState.IsAwakened();
	UImmortalCharacterPathLibrary::NormalizeSpiritRoot(SpiritRootState);
	AwakenSpiritRootIfNeeded();
	CultivationPathState = SaveGame->CultivationPathState;
	UImmortalCharacterPathLibrary::NormalizeCultivationPath(CultivationPathState);
	++CharacterBuildRevision;
	CultivationPathAttackCounter = 0;
	CultivationPathShield = 0.0f;
	const bool bEquipmentReconciled = ReconcileEquipmentForPath(CultivationPathState.Path, true);
	if (!bEquipmentReconciled && CultivationPathState.IsSelected())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Saved cultivation-path equipment could not be reconciled because the backpack is full; incompatible equipped items remain disabled until space is available"));
	}
	if (CultivationComponent)
	{
		const int32 RealmIndex = SaveGame->bHasCultivationData
			? SaveGame->CultivationRealmIndex
			: static_cast<int32>(EImmortalCultivationRealm::QiRefining);
		const int32 MinorStage = SaveGame->bHasCultivationData ? SaveGame->CultivationMinorStage : 1;
		CultivationComponent->InitializeProgress(
			static_cast<EImmortalCultivationRealm>(FMath::Clamp(
				RealmIndex, 0, static_cast<int32>(EImmortalCultivationRealm::Ascension))),
			MinorStage,
			FMath::Max(SaveGame->Cultivation, 0));
		CurrentCultivation = CultivationComponent->GetCurrentCultivation();
	}
	else
	{
		CurrentCultivation = FMath::Max(SaveGame->Cultivation, 0);
	}
	CurrentGold = FMath::Max(SaveGame->SpiritStones, 0);
	if (SaveGame->bHasStageData || SaveGame->MapSystemState.bInitialized)
	{
		FImmortalMapSystemState LoadedMapState = SaveGame->MapSystemState;
		if (!LoadedMapState.bInitialized)
		{
			LoadedMapState = UImmortalMapLibrary::CreateMigratedState(
				SaveGame->QingyunStage, SaveGame->QingyunStageKills, SaveGame->bQingyunMountainCompleted);
		}
		UImmortalMapLibrary::NormalizeState(LoadedMapState);
		CachedMapSystemState = LoadedMapState;
		DisplayedMapId = LoadedMapState.ActiveMapId;
		FImmortalMapDefinition Definition;
		FImmortalMapProgress Progress;
		if (UImmortalMapLibrary::GetMapDefinition(DisplayedMapId, Definition)
			&& UImmortalMapLibrary::GetMapProgress(LoadedMapState, DisplayedMapId, Progress))
		{
			DisplayedMapName = Definition.DisplayName;
			DisplayedMapMaximumStage = Definition.MaximumStage;
			DisplayedStage = Progress.Stage;
			DisplayedStageKills = Progress.StageKills;
			bDisplayedMapCompleted = Progress.bCompleted;
			bDisplayedBossStage = !Progress.bCompleted
				&& (Progress.Stage >= Definition.MaximumStage
					|| Progress.Stage % FMath::Max(Definition.BossStageInterval, 2) == 0);
			DisplayedStageRequiredKills = bDisplayedBossStage || bDisplayedMapCompleted ? 1 : 10;
		}
	}
	ShopState = SaveGame->ShopState;
	const bool bShopStateNormalized = UImmortalShopLibrary::NormalizeState(ShopState);
	++ShopRevision;
	const int32 CurrentShopDayKey = UImmortalShopLibrary::GetDayKeyFromUtcTicks(
		FDateTime::UtcNow().GetTicks(), ShopUtcOffsetMinutes);
	const bool bShopNeedsRegeneration = LoadedSaveVersion < 11
		|| ShopState.Listings.IsEmpty()
		|| UImmortalShopLibrary::NeedsDailyRefresh(ShopState, CurrentShopDayKey);
	bool bNeedsShopMigration = bShopStateNormalized;
	if (bShopNeedsRegeneration)
	{
		bNeedsShopMigration = RefreshShopForDay(
			CurrentShopDayKey,
			SaveGame->bHasStageData ? FMath::Clamp(SaveGame->QingyunStage, 1, 999) : 1,
			true) || bNeedsShopMigration;
	}
	const bool bNeedsCaveMigration = LoadedSaveVersion < 13 || !SaveGame->CaveState.bInitialized;
	if (bNeedsCaveMigration)
	{
		// The cave did not exist before v13, so never manufacture resources for time
		// elapsed before the feature was installed.
		CaveState = UImmortalCaveLibrary::CreateDefaultState(CurrentUtcTicks);
	}
	else
	{
		CaveState = SaveGame->CaveState;
		UImmortalCaveLibrary::NormalizeState(CaveState, CurrentUtcTicks);
		SettleCaveProduction(CurrentUtcTicks);
		// Existing v13 saves receive their persistent cave cultivation multiplier
		// during the same offline interval. Temporary alchemy remains excluded.
		RecalculateCaveBonuses();
	}
	const bool bNeedsFarmingMigration = LoadedSaveVersion < 14 || !SaveGame->FarmingState.bInitialized;
	if (bNeedsFarmingMigration)
	{
		// Farming did not exist before v14. Old saves begin with empty plots at
		// the current time and never receive retroactive crops or harvests.
		FarmingState = UImmortalFarmingLibrary::CreateDefaultState(CurrentUtcTicks);
	}
	else
	{
		FarmingState = SaveGame->FarmingState;
		UImmortalFarmingLibrary::NormalizeState(FarmingState, CurrentUtcTicks);
		SettleFarmingGrowth(CurrentUtcTicks);
	}
	const bool bNeedsSectMigration = LoadedSaveVersion < 15 || !SaveGame->SectState.bInitialized;
	bool bNeedsSectStateSave = bNeedsSectMigration;
	if (bNeedsSectMigration)
	{
		// Sect progression did not exist before v15. Old saves begin unaligned at
		// the current CST day and never receive historical tasks or contribution.
		SectState = UImmortalSectLibrary::CreateDefaultState(CurrentUtcTicks, SectUtcOffsetMinutes);
	}
	else
	{
		SectState = SaveGame->SectState;
		bNeedsSectStateSave = UImmortalSectLibrary::NormalizeState(
			SectState, CurrentUtcTicks, SectUtcOffsetMinutes);
		const int32 PreviousSectDayKey = SectState.TaskDayKey;
		const FImmortalSectDailyRefreshResult SectRefresh = UImmortalSectLibrary::EnsureDailyState(
			SectState, CurrentUtcTicks, SectUtcOffsetMinutes);
		// A timestamp-only high-water advance can ride the next normal autosave.
		// Only canonical repairs or an actual daily reset require an immediate
		// startup write, avoiding a redundant synchronous save on every launch.
		bNeedsSectStateSave = bNeedsSectStateSave
			|| (SectRefresh.bStateChanged && SectState.TaskDayKey != PreviousSectDayKey);
	}
	EquipmentDropCount = FMath::Max(SaveGame->EquipmentDropCount, 0);
	LastOfflineClaimUtcTicks = FMath::Max<int64>(SaveGame->LastOfflineClaimUtcTicks, 0);
	TotalRewardedOfflineSeconds = FMath::Max<int64>(SaveGame->TotalRewardedOfflineSeconds, 0);
	TotalOfflineClaims = FMath::Max(SaveGame->TotalOfflineClaims, 0);
	CurrentHealth = 0.0f;
	RecalculateEquipmentBonuses();
	CurrentHealth = SaveGame->PlayerHealth > 0.0f
		? FMath::Clamp(SaveGame->PlayerHealth, 1.0f, GetMaxHealth())
		: GetMaxHealth();
	CurrentMana = FMath::Clamp(SaveGame->PlayerMana, 0.0f, GetMaxMana());
	RestoreAlchemyCultivationBoost(
		FMath::Max(SaveGame->AlchemyCultivationBoostMultiplier, 1.0f),
		FMath::Max(SaveGame->AlchemyCultivationBoostRemainingSeconds, 0.0f));
	ApplyOfflineRewards(SaveGame);
	if (bNeedsCaveMigration)
	{
		// v12 offline cultivation was calculated above without retroactively applying
		// a cave that did not exist during that interval.
		RecalculateCaveBonuses();
	}
	if (bNeedsInventoryMigration || bNeedsEquipmentExpansionMigration || bNeedsCharacterBuildMigration || bNeedsShopMigration || bNeedsCaveMigration || bNeedsFarmingMigration
		|| bNeedsSectStateSave || bNeedsWorldBossMigration
		|| bNeedsEndlessDungeonMigration
		|| bNeedsPetMigration
		|| bNeedsAscensionMigration
		|| bNeedsQuestMigration
		|| bNeedsDeathRecoveryMigration)
	{
		// Persist one-time version migrations and the current day's stock together.
		const bool bMigrationSaved = SaveProgress();
		const TCHAR* MigrationMessage = bMigrationSaved
			? TEXT("completed")
			: TEXT("remains pending because persistence failed");
		if (bMigrationSaved)
		{
			UE_LOG(LogTemp, Display,
				TEXT("Save migration/state refresh %s | loaded version %d -> current version %d | inventory %s | equipment expansion %s | character build %s | shop %s | cave %s | farming %s | sect %s | worldBoss %s | endlessDungeon %s | pet %s | ascension %s | quest %s | deathRecovery %s"),
				MigrationMessage, LoadedSaveVersion, UImmortalPathSaveGame::CurrentSaveVersion,
				bNeedsInventoryMigration ? TEXT("true") : TEXT("false"),
				bNeedsEquipmentExpansionMigration ? TEXT("true") : TEXT("false"),
				bNeedsCharacterBuildMigration ? TEXT("true") : TEXT("false"),
				bNeedsShopMigration ? TEXT("true") : TEXT("false"),
				bNeedsCaveMigration ? TEXT("true") : TEXT("false"),
				bNeedsFarmingMigration ? TEXT("true") : TEXT("false"),
				bNeedsSectStateSave ? TEXT("true") : TEXT("false"),
				bNeedsWorldBossMigration ? TEXT("true") : TEXT("false"),
				bNeedsEndlessDungeonMigration ? TEXT("true") : TEXT("false"),
				bNeedsPetMigration ? TEXT("true") : TEXT("false"),
				bNeedsAscensionMigration ? TEXT("true") : TEXT("false"),
				bNeedsQuestMigration ? TEXT("true") : TEXT("false"),
				bNeedsDeathRecoveryMigration ? TEXT("true") : TEXT("false"));
		}
		else
		{
			UE_LOG(LogTemp, Error,
				TEXT("Save migration/state refresh %s | loaded version %d -> current version %d | inventory %s | equipment expansion %s | character build %s | shop %s | cave %s | farming %s | sect %s | worldBoss %s | endlessDungeon %s | pet %s | ascension %s | quest %s | deathRecovery %s"),
				MigrationMessage, LoadedSaveVersion, UImmortalPathSaveGame::CurrentSaveVersion,
				bNeedsInventoryMigration ? TEXT("true") : TEXT("false"),
				bNeedsEquipmentExpansionMigration ? TEXT("true") : TEXT("false"),
				bNeedsCharacterBuildMigration ? TEXT("true") : TEXT("false"),
				bNeedsShopMigration ? TEXT("true") : TEXT("false"),
				bNeedsCaveMigration ? TEXT("true") : TEXT("false"),
				bNeedsFarmingMigration ? TEXT("true") : TEXT("false"),
				bNeedsSectStateSave ? TEXT("true") : TEXT("false"),
				bNeedsWorldBossMigration ? TEXT("true") : TEXT("false"),
				bNeedsEndlessDungeonMigration ? TEXT("true") : TEXT("false"),
				bNeedsPetMigration ? TEXT("true") : TEXT("false"),
				bNeedsAscensionMigration ? TEXT("true") : TEXT("false"),
				bNeedsQuestMigration ? TEXT("true") : TEXT("false"),
				bNeedsDeathRecoveryMigration ? TEXT("true") : TEXT("false"));
		}
	}
	int32 SoldOutListings = 0;
	for (const FImmortalShopListing& Listing : ShopState.Listings) SoldOutListings += Listing.bSoldOut ? 1 : 0;
	int32 LockedEquipment = 0;
	for (const FImmortalEquipmentItem& Item : InventoryItems) LockedEquipment += Item.bLocked ? 1 : 0;
	int32 LockedArtifacts = 0;
	for (const FImmortalArtifactItem& Item : ArtifactInventory) LockedArtifacts += Item.bLocked ? 1 : 0;
	UE_LOG(LogTemp, Display, TEXT("Player progress loaded: realm %s | cultivation %d/%d | spirit stones %d | equipped %d | backpack %d/locked%d | material types %d | pill stacks %d | artifacts %d/locked%d | quest types %d | artifact equipped %s | techniques %d/%d | insight %d | root %d/%.2f | path %d/switches %d | shop %d/%d/%d/%d | boost %.0fs | combat power %.2f"),
		*GetFullCultivationRealmName().ToString(), CurrentCultivation, GetRequiredCultivation(),
		CurrentGold, EquippedItems.Num(), InventoryItems.Num(), LockedEquipment, MaterialInventory.Num(), PillInventory.Num(),
		ArtifactInventory.Num(), LockedArtifacts, QuestItemInventory.Num(), EquippedArtifactInstanceId.IsValid() ? TEXT("true") : TEXT("false"),
		TechniqueLibrary.Num(), EquippedTechniqueIds.Num(), TechniqueInsightPoints,
		static_cast<int32>(SpiritRootState.Root), SpiritRootState.Purity,
		static_cast<int32>(CultivationPathState.Path), CultivationPathState.SwitchCount,
		ShopState.RefreshDayKey, ShopState.RefreshSerial, ShopState.Listings.Num(), SoldOutListings,
		GetAlchemyBoostRemainingSeconds(), GetCombatPower());
	FString LoadedSetSummary = GetEquipmentSetSummaryText().ToString();
	LoadedSetSummary.ReplaceInline(TEXT("\n"), TEXT("; "));
	UE_LOG(LogTemp, Display,
		TEXT("Equipment expansion loaded: diskVersion=%d currentVersion=%d initialized=%s ordinarySlots=%d equipped=%d artifact=%s sets=%s"),
		LoadedSaveVersion, UImmortalPathSaveGame::CurrentSaveVersion,
		SaveGame->bEquipmentExpansionInitialized ? TEXT("true") : TEXT("false"),
		static_cast<int32>(EImmortalEquipmentSlot::MAX), EquippedItems.Num(),
		EquippedArtifactInstanceId.IsValid() ? TEXT("equipped") : TEXT("empty"), *LoadedSetSummary);
	UE_LOG(LogTemp, Display,
		TEXT("World Boss state loaded: initialized=%s bosses=%d pendingRewards=%d revision=%d migration=%s"),
		WorldBossState.bInitialized ? TEXT("true") : TEXT("false"),
		WorldBossState.BossProgress.Num(),
		WorldBossState.PendingRewards.Num(),
		WorldBossState.Revision,
		bNeedsWorldBossMigration ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Display,
		TEXT("Endless Dungeon state loaded: initialized=%s highest=%d totalFloors=%lld runs=%d pendingRewards=%d revision=%d migration=%s"),
		EndlessDungeonState.bInitialized
			? TEXT("true") : TEXT("false"),
		EndlessDungeonState.HighestClearedFloor,
		EndlessDungeonState.TotalFloorsCleared,
		EndlessDungeonState.TotalRuns,
		EndlessDungeonState.PendingRewards.Num(),
		EndlessDungeonState.Revision,
		bNeedsEndlessDungeonMigration
			? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Display,
		TEXT("Pet state loaded: initialized=%s active=%s pets=%d totalKills=%lld revision=%d migration=%s"),
		PetState.bInitialized ? TEXT("true") : TEXT("false"),
		*PetState.ActivePetId.ToString(),
		PetState.Pets.Num(),
		PetState.TotalCombatKills,
		PetState.Revision,
		bNeedsPetMigration ? TEXT("true") : TEXT("false"));
	FImmortalAscensionMapLegacy LoadedFinalMapLegacy;
	UImmortalAscensionLibrary::GetLifetimeMapRecord(
		AscensionState,
		UImmortalMapLibrary::GetImmortalPalaceRuinsId(),
		LoadedFinalMapLegacy);
	UE_LOG(LogTemp, Display,
		TEXT("Ascension state loaded: initialized=%s count=%d seals=%d earned=%lld paths=%d/%d/%d multipliers=%.2f/%.2f/%.2f lifetimeCompleted=%d/%d finalLegacy=%d/%d revision=%d migration=%s"),
		AscensionState.bInitialized ? TEXT("true") : TEXT("false"),
		AscensionState.AscensionCount,
		AscensionState.ImmortalSeals,
		AscensionState.TotalImmortalSealsEarned,
		AscensionState.BattlePathRank,
		AscensionState.EnlightenmentPathRank,
		AscensionState.FortunePathRank,
		GetAscensionBattleMultiplier(),
		GetAscensionCultivationMultiplier(),
		GetAscensionEquipmentDropMultiplier(),
		UImmortalAscensionLibrary
			::GetLifetimeCompletedMapCount(AscensionState),
		UImmortalMapLibrary::GetKnownMapIds().Num(),
		LoadedFinalMapLegacy.HighestStage,
		LoadedFinalMapLegacy.TimesCompleted,
		AscensionState.Revision,
		bNeedsAscensionMigration ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Display,
		TEXT("Quest state loaded: initialized=%s day=%d claims=%lld kills=%lld stages=%lld bosses=%lld revision=%d migration=%s"),
		QuestState.bInitialized ? TEXT("true") : TEXT("false"),
		QuestState.DailyDayKey,
		QuestState.TotalClaims,
		QuestState.LifetimeCounters.MonsterKills,
		QuestState.LifetimeCounters.StageClears,
		QuestState.LifetimeCounters.BossKills,
		QuestState.Revision,
		bNeedsQuestMigration ? TEXT("true") : TEXT("false"));
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Death cultivation recovery loaded: required=%s migration=%s"),
		bDeathCultivationRecoveryRequired ? TEXT("true") : TEXT("false"),
		bNeedsDeathRecoveryMigration ? TEXT("true") : TEXT("false"));
	return true;
}

void AImmortalPlayerCharacter::ApplyOfflineRewards(UImmortalPathSaveGame* SaveGame)
{
	if (!SaveGame || !CultivationComponent)
	{
		return;
	}
	const FImmortalOfflineRewardResult PreviousOfflineRewardResult = LastOfflineRewardResult;

	int64 DevelopmentOverrideSeconds = -1;
#if !UE_BUILD_SHIPPING
	int32 OverrideSeconds = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestOfflineSeconds="), OverrideSeconds))
	{
		DevelopmentOverrideSeconds = FMath::Max(OverrideSeconds, 0);
		UE_LOG(LogTemp, Display, TEXT("Offline reward development duration override applied: %lld seconds"),
			DevelopmentOverrideSeconds);
	}
#endif

	FImmortalMapSystemState OfflineMapState = SaveGame->MapSystemState;
	if (!OfflineMapState.bInitialized)
	{
		OfflineMapState = UImmortalMapLibrary::CreateMigratedState(
			SaveGame->QingyunStage, SaveGame->QingyunStageKills, SaveGame->bQingyunMountainCompleted);
	}
	UImmortalMapLibrary::NormalizeState(OfflineMapState);
	FImmortalMapProgress OfflineMapProgress;
	FImmortalMapDefinition OfflineMapDefinition;
	if (!UImmortalMapLibrary::GetMapProgress(
			OfflineMapState, OfflineMapState.ActiveMapId, OfflineMapProgress)
		|| !UImmortalMapLibrary::GetMapDefinition(OfflineMapState.ActiveMapId, OfflineMapDefinition))
	{
		OfflineMapState.ActiveMapId = UImmortalMapLibrary::GetQingyunMountainId();
		UImmortalMapLibrary::GetMapProgress(OfflineMapState, OfflineMapState.ActiveMapId, OfflineMapProgress);
		UImmortalMapLibrary::GetMapDefinition(OfflineMapState.ActiveMapId, OfflineMapDefinition);
	}

	const int64 CurrentUtcTicks = FDateTime::UtcNow().GetTicks();
	LastOfflineRewardResult = UImmortalOfflineRewardLibrary::Calculate(
		SaveGame->LastSavedUtcTicks,
		CurrentUtcTicks,
		MinimumOfflineSeconds,
		MaximumOfflineHours,
		CultivationComponent->HasReachedAscension() ? 0.0f : CultivationComponent->GetCultivationPerSecondWithoutAlchemyBoost(),
		OfflineCultivationEfficiency,
		OfflineSpiritStonesPerMinute * FMath::Max(OfflineMapDefinition.SpiritStoneMultiplier, 0.1f),
		OfflineEquipmentIntervalSeconds,
		MaximumOfflineEquipmentCount,
		OfflineMaterialIntervalSeconds,
		MaximumOfflineMaterialBundles,
		DevelopmentOverrideSeconds);

	if (LastOfflineRewardResult.bClockRollbackDetected)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Offline reward rejected because system UTC moved backwards: saved=%lld current=%lld"),
			SaveGame->LastSavedUtcTicks, CurrentUtcTicks);
		// Normalize the timestamp now so moving the clock forward again cannot claim an artificial interval.
		SaveProgress();
		return;
	}
	if (!LastOfflineRewardResult.bEligible)
	{
		UE_LOG(LogTemp, Display, TEXT("Offline duration %lld seconds is below the %d-second reward threshold"),
			LastOfflineRewardResult.RawOfflineSeconds, FMath::Max(MinimumOfflineSeconds, 0));
		return;
	}

	// Offline rewards are one persistent transaction. Any write failure restores
	// every runtime authority so the same interval cannot be duplicated on restart.
	const EImmortalCultivationRealm PreviousRealm = CultivationComponent->GetCurrentRealm();
	const int32 PreviousMinorStage = CultivationComponent->GetCurrentMinorStage();
	const int32 PreviousCultivation = CultivationComponent->GetCurrentCultivation();
	const int32 PreviousGold = CurrentGold;
	const int32 PreviousEquipmentDropCount = EquipmentDropCount;
	const TArray<FImmortalEquipmentItem> PreviousInventoryItems = InventoryItems;
	const TArray<FImmortalEquipmentItem> PreviousEquippedItems = EquippedItems;
	const TArray<FImmortalMaterialStack> PreviousMaterialInventory = MaterialInventory;
	const int32 PreviousEquipmentRevision = EquipmentInventoryRevision;
	const int32 PreviousMaterialRevision = MaterialInventoryRevision;
	const float PreviousHealth = CurrentHealth;
	const float PreviousMana = CurrentMana;
	const int64 PreviousOfflineClaimTicks = LastOfflineClaimUtcTicks;
	const int64 PreviousTotalRewardedSeconds = TotalRewardedOfflineSeconds;
	const int32 PreviousTotalOfflineClaims = TotalOfflineClaims;
	const bool bPreviouslyHadUnshownOfflineReward = bHasUnshownOfflineReward;
	const FImmortalCaveState PreviousCaveState = CaveState;

	if (LastOfflineRewardResult.Cultivation > 0)
	{
		CultivationComponent->AddCultivation(LastOfflineRewardResult.Cultivation);
		CurrentCultivation = CultivationComponent->GetCurrentCultivation();
	}
	CurrentGold = static_cast<int32>(FMath::Min<int64>(
		static_cast<int64>(CurrentGold) + LastOfflineRewardResult.SpiritStones,
		MAX_int32));

	const int32 OfflineItemLevel = 1 + (FMath::Clamp(OfflineMapProgress.Stage, 1, 999) - 1) / 5
		+ FMath::Max(OfflineMapDefinition.EquipmentLevelBonus, 0);
	const int32 BaseOfflineEquipmentCount = LastOfflineRewardResult.EquipmentCount;
	LastOfflineRewardResult.EquipmentCount = FMath::Clamp(
		FMath::FloorToInt(static_cast<float>(BaseOfflineEquipmentCount) * GetEquipmentDropChanceMultiplier()),
		0,
		FMath::Max(MaximumOfflineEquipmentCount, 0));
	int32 EquipmentGranted = 0;
	for (int32 Index = 0; Index < LastOfflineRewardResult.EquipmentCount; ++Index)
	{
		const FImmortalEquipmentItem Item = UImmortalEquipmentLibrary::GenerateRandomEquipmentWithMinimumQuality(
			OfflineItemLevel, OfflineMapDefinition.MinimumEquipmentQuality);
		if (ProcessEquipmentItem(Item, false, false, false))
		{
			++EquipmentGranted;
		}
	}
	LastOfflineRewardResult.EquipmentCount = EquipmentGranted;

	int32 MaterialsGranted = 0;
	for (int32 Index = 0; Index < LastOfflineRewardResult.MaterialBundleCount; ++Index)
	{
		const FImmortalMaterialStack Material = UImmortalMaterialLibrary::GenerateMapDrop(
			OfflineMapState.ActiveMapId, OfflineMapProgress.Stage, false, Index);
		MaterialsGranted += AddMaterialInternal(Material.MaterialId, Material.Quantity);
	}
	LastOfflineRewardResult.MaterialCount = MaterialsGranted;

	LastOfflineClaimUtcTicks = CurrentUtcTicks;
	TotalRewardedOfflineSeconds = FMath::Min<int64>(
		MAX_int64 - LastOfflineRewardResult.RewardedOfflineSeconds < TotalRewardedOfflineSeconds
			? MAX_int64
			: TotalRewardedOfflineSeconds + LastOfflineRewardResult.RewardedOfflineSeconds,
		MAX_int64);
	if (TotalOfflineClaims < MAX_int32)
	{
		++TotalOfflineClaims;
	}
	bHasUnshownOfflineReward = true;

	GetWorldTimerManager().ClearTimer(CultivationBreakthroughSaveTimerHandle);
	if (!SaveProgress())
	{
		CaveState = PreviousCaveState;
		InventoryItems = PreviousInventoryItems;
		EquippedItems = PreviousEquippedItems;
		MaterialInventory = PreviousMaterialInventory;
		CurrentGold = PreviousGold;
		EquipmentDropCount = PreviousEquipmentDropCount;
		EquipmentInventoryRevision = PreviousEquipmentRevision;
		MaterialInventoryRevision = PreviousMaterialRevision;
		LastOfflineClaimUtcTicks = PreviousOfflineClaimTicks;
		TotalRewardedOfflineSeconds = PreviousTotalRewardedSeconds;
		TotalOfflineClaims = PreviousTotalOfflineClaims;
		bHasUnshownOfflineReward = bPreviouslyHadUnshownOfflineReward;
		LastOfflineRewardResult = PreviousOfflineRewardResult;
		CultivationComponent->InitializeProgress(PreviousRealm, PreviousMinorStage, PreviousCultivation);
		CurrentCultivation = CultivationComponent->GetCurrentCultivation();
		RecalculateEquipmentBonuses();
		CurrentHealth = FMath::Clamp(PreviousHealth, 0.0f, GetMaxHealth());
		CurrentMana = FMath::Clamp(PreviousMana, 0.0f, GetMaxMana());
		BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, 0);
		BP_OnInventoryChanged(InventoryItems.Num(), FMath::Max(InventoryCapacity, 1));
		RefreshCultivationHud();
		UE_LOG(LogTemp, Error,
			TEXT("Offline reward transaction rolled back because the save could not be persisted"));
		return;
	}

	BP_OnRewardsChanged(
		CurrentCultivation,
		CurrentGold,
		LastOfflineRewardResult.Cultivation,
		LastOfflineRewardResult.SpiritStones);
	if (LastOfflineRewardResult.EquipmentCount > 0)
	{
		BP_OnEquipmentPickedUp(EquipmentDropCount, LastOfflineRewardResult.EquipmentCount);
		BP_OnInventoryChanged(InventoryItems.Num(), FMath::Max(InventoryCapacity, 1));
		for (const FImmortalEquipmentItem& Equipped : EquippedItems)
		{
			const FImmortalEquipmentItem* Previous = PreviousEquippedItems.FindByPredicate(
				[&Equipped](const FImmortalEquipmentItem& Item) { return Item.Slot == Equipped.Slot; });
			if (!Previous || Previous->ItemId != Equipped.ItemId)
			{
				BP_OnEquipmentChanged(Equipped.Slot, Equipped, true, GetCombatPower());
			}
		}
	}
	BP_OnOfflineRewardsClaimed(
		LastOfflineRewardResult.RewardedOfflineSeconds,
		LastOfflineRewardResult.Cultivation,
		LastOfflineRewardResult.SpiritStones,
		LastOfflineRewardResult.EquipmentCount,
		LastOfflineRewardResult.bCappedByMaximum);
	BP_OnOfflineMaterialsGranted(LastOfflineRewardResult.MaterialCount);
	UE_LOG(LogTemp, Display,
		TEXT("Offline rewards claimed once: map=%s stage=%d | raw=%llds rewarded=%llds%s | cultivation +%d | spirit stones +%d | equipment %d | materials %d | total claims %d"),
		*OfflineMapState.ActiveMapId.ToString(),
		OfflineMapProgress.Stage,
		LastOfflineRewardResult.RawOfflineSeconds,
		LastOfflineRewardResult.RewardedOfflineSeconds,
		LastOfflineRewardResult.bCappedByMaximum ? TEXT(" (capped)") : TEXT(""),
		LastOfflineRewardResult.Cultivation,
		LastOfflineRewardResult.SpiritStones,
		LastOfflineRewardResult.EquipmentCount,
		LastOfflineRewardResult.MaterialCount,
		TotalOfflineClaims);
}

bool AImmortalPlayerCharacter::GetEquippedItemForSlot(
	const EImmortalEquipmentSlot Slot,
	FImmortalEquipmentItem& OutItem) const
{
	if (const FImmortalEquipmentItem* Found = EquippedItems.FindByPredicate([Slot](const FImmortalEquipmentItem& Item)
	{
		return Item.Slot == Slot;
	}))
	{
		OutItem = *Found;
		return true;
	}

	OutItem = FImmortalEquipmentItem();
	return false;
}

bool AImmortalPlayerCharacter::GetEquipmentItemById(
	const FGuid ItemId,
	FImmortalEquipmentItem& OutItem,
	bool& bOutEquipped) const
{
	bOutEquipped = false;
	if (!ItemId.IsValid()) return false;
	if (const FImmortalEquipmentItem* Found = EquippedItems.FindByPredicate([ItemId](const FImmortalEquipmentItem& Item)
	{
		return Item.ItemId == ItemId;
	}))
	{
		OutItem = *Found;
		bOutEquipped = true;
		return true;
	}
	if (const FImmortalEquipmentItem* Found = InventoryItems.FindByPredicate([ItemId](const FImmortalEquipmentItem& Item)
	{
		return Item.ItemId == ItemId;
	}))
	{
		OutItem = *Found;
		return true;
	}
	OutItem = FImmortalEquipmentItem();
	return false;
}

FImmortalEquipmentItem* AImmortalPlayerCharacter::FindMutableEquipmentItem(
	const FGuid ItemId,
	bool& bOutEquipped)
{
	bOutEquipped = false;
	if (!ItemId.IsValid()) return nullptr;
	if (FImmortalEquipmentItem* Found = EquippedItems.FindByPredicate([ItemId](const FImmortalEquipmentItem& Item)
	{
		return Item.ItemId == ItemId;
	}))
	{
		bOutEquipped = true;
		return Found;
	}
	return InventoryItems.FindByPredicate([ItemId](const FImmortalEquipmentItem& Item)
	{
		return Item.ItemId == ItemId;
	});
}

bool AImmortalPlayerCharacter::IsCraftingRecipeUnlocked(const FName RecipeId) const
{
	FImmortalCraftingRecipeDefinition Definition;
	return UImmortalCraftingLibrary::GetRecipeDefinition(RecipeId, Definition)
		&& UImmortalCraftingLibrary::IsRecipeUnlocked(Definition, GetQingyunStage());
}

bool AImmortalPlayerCharacter::CanCraftEquipment(const FName RecipeId) const
{
	FImmortalCraftingRecipeDefinition Definition;
	if (!IsCraftingRecipeUnlocked(RecipeId)
		|| !UImmortalCraftingLibrary::GetRecipeDefinition(RecipeId, Definition)
		|| !UImmortalCraftingLibrary::CanAfford(
			MaterialInventory, CurrentGold, ApplyCaveForgeDiscount(Definition.Cost)))
	{
		return false;
	}
	const bool bOutputSlotOccupied = EquippedItems.ContainsByPredicate([&Definition](const FImmortalEquipmentItem& Item)
	{
		return Item.Slot == Definition.OutputSlot;
	});
	return (bAutoEquipNewItems && !bOutputSlotOccupied)
		|| InventoryItems.Num() < FMath::Max(InventoryCapacity, 1)
		|| FindWeakestReplaceableInventoryItem() != INDEX_NONE;
}

FImmortalCraftingResult AImmortalPlayerCharacter::CraftEquipment(const FName RecipeId)
{
	FImmortalCraftingResult Result;
	Result.RecipeId = RecipeId;
	FImmortalCraftingRecipeDefinition Definition;
	if (!UImmortalCraftingLibrary::GetRecipeDefinition(RecipeId, Definition))
	{
		Result.Message = FText::FromString(TEXT("未找到炼器配方"));
		return Result;
	}
	Result.bUnlocked = UImmortalCraftingLibrary::IsRecipeUnlocked(Definition, GetQingyunStage());
	if (!Result.bUnlocked)
	{
		Result.Message = FText::FromString(TEXT("当前青云山关卡尚未解锁此配方"));
		return Result;
	}
	const FImmortalCraftingCost EffectiveCost = ApplyCaveForgeDiscount(Definition.Cost);
	Result.bAffordable = UImmortalCraftingLibrary::CanAfford(MaterialInventory, CurrentGold, EffectiveCost);
	if (!Result.bAffordable)
	{
		Result.Message = FText::FromString(TEXT("打造材料或灵石不足"));
		return Result;
	}

	FImmortalEquipmentItem CraftedItem = UImmortalEquipmentLibrary::GenerateCraftedEquipment(
		FMath::Max(GetQingyunStage(), 1), Definition.OutputSlot, Definition.OutputQuality,
		EImmortalEquipmentDiscipline::Universal, Definition.OutputSetId);
	if (!CraftedItem.IsValid())
	{
		Result.Message = FText::FromString(TEXT("炼器结果生成失败，资源未被扣除"));
		return Result;
	}

	const TArray<FImmortalMaterialStack> PreviousMaterials = MaterialInventory;
	const TArray<FImmortalEquipmentItem> PreviousInventory = InventoryItems;
	const TArray<FImmortalEquipmentItem> PreviousEquipped = EquippedItems;
	const int32 PreviousGold = CurrentGold;
	const int32 PreviousDropCount = EquipmentDropCount;
	const int32 PreviousMaterialRevision = MaterialInventoryRevision;
	const int32 PreviousEquipmentRevision = EquipmentInventoryRevision;
	const FImmortalQuestState PreviousQuestState = QuestState;
	const float PreviousHealth = CurrentHealth;
	const float PreviousMana = CurrentMana;
	if (!UImmortalCraftingLibrary::ConsumeCost(MaterialInventory, CurrentGold, EffectiveCost))
	{
		Result.Message = FText::FromString(TEXT("炼器事务未完成，资源未被部分扣除"));
		return Result;
	}
	++MaterialInventoryRevision;

	bool bStored = ProcessEquipmentItem(CraftedItem, false, false, false);
	if (!bStored)
	{
		const int32 WeakestIndex = FindWeakestReplaceableInventoryItem();
		if (WeakestIndex != INDEX_NONE)
		{
			InventoryItems[WeakestIndex] = CraftedItem;
			++EquipmentInventoryRevision;
			bStored = true;
		}
	}
	if (!bStored)
	{
		MaterialInventory = PreviousMaterials;
		InventoryItems = PreviousInventory;
		EquippedItems = PreviousEquipped;
		CurrentGold = PreviousGold;
		EquipmentDropCount = PreviousDropCount;
		MaterialInventoryRevision = PreviousMaterialRevision;
		EquipmentInventoryRevision = PreviousEquipmentRevision;
		RecalculateEquipmentBonuses();
		CurrentHealth = FMath::Clamp(PreviousHealth, 0.0f, GetMaxHealth());
		CurrentMana = FMath::Clamp(PreviousMana, 0.0f, GetMaxMana());
		Result.Message = FText::FromString(TEXT("装备背包已满且没有可替换的未锁定装备，打造未消耗资源"));
		return Result;
	}

	const bool bAutoEquipped = EquippedItems.ContainsByPredicate([&CraftedItem](const FImmortalEquipmentItem& Item)
	{
		return Item.ItemId == CraftedItem.ItemId;
	});
	const bool bQuestChanged = RecordQuestProgressWithoutSave(
		EImmortalQuestMetric::CraftingActions, 1);
	if (ShouldForceInventoryPersistenceFailure(TEXT("Craft")) || !SaveProgress())
	{
		MaterialInventory = PreviousMaterials;
		InventoryItems = PreviousInventory;
		EquippedItems = PreviousEquipped;
		CurrentGold = PreviousGold;
		EquipmentDropCount = PreviousDropCount;
		MaterialInventoryRevision = PreviousMaterialRevision;
		EquipmentInventoryRevision = PreviousEquipmentRevision;
		QuestState = PreviousQuestState;
		RecalculateEquipmentBonuses();
		CurrentHealth = FMath::Clamp(PreviousHealth, 0.0f, GetMaxHealth());
		CurrentMana = FMath::Clamp(PreviousMana, 0.0f, GetMaxMana());
		Result.Message = FText::FromString(TEXT("存档写入失败，打造消耗与装备结果已完整回滚"));
		return Result;
	}

	Result.bSucceeded = true;
	Result.ItemId = CraftedItem.ItemId;
	Result.Message = FText::FromString(FString::Printf(TEXT("打造成功：%s（%d 条词条）"),
		*CraftedItem.DisplayName.ToString(), CraftedItem.Affixes.Num()));
	BP_OnEquipmentPickedUp(EquipmentDropCount, 1);
	if (bQuestChanged) BP_OnQuestStateChanged(QuestState);
	BP_OnInventoryChanged(InventoryItems.Num(), GetInventoryCapacity());
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, 0);
	if (bAutoEquipped)
	{
		BP_OnEquipmentChanged(CraftedItem.Slot, CraftedItem, true, GetCombatPower());
		if (GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)) StartAutoAttack();
	}
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->ShowEquipmentPickup(
			FText::FromName(CraftedItem.DisplayName), UImmortalEquipmentLibrary::GetQualityColor(CraftedItem.Quality), bAutoEquipped);
	}
	QueueManagementNotification(
		FText::FromString(FString::Printf(
			TEXT("%s\uFF08\u70BC\u5668\u5B8C\u6210\uFF09"),
			*CraftedItem.DisplayName.ToString())),
		UImmortalEquipmentLibrary::GetQualityColor(CraftedItem.Quality),
		5.0f);
	BP_OnCraftingCompleted(Result);
	UE_LOG(LogTemp, Display, TEXT("Equipment crafted: recipe %s | item %s | affixes %d | stones %d | materials %d"),
		*RecipeId.ToString(), *CraftedItem.DisplayName.ToString(), CraftedItem.Affixes.Num(), CurrentGold, MaterialInventory.Num());
	return Result;
}

bool AImmortalPlayerCharacter::CanEnhanceEquipment(const FGuid ItemId) const
{
	FImmortalEquipmentItem Item;
	bool bEquipped = false;
	return GetEquipmentItemById(ItemId, Item, bEquipped)
		&& Item.EnhancementLevel < 15
		&& UImmortalCraftingLibrary::CanAfford(
			MaterialInventory, CurrentGold,
			ApplyCaveForgeDiscount(UImmortalCraftingLibrary::GetEnhancementCost(Item)));
}

FImmortalCraftingResult AImmortalPlayerCharacter::EnhanceEquipment(const FGuid ItemId)
{
	FImmortalCraftingResult Result;
	Result.RecipeId = TEXT("Enhance");
	bool bEquipped = false;
	FImmortalEquipmentItem* Item = FindMutableEquipmentItem(ItemId, bEquipped);
	if (!Item || Item->EnhancementLevel >= 15)
	{
		Result.Message = FText::FromString(TEXT("装备不存在或已经强化至 +15"));
		return Result;
	}
	const FImmortalCraftingCost Cost = ApplyCaveForgeDiscount(UImmortalCraftingLibrary::GetEnhancementCost(*Item));
	Result.bUnlocked = true;
	Result.bAffordable = UImmortalCraftingLibrary::CanAfford(MaterialInventory, CurrentGold, Cost);
	if (!Result.bAffordable)
	{
		Result.Message = FText::FromString(TEXT("强化材料或灵石不足"));
		return Result;
	}
	const float PreviousPower = UImmortalEquipmentLibrary::CalculateEquipmentPower(*Item);
	if (!UImmortalCraftingLibrary::ConsumeCost(MaterialInventory, CurrentGold, Cost)
		|| !UImmortalEquipmentLibrary::EnhanceEquipment(*Item))
	{
		Result.Message = FText::FromString(TEXT("强化未完成"));
		return Result;
	}
	++MaterialInventoryRevision;
	++EquipmentInventoryRevision;
	if (bEquipped)
	{
		RecalculateEquipmentBonuses();
		BP_OnEquipmentChanged(Item->Slot, *Item, false, GetCombatPower());
	}
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, 0);
	Result.bSucceeded = true;
	Result.ItemId = ItemId;
	Result.Message = FText::FromString(FString::Printf(TEXT("强化成功：+%d，装备战力 %.1f → %.1f"),
		Item->EnhancementLevel, PreviousPower, UImmortalEquipmentLibrary::CalculateEquipmentPower(*Item)));
	const bool bQuestChanged = RecordQuestProgressWithoutSave(
		EImmortalQuestMetric::CraftingActions, 1);
	if (SaveProgress() && bQuestChanged)
	{
		BP_OnQuestStateChanged(QuestState);
	}
	BP_OnCraftingCompleted(Result);
	UE_LOG(LogTemp, Display, TEXT("Equipment enhanced: %s | +%d | power %.2f -> %.2f | stones %d"),
		*Item->DisplayName.ToString(), Item->EnhancementLevel, PreviousPower,
		UImmortalEquipmentLibrary::CalculateEquipmentPower(*Item), CurrentGold);
	return Result;
}

bool AImmortalPlayerCharacter::CanRefineEquipment(const FGuid ItemId) const
{
	FImmortalEquipmentItem Item;
	bool bEquipped = false;
	return GetEquipmentItemById(ItemId, Item, bEquipped)
		&& UImmortalCraftingLibrary::CanAfford(
			MaterialInventory, CurrentGold,
			ApplyCaveForgeDiscount(UImmortalCraftingLibrary::GetRefinementCost(Item)));
}

FImmortalCraftingResult AImmortalPlayerCharacter::RefineEquipment(const FGuid ItemId)
{
	FImmortalCraftingResult Result;
	Result.RecipeId = TEXT("Refine");
	bool bEquipped = false;
	FImmortalEquipmentItem* Item = FindMutableEquipmentItem(ItemId, bEquipped);
	if (!Item)
	{
		Result.Message = FText::FromString(TEXT("未找到需要洗炼的装备"));
		return Result;
	}
	const FImmortalCraftingCost Cost = ApplyCaveForgeDiscount(UImmortalCraftingLibrary::GetRefinementCost(*Item));
	Result.bUnlocked = true;
	Result.bAffordable = UImmortalCraftingLibrary::CanAfford(MaterialInventory, CurrentGold, Cost);
	if (!Result.bAffordable)
	{
		Result.Message = FText::FromString(TEXT("洗炼材料或灵石不足"));
		return Result;
	}
	const float BaseAttack = Item->BaseAttackBonus;
	if (!UImmortalCraftingLibrary::ConsumeCost(MaterialInventory, CurrentGold, Cost)
		|| !UImmortalEquipmentLibrary::RerollEquipmentAffixes(*Item))
	{
		Result.Message = FText::FromString(TEXT("洗炼未完成"));
		return Result;
	}
	++MaterialInventoryRevision;
	++EquipmentInventoryRevision;
	if (bEquipped)
	{
		RecalculateEquipmentBonuses();
		BP_OnEquipmentChanged(Item->Slot, *Item, false, GetCombatPower());
	}
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, 0);
	Result.bSucceeded = true;
	Result.ItemId = ItemId;
	Result.Message = FText::FromString(FString::Printf(TEXT("洗炼成功：第 %d 次，获得 %d 条新词条"),
		Item->RefinementCount, Item->Affixes.Num()));
	const bool bQuestChanged = RecordQuestProgressWithoutSave(
		EImmortalQuestMetric::CraftingActions, 1);
	if (SaveProgress() && bQuestChanged)
	{
		BP_OnQuestStateChanged(QuestState);
	}
	BP_OnCraftingCompleted(Result);
	UE_LOG(LogTemp, Display, TEXT("Equipment refined: %s | count %d | affixes %d | base attack preserved %.2f -> %.2f | stones %d"),
		*Item->DisplayName.ToString(), Item->RefinementCount, Item->Affixes.Num(), BaseAttack, Item->BaseAttackBonus, CurrentGold);
	return Result;
}

bool AImmortalPlayerCharacter::GetEquippedArtifact(FImmortalArtifactItem& OutArtifact) const
{
	const FImmortalArtifactItem* Found = ArtifactInventory.FindByPredicate([this](const FImmortalArtifactItem& Item)
	{
		return Item.InstanceId == EquippedArtifactInstanceId;
	});
	if (!Found) return false;
	OutArtifact = *Found;
	return true;
}

FImmortalArtifactItem* AImmortalPlayerCharacter::FindMutableArtifact(const FGuid InstanceId)
{
	return ArtifactInventory.FindByPredicate([InstanceId](const FImmortalArtifactItem& Item)
	{
		return Item.InstanceId == InstanceId;
	});
}

bool AImmortalPlayerCharacter::IsArtifactUnlocked(const FName ArtifactId) const
{
	FImmortalArtifactDefinition Definition;
	return UImmortalArtifactLibrary::GetArtifactDefinition(ArtifactId, Definition)
		&& GetQingyunStage() >= FMath::Max(Definition.MinimumQingyunStage, 1);
}

bool AImmortalPlayerCharacter::CanCraftArtifact(const FName ArtifactId) const
{
	FImmortalArtifactDefinition Definition;
	return IsArtifactUnlocked(ArtifactId)
		&& UImmortalArtifactLibrary::GetArtifactDefinition(ArtifactId, Definition)
		&& UImmortalCraftingLibrary::CanAfford(
			MaterialInventory, CurrentGold, ApplyCaveForgeDiscount(Definition.CraftingCost));
}

FImmortalArtifactOperationResult AImmortalPlayerCharacter::CraftArtifact(const FName ArtifactId)
{
	FImmortalArtifactOperationResult Result;
	FImmortalArtifactDefinition Definition;
	if (!UImmortalArtifactLibrary::GetArtifactDefinition(ArtifactId, Definition))
	{
		Result.Message = FText::FromString(TEXT("未找到法宝炼制图谱"));
		return Result;
	}
	Result.bUnlocked = GetQingyunStage() >= FMath::Max(Definition.MinimumQingyunStage, 1);
	if (!Result.bUnlocked)
	{
		Result.Message = FText::FromString(FString::Printf(TEXT("青云山第 %d 关解锁此法宝"), Definition.MinimumQingyunStage));
		return Result;
	}
	const FImmortalCraftingCost EffectiveCost = ApplyCaveForgeDiscount(Definition.CraftingCost);
	Result.bAffordable = UImmortalCraftingLibrary::CanAfford(MaterialInventory, CurrentGold, EffectiveCost);
	if (!Result.bAffordable)
	{
		Result.Message = FText::FromString(TEXT("炼制法宝所需灵石或材料不足"));
		return Result;
	}

	FImmortalArtifactItem Crafted = UImmortalArtifactLibrary::CreateArtifact(ArtifactId);
	if (!Crafted.IsValid()
		|| !UImmortalCraftingLibrary::ConsumeCost(MaterialInventory, CurrentGold, EffectiveCost))
	{
		Result.Message = FText::FromString(TEXT("法宝炼制事务未完成，资源未被部分扣除"));
		return Result;
	}
	ArtifactInventory.Add(Crafted);
	const bool bAutoEquipped = !EquippedArtifactInstanceId.IsValid();
	if (bAutoEquipped)
	{
		EquippedArtifactInstanceId = Crafted.InstanceId;
		ArtifactAttackCounter = 0;
		ArtifactShield = 0.0f;
		RecalculateEquipmentBonuses();
		if (GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)) StartAutoAttack();
	}
	++MaterialInventoryRevision;
	++ArtifactInventoryRevision;
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, 0);
	BP_OnArtifactChanged(Crafted, bAutoEquipped);
	Result.bSucceeded = true;
	Result.InstanceId = Crafted.InstanceId;
	Result.Message = FText::FromString(FString::Printf(TEXT("炼制成功：%s%s"),
		*Definition.DisplayName.ToString(), bAutoEquipped ? TEXT("（已自动装备）") : TEXT("")));
	SaveProgress();
	UE_LOG(LogTemp, Display, TEXT("Artifact crafted: %s | instance %s | auto equipped %s | stones %d | materials %d"),
		*ArtifactId.ToString(), *Crafted.InstanceId.ToString(), bAutoEquipped ? TEXT("true") : TEXT("false"),
		CurrentGold, MaterialInventory.Num());
	return Result;
}

FImmortalArtifactOperationResult AImmortalPlayerCharacter::EquipArtifact(const FGuid InstanceId)
{
	FImmortalArtifactOperationResult Result;
	FImmortalArtifactItem* Item = FindMutableArtifact(InstanceId);
	if (!Item)
	{
		Result.Message = FText::FromString(TEXT("法宝不在储物戒中"));
		return Result;
	}
	FImmortalArtifactDefinition Definition;
	if (!UImmortalArtifactLibrary::GetArtifactDefinition(Item->ArtifactId, Definition))
	{
		Result.Message = FText::FromString(TEXT("法宝数据无效"));
		return Result;
	}
	EquippedArtifactInstanceId = InstanceId;
	ArtifactAttackCounter = 0;
	ArtifactShield = 0.0f;
	RecalculateEquipmentBonuses();
	if (GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)) StartAutoAttack();
	++ArtifactInventoryRevision;
	const FImmortalArtifactItem Changed = *Item;
	Result.bSucceeded = true;
	Result.bUnlocked = true;
	Result.bAffordable = true;
	Result.InstanceId = InstanceId;
	Result.Message = FText::FromString(FString::Printf(TEXT("已装备法宝：%s"), *Definition.DisplayName.ToString()));
	SaveProgress();
	BP_OnArtifactChanged(Changed, true);
	UE_LOG(LogTemp, Display, TEXT("Artifact equipped: %s | level %d | stars %d | combat power %.2f"),
		*Item->ArtifactId.ToString(), Item->Level, Item->Stars, GetCombatPower());
	return Result;
}

bool AImmortalPlayerCharacter::CanUpgradeArtifact(const FGuid InstanceId) const
{
	const FImmortalArtifactItem* Item = ArtifactInventory.FindByPredicate([InstanceId](const FImmortalArtifactItem& Entry)
	{
		return Entry.InstanceId == InstanceId;
	});
	return Item && Item->Level < 50
		&& UImmortalCraftingLibrary::CanAfford(
			MaterialInventory, CurrentGold,
			ApplyCaveForgeDiscount(UImmortalArtifactLibrary::GetUpgradeCost(*Item)));
}

FImmortalArtifactOperationResult AImmortalPlayerCharacter::UpgradeArtifact(const FGuid InstanceId)
{
	FImmortalArtifactOperationResult Result;
	FImmortalArtifactItem* Item = FindMutableArtifact(InstanceId);
	if (!Item || Item->Level >= 50)
	{
		Result.Message = FText::FromString(TEXT("法宝不存在或已达到 50 级"));
		return Result;
	}
	const FImmortalCraftingCost Cost = ApplyCaveForgeDiscount(UImmortalArtifactLibrary::GetUpgradeCost(*Item));
	Result.bUnlocked = true;
	Result.bAffordable = UImmortalCraftingLibrary::CanAfford(MaterialInventory, CurrentGold, Cost);
	if (!Result.bAffordable)
	{
		Result.Message = FText::FromString(TEXT("蕴养法宝所需灵石或法宝碎片不足"));
		return Result;
	}
	if (!UImmortalCraftingLibrary::ConsumeCost(MaterialInventory, CurrentGold, Cost))
	{
		Result.Message = FText::FromString(TEXT("法宝蕴养事务未完成"));
		return Result;
	}
	++Item->Level;
	const FImmortalArtifactItem Changed = *Item;
	++MaterialInventoryRevision;
	++ArtifactInventoryRevision;
	if (EquippedArtifactInstanceId == InstanceId)
	{
		RecalculateEquipmentBonuses();
		if (GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)) StartAutoAttack();
	}
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, 0);
	Result.bSucceeded = true;
	Result.InstanceId = InstanceId;
	Result.Message = FText::FromString(FString::Printf(TEXT("蕴养成功：法宝达到 %d 级"), Changed.Level));
	SaveProgress();
	BP_OnArtifactChanged(Changed, EquippedArtifactInstanceId == InstanceId);
	UE_LOG(LogTemp, Display, TEXT("Artifact upgraded: %s | level %d | stars %d | stones %d"),
		*Changed.ArtifactId.ToString(), Changed.Level, Changed.Stars, CurrentGold);
	return Result;
}

bool AImmortalPlayerCharacter::CanStarUpArtifact(const FGuid InstanceId) const
{
	const FImmortalArtifactItem* Item = ArtifactInventory.FindByPredicate([InstanceId](const FImmortalArtifactItem& Entry)
	{
		return Entry.InstanceId == InstanceId;
	});
	return Item && Item->Stars < 5
		&& UImmortalCraftingLibrary::CanAfford(
			MaterialInventory, CurrentGold,
			ApplyCaveForgeDiscount(UImmortalArtifactLibrary::GetStarUpCost(*Item)));
}

FImmortalArtifactOperationResult AImmortalPlayerCharacter::StarUpArtifact(const FGuid InstanceId)
{
	FImmortalArtifactOperationResult Result;
	FImmortalArtifactItem* Item = FindMutableArtifact(InstanceId);
	if (!Item || Item->Stars >= 5)
	{
		Result.Message = FText::FromString(TEXT("法宝不存在或已达到五星"));
		return Result;
	}
	const FImmortalCraftingCost Cost = ApplyCaveForgeDiscount(UImmortalArtifactLibrary::GetStarUpCost(*Item));
	Result.bUnlocked = true;
	Result.bAffordable = UImmortalCraftingLibrary::CanAfford(MaterialInventory, CurrentGold, Cost);
	if (!Result.bAffordable)
	{
		Result.Message = FText::FromString(TEXT("升星所需灵石、法宝碎片或灵铁不足"));
		return Result;
	}
	if (!UImmortalCraftingLibrary::ConsumeCost(MaterialInventory, CurrentGold, Cost))
	{
		Result.Message = FText::FromString(TEXT("法宝升星事务未完成"));
		return Result;
	}
	++Item->Stars;
	const FImmortalArtifactItem Changed = *Item;
	++MaterialInventoryRevision;
	++ArtifactInventoryRevision;
	ArtifactAttackCounter = FMath::Min(ArtifactAttackCounter, UImmortalArtifactLibrary::CalculateTriggerAttackCount(Changed) - 1);
	if (EquippedArtifactInstanceId == InstanceId)
	{
		RecalculateEquipmentBonuses();
		if (GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)) StartAutoAttack();
	}
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, 0);
	Result.bSucceeded = true;
	Result.InstanceId = InstanceId;
	Result.Message = FText::FromString(FString::Printf(TEXT("升星成功：法宝达到 %d 星"), Changed.Stars));
	SaveProgress();
	BP_OnArtifactChanged(Changed, EquippedArtifactInstanceId == InstanceId);
	UE_LOG(LogTemp, Display, TEXT("Artifact starred up: %s | level %d | stars %d | trigger every %d attacks"),
		*Changed.ArtifactId.ToString(), Changed.Level, Changed.Stars,
		UImmortalArtifactLibrary::CalculateTriggerAttackCount(Changed));
	return Result;
}

bool AImmortalPlayerCharacter::GetTechniqueProgress(
	const FName TechniqueId,
	FImmortalTechniqueProgress& OutProgress) const
{
	const FImmortalTechniqueProgress* Found = TechniqueLibrary.FindByPredicate([TechniqueId](const FImmortalTechniqueProgress& Progress)
	{
		return Progress.TechniqueId == TechniqueId;
	});
	if (!Found) return false;
	OutProgress = *Found;
	return true;
}

FImmortalTechniqueProgress* AImmortalPlayerCharacter::FindMutableTechnique(const FName TechniqueId)
{
	return TechniqueLibrary.FindByPredicate([TechniqueId](const FImmortalTechniqueProgress& Progress)
	{
		return Progress.TechniqueId == TechniqueId;
	});
}

bool AImmortalPlayerCharacter::IsTechniqueLearned(const FName TechniqueId) const
{
	return TechniqueLibrary.ContainsByPredicate([TechniqueId](const FImmortalTechniqueProgress& Progress)
	{
		return Progress.TechniqueId == TechniqueId;
	});
}

bool AImmortalPlayerCharacter::IsTechniqueUnlocked(const FName TechniqueId) const
{
	FImmortalTechniqueDefinition Definition;
	return UImmortalTechniqueLibrary::GetTechniqueDefinition(TechniqueId, Definition)
		&& GetQingyunStage() >= FMath::Max(Definition.MinimumQingyunStage, 1)
		&& static_cast<int32>(GetCultivationRealm()) >= FMath::Max(Definition.MinimumRealmIndex, 0);
}

bool AImmortalPlayerCharacter::IsTechniqueEquipped(const FName TechniqueId, int32& OutSlotIndex) const
{
	OutSlotIndex = EquippedTechniqueIds.IndexOfByKey(TechniqueId);
	return OutSlotIndex != INDEX_NONE;
}

FImmortalTechniqueOperationResult AImmortalPlayerCharacter::LearnTechnique(const FName TechniqueId)
{
	FImmortalTechniqueOperationResult Result;
	Result.TechniqueId = TechniqueId;
	FImmortalTechniqueDefinition Definition;
	if (!UImmortalTechniqueLibrary::GetTechniqueDefinition(TechniqueId, Definition))
	{
		Result.Message = FText::FromString(TEXT("未找到功法传承"));
		return Result;
	}
	if (IsTechniqueLearned(TechniqueId))
	{
		Result.Message = FText::FromString(TEXT("这部功法已经学会"));
		return Result;
	}
	Result.bUnlocked = IsTechniqueUnlocked(TechniqueId);
	if (!Result.bUnlocked)
	{
		Result.Message = FText::FromString(FString::Printf(
			TEXT("需要青云山第 %d 关并达到指定境界"), Definition.MinimumQingyunStage));
		return Result;
	}
	Result.bAffordable = UImmortalCraftingLibrary::CanAfford(MaterialInventory, CurrentGold, Definition.LearningCost);
	if (!Result.bAffordable)
	{
		Result.Message = FText::FromString(TEXT("参悟功法所需灵石或材料不足"));
		return Result;
	}
	FImmortalTechniqueProgress Learned = UImmortalTechniqueLibrary::CreateTechnique(TechniqueId);
	if (!Learned.IsValid()
		|| !UImmortalCraftingLibrary::ConsumeCost(MaterialInventory, CurrentGold, Definition.LearningCost))
	{
		Result.Message = FText::FromString(TEXT("功法学习事务未完成，资源未被部分扣除"));
		return Result;
	}
	TechniqueLibrary.Add(Learned);
	const bool bAutoEquipped = EquippedTechniqueIds.Num() < 2;
	if (bAutoEquipped) EquippedTechniqueIds.Add(TechniqueId);
	++MaterialInventoryRevision;
	++TechniqueRevision;
	TechniqueAttackCounters.Remove(TechniqueId);
	TechniqueActiveCounters.Remove(TechniqueId);
	RecalculateEquipmentBonuses();
	if (GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)) StartAutoAttack();
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, 0);
	BP_OnTechniqueChanged(Learned, bAutoEquipped);
	Result.bSucceeded = true;
	Result.InsightPointsAfter = TechniqueInsightPoints;
	Result.Message = FText::FromString(FString::Printf(TEXT("参悟成功：%s%s"),
		*Definition.DisplayName.ToString(), bAutoEquipped ? TEXT("（已自动装入功法槽）") : TEXT("")));
	SaveProgress();
	UE_LOG(LogTemp, Display, TEXT("Technique learned: %s | auto equipped %s | techniques %d/%d | stones %d | cultivation rate x%.3f"),
		*TechniqueId.ToString(), bAutoEquipped ? TEXT("true") : TEXT("false"), TechniqueLibrary.Num(),
		EquippedTechniqueIds.Num(), CurrentGold, TechniqueCultivationRateMultiplier);
	return Result;
}

bool AImmortalPlayerCharacter::CanUpgradeTechnique(const FName TechniqueId) const
{
	FImmortalTechniqueProgress Progress;
	if (!GetTechniqueProgress(TechniqueId, Progress)) return false;
	const int32 Cost = UImmortalTechniqueLibrary::GetUpgradeCultivationCost(Progress);
	return Cost > 0 && CultivationComponent && CultivationComponent->CanSpendCultivation(Cost);
}

FImmortalTechniqueOperationResult AImmortalPlayerCharacter::UpgradeTechnique(const FName TechniqueId)
{
	FImmortalTechniqueOperationResult Result;
	Result.TechniqueId = TechniqueId;
	FImmortalTechniqueProgress* Progress = FindMutableTechnique(TechniqueId);
	if (!Progress)
	{
		Result.Message = FText::FromString(TEXT("尚未学会这部功法"));
		return Result;
	}
	const int32 Cost = UImmortalTechniqueLibrary::GetUpgradeCultivationCost(*Progress);
	if (Cost <= 0)
	{
		Result.Message = Progress->Level >= 50
			? FText::FromString(TEXT("功法已经修至 50 级圆满"))
			: FText::FromString(TEXT("已达到当前重数上限，请先突破功法"));
		return Result;
	}
	Result.bUnlocked = true;
	Result.bAffordable = CultivationComponent && CultivationComponent->CanSpendCultivation(Cost);
	if (!Result.bAffordable || !CultivationComponent->TrySpendCultivation(Cost))
	{
		Result.Message = FText::FromString(FString::Printf(TEXT("当前层修为不足，需要 %d 点修为"), Cost));
		return Result;
	}
	++Progress->Level;
	TechniqueInsightPoints = FMath::Min(TechniqueInsightPoints + 1, 9999);
	const FImmortalTechniqueProgress Changed = *Progress;
	++TechniqueRevision;
	RecalculateEquipmentBonuses();
	if (GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)) StartAutoAttack();
	Result.bSucceeded = true;
	Result.InsightPointsAfter = TechniqueInsightPoints;
	Result.Message = FText::FromString(FString::Printf(
		TEXT("修习成功：%d 级，消耗修为 %d，获得悟性点 1"), Changed.Level, Cost));
	SaveProgress();
	BP_OnTechniqueChanged(Changed, EquippedTechniqueIds.Contains(TechniqueId));
	UE_LOG(LogTemp, Display, TEXT("Technique upgraded: %s | level %d | rank %d | cultivation spent %d | remaining %d | insight %d"),
		*TechniqueId.ToString(), Changed.Level, Changed.BreakthroughRank, Cost, CurrentCultivation, TechniqueInsightPoints);
	return Result;
}

bool AImmortalPlayerCharacter::CanBreakthroughTechnique(const FName TechniqueId) const
{
	FImmortalTechniqueProgress Progress;
	if (!GetTechniqueProgress(TechniqueId, Progress)
		|| Progress.BreakthroughRank >= 4
		|| Progress.Level < UImmortalTechniqueLibrary::GetLevelCap(Progress)) return false;
	return UImmortalCraftingLibrary::CanAfford(
		MaterialInventory, CurrentGold, UImmortalTechniqueLibrary::GetBreakthroughCost(Progress));
}

FImmortalTechniqueOperationResult AImmortalPlayerCharacter::BreakthroughTechnique(const FName TechniqueId)
{
	FImmortalTechniqueOperationResult Result;
	Result.TechniqueId = TechniqueId;
	FImmortalTechniqueProgress* Progress = FindMutableTechnique(TechniqueId);
	if (!Progress || Progress->BreakthroughRank >= 4)
	{
		Result.Message = FText::FromString(TEXT("功法不存在或已完成全部四次突破"));
		return Result;
	}
	if (Progress->Level < UImmortalTechniqueLibrary::GetLevelCap(*Progress))
	{
		Result.Message = FText::FromString(TEXT("请先修至当前重数的等级上限"));
		return Result;
	}
	const FImmortalCraftingCost Cost = UImmortalTechniqueLibrary::GetBreakthroughCost(*Progress);
	Result.bUnlocked = true;
	Result.bAffordable = UImmortalCraftingLibrary::CanAfford(MaterialInventory, CurrentGold, Cost);
	if (!Result.bAffordable)
	{
		Result.Message = FText::FromString(TEXT("功法突破所需灵石或材料不足"));
		return Result;
	}
	if (!UImmortalCraftingLibrary::ConsumeCost(MaterialInventory, CurrentGold, Cost))
	{
		Result.Message = FText::FromString(TEXT("功法突破事务未完成"));
		return Result;
	}
	++Progress->BreakthroughRank;
	TechniqueInsightPoints = FMath::Min(TechniqueInsightPoints + 2, 9999);
	const FImmortalTechniqueProgress Changed = *Progress;
	++MaterialInventoryRevision;
	++TechniqueRevision;
	RecalculateEquipmentBonuses();
	if (GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)) StartAutoAttack();
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, 0);
	Result.bSucceeded = true;
	Result.InsightPointsAfter = TechniqueInsightPoints;
	Result.Message = FText::FromString(FString::Printf(
		TEXT("功法突破成功：第 %d 重，可继续修至 %d 级，悟性点 +2"),
		Changed.BreakthroughRank + 1, UImmortalTechniqueLibrary::GetLevelCap(Changed)));
	SaveProgress();
	BP_OnTechniqueChanged(Changed, EquippedTechniqueIds.Contains(TechniqueId));
	UE_LOG(LogTemp, Display, TEXT("Technique breakthrough: %s | rank %d | level cap %d | insight %d | stones %d"),
		*TechniqueId.ToString(), Changed.BreakthroughRank,
		UImmortalTechniqueLibrary::GetLevelCap(Changed), TechniqueInsightPoints, CurrentGold);
	return Result;
}

bool AImmortalPlayerCharacter::CanAllocateTechniquePoint(
	const FName TechniqueId,
	const EImmortalTechniquePointBranch Branch) const
{
	FImmortalTechniqueProgress Progress;
	if (TechniqueInsightPoints <= 0 || !GetTechniqueProgress(TechniqueId, Progress)) return false;
	int32 CurrentPoints = 0;
	switch (Branch)
	{
	case EImmortalTechniquePointBranch::Active: CurrentPoints = Progress.ActivePoints; break;
	case EImmortalTechniquePointBranch::Passive: CurrentPoints = Progress.PassivePoints; break;
	case EImmortalTechniquePointBranch::Special: CurrentPoints = Progress.SpecialPoints; break;
	default: return false;
	}
	return CurrentPoints < UImmortalTechniqueLibrary::GetBranchPointCap(Branch);
}

FImmortalTechniqueOperationResult AImmortalPlayerCharacter::AllocateTechniquePoint(
	const FName TechniqueId,
	const EImmortalTechniquePointBranch Branch)
{
	FImmortalTechniqueOperationResult Result;
	Result.TechniqueId = TechniqueId;
	FImmortalTechniqueProgress* Progress = FindMutableTechnique(TechniqueId);
	if (!Progress || !CanAllocateTechniquePoint(TechniqueId, Branch))
	{
		Result.Message = TechniqueInsightPoints <= 0
			? FText::FromString(TEXT("悟性点不足，请先修习或突破功法"))
			: FText::FromString(TEXT("该分支已经加满或功法不存在"));
		return Result;
	}
	FText BranchName;
	switch (Branch)
	{
	case EImmortalTechniquePointBranch::Active:
		++Progress->ActivePoints;
		BranchName = FText::FromString(TEXT("主动"));
		break;
	case EImmortalTechniquePointBranch::Passive:
		++Progress->PassivePoints;
		BranchName = FText::FromString(TEXT("被动"));
		break;
	case EImmortalTechniquePointBranch::Special:
		++Progress->SpecialPoints;
		BranchName = FText::FromString(TEXT("特殊"));
		break;
	default:
		return Result;
	}
	--TechniqueInsightPoints;
	const FImmortalTechniqueProgress Changed = *Progress;
	++TechniqueRevision;
	RecalculateEquipmentBonuses();
	if (GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)) StartAutoAttack();
	Result.bSucceeded = true;
	Result.bUnlocked = true;
	Result.bAffordable = true;
	Result.InsightPointsAfter = TechniqueInsightPoints;
	Result.Message = FText::FromString(FString::Printf(TEXT("%s分支加点成功，剩余悟性点 %d"),
		*BranchName.ToString(), TechniqueInsightPoints));
	SaveProgress();
	BP_OnTechniqueChanged(Changed, EquippedTechniqueIds.Contains(TechniqueId));
	UE_LOG(LogTemp, Display, TEXT("Technique point allocated: %s | branch %d | active %d passive %d special %d | insight %d"),
		*TechniqueId.ToString(), static_cast<int32>(Branch), Changed.ActivePoints, Changed.PassivePoints,
		Changed.SpecialPoints, TechniqueInsightPoints);
	return Result;
}

FImmortalTechniqueOperationResult AImmortalPlayerCharacter::EquipTechnique(
	const FName TechniqueId,
	const int32 SlotIndex)
{
	FImmortalTechniqueOperationResult Result;
	Result.TechniqueId = TechniqueId;
	FImmortalTechniqueProgress Progress;
	if (!GetTechniqueProgress(TechniqueId, Progress))
	{
		Result.Message = FText::FromString(TEXT("尚未学会这部功法"));
		return Result;
	}
	const int32 SafeSlot = FMath::Clamp(SlotIndex, 0, 1);
	while (EquippedTechniqueIds.Num() < 2) EquippedTechniqueIds.Add(NAME_None);
	const int32 ExistingSlot = EquippedTechniqueIds.IndexOfByKey(TechniqueId);
	if (ExistingSlot != INDEX_NONE && ExistingSlot != SafeSlot)
	{
		EquippedTechniqueIds.Swap(ExistingSlot, SafeSlot);
	}
	else
	{
		EquippedTechniqueIds[SafeSlot] = TechniqueId;
	}
	UImmortalTechniqueLibrary::NormalizeLibrary(TechniqueLibrary, EquippedTechniqueIds, TechniqueInsightPoints);
	TechniqueAttackCounters.Reset();
	TechniqueActiveCounters.Reset();
	TechniqueShield = 0.0f;
	++TechniqueRevision;
	RecalculateEquipmentBonuses();
	if (GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)) StartAutoAttack();
	FImmortalTechniqueDefinition Definition;
	UImmortalTechniqueLibrary::GetTechniqueDefinition(TechniqueId, Definition);
	int32 ActualSlot = INDEX_NONE;
	IsTechniqueEquipped(TechniqueId, ActualSlot);
	Result.bSucceeded = true;
	Result.bUnlocked = true;
	Result.bAffordable = true;
	Result.InsightPointsAfter = TechniqueInsightPoints;
	Result.Message = FText::FromString(FString::Printf(TEXT("已将%s装入功法槽 %d"),
		*Definition.DisplayName.ToString(), ActualSlot + 1));
	SaveProgress();
	BP_OnTechniqueChanged(Progress, true);
	UE_LOG(LogTemp, Display, TEXT("Technique equipped: %s | slot %d | equipped count %d | combat power %.2f | cultivation rate x%.3f"),
		*TechniqueId.ToString(), ActualSlot + 1, EquippedTechniqueIds.Num(), GetCombatPower(), TechniqueCultivationRateMultiplier);
	return Result;
}

bool AImmortalPlayerCharacter::AddItemToInventory(const FImmortalEquipmentItem& Item)
{
	const int32 SafeCapacity = FMath::Max(InventoryCapacity, 1);
	if (InventoryItems.Num() < SafeCapacity)
	{
		InventoryItems.Add(Item);
		return true;
	}

	const int32 WeakestIndex = FindWeakestReplaceableInventoryItem();
	const float WeakestPower = WeakestIndex == INDEX_NONE
		? TNumericLimits<float>::Max()
		: UImmortalEquipmentLibrary::CalculateEquipmentPower(InventoryItems[WeakestIndex]);

	if (WeakestIndex != INDEX_NONE && UImmortalEquipmentLibrary::CalculateEquipmentPower(Item) > WeakestPower)
	{
		InventoryItems[WeakestIndex] = Item;
		return true;
	}
	return false;
}

int32 AImmortalPlayerCharacter::FindWeakestReplaceableInventoryItem() const
{
	int32 WeakestIndex = INDEX_NONE;
	float WeakestPower = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < InventoryItems.Num(); ++Index)
	{
		if (!InventoryItems[Index].IsValid() || InventoryItems[Index].bLocked) continue;
		const float Power = UImmortalEquipmentLibrary::CalculateEquipmentPower(InventoryItems[Index]);
		if (Power < WeakestPower)
		{
			WeakestPower = Power;
			WeakestIndex = Index;
		}
	}
	return WeakestIndex;
}

void AImmortalPlayerCharacter::RecalculateEquipmentBonuses()
{
	const float PreviousMaxHealth = GetMaxHealth();
	const float PreviousMaxMana = GetMaxMana();
	EquippedAttackBonus = 0.0f;
	EquippedDefenseBonus = 0.0f;
	EquippedHealthBonus = 0.0f;
	EquippedAttackSpeedBonus = 0.0f;
	EquippedCriticalChanceBonus = 0.0f;
	EquippedCriticalDamageBonus = 0.0f;
	EquippedFireDamageBonus = 0.0f;
	EquippedThunderDamageBonus = 0.0f;
	EquippedIceDamageBonus = 0.0f;
	EquippedLifeStealBonus = 0.0f;
	EquippedCultivationGainBonus = 0.0f;
	EquippedLootFindBonus = 0.0f;
	EquippedBossDamageBonus = 0.0f;
	EquipmentAttackMultiplier = 1.0f;
	EquipmentDefenseMultiplier = 1.0f;
	EquipmentHealthMultiplier = 1.0f;
	EquipmentFinalDamageBonus = 0.0f;
	EquipmentDamageReduction = 0.0f;
	TArray<FImmortalEquipmentItem> CompatibleItems;

	for (const FImmortalEquipmentItem& Item : EquippedItems)
	{
		if (!IsEquipmentCompatibleWithPath(Item)) continue;
		CompatibleItems.Add(Item);
		EquippedAttackBonus += FMath::Max(Item.AttackBonus, 0.0f);
		EquippedDefenseBonus += FMath::Max(Item.DefenseBonus, 0.0f);
		EquippedHealthBonus += FMath::Max(Item.HealthBonus, 0.0f);
		EquippedAttackSpeedBonus += FMath::Max(Item.AttackSpeedBonus, 0.0f);
		EquippedCriticalChanceBonus += FMath::Max(Item.CriticalChanceBonus, 0.0f);
		EquippedCriticalDamageBonus += FMath::Max(Item.CriticalDamageBonus, 0.0f);
		EquippedFireDamageBonus += FMath::Max(Item.FireDamageBonus, 0.0f);
		EquippedThunderDamageBonus += FMath::Max(Item.ThunderDamageBonus, 0.0f);
		EquippedIceDamageBonus += FMath::Max(Item.IceDamageBonus, 0.0f);
		EquippedLifeStealBonus += FMath::Max(Item.LifeStealBonus, 0.0f);
		EquippedCultivationGainBonus += FMath::Max(Item.CultivationGainBonus, 0.0f);
		EquippedLootFindBonus += FMath::Max(Item.LootFindBonus, 0.0f);
		EquippedBossDamageBonus += FMath::Max(Item.BossDamageBonus, 0.0f);
	}
	ActiveEquipmentSetBonuses = UImmortalEquipmentLibrary::CalculateSetBonuses(CompatibleItems);
	EquipmentAttackMultiplier = 1.0f + FMath::Max(ActiveEquipmentSetBonuses.AttackMultiplierBonus, 0.0f);
	EquipmentDefenseMultiplier = 1.0f + FMath::Max(ActiveEquipmentSetBonuses.DefenseMultiplierBonus, 0.0f);
	EquipmentHealthMultiplier = 1.0f + FMath::Max(ActiveEquipmentSetBonuses.HealthMultiplierBonus, 0.0f);
	EquippedAttackSpeedBonus += FMath::Max(ActiveEquipmentSetBonuses.AttackSpeedBonus, 0.0f);
	EquippedCriticalChanceBonus += FMath::Max(ActiveEquipmentSetBonuses.CriticalChanceBonus, 0.0f);
	EquippedCriticalDamageBonus += FMath::Max(ActiveEquipmentSetBonuses.CriticalDamageBonus, 0.0f);
	EquippedThunderDamageBonus += FMath::Max(ActiveEquipmentSetBonuses.ThunderDamageBonus, 0.0f);
	EquippedCultivationGainBonus += FMath::Max(ActiveEquipmentSetBonuses.CultivationGainBonus, 0.0f);
	EquippedBossDamageBonus += FMath::Max(ActiveEquipmentSetBonuses.BossDamageBonus, 0.0f);
	EquipmentFinalDamageBonus = FMath::Clamp(ActiveEquipmentSetBonuses.FinalDamageBonus, 0.0f, 3.0f);
	EquipmentDamageReduction = FMath::Clamp(ActiveEquipmentSetBonuses.DamageReductionBonus, 0.0f, 0.75f);
	EquippedLifeStealBonus = FMath::Clamp(EquippedLifeStealBonus, 0.0f, 0.75f);
	EquippedLootFindBonus = FMath::Clamp(EquippedLootFindBonus, 0.0f, 4.0f);
	EquippedBossDamageBonus = FMath::Clamp(EquippedBossDamageBonus, 0.0f, 4.0f);
	if (CultivationComponent)
	{
		CultivationComponent->SetEquipmentRateMultiplier(1.0f + FMath::Clamp(EquippedCultivationGainBonus, 0.0f, 4.0f));
	}
	RecalculateArtifactBonuses();
	RecalculateTechniqueBonuses();
	RecalculateCharacterPathBonuses();

	const float NewMaxHealth = GetMaxHealth();
	if (CurrentHealth > 0.0f)
	{
		CurrentHealth = FMath::Clamp(CurrentHealth + FMath::Max(NewMaxHealth - PreviousMaxHealth, 0.0f), 0.0f, NewMaxHealth);
	}
	const float NewMaxMana = GetMaxMana();
	if (CurrentMana > 0.0f)
	{
		CurrentMana = FMath::Clamp(CurrentMana + FMath::Max(NewMaxMana - PreviousMaxMana, 0.0f), 0.0f, NewMaxMana);
	}
}

void AImmortalPlayerCharacter::RecalculateArtifactBonuses()
{
	ArtifactAttackMultiplier = 1.0f;
	ArtifactDefenseMultiplier = 1.0f;
	ArtifactHealthMultiplier = 1.0f;
	ArtifactAttackSpeedBonus = 0.0f;
	ArtifactCriticalChanceBonus = 0.0f;
	FImmortalArtifactItem Equipped;
	if (!GetEquippedArtifact(Equipped)) return;
	const FImmortalArtifactBonuses Bonuses = UImmortalArtifactLibrary::CalculateBonuses(Equipped);
	ArtifactAttackMultiplier = FMath::Max(Bonuses.AttackMultiplier, 0.01f);
	ArtifactDefenseMultiplier = FMath::Max(Bonuses.DefenseMultiplier, 0.01f);
	ArtifactHealthMultiplier = FMath::Max(Bonuses.HealthMultiplier, 0.01f);
	ArtifactAttackSpeedBonus = FMath::Max(Bonuses.AttackSpeedBonus, 0.0f);
	ArtifactCriticalChanceBonus = FMath::Max(Bonuses.CriticalChanceBonus, 0.0f);
}

void AImmortalPlayerCharacter::RecalculateTechniqueBonuses()
{
	TechniqueAttackMultiplier = 1.0f;
	TechniqueDefenseMultiplier = 1.0f;
	TechniqueHealthMultiplier = 1.0f;
	TechniqueAttackSpeedBonus = 0.0f;
	TechniqueCriticalChanceBonus = 0.0f;
	TechniqueCultivationRateMultiplier = 1.0f;
	for (const FName TechniqueId : EquippedTechniqueIds)
	{
		FImmortalTechniqueProgress Progress;
		if (!GetTechniqueProgress(TechniqueId, Progress)) continue;
		const FImmortalTechniqueBonuses Bonuses = UImmortalTechniqueLibrary::CalculateBonuses(Progress);
		TechniqueAttackMultiplier += FMath::Max(Bonuses.AttackMultiplier - 1.0f, 0.0f);
		TechniqueDefenseMultiplier += FMath::Max(Bonuses.DefenseMultiplier - 1.0f, 0.0f);
		TechniqueHealthMultiplier += FMath::Max(Bonuses.HealthMultiplier - 1.0f, 0.0f);
		TechniqueAttackSpeedBonus += FMath::Max(Bonuses.AttackSpeedBonus, 0.0f);
		TechniqueCriticalChanceBonus += FMath::Max(Bonuses.CriticalChanceBonus, 0.0f);
		TechniqueCultivationRateMultiplier += FMath::Max(Bonuses.CultivationRateMultiplier - 1.0f, 0.0f);
	}
	if (CultivationComponent)
	{
		CultivationComponent->SetTechniqueRateMultiplier(TechniqueCultivationRateMultiplier);
	}
}

void AImmortalPlayerCharacter::AwakenSpiritRootIfNeeded()
{
	if (SpiritRootState.IsAwakened()) return;
	SpiritRootState = UImmortalCharacterPathLibrary::GenerateRandomSpiritRoot();
	++CharacterBuildRevision;
	RecalculateCharacterPathBonuses();
	FImmortalSpiritRootDefinition Definition;
	UImmortalCharacterPathLibrary::GetSpiritRootDefinition(SpiritRootState.Root, Definition);
	BP_OnSpiritRootAwakened(SpiritRootState);
	UE_LOG(LogTemp, Display, TEXT("Spirit root awakened: %s | type %d | purity %.1f%% | cultivation x%.3f | pill x%.3f"),
		*Definition.DisplayName.ToString(), static_cast<int32>(SpiritRootState.Root), SpiritRootState.Purity * 100.0f,
		UImmortalCharacterPathLibrary::CalculateCultivationRateMultiplier(SpiritRootState), GetPillEffectMultiplier());
}

float AImmortalPlayerCharacter::GetElementDamageMultiplier(const EImmortalElementType Element) const
{
	float EquipmentElementBonus = 0.0f;
	switch (Element)
	{
	case EImmortalElementType::Fire: EquipmentElementBonus = EquippedFireDamageBonus; break;
	case EImmortalElementType::Thunder: EquipmentElementBonus = EquippedThunderDamageBonus; break;
	case EImmortalElementType::Ice: EquipmentElementBonus = EquippedIceDamageBonus; break;
	default: break;
	}
	return UImmortalCharacterPathLibrary::CalculateElementDamageMultiplier(SpiritRootState, Element)
		* (1.0f + FMath::Clamp(EquipmentElementBonus, 0.0f, 5.0f));
}

bool AImmortalPlayerCharacter::IsEquipmentCompatibleWithPath(const FImmortalEquipmentItem& Item) const
{
	return Item.IsValid() && UImmortalCharacterPathLibrary::IsEquipmentCompatible(CultivationPathState.Path, Item.Discipline);
}

bool AImmortalPlayerCharacter::ReconcileEquipmentForPath(
	const EImmortalCultivationPath NewPath,
	const bool bApplyChanges)
{
	TArray<FImmortalEquipmentItem> CandidateEquipped = EquippedItems;
	TArray<FImmortalEquipmentItem> CandidateInventory = InventoryItems;
	const int32 Capacity = FMath::Max(InventoryCapacity, 1);
	for (int32 EquippedIndex = CandidateEquipped.Num() - 1; EquippedIndex >= 0; --EquippedIndex)
	{
		FImmortalEquipmentItem& Equipped = CandidateEquipped[EquippedIndex];
		if (UImmortalCharacterPathLibrary::IsEquipmentCompatible(NewPath, Equipped.Discipline)) continue;
		int32 BestReplacement = INDEX_NONE;
		float BestPower = -1.0f;
		for (int32 InventoryIndex = 0; InventoryIndex < CandidateInventory.Num(); ++InventoryIndex)
		{
			const FImmortalEquipmentItem& Candidate = CandidateInventory[InventoryIndex];
			if (Candidate.Slot != Equipped.Slot
				|| !UImmortalCharacterPathLibrary::IsEquipmentCompatible(NewPath, Candidate.Discipline)) continue;
			const float Power = UImmortalEquipmentLibrary::CalculateEquipmentPower(Candidate);
			if (Power > BestPower)
			{
				BestPower = Power;
				BestReplacement = InventoryIndex;
			}
		}
		if (BestReplacement != INDEX_NONE)
		{
			Swap(Equipped, CandidateInventory[BestReplacement]);
		}
		else
		{
			if (CandidateInventory.Num() >= Capacity) return false;
			CandidateInventory.Add(Equipped);
			CandidateEquipped.RemoveAt(EquippedIndex);
		}
	}

	for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EImmortalEquipmentSlot::MAX); ++SlotIndex)
	{
		const EImmortalEquipmentSlot Slot = static_cast<EImmortalEquipmentSlot>(SlotIndex);
		if (CandidateEquipped.ContainsByPredicate([Slot](const FImmortalEquipmentItem& Item) { return Item.Slot == Slot; })) continue;
		int32 BestCandidate = INDEX_NONE;
		float BestPower = -1.0f;
		for (int32 InventoryIndex = 0; InventoryIndex < CandidateInventory.Num(); ++InventoryIndex)
		{
			const FImmortalEquipmentItem& Candidate = CandidateInventory[InventoryIndex];
			if (Candidate.Slot != Slot
				|| !UImmortalCharacterPathLibrary::IsEquipmentCompatible(NewPath, Candidate.Discipline)) continue;
			const float Power = UImmortalEquipmentLibrary::CalculateEquipmentPower(Candidate);
			if (Power > BestPower)
			{
				BestPower = Power;
				BestCandidate = InventoryIndex;
			}
		}
		if (BestCandidate != INDEX_NONE)
		{
			CandidateEquipped.Add(CandidateInventory[BestCandidate]);
			CandidateInventory.RemoveAt(BestCandidate);
		}
	}

	if (bApplyChanges)
	{
		EquippedItems = MoveTemp(CandidateEquipped);
		InventoryItems = MoveTemp(CandidateInventory);
		++EquipmentInventoryRevision;
		BP_OnInventoryChanged(InventoryItems.Num(), Capacity);
	}
	return true;
}

bool AImmortalPlayerCharacter::CanSelectCultivationPath(const EImmortalCultivationPath NewPath) const
{
	FImmortalCultivationPathDefinition Definition;
	if (!UImmortalCharacterPathLibrary::GetCultivationPathDefinition(NewPath, Definition)
		|| CultivationPathState.Path == NewPath) return false;
	const FImmortalCraftingCost Cost = UImmortalCharacterPathLibrary::GetPathSwitchCost(CultivationPathState, NewPath);
	if (!UImmortalCraftingLibrary::CanAfford(MaterialInventory, CurrentGold, Cost)) return false;
	return const_cast<AImmortalPlayerCharacter*>(this)->ReconcileEquipmentForPath(NewPath, false);
}

FImmortalCharacterPathOperationResult AImmortalPlayerCharacter::SelectCultivationPath(
	const EImmortalCultivationPath NewPath)
{
	FImmortalCharacterPathOperationResult Result;
	Result.Path = NewPath;
	FImmortalCultivationPathDefinition Definition;
	if (!UImmortalCharacterPathLibrary::GetCultivationPathDefinition(NewPath, Definition))
	{
		Result.Message = FText::FromString(TEXT("无效的修炼流派"));
		return Result;
	}
	if (CultivationPathState.Path == NewPath)
	{
		Result.Message = FText::FromString(TEXT("当前已经修炼该流派"));
		return Result;
	}
	const FImmortalCraftingCost Cost = UImmortalCharacterPathLibrary::GetPathSwitchCost(CultivationPathState, NewPath);
	Result.bAffordable = UImmortalCraftingLibrary::CanAfford(MaterialInventory, CurrentGold, Cost);
	if (!Result.bAffordable)
	{
		Result.Message = FText::FromString(TEXT("转修所需灵石或材料不足"));
		return Result;
	}
	if (!ReconcileEquipmentForPath(NewPath, false))
	{
		Result.Message = FText::FromString(TEXT("背包空间不足，无法卸下与新流派不契合的装备"));
		return Result;
	}
	const bool bSwitching = CultivationPathState.IsSelected();
	if (bSwitching && !UImmortalCraftingLibrary::ConsumeCost(MaterialInventory, CurrentGold, Cost))
	{
		Result.Message = FText::FromString(TEXT("转修资源扣除失败"));
		return Result;
	}
	ReconcileEquipmentForPath(NewPath, true);
	CultivationPathState.Path = NewPath;
	if (bSwitching) ++CultivationPathState.SwitchCount;
	UImmortalCharacterPathLibrary::NormalizeCultivationPath(CultivationPathState);
	CultivationPathAttackCounter = 0;
	CultivationPathShield = 0.0f;
	++CharacterBuildRevision;
	++MaterialInventoryRevision;
	RecalculateEquipmentBonuses();
	if (GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)) StartAutoAttack();
	Result.bSucceeded = true;
	Result.bAffordable = true;
	Result.Message = FText::FromString(FString::Printf(TEXT("已选择%s%s，秘技·%s开始自动运转"),
		*Definition.DisplayName.ToString(), bSwitching ? TEXT("（转修）") : TEXT(""), *Definition.SkillName.ToString()));
	SaveProgress();
	BP_OnCultivationPathChanged(CultivationPathState);
	UE_LOG(LogTemp, Display, TEXT("Cultivation path selected: %d | switches %d | stones %d | equipment %d/%d | combat power %.2f"),
		static_cast<int32>(NewPath), CultivationPathState.SwitchCount, CurrentGold,
		EquippedItems.Num(), InventoryItems.Num(), GetCombatPower());
	return Result;
}

void AImmortalPlayerCharacter::RecalculateCharacterPathBonuses()
{
	const FImmortalCharacterPathBonuses Bonuses = UImmortalCharacterPathLibrary::CalculatePathBonuses(CultivationPathState);
	CharacterPathAttackMultiplier = FMath::Max(Bonuses.AttackMultiplier, 0.01f);
	CharacterPathDefenseMultiplier = FMath::Max(Bonuses.DefenseMultiplier, 0.01f);
	CharacterPathHealthMultiplier = FMath::Max(Bonuses.HealthMultiplier, 0.01f);
	CharacterPathManaMultiplier = FMath::Max(Bonuses.ManaMultiplier, 0.01f);
	CharacterPathAttackSpeedBonus = FMath::Max(Bonuses.AttackSpeedBonus, 0.0f);
	CharacterPathCriticalChanceBonus = FMath::Max(Bonuses.CriticalChanceBonus, 0.0f);
	CharacterPathDamageReduction = FMath::Clamp(Bonuses.DamageReduction, 0.0f, 0.75f);
	CharacterPathCultivationRateMultiplier =
		UImmortalCharacterPathLibrary::CalculateCultivationRateMultiplier(SpiritRootState)
		* FMath::Max(Bonuses.CultivationRateMultiplier, 0.0f);
	if (CultivationComponent)
	{
		CultivationComponent->SetCharacterPathRateMultiplier(CharacterPathCultivationRateMultiplier);
	}
}

void AImmortalPlayerCharacter::RecalculateAscensionBonuses()
{
	if (CultivationComponent)
	{
		CultivationComponent->SetAscensionRateMultiplier(
			GetAscensionCultivationMultiplier());
	}
}

void AImmortalPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Covers native window close / Alt+F4 as well as the in-game quit button.
	// Level transitions must not hide the persistent game window.
	if (EndPlayReason == EEndPlayReason::Quit || bSaveAndQuitRequested)
	{
		ImmortalDesktopWindow::PrepareForExit(GetWorld());
	}
	ImmortalDesktopWindow::Restore(GetWorld());
	if (!bSaveAndQuitRequested)
	{
		SaveProgress();
	}
	DespawnActivePetActor();
	StopAutoAttack();
	if (CultivationComponent)
	{
		CultivationComponent->StopCultivating();
	}
	GetWorldTimerManager().ClearTimer(TaskbarWindowTimerHandle);
	GetWorldTimerManager().ClearTimer(AutoReviveTimerHandle);
	GetWorldTimerManager().ClearTimer(MortalRealmAttackAnimationTimerHandle);
	GetWorldTimerManager().ClearTimer(MortalRealmHurtAnimationTimerHandle);
	GetWorldTimerManager().ClearTimer(DeathCultivationRedirectTimerHandle);
	GetWorldTimerManager().ClearTimer(DeathCultivationStartupTimerHandle);
	GetWorldTimerManager().ClearTimer(AscensionRealmDeathRecoveryTimerHandle);
	GetWorldTimerManager().ClearTimer(CultivationAutosaveTimerHandle);
	GetWorldTimerManager().ClearTimer(CultivationBreakthroughSaveTimerHandle);
	GetWorldTimerManager().ClearTimer(AlchemyBoostTimerHandle);
	GetWorldTimerManager().ClearTimer(ShopDailyRefreshTimerHandle);
	GetWorldTimerManager().ClearTimer(CaveProductionTimerHandle);
	bInventoryOpen = false;
	bAlchemyOpen = false;
	bCraftingOpen = false;
	bArtifactOpen = false;
	bTechniqueOpen = false;
	bCharacterBuildOpen = false;
	bShopOpen = false;
	bMapSelectionOpen = false;
	bQuestOpen = false;
	bCaveOpen = false;
	bFarmingOpen = false;
	bSectOpen = false;
	bWorldBossOpen = false;
	bEndlessDungeonOpen = false;
	bPetOpen = false;
	bAscensionOpen = false;
	bSettingsOpen = false;
	bManagementInterfaceOpen = false;
	ActiveManagementFeature = EImmortalManagementFeature::Home;
	if (PlayerCultivationWidget)
	{
		PlayerCultivationWidget->RemoveFromParent();
		PlayerCultivationWidget = nullptr;
	}
	if (PlayerInventoryWidget)
	{
		PlayerInventoryWidget->RemoveFromParent();
		PlayerInventoryWidget = nullptr;
	}
	if (PlayerAlchemyWidget)
	{
		PlayerAlchemyWidget->RemoveFromParent();
		PlayerAlchemyWidget = nullptr;
	}
	if (PlayerCraftingWidget)
	{
		PlayerCraftingWidget->RemoveFromParent();
		PlayerCraftingWidget = nullptr;
	}
	if (PlayerArtifactWidget)
	{
		PlayerArtifactWidget->RemoveFromParent();
		PlayerArtifactWidget = nullptr;
	}
	if (PlayerTechniqueWidget)
	{
		PlayerTechniqueWidget->RemoveFromParent();
		PlayerTechniqueWidget = nullptr;
	}
	if (PlayerCharacterBuildWidget)
	{
		PlayerCharacterBuildWidget->RemoveFromParent();
		PlayerCharacterBuildWidget = nullptr;
	}
	if (PlayerShopWidget)
	{
		PlayerShopWidget->RemoveFromParent();
		PlayerShopWidget = nullptr;
	}
	if (PlayerMapWidget)
	{
		PlayerMapWidget->RemoveFromParent();
		PlayerMapWidget = nullptr;
	}
	if (PlayerQuestWidget)
	{
		PlayerQuestWidget->RemoveFromParent();
		PlayerQuestWidget = nullptr;
	}
	if (PlayerCaveWidget)
	{
		PlayerCaveWidget->RemoveFromParent();
		PlayerCaveWidget = nullptr;
	}
	if (PlayerFarmingWidget)
	{
		PlayerFarmingWidget->RemoveFromParent();
		PlayerFarmingWidget = nullptr;
	}
	if (PlayerSectWidget)
	{
		PlayerSectWidget->RemoveFromParent();
		PlayerSectWidget = nullptr;
	}
	if (PlayerWorldBossWidget)
	{
		PlayerWorldBossWidget->RemoveFromParent();
		PlayerWorldBossWidget = nullptr;
	}
	if (PlayerEndlessDungeonWidget)
	{
		PlayerEndlessDungeonWidget->RemoveFromParent();
		PlayerEndlessDungeonWidget = nullptr;
	}
	if (PlayerPetWidget)
	{
		PlayerPetWidget->RemoveFromParent();
		PlayerPetWidget = nullptr;
	}
	if (PlayerAscensionWidget)
	{
		PlayerAscensionWidget->RemoveFromParent();
		PlayerAscensionWidget = nullptr;
	}
	if (PlayerSettingsWidget)
	{
		PlayerSettingsWidget->RemoveFromParent();
		PlayerSettingsWidget = nullptr;
	}
	if (PlayerManagementWidget)
	{
		PlayerManagementWidget->RemoveFromParent();
		PlayerManagementWidget = nullptr;
	}
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->RemoveFromParent();
		CombatFeedbackWidget = nullptr;
	}
	if (PlayerStatusWidget)
	{
		PlayerStatusWidget->RemoveFromParent();
		PlayerStatusWidget = nullptr;
	}
	if (DesktopGroundWidget)
	{
		DesktopGroundWidget->RemoveFromParent();
		DesktopGroundWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AImmortalPlayerCharacter::LoadMortalRealmAnimationSet()
{
	if (!bUseMortalRealmAnimationSet)
	{
		return;
	}

	MortalRealmIdleFlipbook = MortalRealmIdleFlipbookAsset.LoadSynchronous();
	MortalRealmMoveFlipbook = MortalRealmMoveFlipbookAsset.LoadSynchronous();
	MortalRealmAttackFlipbook = MortalRealmAttackFlipbookAsset.LoadSynchronous();
	MortalRealmHurtFlipbook = MortalRealmHurtFlipbookAsset.LoadSynchronous();
	MortalRealmDeathFlipbook = MortalRealmDeathFlipbookAsset.LoadSynchronous();
	bMortalRealmOneShotAnimation = false;
	ApplyMortalRealmSpritePresentation();
	UpdateMortalRealmLocomotionAnimation();

	const TCHAR* IdleState = MortalRealmIdleFlipbook ? TEXT("loaded") : TEXT("missing");
	const TCHAR* MoveState = MortalRealmMoveFlipbook ? TEXT("loaded") : TEXT("missing");
	const TCHAR* AttackState = MortalRealmAttackFlipbook ? TEXT("loaded") : TEXT("missing");
	const TCHAR* HurtState = MortalRealmHurtFlipbook ? TEXT("loaded") : TEXT("missing");
	const TCHAR* DeathState = MortalRealmDeathFlipbook ? TEXT("loaded") : TEXT("missing");
	if (MortalRealmIdleFlipbook && MortalRealmMoveFlipbook
		&& MortalRealmAttackFlipbook && MortalRealmHurtFlipbook
		&& MortalRealmDeathFlipbook)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Mortal Realm player animation set: idle=%s move=%s attack=%s hurt=%s death=%s"),
			IdleState,
			MoveState,
			AttackState,
			HurtState,
			DeathState);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Mortal Realm player animation set: idle=%s move=%s attack=%s hurt=%s death=%s"),
			IdleState,
			MoveState,
			AttackState,
			HurtState,
			DeathState);
	}
}

void AImmortalPlayerCharacter::ApplyMortalRealmSpritePresentation()
{
	UPaperFlipbookComponent* SpriteComponent = GetSprite();
	if (!SpriteComponent)
	{
		return;
	}

	const FVector PreviousScale = SpriteComponent->GetRelativeScale3D();
	const FVector PreviousLocation = SpriteComponent->GetRelativeLocation();
	float VisualScaleMultiplier = MortalRealmVisualScaleMultiplier;
	float GroundOffset = MortalRealmGroundOffset;
#if !UE_BUILD_SHIPPING
	FParse::Value(
		FCommandLine::Get(),
		TEXT("ImmortalTestMortalPlayerVisualScale="),
		VisualScaleMultiplier);
	FParse::Value(
		FCommandLine::Get(),
		TEXT("ImmortalTestMortalPlayerGroundOffset="),
		GroundOffset);
#endif
	VisualScaleMultiplier = FMath::Max(VisualScaleMultiplier, 0.1f);
	SpriteComponent->SetRelativeScale3D(PreviousScale * VisualScaleMultiplier);

	FVector GroundedLocation = PreviousLocation;
	float CapsuleHalfHeight = 0.0f;
	if (bGroundMortalRealmSpriteAtCapsuleBottom && GetCapsuleComponent())
	{
		CapsuleHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		GroundedLocation.Z = -CapsuleHalfHeight + GroundOffset;
		SpriteComponent->SetRelativeLocation(GroundedLocation);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Mortal player sprite presentation: scale %s -> %s location %s -> %s capsuleHalfHeight=%.1f multiplier=%.2f groundOffset=%.1f"),
		*PreviousScale.ToCompactString(),
		*SpriteComponent->GetRelativeScale3D().ToCompactString(),
		*PreviousLocation.ToCompactString(),
		*SpriteComponent->GetRelativeLocation().ToCompactString(),
		CapsuleHalfHeight,
		VisualScaleMultiplier,
		GroundOffset);
}

void AImmortalPlayerCharacter::UpdateMortalRealmLocomotionAnimation()
{
	if (!bUseMortalRealmAnimationSet || bDead || bMortalRealmOneShotAnimation)
	{
		return;
	}

	UPaperFlipbookComponent* SpriteComponent = GetSprite();
	UPaperFlipbook* DesiredFlipbook =
		GetVelocity().SizeSquared2D() > FMath::Square(5.0f)
			? MortalRealmMoveFlipbook.Get()
			: MortalRealmIdleFlipbook.Get();
	if (!SpriteComponent || !DesiredFlipbook)
	{
		return;
	}

	if (SpriteComponent->GetFlipbook() != DesiredFlipbook
		|| !SpriteComponent->IsLooping())
	{
		PlayMortalRealmFlipbook(DesiredFlipbook, true);
	}
	else if (!SpriteComponent->IsPlaying())
	{
		SpriteComponent->Play();
	}
}

void AImmortalPlayerCharacter::PlayMortalRealmAttackAnimation()
{
	if (!bUseMortalRealmAnimationSet || !MortalRealmAttackFlipbook || !GetWorld())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(MortalRealmAttackAnimationTimerHandle);
	GetWorldTimerManager().ClearTimer(MortalRealmHurtAnimationTimerHandle);
	bMortalRealmOneShotAnimation = true;
	PlayMortalRealmFlipbook(MortalRealmAttackFlipbook, false);
	GetWorldTimerManager().SetTimer(
		MortalRealmAttackAnimationTimerHandle,
		this,
		&AImmortalPlayerCharacter::FinishMortalRealmOneShotAnimation,
		GetMortalRealmFlipbookDuration(MortalRealmAttackFlipbook, 0.67f),
		false);
}

void AImmortalPlayerCharacter::PlayMortalRealmHurtAnimation()
{
	if (!bUseMortalRealmAnimationSet || !MortalRealmHurtFlipbook || !GetWorld())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(MortalRealmAttackAnimationTimerHandle);
	GetWorldTimerManager().ClearTimer(MortalRealmHurtAnimationTimerHandle);
	bMortalRealmOneShotAnimation = true;
	PlayMortalRealmFlipbook(MortalRealmHurtFlipbook, false);
	GetWorldTimerManager().SetTimer(
		MortalRealmHurtAnimationTimerHandle,
		this,
		&AImmortalPlayerCharacter::FinishMortalRealmOneShotAnimation,
		GetMortalRealmFlipbookDuration(MortalRealmHurtFlipbook, 0.5f),
		false);
}

void AImmortalPlayerCharacter::PlayMortalRealmDeathAnimation()
{
	if (!bUseMortalRealmAnimationSet || !MortalRealmDeathFlipbook)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(MortalRealmAttackAnimationTimerHandle);
	GetWorldTimerManager().ClearTimer(MortalRealmHurtAnimationTimerHandle);
	bMortalRealmOneShotAnimation = true;
	PlayMortalRealmFlipbook(MortalRealmDeathFlipbook, false);
}

void AImmortalPlayerCharacter::FinishMortalRealmOneShotAnimation()
{
	GetWorldTimerManager().ClearTimer(MortalRealmAttackAnimationTimerHandle);
	GetWorldTimerManager().ClearTimer(MortalRealmHurtAnimationTimerHandle);
	if (bDead)
	{
		return;
	}

	bMortalRealmOneShotAnimation = false;
	UpdateMortalRealmLocomotionAnimation();
}

void AImmortalPlayerCharacter::PlayMortalRealmFlipbook(
	UPaperFlipbook* Flipbook,
	const bool bLooping)
{
	UPaperFlipbookComponent* SpriteComponent = GetSprite();
	if (!SpriteComponent || !Flipbook)
	{
		return;
	}

	SpriteComponent->SetFlipbook(Flipbook);
	SpriteComponent->SetLooping(bLooping);
	SpriteComponent->PlayFromStart();
}

float AImmortalPlayerCharacter::GetMortalRealmFlipbookDuration(
	UPaperFlipbook* Flipbook,
	const float FallbackDuration) const
{
	return Flipbook
		? FMath::Max(Flipbook->GetTotalDuration(), 0.01f)
		: FMath::Max(FallbackDuration, 0.01f);
}

void AImmortalPlayerCharacter::BeginDeathCultivationRecovery()
{
	const bool bWasAlreadyRequired =
		bDeathCultivationRecoveryRequired;
	bDeathCultivationRecoveryRequired = true;
	bAdventureSuspendedForDeathRecovery = true;
	StopAutoAttack();

	if (!bWasAlreadyRequired && !SaveProgress())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Death cultivation recovery could not be persisted; current session remains locked"));
	}

	if (GetWorld())
	{
		GetWorldTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(
				this,
				[this] { SuspendAdventureForDeathRecovery(); }));
		GetWorldTimerManager().ClearTimer(
			DeathCultivationRedirectTimerHandle);
		GetWorldTimerManager().SetTimer(
			DeathCultivationRedirectTimerHandle,
			this,
			&AImmortalPlayerCharacter::RedirectToCultivationAfterDeath,
			GetMortalRealmFlipbookDuration(
				MortalRealmDeathFlipbook, 0.8f) + 0.10f,
			false);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Player death closed adventure; cultivation breakthrough is now required"));
}

void AImmortalPlayerCharacter::SuspendAdventureForDeathRecovery()
{
	bAdventureSuspendedForDeathRecovery = true;
	StopAutoAttack();
	if (AImmortalMonsterSpawner* Spawner = FindMapSpawner())
	{
		Spawner->SuspendAdventureForCultivation();
	}
}

void AImmortalPlayerCharacter::RedirectToCultivationAfterDeath()
{
	if (!bDeathCultivationRecoveryRequired
		&& !bAdventureSuspendedForDeathRecovery)
	{
		return;
	}
	OpenManagementFeature(
		EImmortalManagementFeature::Cultivation);
	QueueManagementNotification(
		FText::FromString(
			bDeathCultivationRecoveryRequired
				? TEXT("历练失败，历练通道已关闭。完成下一次修炼突破后可重返历练。")
				: TEXT("离线修炼已完成恢复。点击“重返历练”继续自动战斗。")),
		bDeathCultivationRecoveryRequired
			? FLinearColor(1.0f, 0.58f, 0.32f, 1.0f)
			: FLinearColor(0.48f, 1.0f, 0.62f, 1.0f),
		5.0f);
	if (PlayerCultivationWidget)
	{
		PlayerCultivationWidget->RefreshFromPlayer();
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Death recovery redirected player to cultivation: managementOpen=%s activeFeature=%d"),
		bManagementInterfaceOpen ? TEXT("true") : TEXT("false"),
		static_cast<int32>(ActiveManagementFeature));
}

void AImmortalPlayerCharacter::UnlockDeathCultivationRecovery(
	const TCHAR* Reason)
{
	if (!bDeathCultivationRecoveryRequired)
	{
		return;
	}

	bDeathCultivationRecoveryRequired = false;
	if (!SaveProgress())
	{
		bDeathCultivationRecoveryRequired = true;
		QueueManagementNotification(
			FText::FromString(TEXT(
				"修炼恢复已完成，但存档写入失败；历练通道仍保持关闭。")),
			FLinearColor(1.0f, 0.40f, 0.30f, 1.0f),
			5.0f);
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Death cultivation recovery unlock rolled back because persistence failed: reason=%s"),
			Reason ? Reason : TEXT("unknown"));
		return;
	}

	QueueManagementNotification(
		FText::FromString(TEXT(
			"修炼恢复完成，历练通道已解锁。点击“重返历练”继续自动战斗。")),
		FLinearColor(0.48f, 1.0f, 0.62f, 1.0f),
		5.0f);
	if (PlayerCultivationWidget)
	{
		PlayerCultivationWidget->RefreshFromPlayer();
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Death cultivation recovery unlocked: reason=%s awaitingManualReturn=%s"),
		Reason ? Reason : TEXT("unknown"),
		bAdventureSuspendedForDeathRecovery
			? TEXT("true") : TEXT("false"));
}

void AImmortalPlayerCharacter::ResumeAdventureAfterDeathRecovery()
{
	if (bDeathCultivationRecoveryRequired
		|| !bAdventureSuspendedForDeathRecovery)
	{
		return;
	}

	bool bSpawnerResumed = false;
	if (AImmortalMonsterSpawner* Spawner = FindMapSpawner())
	{
		bSpawnerResumed =
			Spawner->ResumeAdventureAfterCultivation();
	}
	bAdventureSuspendedForDeathRecovery = false;
	if (!bDead && bAutoAttackOnBeginPlay)
	{
		StartAutoAttack();
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Player returned to adventure after cultivation recovery: spawnerResumed=%s autoAttackActive=%s"),
		bSpawnerResumed ? TEXT("true") : TEXT("false"),
		GetWorldTimerManager().IsTimerActive(AutoAttackTimerHandle)
			? TEXT("true") : TEXT("false"));
}

void AImmortalPlayerCharacter::ApplyPersistedDeathCultivationRecovery()
{
	if (!bAdventureSuspendedForDeathRecovery)
	{
		return;
	}
	SuspendAdventureForDeathRecovery();
	RedirectToCultivationAfterDeath();
	if (CultivationComponent
		&& CultivationComponent->HasReachedAscension()
		&& GetWorld())
	{
		GetWorldTimerManager().SetTimer(
			AscensionRealmDeathRecoveryTimerHandle,
			FTimerDelegate::CreateWeakLambda(
				this,
				[this]
				{
					UnlockDeathCultivationRecovery(
						TEXT("ascension-realm meditation"));
				}),
			FMath::Max(AutoReviveDelay, 1.0f),
			false);
	}
}

void AImmortalPlayerCharacter::AutoRevive()
{
	if (!bDead || !GetWorld())
	{
		return;
	}

	SetActorLocation(InitialSpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
	bDead = false;
	CurrentHealth = GetMaxHealth();
	CurrentMana = GetMaxMana();
	InvulnerableUntilTime = GetWorld()->GetTimeSeconds() + FMath::Max(ReviveInvulnerabilityDuration, 0.0f);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (bDeathCultivationRecoveryRequired
		&& CultivationComponent
		&& CultivationComponent->HasReachedAscension())
	{
		UnlockDeathCultivationRecovery(
			TEXT("ascension-realm meditation"));
	}
	if (!bDeathCultivationRecoveryRequired
		&& !bAdventureSuspendedForDeathRecovery)
	{
		StartAutoAttack();
	}
	BP_OnPlayerAutoRevived();
	FinishMortalRealmOneShotAnimation();
	UE_LOG(LogTemp, Display, TEXT("Player auto-revived with %.1f seconds of protection"), FMath::Max(ReviveInvulnerabilityDuration, 0.0f));
	SaveProgress();
}

void AImmortalPlayerCharacter::StartAutoAttack()
{
	if (!GetWorld() || bDead
		|| bDeathCultivationRecoveryRequired
		|| bAdventureSuspendedForDeathRecovery)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		AutoAttackTimerHandle,
		this,
		&AImmortalPlayerCharacter::TryAutoAttack,
		GetEffectiveAttackInterval(),
		true,
		0.05f);
}

void AImmortalPlayerCharacter::StopAutoAttack()
{
	GetWorldTimerManager().ClearTimer(AutoAttackTimerHandle);
	GetWorldTimerManager().ClearTimer(AttackWindupTimerHandle);
	CurrentAttackTarget.Reset();
	bAttackPending = false;
}

void AImmortalPlayerCharacter::TryAutoAttack()
{
	if (bDead || bDeathCultivationRecoveryRequired
		|| bAdventureSuspendedForDeathRecovery
		|| bAttackPending || !GetWorld())
	{
		return;
	}

	CurrentAttackTarget = FindNearestTarget();
	AActor* Target = CurrentAttackTarget.Get();
	if (!Target)
	{
		return;
	}

	bAttackPending = true;
	BP_OnAutoAttackStarted(Target);
	PlayMortalRealmAttackAnimation();

	if (AttackWindup <= 0.0f)
	{
		ResolvePendingAttack();
		return;
	}

	GetWorldTimerManager().SetTimer(
		AttackWindupTimerHandle,
		this,
		&AImmortalPlayerCharacter::ResolvePendingAttack,
		AttackWindup,
		false);
}

AActor* AImmortalPlayerCharacter::FindNearestTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ImmortalPathAutoAttack), false, this);
	World->OverlapMultiByObjectType(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(AttackRange),
		QueryParams);

	if (bDrawAttackRange)
	{
		DrawDebugSphere(World, GetActorLocation(), AttackRange, 32, FColor::Cyan, false, AttackInterval);
	}

	AActor* NearestTarget = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!IsTargetAttackable(Candidate, true))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), GetAutoAttackLocation(Candidate));
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestTarget = Candidate;
		}
	}

	return NearestTarget;
}

bool AImmortalPlayerCharacter::IsTargetAttackable(const AActor* Target, const bool bCheckRange) const
{
	if (!IsValid(Target) || Target == this || Target->IsActorBeingDestroyed())
	{
		return false;
	}

	bool bCanAttack = false;
	if (Target->Implements<UAutoAttackTarget>())
	{
		bCanAttack = IAutoAttackTarget::Execute_CanBeAutoAttacked(Target);
	}
	else
	{
		// This fallback lets a temporary Blueprint target work before the C++
		// monster base is added in the next development step.
		bCanAttack = Target->ActorHasTag(TEXT("Monster"));
	}

	if (!bCanAttack || !bCheckRange)
	{
		return bCanAttack;
	}

	return FVector::DistSquared(GetActorLocation(), GetAutoAttackLocation(Target)) <= FMath::Square(AttackRange);
}

FVector AImmortalPlayerCharacter::GetAutoAttackLocation(const AActor* Target) const
{
	if (Target && Target->Implements<UAutoAttackTarget>())
	{
		return IAutoAttackTarget::Execute_GetAutoAttackTargetLocation(Target);
	}

	return Target ? Target->GetActorLocation() : FVector::ZeroVector;
}

float AImmortalPlayerCharacter::ApplyOutgoingDamage(AActor* Target, const float RequestedDamage)
{
	if (!IsTargetAttackable(Target, false) || RequestedDamage <= 0.0f) return 0.0f;
	float DamageMultiplier = 1.0f + FMath::Clamp(EquipmentFinalDamageBonus, 0.0f, 3.0f);
	if (const AImmortalMonsterCharacter* Monster = Cast<AImmortalMonsterCharacter>(Target); Monster && Monster->IsBoss())
	{
		DamageMultiplier *= 1.0f + FMath::Clamp(EquippedBossDamageBonus, 0.0f, 4.0f);
	}
	const float AppliedDamage = UGameplayStatics::ApplyDamage(
		Target,
		FMath::Max(RequestedDamage * DamageMultiplier, 0.0f),
		GetController(),
		this,
		DamageTypeClass);
	if (AppliedDamage > 0.0f && EquippedLifeStealBonus > 0.0f && !bDead)
	{
		CurrentHealth = FMath::Clamp(
			CurrentHealth + AppliedDamage * FMath::Clamp(EquippedLifeStealBonus, 0.0f, 0.75f),
			0.0f,
			GetMaxHealth());
	}
	return AppliedDamage;
}

void AImmortalPlayerCharacter::ResolvePendingAttack()
{
	AActor* Target = CurrentAttackTarget.Get();
	float DamageDealt = 0.0f;

	if (IsTargetAttackable(Target, true))
	{
		// Count a resolved attack against a valid target as a hit before applying the
		// base strike. Automatic technique/artifact skills fire first so they remain
		// meaningful when an overpowered idle character would otherwise one-shot every monster.
		TryTriggerCultivationPathSkill(Target);
		TryTriggerEquippedTechniques(Target);
		TryTriggerEquippedArtifact(Target);
		const bool bCriticalHit = FMath::FRand() < GetTotalCriticalChance();
		const float CriticalMultiplier = bCriticalHit ? GetTotalCriticalDamageMultiplier() : 1.0f;
		const float RequestedDamage = FMath::Max(GetTotalAttackDamage(), 0.0f) * CriticalMultiplier;
		DamageDealt = ApplyOutgoingDamage(Target, RequestedDamage);
		if (DamageDealt > 0.0f && CombatFeedbackWidget)
		{
			CombatFeedbackWidget->ShowDamage(Target->GetActorLocation() + FVector(0.0f, 0.0f, 115.0f), DamageDealt, bCriticalHit, false);
		}
		if (bCriticalHit && DamageDealt > 0.0f)
		{
			BP_OnPlayerCriticalHit(Target, DamageDealt);
		}
	}

	BP_OnAutoAttackResolved(Target, DamageDealt);
	bAttackPending = false;
	CurrentAttackTarget.Reset();
}

void AImmortalPlayerCharacter::TryTriggerEquippedArtifact(AActor* PrimaryTarget)
{
	FImmortalArtifactItem Equipped;
	FImmortalArtifactDefinition Definition;
	if (!GetEquippedArtifact(Equipped)
		|| !UImmortalArtifactLibrary::GetArtifactDefinition(Equipped.ArtifactId, Definition))
	{
		ArtifactAttackCounter = 0;
		return;
	}

	++ArtifactAttackCounter;
	const int32 TriggerCount = UImmortalArtifactLibrary::CalculateTriggerAttackCount(Equipped);
	if (ArtifactAttackCounter < TriggerCount) return;
	ArtifactAttackCounter = 0;
	const float Magnitude = UImmortalArtifactLibrary::CalculateActiveMagnitude(Equipped);
	AActor* EffectiveTarget = IsTargetAttackable(PrimaryTarget, false) ? PrimaryTarget : FindNearestTarget();
	float TotalEffect = 0.0f;

	auto ApplyArtifactDamage = [this, &TotalEffect](AActor* DamageTarget, const float RequestedDamage)
	{
		if (!IsTargetAttackable(DamageTarget, false)) return;
		const float Applied = ApplyOutgoingDamage(DamageTarget, FMath::Max(RequestedDamage, 0.0f));
		if (Applied <= 0.0f) return;
		TotalEffect += Applied;
		if (CombatFeedbackWidget)
		{
			CombatFeedbackWidget->ShowDamage(
				DamageTarget->GetActorLocation() + FVector(0.0f, 0.0f, 145.0f), Applied, true, false);
		}
	};

	switch (Definition.ActiveEffect)
	{
	case EImmortalArtifactActiveEffect::AreaDamage:
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionObjectQueryParams ObjectQuery;
		ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
		ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ImmortalArtifactArea), false, this);
		GetWorld()->OverlapMultiByObjectType(
			Overlaps, GetActorLocation(), FQuat::Identity, ObjectQuery,
			FCollisionShape::MakeSphere(FMath::Max(AttackRange * 1.75f, 300.0f)), QueryParams);
		TSet<AActor*> DamagedTargets;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Candidate = Overlap.GetActor();
			if (!Candidate || DamagedTargets.Contains(Candidate)) continue;
			DamagedTargets.Add(Candidate);
			ApplyArtifactDamage(Candidate, GetTotalAttackDamage() * Magnitude);
		}
		break;
	}
	case EImmortalArtifactActiveEffect::HealAndShield:
	{
		const float RestoreAmount = GetMaxHealth() * Magnitude;
		const float HealthBefore = CurrentHealth;
		CurrentHealth = FMath::Clamp(CurrentHealth + RestoreAmount, 0.0f, GetMaxHealth());
		const float ShieldBefore = ArtifactShield;
		ArtifactShield = FMath::Clamp(ArtifactShield + RestoreAmount, 0.0f, GetMaxHealth() * 0.4f);
		TotalEffect = (CurrentHealth - HealthBefore) + (ArtifactShield - ShieldBefore);
		break;
	}
	case EImmortalArtifactActiveEffect::ChaosBurst:
	case EImmortalArtifactActiveEffect::DirectDamage:
	default:
		ApplyArtifactDamage(EffectiveTarget, GetTotalAttackDamage() * Magnitude);
		break;
	}

	ShowBossMessage(
		FText::FromString(FString::Printf(TEXT("法宝·%s 触发"), *Definition.DisplayName.ToString())),
		UImmortalArtifactLibrary::GetQualityColor(Definition.Quality));
	BP_OnArtifactSkillTriggered(Equipped.ArtifactId, Definition.ActiveEffect, EffectiveTarget, TotalEffect);
	UE_LOG(LogTemp, Display, TEXT("Artifact active triggered: %s | effect %d | level %d | stars %d | magnitude %.3f | applied %.2f | shield %.2f"),
		*Equipped.ArtifactId.ToString(), static_cast<int32>(Definition.ActiveEffect), Equipped.Level, Equipped.Stars,
		Magnitude, TotalEffect, ArtifactShield);
}

void AImmortalPlayerCharacter::TryTriggerEquippedTechniques(AActor* PrimaryTarget)
{
	for (const FName TechniqueId : EquippedTechniqueIds)
	{
		FImmortalTechniqueProgress Progress;
		FImmortalTechniqueDefinition Definition;
		if (!GetTechniqueProgress(TechniqueId, Progress)
			|| !UImmortalTechniqueLibrary::GetTechniqueDefinition(TechniqueId, Definition)) continue;
		int32& AttackCounter = TechniqueAttackCounters.FindOrAdd(TechniqueId);
		++AttackCounter;
		const int32 TriggerCount = UImmortalTechniqueLibrary::CalculateTriggerAttackCount(Progress);
		if (AttackCounter < TriggerCount) continue;
		AttackCounter = 0;
		int32& ActiveCounter = TechniqueActiveCounters.FindOrAdd(TechniqueId);
		++ActiveCounter;
		const int32 UltimateCount = UImmortalTechniqueLibrary::CalculateUltimateActiveCount(Progress);
		const bool bUltimate = ActiveCounter >= UltimateCount;
		if (bUltimate) ActiveCounter = 0;
		const float AppliedValue = ExecuteTechniqueSkill(Progress, PrimaryTarget, bUltimate);
		const FLinearColor MessageColor = bUltimate
			? FLinearColor(1.0f, 0.48f, 0.12f, 1.0f)
			: UImmortalTechniqueLibrary::GetQualityColor(Definition.Quality);
		const FString SkillBanner = bUltimate
			? FString::Printf(TEXT("终极·%s"), *Definition.DisplayName.ToString())
			: FString::Printf(TEXT("功法·%s"), *Definition.DisplayName.ToString());
		ShowBossMessage(FText::FromString(SkillBanner), MessageColor);
		BP_OnTechniqueSkillTriggered(TechniqueId, Definition.ActiveEffect, bUltimate, PrimaryTarget, AppliedValue);
		UE_LOG(LogTemp, Display, TEXT("Technique skill triggered: %s | effect %d | ultimate %s | level %d | rank %d | applied %.2f | active cycle %d/%d | shield %.2f"),
			*TechniqueId.ToString(), static_cast<int32>(Definition.ActiveEffect), bUltimate ? TEXT("true") : TEXT("false"),
			Progress.Level, Progress.BreakthroughRank, AppliedValue, ActiveCounter, UltimateCount, TechniqueShield);
	}
}

float AImmortalPlayerCharacter::ExecuteTechniqueSkill(
	const FImmortalTechniqueProgress& Technique,
	AActor* PrimaryTarget,
	const bool bUltimate)
{
	FImmortalTechniqueDefinition Definition;
	if (!GetWorld() || !UImmortalTechniqueLibrary::GetTechniqueDefinition(Technique.TechniqueId, Definition)) return 0.0f;
	const float Magnitude = bUltimate
		? UImmortalTechniqueLibrary::CalculateUltimateMagnitude(Technique)
		: UImmortalTechniqueLibrary::CalculateActiveMagnitude(Technique);
	const float ElementMultiplier = GetElementDamageMultiplier(Definition.Element);
	float TotalEffect = 0.0f;

	auto ApplyTechniqueDamage = [this, &TotalEffect](AActor* DamageTarget, const float RequestedDamage)
	{
		if (!IsTargetAttackable(DamageTarget, false)) return;
		const float Applied = ApplyOutgoingDamage(DamageTarget, FMath::Max(RequestedDamage, 0.0f));
		if (Applied <= 0.0f) return;
		TotalEffect += Applied;
		if (CombatFeedbackWidget)
		{
			CombatFeedbackWidget->ShowDamage(
				DamageTarget->GetActorLocation() + FVector(0.0f, 0.0f, 165.0f), Applied, true, false);
		}
	};

	if (Definition.ActiveEffect == EImmortalTechniqueActiveEffect::BreathRecovery)
	{
		const float HealthBefore = CurrentHealth;
		const float ManaBefore = CurrentMana;
		CurrentHealth = FMath::Clamp(CurrentHealth + GetMaxHealth() * Magnitude, 0.0f, GetMaxHealth());
		CurrentMana = FMath::Clamp(CurrentMana + GetMaxMana() * Magnitude, 0.0f, GetMaxMana());
		const float ShieldBefore = TechniqueShield;
		const float ShieldGain = GetMaxHealth() * (0.02f * Technique.SpecialPoints + (bUltimate ? Magnitude * 0.5f : 0.0f));
		TechniqueShield = FMath::Clamp(TechniqueShield + ShieldGain, 0.0f, GetMaxHealth() * 0.35f);
		return (CurrentHealth - HealthBefore) + (CurrentMana - ManaBefore) + (TechniqueShield - ShieldBefore);
	}

	TArray<AActor*> Targets;
	AActor* EffectivePrimary = IsTargetAttackable(PrimaryTarget, false) ? PrimaryTarget : FindNearestTarget();
	if (EffectivePrimary) Targets.Add(EffectivePrimary);
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ImmortalTechniqueArea), false, this);
	GetWorld()->OverlapMultiByObjectType(
		Overlaps, GetActorLocation(), FQuat::Identity, ObjectQuery,
		FCollisionShape::MakeSphere(FMath::Max(AttackRange * 2.1f, 360.0f)), QueryParams);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (IsTargetAttackable(Candidate, false)) Targets.AddUnique(Candidate);
	}
	Targets.Sort([this](const AActor& Left, const AActor& Right)
	{
		return FVector::DistSquared(GetActorLocation(), GetAutoAttackLocation(&Left))
			< FVector::DistSquared(GetActorLocation(), GetAutoAttackLocation(&Right));
	});

	switch (Definition.ActiveEffect)
	{
	case EImmortalTechniqueActiveEffect::SwordWave:
	{
		const float SpecialMultiplier = 1.0f + 0.12f * Technique.SpecialPoints;
		if (bUltimate)
		{
			for (AActor* Target : Targets) ApplyTechniqueDamage(Target, GetTotalAttackDamage() * Magnitude * SpecialMultiplier * ElementMultiplier);
		}
		else
		{
			ApplyTechniqueDamage(EffectivePrimary, GetTotalAttackDamage() * Magnitude * SpecialMultiplier * ElementMultiplier);
		}
		break;
	}
	case EImmortalTechniqueActiveEffect::FlameNova:
	{
		const float AfterFlameMultiplier = Technique.SpecialPoints >= 3 ? 1.45f : 1.0f;
		for (AActor* Target : Targets)
		{
			ApplyTechniqueDamage(Target, GetTotalAttackDamage() * Magnitude * AfterFlameMultiplier * ElementMultiplier);
		}
		break;
	}
	case EImmortalTechniqueActiveEffect::ChainLightning:
	{
		const int32 MaxTargets = bUltimate ? 6 + Technique.SpecialPoints : 2 + Technique.SpecialPoints;
		for (int32 Index = 0; Index < FMath::Min(Targets.Num(), MaxTargets); ++Index)
		{
			const float Falloff = FMath::Pow(0.86f, Index);
			ApplyTechniqueDamage(Targets[Index], GetTotalAttackDamage() * Magnitude * Falloff * ElementMultiplier);
		}
		break;
	}
	default:
		break;
	}
	return TotalEffect;
}

void AImmortalPlayerCharacter::TryTriggerCultivationPathSkill(AActor* PrimaryTarget)
{
	FImmortalCultivationPathDefinition Definition;
	if (!CultivationPathState.IsSelected()
		|| !UImmortalCharacterPathLibrary::GetCultivationPathDefinition(CultivationPathState.Path, Definition))
	{
		CultivationPathAttackCounter = 0;
		return;
	}
	++CultivationPathAttackCounter;
	if (CultivationPathAttackCounter < FMath::Max(Definition.AttacksPerSkill, 3)) return;
	CultivationPathAttackCounter = 0;
	const float AppliedValue = ExecuteCultivationPathSkill(Definition, PrimaryTarget);
	ShowBossMessage(
		FText::FromString(FString::Printf(TEXT("流派秘技·%s"), *Definition.SkillName.ToString())),
		UImmortalCharacterPathLibrary::GetCultivationPathColor(CultivationPathState.Path));
	BP_OnCultivationPathSkillTriggered(CultivationPathState.Path, Definition.SkillEffect, PrimaryTarget, AppliedValue);
	UE_LOG(LogTemp, Display, TEXT("Cultivation path skill triggered: path %d | effect %d | skill %s | applied %.2f | root element multiplier %.3f | shield %.2f"),
		static_cast<int32>(CultivationPathState.Path), static_cast<int32>(Definition.SkillEffect),
		*Definition.SkillName.ToString(), AppliedValue, GetElementDamageMultiplier(Definition.SkillElement), CultivationPathShield);
}

float AImmortalPlayerCharacter::ExecuteCultivationPathSkill(
	const FImmortalCultivationPathDefinition& Definition,
	AActor* PrimaryTarget)
{
	if (!GetWorld()) return 0.0f;
	EImmortalElementType EffectiveElement = Definition.SkillElement;
	if (Definition.SkillEffect == EImmortalPathSkillEffect::FiveElementsSpell
		&& EffectiveElement == EImmortalElementType::None)
	{
		FImmortalSpiritRootDefinition RootDefinition;
		if (UImmortalCharacterPathLibrary::GetSpiritRootDefinition(SpiritRootState.Root, RootDefinition))
		{
			EffectiveElement = RootDefinition.Element;
		}
	}
	const float ElementMultiplier = GetElementDamageMultiplier(EffectiveElement);
	const float TotalRequestedDamage = GetTotalAttackDamage() * FMath::Max(Definition.SkillMagnitude, 0.0f) * ElementMultiplier;
	float TotalEffect = 0.0f;

	auto ApplyPathDamage = [this, &TotalEffect](AActor* DamageTarget, const float RequestedDamage)
	{
		if (!IsTargetAttackable(DamageTarget, false)) return;
		const float Applied = ApplyOutgoingDamage(DamageTarget, FMath::Max(RequestedDamage, 0.0f));
		if (Applied <= 0.0f) return;
		TotalEffect += Applied;
		if (CombatFeedbackWidget)
		{
			CombatFeedbackWidget->ShowDamage(
				DamageTarget->GetActorLocation() + FVector(0.0f, 0.0f, 185.0f), Applied, true, false);
		}
	};

	AActor* EffectivePrimary = IsTargetAttackable(PrimaryTarget, false) ? PrimaryTarget : FindNearestTarget();
	TArray<AActor*> Targets;
	if (EffectivePrimary) Targets.Add(EffectivePrimary);
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ImmortalCultivationPathSkill), false, this);
	GetWorld()->OverlapMultiByObjectType(
		Overlaps, GetActorLocation(), FQuat::Identity, ObjectQuery,
		FCollisionShape::MakeSphere(FMath::Max(AttackRange * 2.2f, 380.0f)), QueryParams);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (IsTargetAttackable(Candidate, false)) Targets.AddUnique(Candidate);
	}
	Targets.Sort([this](const AActor& Left, const AActor& Right)
	{
		return FVector::DistSquared(GetActorLocation(), GetAutoAttackLocation(&Left))
			< FVector::DistSquared(GetActorLocation(), GetAutoAttackLocation(&Right));
	});

	switch (Definition.SkillEffect)
	{
	case EImmortalPathSkillEffect::BodyQuake:
	{
		for (AActor* Target : Targets) ApplyPathDamage(Target, TotalRequestedDamage);
		const float ShieldBefore = CultivationPathShield;
		CultivationPathShield = FMath::Clamp(
			CultivationPathShield + GetMaxHealth() * 0.08f, 0.0f, GetMaxHealth() * 0.30f);
		TotalEffect += CultivationPathShield - ShieldBefore;
		break;
	}
	case EImmortalPathSkillEffect::FiveElementsSpell:
		for (AActor* Target : Targets) ApplyPathDamage(Target, TotalRequestedDamage);
		CurrentMana = FMath::Clamp(CurrentMana + GetMaxMana() * 0.06f, 0.0f, GetMaxMana());
		break;
	case EImmortalPathSkillEffect::SwordArray:
		for (int32 Strike = 0; Strike < 3; ++Strike) ApplyPathDamage(EffectivePrimary, TotalRequestedDamage / 3.0f);
		break;
	case EImmortalPathSkillEffect::PoisonCloud:
	{
		for (AActor* Target : Targets)
		{
			ApplyPathDamage(Target, TotalRequestedDamage * 0.55f);
			const TWeakObjectPtr<AActor> WeakTarget(Target);
			const float TickDamage = TotalRequestedDamage * 0.225f;
			for (int32 TickIndex = 1; TickIndex <= 2; ++TickIndex)
			{
				FTimerHandle PoisonTimer;
				GetWorldTimerManager().SetTimer(
					PoisonTimer,
					FTimerDelegate::CreateWeakLambda(this, [this, WeakTarget, TickDamage]
					{
						AActor* PoisonedTarget = WeakTarget.Get();
						if (!IsTargetAttackable(PoisonedTarget, false)) return;
						const float Applied = ApplyOutgoingDamage(PoisonedTarget, TickDamage);
						if (Applied > 0.0f && CombatFeedbackWidget)
						{
							CombatFeedbackWidget->ShowDamage(
								PoisonedTarget->GetActorLocation() + FVector(0.0f, 0.0f, 180.0f), Applied, false, false);
						}
					}),
					0.45f * TickIndex,
					false);
			}
		}
		CurrentHealth = FMath::Clamp(CurrentHealth + TotalEffect * 0.05f, 0.0f, GetMaxHealth());
		break;
	}
	case EImmortalPathSkillEffect::ThunderChain:
		for (int32 Index = 0; Index < FMath::Min(Targets.Num(), 5); ++Index)
		{
			ApplyPathDamage(Targets[Index], TotalRequestedDamage * FMath::Pow(0.82f, Index));
		}
		break;
	default:
		break;
	}
	return TotalEffect;
}
