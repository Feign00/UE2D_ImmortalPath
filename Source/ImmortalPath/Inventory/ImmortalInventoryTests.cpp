// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalInventoryTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../Save/ImmortalPathSaveGame.h"
#include "../Shop/ImmortalShopTypes.h"

namespace
{
	FImmortalEquipmentItem MakeInventoryTestItem(
		const TCHAR* StableGuid,
		const EImmortalEquipmentSlot Slot,
		const EImmortalEquipmentQuality Quality,
		const int32 Level,
		const bool bLocked)
	{
		FImmortalEquipmentItem Item = UImmortalEquipmentLibrary::GenerateCraftedEquipment(Level, Slot, Quality);
		FGuid::Parse(StableGuid, Item.ItemId);
		Item.bLocked = bLocked;
		return Item;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalInventoryOrganizationTest,
	"ImmortalPath.Inventory.OrganizationLockingAndQuestItems",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalInventoryOrganizationTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Inventory exposes five stable categories"),
		static_cast<int32>(EImmortalInventoryCategory::QuestItem) + 1, 5);
	TestEqual(TEXT("SaveGame version is v17 for equipment expansion"),
		UImmortalPathSaveGame::CurrentSaveVersion, 17);
	const TArray<FName> KnownQuestItems = UImmortalInventoryLibrary::GetKnownQuestItemIds();
	TestTrue(TEXT("Catalog contains every built-in task item type"), KnownQuestItems.Num() >= 3);
	TestTrue(TEXT("Catalog contains Qingyun trial slip"), KnownQuestItems.Contains(TEXT("QingyunTrialJadeSlip")));
	TestTrue(TEXT("Catalog contains ancient map fragment"), KnownQuestItems.Contains(TEXT("AncientMapFragment")));
	TestTrue(TEXT("Catalog contains demon king seal"), KnownQuestItems.Contains(TEXT("DemonKingSeal")));
	for (const FName QuestItemId : KnownQuestItems)
	{
		FImmortalQuestItemDefinition Definition;
		TestTrue(*FString::Printf(TEXT("Task item definition exists: %s"), *QuestItemId.ToString()),
			UImmortalInventoryLibrary::GetQuestItemDefinition(QuestItemId, Definition));
		TestFalse(TEXT("Task item display name is not empty"), Definition.DisplayName.IsEmpty());
		TestTrue(TEXT("Task item stack limit is positive"), Definition.MaximumStack > 0);
	}

	TArray<FImmortalQuestItemStack> QuestItems;
	TestEqual(TEXT("Known task item can be added"),
		UImmortalInventoryLibrary::AddQuestItemStack(QuestItems, TEXT("DemonKingSeal"), 2), 2);
	TestEqual(TEXT("Duplicate task item merges"),
		UImmortalInventoryLibrary::AddQuestItemStack(QuestItems, TEXT("DemonKingSeal"), 3), 3);
	TestEqual(TEXT("Merged task item quantity"),
		UImmortalInventoryLibrary::GetQuestItemQuantity(QuestItems, TEXT("DemonKingSeal")), 5);
	TestEqual(TEXT("Unknown task item is rejected"),
		UImmortalInventoryLibrary::AddQuestItemStack(QuestItems, TEXT("MissingQuestItem"), 1), 0);
	QuestItems.Add({TEXT("DemonKingSeal"), 4});
	QuestItems.Add({NAME_None, 100});
	UImmortalInventoryLibrary::NormalizeQuestItemInventory(QuestItems);
	TestEqual(TEXT("Task normalization merges duplicates and removes invalid entries"), QuestItems.Num(), 1);
	TestEqual(TEXT("Normalized task item quantity"), QuestItems[0].Quantity, 9);
	TestFalse(TEXT("Task-item removal is all-or-nothing when quantity is insufficient"),
		UImmortalInventoryLibrary::RemoveQuestItemStack(QuestItems, TEXT("DemonKingSeal"), 10));
	TestEqual(TEXT("Rejected removal leaves quantity unchanged"), QuestItems[0].Quantity, 9);
	TestTrue(TEXT("Task-item removal succeeds with sufficient quantity"),
		UImmortalInventoryLibrary::RemoveQuestItemStack(QuestItems, TEXT("DemonKingSeal"), 4));
	TestEqual(TEXT("Task-item removal persists the exact remainder"), QuestItems[0].Quantity, 5);
	FImmortalQuestItemDefinition SealDefinition;
	UImmortalInventoryLibrary::GetQuestItemDefinition(TEXT("DemonKingSeal"), SealDefinition);
	TArray<FImmortalQuestItemStack> FullQuestStack;
	TestEqual(TEXT("Task-item additions clamp at the configured stack maximum"),
		UImmortalInventoryLibrary::AddQuestItemStack(FullQuestStack, TEXT("DemonKingSeal"), MAX_int32),
		SealDefinition.MaximumStack);
	TestEqual(TEXT("A full task-item stack rejects further additions"),
		UImmortalInventoryLibrary::AddQuestItemStack(FullQuestStack, TEXT("DemonKingSeal"), 1), 0);
	TArray<FImmortalQuestItemStack> SortedQuestItems =
	{
		{TEXT("QingyunTrialJadeSlip"), 1}, {TEXT("DemonKingSeal"), 1}, {TEXT("AncientMapFragment"), 1}
	};
	UImmortalInventoryLibrary::NormalizeQuestItemInventory(SortedQuestItems);
	for (int32 Index = 1; Index < SortedQuestItems.Num(); ++Index)
	{
		FImmortalQuestItemDefinition PreviousDefinition;
		FImmortalQuestItemDefinition CurrentDefinition;
		UImmortalInventoryLibrary::GetQuestItemDefinition(SortedQuestItems[Index - 1].QuestItemId, PreviousDefinition);
		UImmortalInventoryLibrary::GetQuestItemDefinition(SortedQuestItems[Index].QuestItemId, CurrentDefinition);
		TestTrue(TEXT("Task items sort deterministically by display name"),
			PreviousDefinition.DisplayName.ToString() <= CurrentDefinition.DisplayName.ToString());
	}

