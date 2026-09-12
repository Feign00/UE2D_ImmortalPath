#include "ImmortalAlchemyArt.h"
#include "ImmortalAlchemyWidget.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalAlchemyPresentationTest, "ImmortalPath.UI.AlchemyPresentation",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalAlchemyPresentationTest::RunTest(const FString& Parameters)
{
 TSet<int32> Cells;
 for (FName Id : UImmortalAlchemyLibrary::GetKnownRecipeIds())
 {
  const int32 Cell = ImmortalAlchemyArt::Cell(Id);
  TestTrue(TEXT("Known pill has its own icon"), Cell >= 0 && Cell < 5);
  Cells.Add(Cell);
  const FSlateBrush Brush = ImmortalAlchemyArt::Brush(nullptr, Id);
  const FBox2f UV = Brush.GetUVRegion();
  TestTrue(TEXT("UV stays within atlas"), UV.bIsValid && UV.Min.X >= 0 && UV.Min.Y >= 0 && UV.Max.X <= 1 && UV.Max.Y <= 1);
  // Normalized thirds are not exactly representable; tolerate 0.01 source pixel, not a visual distortion.
  TestTrue(TEXT("Square source cell"), FMath::IsNearlyEqual(UV.GetSize().X * 1536, UV.GetSize().Y * 1024, 0.01f));
  TestEqual(TEXT("Missing resource keeps glyph fallback"), Brush.DrawAs, ESlateBrushDrawType::NoDrawType);
 }
 TestEqual(TEXT("Five distinct pill images"), Cells.Num(), 5);
 TestEqual(TEXT("Cauldron has its separate cell"), ImmortalAlchemyArt::Cell(TEXT("Cauldron")), 5);
 TestEqual(TEXT("Unknown pill does not borrow art"), ImmortalAlchemyArt::Cell(TEXT("UnknownPill")), INDEX_NONE);
 TSet<int32> Materials;
 for (FName Id : {FName(TEXT("SpiritGrass")), FName(TEXT("DemonCore")), FName(TEXT("SpiritLiquid")),
  FName(TEXT("Ore")), FName(TEXT("ImmortalFruit")), FName(TEXT("SpiritStones"))})
 {
  const int32 Index = ImmortalAlchemyArt::MaterialCell(Id);
  TestTrue(TEXT("Known material has an icon"), Index >= 0 && Index < 6);
  Materials.Add(Index);
 }
 TestEqual(TEXT("Six distinct material icons"), Materials.Num(), 6);
 TestEqual(TEXT("Unknown material has no misleading icon"), ImmortalAlchemyArt::MaterialCell(TEXT("Unknown")), INDEX_NONE);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalAlchemyAtlasTest, "ImmortalPath.Art.AlchemyAtlas",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalAlchemyAtlasTest::RunTest(const FString& Parameters)
{
 const UTexture2D* Atlas = GetDefault<UImmortalAlchemyWidget>()->GetAlchemyAtlas();
 if (!TestNotNull(TEXT("Configured atlas loads"), Atlas)) return false;
 TestEqual(TEXT("Atlas width"), Atlas->GetImportedSize().X, 1536);
 TestEqual(TEXT("Atlas height"), Atlas->GetImportedSize().Y, 1024);
 TestEqual(TEXT("Lossless UI compression"), Atlas->CompressionSettings, TC_EditorIcon);
 TestTrue(TEXT("Resident atlas"), Atlas->NeverStream);
 const UTexture2D* Materials = GetDefault<UImmortalAlchemyWidget>()->GetMaterialAtlas();
 if (!TestNotNull(TEXT("Material atlas loads"), Materials)) return false;
 TestEqual(TEXT("Material width"), Materials->GetImportedSize().X, 1536);
 TestEqual(TEXT("Material height"), Materials->GetImportedSize().Y, 1024);
 TestEqual(TEXT("Material lossless UI compression"), Materials->CompressionSettings, TC_EditorIcon);
 TestTrue(TEXT("Material atlas resident"), Materials->NeverStream);
 TestEqual(TEXT("Unknown ID with loaded atlas is hidden"),
  ImmortalAlchemyArt::Brush(const_cast<UTexture2D*>(Atlas), TEXT("Unknown")).DrawAs, ESlateBrushDrawType::NoDrawType);
 return true;
}
#endif
