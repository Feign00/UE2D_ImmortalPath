// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalShopTypes.h"

#include "../Crafting/ImmortalCraftingTypes.h"
#include "Math/RandomStream.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"

namespace
{
	int32 ClampPrice(const int64 Value)
	{
		return static_cast<int32>(FMath::Clamp<int64>(Value, 1, MAX_int32));
	}

	int32 RoundUpToFive(const double Value)
	{
		return ClampPrice(static_cast<int64>(FMath::CeilToDouble(FMath::Max(Value, 1.0) / 5.0) * 5.0));
	}

	double EquipmentQualityMultiplier(const EImmortalEquipmentQuality Quality)
	{
		switch (Quality)
		{
		case EImmortalEquipmentQuality::Uncommon: return 1.5;
		case EImmortalEquipmentQuality::Rare: return 2.4;
		case EImmortalEquipmentQuality::Epic: return 4.0;
		case EImmortalEquipmentQuality::Legendary: return 7.0;
		default: return 1.0;
		}
	}

	EImmortalEquipmentQuality GetMinimumShopQuality(const int32 Stage)
	{
		if (Stage >= 750) return EImmortalEquipmentQuality::Legendary;
		if (Stage >= 500) return EImmortalEquipmentQuality::Epic;
		if (Stage >= 200) return EImmortalEquipmentQuality::Rare;
		if (Stage >= 50) return EImmortalEquipmentQuality::Uncommon;
		return EImmortalEquipmentQuality::Common;
	}

	template <typename T>
	void ShuffleWithStream(TArray<T>& Values, FRandomStream& Stream)
	{
		for (int32 Index = Values.Num() - 1; Index > 0; --Index)
		{
			Values.Swap(Index, Stream.RandRange(0, Index));
		}
	}

	FGuid MakeListingGuid(const int32 DayKey, const int32 Serial, const int32 SlotIndex)
	{
		const uint32 A = static_cast<uint32>(FMath::Max(DayKey, 1));
		const uint32 B = static_cast<uint32>(FMath::Max(Serial, 0) + 1);
		const uint32 C = static_cast<uint32>(FMath::Max(SlotIndex, 0) + 1);
		const uint32 D = HashCombineFast(A, HashCombineFast(B, C));
		return FGuid(A, B, C, D == 0 ? 1u : D);
	}
}

bool FImmortalShopListing::IsValid() const
{
	if (!ListingId.IsValid() || BundleQuantity <= 0 || BundlePrice <= 0) return false;
	switch (ProductType)
	{
	case EImmortalShopProductType::Equipment:
		return BundleQuantity == 1 && EquipmentItem.IsValid();
	case EImmortalShopProductType::Material:
	{
		FImmortalMaterialDefinition Definition;
		return UImmortalMaterialLibrary::GetMaterialDefinition(ProductId, Definition);
	}
	case EImmortalShopProductType::Pill:
	{
		FImmortalPillDefinition Definition;
		return UImmortalAlchemyLibrary::GetPillDefinition(ProductId, Definition);
	}
	case EImmortalShopProductType::Artifact:
	{
		FImmortalArtifactDefinition Definition;
		return BundleQuantity == 1 && UImmortalArtifactLibrary::GetArtifactDefinition(ProductId, Definition);
	}
	default:
		return false;
	}
}

int32 UImmortalShopLibrary::GetDayKeyFromUtcTicks(const int64 UtcTicks, const int32 UtcOffsetMinutes)
{
	if (UtcTicks <= 0) return 0;
	const FDateTime Local = FDateTime(UtcTicks)
		+ FTimespan::FromMinutes(FMath::Clamp(UtcOffsetMinutes, -720, 840));
	return Local.GetYear() * 10000 + Local.GetMonth() * 100 + Local.GetDay();
}

int64 UImmortalShopLibrary::GetSecondsUntilNextRefresh(const int64 UtcTicks, const int32 UtcOffsetMinutes)
{
	if (UtcTicks <= 0) return 0;
	const FDateTime Local = FDateTime(UtcTicks)
		+ FTimespan::FromMinutes(FMath::Clamp(UtcOffsetMinutes, -720, 840));
	const FDateTime NextDay = FDateTime(Local.GetYear(), Local.GetMonth(), Local.GetDay()) + FTimespan::FromDays(1.0);
	return FMath::Clamp<int64>(FMath::CeilToInt64((NextDay - Local).GetTotalSeconds()), 0, 86400);
}

