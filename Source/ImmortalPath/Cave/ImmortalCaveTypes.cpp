// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalCaveTypes.h"

#include "Misc/DateTime.h"
#include "Misc/Timespan.h"

namespace
{
	constexpr int32 MinimumBuildingLevel = 1;
	constexpr int32 MaximumBuildingLevel = 20;
	const FName CaveSpiritGrassId(TEXT("SpiritGrass"));
	const FName CaveOreId(TEXT("Ore"));

	const TArray<EImmortalCaveBuildingType>& KnownBuildingTypes()
	{
		static const TArray<EImmortalCaveBuildingType> Types =
		{
			EImmortalCaveBuildingType::CaveHeart,
			EImmortalCaveBuildingType::MeditationRoom,
			EImmortalCaveBuildingType::SpiritVein,
			EImmortalCaveBuildingType::StoragePavilion,
			EImmortalCaveBuildingType::AlchemyRoom,
			EImmortalCaveBuildingType::ForgeRoom,
			EImmortalCaveBuildingType::SpiritField
		};
		return Types;
	}

	bool IsKnownBuildingType(const EImmortalCaveBuildingType Type)
	{
		switch (Type)
		{
		case EImmortalCaveBuildingType::CaveHeart:
		case EImmortalCaveBuildingType::MeditationRoom:
		case EImmortalCaveBuildingType::SpiritVein:
		case EImmortalCaveBuildingType::StoragePavilion:
		case EImmortalCaveBuildingType::AlchemyRoom:
		case EImmortalCaveBuildingType::ForgeRoom:
		case EImmortalCaveBuildingType::SpiritField:
			return true;
		default:
			return false;
		}
	}

	FImmortalCaveBuildingDefinition MakeDefinition(
		const EImmortalCaveBuildingType Type,
		const TCHAR* DisplayName,
		const TCHAR* Description,
		const FLinearColor& Color)
	{
		FImmortalCaveBuildingDefinition Result;
		Result.Type = Type;
		Result.DisplayName = FText::FromString(DisplayName);
		Result.Description = FText::FromString(Description);
		Result.DisplayColor = Color;
		Result.MaximumLevel = MaximumBuildingLevel;
		return Result;
	}

	const TMap<EImmortalCaveBuildingType, FImmortalCaveBuildingDefinition>& BuildingCatalog()
	{
		static const TMap<EImmortalCaveBuildingType, FImmortalCaveBuildingDefinition> Catalog =
		{
			{EImmortalCaveBuildingType::CaveHeart, MakeDefinition(
				EImmortalCaveBuildingType::CaveHeart,
				TEXT("洞府核心"),
				TEXT("洞府灵脉与建筑的中枢。其他建筑不能超过核心等级。"),
				FLinearColor(0.95f, 0.72f, 0.25f))},
			{EImmortalCaveBuildingType::MeditationRoom, MakeDefinition(
				EImmortalCaveBuildingType::MeditationRoom,
				TEXT("静修室"),
				TEXT("汇聚清灵之气，同时提高在线与离线修炼效率。"),
				FLinearColor(0.42f, 0.83f, 1.0f))},
			{EImmortalCaveBuildingType::SpiritVein, MakeDefinition(
				EImmortalCaveBuildingType::SpiritVein,
				TEXT("灵脉"),
				TEXT("持续凝聚灵石并伴生少量矿石。"),
				FLinearColor(0.43f, 1.0f, 0.78f))},
			{EImmortalCaveBuildingType::StoragePavilion, MakeDefinition(
				EImmortalCaveBuildingType::StoragePavilion,
				TEXT("藏珍阁"),
				TEXT("延长洞府产出的最大积累时间。"),
				FLinearColor(0.82f, 0.66f, 1.0f))},
			{EImmortalCaveBuildingType::AlchemyRoom, MakeDefinition(
				EImmortalCaveBuildingType::AlchemyRoom,
				TEXT("丹房"),
				TEXT("提高炼丹成功率与炼成极品丹药的概率。"),
				FLinearColor(0.96f, 0.42f, 0.35f))},
			{EImmortalCaveBuildingType::ForgeRoom, MakeDefinition(
				EImmortalCaveBuildingType::ForgeRoom,
				TEXT("器室"),
				TEXT("降低装备与法宝打造、强化所需的灵石。"),
				FLinearColor(1.0f, 0.55f, 0.22f))},
			{EImmortalCaveBuildingType::SpiritField, MakeDefinition(
				EImmortalCaveBuildingType::SpiritField,
				TEXT("灵田"),
				TEXT("自然培育灵草，并开放可播种、离线生长和收获的灵田地块。"),
				FLinearColor(0.38f, 0.95f, 0.35f))}
		};
		return Catalog;
	}