	TArray<FImmortalEquipmentItem> Equipment =
	{
		MakeInventoryTestItem(TEXT("00000000000000000000000000000003"), EImmortalEquipmentSlot::Chest, EImmortalEquipmentQuality::Rare, 20, false),
		MakeInventoryTestItem(TEXT("00000000000000000000000000000002"), EImmortalEquipmentSlot::Weapon, EImmortalEquipmentQuality::Common, 5, false),
		MakeInventoryTestItem(TEXT("00000000000000000000000000000001"), EImmortalEquipmentSlot::Boots, EImmortalEquipmentQuality::Common, 1, true)
	};
	TestTrue(TEXT("Unsorted equipment changes during organization"),
		UImmortalInventoryLibrary::SortEquipmentInventory(Equipment));
	TestTrue(TEXT("Locked equipment sorts first"), Equipment[0].bLocked);
	TestEqual(TEXT("Remaining equipment sorts by slot before quality"), Equipment[1].Slot, EImmortalEquipmentSlot::Weapon);
	TestFalse(TEXT("Already organized equipment does not report a mutation"),
		UImmortalInventoryLibrary::SortEquipmentInventory(Equipment));
	TestEqual(TEXT("Only one unlocked common item is bulk eligible"),
		UImmortalInventoryLibrary::GetBulkEligibleEquipmentCount(Equipment, EImmortalEquipmentQuality::Common), 1);
	TestEqual(TEXT("Common threshold has the exact eligible sale value"),
		UImmortalInventoryLibrary::GetBulkEquipmentSellValue(Equipment, EImmortalEquipmentQuality::Common),
		UImmortalShopLibrary::GetEquipmentSellPrice(Equipment[1]));
	TestEqual(TEXT("Rare threshold includes both unlocked common and rare items"),
		UImmortalInventoryLibrary::GetBulkEligibleEquipmentCount(Equipment, EImmortalEquipmentQuality::Rare), 2);

