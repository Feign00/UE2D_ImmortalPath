// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalArtifactTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalArtifactCoreTest,
	"ImmortalPath.Artifacts.DefinitionsGrowthAndPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalArtifactCoreTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Ids = UImmortalArtifactLibrary::GetKnownArtifactIds();
	TestEqual(TEXT("Four initial artifacts"), Ids.Num(), 4);
	TSet<uint8> ActiveEffects;
	for (const FName Id : Ids)
	{
		FImmortalArtifactDefinition Definition;
		TestTrue(TEXT("Artifact definition resolves"), UImmortalArtifactLibrary::GetArtifactDefinition(Id, Definition));
		TestFalse(TEXT("Artifact crafting consumes materials"), Definition.CraftingCost.Materials.IsEmpty());
		TestTrue(TEXT("Artifact crafting consumes spirit stones"), Definition.CraftingCost.SpiritStones > 0);
		TestTrue(TEXT("Artifact has active trigger"), Definition.AttacksPerTrigger >= 2);
		TestTrue(TEXT("Artifact unlock stage is valid"), Definition.MinimumQingyunStage >= 1 && Definition.MinimumQingyunStage <= 999);
		ActiveEffects.Add(static_cast<uint8>(Definition.ActiveEffect));
	}
	TestEqual(TEXT("Initial catalog covers four distinct active effects"), ActiveEffects.Num(), 4);

	FImmortalArtifactItem Sword = UImmortalArtifactLibrary::CreateArtifact(TEXT("XuanGuangSword"));
	TestTrue(TEXT("Created artifact is valid"), Sword.IsValid());
	const float InitialMagnitude = UImmortalArtifactLibrary::CalculateActiveMagnitude(Sword);
	const FImmortalArtifactBonuses InitialBonus = UImmortalArtifactLibrary::CalculateBonuses(Sword);
	Sword.Level = 20;
	Sword.Stars = 3;
	UImmortalArtifactLibrary::NormalizeArtifact(Sword);
	TestTrue(TEXT("Growth raises active effect"), UImmortalArtifactLibrary::CalculateActiveMagnitude(Sword) > InitialMagnitude);
	TestTrue(TEXT("Growth raises passive attack"), UImmortalArtifactLibrary::CalculateBonuses(Sword).AttackMultiplier > InitialBonus.AttackMultiplier);
	TestTrue(TEXT("Stars reduce trigger interval"), UImmortalArtifactLibrary::CalculateTriggerAttackCount(Sword) < 5);
	const FImmortalCraftingCost UpgradeCost = UImmortalArtifactLibrary::GetUpgradeCost(Sword);
	const FImmortalCraftingCost StarCost = UImmortalArtifactLibrary::GetStarUpCost(Sword);
	TestTrue(TEXT("Level growth has a spirit-stone cost"), UpgradeCost.SpiritStones > 0);
	TestFalse(TEXT("Level growth consumes artifact materials"), UpgradeCost.Materials.IsEmpty());
	TestTrue(TEXT("Star growth has a spirit-stone cost"), StarCost.SpiritStones > 0);
	TestTrue(TEXT("Star growth consumes multiple material types"), StarCost.Materials.Num() >= 2);

	TArray<FImmortalArtifactItem> Inventory = {Sword, Sword};
	FGuid Equipped = Sword.InstanceId;
	UImmortalArtifactLibrary::NormalizeInventory(Inventory, Equipped);
	TestEqual(TEXT("Duplicate instance is removed"), Inventory.Num(), 1);
	TestTrue(TEXT("Valid equipped ID survives normalization"), Equipped.IsValid());
	Inventory.Reset();
	UImmortalArtifactLibrary::NormalizeInventory(Inventory, Equipped);
	TestFalse(TEXT("Missing equipped ID is cleared"), Equipped.IsValid());
	return true;
}

#endif