	int32 SaturatingIncrement(const int32 Value)
	{
		return Value >= MAX_int32 ? MAX_int32 : FMath::Max(Value, 0) + 1;
	}

	int64 SaturatingAdd(const int64 Value, const int64 Amount)
	{
		const int64 SafeValue = FMath::Max<int64>(Value, 0);
		if (Amount <= 0) return SafeValue;
		return SafeValue > MAX_int64 - Amount ? MAX_int64 : SafeValue + Amount;
	}

	int32 CapacityFromRate(const double RatePerHour, const double StorageHours)
	{
		const double Capacity = FMath::CeilToDouble(FMath::Max(0.0, RatePerHour * StorageHours));
		return Capacity >= static_cast<double>(MAX_int32) ? MAX_int32 : static_cast<int32>(Capacity);
	}

	bool AreBuildingArraysEqual(
		const TArray<FImmortalCaveBuildingProgress>& Left,
		const TArray<FImmortalCaveBuildingProgress>& Right)
	{
		if (Left.Num() != Right.Num()) return false;
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index].Type != Right[Index].Type || Left[Index].Level != Right[Index].Level) return false;
		}
		return true;
	}

	void AddIngredient(FImmortalCraftingCost& Cost, const FName MaterialId, const int32 Quantity)
	{
		if (MaterialId.IsNone() || Quantity <= 0) return;
		if (FImmortalCraftingMaterialCost* Existing = Cost.Materials.FindByPredicate(
			[MaterialId](const FImmortalCraftingMaterialCost& Entry)
			{
				return Entry.MaterialId == MaterialId;
			}))
		{
			Existing->Quantity = static_cast<int32>(FMath::Min<int64>(
				static_cast<int64>(Existing->Quantity) + Quantity,
				MAX_int32));
			return;
		}
		FImmortalCraftingMaterialCost& Entry = Cost.Materials.AddDefaulted_GetRef();
		Entry.MaterialId = MaterialId;
		Entry.Quantity = Quantity;
	}

	bool AggregateCost(const FImmortalCraftingCost& Cost, TMap<FName, int32>& OutMaterials)
	{
		OutMaterials.Reset();
		if (Cost.SpiritStones < 0) return false;
		for (const FImmortalCraftingMaterialCost& Entry : Cost.Materials)
		{
			if (Entry.MaterialId.IsNone() || Entry.Quantity <= 0) return false;
			const int32 Existing = OutMaterials.FindRef(Entry.MaterialId);
			if (Existing > MAX_int32 - Entry.Quantity) return false;
			OutMaterials.Add(Entry.MaterialId, Existing + Entry.Quantity);
		}
		return true;
	}

	bool CanAffordAggregated(
		const TArray<FImmortalMaterialStack>& Materials,
		const int32 SpiritStones,
		const FImmortalCraftingCost& Cost)
	{
		TMap<FName, int32> Aggregated;
		if (!AggregateCost(Cost, Aggregated) || SpiritStones < Cost.SpiritStones) return false;
		for (const TPair<FName, int32>& Entry : Aggregated)
		{
			if (UImmortalMaterialLibrary::GetMaterialQuantity(Materials, Entry.Key) < Entry.Value) return false;
		}
		return true;
	}

	bool ConsumeAggregatedCost(
		TArray<FImmortalMaterialStack>& Materials,
		int32& SpiritStones,
		const FImmortalCraftingCost& Cost)
	{
		if (!CanAffordAggregated(Materials, SpiritStones, Cost)) return false;

		TArray<FImmortalMaterialStack> NewMaterials = Materials;
		int32 NewSpiritStones = SpiritStones;
		TMap<FName, int32> Aggregated;
		if (!AggregateCost(Cost, Aggregated)) return false;
		for (const TPair<FName, int32>& Entry : Aggregated)
		{
			if (!UImmortalMaterialLibrary::RemoveMaterialStack(NewMaterials, Entry.Key, Entry.Value)) return false;
		}
		NewSpiritStones -= Cost.SpiritStones;
		Materials = MoveTemp(NewMaterials);
		SpiritStones = NewSpiritStones;
		return true;
	}

	bool NormalizeStoredResource(int32& Stored, double& Fraction, const int32 Capacity)
	{
		const int32 OldStored = Stored;
		const double OldFraction = Fraction;
		Stored = FMath::Clamp(Stored, 0, FMath::Max(Capacity, 0));

		if (!FMath::IsFinite(Fraction) || Fraction < 0.0)
		{
			Fraction = 0.0;
		}
		else if (Fraction >= 1.0)
		{
			if (Fraction >= static_cast<double>(MAX_int64))
			{
				Stored = FMath::Max(Capacity, 0);
				Fraction = 0.0;
			}
			else
			{
				const int64 Whole = static_cast<int64>(FMath::FloorToDouble(Fraction));
				const int32 Available = FMath::Max(Capacity - Stored, 0);
				Stored += static_cast<int32>(FMath::Min<int64>(Whole, Available));
				Fraction -= static_cast<double>(Whole);
			}
		}
		if (Stored >= Capacity || !FMath::IsFinite(Fraction)) Fraction = 0.0;
		Fraction = FMath::Clamp(Fraction, 0.0, 0.999999999999);
		return Stored != OldStored || Fraction != OldFraction;
	}

	void SettleOneResource(
		const double RatePerHour,
		const double ElapsedSeconds,
		const int32 Capacity,
		int32& Stored,
		double& Fraction,
		int64& TotalProduced,
		int32& OutAdded,
		int64& OutDiscarded)
	{
		const double Accrued = FMath::Max(0.0, Fraction)
			+ FMath::Max(0.0, RatePerHour) * FMath::Max(0.0, ElapsedSeconds) / 3600.0;
		const int64 WholeProduced = Accrued >= static_cast<double>(MAX_int64)
			? MAX_int64
			: static_cast<int64>(FMath::FloorToDouble(Accrued));
		const int32 Available = FMath::Max(Capacity - Stored, 0);
		OutAdded = static_cast<int32>(FMath::Min<int64>(WholeProduced, Available));
		OutDiscarded = FMath::Max<int64>(WholeProduced - OutAdded, 0);
		Stored += OutAdded;
		TotalProduced = SaturatingAdd(TotalProduced, WholeProduced);

		// Once a store is full, both overflow and sub-unit progress are deliberately discarded.
		Fraction = Stored >= Capacity || WholeProduced == MAX_int64
			? 0.0
			: FMath::Clamp(Accrued - static_cast<double>(WholeProduced), 0.0, 0.999999999999);
	}
}