bool UImmortalShopLibrary::NeedsDailyRefresh(const FImmortalShopState& State, const int32 CurrentDayKey)
{
	if (CurrentDayKey <= 0) return false;
	if (State.Listings.IsEmpty() || State.RefreshDayKey <= 0) return true;
	// A clock rollback must not grant another free refresh.
	return CurrentDayKey > State.RefreshDayKey;
}

int32 UImmortalShopLibrary::GetManualRefreshCost(const FImmortalShopState& State)
{
	return 100 * FMath::Clamp(State.ManualRefreshCount + 1, 1, 50);
}

FImmortalShopState UImmortalShopLibrary::GenerateStock(
	const int32 QingyunStage,
	const int32 RealmIndex,
	const int32 MinorStage,
	const int32 DayKey,
	const int32 RefreshSerial)
{
	const int32 SafeStage = FMath::Clamp(QingyunStage, 1, 999);
	const int32 SafeRealm = FMath::Clamp(RealmIndex, 0, 9);
	const int32 SafeMinor = FMath::Clamp(MinorStage, 1, 10);
	FImmortalShopState Result;
	Result.RefreshDayKey = FMath::Max(DayKey, 1);
	Result.RefreshSerial = FMath::Max(RefreshSerial, 0);
	Result.ManualRefreshCount = FMath::Max(RefreshSerial, 0);
	FRandomStream Stream(HashCombineFast(
		static_cast<uint32>(Result.RefreshDayKey),
		HashCombineFast(static_cast<uint32>(Result.RefreshSerial + 1), static_cast<uint32>(SafeStage * 17 + SafeRealm * 7 + SafeMinor))));

	auto AddListing = [&Result](FImmortalShopListing Listing)
	{
		Listing.ListingId = MakeListingGuid(Result.RefreshDayKey, Result.RefreshSerial, Result.Listings.Num());
		Result.Listings.Add(MoveTemp(Listing));
	};

	const int32 EquipmentLevel = 1 + (SafeStage - 1) / 5;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FImmortalShopListing Listing;
		Listing.ProductType = EImmortalShopProductType::Equipment;
		Listing.EquipmentItem = UImmortalEquipmentLibrary::GenerateRandomEquipmentWithMinimumQuality(
			EquipmentLevel, GetMinimumShopQuality(SafeStage));
		Listing.BundlePrice = GetEquipmentBuyPrice(Listing.EquipmentItem);
		AddListing(MoveTemp(Listing));
	}

	TArray<FName> MaterialIds;
	for (const FName Id : UImmortalMaterialLibrary::GetKnownMaterialIds())
	{
		FImmortalMaterialDefinition Definition;
		if (UImmortalMaterialLibrary::GetMaterialDefinition(Id, Definition)
			&& SafeStage >= FMath::Max(Definition.MinimumQingyunStage, 1))
		{
			MaterialIds.Add(Id);
		}
	}
	ShuffleWithStream(MaterialIds, Stream);
	for (int32 Index = 0; Index < FMath::Min(4, MaterialIds.Num()); ++Index)
	{
		FImmortalShopListing Listing;
		Listing.ProductType = EImmortalShopProductType::Material;
		Listing.ProductId = MaterialIds[Index];
		Listing.BundleQuantity = Stream.RandRange(3, 7) + SafeStage / 100;
		Listing.BundlePrice = ClampPrice(
			static_cast<int64>(GetMaterialUnitBuyPrice(Listing.ProductId)) * Listing.BundleQuantity);
		AddListing(MoveTemp(Listing));
	}

	TArray<FName> PillIds;
	for (const FName Id : UImmortalAlchemyLibrary::GetKnownRecipeIds())
	{
		FImmortalPillDefinition Definition;
		if (UImmortalAlchemyLibrary::GetPillDefinition(Id, Definition)
			&& UImmortalAlchemyLibrary::IsRecipeUnlocked(Definition, SafeRealm, SafeMinor))
		{
			PillIds.Add(Id);
		}
	}
	ShuffleWithStream(PillIds, Stream);
	for (int32 Index = 0; Index < FMath::Min(2, PillIds.Num()); ++Index)
	{
		FImmortalShopListing Listing;
		Listing.ProductType = EImmortalShopProductType::Pill;
		Listing.ProductId = PillIds[Index];
		Listing.PillQuality = Stream.FRand() < 0.15f
			? EImmortalPillQuality::Exceptional
			: EImmortalPillQuality::Ordinary;
		Listing.BundleQuantity = Listing.PillQuality == EImmortalPillQuality::Exceptional ? 1 : 2;
		Listing.BundlePrice = ClampPrice(
			static_cast<int64>(GetPillUnitBuyPrice(Listing.ProductId, Listing.PillQuality)) * Listing.BundleQuantity);
		AddListing(MoveTemp(Listing));
	}

	TArray<FName> ArtifactIds;
	for (const FName Id : UImmortalArtifactLibrary::GetKnownArtifactIds())
	{
		FImmortalArtifactDefinition Definition;
		if (UImmortalArtifactLibrary::GetArtifactDefinition(Id, Definition)
			&& SafeStage >= FMath::Max(Definition.MinimumQingyunStage, 1))
		{
			ArtifactIds.Add(Id);
		}
	}
	ShuffleWithStream(ArtifactIds, Stream);
	if (!ArtifactIds.IsEmpty())
	{
		FImmortalShopListing Listing;
		Listing.ProductType = EImmortalShopProductType::Artifact;
		Listing.ProductId = ArtifactIds[0];
		Listing.BundlePrice = GetArtifactBuyPrice(Listing.ProductId);
		AddListing(MoveTemp(Listing));
	}

	NormalizeState(Result);
	return Result;
}

