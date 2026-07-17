// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPlayerCharacter.h"

#include "../Combat/AutoAttackTarget.h"
#include "../Save/ImmortalPathSaveGame.h"
#include "../UI/ImmortalAlchemyWidget.h"
#include "../UI/ImmortalCombatFeedbackWidget.h"
#include "../UI/ImmortalCraftingWidget.h"
#include "../UI/ImmortalInventoryWidget.h"
#include "../UI/ImmortalPlayerStatusWidget.h"
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
	LoadProgress();
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
			PlayerStatusWidget->SetDesiredSizeInViewport(FVector2D(820.0f, 64.0f));
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
}

void AImmortalPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (PlayerInputComponent)
	{
		PlayerInputComponent->BindKey(EKeys::I, IE_Pressed, this, &AImmortalPlayerCharacter::ToggleInventory);
		PlayerInputComponent->BindKey(EKeys::L, IE_Pressed, this, &AImmortalPlayerCharacter::ToggleAlchemy);
		PlayerInputComponent->BindKey(EKeys::K, IE_Pressed, this, &AImmortalPlayerCharacter::ToggleCrafting);
	}
}

void AImmortalPlayerCharacter::ToggleInventory()
{
	if (!PlayerInventoryWidget)
	{
		return;
	}

	const bool bWantsOpen = !bInventoryOpen;
	if (bWantsOpen && bAlchemyOpen && PlayerAlchemyWidget)
	{
		bAlchemyOpen = false;
		PlayerAlchemyWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (bWantsOpen && bCraftingOpen && PlayerCraftingWidget)
	{
		bCraftingOpen = false;
		PlayerCraftingWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
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
	if (bWantsOpen && bInventoryOpen && PlayerInventoryWidget)
	{
		bInventoryOpen = false;
		PlayerInventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (bWantsOpen && bCraftingOpen && PlayerCraftingWidget)
	{
		bCraftingOpen = false;
		PlayerCraftingWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
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
	if (bWantsOpen && bInventoryOpen && PlayerInventoryWidget)
	{
		bInventoryOpen = false;
		PlayerInventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (bWantsOpen && bAlchemyOpen && PlayerAlchemyWidget)
	{
		bAlchemyOpen = false;
		PlayerAlchemyWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
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
	const float ReducedDamage = FMath::Max(RequestedDamage - FMath::Max(GetTotalDefense(), 0.0f), 1.0f);
	const float DamageApplied = FMath::Min(ReducedDamage, CurrentHealth);
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

	return DamageApplied;
}

float AImmortalPlayerCharacter::GetHealthPercent() const
{
	return GetMaxHealth() > 0.0f ? CurrentHealth / GetMaxHealth() : 0.0f;
}

float AImmortalPlayerCharacter::GetMaxHealth() const
{
	const float CultivationBonus = CultivationComponent ? CultivationComponent->GetHealthBonus() : 0.0f;
	return FMath::Max(MaxHealth + EquippedHealthBonus + CultivationBonus, 1.0f);
}

float AImmortalPlayerCharacter::GetMaxMana() const
{
	const float CultivationBonus = CultivationComponent ? CultivationComponent->GetManaBonus() : 0.0f;
	return FMath::Max(MaxMana + CultivationBonus, 0.0f);
}

float AImmortalPlayerCharacter::GetManaPercent() const
{
	return GetMaxMana() > 0.0f ? CurrentMana / GetMaxMana() : 0.0f;
}

float AImmortalPlayerCharacter::GetTotalAttackDamage() const
{
	const float CultivationBonus = CultivationComponent ? CultivationComponent->GetAttackBonus() : 0.0f;
	return AttackDamage + EquippedAttackBonus + CultivationBonus;
}

float AImmortalPlayerCharacter::GetTotalDefense() const
{
	const float CultivationBonus = CultivationComponent ? CultivationComponent->GetDefenseBonus() : 0.0f;
	return Defense + EquippedDefenseBonus + CultivationBonus;
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
	CurrentGold += SafeAmount;
	if (CombatFeedbackWidget)
	{
		CombatFeedbackWidget->ShowRewards(PickupWorldLocation, 0, SafeAmount);
	}
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, 0, SafeAmount);
	UE_LOG(LogTemp, Display, TEXT("Spirit stones collected: +%d | total %d"), SafeAmount, CurrentGold);
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
	const float Magnitude = bExceptional ? Definition.ExceptionalMagnitude : Definition.OrdinaryMagnitude;
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

	const FText EffectText = UImmortalAlchemyLibrary::GetEffectText(Definition, Quality);
	SaveProgress();
	BP_OnPillUsed(PillId, Quality, EffectText);
	UE_LOG(LogTemp, Display, TEXT("Pill used: %s %s | effect %s | remaining %d"),
		*UImmortalAlchemyLibrary::GetQualityText(Quality).ToString(),
		*Definition.DisplayName.ToString(),
		*EffectText.ToString(),
		GetPillQuantity(PillId, Quality));
	return true;
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
	const bool bShouldEquip = bAutoEquipNewItems && (!bHasEquippedItem || NewPower > ExistingPower);

	if (bShouldEquip)
	{
		if (bHasEquippedItem)
		{
			AddItemToInventory(EquippedItems[EquippedIndex]);
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
	UE_LOG(LogTemp, Display, TEXT("Equipment stored=%s: %s | item power %.2f | backpack %d/%d"),
		bStored ? TEXT("true") : TEXT("false"), *Item.DisplayName.ToString(), NewPower,
		InventoryItems.Num(), FMath::Max(InventoryCapacity, 1));
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
	SaveGame->AlchemyCultivationBoostMultiplier = AlchemyCultivationBoostMultiplier;
	SaveGame->AlchemyCultivationBoostRemainingSeconds = GetAlchemyBoostRemainingSeconds();
	SaveGame->LastOfflineClaimUtcTicks = LastOfflineClaimUtcTicks;
	SaveGame->TotalRewardedOfflineSeconds = FMath::Max<int64>(TotalRewardedOfflineSeconds, 0);
	SaveGame->TotalOfflineClaims = FMath::Max(TotalOfflineClaims, 0);
	const bool bSaved = SaveGame->SaveToDisk();
	if (bSaved)
	{
		UE_LOG(LogTemp, Display, TEXT("Player progress saved: realm %s | cultivation %d/%d | spirit stones %d | equipped %d | backpack %d | material types %d | pill stacks %d | alchemy boost %.0fs"),
			*GetFullCultivationRealmName().ToString(), CurrentCultivation, GetRequiredCultivation(),
			CurrentGold, EquippedItems.Num(), InventoryItems.Num(), MaterialInventory.Num(), PillInventory.Num(),
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
	UE_LOG(LogTemp, Display, TEXT("Player progress loaded: realm %s | cultivation %d/%d | spirit stones %d | equipped %d | backpack %d | material types %d | pill stacks %d | boost %.0fs | combat power %.2f"),
		*GetFullCultivationRealmName().ToString(), CurrentCultivation, GetRequiredCultivation(),
		CurrentGold, EquippedItems.Num(), InventoryItems.Num(), MaterialInventory.Num(), PillInventory.Num(),
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
	EquippedAttackBonus = 0.0f;
	EquippedDefenseBonus = 0.0f;
	EquippedHealthBonus = 0.0f;
	EquippedAttackSpeedBonus = 0.0f;
	EquippedCriticalChanceBonus = 0.0f;

	for (const FImmortalEquipmentItem& Item : EquippedItems)
	{
		EquippedAttackBonus += FMath::Max(Item.AttackBonus, 0.0f);
		EquippedDefenseBonus += FMath::Max(Item.DefenseBonus, 0.0f);
		EquippedHealthBonus += FMath::Max(Item.HealthBonus, 0.0f);
		EquippedAttackSpeedBonus += FMath::Max(Item.AttackSpeedBonus, 0.0f);
		EquippedCriticalChanceBonus += FMath::Max(Item.CriticalChanceBonus, 0.0f);
	}

	const float NewMaxHealth = GetMaxHealth();
	if (CurrentHealth > 0.0f)
	{
		CurrentHealth = FMath::Clamp(CurrentHealth + FMath::Max(NewMaxHealth - PreviousMaxHealth, 0.0f), 0.0f, NewMaxHealth);
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
	bInventoryOpen = false;
	bAlchemyOpen = false;
	bCraftingOpen = false;
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
