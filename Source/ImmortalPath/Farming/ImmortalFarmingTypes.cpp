// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalFarmingTypes.h"

#include "Engine/DataTable.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Timespan.h"

namespace
{
	constexpr int32 MaximumPlotCount = 6;
	constexpr int32 MinimumSpiritFieldLevel = 1;
	constexpr int32 MaximumSpiritFieldLevel = 20;
	constexpr int32 PermilleScale = 1000;

	const FName SpiritGrassCropId(TEXT("SpiritGrassCrop"));
	const FName ImmortalFruitCropId(TEXT("ImmortalFruitCrop"));
	const FName SpiritWoodCropId(TEXT("SpiritWoodCrop"));

	FImmortalCraftingMaterialCost Ingredient(const TCHAR* MaterialId, const int32 Quantity)
	{
		FImmortalCraftingMaterialCost Result;
		Result.MaterialId = MaterialId;
		Result.Quantity = Quantity;
		return Result;
	}

	FImmortalCraftingCost PlantingCost(
		const int32 SpiritStones,
		const std::initializer_list<FImmortalCraftingMaterialCost> Materials = {})
	{
		FImmortalCraftingCost Result;
		Result.SpiritStones = SpiritStones;
		for (const FImmortalCraftingMaterialCost& Material : Materials)
		{
			Result.Materials.Add(Material);
		}
		return Result;
	}

	FImmortalFarmingCropDefinition CropDefinition(
		const TCHAR* DisplayName,
		const TCHAR* Description,
		const TCHAR* OutputMaterialId,
		const FImmortalCraftingCost& Cost,
		const int32 BaseGrowthSeconds,
		const int32 MinimumBaseYield,
		const int32 MaximumBaseYield,
		const int32 RequiredSpiritFieldLevel,
		const FLinearColor& DisplayColor)
	{
		FImmortalFarmingCropDefinition Result;
		Result.DisplayName = FText::FromString(DisplayName);
		Result.Description = FText::FromString(Description);
		Result.OutputMaterialId = OutputMaterialId;
		Result.PlantingCost = Cost;
		Result.BaseGrowthSeconds = BaseGrowthSeconds;
		Result.MinimumBaseYield = MinimumBaseYield;
		Result.MaximumBaseYield = MaximumBaseYield;
		Result.RequiredSpiritFieldLevel = RequiredSpiritFieldLevel;
		Result.DisplayColor = DisplayColor;
		return Result;
	}

	const TArray<FName>& KnownCropIds()
	{
		static const TArray<FName> Result =
		{
			SpiritGrassCropId,
			ImmortalFruitCropId,
			SpiritWoodCropId
		};
		return Result;
	}

	const TMap<FName, FImmortalFarmingCropDefinition>& FallbackCropCatalog()
	{
		static const TMap<FName, FImmortalFarmingCropDefinition> Result =
		{
			{SpiritGrassCropId, CropDefinition(
				TEXT("灵草"),
				TEXT("最基础的灵植，成熟较快，是炼丹与洞府建设的常用材料。"),
				TEXT("SpiritGrass"),
				PlantingCost(3),
				300,
				3,
				4,
				1,
				FLinearColor(0.28f, 0.95f, 0.38f))},
			{ImmortalFruitCropId, CropDefinition(
				TEXT("仙果"),
				TEXT("吸纳灵田精华结出的仙果，可用于高阶炼丹并出售换取灵石。"),
				TEXT("ImmortalFruit"),
				PlantingCost(8, {Ingredient(TEXT("SpiritGrass"), 1)}),
				900,
				2,
				3,
				5,
				FLinearColor(1.0f, 0.48f, 0.32f))},
			{SpiritWoodCropId, CropDefinition(
				TEXT("灵木"),
				TEXT("经灵气淬养的木材，成长缓慢，可用于炼器与洞府建设。"),
				TEXT("SpiritWood"),
				PlantingCost(12, {
					Ingredient(TEXT("SpiritGrass"), 1),
					Ingredient(TEXT("Ore"), 1)}),
				1800,
				2,
				3,
				9,
				FLinearColor(0.67f, 0.86f, 0.34f))}
		};
		return Result;
	}

	UDataTable* GetOptionalCropTable()
	{
		static TWeakObjectPtr<UDataTable> CachedTable;
		static bool bAttemptedLoad = false;
		if (!bAttemptedLoad)
		{
			bAttemptedLoad = true;
			const FString PackageName(TEXT("/Game/GAME/Data/DT_FarmingCrops"));
			if (FPackageName::DoesPackageExist(PackageName))
			{
				CachedTable = LoadObject<UDataTable>(
					nullptr,
					TEXT("/Game/GAME/Data/DT_FarmingCrops.DT_FarmingCrops"));
			}
		}
		return CachedTable.Get();
	}