void UImmortalShopLibrary::NormalizeState(FImmortalShopState& State)
{
	State.RefreshDayKey = FMath::Max(State.RefreshDayKey, 0);
	State.RefreshSerial = FMath::Max(State.RefreshSerial, 0);
	State.ManualRefreshCount = FMath::Max(State.ManualRefreshCount, 0);
	TSet<FGuid> Seen;
	for (FImmortalShopListing& Listing : State.Listings)
	{
		Listing.BundleQuantity = FMath::Clamp(Listing.BundleQuantity, 1, 9999);
		Listing.BundlePrice = FMath::Clamp(Listing.BundlePrice, 1, MAX_int32);
		if (Listing.ProductType == EImmortalShopProductType::Equipment)
		{
			UImmortalEquipmentLibrary::NormalizeForgingState(Listing.EquipmentItem);
			Listing.BundleQuantity = 1;
		}
		if (!Listing.ListingId.IsValid()) Listing.ListingId = FGuid::NewGuid();
	}
	State.Listings.RemoveAll([&Seen](const FImmortalShopListing& Listing)
	{
		if (!Listing.IsValid() || Seen.Contains(Listing.ListingId)) return true;
		Seen.Add(Listing.ListingId);
		return false;
	});
}

int32 UImmortalShopLibrary::GetEquipmentBuyPrice(const FImmortalEquipmentItem& Item)
{
	if (!Item.IsValid()) return 0;
	const double Base = 100.0 + 20.0 * FMath::Max(Item.ItemLevel, 1);
	const double Value = Base * EquipmentQualityMultiplier(Item.Quality)
		* (1.0 + 0.12 * FMath::Clamp(Item.EnhancementLevel, 0, 15));
	return RoundUpToFive(Value);
}

int32 UImmortalShopLibrary::GetEquipmentSellPrice(const FImmortalEquipmentItem& Item)
{
	const int32 BuyValue = GetEquipmentBuyPrice(Item);
	return BuyValue <= 0 ? 0 : FMath::Max(BuyValue / 4, 1);
}

