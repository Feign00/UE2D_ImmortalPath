// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalCultivationComponent.h"

#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	constexpr int32 StagesPerMajorRealm = 10;

	FText GetMinorStageName(const int32 Stage)
	{
		static const TCHAR* Names[] =
		{
			TEXT("一层"), TEXT("二层"), TEXT("三层"), TEXT("四层"), TEXT("五层"),
			TEXT("六层"), TEXT("七层"), TEXT("八层"), TEXT("九层"), TEXT("圆满")
		};
		return FText::FromString(Names[FMath::Clamp(Stage, 1, StagesPerMajorRealm) - 1]);
	}
}

UImmortalCultivationComponent::UImmortalCultivationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UImmortalCultivationComponent::InitializeProgress(
	const EImmortalCultivationRealm Realm,
	const int32 MinorStage,
	const int32 Cultivation)
{
	const int32 RealmIndex = FMath::Clamp(
		static_cast<int32>(Realm), 0, static_cast<int32>(EImmortalCultivationRealm::Ascension));
	CurrentRealm = static_cast<EImmortalCultivationRealm>(RealmIndex);
	CurrentMinorStage = CurrentRealm == EImmortalCultivationRealm::Ascension
		? 1
		: FMath::Clamp(MinorStage, 1, StagesPerMajorRealm);
	CurrentCultivation = HasReachedAscension()
		? 0
		: FMath::Clamp(Cultivation, 0, FMath::Max(GetRequiredCultivation() - 1, 0));
	FractionalCultivation = 0.0;
	BroadcastProgress();
}

void UImmortalCultivationComponent::StartCultivating()
{
	if (!GetWorld() || HasReachedAscension() || GetCultivationPerSecond() <= 0.0f)
	{
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(
		CultivationTimerHandle,
		this,
		&UImmortalCultivationComponent::HandleCultivationTick,
		FMath::Max(CultivationTickInterval, 0.1f),
		true);
}

void UImmortalCultivationComponent::StopCultivating()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CultivationTimerHandle);
	}
}

void UImmortalCultivationComponent::AddCultivation(const int32 Amount)
{
	if (Amount <= 0 || HasReachedAscension())
	{
		return;
	}

	CurrentCultivation = static_cast<int32>(FMath::Min(
		static_cast<int64>(CurrentCultivation) + static_cast<int64>(Amount),
		static_cast<int64>(MAX_int32)));
	int32 SafetyCounter = 0;
	while (!HasReachedAscension() && CurrentCultivation >= GetRequiredCultivation() && SafetyCounter++ < 100)
	{
		CurrentCultivation -= GetRequiredCultivation();
		if (!AdvanceOneStage())
		{
			CurrentCultivation = 0;
			break;
		}
	}
	if (HasReachedAscension())
	{
		CurrentCultivation = 0;
		StopCultivating();
	}
	BroadcastProgress();
}

bool UImmortalCultivationComponent::CanSpendCultivation(const int32 Amount) const
{
	return Amount > 0 && !HasReachedAscension() && CurrentCultivation >= Amount;
}

bool UImmortalCultivationComponent::TrySpendCultivation(const int32 Amount)
{
	if (!CanSpendCultivation(Amount)) return false;
	CurrentCultivation -= Amount;
	BroadcastProgress();
	UE_LOG(LogTemp, Display, TEXT("Cultivation spent: %d | remaining %d/%d | realm %s"),
		Amount, CurrentCultivation, GetRequiredCultivation(), *GetFullRealmName().ToString());
	return true;
}

int32 UImmortalCultivationComponent::GetRequiredCultivation() const
{
	if (HasReachedAscension())
	{
		return 1;
	}
	const int32 RealmIndex = static_cast<int32>(CurrentRealm);
	const double Requirement = static_cast<double>(FMath::Max(BaseBreakthroughRequirement, 1))
		* FMath::Pow(FMath::Max(MajorRealmRequirementMultiplier, 1.0f), RealmIndex)
		* FMath::Pow(FMath::Max(MinorStageRequirementMultiplier, 1.0f), CurrentMinorStage - 1);
	return FMath::Clamp(FMath::RoundToInt64(Requirement), static_cast<int64>(1), static_cast<int64>(MAX_int32));
}

float UImmortalCultivationComponent::GetCultivationPerSecond() const
{
	return GetCultivationPerSecondWithoutAlchemyBoost() * FMath::Max(AlchemyRateMultiplier, 0.0f);
}

float UImmortalCultivationComponent::GetCultivationPerSecondWithoutAlchemyBoost() const
{
	return FMath::Max(BaseCultivationPerSecond, 0.0f)
		* FMath::Max(RuntimeRateMultiplier, 0.0f)
		* FMath::Max(TechniqueRateMultiplier, 0.0f)
		* FMath::Max(CharacterPathRateMultiplier, 0.0f);
}

bool UImmortalCultivationComponent::HasReachedAscension() const
{
	return CurrentRealm == EImmortalCultivationRealm::Ascension;
}

FText UImmortalCultivationComponent::GetCurrentRealmName() const
{
	return GetRealmDisplayName(CurrentRealm);
}

FText UImmortalCultivationComponent::GetFullRealmName() const
{
	if (HasReachedAscension())
	{
		return GetRealmDisplayName(CurrentRealm);
	}
	return FText::FromString(FString::Printf(
		TEXT("%s%s"), *GetRealmDisplayName(CurrentRealm).ToString(), *GetMinorStageName(CurrentMinorStage).ToString()));
}

float UImmortalCultivationComponent::GetHealthBonus() const
{
	return CalculateAttributeBonus(20.0f);
}