	bool IsKnownCropId(const FName CropId)
	{
		return FallbackCropCatalog().Contains(CropId);
	}

	int32 ClampSpiritFieldLevel(const int32 SpiritFieldLevel)
	{
		return FMath::Clamp(SpiritFieldLevel, MinimumSpiritFieldLevel, MaximumSpiritFieldLevel);
	}

	int32 GetGrowthSpeedPermille(const int32 SpiritFieldLevel)
	{
		return PermilleScale + 30 * (ClampSpiritFieldLevel(SpiritFieldLevel) - 1);
	}

	int32 GetYieldLevelBonus(const int32 SpiritFieldLevel)
	{
		return (ClampSpiritFieldLevel(SpiritFieldLevel) - 1) / 4;
	}

	int64 GetDefinitionGrowthTicks(const FImmortalFarmingCropDefinition& Definition)
	{
		return static_cast<int64>(FMath::Max(Definition.BaseGrowthSeconds, 1)) * ETimespan::TicksPerSecond;
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

	int64 SaturatingMultiply(const int64 Left, const int32 Right)
	{
		if (Left <= 0 || Right <= 0) return 0;
		return Left > MAX_int64 / Right ? MAX_int64 : Left * Right;
	}

	int64 ScaleElapsedTicks(
		const int64 ElapsedTicks,
		const int32 SpeedPermille,
		const int32 PreviousRemainder,
		int32& OutRemainder)
	{
		if (ElapsedTicks <= 0)
		{
			OutRemainder = FMath::Clamp(PreviousRemainder, 0, PermilleScale - 1);
			return 0;
		}

		const int64 WholeThousands = ElapsedTicks / PermilleScale;
		const int64 PartialTicks = ElapsedTicks % PermilleScale;
		const int64 WholeProgress = SaturatingMultiply(WholeThousands, SpeedPermille);
		const int64 PartialNumerator = PartialTicks * SpeedPermille
			+ FMath::Clamp(PreviousRemainder, 0, PermilleScale - 1);
		const int64 PartialProgress = PartialNumerator / PermilleScale;
		OutRemainder = static_cast<int32>(PartialNumerator % PermilleScale);
		return WholeProgress > MAX_int64 - PartialProgress
			? MAX_int64
			: WholeProgress + PartialProgress;
	}

	bool IsPlotStateEqual(
		const FImmortalFarmingPlotState& Left,
		const FImmortalFarmingPlotState& Right)
	{
		return Left.CropId == Right.CropId
			&& Left.TotalGrowthTicks == Right.TotalGrowthTicks
			&& Left.RemainingGrowthTicks == Right.RemainingGrowthTicks
			&& Left.GrowthProgressPermilleRemainder == Right.GrowthProgressPermilleRemainder
			&& Left.PendingYield == Right.PendingYield
			&& Left.PlantedSpiritFieldLevel == Right.PlantedSpiritFieldLevel;
	}

	void ResetPlot(FImmortalFarmingPlotState& Plot)
	{
		Plot = FImmortalFarmingPlotState();
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
		TMap<FName, int32> AggregatedMaterials;
		if (SpiritStones < 0
			|| !AggregateCost(Cost, AggregatedMaterials)
			|| SpiritStones < Cost.SpiritStones)
		{
			return false;
		}
		for (const TPair<FName, int32>& Entry : AggregatedMaterials)
		{
			if (UImmortalMaterialLibrary::GetMaterialQuantity(Materials, Entry.Key) < Entry.Value)
			{
				return false;
			}
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
		TMap<FName, int32> AggregatedMaterials;
		if (!AggregateCost(Cost, AggregatedMaterials)) return false;
		for (const TPair<FName, int32>& Entry : AggregatedMaterials)
		{
			if (!UImmortalMaterialLibrary::RemoveMaterialStack(NewMaterials, Entry.Key, Entry.Value))
			{
				return false;
			}
		}
		NewSpiritStones -= Cost.SpiritStones;
		Materials = MoveTemp(NewMaterials);
		SpiritStones = NewSpiritStones;
		return true;
	}

	int32 RollFrozenYield(
		const FName CropId,
		const FImmortalFarmingCropDefinition& Definition,
		const int32 PlotIndex,
		const int32 SpiritFieldLevel,
		const int64 CurrentUtcTicks)
	{
		const uint32 Seed = HashCombine(
			GetTypeHash(CropId),
			HashCombine(GetTypeHash(PlotIndex), GetTypeHash(CurrentUtcTicks)));
		FRandomStream Stream(static_cast<int32>(Seed));
		const int32 MinimumYield = FMath::Max(Definition.MinimumBaseYield, 1);
		const int32 MaximumYield = FMath::Max(Definition.MaximumBaseYield, MinimumYield);
		return Stream.RandRange(MinimumYield, MaximumYield) + GetYieldLevelBonus(SpiritFieldLevel);
	}
}

TArray<FName> UImmortalFarmingLibrary::GetKnownCropIds()
{
	return KnownCropIds();
}

bool UImmortalFarmingLibrary::GetCropDefinition(
	const FName CropId,
	FImmortalFarmingCropDefinition& OutDefinition)
{
	if (!IsKnownCropId(CropId)) return false;
	if (const UDataTable* Table = GetOptionalCropTable())
	{
		if (const FImmortalFarmingCropDefinition* Row = Table->FindRow<FImmortalFarmingCropDefinition>(
			CropId, TEXT("Farming crop lookup"), false))
		{
			OutDefinition = *Row;
			OutDefinition.BaseGrowthSeconds = FMath::Max(OutDefinition.BaseGrowthSeconds, 1);
			OutDefinition.MinimumBaseYield = FMath::Max(OutDefinition.MinimumBaseYield, 1);
			OutDefinition.MaximumBaseYield = FMath::Max(
				OutDefinition.MaximumBaseYield, OutDefinition.MinimumBaseYield);
			OutDefinition.RequiredSpiritFieldLevel = FMath::Clamp(
				OutDefinition.RequiredSpiritFieldLevel,
				MinimumSpiritFieldLevel,
				MaximumSpiritFieldLevel);
			return !OutDefinition.OutputMaterialId.IsNone();
		}
	}
	OutDefinition = FallbackCropCatalog().FindChecked(CropId);
	return true;
}

int32 UImmortalFarmingLibrary::GetMaximumPlotCount()
{
	return MaximumPlotCount;
}

int32 UImmortalFarmingLibrary::GetUnlockedPlotCount(const int32 SpiritFieldLevel)
{
	if (SpiritFieldLevel < MinimumSpiritFieldLevel) return 0;
	return FMath::Clamp(2 + (ClampSpiritFieldLevel(SpiritFieldLevel) - 1) / 4, 2, MaximumPlotCount);
}

int32 UImmortalFarmingLibrary::GetRequiredFieldLevelForPlot(const int32 PlotIndex)
{
	if (PlotIndex < 0 || PlotIndex >= MaximumPlotCount) return 0;
	return PlotIndex < 2 ? 1 : 1 + 4 * (PlotIndex - 1);
}

float UImmortalFarmingLibrary::GetGrowthSpeedMultiplier(const int32 SpiritFieldLevel)
{
	return static_cast<float>(GetGrowthSpeedPermille(SpiritFieldLevel)) / PermilleScale;
}

FImmortalFarmingState UImmortalFarmingLibrary::CreateDefaultState(const int64 InitialUtcTicks)
{
	FImmortalFarmingState Result;
	Result.bInitialized = true;
	Result.Plots.SetNum(MaximumPlotCount);
	Result.LastSettlementUtcTicks = InitialUtcTicks > 0
		? InitialUtcTicks
		: FDateTime::UtcNow().GetTicks();
	Result.Revision = 1;
	return Result;
}

bool UImmortalFarmingLibrary::NormalizeState(
	FImmortalFarmingState& State,
	const int64 DefaultUtcTicks)
{
	if (!State.bInitialized)
	{
		State = CreateDefaultState(DefaultUtcTicks);
		return true;
	}

	bool bChanged = false;
	if (State.Plots.Num() != MaximumPlotCount)
	{
		State.Plots.SetNum(MaximumPlotCount);
		bChanged = true;
	}

	for (FImmortalFarmingPlotState& Plot : State.Plots)
	{
		const FImmortalFarmingPlotState Previous = Plot;
		if (Plot.CropId.IsNone())
		{
			ResetPlot(Plot);
		}
		else
		{
			FImmortalFarmingCropDefinition Definition;
			if (!GetCropDefinition(Plot.CropId, Definition) || Plot.PendingYield <= 0)
			{
				ResetPlot(Plot);
			}
			else
			{
				Plot.PlantedSpiritFieldLevel = ClampSpiritFieldLevel(Plot.PlantedSpiritFieldLevel);
				Plot.TotalGrowthTicks = GetDefinitionGrowthTicks(Definition);
				Plot.RemainingGrowthTicks = FMath::Clamp<int64>(
					Plot.RemainingGrowthTicks, 0, Plot.TotalGrowthTicks);
				const int32 MaximumFrozenYield = FMath::Max(
					Definition.MaximumBaseYield,
					Definition.MinimumBaseYield)
					+ GetYieldLevelBonus(Plot.PlantedSpiritFieldLevel);
				Plot.PendingYield = FMath::Clamp(Plot.PendingYield, 1, MaximumFrozenYield);
				Plot.GrowthProgressPermilleRemainder = Plot.RemainingGrowthTicks <= 0
					? 0
					: FMath::Clamp(Plot.GrowthProgressPermilleRemainder, 0, PermilleScale - 1);
			}
		}
		if (!IsPlotStateEqual(Plot, Previous)) bChanged = true;
	}

	if (State.LastSettlementUtcTicks <= 0)
	{
		State.LastSettlementUtcTicks = DefaultUtcTicks > 0
			? DefaultUtcTicks
			: FDateTime::UtcNow().GetTicks();
		bChanged = true;
	}
	if (State.TotalCropsPlanted < 0)
	{
		State.TotalCropsPlanted = 0;
		bChanged = true;
	}
	if (State.TotalCropsHarvested < 0)
	{
		State.TotalCropsHarvested = 0;
		bChanged = true;
	}
	if (State.TotalItemsHarvested < 0)
	{
		State.TotalItemsHarvested = 0;
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

FImmortalFarmingSettlementResult UImmortalFarmingLibrary::SettleGrowth(
	FImmortalFarmingState& State,
	const int32 SpiritFieldLevel,
	const int64 CurrentUtcTicks)
{
	FImmortalFarmingSettlementResult Result;
	Result.bStateChanged = NormalizeState(State, CurrentUtcTicks);
	if (CurrentUtcTicks <= 0)
	{
		Result.Message = FText::FromString(TEXT("无效的种植结算时间。"));
		return Result;
	}
	if (CurrentUtcTicks < State.LastSettlementUtcTicks)
	{
		Result.bClockRollbackDetected = true;
		Result.Message = FText::FromString(TEXT("检测到系统时间倒退，灵田成长已暂停。"));
		return Result;
	}
	if (CurrentUtcTicks == State.LastSettlementUtcTicks)
	{
		Result.Message = FText::FromString(TEXT("灵田已经结算到当前时间。"));
		return Result;
	}

	const int64 ElapsedTicks = CurrentUtcTicks - State.LastSettlementUtcTicks;
	Result.ElapsedSeconds = static_cast<double>(ElapsedTicks)
		/ static_cast<double>(ETimespan::TicksPerSecond);
	const int32 SpeedPermille = GetGrowthSpeedPermille(SpiritFieldLevel);
	for (FImmortalFarmingPlotState& Plot : State.Plots)
	{
		if (Plot.IsEmpty() || Plot.RemainingGrowthTicks <= 0) continue;
		++Result.GrowingPlotCount;
		int32 NewRemainder = Plot.GrowthProgressPermilleRemainder;
		const int64 ProgressTicks = ScaleElapsedTicks(
			ElapsedTicks,
			SpeedPermille,
			Plot.GrowthProgressPermilleRemainder,
			NewRemainder);
		if (ProgressTicks >= Plot.RemainingGrowthTicks)
		{
			Plot.RemainingGrowthTicks = 0;
			Plot.GrowthProgressPermilleRemainder = 0;
			++Result.MaturedPlotCount;
		}
		else
		{
			Plot.RemainingGrowthTicks -= ProgressTicks;
			Plot.GrowthProgressPermilleRemainder = NewRemainder;
		}
		Result.bPlotStateChanged = true;
	}

	// Always advance the independent high-water mark. Otherwise historical idle
	// time could be applied to a crop planted into a previously empty field.
	State.LastSettlementUtcTicks = CurrentUtcTicks;
	State.Revision = SaturatingIncrement(State.Revision);
	Result.bStateChanged = true;
	Result.Message = Result.MaturedPlotCount > 0
		? FText::Format(
			FText::FromString(TEXT("灵田结算完成，{0} 块作物已经成熟。")),
			FText::AsNumber(Result.MaturedPlotCount))
		: FText::FromString(TEXT("灵田成长进度已更新。"));
	return Result;
}

FImmortalFarmingPlotView UImmortalFarmingLibrary::GetPlotView(
	const FImmortalFarmingState& State,
	const int32 PlotIndex,
	const int32 SpiritFieldLevel,
	const int64 CurrentUtcTicks)
{
	FImmortalFarmingPlotView Result;
	Result.PlotIndex = PlotIndex;
	Result.bValidPlot = PlotIndex >= 0 && PlotIndex < MaximumPlotCount;
	if (!Result.bValidPlot)
	{
		Result.Message = FText::FromString(TEXT("未知的灵田地块。"));
		return Result;
	}

	Result.RequiredSpiritFieldLevel = GetRequiredFieldLevelForPlot(PlotIndex);
	Result.bUnlocked = SpiritFieldLevel >= Result.RequiredSpiritFieldLevel;
	FImmortalFarmingState ProjectedState = State;
	NormalizeState(ProjectedState, CurrentUtcTicks);
	const FImmortalFarmingSettlementResult Settlement = SettleGrowth(
		ProjectedState, SpiritFieldLevel, CurrentUtcTicks);
	Result.bClockRollbackDetected = Settlement.bClockRollbackDetected;
	const FImmortalFarmingPlotState& Plot = ProjectedState.Plots[PlotIndex];
	Result.CropId = Plot.CropId;
	Result.TotalGrowthTicks = Plot.TotalGrowthTicks;
	Result.RemainingGrowthTicks = Plot.RemainingGrowthTicks;
	Result.RemainingSeconds = Plot.RemainingGrowthTicks <= 0
		? 0
		: (Plot.RemainingGrowthTicks + ETimespan::TicksPerSecond - 1) / ETimespan::TicksPerSecond;
	Result.PendingYield = Plot.PendingYield;

	if (Plot.IsEmpty())
	{
		Result.GrowthStage = EImmortalFarmingGrowthStage::Empty;
		Result.Message = Result.bUnlocked
			? FText::FromString(TEXT("空闲地块，可选择作物播种。"))
			: FText::Format(
				FText::FromString(TEXT("灵田达到 {0} 级后解锁。")),
				FText::AsNumber(Result.RequiredSpiritFieldLevel));
		return Result;
	}

	FImmortalFarmingCropDefinition Definition;
	if (GetCropDefinition(Plot.CropId, Definition)) Result.OutputMaterialId = Definition.OutputMaterialId;
	if (Plot.IsMature())
	{
		Result.GrowthStage = EImmortalFarmingGrowthStage::Mature;
		Result.ProgressPermille = PermilleScale;
		Result.Message = FText::Format(
			FText::FromString(TEXT("作物已成熟，可收获 {0} 份。")),
			FText::AsNumber(Plot.PendingYield));
		return Result;
	}

	if (Plot.TotalGrowthTicks > 0)
	{
		const int64 CompletedTicks = Plot.TotalGrowthTicks - Plot.RemainingGrowthTicks;
		Result.ProgressPermille = FMath::Clamp(
			FMath::FloorToInt32(
				static_cast<double>(CompletedTicks)
				/ static_cast<double>(Plot.TotalGrowthTicks)
				* PermilleScale),
			0,
			PermilleScale - 1);
	}
	if (Result.ProgressPermille < 334)
	{
		Result.GrowthStage = EImmortalFarmingGrowthStage::Seedling;
	}
	else if (Result.ProgressPermille < 667)
	{
		Result.GrowthStage = EImmortalFarmingGrowthStage::Growing;
	}
	else
	{
		Result.GrowthStage = EImmortalFarmingGrowthStage::Ripening;
	}
	Result.Message = Result.bClockRollbackDetected
		? FText::FromString(TEXT("系统时间倒退，成长倒计时暂时冻结。"))
		: FText::Format(
			FText::FromString(TEXT("距离成熟还需 {0}。")),
			FormatDuration(Plot.RemainingGrowthTicks));
	return Result;
}

FImmortalFarmingPlantResult UImmortalFarmingLibrary::EvaluatePlant(
	const FImmortalFarmingState& State,
	const int32 PlotIndex,
	const FName CropId,
	const int32 SpiritFieldLevel,
	const TArray<FImmortalMaterialStack>& Materials,
	const int32 SpiritStones,
	const int64 CurrentUtcTicks)
{
	FImmortalFarmingPlantResult Result;
	Result.PlotIndex = PlotIndex;
	Result.CropId = CropId;
	FImmortalFarmingCropDefinition Definition;
	Result.bKnownCrop = GetCropDefinition(CropId, Definition);
	if (!Result.bKnownCrop)
	{
		Result.Message = FText::FromString(TEXT("未知的灵田作物。"));
		return Result;
	}
	Result.Cost = Definition.PlantingCost;
	Result.GrowthDurationTicks = GetDefinitionGrowthTicks(Definition);
	Result.bValidPlot = PlotIndex >= 0 && PlotIndex < MaximumPlotCount;
	if (!Result.bValidPlot)
	{
		Result.Message = FText::FromString(TEXT("未知的灵田地块。"));
		return Result;
	}
	if (CurrentUtcTicks <= 0)
	{
		Result.Message = FText::FromString(TEXT("无效的播种时间。"));
		return Result;
	}

	FImmortalFarmingState EvaluatedState = State;
	NormalizeState(EvaluatedState, CurrentUtcTicks);
	const FImmortalFarmingSettlementResult Settlement = SettleGrowth(
		EvaluatedState, SpiritFieldLevel, CurrentUtcTicks);
	Result.bClockRollbackDetected = Settlement.bClockRollbackDetected;
	Result.bPlotUnlocked = SpiritFieldLevel >= GetRequiredFieldLevelForPlot(PlotIndex);
	Result.bCropUnlocked = SpiritFieldLevel >= Definition.RequiredSpiritFieldLevel;
	Result.bPlotEmpty = EvaluatedState.Plots[PlotIndex].IsEmpty();
	Result.bAffordable = CanAffordAggregated(Materials, SpiritStones, Result.Cost);
	Result.FrozenYield = RollFrozenYield(
		CropId, Definition, PlotIndex, SpiritFieldLevel, CurrentUtcTicks);
	Result.bCanPlant = !Result.bClockRollbackDetected
		&& Result.bPlotUnlocked
		&& Result.bCropUnlocked
		&& Result.bPlotEmpty
		&& Result.bAffordable;

	if (Result.bClockRollbackDetected)
	{
		Result.Message = FText::FromString(TEXT("系统时间尚未恢复，暂时不能播种。"));
	}
	else if (!Result.bPlotUnlocked)
	{
		Result.Message = FText::Format(
			FText::FromString(TEXT("该地块需要灵田达到 {0} 级。")),
			FText::AsNumber(GetRequiredFieldLevelForPlot(PlotIndex)));
	}
	else if (!Result.bCropUnlocked)
	{
		Result.Message = FText::Format(
			FText::FromString(TEXT("{0} 需要灵田达到 {1} 级。")),
			Definition.DisplayName,
			FText::AsNumber(Definition.RequiredSpiritFieldLevel));
	}
	else if (!Result.bPlotEmpty)
	{
		Result.Message = FText::FromString(TEXT("该地块已有作物。"));
	}
	else if (!Result.bAffordable)
	{
		Result.Message = FText::FromString(TEXT("播种所需的灵石或材料不足。"));
	}
	else
	{
		Result.Message = FText::FromString(TEXT("可以播种。"));
	}
	return Result;
}

FImmortalFarmingPlantResult UImmortalFarmingLibrary::TryPlantCrop(
	FImmortalFarmingState& State,
	const int32 PlotIndex,
	const FName CropId,
	const int32 SpiritFieldLevel,
	TArray<FImmortalMaterialStack>& Materials,
	int32& SpiritStones,
	const int64 CurrentUtcTicks)
{
	FImmortalFarmingState NewState = State;
	TArray<FImmortalMaterialStack> NewMaterials = Materials;
	int32 NewSpiritStones = SpiritStones;
	NormalizeState(NewState, CurrentUtcTicks);
	SettleGrowth(NewState, SpiritFieldLevel, CurrentUtcTicks);

	FImmortalFarmingPlantResult Result = EvaluatePlant(
		NewState,
		PlotIndex,
		CropId,
		SpiritFieldLevel,
		NewMaterials,
		NewSpiritStones,
		CurrentUtcTicks);
	if (!Result.bCanPlant) return Result;
	if (!ConsumeAggregatedCost(NewMaterials, NewSpiritStones, Result.Cost))
	{
		Result.bAffordable = false;
		Result.bCanPlant = false;
		Result.Message = FText::FromString(TEXT("播种事务验证失败。"));
		return Result;
	}

	FImmortalFarmingPlotState& Plot = NewState.Plots[PlotIndex];
	Plot.CropId = CropId;
	Plot.TotalGrowthTicks = Result.GrowthDurationTicks;
	Plot.RemainingGrowthTicks = Result.GrowthDurationTicks;
	Plot.GrowthProgressPermilleRemainder = 0;
	Plot.PendingYield = Result.FrozenYield;
	Plot.PlantedSpiritFieldLevel = ClampSpiritFieldLevel(SpiritFieldLevel);
	NewState.TotalCropsPlanted = SaturatingAdd(NewState.TotalCropsPlanted, 1);
	NewState.Revision = SaturatingIncrement(NewState.Revision);

	State = MoveTemp(NewState);
	Materials = MoveTemp(NewMaterials);
	SpiritStones = NewSpiritStones;
	Result.bSucceeded = true;
	Result.Message = FText::Format(
		FText::FromString(TEXT("播种成功，预计收获 {0} 份。")),
		FText::AsNumber(Result.FrozenYield));
	return Result;
}

FImmortalFarmingBatchPlantResult UImmortalFarmingLibrary::TryPlantAllEmpty(
	FImmortalFarmingState& State,
	const FName CropId,
	const int32 SpiritFieldLevel,
	TArray<FImmortalMaterialStack>& Materials,
	int32& SpiritStones,
	const int64 CurrentUtcTicks)
{
	FImmortalFarmingBatchPlantResult Result;
	Result.CropId = CropId;
	FImmortalFarmingState NewState = State;
	TArray<FImmortalMaterialStack> NewMaterials = Materials;
	int32 NewSpiritStones = SpiritStones;
	NormalizeState(NewState, CurrentUtcTicks);
	const FImmortalFarmingSettlementResult Settlement = SettleGrowth(
		NewState, SpiritFieldLevel, CurrentUtcTicks);
	if (Settlement.bClockRollbackDetected)
	{
		Result.Message = FText::FromString(TEXT("系统时间尚未恢复，暂时不能批量播种。"));
		return Result;
	}

	const int32 UnlockedPlotCount = GetUnlockedPlotCount(SpiritFieldLevel);
	for (int32 PlotIndex = 0; PlotIndex < UnlockedPlotCount; ++PlotIndex)
	{
		if (!NewState.Plots[PlotIndex].IsEmpty()) continue;
		++Result.EligiblePlotCount;
		FImmortalFarmingPlantResult PlantResult = TryPlantCrop(
			NewState,
			PlotIndex,
			CropId,
			SpiritFieldLevel,
			NewMaterials,
			NewSpiritStones,
			CurrentUtcTicks);
		if (PlantResult.bSucceeded) ++Result.PlantedPlotCount;
		Result.PlantResults.Add(MoveTemp(PlantResult));
	}

	Result.bSucceeded = Result.PlantedPlotCount > 0;
	Result.bAllEligiblePlotsPlanted = Result.EligiblePlotCount > 0
		&& Result.PlantedPlotCount == Result.EligiblePlotCount;
	if (!Result.bSucceeded)
	{
		Result.Message = Result.EligiblePlotCount <= 0
			? FText::FromString(TEXT("没有可播种的空闲地块。"))
			: FText::FromString(TEXT("没有任何地块成功播种。"));
		return Result;
	}

	State = MoveTemp(NewState);
	Materials = MoveTemp(NewMaterials);
	SpiritStones = NewSpiritStones;
	Result.Message = Result.bAllEligiblePlotsPlanted
		? FText::Format(
			FText::FromString(TEXT("已在全部 {0} 块空闲地块播种。")),
			FText::AsNumber(Result.PlantedPlotCount))
		: FText::Format(
			FText::FromString(TEXT("已播种 {0}/{1} 块地，剩余资源不足。")),
			FText::AsNumber(Result.PlantedPlotCount),
			FText::AsNumber(Result.EligiblePlotCount));
	return Result;
}

FImmortalFarmingHarvestResult UImmortalFarmingLibrary::TryHarvestPlot(
	FImmortalFarmingState& State,
	const int32 PlotIndex,
	const int32 SpiritFieldLevel,
	TArray<FImmortalMaterialStack>& Materials,
	const int64 CurrentUtcTicks)
{
	FImmortalFarmingHarvestResult Result;
	Result.PlotIndex = PlotIndex;
	Result.bValidPlot = PlotIndex >= 0 && PlotIndex < MaximumPlotCount;
	if (!Result.bValidPlot)
	{
		Result.Message = FText::FromString(TEXT("未知的灵田地块。"));
		return Result;
	}
	if (CurrentUtcTicks <= 0)
	{
		Result.Message = FText::FromString(TEXT("无效的收获时间。"));
		return Result;
	}

	FImmortalFarmingState NewState = State;
	TArray<FImmortalMaterialStack> NewMaterials = Materials;
	NormalizeState(NewState, CurrentUtcTicks);
	const FImmortalFarmingSettlementResult Settlement = SettleGrowth(
		NewState, SpiritFieldLevel, CurrentUtcTicks);
	Result.bClockRollbackDetected = Settlement.bClockRollbackDetected;
	FImmortalFarmingPlotState& Plot = NewState.Plots[PlotIndex];
	Result.CropId = Plot.CropId;
	Result.bWasReady = Plot.IsMature();
	if (!Result.bWasReady)
	{
		Result.Message = Plot.IsEmpty()
			? FText::FromString(TEXT("该地块没有作物。"))
			: FText::FromString(TEXT("作物尚未成熟。"));
		return Result;
	}

	FImmortalFarmingCropDefinition Definition;
	if (!GetCropDefinition(Plot.CropId, Definition))
	{
		Result.Message = FText::FromString(TEXT("作物定义已经失效，无法收获。"));
		return Result;
	}
	Result.OutputMaterialId = Definition.OutputMaterialId;
	Result.RequestedQuantity = Plot.PendingYield;
	Result.HarvestedQuantity = UImmortalMaterialLibrary::AddMaterialStack(
		NewMaterials, Result.OutputMaterialId, Result.RequestedQuantity);
	if (Result.HarvestedQuantity <= 0)
	{
		Result.RemainingQuantity = Result.RequestedQuantity;
		Result.Message = FText::FromString(TEXT("对应材料堆已满，成熟作物仍保留在地块中。"));
		return Result;
	}

	Plot.PendingYield -= Result.HarvestedQuantity;
	Result.RemainingQuantity = Plot.PendingYield;
	Result.bFullyHarvested = Plot.PendingYield <= 0;
	Result.bPartiallyHarvested = !Result.bFullyHarvested;
	if (Result.bFullyHarvested)
	{
		ResetPlot(Plot);
		NewState.TotalCropsHarvested = SaturatingAdd(NewState.TotalCropsHarvested, 1);
	}
	NewState.TotalItemsHarvested = SaturatingAdd(
		NewState.TotalItemsHarvested, Result.HarvestedQuantity);
	NewState.Revision = SaturatingIncrement(NewState.Revision);

	State = MoveTemp(NewState);
	Materials = MoveTemp(NewMaterials);
	Result.bSucceeded = true;
	Result.Message = Result.bFullyHarvested
		? FText::Format(
			FText::FromString(TEXT("收获成功，获得 {0} 份材料。")),
			FText::AsNumber(Result.HarvestedQuantity))
		: FText::Format(
			FText::FromString(TEXT("材料堆接近上限，本次收获 {0} 份，地块保留 {1} 份。")),
			FText::AsNumber(Result.HarvestedQuantity),
			FText::AsNumber(Result.RemainingQuantity));
	return Result;
}

FImmortalFarmingBatchHarvestResult UImmortalFarmingLibrary::TryHarvestAllReady(
	FImmortalFarmingState& State,
	const int32 SpiritFieldLevel,
	TArray<FImmortalMaterialStack>& Materials,
	const int64 CurrentUtcTicks)
{
	FImmortalFarmingBatchHarvestResult Result;
	if (CurrentUtcTicks <= 0)
	{
		Result.Message = FText::FromString(TEXT("无效的批量收获时间。"));
		return Result;
	}
	FImmortalFarmingState NewState = State;
	TArray<FImmortalMaterialStack> NewMaterials = Materials;
	NormalizeState(NewState, CurrentUtcTicks);
	SettleGrowth(NewState, SpiritFieldLevel, CurrentUtcTicks);

	for (int32 PlotIndex = 0; PlotIndex < MaximumPlotCount; ++PlotIndex)
	{
		if (!NewState.Plots[PlotIndex].IsMature()) continue;
		++Result.ReadyPlotCount;
		FImmortalFarmingHarvestResult HarvestResult = TryHarvestPlot(
			NewState,
			PlotIndex,
			SpiritFieldLevel,
			NewMaterials,
			CurrentUtcTicks);
		if (HarvestResult.bSucceeded)
		{
			Result.HarvestedItemCount = static_cast<int32>(FMath::Min<int64>(
				static_cast<int64>(Result.HarvestedItemCount) + HarvestResult.HarvestedQuantity,
				MAX_int32));
			if (HarvestResult.bFullyHarvested) ++Result.FullyHarvestedPlotCount;
			if (HarvestResult.bPartiallyHarvested) ++Result.PartiallyHarvestedPlotCount;
		}
		Result.HarvestResults.Add(MoveTemp(HarvestResult));
	}

	Result.bSucceeded = Result.HarvestedItemCount > 0;
	if (!Result.bSucceeded)
	{
		Result.Message = Result.ReadyPlotCount <= 0
			? FText::FromString(TEXT("没有已经成熟的作物。"))
			: FText::FromString(TEXT("材料堆已满，成熟作物均保留在原地。"));
		return Result;
	}

	State = MoveTemp(NewState);
	Materials = MoveTemp(NewMaterials);
	Result.Message = FText::Format(
		FText::FromString(TEXT("批量收获完成，共获得 {0} 份材料。")),
		FText::AsNumber(Result.HarvestedItemCount));
	return Result;
}

FText UImmortalFarmingLibrary::FormatDuration(const int64 DurationTicks)
{
	if (DurationTicks <= 0) return FText::FromString(TEXT("已成熟"));
	const int64 TotalSeconds = DurationTicks / ETimespan::TicksPerSecond
		+ (DurationTicks % ETimespan::TicksPerSecond == 0 ? 0 : 1);
	const int64 Days = TotalSeconds / 86400;
	const int64 Hours = TotalSeconds / 3600 % 24;
	const int64 Minutes = TotalSeconds / 60 % 60;
	const int64 Seconds = TotalSeconds % 60;
	if (Days > 0)
	{
		return FText::FromString(FString::Printf(
			TEXT("%lld天 %02lld:%02lld:%02lld"), Days, Hours, Minutes, Seconds));
	}
	if (Hours > 0)
	{
		return FText::FromString(FString::Printf(
			TEXT("%02lld:%02lld:%02lld"), Hours, Minutes, Seconds));
	}
	return FText::FromString(FString::Printf(TEXT("%02lld:%02lld"), Minutes, Seconds));
}
