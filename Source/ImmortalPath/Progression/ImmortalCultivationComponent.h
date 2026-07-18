// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ImmortalCultivationComponent.generated.h"

UENUM(BlueprintType)
enum class EImmortalCultivationRealm : uint8
{
	QiRefining UMETA(DisplayName = "Qi Refining"),
	FoundationEstablishment UMETA(DisplayName = "Foundation Establishment"),
	GoldenCore UMETA(DisplayName = "Golden Core"),
	NascentSoul UMETA(DisplayName = "Nascent Soul"),
	SpiritTransformation UMETA(DisplayName = "Spirit Transformation"),
	VoidRefining UMETA(DisplayName = "Void Refining"),
	BodyIntegration UMETA(DisplayName = "Body Integration"),
	Mahayana UMETA(DisplayName = "Mahayana"),
	TribulationTranscendence UMETA(DisplayName = "Tribulation Transcendence"),
	Ascension UMETA(DisplayName = "Ascension"),
	MAX UMETA(Hidden)
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FCultivationProgressChangedSignature,
	int32, CurrentCultivation,
	int32, RequiredCultivation,
	FText, FullRealmName);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FCultivationBreakthroughSignature,
	FText, PreviousRealmName,
	FText, NewRealmName,
	EImmortalCultivationRealm, NewRealm,
	int32, NewMinorStage);

/**
 * Independent online-cultivation system. Combat kills never call into this component.
 * Progression is deterministic so save/load cannot apply attribute growth twice.
 */
UCLASS(ClassGroup = (ImmortalPath), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class IMMORTALPATH_API UImmortalCultivationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UImmortalCultivationComponent();

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Cultivation")
	void InitializeProgress(EImmortalCultivationRealm Realm, int32 MinorStage, int32 Cultivation);

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Cultivation")
	void StartCultivating();

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Cultivation")
	void StopCultivating();

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Cultivation")
	void AddCultivation(int32 Amount);

	/** Spends only progress in the current minor stage; realms never regress. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Cultivation")
	bool TrySpendCultivation(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	bool CanSpendCultivation(int32 Amount) const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	EImmortalCultivationRealm GetCurrentRealm() const { return CurrentRealm; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	int32 GetCurrentMinorStage() const { return CurrentMinorStage; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	int32 GetCurrentCultivation() const { return CurrentCultivation; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	int32 GetRequiredCultivation() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	float GetCultivationPerSecond() const;

	/** Base/external rate without temporary alchemy effects; used by offline rewards. */
	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	float GetCultivationPerSecondWithoutAlchemyBoost() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	bool HasReachedAscension() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	FText GetCurrentRealmName() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	FText GetFullRealmName() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation|Attributes")
	float GetHealthBonus() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation|Attributes")
	float GetManaBonus() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation|Attributes")
	float GetAttackBonus() const;

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation|Attributes")
	float GetDefenseBonus() const;

	/** Runtime multiplier used by future spiritual roots, techniques and cave upgrades. */
	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Cultivation")
	void SetRuntimeRateMultiplier(float Multiplier);

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Cultivation")
	void SetAlchemyRateMultiplier(float Multiplier);

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Cultivation")
	void SetTechniqueRateMultiplier(float Multiplier);

	UFUNCTION(BlueprintCallable, Category = "Immortal Path|Cultivation")
	void SetCharacterPathRateMultiplier(float Multiplier);

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	float GetAlchemyRateMultiplier() const { return AlchemyRateMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	float GetTechniqueRateMultiplier() const { return TechniqueRateMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Immortal Path|Cultivation")
	float GetCharacterPathRateMultiplier() const { return CharacterPathRateMultiplier; }

	UPROPERTY(BlueprintAssignable, Category = "Immortal Path|Cultivation")
	FCultivationProgressChangedSignature OnCultivationProgressChanged;

	UPROPERTY(BlueprintAssignable, Category = "Immortal Path|Cultivation")
	FCultivationBreakthroughSignature OnCultivationBreakthrough;

	static FText GetRealmDisplayName(EImmortalCultivationRealm Realm);

protected:
	/** Base online rate before spiritual-root, technique and cave multipliers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Cultivation", meta = (ClampMin = "0.0"))
	float BaseCultivationPerSecond = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Cultivation", meta = (ClampMin = "0.1", Units = "s"))
	float CultivationTickInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Cultivation", meta = (ClampMin = "1"))
	int32 BaseBreakthroughRequirement = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Cultivation", meta = (ClampMin = "1.0"))
	float MajorRealmRequirementMultiplier = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immortal Path|Cultivation", meta = (ClampMin = "1.0"))
	float MinorStageRequirementMultiplier = 1.18f;

private:
	void HandleCultivationTick();
	bool AdvanceOneStage();
	void BroadcastProgress();
	int32 GetCompletedBreakthroughCount() const;
	float CalculateAttributeBonus(float BaseBonusPerStep) const;

	UPROPERTY(VisibleInstanceOnly, Category = "Immortal Path|Cultivation")
	EImmortalCultivationRealm CurrentRealm = EImmortalCultivationRealm::QiRefining;

	UPROPERTY(VisibleInstanceOnly, Category = "Immortal Path|Cultivation")
	int32 CurrentMinorStage = 1;

	UPROPERTY(VisibleInstanceOnly, Category = "Immortal Path|Cultivation")
	int32 CurrentCultivation = 0;

	float RuntimeRateMultiplier = 1.0f;
	float AlchemyRateMultiplier = 1.0f;
	float TechniqueRateMultiplier = 1.0f;
	float CharacterPathRateMultiplier = 1.0f;
	double FractionalCultivation = 0.0;
	FTimerHandle CultivationTimerHandle;
};