TArray<EImmortalCaveBuildingType> UImmortalCaveLibrary::GetKnownBuildingTypes()
{
	return KnownBuildingTypes();
}

bool UImmortalCaveLibrary::GetBuildingDefinition(
	const EImmortalCaveBuildingType Type,
	FImmortalCaveBuildingDefinition& OutDefinition)
{
	if (const FImmortalCaveBuildingDefinition* Definition = BuildingCatalog().Find(Type))
	{
		OutDefinition = *Definition;
		return true;
	}
	return false;
}

FImmortalCaveState UImmortalCaveLibrary::CreateDefaultState(const int64 InitialUtcTicks)
{
	FImmortalCaveState Result;
	Result.bInitialized = true;
	Result.LastSettlementUtcTicks = InitialUtcTicks > 0 ? InitialUtcTicks : FDateTime::UtcNow().GetTicks();
	for (const EImmortalCaveBuildingType Type : KnownBuildingTypes())
	{
		FImmortalCaveBuildingProgress& Building = Result.Buildings.AddDefaulted_GetRef();
		Building.Type = Type;
		Building.Level = MinimumBuildingLevel;
	}
	Result.Revision = 1;
	return Result;
}

bool UImmortalCaveLibrary::NormalizeState(FImmortalCaveState& State, const int64 DefaultUtcTicks)
{
	if (!State.bInitialized)
	{
		State = CreateDefaultState(DefaultUtcTicks);
		return true;
	}

	bool bChanged = false;
	TMap<EImmortalCaveBuildingType, int32> HighestLevels;
	for (const FImmortalCaveBuildingProgress& Building : State.Buildings)
	{
		if (!IsKnownBuildingType(Building.Type))
		{
			bChanged = true;
			continue;
		}
		const int32 SafeLevel = FMath::Clamp(Building.Level, MinimumBuildingLevel, MaximumBuildingLevel);
		const int32 ExistingLevel = HighestLevels.FindRef(Building.Type);
		HighestLevels.Add(Building.Type, FMath::Max(ExistingLevel, SafeLevel));
		if (SafeLevel != Building.Level || ExistingLevel > 0) bChanged = true;
	}

	const int32 CoreLevel = FMath::Clamp(
		HighestLevels.FindRef(EImmortalCaveBuildingType::CaveHeart),
		MinimumBuildingLevel,
		MaximumBuildingLevel);
	TArray<FImmortalCaveBuildingProgress> CanonicalBuildings;
	CanonicalBuildings.Reserve(KnownBuildingTypes().Num());
	for (const EImmortalCaveBuildingType Type : KnownBuildingTypes())
	{
		FImmortalCaveBuildingProgress& Building = CanonicalBuildings.AddDefaulted_GetRef();
		Building.Type = Type;
		const int32 LoadedLevel = FMath::Clamp(
			HighestLevels.FindRef(Type),
			MinimumBuildingLevel,
			MaximumBuildingLevel);
		Building.Level = Type == EImmortalCaveBuildingType::CaveHeart
			? CoreLevel
			: FMath::Min(LoadedLevel, CoreLevel);
	}
	if (!AreBuildingArraysEqual(State.Buildings, CanonicalBuildings))
	{
		State.Buildings = MoveTemp(CanonicalBuildings);
		bChanged = true;
	}

	if (State.LastSettlementUtcTicks <= 0)
	{
		State.LastSettlementUtcTicks = DefaultUtcTicks > 0 ? DefaultUtcTicks : FDateTime::UtcNow().GetTicks();
		bChanged = true;
	}

	const FImmortalCaveProductionSnapshot Snapshot = GetProductionSnapshot(State);
	bChanged |= NormalizeStoredResource(State.StoredSpiritStones, State.SpiritStoneFraction, Snapshot.SpiritStoneCapacity);
	bChanged |= NormalizeStoredResource(State.StoredSpiritGrass, State.SpiritGrassFraction, Snapshot.SpiritGrassCapacity);
	bChanged |= NormalizeStoredResource(State.StoredOre, State.OreFraction, Snapshot.OreCapacity);

	auto ClampCounter = [&bChanged](int64& Value)
	{
		if (Value < 0)
		{
			Value = 0;
			bChanged = true;
		}
	};
	ClampCounter(State.TotalSpiritStonesProduced);
	ClampCounter(State.TotalSpiritGrassProduced);
	ClampCounter(State.TotalOreProduced);
	ClampCounter(State.TotalSpiritStonesCollected);
	ClampCounter(State.TotalSpiritGrassCollected);
	ClampCounter(State.TotalOreCollected);
	if (State.CollectionCount < 0)
	{
		State.CollectionCount = 0;
		bChanged = true;
	}
	if (State.Revision < 0)
	{
		State.Revision = 0;
		bChanged = true;
	}
	if (bChanged) State.Revision = SaturatingIncrement(State.Revision);
	return bChanged;
}