int32 UImmortalShopLibrary::GetMaterialUnitBuyPrice(const FName MaterialId)
{
	if (MaterialId == TEXT("SpiritGrass")) return 8;
	if (MaterialId == TEXT("Ore")) return 10;
	if (MaterialId == TEXT("DemonCore")) return 12;
	if (MaterialId == TEXT("SpiritLiquid")) return 15;
	if (MaterialId == TEXT("DemonBone")) return 25;
	if (MaterialId == TEXT("SpiritIron")) return 50;
	if (MaterialId == TEXT("ArtifactFragment")) return 80;
	FImmortalMaterialDefinition Definition;
	return UImmortalMaterialLibrary::GetMaterialDefinition(MaterialId, Definition)
		? FMath::Max(8 + Definition.MinimumQingyunStage / 5, 1)
		: 0;
}

int32 UImmortalShopLibrary::GetMaterialUnitSellPrice(const FName MaterialId)
{
	const int32 BuyValue = GetMaterialUnitBuyPrice(MaterialId);
	return BuyValue <= 0 ? 0 : FMath::Max(BuyValue / 4, 1);
}

int32 UImmortalShopLibrary::GetPillUnitBuyPrice(const FName PillId, const EImmortalPillQuality Quality)
{
	FImmortalPillDefinition Definition;
	if (!UImmortalAlchemyLibrary::GetPillDefinition(PillId, Definition)) return 0;
	int64 IngredientValue = 0;
	for (const FImmortalAlchemyIngredient& Ingredient : Definition.Ingredients)
	{
		IngredientValue = FMath::Min<int64>(IngredientValue
			+ static_cast<int64>(GetMaterialUnitBuyPrice(Ingredient.MaterialId)) * FMath::Max(Ingredient.Quantity, 0), MAX_int32);
	}
	const double SuccessChance = FMath::Max(static_cast<double>(Definition.BaseSuccessChance), 0.05);
	double Value = static_cast<double>(IngredientValue) / SuccessChance * 1.5 + 25.0;
	if (Quality == EImmortalPillQuality::Exceptional) Value *= 2.5;
	return RoundUpToFive(Value);
}

int32 UImmortalShopLibrary::GetArtifactBuyPrice(const FName ArtifactId)
{
	FImmortalArtifactDefinition Definition;
	if (!UImmortalArtifactLibrary::GetArtifactDefinition(ArtifactId, Definition)) return 0;
	int64 Value = FMath::Max(Definition.CraftingCost.SpiritStones, 0);
	for (const FImmortalCraftingMaterialCost& Material : Definition.CraftingCost.Materials)
	{
		Value = FMath::Min<int64>(Value
			+ static_cast<int64>(GetMaterialUnitBuyPrice(Material.MaterialId)) * FMath::Max(Material.Quantity, 0), MAX_int32);
	}
	return RoundUpToFive(static_cast<double>(Value) * 1.5);
}

FText UImmortalShopLibrary::GetProductTypeText(const EImmortalShopProductType ProductType)
{
	switch (ProductType)
	{
	case EImmortalShopProductType::Equipment: return FText::FromString(TEXT("装备"));
	case EImmortalShopProductType::Material: return FText::FromString(TEXT("材料"));
	case EImmortalShopProductType::Pill: return FText::FromString(TEXT("丹药"));
	case EImmortalShopProductType::Artifact: return FText::FromString(TEXT("法宝"));
	default: return FText::FromString(TEXT("商品"));
	}
}

FText UImmortalShopLibrary::GetListingDisplayName(const FImmortalShopListing& Listing)
{
	switch (Listing.ProductType)
	{
	case EImmortalShopProductType::Equipment:
		return FText::FromName(Listing.EquipmentItem.DisplayName);
	case EImmortalShopProductType::Material:
	{
		FImmortalMaterialDefinition Definition;
		return UImmortalMaterialLibrary::GetMaterialDefinition(Listing.ProductId, Definition)
			? Definition.DisplayName : FText::FromName(Listing.ProductId);
	}
	case EImmortalShopProductType::Pill:
	{
		FImmortalPillDefinition Definition;
		if (!UImmortalAlchemyLibrary::GetPillDefinition(Listing.ProductId, Definition)) return FText::FromName(Listing.ProductId);
		return FText::FromString(FString::Printf(TEXT("%s·%s"),
			*UImmortalAlchemyLibrary::GetQualityText(Listing.PillQuality).ToString(), *Definition.DisplayName.ToString()));
	}
	case EImmortalShopProductType::Artifact:
	{
		FImmortalArtifactDefinition Definition;
		return UImmortalArtifactLibrary::GetArtifactDefinition(Listing.ProductId, Definition)
			? Definition.DisplayName : FText::FromName(Listing.ProductId);
	}
	default:
		return FText::FromString(TEXT("未知商品"));
	}
}

