// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPlayerCharacter.h"

#include "../Combat/AutoAttackTarget.h"
#include "../UI/ImmortalInventoryWidget.h"
#include "../UI/ImmortalPlayerStatusWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AImmortalPlayerCharacter::AImmortalPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	DamageTypeClass = UDamageType::StaticClass();
}

void AImmortalPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	InventoryItems.Reset();
	EquippedItems.Reset();
	RecalculateEquipmentBonuses();
	CurrentHealth = FMath::Max(GetMaxHealth(), 1.0f);
	CurrentMana = FMath::Max(MaxMana, 0.0f);
	CurrentCultivation = FMath::Max(StartingCultivation, 0);
	CurrentGold = FMath::Max(StartingGold, 0);
	EquipmentDropCount = 0;
	bDead = false;
	ConfigureCombatCamera();

	if (APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PlayerStatusWidget = CreateWidget<UImmortalPlayerStatusWidget>(PlayerController, UImmortalPlayerStatusWidget::StaticClass());
		if (PlayerStatusWidget)
		{
			PlayerStatusWidget->InitializeForPlayer(this);
			PlayerStatusWidget->AddToViewport(10);
			PlayerStatusWidget->SetPositionInViewport(FVector2D(32.0f, 28.0f), false);
			PlayerStatusWidget->SetDesiredSizeInViewport(FVector2D(620.0f, 64.0f));
		}

		PlayerInventoryWidget = CreateWidget<UImmortalInventoryWidget>(PlayerController, UImmortalInventoryWidget::StaticClass());
		if (PlayerInventoryWidget)
		{
			PlayerInventoryWidget->InitializeForPlayer(this);
			PlayerInventoryWidget->AddToViewport(100);
			PlayerInventoryWidget->SetDesiredSizeInViewport(FVector2D(900.0f, 600.0f));
			PlayerInventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (bAutoAttackOnBeginPlay)
	{
		StartAutoAttack();
	}
}

void AImmortalPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (PlayerInputComponent)
	{
		PlayerInputComponent->BindKey(EKeys::I, IE_Pressed, this, &AImmortalPlayerCharacter::ToggleInventory);
	}
}

void AImmortalPlayerCharacter::ToggleInventory()
{
	if (!PlayerInventoryWidget)
	{
		return;
	}

	bInventoryOpen = !bInventoryOpen;
	PlayerInventoryWidget->SetVisibility(bInventoryOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController && GetWorld())
	{
		PlayerController = GetWorld()->GetFirstPlayerController();
	}
	if (!PlayerController)
	{
		return;
	}

	PlayerController->bShowMouseCursor = bInventoryOpen;
	if (bInventoryOpen)
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
		PlayerInventoryWidget->SetDesiredSizeInViewport(InventorySize);
		PlayerInventoryWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f));
		PlayerInventoryWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
		PlayerInventoryWidget->SetPositionInViewport(CentredPosition, false);
		PlayerInventoryWidget->RefreshFromPlayer();
		UE_LOG(LogTemp, Display, TEXT("Inventory opened: viewport %dx%d | panel %.0fx%.0f at %.1f, %.1f | equipped %d | backpack %d/%d | combat power %.2f"),
			ViewportWidth, ViewportHeight, InventorySize.X, InventorySize.Y,
			CentredPosition.X, CentredPosition.Y, EquippedItems.Num(), InventoryItems.Num(),
			GetInventoryCapacity(), GetCombatPower());
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(PlayerInventoryWidget->TakeWidget());
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
		// The existing 2D Blueprint faces the SpringArm down the Y axis, so its
		// local Y is the horizontal screen axis. Moving the camera along that
		// axis keeps the character left of centre without changing the lane.
		FVector CameraOffset = SpringArm->SocketOffset;
		CameraOffset.Y += CameraLeadDistance;
		SpringArm->SocketOffset = CameraOffset;
	}

	if (UCameraComponent* Camera = FindComponentByClass<UCameraComponent>())
	{
		if (Camera->ProjectionMode == ECameraProjectionMode::Orthographic)
		{
			Camera->SetOrthoWidth(FMath::Max(CombatOrthoWidth, 100.0f));
		}
		else
		{
			Camera->SetFieldOfView(FMath::Clamp(CombatPerspectiveFOV, 5.0f, 170.0f));
		}
	}
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

	const float EngineAcceptedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	const float RequestedDamage = EngineAcceptedDamage > 0.0f ? EngineAcceptedDamage : DamageAmount;
	const float ReducedDamage = FMath::Max(RequestedDamage - FMath::Max(GetTotalDefense(), 0.0f), 1.0f);
	const float DamageApplied = FMath::Min(ReducedDamage, CurrentHealth);
	CurrentHealth = FMath::Max(CurrentHealth - DamageApplied, 0.0f);
	BP_OnPlayerDamaged(DamageApplied, CurrentHealth, DamageCauser);

	if (CurrentHealth <= 0.0f)
	{
		bDead = true;
		StopAutoAttack();
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BP_OnPlayerDied(DamageCauser);
	}

	return DamageApplied;
}

