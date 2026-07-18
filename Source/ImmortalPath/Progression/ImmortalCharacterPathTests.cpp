// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalCharacterPathTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ImmortalCultivationComponent.h"
#include "../Techniques/ImmortalTechniqueTypes.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalCharacterPathTest,
	"ImmortalPath.Builds.SpiritRootsPathsElementsAndEquipment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalCharacterPathTest::RunTest(const FString& Parameters)
{
	const TArray<EImmortalSpiritRoot> Roots = UImmortalCharacterPathLibrary::GetKnownSpiritRoots();
	TestEqual(TEXT("All nine spirit roots exist"), Roots.Num(), 9);
	TSet<EImmortalElementType> RootElements;
	float WeightSum = 0.0f;
	for (const EImmortalSpiritRoot Root : Roots)
	{
		FImmortalSpiritRootDefinition Definition;
		TestTrue(TEXT("Every spirit root has a definition"),
			UImmortalCharacterPathLibrary::GetSpiritRootDefinition(Root, Definition));
		TestFalse(TEXT("Spirit-root name is present"), Definition.DisplayName.IsEmpty());
		TestTrue(TEXT("Spirit-root roll weight is positive"), Definition.RollWeight > 0.0f);
		TestTrue(TEXT("Spirit-root cultivation bonus is consumed"), Definition.CultivationRateBonus > 0.0f);
		TestTrue(TEXT("Spirit-root damage bonus is consumed"), Definition.MatchingElementDamageBonus > 0.0f);
		TestTrue(TEXT("Spirit-root pill bonus is consumed"), Definition.PillEffectBonus > 0.0f);
		RootElements.Add(Definition.Element);
		WeightSum += Definition.RollWeight;
	}
	TestEqual(TEXT("Eight elements plus chaos are represented"), RootElements.Num(), 9);
	TestTrue(TEXT("Root weight table has a stable positive total"), WeightSum > 0.0f);

	const FImmortalSpiritRootState FirstRoll = UImmortalCharacterPathLibrary::GenerateSpiritRootFromRoll(0.0f, 0.0f);
	TestEqual(TEXT("Lowest deterministic roll is metal"), FirstRoll.Root, EImmortalSpiritRoot::Metal);
	TestTrue(TEXT("Ordinary root purity starts at sixty percent"), FMath::IsNearlyEqual(FirstRoll.Purity, 0.60f));
	const FImmortalSpiritRootState LastRoll = UImmortalCharacterPathLibrary::GenerateSpiritRootFromRoll(0.999999f, 0.0f);
	TestEqual(TEXT("Highest deterministic roll is mutated"), LastRoll.Root, EImmortalSpiritRoot::Mutated);
	TestTrue(TEXT("Mutated purity starts at ninety percent"), FMath::IsNearlyEqual(LastRoll.Purity, 0.90f));

	const FImmortalSpiritRootState FireRoot = UImmortalCharacterPathLibrary::GenerateSpiritRootFromRoll(0.45f, 1.0f);
	TestEqual(TEXT("Deterministic mid roll resolves to fire"), FireRoot.Root, EImmortalSpiritRoot::Fire);
	TestTrue(TEXT("Fire root amplifies fire"),
		UImmortalCharacterPathLibrary::CalculateElementDamageMultiplier(FireRoot, EImmortalElementType::Fire) > 1.0f);
	TestTrue(TEXT("Fire root does not amplify metal"), FMath::IsNearlyEqual(
		UImmortalCharacterPathLibrary::CalculateElementDamageMultiplier(FireRoot, EImmortalElementType::Metal), 1.0f));
	TestTrue(TEXT("Root increases cultivation"),
		UImmortalCharacterPathLibrary::CalculateCultivationRateMultiplier(FireRoot) > 1.0f);
	const float PillMultiplier = UImmortalCharacterPathLibrary::CalculatePillEffectMultiplier(FireRoot);
	TestTrue(TEXT("Root increases pill effects"), PillMultiplier > 1.0f);
	TestTrue(TEXT("Direct pill magnitude scales completely"), FMath::IsNearlyEqual(
		UImmortalCharacterPathLibrary::CalculateEffectivePillMagnitude(FireRoot, 0.5f, false), 0.5f * PillMultiplier));
	TestTrue(TEXT("Rate pill scales only the bonus above one"), FMath::IsNearlyEqual(
		UImmortalCharacterPathLibrary::CalculateEffectivePillMagnitude(FireRoot, 1.5f, true), 1.0f + 0.5f * PillMultiplier));
	TestTrue(TEXT("Mutated root amplifies every element"),
		UImmortalCharacterPathLibrary::CalculateElementDamageMultiplier(LastRoll, EImmortalElementType::Ice) > 1.0f);

	FImmortalSpiritRootState InvalidRoot;
	InvalidRoot.Root = EImmortalSpiritRoot::MAX;
	InvalidRoot.Purity = 99.0f;
	UImmortalCharacterPathLibrary::NormalizeSpiritRoot(InvalidRoot);
	TestFalse(TEXT("Invalid saved root normalizes to unawakened"), InvalidRoot.IsAwakened());

	const TArray<EImmortalCultivationPath> Paths = UImmortalCharacterPathLibrary::GetKnownCultivationPaths();
	TestEqual(TEXT("All five cultivation paths exist"), Paths.Num(), 5);
	TSet<EImmortalPathSkillEffect> SkillEffects;
	TSet<EImmortalEquipmentDiscipline> Disciplines;
	for (const EImmortalCultivationPath Path : Paths)
	{
		FImmortalCultivationPathDefinition Definition;
		TestTrue(TEXT("Every cultivation path has a definition"),
			UImmortalCharacterPathLibrary::GetCultivationPathDefinition(Path, Definition));
		TestFalse(TEXT("Path skill is named"), Definition.SkillName.IsEmpty());
		TestTrue(TEXT("Path skill has a real trigger"), Definition.AttacksPerSkill >= 3);
		TestTrue(TEXT("Path skill has nonzero magnitude"), Definition.SkillMagnitude > 0.0f);
		SkillEffects.Add(Definition.SkillEffect);
		Disciplines.Add(Definition.EquipmentDiscipline);
		FImmortalCultivationPathState State;
		State.Path = Path;
		const FImmortalCharacterPathBonuses Bonuses = UImmortalCharacterPathLibrary::CalculatePathBonuses(State);
		TestTrue(TEXT("Every path changes a consumed character attribute"),
			Bonuses.AttackMultiplier > 1.0f || Bonuses.DefenseMultiplier > 1.0f
			|| Bonuses.HealthMultiplier > 1.0f || Bonuses.ManaMultiplier > 1.0f
			|| Bonuses.AttackSpeedBonus > 0.0f || Bonuses.CriticalChanceBonus > 0.0f
			|| Bonuses.DamageReduction > 0.0f || Bonuses.CultivationRateMultiplier > 1.0f);
	}
	TestEqual(TEXT("All five path skills are distinct"), SkillEffects.Num(), 5);
	TestEqual(TEXT("All five equipment disciplines are distinct"), Disciplines.Num(), 5);

	FImmortalCultivationPathState UnselectedPath;
	TestEqual(TEXT("First path choice is free"),
		UImmortalCharacterPathLibrary::GetPathSwitchCost(UnselectedPath, EImmortalCultivationPath::Sword).SpiritStones, 0);
	FImmortalCultivationPathState SwordState;
	SwordState.Path = EImmortalCultivationPath::Sword;
	const FImmortalCraftingCost SwitchCost = UImmortalCharacterPathLibrary::GetPathSwitchCost(SwordState, EImmortalCultivationPath::Thunder);
	TestEqual(TEXT("First transfer costs one thousand stones"), SwitchCost.SpiritStones, 1000);
	TestEqual(TEXT("Transfer has two material requirements"), SwitchCost.Materials.Num(), 2);
	TestTrue(TEXT("Universal equipment is compatible"), UImmortalCharacterPathLibrary::IsEquipmentCompatible(
		EImmortalCultivationPath::Sword, EImmortalEquipmentDiscipline::Universal));
	TestTrue(TEXT("Matching equipment is compatible"), UImmortalCharacterPathLibrary::IsEquipmentCompatible(
		EImmortalCultivationPath::Sword, EImmortalEquipmentDiscipline::Sword));
	TestFalse(TEXT("Different discipline is incompatible"), UImmortalCharacterPathLibrary::IsEquipmentCompatible(
		EImmortalCultivationPath::Sword, EImmortalEquipmentDiscipline::Thunder));

	FImmortalEquipmentItem SwordEquipment = UImmortalEquipmentLibrary::GenerateCraftedEquipment(
		10, EImmortalEquipmentSlot::Weapon, EImmortalEquipmentQuality::Rare, EImmortalEquipmentDiscipline::Sword);
	TestEqual(TEXT("Explicit crafted discipline persists"), SwordEquipment.Discipline, EImmortalEquipmentDiscipline::Sword);
	SwordEquipment.Discipline = EImmortalEquipmentDiscipline::MAX;
	UImmortalEquipmentLibrary::NormalizeForgingState(SwordEquipment);
	TestEqual(TEXT("Invalid legacy discipline normalizes to universal"), SwordEquipment.Discipline, EImmortalEquipmentDiscipline::Universal);

	FImmortalTechniqueDefinition Basic;
	FImmortalTechniqueDefinition Sword;
	FImmortalTechniqueDefinition Flame;
	FImmortalTechniqueDefinition Thunder;
	TestTrue(TEXT("Breathing technique resolves"), UImmortalTechniqueLibrary::GetTechniqueDefinition(TEXT("BasicBreathing"), Basic));
	TestTrue(TEXT("Sword technique resolves"), UImmortalTechniqueLibrary::GetTechniqueDefinition(TEXT("QingyunSwordArt"), Sword));
	TestTrue(TEXT("Flame technique resolves"), UImmortalTechniqueLibrary::GetTechniqueDefinition(TEXT("BurningHeavenArt"), Flame));
	TestTrue(TEXT("Thunder technique resolves"), UImmortalTechniqueLibrary::GetTechniqueDefinition(TEXT("NineHeavensThunder"), Thunder));
	TestEqual(TEXT("Breathing is water"), Basic.Element, EImmortalElementType::Water);
	TestEqual(TEXT("Sword art is metal"), Sword.Element, EImmortalElementType::Metal);
	TestEqual(TEXT("Burning art is fire"), Flame.Element, EImmortalElementType::Fire);
	TestEqual(TEXT("Thunder art is thunder"), Thunder.Element, EImmortalElementType::Thunder);

	UImmortalCultivationComponent* Cultivation = NewObject<UImmortalCultivationComponent>();
	Cultivation->InitializeProgress(EImmortalCultivationRealm::QiRefining, 1, 0);
	Cultivation->SetRuntimeRateMultiplier(1.25f);
	Cultivation->SetTechniqueRateMultiplier(1.50f);
	Cultivation->SetCharacterPathRateMultiplier(1.20f);
	Cultivation->SetAlchemyRateMultiplier(2.0f);
	TestTrue(TEXT("Offline rate includes external, technique and character-build multipliers"), FMath::IsNearlyEqual(
		Cultivation->GetCultivationPerSecondWithoutAlchemyBoost(), 2.0f * 1.25f * 1.50f * 1.20f));
	TestTrue(TEXT("Online rate additionally includes alchemy"), FMath::IsNearlyEqual(
		Cultivation->GetCultivationPerSecond(), 2.0f * 1.25f * 1.50f * 1.20f * 2.0f));

	return true;
}

#endif
