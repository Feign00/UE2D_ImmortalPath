// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalAlchemyTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalAlchemyCoreTest,
	"ImmortalPath.Alchemy.RecipesCraftingAndInventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalAlchemyCoreTest::RunTest(const FString& Parameters)
{
	const TArray<FName> RecipeIds = UImmortalAlchemyLibrary::GetKnownRecipeIds();
	TestEqual(TEXT("Five initial recipes"), RecipeIds.Num(), 5);
	for (const FName RecipeId : RecipeIds)
	{
		FImmortalPillDefinition Definition;
		TestTrue(*FString::Printf(TEXT("Recipe definition exists: %s"), *RecipeId.ToString()),
			UImmortalAlchemyLibrary::GetPillDefinition(RecipeId, Definition));
		TestFalse(TEXT("Recipe has ingredients"), Definition.Ingredients.IsEmpty());
		TestTrue(TEXT("Success chance is valid"), Definition.BaseSuccessChance > 0.0f && Definition.BaseSuccessChance <= 1.0f);
		TestTrue(TEXT("Exceptional chance fits success band"), Definition.ExceptionalChance <= Definition.BaseSuccessChance);
	}

	FImmortalPillDefinition Healing;
	UImmortalAlchemyLibrary::GetPillDefinition(TEXT("HealingPill"), Healing);
	TestEqual(TEXT("Low roll creates exceptional pill"),
		UImmortalAlchemyLibrary::CalculateOutcome(Healing, 0.05f), EImmortalAlchemyOutcome::Exceptional);
	TestEqual(TEXT("Middle roll creates ordinary pill"),
		UImmortalAlchemyLibrary::CalculateOutcome(Healing, 0.50f), EImmortalAlchemyOutcome::Ordinary);
	TestEqual(TEXT("High roll fails"),
		UImmortalAlchemyLibrary::CalculateOutcome(Healing, 0.95f), EImmortalAlchemyOutcome::Failure);

	TArray<FImmortalMaterialStack> Materials;
	UImmortalMaterialLibrary::AddMaterialStack(Materials, TEXT("SpiritGrass"), 2);
	TestFalse(TEXT("Incomplete ingredients cannot craft"), UImmortalAlchemyLibrary::CanCraft(Materials, Healing));
	const TArray<FImmortalMaterialStack> BeforeFailedConsume = Materials;
	TestFalse(TEXT("Incomplete transaction is rejected"), UImmortalAlchemyLibrary::ConsumeIngredients(Materials, Healing));
	TestEqual(TEXT("Rejected transaction does not mutate inventory"), Materials[0].Quantity, BeforeFailedConsume[0].Quantity);
	UImmortalMaterialLibrary::AddMaterialStack(Materials, TEXT("SpiritLiquid"), 1);
	TestTrue(TEXT("Complete ingredients can craft"), UImmortalAlchemyLibrary::CanCraft(Materials, Healing));
	TestTrue(TEXT("Complete transaction consumes ingredients"), UImmortalAlchemyLibrary::ConsumeIngredients(Materials, Healing));
	TestTrue(TEXT("All exact ingredients were removed"), Materials.IsEmpty());

	TArray<FImmortalPillStack> Pills;
	TestEqual(TEXT("Pill add"), UImmortalAlchemyLibrary::AddPillStack(Pills, TEXT("HealingPill"), EImmortalPillQuality::Ordinary, 2), 2);
	TestEqual(TEXT("Pill stack merge"), UImmortalAlchemyLibrary::AddPillStack(Pills, TEXT("HealingPill"), EImmortalPillQuality::Ordinary, 3), 3);
	TestEqual(TEXT("Merged pill quantity"), UImmortalAlchemyLibrary::GetPillQuantity(Pills, TEXT("HealingPill"), EImmortalPillQuality::Ordinary), 5);
	TestTrue(TEXT("Remove one pill"), UImmortalAlchemyLibrary::RemovePill(Pills, TEXT("HealingPill"), EImmortalPillQuality::Ordinary, 1));
	TestEqual(TEXT("Pill quantity after use"), Pills[0].Quantity, 4);
	TestEqual(TEXT("Unknown pill rejected"), UImmortalAlchemyLibrary::AddPillStack(Pills, TEXT("UnknownPill"), EImmortalPillQuality::Ordinary, 1), 0);
	return true;
}

#endif

