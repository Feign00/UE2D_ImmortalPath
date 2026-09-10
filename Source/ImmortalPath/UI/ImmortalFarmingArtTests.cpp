#include "ImmortalFarmingArt.h"
#include "ImmortalFarmingWidget.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalFarmingArtStatesTest, "ImmortalPath.UI.FarmingArtStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalFarmingArtStatesTest::RunTest(const FString& Parameters)
{
	FImmortalFarmingPlotView View;
	TestEqual(TEXT("Invalid plots have a locked illustration"), ImmortalFarmingArt::Cell(View), 5);
	View.bValidPlot = true; View.bUnlocked = true;
	TestEqual(TEXT("Empty soil"), ImmortalFarmingArt::Cell(View), 0);
	View.CropId = TEXT("SpiritGrassCrop"); View.GrowthStage = EImmortalFarmingGrowthStage::Seedling;
	TestEqual(TEXT("Young crops show seedlings"), ImmortalFarmingArt::Cell(View), 1);
	View.GrowthStage = EImmortalFarmingGrowthStage::Mature;
	TestEqual(TEXT("Spirit grass is identifiable"), ImmortalFarmingArt::Cell(View), 2);
	View.CropId = TEXT("ImmortalFruitCrop"); TestEqual(TEXT("Fruit"), ImmortalFarmingArt::Cell(View), 3);
	View.CropId = TEXT("SpiritWoodCrop"); TestEqual(TEXT("Spirit wood"), ImmortalFarmingArt::Cell(View), 4);
	View.bUnlocked = false; TestEqual(TEXT("Locked takes priority over stale crop data"), ImmortalFarmingArt::Cell(View), 5);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const auto UV = ImmortalFarmingArt::CellUV(Index);
		TestTrue(TEXT("Atlas cells bounded"), UV.Min.X >= 0 && UV.Min.Y >= 0 && UV.Max.X <= 1 && UV.Max.Y <= 1);
	}
	TestEqual(TEXT("Missing art never draws a white rectangle"), ImmortalFarmingArt::Brush(nullptr, 0).DrawAs, ESlateBrushDrawType::NoDrawType);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalFarmingAtlasTest, "ImmortalPath.Art.FarmingAtlas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalFarmingAtlasTest::RunTest(const FString& Parameters)
{
	const UImmortalFarmingWidget* Defaults = GetDefault<UImmortalFarmingWidget>();
	const FSoftObjectProperty* Property = FindFProperty<FSoftObjectProperty>(Defaults->GetClass(), TEXT("FarmingAtlas"));
	if (!TestNotNull(TEXT("Atlas is a reflected cook dependency"), Property)) return false;
	const FSoftObjectPtr* Reference = Property->ContainerPtrToValuePtr<FSoftObjectPtr>(Defaults);
	const UTexture2D* Texture = Cast<UTexture2D>(Reference->LoadSynchronous());
	if (!TestNotNull(TEXT("Farming atlas imported"), Texture)) return false;
	TestEqual(TEXT("Atlas width"), Texture->GetImportedSize().X, 1536);
	TestEqual(TEXT("Atlas height"), Texture->GetImportedSize().Y, 1024);
	TestEqual(TEXT("Lossless RGBA compression"), Texture->CompressionSettings, TC_EditorIcon);
	TestTrue(TEXT("Resident UI art"), Texture->NeverStream);
	return true;
}
#endif
