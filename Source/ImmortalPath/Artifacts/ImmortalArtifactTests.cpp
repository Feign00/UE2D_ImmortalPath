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
	for (const FName Id : Ids)
	{
		FImmortalArtifactDefinition Definition;
		TestTrue(TEXT("Artifact definition resolves"), UImmortalArtifactLibrary::GetArtifactDefinition(Id, Definition));
		TestFalse(TEXT("Artifact crafting consumes materials"), Definition.CraftingCost.Materials.IsEmpty());
		TestTrue(TEXT("Artifact has active trigger"), Definition.AttacksPerTrigger >= 2);
	}

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