float UImmortalCultivationComponent::GetManaBonus() const
{
	return CalculateAttributeBonus(8.0f);
}

float UImmortalCultivationComponent::GetAttackBonus() const
{
	return CalculateAttributeBonus(2.5f);
}

float UImmortalCultivationComponent::GetDefenseBonus() const
{
	return CalculateAttributeBonus(0.5f);
}

void UImmortalCultivationComponent::SetRuntimeRateMultiplier(const float Multiplier)
{
	RuntimeRateMultiplier = FMath::Max(Multiplier, 0.0f);
	if (RuntimeRateMultiplier <= 0.0f)
	{
		StopCultivating();
	}
	else
	{
		StartCultivating();
	}
	BroadcastProgress();
}

void UImmortalCultivationComponent::SetAlchemyRateMultiplier(const float Multiplier)
{
	AlchemyRateMultiplier = FMath::Max(Multiplier, 0.0f);
	if (GetCultivationPerSecond() <= 0.0f)
	{
		StopCultivating();
	}
	else
	{
		StartCultivating();
	}
	BroadcastProgress();
}

void UImmortalCultivationComponent::SetTechniqueRateMultiplier(const float Multiplier)
{
	TechniqueRateMultiplier = FMath::Max(Multiplier, 0.0f);
	if (GetCultivationPerSecond() <= 0.0f)
	{
		StopCultivating();
	}
	else
	{
		StartCultivating();
	}
	BroadcastProgress();
}

void UImmortalCultivationComponent::SetCharacterPathRateMultiplier(const float Multiplier)
{
	CharacterPathRateMultiplier = FMath::Max(Multiplier, 0.0f);
	if (GetCultivationPerSecond() <= 0.0f)
	{
		StopCultivating();
	}
	else StartCultivating();
	BroadcastProgress();
}

FText UImmortalCultivationComponent::GetRealmDisplayName(const EImmortalCultivationRealm Realm)
{
	switch (Realm)
	{
	case EImmortalCultivationRealm::FoundationEstablishment: return FText::FromString(TEXT("筑基"));
	case EImmortalCultivationRealm::GoldenCore: return FText::FromString(TEXT("金丹"));
	case EImmortalCultivationRealm::NascentSoul: return FText::FromString(TEXT("元婴"));
	case EImmortalCultivationRealm::SpiritTransformation: return FText::FromString(TEXT("化神"));
	case EImmortalCultivationRealm::VoidRefining: return FText::FromString(TEXT("炼虚"));
	case EImmortalCultivationRealm::BodyIntegration: return FText::FromString(TEXT("合体"));
	case EImmortalCultivationRealm::Mahayana: return FText::FromString(TEXT("大乘"));
	case EImmortalCultivationRealm::TribulationTranscendence: return FText::FromString(TEXT("渡劫"));
	case EImmortalCultivationRealm::Ascension: return FText::FromString(TEXT("飞升"));
	default: return FText::FromString(TEXT("炼气"));
	}
}

void UImmortalCultivationComponent::HandleCultivationTick()
{
	const double Earned = GetCultivationPerSecond() * FMath::Max(CultivationTickInterval, 0.1f) + FractionalCultivation;
	const int32 WholeCultivation = FMath::FloorToInt(Earned);
	FractionalCultivation = Earned - static_cast<double>(WholeCultivation);
	AddCultivation(WholeCultivation);
}

bool UImmortalCultivationComponent::AdvanceOneStage()
{
	if (HasReachedAscension())
	{
		return false;
	}

	const FText PreviousName = GetFullRealmName();
	if (CurrentMinorStage < StagesPerMajorRealm)
	{
		++CurrentMinorStage;
	}
	else
	{
		const int32 NextRealmIndex = static_cast<int32>(CurrentRealm) + 1;
		CurrentRealm = static_cast<EImmortalCultivationRealm>(FMath::Min(
			NextRealmIndex, static_cast<int32>(EImmortalCultivationRealm::Ascension)));
		CurrentMinorStage = 1;
	}

	const FText NewName = GetFullRealmName();
	OnCultivationBreakthrough.Broadcast(PreviousName, NewName, CurrentRealm, CurrentMinorStage);
	UE_LOG(LogTemp, Display, TEXT("Cultivation breakthrough: %s -> %s | HP bonus %.1f | attack bonus %.1f"),
		*PreviousName.ToString(), *NewName.ToString(), GetHealthBonus(), GetAttackBonus());
	return true;
}

void UImmortalCultivationComponent::BroadcastProgress()
{
	OnCultivationProgressChanged.Broadcast(CurrentCultivation, GetRequiredCultivation(), GetFullRealmName());
}

int32 UImmortalCultivationComponent::GetCompletedBreakthroughCount() const
{
	if (HasReachedAscension())
	{
		return static_cast<int32>(EImmortalCultivationRealm::Ascension) * StagesPerMajorRealm;
	}
	return static_cast<int32>(CurrentRealm) * StagesPerMajorRealm + FMath::Max(CurrentMinorStage - 1, 0);
}

float UImmortalCultivationComponent::CalculateAttributeBonus(const float BaseBonusPerStep) const
{
	float TotalBonus = 0.0f;
	const int32 CompletedSteps = GetCompletedBreakthroughCount();
	for (int32 Step = 0; Step < CompletedSteps; ++Step)
	{
		const int32 RealmIndex = Step / StagesPerMajorRealm;
		TotalBonus += BaseBonusPerStep * FMath::Pow(1.35f, RealmIndex);
	}
	return TotalBonus;
}
