// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPlayerCharacter.h"

#include "../Combat/AutoAttackTarget.h"
#include "../Save/ImmortalPathSaveGame.h"
#include "../UI/ImmortalAlchemyWidget.h"
#include "../UI/ImmortalArtifactWidget.h"
#include "../UI/ImmortalCombatFeedbackWidget.h"
#include "../UI/ImmortalCraftingWidget.h"
#include "../UI/ImmortalCharacterBuildWidget.h"
#include "../UI/ImmortalInventoryWidget.h"
#include "../UI/ImmortalPlayerStatusWidget.h"
#include "../UI/ImmortalShopWidget.h"
#include "../UI/ImmortalTechniqueWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
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
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

AImmortalPlayerCharacter::AImmortalPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	DamageTypeClass = UDamageType::StaticClass();
	CultivationComponent = CreateDefaultSubobject<UImmortalCultivationComponent>(TEXT("CultivationComponent"));
}

void AImmortalPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	InventoryItems.Reset();
	EquippedItems.Reset();
	MaterialInventory.Reset();
	MaterialInventoryRevision = 0;
	PillInventory.Reset();
	PillInventoryRevision = 0;
	ArtifactInventory.Reset();
	EquippedArtifactInstanceId.Invalidate();
	ArtifactInventoryRevision = 0;
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
	ConfigureCombatCamera();
	ConfigureTaskbarWindow();

	if (APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PlayerStatusWidget = CreateWidget<UImmortalPlayerStatusWidget>(PlayerController, UImmortalPlayerStatusWidget::StaticClass());
		if (PlayerStatusWidget)
		{
			PlayerStatusWidget->InitializeForPlayer(this);
			PlayerStatusWidget->AddToViewport(10);
			PlayerStatusWidget->SetPositionInViewport(FVector2D(32.0f, 28.0f), false);
			PlayerStatusWidget->SetDesiredSizeInViewport(FVector2D(1220.0f, 64.0f));
		}

		PlayerInventoryWidget = CreateWidget<UImmortalInventoryWidget>(PlayerController, UImmortalInventoryWidget::StaticClass());
		if (PlayerInventoryWidget)
		{
			PlayerInventoryWidget->InitializeForPlayer(this);
			PlayerInventoryWidget->AddToViewport(100);
			PlayerInventoryWidget->SetDesiredSizeInViewport(FVector2D(900.0f, 600.0f));
			PlayerInventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
		}

		PlayerAlchemyWidget = CreateWidget<UImmortalAlchemyWidget>(PlayerController, UImmortalAlchemyWidget::StaticClass());
		if (PlayerAlchemyWidget)
		{
			PlayerAlchemyWidget->InitializeForPlayer(this);
			PlayerAlchemyWidget->AddToViewport(110);
			PlayerAlchemyWidget->SetDesiredSizeInViewport(FVector2D(900.0f, 600.0f));
			PlayerAlchemyWidget->SetVisibility(ESlateVisibility::Collapsed);
		}

		PlayerCraftingWidget = CreateWidget<UImmortalCraftingWidget>(PlayerController, UImmortalCraftingWidget::StaticClass());
		if (PlayerCraftingWidget)
		{
			PlayerCraftingWidget->InitializeForPlayer(this);
			PlayerCraftingWidget->AddToViewport(115);
			PlayerCraftingWidget->SetDesiredSizeInViewport(FVector2D(900.0f, 600.0f));
			PlayerCraftingWidget->SetVisibility(ESlateVisibility::Collapsed);
		}

		PlayerArtifactWidget = CreateWidget<UImmortalArtifactWidget>(PlayerController, UImmortalArtifactWidget::StaticClass());
		if (PlayerArtifactWidget)
		{
			PlayerArtifactWidget->InitializeForPlayer(this);
			PlayerArtifactWidget->AddToViewport(120);
			PlayerArtifactWidget->SetDesiredSizeInViewport(FVector2D(900.0f, 600.0f));
			PlayerArtifactWidget->SetVisibility(ESlateVisibility::Collapsed);
		}

		PlayerTechniqueWidget = CreateWidget<UImmortalTechniqueWidget>(PlayerController, UImmortalTechniqueWidget::StaticClass());
		if (PlayerTechniqueWidget)
		{
			PlayerTechniqueWidget->InitializeForPlayer(this);
			PlayerTechniqueWidget->AddToViewport(125);
			PlayerTechniqueWidget->SetDesiredSizeInViewport(FVector2D(900.0f, 600.0f));
			PlayerTechniqueWidget->SetVisibility(ESlateVisibility::Collapsed);
		}

		PlayerCharacterBuildWidget = CreateWidget<UImmortalCharacterBuildWidget>(PlayerController, UImmortalCharacterBuildWidget::StaticClass());
		if (PlayerCharacterBuildWidget)
		{
			PlayerCharacterBuildWidget->InitializeForPlayer(this);
			PlayerCharacterBuildWidget->AddToViewport(130);
			PlayerCharacterBuildWidget->SetDesiredSizeInViewport(FVector2D(900.0f, 600.0f));
			PlayerCharacterBuildWidget->SetVisibility(ESlateVisibility::Collapsed);
		}

		PlayerShopWidget = CreateWidget<UImmortalShopWidget>(PlayerController, UImmortalShopWidget::StaticClass());
		if (PlayerShopWidget)
		{
			PlayerShopWidget->InitializeForPlayer(this);
			PlayerShopWidget->AddToViewport(135);
			PlayerShopWidget->SetDesiredSizeInViewport(FVector2D(900.0f, 600.0f));
			PlayerShopWidget->SetVisibility(ESlateVisibility::Collapsed);
		}

		CombatFeedbackWidget = CreateWidget<UImmortalCombatFeedbackWidget>(PlayerController, UImmortalCombatFeedbackWidget::StaticClass());
		if (CombatFeedbackWidget)
		{
			CombatFeedbackWidget->InitializeForPlayer(this);
			CombatFeedbackWidget->AddToViewport(50);
			CombatFeedbackWidget->SetStageProgress(
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
		for (const FName MaterialId : { FName(TEXT("SpiritGrass")), FName(TEXT("DemonCore")), FName(TEXT("SpiritLiquid")), FName(TEXT("Ore")) })
		{
			AddMaterialInternal(MaterialId, 20);
		}
		++MaterialInventoryRevision;
		UE_LOG(LogTemp, Display, TEXT("Alchemy development materials granted: four recipe materials x20"));
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
			{ FName(TEXT("Ore")), FName(TEXT("DemonBone")), FName(TEXT("SpiritIron")), FName(TEXT("ArtifactFragment")) })
		{
			AddMaterialInternal(MaterialId, 50);
		}
		CurrentGold = FMath::Min(CurrentGold + 5000, MAX_int32);
		++MaterialInventoryRevision;
		BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, 0);
		UE_LOG(LogTemp, Display, TEXT("Crafting development resources granted: four materials x50 | spirit stones +5000"));
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
#endif

	if (bAutoAttackOnBeginPlay)
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
	}
}