int32 UImmortalCaveLibrary::GetBuildingLevel(
	const FImmortalCaveState& State,
	const EImmortalCaveBuildingType Type)
{
	if (!IsKnownBuildingType(Type)) return 0;
	int32 RequestedLevel = MinimumBuildingLevel;
	int32 CoreLevel = MinimumBuildingLevel;
	for (const FImmortalCaveBuildingProgress& Building : State.Buildings)
	{
		if (!IsKnownBuildingType(Building.Type)) continue;
		const int32 SafeLevel = FMath::Clamp(Building.Level, MinimumBuildingLevel, MaximumBuildingLevel);
		if (Building.Type == EImmortalCaveBuildingType::CaveHeart) CoreLevel = FMath::Max(CoreLevel, SafeLevel);
		if (Building.Type == Type) RequestedLevel = FMath::Max(RequestedLevel, SafeLevel);
	}
	return Type == EImmortalCaveBuildingType::CaveHeart
		? CoreLevel
		: FMath::Min(RequestedLevel, CoreLevel);
}

FImmortalCraftingCost UImmortalCaveLibrary::GetUpgradeCost(
	const FImmortalCaveState& State,
	const EImmortalCaveBuildingType Type)
{
	FImmortalCraftingCost Result;
	const int32 Level = GetBuildingLevel(State, Type);
	if (Level <= 0 || Level >= MaximumBuildingLevel) return Result;
	const int32 TargetLevel = Level + 1;
	const int64 LevelSquared = static_cast<int64>(Level) * Level;

	switch (Type)
	{
	case EImmortalCaveBuildingType::CaveHeart:
		Result.SpiritStones = static_cast<int32>(200 * LevelSquared);
		AddIngredient(Result, CaveOreId, 4 * Level);
		if (TargetLevel >= 5) AddIngredient(Result, TEXT("SpiritIron"), 1 + (TargetLevel - 5) / 4);
		break;
	case EImmortalCaveBuildingType::MeditationRoom:
		Result.SpiritStones = static_cast<int32>(120 * LevelSquared);
		AddIngredient(Result, TEXT("SpiritLiquid"), 2 * Level);
		AddIngredient(Result, CaveSpiritGrassId, 2 * Level);
		break;
	case EImmortalCaveBuildingType::SpiritVein:
		Result.SpiritStones = static_cast<int32>(100 * LevelSquared);
		AddIngredient(Result, CaveOreId, 4 * Level);
		if (TargetLevel >= 4) AddIngredient(Result, TEXT("SpiritIron"), 1 + (TargetLevel - 4) / 4);
		break;
	case EImmortalCaveBuildingType::StoragePavilion:
		Result.SpiritStones = static_cast<int32>(80 * LevelSquared);
		AddIngredient(Result, CaveOreId, 3 * Level);
		if (TargetLevel >= 3) AddIngredient(Result, TEXT("DemonBone"), 1 + (TargetLevel - 3) / 4);
		break;
	case EImmortalCaveBuildingType::AlchemyRoom:
		Result.SpiritStones = static_cast<int32>(110 * LevelSquared);
		AddIngredient(Result, CaveSpiritGrassId, 3 * Level);
		AddIngredient(Result, TEXT("SpiritLiquid"), 2 * Level);
		break;
	case EImmortalCaveBuildingType::ForgeRoom:
		Result.SpiritStones = static_cast<int32>(120 * LevelSquared);
		AddIngredient(Result, CaveOreId, 3 * Level);
		if (TargetLevel >= 3) AddIngredient(Result, TEXT("SpiritIron"), 1 + (TargetLevel - 3) / 4);
		break;
	case EImmortalCaveBuildingType::SpiritField:
		Result.SpiritStones = static_cast<int32>(90 * LevelSquared);
		AddIngredient(Result, CaveSpiritGrassId, 2 * Level);
		AddIngredient(Result, TEXT("SpiritLiquid"), Level);
		break;
	default:
		return FImmortalCraftingCost();
	}
	return Result;
}