float AImmortalPlayerCharacter::GetHealthPercent() const
{
	return GetMaxHealth() > 0.0f ? CurrentHealth / GetMaxHealth() : 0.0f;
}

float AImmortalPlayerCharacter::GetMaxHealth() const
{
	return FMath::Max(MaxHealth + EquippedHealthBonus, 1.0f);
}

float AImmortalPlayerCharacter::GetManaPercent() const
{
	return MaxMana > 0.0f ? CurrentMana / MaxMana : 0.0f;
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

void AImmortalPlayerCharacter::ReceiveKillRewards(const int32 CultivationGained, const int32 GoldGained)
{
	const int32 SafeCultivation = FMath::Max(CultivationGained, 0);
	const int32 SafeGold = FMath::Max(GoldGained, 0);
	CurrentCultivation += SafeCultivation;
	CurrentGold += SafeGold;
	BP_OnRewardsChanged(CurrentCultivation, CurrentGold, SafeCultivation, SafeGold);
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
}

bool AImmortalPlayerCharacter::ReceiveEquipmentItem(const FImmortalEquipmentItem& Item)
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

		RecalculateEquipmentBonuses();
		if (!bDead && bAutoAttackOnBeginPlay)
		{
			StartAutoAttack();
		}
		UE_LOG(LogTemp, Display, TEXT("Equipment auto-equipped: %s | item power %.2f | combat power %.2f"),
			*Item.DisplayName.ToString(), NewPower, GetCombatPower());
		BP_OnEquipmentChanged(Item.Slot, Item, true, GetCombatPower());
		BP_OnInventoryChanged(InventoryItems.Num(), FMath::Max(InventoryCapacity, 1));
		return true;
	}

	const bool bStored = AddItemToInventory(Item);
	UE_LOG(LogTemp, Display, TEXT("Equipment stored=%s: %s | item power %.2f | backpack %d/%d"),
		bStored ? TEXT("true") : TEXT("false"), *Item.DisplayName.ToString(), NewPower,
		InventoryItems.Num(), FMath::Max(InventoryCapacity, 1));
	BP_OnInventoryChanged(InventoryItems.Num(), FMath::Max(InventoryCapacity, 1));
	return bStored;
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
	StopAutoAttack();
	bInventoryOpen = false;
	if (PlayerInventoryWidget)
	{
		PlayerInventoryWidget->RemoveFromParent();
		PlayerInventoryWidget = nullptr;
	}
	if (PlayerStatusWidget)
	{
		PlayerStatusWidget->RemoveFromParent();
		PlayerStatusWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
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
		if (bCriticalHit && DamageDealt > 0.0f)
		{
			BP_OnPlayerCriticalHit(Target, DamageDealt);
		}
	}

	BP_OnAutoAttackResolved(Target, DamageDealt);
	bAttackPending = false;
	CurrentAttackTarget.Reset();
}