void AImmortalPlayerCharacter::ToggleInventory()
{
	if (!PlayerInventoryWidget)
	{
		return;
	}

	const bool bWantsOpen = !bInventoryOpen;
	if (bWantsOpen) CloseAllModalWidgetsExcept(PlayerInventoryWidget);
	bInventoryOpen = bWantsOpen;
	PlayerInventoryWidget->SetVisibility(bInventoryOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bInventoryOpen) PlayerInventoryWidget->RefreshFromPlayer();
	ConfigureModalWidget(PlayerInventoryWidget, bInventoryOpen);
	if (bInventoryOpen)
	{
		UE_LOG(LogTemp, Display, TEXT("Inventory opened: equipped %d | backpack %d/%d | material types %d | pill stacks %d | combat power %.2f"),
			EquippedItems.Num(), InventoryItems.Num(), GetInventoryCapacity(), MaterialInventory.Num(), PillInventory.Num(), GetCombatPower());
	}
}

void AImmortalPlayerCharacter::ToggleAlchemy()
{
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

	PlayerController->bShowMouseCursor = bOpen;
	if (bOpen && Widget)
	{
		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
		const FVector2D InventorySize(900.0f, 600.0f);
		const FVector2D CentredPosition(
			FMath::Max((static_cast<float>(ViewportWidth) - InventorySize.X) * 0.5f, 0.0f),
			FMath::Max((static_cast<float>(ViewportHeight) - InventorySize.Y) * 0.5f, 0.0f));
		// Apply layout only after AddToViewport has registered the widget with
		// UE 5.7's GameViewportSubsystem; early consecutive setters can replace
		// one another while the viewport slot is still unmanaged.
		Widget->SetDesiredSizeInViewport(InventorySize);
		Widget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f));
		Widget->SetAlignmentInViewport(FVector2D::ZeroVector);
		Widget->SetPositionInViewport(CentredPosition, false);
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(Widget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
	}
	else
	{
		PlayerController->SetInputMode(FInputModeGameOnly());
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
	if (!RefreshShopForDay(CurrentDayKey, FMath::Max(DisplayedStage, 1), true)) return false;
	SaveProgress();
	return true;
}

