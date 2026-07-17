// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalCraftingTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalCraftingCoreTest,
	"ImmortalPath.Crafting.RecipesEnhancementAndRefinement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalCraftingCoreTest::RunTest(const FString& Parameters)
{
	const TArray<FName> RecipeIds = UImmortalCraftingLibrary::GetKnownRecipeIds();
	TestEqual(TEXT("Five initial equipment recipes"), RecipeIds.Num(), 5);
	for (const FName RecipeId : RecipeIds)
	{
		FImmortalCraftingRecipeDefinition Recipe;
		TestTrue(TEXT("Recipe resolves"), UImmortalCraftingLibrary::GetRecipeDefinition(RecipeId, Recipe));
		TestFalse(TEXT("Recipe consumes materials"), Recipe.Cost.Materials.IsEmpty());
		TestTrue(TEXT("Recipe consumes spirit stones"), Recipe.Cost.SpiritStones > 0);
	}

	TArray<FImmortalMaterialStack> Materials;
	UImmortalMaterialLibrary::AddMaterialStack(Materials, TEXT("Ore"), 3);
	int32 SpiritStones = 999;
	FImmortalCraftingCost Unaffordable;
	Unaffordable.SpiritStones = 100;
	FImmortalCraftingMaterialCost OreCost;
	OreCost.MaterialId = TEXT("Ore");
	OreCost.Quantity = 4;
	Unaffordable.Materials.Add(OreCost);
	TestFalse(TEXT("Incomplete cost is rejected"), UImmortalCraftingLibrary::ConsumeCost(Materials, SpiritStones, Unaffordable));
	TestEqual(TEXT("Rejected transaction preserves ore"), UImmortalMaterialLibrary::GetMaterialQuantity(Materials, TEXT("Ore")), 3);
	TestEqual(TEXT("Rejected transaction preserves currency"), SpiritStones, 999);

	FImmortalEquipmentItem Item = UImmortalEquipmentLibrary::GenerateCraftedEquipment(
		25, EImmortalEquipmentSlot::Weapon, EImmortalEquipmentQuality::Rare);
	TestTrue(TEXT("Crafted item is valid"), Item.IsValid());
	TestEqual(TEXT("Rare item has three affixes"), Item.Affixes.Num(), 3);
	const float BaseAttack = Item.BaseAttackBonus;
	const TArray<FImmortalEquipmentAffix> OriginalAffixes = Item.Affixes;
	const float BeforePower = UImmortalEquipmentLibrary::CalculateEquipmentPower(Item);
	TestTrue(TEXT("Enhancement succeeds"), UImmortalEquipmentLibrary::EnhanceEquipment(Item));
	TestEqual(TEXT("Enhancement increases level"), Item.EnhancementLevel, 1);
	TestEqual(TEXT("Enhancement preserves base attack"), Item.BaseAttackBonus, BaseAttack);
	TestEqual(TEXT("Enhancement preserves affix count"), Item.Affixes.Num(), OriginalAffixes.Num());
	TestTrue(TEXT("Enhancement raises equipment power"), UImmortalEquipmentLibrary::CalculateEquipmentPower(Item) > BeforePower);

	TestTrue(TEXT("Refinement succeeds"), UImmortalEquipmentLibrary::RerollEquipmentAffixes(Item));
	TestEqual(TEXT("Refinement count increments"), Item.RefinementCount, 1);
	TestEqual(TEXT("Refinement preserves base attack"), Item.BaseAttackBonus, BaseAttack);
	TestEqual(TEXT("Refinement preserves enhancement"), Item.EnhancementLevel, 1);
	TestEqual(TEXT("Refinement preserves quality affix count"), Item.Affixes.Num(), 3);
	return true;
}

#endif