FText UImmortalShopLibrary::GetListingDetailText(const FImmortalShopListing& Listing)
{
	switch (Listing.ProductType)
	{
	case EImmortalShopProductType::Equipment:
		return FText::FromString(FString::Printf(
			TEXT("%s · %s · %d级\n战力 %.1f\n攻击 %.1f · 防御 %.1f · 生命 %.1f\n攻速 %.1f%% · 暴击 %.1f%%"),
			*UImmortalEquipmentLibrary::GetSlotText(Listing.EquipmentItem.Slot).ToString(),
			*UImmortalEquipmentLibrary::GetDisciplineText(Listing.EquipmentItem.Discipline).ToString(),
			Listing.EquipmentItem.ItemLevel,
			UImmortalEquipmentLibrary::CalculateEquipmentPower(Listing.EquipmentItem),
			Listing.EquipmentItem.AttackBonus, Listing.EquipmentItem.DefenseBonus, Listing.EquipmentItem.HealthBonus,
			Listing.EquipmentItem.AttackSpeedBonus * 100.0f, Listing.EquipmentItem.CriticalChanceBonus * 100.0f));
	case EImmortalShopProductType::Material:
	{
		FImmortalMaterialDefinition Definition;
		if (!UImmortalMaterialLibrary::GetMaterialDefinition(Listing.ProductId, Definition)) return FText::GetEmpty();
		return FText::FromString(FString::Printf(TEXT("%s\n分类：%s\n整组数量：%d"),
			*Definition.Description.ToString(), *UImmortalMaterialLibrary::GetCategoryText(Definition.Category).ToString(), Listing.BundleQuantity));
	}
	case EImmortalShopProductType::Pill:
	{
		FImmortalPillDefinition Definition;
		if (!UImmortalAlchemyLibrary::GetPillDefinition(Listing.ProductId, Definition)) return FText::GetEmpty();
		return FText::FromString(FString::Printf(TEXT("%s\n%s\n整组数量：%d"),
			*Definition.Description.ToString(),
			*UImmortalAlchemyLibrary::GetEffectText(Definition, Listing.PillQuality).ToString(), Listing.BundleQuantity));
	}
	case EImmortalShopProductType::Artifact:
	{
		FImmortalArtifactDefinition Definition;
		if (!UImmortalArtifactLibrary::GetArtifactDefinition(Listing.ProductId, Definition)) return FText::GetEmpty();
		const FImmortalArtifactItem Preview = UImmortalArtifactLibrary::CreateArtifact(Listing.ProductId);
		return FText::FromString(FString::Printf(TEXT("%s\n%s\n%s"), *Definition.Description.ToString(),
			*UImmortalArtifactLibrary::GetActiveEffectText(Preview).ToString(),
			*UImmortalArtifactLibrary::GetPassiveEffectText(Preview).ToString()));
	}
	default:
		return FText::GetEmpty();
	}
}

FLinearColor UImmortalShopLibrary::GetListingColor(const FImmortalShopListing& Listing)
{
	switch (Listing.ProductType)
	{
	case EImmortalShopProductType::Equipment:
		return UImmortalEquipmentLibrary::GetQualityColor(Listing.EquipmentItem.Quality);
	case EImmortalShopProductType::Material:
	{
		FImmortalMaterialDefinition Definition;
		return UImmortalMaterialLibrary::GetMaterialDefinition(Listing.ProductId, Definition)
			? Definition.DisplayColor : FLinearColor::White;
	}
	case EImmortalShopProductType::Pill:
		return UImmortalAlchemyLibrary::GetQualityColor(Listing.PillQuality);
	case EImmortalShopProductType::Artifact:
	{
		FImmortalArtifactDefinition Definition;
		return UImmortalArtifactLibrary::GetArtifactDefinition(Listing.ProductId, Definition)
			? UImmortalArtifactLibrary::GetQualityColor(Definition.Quality) : FLinearColor::White;
	}
	default:
		return FLinearColor::White;
	}
}