void AImmortalPlayerCharacter::CheckDailyShopRefresh()
{
	EnsureDailyShopRefresh();
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
		return InventoryItems.Num() < FMath::Max(InventoryCapacity, 1);
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
		const float ExistingPower = EquippedIndex == INDEX_NONE ? -1.0f
			: UImmortalEquipmentLibrary::CalculateEquipmentPower(EquippedItems[EquippedIndex]);
		if (bAutoEquipNewItems && bCompatible
			&& UImmortalEquipmentLibrary::CalculateEquipmentPower(PurchasedItem) > ExistingPower)
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
		FMath::Max(DisplayedStage, 1), static_cast<int32>(GetCultivationRealm()), GetCultivationMinorStage(),
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
	return Item ? UImmortalShopLibrary::GetEquipmentSellPrice(*Item) : 0;
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
	const int32 Price = UImmortalShopLibrary::GetEquipmentSellPrice(SoldItem);
	if (Price <= 0)
	{
		Result.Message = FText::FromString(TEXT("该装备无法估价"));
		return Result;
	}
	const TArray<FImmortalEquipmentItem> PreviousInventory = InventoryItems;
	const int32 PreviousGold = CurrentGold;
	const int32 PreviousRevision = EquipmentInventoryRevision;
	InventoryItems.RemoveAt(Index);
	CurrentGold = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(CurrentGold) + Price, MAX_int32));
	++EquipmentInventoryRevision;
	if (!SaveProgress())
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
	if (!GEngine || !GEngine->GameViewport) return;
	const TSharedPtr<SWindow> GameWindow = GEngine->GameViewport->GetWindow();
	if (!GameWindow.IsValid() || !GameWindow->GetNativeWindow().IsValid()) return;

	HWND WindowHandle = static_cast<HWND>(GameWindow->GetNativeWindow()->GetOSWindowHandle());
	if (!WindowHandle) return;
	RECT WorkArea = {};
	if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &WorkArea, 0)) return;

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
		SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	UE_LOG(LogTemp, Display, TEXT("TBH taskbar window applied: %dx%d at %d,%d | topmost=%s"),
		Width, Height, WorkArea.left, Top, bTaskbarWindowAlwaysOnTop ? TEXT("true") : TEXT("false"));
	// The viewport scale changes after the native TBH window is resized. Reapply
	// the active modal's viewport geometry so it remains centered and fully visible.
	if (bInventoryOpen) ConfigureModalWidget(PlayerInventoryWidget, true);
	else if (bAlchemyOpen) ConfigureModalWidget(PlayerAlchemyWidget, true);
	else if (bCraftingOpen) ConfigureModalWidget(PlayerCraftingWidget, true);
	else if (bArtifactOpen) ConfigureModalWidget(PlayerArtifactWidget, true);
	else if (bTechniqueOpen) ConfigureModalWidget(PlayerTechniqueWidget, true);
	else if (bCharacterBuildOpen) ConfigureModalWidget(PlayerCharacterBuildWidget, true);
	else if (bShopOpen) ConfigureModalWidget(PlayerShopWidget, true);
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
	const float ReducedDamage = FMath::Max(
		AfterDefense * (1.0f - FMath::Clamp(CharacterPathDamageReduction, 0.0f, 0.75f)), 1.0f);
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
		* ArtifactHealthMultiplier * TechniqueHealthMultiplier * CharacterPathHealthMultiplier, 1.0f);
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
		* ArtifactAttackMultiplier * TechniqueAttackMultiplier * CharacterPathAttackMultiplier;
}