FImmortalCaveUpgradeResult UImmortalCaveLibrary::EvaluateUpgrade(
	const FImmortalCaveState& State,
	const EImmortalCaveBuildingType Type,
	const TArray<FImmortalMaterialStack>& Materials,
	const int32 SpiritStones)
{
	FImmortalCaveUpgradeResult Result;
	FImmortalCaveBuildingDefinition Definition;
	Result.bKnownBuilding = GetBuildingDefinition(Type, Definition);
	if (!Result.bKnownBuilding)
	{
		Result.Message = FText::FromString(TEXT("未知的洞府建筑。"));
		return Result;
	}

	Result.CurrentLevel = GetBuildingLevel(State, Type);
	Result.TargetLevel = FMath::Min(Result.CurrentLevel + 1, Definition.MaximumLevel);
	Result.bAtMaximumLevel = Result.CurrentLevel >= Definition.MaximumLevel;
	if (Result.bAtMaximumLevel)
	{
		Result.Message = FText::FromString(TEXT("该建筑已达到最高等级。"));
		return Result;
	}

	const int32 CoreLevel = GetBuildingLevel(State, EImmortalCaveBuildingType::CaveHeart);
	Result.bBlockedByCaveHeart = Type != EImmortalCaveBuildingType::CaveHeart
		&& Result.TargetLevel > CoreLevel;
	Result.Cost = GetUpgradeCost(State, Type);
	Result.bAffordable = CanAffordAggregated(Materials, SpiritStones, Result.Cost);
	Result.bCanUpgrade = !Result.bBlockedByCaveHeart && Result.bAffordable;
	if (Result.bBlockedByCaveHeart)
	{
		Result.Message = FText::Format(
			FText::FromString(TEXT("需要先将洞府核心升至 {0} 级。")),
			FText::AsNumber(Result.TargetLevel));
	}
	else if (!Result.bAffordable)
	{
		Result.Message = FText::FromString(TEXT("灵石或材料不足。"));
	}
	else
	{
		Result.Message = FText::Format(
			FText::FromString(TEXT("可升级至 {0} 级。")),
			FText::AsNumber(Result.TargetLevel));
	}
	return Result;
}

