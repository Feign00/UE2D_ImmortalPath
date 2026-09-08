#include "ImmortalManagementArt.h"
#include "ImmortalManagementWidget.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalManagementBuildingMapTest,
	"ImmortalPath.UI.ManagementBuildingCells", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalManagementBuildingMapTest::RunTest(const FString& Parameters)
{
	const EImmortalManagementFeature Features[] = {EImmortalManagementFeature::Cultivation,
		EImmortalManagementFeature::Sect, EImmortalManagementFeature::Alchemy,
		EImmortalManagementFeature::Crafting, EImmortalManagementFeature::Cave, EImmortalManagementFeature::Farming};
	TSet<int32> Cells;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Features); ++Index)
	{
		const int32 Cell = ImmortalManagementArt::BuildingCell(Features[Index]);
		TestEqual(TEXT("Art cells follow the authored row-major order"), Cell, Index);
		Cells.Add(Cell);
		const FBox2f UV = ImmortalManagementArt::CellUV(Cell);
		TestTrue(TEXT("UV region remains within the atlas"), UV.Min.X >= 0 && UV.Min.Y >= 0 && UV.Max.X <= 1 && UV.Max.Y <= 1);
		TestTrue(TEXT("Each region is one 512px square"), FMath::IsNearlyEqual(UV.GetSize().X * 1536, 512.0f, 0.001f)
			&& FMath::IsNearlyEqual(UV.GetSize().Y * 1024, 512.0f, 0.001f));
	}
	TestEqual(TEXT("All six buildings have distinct cells"), Cells.Num(), 6);
	TestEqual(TEXT("Shop uses a separate illustration"), ImmortalManagementArt::BuildingCell(EImmortalManagementFeature::Shop), INDEX_NONE);
	TestEqual(TEXT("Unchanged features retain their icons"), ImmortalManagementArt::BuildingCell(EImmortalManagementFeature::Inventory), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalManagementBuildingAssetsTest,
	"ImmortalPath.Art.ManagementBuildings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalManagementBuildingAssetsTest::RunTest(const FString& Parameters)
{
	const UImmortalManagementWidget* Defaults = GetDefault<UImmortalManagementWidget>();
	for (const TCHAR* PropertyName : {TEXT("MortalBuildingAtlas"), TEXT("MortalMarketBuilding")})
	{
		const FSoftObjectProperty* Property = FindFProperty<FSoftObjectProperty>(Defaults->GetClass(), PropertyName);
		if (!TestNotNull(TEXT("Building art is a reflected soft dependency"), Property)) continue;
		const FSoftObjectPtr* Reference = Property->ContainerPtrToValuePtr<FSoftObjectPtr>(Defaults);
		UTexture2D* Texture = Cast<UTexture2D>(Reference->LoadSynchronous());
		if (!TestNotNull(PropertyName, Texture)) continue;
		// Imported dimensions remain available even when NullRHI has no GPU resource.
		TestEqual(TEXT("Authored width retained"), Texture->GetImportedSize().X, 1536);
		TestEqual(TEXT("Authored height retained"), Texture->GetImportedSize().Y, 1024);
		TestEqual(TEXT("Lossless menu-art compression"), Texture->CompressionSettings, TC_EditorIcon);
		TestTrue(TEXT("Menu art is resident while used"), Texture->NeverStream);
	}
	return true;
}
#endif
