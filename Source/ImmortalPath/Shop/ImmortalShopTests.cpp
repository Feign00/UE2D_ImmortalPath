// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalShopTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/DateTime.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalShopCoreTest,
	"ImmortalPath.Shop.GenerationRefreshPricingAndInventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalShopCoreTest::RunTest(const FString& Parameters)
{
	// The free refresh uses the shop's configured calendar day (China Standard Time by default),
	// not the machine's local time or the UTC date.
	const FDateTime BeforeCstMidnight(2026, 7, 18, 15, 59, 59);
	const FDateTime AtCstMidnight(2026, 7, 18, 16, 0, 0);
	TestEqual(TEXT("CST day before midnight"),
		UImmortalShopLibrary::GetDayKeyFromUtcTicks(BeforeCstMidnight.GetTicks(), 480), 20260718);
	TestEqual(TEXT("CST day at midnight"),
		UImmortalShopLibrary::GetDayKeyFromUtcTicks(AtCstMidnight.GetTicks(), 480), 20260719);
	TestEqual(TEXT("One second remains before CST refresh"),
		UImmortalShopLibrary::GetSecondsUntilNextRefresh(BeforeCstMidnight.GetTicks(), 480), int64(1));
	TestEqual(TEXT("A full day remains immediately after CST refresh"),
		UImmortalShopLibrary::GetSecondsUntilNextRefresh(AtCstMidnight.GetTicks(), 480), int64(86400));

	FImmortalShopState EmptyState;
	TestTrue(TEXT("First visit creates free daily stock"),
		UImmortalShopLibrary::NeedsDailyRefresh(EmptyState, 20260718));
	const FImmortalShopState DailyStock = UImmortalShopLibrary::GenerateStock(999, 9, 10, 20260718);
	TestFalse(TEXT("Same calendar day does not refresh again"),
		UImmortalShopLibrary::NeedsDailyRefresh(DailyStock, 20260718));
	TestTrue(TEXT("Next calendar day grants a free refresh"),
		UImmortalShopLibrary::NeedsDailyRefresh(DailyStock, 20260719));
	TestFalse(TEXT("Clock rollback does not grant a free refresh"),
		UImmortalShopLibrary::NeedsDailyRefresh(DailyStock, 20260717));

	TestEqual(TEXT("Full unlocked shop contains ten listings"), DailyStock.Listings.Num(), 10);
	int32 EquipmentCount = 0;
	int32 MaterialCount = 0;
	int32 PillCount = 0;
	int32 ArtifactCount = 0;
	TSet<FGuid> ListingIds;
	for (const FImmortalShopListing& Listing : DailyStock.Listings)
	{
		TestTrue(TEXT("Generated listing is valid"), Listing.IsValid());
		TestTrue(TEXT("Generated listing has a positive price"), Listing.BundlePrice > 0);
		TestFalse(TEXT("Generated listing ID is unique"), ListingIds.Contains(Listing.ListingId));
		ListingIds.Add(Listing.ListingId);
		switch (Listing.ProductType)
		{
		case EImmortalShopProductType::Equipment: ++EquipmentCount; break;
		case EImmortalShopProductType::Material: ++MaterialCount; break;
		case EImmortalShopProductType::Pill: ++PillCount; break;
		case EImmortalShopProductType::Artifact: ++ArtifactCount; break;
		default: AddError(TEXT("Generated listing has an unknown product type")); break;
		}
	}
	TestEqual(TEXT("Daily shop has three equipment offers"), EquipmentCount, 3);
	TestEqual(TEXT("Daily shop has four material offers"), MaterialCount, 4);
	TestEqual(TEXT("Daily shop has two pill offers"), PillCount, 2);
	TestEqual(TEXT("Daily shop has one artifact offer"), ArtifactCount, 1);
	TestEqual(TEXT("All listing IDs are unique"), ListingIds.Num(), DailyStock.Listings.Num());

	const FImmortalShopState PaidRefresh = UImmortalShopLibrary::GenerateStock(999, 9, 10, 20260718, 1);
	TestEqual(TEXT("Paid refresh preserves the calendar day"), PaidRefresh.RefreshDayKey, 20260718);
	TestEqual(TEXT("Paid refresh records its serial"), PaidRefresh.RefreshSerial, 1);
	TestEqual(TEXT("Paid refresh records its daily count"), PaidRefresh.ManualRefreshCount, 1);
	TestEqual(TEXT("First manual refresh costs 100 spirit stones"),
		UImmortalShopLibrary::GetManualRefreshCost(DailyStock), 100);
	TestEqual(TEXT("Second manual refresh costs 200 spirit stones"),
		UImmortalShopLibrary::GetManualRefreshCost(PaidRefresh), 200);
	TestNotEqual(TEXT("Paid refresh produces new listing IDs"),
		PaidRefresh.Listings[0].ListingId, DailyStock.Listings[0].ListingId);

	FImmortalEquipmentItem CommonEquipment;
	CommonEquipment.ItemId = FGuid::NewGuid();
	CommonEquipment.DisplayName = TEXT("PricingTestCommon");
	CommonEquipment.ItemLevel = 10;
	CommonEquipment.Quality = EImmortalEquipmentQuality::Common;
	CommonEquipment.EnhancementLevel = 0;
	TestEqual(TEXT("Level-ten common equipment exact buy price"),
		UImmortalShopLibrary::GetEquipmentBuyPrice(CommonEquipment), 300);
	TestEqual(TEXT("Level-ten common equipment exact sell price"),
		UImmortalShopLibrary::GetEquipmentSellPrice(CommonEquipment), 75);

	FImmortalEquipmentItem RareEquipment = CommonEquipment;
	RareEquipment.ItemId = FGuid::NewGuid();
	RareEquipment.DisplayName = TEXT("PricingTestRare");
	RareEquipment.Quality = EImmortalEquipmentQuality::Rare;
	TestEqual(TEXT("Level-ten rare equipment exact buy price"),
		UImmortalShopLibrary::GetEquipmentBuyPrice(RareEquipment), 720);
	TestEqual(TEXT("Level-ten rare equipment exact sell price"),
		UImmortalShopLibrary::GetEquipmentSellPrice(RareEquipment), 180);
	TestEqual(TEXT("Invalid equipment has no buy value"),
		UImmortalShopLibrary::GetEquipmentBuyPrice(FImmortalEquipmentItem()), 0);

	const TArray<FName> MaterialIds = UImmortalMaterialLibrary::GetKnownMaterialIds();
	TestFalse(TEXT("Material catalog is available to the shop"), MaterialIds.IsEmpty());
	for (const FName MaterialId : MaterialIds)
	{
		const int32 BuyPrice = UImmortalShopLibrary::GetMaterialUnitBuyPrice(MaterialId);
		const int32 SellPrice = UImmortalShopLibrary::GetMaterialUnitSellPrice(MaterialId);
		TestTrue(*FString::Printf(TEXT("Material buy price is positive: %s"), *MaterialId.ToString()), BuyPrice > 0);
		TestTrue(*FString::Printf(TEXT("Material sell price is positive: %s"), *MaterialId.ToString()), SellPrice > 0);
		TestTrue(*FString::Printf(TEXT("Material cannot be bought and resold for profit: %s"), *MaterialId.ToString()),
			SellPrice < BuyPrice);
	}
	TestEqual(TEXT("Unknown material has no buy value"),
		UImmortalShopLibrary::GetMaterialUnitBuyPrice(TEXT("MissingMaterial")), 0);
	TestEqual(TEXT("Unknown material has no sell value"),
		UImmortalShopLibrary::GetMaterialUnitSellPrice(TEXT("MissingMaterial")), 0);

	FImmortalShopState DirtyState = DailyStock;
	const int32 CleanListingCount = DirtyState.Listings.Num();
	FImmortalShopListing Duplicate = DirtyState.Listings[0];
	Duplicate.bSoldOut = true;
	DirtyState.Listings.Add(Duplicate);
	FImmortalShopListing Invalid;
	Invalid.ListingId = FGuid::NewGuid();
	Invalid.ProductType = EImmortalShopProductType::Material;
	Invalid.ProductId = TEXT("MissingMaterial");
	Invalid.BundleQuantity = 5;
	Invalid.BundlePrice = 50;
	DirtyState.Listings.Add(Invalid);
	DirtyState.RefreshDayKey = -1;
	DirtyState.RefreshSerial = -2;
	DirtyState.ManualRefreshCount = -3;
	UImmortalShopLibrary::NormalizeState(DirtyState);
	TestEqual(TEXT("Normalize removes invalid and duplicate listings"), DirtyState.Listings.Num(), CleanListingCount);
	TestEqual(TEXT("Normalize clamps invalid day key"), DirtyState.RefreshDayKey, 0);
	TestEqual(TEXT("Normalize clamps invalid refresh serial"), DirtyState.RefreshSerial, 0);
	TestEqual(TEXT("Normalize clamps invalid manual refresh count"), DirtyState.ManualRefreshCount, 0);

	TArray<FImmortalMaterialStack> Materials;
	Materials.Add({TEXT("SpiritGrass"), 2});
	Materials.Add({TEXT("SpiritGrass"), 3});
	Materials.Add({TEXT("Ore"), 4});
	const TArray<FImmortalMaterialStack> BeforeRejectedRemoval = Materials;
	TestFalse(TEXT("Insufficient material removal is rejected transactionally"),
		UImmortalMaterialLibrary::RemoveMaterialStack(Materials, TEXT("SpiritGrass"), 6));
	TestEqual(TEXT("Rejected removal preserves stack count"), Materials.Num(), BeforeRejectedRemoval.Num());
	for (int32 Index = 0; Index < BeforeRejectedRemoval.Num(); ++Index)
	{
		TestEqual(TEXT("Rejected removal preserves material ID"), Materials[Index].MaterialId, BeforeRejectedRemoval[Index].MaterialId);
		TestEqual(TEXT("Rejected removal preserves quantity"), Materials[Index].Quantity, BeforeRejectedRemoval[Index].Quantity);
	}
	TestTrue(TEXT("Exact available material quantity can be removed"),
		UImmortalMaterialLibrary::RemoveMaterialStack(Materials, TEXT("SpiritGrass"), 5));
	TestEqual(TEXT("Exact removal consumes every matching stack"),
		UImmortalMaterialLibrary::GetMaterialQuantity(Materials, TEXT("SpiritGrass")), 0);
	TestEqual(TEXT("Exact removal preserves unrelated material"),
		UImmortalMaterialLibrary::GetMaterialQuantity(Materials, TEXT("Ore")), 4);
	TestEqual(TEXT("Removal normalization leaves one unrelated stack"), Materials.Num(), 1);
	return true;
}

#endif
