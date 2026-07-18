// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalTechniqueTypes.h"
#include "../Progression/ImmortalCultivationComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalTechniqueCoreTest,
	"ImmortalPath.Techniques.DefinitionsGrowthPointsAndPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalTechniqueCoreTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Ids = UImmortalTechniqueLibrary::GetKnownTechniqueIds();
	TestEqual(TEXT("Four initial techniques"), Ids.Num(), 4);
	TSet<uint8> Effects;
	for (const FName Id : Ids)
	{
		FImmortalTechniqueDefinition DefinitionData;
		TestTrue(TEXT("Technique definition resolves"), UImmortalTechniqueLibrary::GetTechniqueDefinition(Id, DefinitionData));
		TestTrue(TEXT("Learning consumes spirit stones"), DefinitionData.LearningCost.SpiritStones > 0);
		TestFalse(TEXT("Learning consumes materials"), DefinitionData.LearningCost.Materials.IsEmpty());
		TestTrue(TEXT("Technique has an active trigger"), DefinitionData.AttacksPerTrigger >= 3);
		TestTrue(TEXT("Technique has an ultimate trigger"), DefinitionData.ActivesPerUltimate >= 2);
		Effects.Add(static_cast<uint8>(DefinitionData.ActiveEffect));
	}
	TestEqual(TEXT("Initial catalog covers four active families"), Effects.Num(), 4);

	FImmortalTechniqueProgress Sword = UImmortalTechniqueLibrary::CreateTechnique(TEXT("QingyunSwordArt"));
	TestTrue(TEXT("Created technique is valid"), Sword.IsValid());
	const float InitialActive = UImmortalTechniqueLibrary::CalculateActiveMagnitude(Sword);
	const FImmortalTechniqueBonuses InitialBonus = UImmortalTechniqueLibrary::CalculateBonuses(Sword);
	Sword.Level = 10;
	Sword.BreakthroughRank = 1;
	Sword.ActivePoints = 5;
	Sword.PassivePoints = 5;
	Sword.SpecialPoints = 3;
	UImmortalTechniqueLibrary::NormalizeProgress(Sword);
	TestEqual(TEXT("First breakthrough raises level cap"), UImmortalTechniqueLibrary::GetLevelCap(Sword), 20);
	TestTrue(TEXT("Growth raises active magnitude"), UImmortalTechniqueLibrary::CalculateActiveMagnitude(Sword) > InitialActive);
	TestTrue(TEXT("Passive points raise attack multiplier"), UImmortalTechniqueLibrary::CalculateBonuses(Sword).AttackMultiplier > InitialBonus.AttackMultiplier);
	TestTrue(TEXT("Active points shorten trigger"), UImmortalTechniqueLibrary::CalculateTriggerAttackCount(Sword) < 6);
	TestTrue(TEXT("Special points shorten ultimate cycle"), UImmortalTechniqueLibrary::CalculateUltimateActiveCount(Sword) < 3);
	TestTrue(TEXT("Level upgrade consumes cultivation"), UImmortalTechniqueLibrary::GetUpgradeCultivationCost(Sword) > 0);

	Sword.Level = 20;
	const FImmortalCraftingCost BreakthroughCost = UImmortalTechniqueLibrary::GetBreakthroughCost(Sword);
	TestTrue(TEXT("Breakthrough consumes spirit stones"), BreakthroughCost.SpiritStones > 0);
	TestTrue(TEXT("Breakthrough consumes two material types"), BreakthroughCost.Materials.Num() >= 2);

	TArray<FImmortalTechniqueProgress> Library = {Sword, Sword};
	TArray<FName> Equipped = {Sword.TechniqueId, Sword.TechniqueId, TEXT("MissingTechnique")};
	int32 InsightPoints = -20;
	UImmortalTechniqueLibrary::NormalizeLibrary(Library, Equipped, InsightPoints);
	TestEqual(TEXT("Duplicate learned technique is removed"), Library.Num(), 1);
	TestEqual(TEXT("Duplicate and unknown equipped techniques are removed"), Equipped.Num(), 1);
	TestEqual(TEXT("Negative insight points are clamped"), InsightPoints, 0);

	UImmortalCultivationComponent* Cultivation = NewObject<UImmortalCultivationComponent>();
	Cultivation->InitializeProgress(EImmortalCultivationRealm::QiRefining, 1, 80);
	TestTrue(TEXT("Current-stage cultivation can pay technique costs"), Cultivation->TrySpendCultivation(25));
	TestEqual(TEXT("Technique spending deducts current-stage cultivation"), Cultivation->GetCurrentCultivation(), 55);
	TestFalse(TEXT("Technique spending cannot overdraw or regress realm"), Cultivation->TrySpendCultivation(60));
	Cultivation->SetTechniqueRateMultiplier(1.5f);
	TestEqual(TEXT("Technique multiplier affects online and offline base rate"), Cultivation->GetCultivationPerSecondWithoutAlchemyBoost(), 3.0f);
	return true;
}

#endif