FImmortalCaveUpgradeResult UImmortalCaveLibrary::TryUpgradeBuilding(
	FImmortalCaveState& State,
	const EImmortalCaveBuildingType Type,
	TArray<FImmortalMaterialStack>& Materials,
	int32& SpiritStones)
{
	FImmortalCaveState NewState = State;
	TArray<FImmortalMaterialStack> NewMaterials = Materials;
	int32 NewSpiritStones = SpiritStones;
	NormalizeState(NewState, State.LastSettlementUtcTicks);

	FImmortalCaveUpgradeResult Result = EvaluateUpgrade(NewState, Type, NewMaterials, NewSpiritStones);
	if (!Result.bCanUpgrade) return Result;
	if (!ConsumeAggregatedCost(NewMaterials, NewSpiritStones, Result.Cost))
	{
		Result.bAffordable = false;
		Result.bCanUpgrade = false;
		Result.Message = FText::FromString(TEXT("升级交易验证失败。"));
		return Result;
	}

	FImmortalCaveBuildingProgress* Building = NewState.Buildings.FindByPredicate(
		[Type](const FImmortalCaveBuildingProgress& Entry)
		{
			return Entry.Type == Type;
		});
	if (!Building) return Result;
	Building->Level = Result.TargetLevel;
	NewState.Revision = SaturatingIncrement(NewState.Revision);

	State = MoveTemp(NewState);
	Materials = MoveTemp(NewMaterials);
	SpiritStones = NewSpiritStones;
	Result.bSucceeded = true;
	Result.Message = FText::Format(
		FText::FromString(TEXT("升级成功，当前 {0} 级。")),
		FText::AsNumber(Result.TargetLevel));
	return Result;
}

FImmortalCaveProductionSnapshot UImmortalCaveLibrary::GetProductionSnapshot(const FImmortalCaveState& State)
{
	FImmortalCaveProductionSnapshot Result;
	const int32 CoreLevel = GetBuildingLevel(State, EImmortalCaveBuildingType::CaveHeart);
	const int32 MeditationLevel = GetBuildingLevel(State, EImmortalCaveBuildingType::MeditationRoom);
	const int32 SpiritVeinLevel = GetBuildingLevel(State, EImmortalCaveBuildingType::SpiritVein);
	const int32 StorageLevel = GetBuildingLevel(State, EImmortalCaveBuildingType::StoragePavilion);
	const int32 AlchemyLevel = GetBuildingLevel(State, EImmortalCaveBuildingType::AlchemyRoom);
	const int32 ForgeLevel = GetBuildingLevel(State, EImmortalCaveBuildingType::ForgeRoom);
	const int32 SpiritFieldLevel = GetBuildingLevel(State, EImmortalCaveBuildingType::SpiritField);

	Result.GlobalProductionMultiplier = 1.0f + 0.05f * FMath::Max(CoreLevel - 1, 0);
	Result.CultivationRateMultiplier = 1.0f + 0.05f * MeditationLevel;
	Result.AlchemySuccessChanceBonus = 0.01f * FMath::Max(AlchemyLevel - 1, 0);
	Result.AlchemyExceptionalChanceBonus = 0.005f * FMath::Max(AlchemyLevel - 1, 0);
	Result.ForgeSpiritStoneDiscount = FMath::Min(0.015f * FMath::Max(ForgeLevel - 1, 0), 0.25f);
	Result.SpiritStonesPerHour = (20.0 + 10.0 * SpiritVeinLevel) * Result.GlobalProductionMultiplier;
	Result.OrePerHour = 0.5 * SpiritVeinLevel * Result.GlobalProductionMultiplier;
	Result.SpiritGrassPerHour = 0.75 * SpiritFieldLevel * Result.GlobalProductionMultiplier;
	Result.StorageHours = 8.0 + 2.0 * FMath::Max(StorageLevel - 1, 0);
	Result.SpiritStoneCapacity = CapacityFromRate(Result.SpiritStonesPerHour, Result.StorageHours);
	Result.SpiritGrassCapacity = CapacityFromRate(Result.SpiritGrassPerHour, Result.StorageHours);
	Result.OreCapacity = CapacityFromRate(Result.OrePerHour, Result.StorageHours);
	return Result;
}