	FImmortalArtifactItem EquippedArtifact = UImmortalArtifactLibrary::CreateArtifact(TEXT("XuanGuangSword"));
	FImmortalArtifactItem LockedArtifact = UImmortalArtifactLibrary::CreateArtifact(TEXT("ChaosPearl"));
	FImmortalArtifactItem OtherArtifact = UImmortalArtifactLibrary::CreateArtifact(TEXT("SevenStarBanner"));
	FGuid::Parse(TEXT("30000000000000000000000000000001"), EquippedArtifact.InstanceId);
	FGuid::Parse(TEXT("30000000000000000000000000000002"), LockedArtifact.InstanceId);
	FGuid::Parse(TEXT("30000000000000000000000000000003"), OtherArtifact.InstanceId);
	LockedArtifact.bLocked = true;
	LockedArtifact.Level = 50;
	LockedArtifact.Stars = 5;
	OtherArtifact.Level = 30;
	OtherArtifact.Stars = 3;
	TArray<FImmortalArtifactItem> Artifacts = {OtherArtifact, LockedArtifact, EquippedArtifact};
	TestTrue(TEXT("Unsorted artifacts change during organization"),
		UImmortalInventoryLibrary::SortArtifactInventory(Artifacts, EquippedArtifact.InstanceId));
	TestEqual(TEXT("Equipped artifact always sorts first"), Artifacts[0].InstanceId, EquippedArtifact.InstanceId);
	TestEqual(TEXT("Locked artifact sorts ahead of remaining unequipped artifacts"), Artifacts[1].InstanceId, LockedArtifact.InstanceId);
	TestFalse(TEXT("Already organized artifacts do not report a mutation"),
		UImmortalInventoryLibrary::SortArtifactInventory(Artifacts, EquippedArtifact.InstanceId));