float AImmortalPlayerCharacter::GetTotalDefense() const
{
	const float CultivationBonus = CultivationComponent ? CultivationComponent->GetDefenseBonus() : 0.0f;
	return (Defense + EquippedDefenseBonus + CultivationBonus)
		* ArtifactDefenseMultiplier * TechniqueDefenseMultiplier * CharacterPathDefenseMultiplier;
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
		+ GetTotalCriticalChance() * 100.0f;
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
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->ShowRewards(PickupWorldLocation, 0, GrantedAmount);
	}
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, GrantedAmount);
	UE_LOG(LogTemp, Display, TEXT("Spirit stones collected: +%d | total %d"), GrantedAmount, CurrentGold);
	SaveProgress();
}

void AImmortalPlayerCharacter::UpdateStageProgress(
	const int32 Stage,
	const int32 Kills,
	const int32 RequiredKills,
	const bool bBossStage,
	const bool bMapCompleted)
{
	DisplayedStage = FMath::Clamp(Stage, 1, 999);
	DisplayedStageKills = FMath::Max(Kills, 0);
	DisplayedStageRequiredKills = FMath::Max(RequiredKills, 1);
	bDisplayedBossStage = bBossStage;
	bDisplayedMapCompleted = bMapCompleted;
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->SetStageProgress(
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
	Result.Outcome = UImmortalAlchemyLibrary::CalculateOutcome(Definition, Roll);
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

	SaveProgress();
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
	const bool bSaveAfter)
{
	if (!Item.IsValid())
	{
		return false;
	}

	++EquipmentDropCount;
	BP_OnEquipmentPickedUp(EquipmentDropCount, 1);

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
	bool bShouldEquip = bAutoEquipNewItems && bCompatible && (!bHasEquippedItem || NewPower > ExistingPower);
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
		if (bShowFeedback && !bDead && bAutoAttackOnBeginPlay)
		{
			StartAutoAttack();
		}
		UE_LOG(LogTemp, Display, TEXT("Equipment auto-equipped: %s | item power %.2f | combat power %.2f"),
			*Item.DisplayName.ToString(), NewPower, GetCombatPower());
		BP_OnEquipmentChanged(Item.Slot, Item, true, GetCombatPower());
		BP_OnInventoryChanged(InventoryItems.Num(), FMath::Max(InventoryCapacity, 1));
		if (bShowFeedback && CombatFeedbackWidget)
		{
			CombatFeedbackWidget->ShowEquipmentPickup(FText::FromName(Item.DisplayName), UImmortalEquipmentLibrary::GetQualityColor(Item.Quality), true);
		}
		if (bSaveAfter)
		{
			SaveProgress();
		}
		return true;
	}

	const bool bStored = AddItemToInventory(Item);
	if (bStored) ++EquipmentInventoryRevision;
	UE_LOG(LogTemp, Display, TEXT("Equipment stored=%s: %s | compatible %s | item power %.2f | backpack %d/%d"),
		bStored ? TEXT("true") : TEXT("false"), *Item.DisplayName.ToString(),
		bCompatible ? TEXT("true") : TEXT("false"), NewPower, InventoryItems.Num(), FMath::Max(InventoryCapacity, 1));
	BP_OnInventoryChanged(InventoryItems.Num(), FMath::Max(InventoryCapacity, 1));
	if (bStored && bShowFeedback && CombatFeedbackWidget)
	{
		CombatFeedbackWidget->ShowEquipmentPickup(FText::FromName(Item.DisplayName), UImmortalEquipmentLibrary::GetQualityColor(Item.Quality), false);
	}
	if (bStored && bSaveAfter)
	{
		SaveProgress();
	}
	return bStored;
}