FImmortalCaveSettlementResult UImmortalCaveLibrary::SettleProduction(
	FImmortalCaveState& State,
	const int64 CurrentUtcTicks)
{
	FImmortalCaveSettlementResult Result;
	Result.bStateChanged = NormalizeState(State, CurrentUtcTicks);
	if (CurrentUtcTicks <= 0) return Result;
	if (CurrentUtcTicks < State.LastSettlementUtcTicks)
	{
		Result.bClockRollbackDetected = true;
		return Result;
	}
	if (CurrentUtcTicks == State.LastSettlementUtcTicks) return Result;

	const int64 ElapsedTicks = CurrentUtcTicks - State.LastSettlementUtcTicks;
	Result.ElapsedSeconds = static_cast<double>(ElapsedTicks) / static_cast<double>(ETimespan::TicksPerSecond);
	const FImmortalCaveProductionSnapshot Snapshot = GetProductionSnapshot(State);
	SettleOneResource(
		Snapshot.SpiritStonesPerHour,
		Result.ElapsedSeconds,
		Snapshot.SpiritStoneCapacity,
		State.StoredSpiritStones,
		State.SpiritStoneFraction,
		State.TotalSpiritStonesProduced,
		Result.AddedSpiritStones,
		Result.DiscardedSpiritStones);
	SettleOneResource(
		Snapshot.SpiritGrassPerHour,
		Result.ElapsedSeconds,
		Snapshot.SpiritGrassCapacity,
		State.StoredSpiritGrass,
		State.SpiritGrassFraction,
		State.TotalSpiritGrassProduced,
		Result.AddedSpiritGrass,
		Result.DiscardedSpiritGrass);
	SettleOneResource(
		Snapshot.OrePerHour,
		Result.ElapsedSeconds,
		Snapshot.OreCapacity,
		State.StoredOre,
		State.OreFraction,
		State.TotalOreProduced,
		Result.AddedOre,
		Result.DiscardedOre);

	// Advance even at full capacity so collecting later cannot release historical overflow.
	State.LastSettlementUtcTicks = CurrentUtcTicks;
	State.Revision = SaturatingIncrement(State.Revision);
	Result.bStateChanged = true;
	return Result;
}

FImmortalCaveCollectionResult UImmortalCaveLibrary::CollectStoredResources(
	FImmortalCaveState& State,
	TArray<FImmortalMaterialStack>& Materials,
	int32& SpiritStones)
{
	FImmortalCaveState NewState = State;
	TArray<FImmortalMaterialStack> NewMaterials = Materials;
	int32 NewSpiritStones = FMath::Max(SpiritStones, 0);
	const bool bNormalized = NormalizeState(NewState, State.LastSettlementUtcTicks);

	FImmortalCaveCollectionResult Result;
	const int32 StoneCapacity = MAX_int32 - NewSpiritStones;
	Result.SpiritStonesCollected = FMath::Min(NewState.StoredSpiritStones, StoneCapacity);
	NewSpiritStones += Result.SpiritStonesCollected;
	Result.SpiritGrassCollected = UImmortalMaterialLibrary::AddMaterialStack(
		NewMaterials, CaveSpiritGrassId, NewState.StoredSpiritGrass);
	Result.OreCollected = UImmortalMaterialLibrary::AddMaterialStack(
		NewMaterials, CaveOreId, NewState.StoredOre);
	Result.bCollectedAnything = Result.SpiritStonesCollected > 0
		|| Result.SpiritGrassCollected > 0
		|| Result.OreCollected > 0;
	if (!Result.bCollectedAnything)
	{
		if (bNormalized) State = MoveTemp(NewState);
		return Result;
	}

	NewState.StoredSpiritStones -= Result.SpiritStonesCollected;
	NewState.StoredSpiritGrass -= Result.SpiritGrassCollected;
	NewState.StoredOre -= Result.OreCollected;
	NewState.TotalSpiritStonesCollected = SaturatingAdd(
		NewState.TotalSpiritStonesCollected, Result.SpiritStonesCollected);
	NewState.TotalSpiritGrassCollected = SaturatingAdd(
		NewState.TotalSpiritGrassCollected, Result.SpiritGrassCollected);
	NewState.TotalOreCollected = SaturatingAdd(NewState.TotalOreCollected, Result.OreCollected);
	NewState.CollectionCount = SaturatingIncrement(NewState.CollectionCount);
	NewState.Revision = SaturatingIncrement(NewState.Revision);

	State = MoveTemp(NewState);
	Materials = MoveTemp(NewMaterials);
	SpiritStones = NewSpiritStones;
	return Result;
}

FImmortalCraftingCost UImmortalCaveLibrary::ApplyForgeDiscount(
	const FImmortalCraftingCost& Cost,
	const FImmortalCaveState& State)
{
	FImmortalCraftingCost Result = Cost;
	if (Result.SpiritStones <= 0)
	{
		Result.SpiritStones = FMath::Max(Result.SpiritStones, 0);
		return Result;
	}
	const float Discount = GetProductionSnapshot(State).ForgeSpiritStoneDiscount;
	const double DiscountedCost = FMath::CeilToDouble(
		static_cast<double>(Result.SpiritStones) * (1.0 - static_cast<double>(Discount)));
	Result.SpiritStones = static_cast<int32>(FMath::Clamp<int64>(
		DiscountedCost >= static_cast<double>(MAX_int32)
			? static_cast<int64>(MAX_int32)
			: static_cast<int64>(DiscountedCost),
		1,
		static_cast<int64>(MAX_int32)));
	return Result;
}