	FImmortalEquipmentItem EquippedWeapon = MakeInventoryTestItem(
		TEXT("20000000000000000000000000000001"), EImmortalEquipmentSlot::Weapon,
		EImmortalEquipmentQuality::Rare, 10, false);
	FImmortalEquipmentItem DuplicateSlot = MakeInventoryTestItem(
		TEXT("20000000000000000000000000000002"), EImmortalEquipmentSlot::Weapon,
		EImmortalEquipmentQuality::Common, 2, false);
	FImmortalEquipmentItem DuplicateEquippedId = EquippedWeapon;
	DuplicateEquippedId.bLocked = true;
	FImmortalEquipmentItem DuplicateInventory = MakeInventoryTestItem(
		TEXT("20000000000000000000000000000003"), EImmortalEquipmentSlot::Head,
		EImmortalEquipmentQuality::Common, 3, false);
	FImmortalEquipmentItem LockedDuplicateInventory = DuplicateInventory;
	LockedDuplicateInventory.bLocked = true;
	TArray<FImmortalEquipmentItem> DirtyEquipped = {EquippedWeapon, DuplicateSlot};
	TArray<FImmortalEquipmentItem> DirtyInventory =
		{DuplicateEquippedId, DuplicateInventory, LockedDuplicateInventory, FImmortalEquipmentItem()};
	TestTrue(TEXT("Equipment collection normalization reports repairs"),
		UImmortalInventoryLibrary::NormalizeEquipmentCollections(DirtyInventory, DirtyEquipped));
	TestEqual(TEXT("Duplicate equipped slot is moved out without losing its distinct item"), DirtyEquipped.Num(), 1);
	TestEqual(TEXT("Invalid and duplicate IDs are removed while distinct items survive"), DirtyInventory.Num(), 2);
	TestTrue(TEXT("Lock protection is merged into the retained equipped instance"), DirtyEquipped[0].bLocked);
	const FImmortalEquipmentItem* RetainedDuplicate = DirtyInventory.FindByPredicate([&DuplicateInventory](const FImmortalEquipmentItem& Item)
	{
		return Item.ItemId == DuplicateInventory.ItemId;
	});
	TestTrue(TEXT("Lock protection is merged into the retained backpack instance"),
		RetainedDuplicate && RetainedDuplicate->bLocked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalInventoryDismantleTest,
	"ImmortalPath.Inventory.DismantleYieldAndBulkProtection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalInventoryDismantleTest::RunTest(const FString& Parameters)
{
	FImmortalEquipmentItem Common = MakeInventoryTestItem(
		TEXT("10000000000000000000000000000001"), EImmortalEquipmentSlot::Weapon,
		EImmortalEquipmentQuality::Common, 1, false);
	FImmortalEquipmentItem Epic = MakeInventoryTestItem(
		TEXT("10000000000000000000000000000002"), EImmortalEquipmentSlot::Chest,
		EImmortalEquipmentQuality::Epic, 21, false);
	Epic.EnhancementLevel = 6;
	UImmortalEquipmentLibrary::RebuildEquipmentStats(Epic);
	FImmortalEquipmentItem LockedLegendary = MakeInventoryTestItem(
		TEXT("10000000000000000000000000000003"), EImmortalEquipmentSlot::Accessory,
		EImmortalEquipmentQuality::Legendary, 50, true);

	const TArray<FImmortalMaterialStack> CommonYield = UImmortalInventoryLibrary::GetEquipmentDismantleYield(Common);
	TestEqual(TEXT("Common level-one equipment yields one ore"),
		UImmortalMaterialLibrary::GetMaterialQuantity(CommonYield, TEXT("Ore")), 1);
	TestEqual(TEXT("Common equipment yields no spirit iron"),
		UImmortalMaterialLibrary::GetMaterialQuantity(CommonYield, TEXT("SpiritIron")), 0);
	const TArray<FImmortalMaterialStack> EpicYield = UImmortalInventoryLibrary::GetEquipmentDismantleYield(Epic);
	TestEqual(TEXT("Epic level-21 +6 equipment exact ore yield"),
		UImmortalMaterialLibrary::GetMaterialQuantity(EpicYield, TEXT("Ore")), 5);
	TestEqual(TEXT("Epic level-21 +6 equipment exact spirit-iron yield"),
		UImmortalMaterialLibrary::GetMaterialQuantity(EpicYield, TEXT("SpiritIron")), 4);
	TestEqual(TEXT("Epic equipment yields one artifact fragment"),
		UImmortalMaterialLibrary::GetMaterialQuantity(EpicYield, TEXT("ArtifactFragment")), 1);

	const TArray<FImmortalEquipmentItem> Inventory = {Common, Epic, LockedLegendary};
	const TArray<FImmortalMaterialStack> BulkYield = UImmortalInventoryLibrary::GetBulkEquipmentDismantleYield(
		Inventory, EImmortalEquipmentQuality::Legendary);
	TestEqual(TEXT("Bulk yield includes common and epic ore but excludes locked legendary"),
		UImmortalMaterialLibrary::GetMaterialQuantity(BulkYield, TEXT("Ore")), 6);
	TestEqual(TEXT("Bulk yield excludes locked legendary spirit iron"),
		UImmortalMaterialLibrary::GetMaterialQuantity(BulkYield, TEXT("SpiritIron")), 4);
	TestFalse(TEXT("Locked item is never bulk eligible"),
		UImmortalInventoryLibrary::IsEligibleForBulkAction(LockedLegendary, EImmortalEquipmentQuality::Legendary));

	FImmortalMaterialDefinition SpiritIronDefinition;
	TestTrue(TEXT("Spirit-iron definition exists for capacity checks"),
		UImmortalMaterialLibrary::GetMaterialDefinition(TEXT("SpiritIron"), SpiritIronDefinition));
	TArray<FImmortalMaterialStack> CapacityInventory =
		{{TEXT("SpiritIron"), SpiritIronDefinition.MaximumStack - 1}, {TEXT("Ore"), 5}};
	TestTrue(TEXT("A reward that exactly fills a material stack succeeds"),
		UImmortalInventoryLibrary::TryAddMaterialRewards(
			CapacityInventory, {{TEXT("SpiritIron"), 1}}));
	TestEqual(TEXT("Exact-capacity reward reaches the stack maximum"),
		UImmortalMaterialLibrary::GetMaterialQuantity(CapacityInventory, TEXT("SpiritIron")),
		SpiritIronDefinition.MaximumStack);
	const int32 OreBeforeRejectedReward = UImmortalMaterialLibrary::GetMaterialQuantity(CapacityInventory, TEXT("Ore"));
	TestFalse(TEXT("A multi-material reward is rejected when any one stack is full"),
		UImmortalInventoryLibrary::TryAddMaterialRewards(
			CapacityInventory, {{TEXT("Ore"), 2}, {TEXT("SpiritIron"), 1}}));
	TestEqual(TEXT("Rejected multi-material reward does not partially add earlier stacks"),
		UImmortalMaterialLibrary::GetMaterialQuantity(CapacityInventory, TEXT("Ore")), OreBeforeRejectedReward);
	TestTrue(TEXT("Wallet can receive an amount that exactly reaches MAX_int32"),
		UImmortalInventoryLibrary::CanReceiveSpiritStones(MAX_int32 - 10, 10));
	TestFalse(TEXT("Wallet rejects a sale that would overflow by one"),
		UImmortalInventoryLibrary::CanReceiveSpiritStones(MAX_int32 - 10, 11));
	TestFalse(TEXT("Wallet rejects zero-value mutations"),
		UImmortalInventoryLibrary::CanReceiveSpiritStones(100, 0));
	return true;
}

#endif