bool AImmortalPlayerCharacter::SaveProgress()
{
	UImmortalPathSaveGame* SaveGame = UImmortalPathSaveGame::LoadOrCreate(this);
	if (!SaveGame)
	{
		return false;
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
	SaveGame->TechniqueLibrary = TechniqueLibrary;
	SaveGame->EquippedTechniqueIds = EquippedTechniqueIds;
	SaveGame->TechniqueInsightPoints = TechniqueInsightPoints;
	SaveGame->SpiritRootState = SpiritRootState;
	SaveGame->CultivationPathState = CultivationPathState;
	SaveGame->ShopState = ShopState;
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
		UE_LOG(LogTemp, Display, TEXT("Player progress saved: realm %s | cultivation %d/%d | spirit stones %d | equipped %d | backpack %d | material types %d | pill stacks %d | artifacts %d | artifact equipped %s | techniques %d/%d | insight %d | root %d/%.2f | path %d/switches %d | shop %d/%d/%d/%d | alchemy boost %.0fs"),
			*GetFullCultivationRealmName().ToString(), CurrentCultivation, GetRequiredCultivation(),
			CurrentGold, EquippedItems.Num(), InventoryItems.Num(), MaterialInventory.Num(), PillInventory.Num(),
			ArtifactInventory.Num(), EquippedArtifactInstanceId.IsValid() ? TEXT("true") : TEXT("false"),
			TechniqueLibrary.Num(), EquippedTechniqueIds.Num(), TechniqueInsightPoints,
			static_cast<int32>(SpiritRootState.Root), SpiritRootState.Purity,
			static_cast<int32>(CultivationPathState.Path), CultivationPathState.SwitchCount,
			ShopState.RefreshDayKey, ShopState.RefreshSerial, ShopState.Listings.Num(), SoldOutListings,
			GetAlchemyBoostRemainingSeconds());
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

	InventoryItems = SaveGame->InventoryItems;
	EquippedItems = SaveGame->EquippedItems;
	for (FImmortalEquipmentItem& Item : InventoryItems) UImmortalEquipmentLibrary::NormalizeForgingState(Item);
	for (FImmortalEquipmentItem& Item : EquippedItems) UImmortalEquipmentLibrary::NormalizeForgingState(Item);
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
	const int32 LoadedSaveVersion = SaveGame->SaveVersion;
	SpiritRootState = SaveGame->SpiritRootState;
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
	if (SaveGame->bHasStageData)
	{
		DisplayedStage = FMath::Clamp(SaveGame->QingyunStage, 1, 999);
		DisplayedStageKills = FMath::Max(SaveGame->QingyunStageKills, 0);
		bDisplayedMapCompleted = SaveGame->bQingyunMountainCompleted;
	}
	ShopState = SaveGame->ShopState;
	UImmortalShopLibrary::NormalizeState(ShopState);
	++ShopRevision;
	const int32 CurrentShopDayKey = UImmortalShopLibrary::GetDayKeyFromUtcTicks(
		FDateTime::UtcNow().GetTicks(), ShopUtcOffsetMinutes);
	bool bNeedsShopMigration = LoadedSaveVersion < 11 || ShopState.Listings.IsEmpty();
	if (bNeedsShopMigration || UImmortalShopLibrary::NeedsDailyRefresh(ShopState, CurrentShopDayKey))
	{
		bNeedsShopMigration = RefreshShopForDay(
			CurrentShopDayKey,
			SaveGame->bHasStageData ? FMath::Clamp(SaveGame->QingyunStage, 1, 999) : 1,
			true) || bNeedsShopMigration;
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
	if (bNeedsCharacterBuildMigration || bNeedsShopMigration)
	{
		// Persist one-time version migrations and the current day's stock together.
		SaveProgress();
		UE_LOG(LogTemp, Display, TEXT("Save migration completed at version %d | character build %s | shop %s"),
			UImmortalPathSaveGame::CurrentSaveVersion,
			bNeedsCharacterBuildMigration ? TEXT("true") : TEXT("false"),
			bNeedsShopMigration ? TEXT("true") : TEXT("false"));
	}
	int32 SoldOutListings = 0;
	for (const FImmortalShopListing& Listing : ShopState.Listings) SoldOutListings += Listing.bSoldOut ? 1 : 0;
	UE_LOG(LogTemp, Display, TEXT("Player progress loaded: realm %s | cultivation %d/%d | spirit stones %d | equipped %d | backpack %d | material types %d | pill stacks %d | artifacts %d | artifact equipped %s | techniques %d/%d | insight %d | root %d/%.2f | path %d/switches %d | shop %d/%d/%d/%d | boost %.0fs | combat power %.2f"),
		*GetFullCultivationRealmName().ToString(), CurrentCultivation, GetRequiredCultivation(),
		CurrentGold, EquippedItems.Num(), InventoryItems.Num(), MaterialInventory.Num(), PillInventory.Num(),
		ArtifactInventory.Num(), EquippedArtifactInstanceId.IsValid() ? TEXT("true") : TEXT("false"),
		TechniqueLibrary.Num(), EquippedTechniqueIds.Num(), TechniqueInsightPoints,
		static_cast<int32>(SpiritRootState.Root), SpiritRootState.Purity,
		static_cast<int32>(CultivationPathState.Path), CultivationPathState.SwitchCount,
		ShopState.RefreshDayKey, ShopState.RefreshSerial, ShopState.Listings.Num(), SoldOutListings,
		GetAlchemyBoostRemainingSeconds(), GetCombatPower());
	return true;
}

void AImmortalPlayerCharacter::ApplyOfflineRewards(UImmortalPathSaveGame* SaveGame)
{
	if (!SaveGame || !CultivationComponent)
	{
		return;
	}

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

	const int64 CurrentUtcTicks = FDateTime::UtcNow().GetTicks();
	LastOfflineRewardResult = UImmortalOfflineRewardLibrary::Calculate(
		SaveGame->LastSavedUtcTicks,
		CurrentUtcTicks,
		MinimumOfflineSeconds,
		MaximumOfflineHours,
		CultivationComponent->HasReachedAscension() ? 0.0f : CultivationComponent->GetCultivationPerSecondWithoutAlchemyBoost(),
		OfflineCultivationEfficiency,
		OfflineSpiritStonesPerMinute,
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

	if (LastOfflineRewardResult.Cultivation > 0)
	{
		CultivationComponent->AddCultivation(LastOfflineRewardResult.Cultivation);
		CurrentCultivation = CultivationComponent->GetCurrentCultivation();
	}
	CurrentGold = static_cast<int32>(FMath::Min<int64>(
		static_cast<int64>(CurrentGold) + LastOfflineRewardResult.SpiritStones,
		MAX_int32));

	const int32 OfflineItemLevel = 1 + (FMath::Clamp(SaveGame->QingyunStage, 1, 999) - 1) / 5;
	int32 EquipmentGranted = 0;
	for (int32 Index = 0; Index < LastOfflineRewardResult.EquipmentCount; ++Index)
	{
		const FImmortalEquipmentItem Item = UImmortalEquipmentLibrary::GenerateRandomEquipment(OfflineItemLevel);
		if (ProcessEquipmentItem(Item, false, false))
		{
			++EquipmentGranted;
		}
	}
	LastOfflineRewardResult.EquipmentCount = EquipmentGranted;

	int32 MaterialsGranted = 0;
	for (int32 Index = 0; Index < LastOfflineRewardResult.MaterialBundleCount; ++Index)
	{
		const FImmortalMaterialStack Material = UImmortalMaterialLibrary::GenerateStageDrop(
			FMath::Clamp(SaveGame->QingyunStage, 1, 999), false, Index);
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

	BP_OnRewardsChanged(
		CurrentCultivation,
		CurrentGold,
		LastOfflineRewardResult.Cultivation,
		LastOfflineRewardResult.SpiritStones);
	BP_OnOfflineRewardsClaimed(
		LastOfflineRewardResult.RewardedOfflineSeconds,
		LastOfflineRewardResult.Cultivation,
		LastOfflineRewardResult.SpiritStones,
		LastOfflineRewardResult.EquipmentCount,
		LastOfflineRewardResult.bCappedByMaximum);
	BP_OnOfflineMaterialsGranted(LastOfflineRewardResult.MaterialCount);

	GetWorldTimerManager().ClearTimer(CultivationBreakthroughSaveTimerHandle);
	SaveProgress();
	UE_LOG(LogTemp, Display,
		TEXT("Offline rewards claimed once: raw=%llds rewarded=%llds%s | cultivation +%d | spirit stones +%d | equipment %d | materials %d | total claims %d"),
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
		&& UImmortalCraftingLibrary::IsRecipeUnlocked(Definition, DisplayedStage);
}

bool AImmortalPlayerCharacter::CanCraftEquipment(const FName RecipeId) const
{
	FImmortalCraftingRecipeDefinition Definition;
	return IsCraftingRecipeUnlocked(RecipeId)
		&& UImmortalCraftingLibrary::GetRecipeDefinition(RecipeId, Definition)
		&& UImmortalCraftingLibrary::CanAfford(MaterialInventory, CurrentGold, Definition.Cost);
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
	Result.bUnlocked = UImmortalCraftingLibrary::IsRecipeUnlocked(Definition, DisplayedStage);
	if (!Result.bUnlocked)
	{
		Result.Message = FText::FromString(TEXT("当前青云山关卡尚未解锁此配方"));
		return Result;
	}
	Result.bAffordable = UImmortalCraftingLibrary::CanAfford(MaterialInventory, CurrentGold, Definition.Cost);
	if (!Result.bAffordable)
	{
		Result.Message = FText::FromString(TEXT("打造材料或灵石不足"));
		return Result;
	}

	FImmortalEquipmentItem CraftedItem = UImmortalEquipmentLibrary::GenerateCraftedEquipment(
		FMath::Max(DisplayedStage, 1), Definition.OutputSlot, Definition.OutputQuality);
	if (!CraftedItem.IsValid()
		|| !UImmortalCraftingLibrary::ConsumeCost(MaterialInventory, CurrentGold, Definition.Cost))
	{
		Result.Message = FText::FromString(TEXT("炼器事务未完成，资源未被部分扣除"));
		return Result;
	}
	++MaterialInventoryRevision;
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, 0);

	bool bStored = ProcessEquipmentItem(CraftedItem, true, false);
	if (!bStored)
	{
		if (InventoryItems.IsEmpty()) InventoryItems.Add(CraftedItem);
		else
		{
			int32 WeakestIndex = 0;
			for (int32 Index = 1; Index < InventoryItems.Num(); ++Index)
			{
				if (UImmortalEquipmentLibrary::CalculateEquipmentPower(InventoryItems[Index])
					< UImmortalEquipmentLibrary::CalculateEquipmentPower(InventoryItems[WeakestIndex]))
				{
					WeakestIndex = Index;
				}
			}
			InventoryItems[WeakestIndex] = CraftedItem;
		}
		++EquipmentInventoryRevision;
		bStored = true;
		BP_OnInventoryChanged(InventoryItems.Num(), FMath::Max(InventoryCapacity, 1));
	}

	Result.bSucceeded = bStored;
	Result.ItemId = CraftedItem.ItemId;
	Result.Message = FText::FromString(FString::Printf(TEXT("打造成功：%s（%d 条词条）"),
		*CraftedItem.DisplayName.ToString(), CraftedItem.Affixes.Num()));
	SaveProgress();
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
			MaterialInventory, CurrentGold, UImmortalCraftingLibrary::GetEnhancementCost(Item));
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
	const FImmortalCraftingCost Cost = UImmortalCraftingLibrary::GetEnhancementCost(*Item);
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
	SaveProgress();
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
			MaterialInventory, CurrentGold, UImmortalCraftingLibrary::GetRefinementCost(Item));
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
	const FImmortalCraftingCost Cost = UImmortalCraftingLibrary::GetRefinementCost(*Item);
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
	SaveProgress();
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
		&& DisplayedStage >= FMath::Max(Definition.MinimumQingyunStage, 1);
}

bool AImmortalPlayerCharacter::CanCraftArtifact(const FName ArtifactId) const
{
	FImmortalArtifactDefinition Definition;
	return IsArtifactUnlocked(ArtifactId)
		&& UImmortalArtifactLibrary::GetArtifactDefinition(ArtifactId, Definition)
		&& UImmortalCraftingLibrary::CanAfford(MaterialInventory, CurrentGold, Definition.CraftingCost);
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
	Result.bUnlocked = DisplayedStage >= FMath::Max(Definition.MinimumQingyunStage, 1);
	if (!Result.bUnlocked)
	{
		Result.Message = FText::FromString(FString::Printf(TEXT("青云山第 %d 关解锁此法宝"), Definition.MinimumQingyunStage));
		return Result;
	}
	Result.bAffordable = UImmortalCraftingLibrary::CanAfford(MaterialInventory, CurrentGold, Definition.CraftingCost);
	if (!Result.bAffordable)
	{
		Result.Message = FText::FromString(TEXT("炼制法宝所需灵石或材料不足"));
		return Result;
	}

	FImmortalArtifactItem Crafted = UImmortalArtifactLibrary::CreateArtifact(ArtifactId);
	if (!Crafted.IsValid()
		|| !UImmortalCraftingLibrary::ConsumeCost(MaterialInventory, CurrentGold, Definition.CraftingCost))
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
		&& UImmortalCraftingLibrary::CanAfford(MaterialInventory, CurrentGold, UImmortalArtifactLibrary::GetUpgradeCost(*Item));
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
	const FImmortalCraftingCost Cost = UImmortalArtifactLibrary::GetUpgradeCost(*Item);
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
		&& UImmortalCraftingLibrary::CanAfford(MaterialInventory, CurrentGold, UImmortalArtifactLibrary::GetStarUpCost(*Item));
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
	const FImmortalCraftingCost Cost = UImmortalArtifactLibrary::GetStarUpCost(*Item);
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
		&& DisplayedStage >= FMath::Max(Definition.MinimumQingyunStage, 1)
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

	int32 WeakestIndex = INDEX_NONE;
	float WeakestPower = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < InventoryItems.Num(); ++Index)
	{
		const float Power = UImmortalEquipmentLibrary::CalculateEquipmentPower(InventoryItems[Index]);
		if (Power < WeakestPower)
		{
			WeakestPower = Power;
			WeakestIndex = Index;
		}
	}

	if (WeakestIndex != INDEX_NONE && UImmortalEquipmentLibrary::CalculateEquipmentPower(Item) > WeakestPower)
	{
		InventoryItems[WeakestIndex] = Item;
		return true;
	}
	return false;
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

	for (const FImmortalEquipmentItem& Item : EquippedItems)
	{
		if (!IsEquipmentCompatibleWithPath(Item)) continue;
		EquippedAttackBonus += FMath::Max(Item.AttackBonus, 0.0f);
		EquippedDefenseBonus += FMath::Max(Item.DefenseBonus, 0.0f);
		EquippedHealthBonus += FMath::Max(Item.HealthBonus, 0.0f);
		EquippedAttackSpeedBonus += FMath::Max(Item.AttackSpeedBonus, 0.0f);
		EquippedCriticalChanceBonus += FMath::Max(Item.CriticalChanceBonus, 0.0f);
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
	return UImmortalCharacterPathLibrary::CalculateElementDamageMultiplier(SpiritRootState, Element);
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

void AImmortalPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SaveProgress();
	StopAutoAttack();
	if (CultivationComponent)
	{
		CultivationComponent->StopCultivating();
	}
	GetWorldTimerManager().ClearTimer(TaskbarWindowTimerHandle);
	GetWorldTimerManager().ClearTimer(AutoReviveTimerHandle);
	GetWorldTimerManager().ClearTimer(CultivationAutosaveTimerHandle);
	GetWorldTimerManager().ClearTimer(CultivationBreakthroughSaveTimerHandle);
	GetWorldTimerManager().ClearTimer(AlchemyBoostTimerHandle);
	GetWorldTimerManager().ClearTimer(ShopDailyRefreshTimerHandle);
	bInventoryOpen = false;
	bAlchemyOpen = false;
	bCraftingOpen = false;
	bArtifactOpen = false;
	bTechniqueOpen = false;
	bCharacterBuildOpen = false;
	bShopOpen = false;
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
	Super::EndPlay(EndPlayReason);
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
	StartAutoAttack();
	BP_OnPlayerAutoRevived();
	UE_LOG(LogTemp, Display, TEXT("Player auto-revived with %.1f seconds of protection"), FMath::Max(ReviveInvulnerabilityDuration, 0.0f));
	SaveProgress();
}

void AImmortalPlayerCharacter::StartAutoAttack()
{
	if (!GetWorld())
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
	if (bDead || bAttackPending || !GetWorld())
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
		const float CriticalMultiplier = bCriticalHit ? FMath::Max(CriticalDamageMultiplier, 1.0f) : 1.0f;
		const float RequestedDamage = FMath::Max(GetTotalAttackDamage(), 0.0f) * CriticalMultiplier;
		DamageDealt = UGameplayStatics::ApplyDamage(Target, RequestedDamage, GetController(), this, DamageTypeClass);
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
		const float Applied = UGameplayStatics::ApplyDamage(
			DamageTarget, FMath::Max(RequestedDamage, 0.0f), GetController(), this, DamageTypeClass);
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
		const float Applied = UGameplayStatics::ApplyDamage(
			DamageTarget, FMath::Max(RequestedDamage, 0.0f), GetController(), this, DamageTypeClass);
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
		const float Applied = UGameplayStatics::ApplyDamage(
			DamageTarget, FMath::Max(RequestedDamage, 0.0f), GetController(), this, DamageTypeClass);
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
						const float Applied = UGameplayStatics::ApplyDamage(
							PoisonedTarget, TickDamage, GetController(), this, DamageTypeClass);
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