FText UImmortalCaveLibrary::GetCurrentEffectText(
	const FImmortalCaveState& State,
	const EImmortalCaveBuildingType Type)
{
	if (!IsKnownBuildingType(Type)) return FText::FromString(TEXT("无效建筑"));
	const FImmortalCaveProductionSnapshot Snapshot = GetProductionSnapshot(State);
	switch (Type)
	{
	case EImmortalCaveBuildingType::CaveHeart:
		return FText::Format(
			FText::FromString(TEXT("全局产出 x{0}（+{1}%）")),
			FText::AsNumber(Snapshot.GlobalProductionMultiplier),
			FText::AsNumber(FMath::RoundToInt((Snapshot.GlobalProductionMultiplier - 1.0f) * 100.0f)));
	case EImmortalCaveBuildingType::MeditationRoom:
		return FText::Format(
			FText::FromString(TEXT("修炼速度 x{0}（+{1}%）")),
			FText::AsNumber(Snapshot.CultivationRateMultiplier),
			FText::AsNumber(FMath::RoundToInt((Snapshot.CultivationRateMultiplier - 1.0f) * 100.0f)));
	case EImmortalCaveBuildingType::SpiritVein:
		return FText::Format(
			FText::FromString(TEXT("每小时：灵石 {0}，矿石 {1}")),
			FText::AsNumber(Snapshot.SpiritStonesPerHour),
			FText::AsNumber(Snapshot.OrePerHour));
	case EImmortalCaveBuildingType::StoragePavilion:
		return FText::Format(
			FText::FromString(TEXT("最多积累 {0} 小时")),
			FText::AsNumber(Snapshot.StorageHours));
	case EImmortalCaveBuildingType::AlchemyRoom:
		return FText::Format(
			FText::FromString(TEXT("成丹率 +{0}%，极品率 +{1}%")),
			FText::AsNumber(Snapshot.AlchemySuccessChanceBonus * 100.0f),
			FText::AsNumber(Snapshot.AlchemyExceptionalChanceBonus * 100.0f));
	case EImmortalCaveBuildingType::ForgeRoom:
		return FText::Format(
			FText::FromString(TEXT("炼器灵石消耗 -{0}%")),
			FText::AsNumber(Snapshot.ForgeSpiritStoneDiscount * 100.0f));
	case EImmortalCaveBuildingType::SpiritField:
	{
		const int32 FieldLevel = GetBuildingLevel(State, EImmortalCaveBuildingType::SpiritField);
		const int32 UnlockedPlots = FMath::Clamp(2 + FMath::Max(FieldLevel - 1, 0) / 4, 2, 6);
		const float GrowthMultiplier = 1.0f + 0.03f * FMath::Max(FieldLevel - 1, 0);
		return FText::Format(
			FText::FromString(TEXT("每小时自然培育灵草 {0}；地块 {1}/6；生长 x{2}")),
			FText::AsNumber(Snapshot.SpiritGrassPerHour),
			FText::AsNumber(UnlockedPlots),
			FText::AsNumber(GrowthMultiplier));
	}
	default:
		return FText::GetEmpty();
	}
}

FText UImmortalCaveLibrary::GetNextEffectText(
	const FImmortalCaveState& State,
	const EImmortalCaveBuildingType Type)
{
	FImmortalCaveBuildingDefinition Definition;
	if (!GetBuildingDefinition(Type, Definition)) return FText::FromString(TEXT("无效建筑"));
	const int32 CurrentLevel = GetBuildingLevel(State, Type);
	if (CurrentLevel >= Definition.MaximumLevel) return FText::FromString(TEXT("已满级"));
	if (Type != EImmortalCaveBuildingType::CaveHeart
		&& CurrentLevel + 1 > GetBuildingLevel(State, EImmortalCaveBuildingType::CaveHeart))
	{
		return FText::Format(
			FText::FromString(TEXT("需先将洞府核心升至 {0} 级")),
			FText::AsNumber(CurrentLevel + 1));
	}

	FImmortalCaveState NextState = State;
	NormalizeState(NextState, State.LastSettlementUtcTicks);
	if (FImmortalCaveBuildingProgress* Building = NextState.Buildings.FindByPredicate(
		[Type](const FImmortalCaveBuildingProgress& Entry)
		{
			return Entry.Type == Type;
		}))
	{
		Building->Level = CurrentLevel + 1;
	}
	return GetCurrentEffectText(NextState, Type);
}
