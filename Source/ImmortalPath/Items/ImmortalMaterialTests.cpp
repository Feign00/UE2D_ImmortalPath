// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalMaterialTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalMaterialInventoryTest,
	"ImmortalPath.Items.Materials.CatalogAndStacking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalMaterialInventoryTest::RunTest(const FString& Parameters)
{
	const TArray<FName> MaterialIds = UImmortalMaterialLibrary::GetKnownMaterialIds();
	TestTrue(TEXT("Catalog contains alchemy and crafting foundations"), MaterialIds.Num() >= 7);
	for (const FName MaterialId : MaterialIds)
	{
		FImmortalMaterialDefinition Definition;
		TestTrue(*FString::Printf(TEXT("Definition exists: %s"), *MaterialId.ToString()),
			UImmortalMaterialLibrary::GetMaterialDefinition(MaterialId, Definition));
		TestFalse(TEXT("Display name is not empty"), Definition.DisplayName.IsEmpty());
		TestTrue(TEXT("Maximum stack is positive"), Definition.MaximumStack > 0);
	}

	TArray<FImmortalMaterialStack> Inventory;
	const FName FirstId = MaterialIds[0];
	TestEqual(TEXT("First addition"), UImmortalMaterialLibrary::AddMaterialStack(Inventory, FirstId, 3), 3);
	TestEqual(TEXT("Duplicate addition merges"), UImmortalMaterialLibrary::AddMaterialStack(Inventory, FirstId, 4), 4);
	TestEqual(TEXT("One stack after merge"), Inventory.Num(), 1);
	TestEqual(TEXT("Merged quantity"), UImmortalMaterialLibrary::GetMaterialQuantity(Inventory, FirstId), 7);
	TestEqual(TEXT("Unknown IDs are rejected"), UImmortalMaterialLibrary::AddMaterialStack(Inventory, TEXT("MissingMaterial"), 5), 0);

	Inventory.Add({FirstId, 2});
	Inventory.Add({NAME_None, 99});
	UImmortalMaterialLibrary::NormalizeInventory(Inventory);
	TestEqual(TEXT("Normalization merges duplicates and removes invalid entries"), Inventory.Num(), 1);
	TestEqual(TEXT("Normalized quantity"), Inventory[0].Quantity, 9);

	const FImmortalMaterialStack NormalDrop = UImmortalMaterialLibrary::GenerateStageDrop(1, false, 0);
	TestTrue(TEXT("Stage-one drop is valid"), NormalDrop.IsValid());
	const FImmortalMaterialStack BossCore = UImmortalMaterialLibrary::GenerateStageDrop(10, true, 0);
	TestEqual(TEXT("Boss first material is a demon core"), BossCore.MaterialId, FName(TEXT("DemonCore")));
	const FImmortalMaterialStack BossFragment = UImmortalMaterialLibrary::GenerateStageDrop(10, true, 2);
	TestEqual(TEXT("Boss third material is an artifact fragment"), BossFragment.MaterialId, FName(TEXT("ArtifactFragment")));
	return true;
}

#endif

