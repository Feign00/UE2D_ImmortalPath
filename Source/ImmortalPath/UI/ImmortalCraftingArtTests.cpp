#include "ImmortalCraftingArt.h"
#include "ImmortalCraftingWidget.h"
#include "../Crafting/ImmortalCraftingTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalCraftingPresentationTest, "ImmortalPath.UI.CraftingPresentation",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalCraftingPresentationTest::RunTest(const FString& Parameters)
{
	TSet<int32> Cells;
	for (int32 Index = 0; Index < static_cast<int32>(EImmortalEquipmentSlot::MAX); ++Index)
	{
		const auto Slot = static_cast<EImmortalEquipmentSlot>(Index);
		Cells.Add(ImmortalCraftingArt::EquipmentCell(Slot));
		const FSlateBrush Brush = ImmortalCraftingArt::EquipmentBrush(nullptr, Slot);
		const FBox2f UV = Brush.GetUVRegion();
		TestTrue(TEXT("Equipment UV bounded and square"), UV.bIsValid && UV.Min.X >= 0 && UV.Min.Y >= 0
			&& UV.Max.X <= 1 && UV.Max.Y <= 1
			&& FMath::IsNearlyEqual(UV.GetSize().X * 1254, 418.0f, 0.01f)
			&& FMath::IsNearlyEqual(UV.GetSize().Y * 1254, 418.0f, 0.01f));
		TestEqual(TEXT("Missing atlas hides art without hiding label"), Brush.DrawAs, ESlateBrushDrawType::NoDrawType);
	}
	TestEqual(TEXT("All nine equipment slots have distinct art"), Cells.Num(), 9);
	TestFalse(TEXT("Known slots never map to unknown cell"), Cells.Contains(INDEX_NONE));
	TestEqual(TEXT("Legacy enum boots maps to sixth visual cell"), ImmortalCraftingArt::EquipmentCell(EImmortalEquipmentSlot::Boots), 5);
	TestEqual(TEXT("Legacy accessory maps to ninth visual cell"), ImmortalCraftingArt::EquipmentCell(EImmortalEquipmentSlot::Accessory), 8);
	TestEqual(TEXT("Invalid enum has no misleading image"), ImmortalCraftingArt::EquipmentCell(EImmortalEquipmentSlot::MAX), INDEX_NONE);
	for (FName Recipe : UImmortalCraftingLibrary::GetKnownRecipeIds())
	{
		FImmortalCraftingRecipeDefinition Definition;
		TestTrue(TEXT("Recipe exists"), UImmortalCraftingLibrary::GetRecipeDefinition(Recipe, Definition));
		TestTrue(TEXT("Recipe output covered"), ImmortalCraftingArt::EquipmentCell(Definition.OutputSlot) != INDEX_NONE);
		for (const auto& Cost : Definition.Cost.Materials)
			TestTrue(TEXT("Recipe material covered by either atlas"), ImmortalCraftingArt::ForgeCell(Cost.MaterialId) != INDEX_NONE
				|| ImmortalAlchemyArt::MaterialCell(Cost.MaterialId) != INDEX_NONE);
	}
	TestEqual(TEXT("Unknown material not substituted"), ImmortalCraftingArt::ForgeCell(TEXT("UnknownMaterial")), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalCraftingAtlasTest, "ImmortalPath.Art.CraftingAtlases",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalCraftingAtlasTest::RunTest(const FString& Parameters)
{
	const auto* Defaults = GetDefault<UImmortalCraftingWidget>();
	const UTexture2D* Atlases[] = {Defaults->GetEquipmentAtlas(), Defaults->GetForgeAtlas(), Defaults->GetMaterialAtlas()};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (!TestNotNull(TEXT("Configured crafting atlas loads"), Atlases[Index])) return false;
		TestEqual(TEXT("Atlas imported width"), Atlases[Index]->GetImportedSize().X, Index == 0 ? 1254 : 1536);
		TestEqual(TEXT("Atlas imported height"), Atlases[Index]->GetImportedSize().Y, Index == 0 ? 1254 : 1024);
		TestEqual(TEXT("UI compression preserves alpha"), Atlases[Index]->CompressionSettings, TC_EditorIcon);
		TestTrue(TEXT("UI atlas resident"), Atlases[Index]->NeverStream);
	}
	TestEqual(TEXT("Unknown ID hidden with loaded texture"),
		ImmortalCraftingArt::ForgeBrush(const_cast<UTexture2D*>(Atlases[1]), TEXT("Unknown")).DrawAs, ESlateBrushDrawType::NoDrawType);
	return true;
}
#endif
