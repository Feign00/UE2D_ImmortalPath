#include "ImmortalInventoryPresentation.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalInventoryComparisonTest,
	"ImmortalPath.UI.InventoryAttributeComparison", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalInventoryComparisonTest::RunTest(const FString& Parameters)
{
	FImmortalEquipmentItem Candidate, Current;
	Candidate.AttackBonus = 15; Current.AttackBonus = 10;
	Candidate.HealthBonus = 20; Current.HealthBonus = 50;
	Candidate.CriticalChanceBonus = .035f; Current.CriticalChanceBonus = .01f;
	const FString Comparison = ImmortalInventoryPresentation::Compare(Candidate, Current, true);
	TestTrue(TEXT("Show gain"), Comparison.Contains(TEXT("攻击 +5.0")));
	TestTrue(TEXT("Show loss even if overall score increases"), Comparison.Contains(TEXT("生命 -30.0")));
	TestTrue(TEXT("Percentage differences use percentage points"), Comparison.Contains(TEXT("暴击 +2.5百分点")));
	TestFalse(TEXT("Unchanged stats do not crowd the comparison"), Comparison.Contains(TEXT("防御")));
	TestTrue(TEXT("Does not promise an exact character power change"), Comparison.Contains(TEXT("未计套装与流派联动")));
	TestTrue(TEXT("Empty slot ignores unrelated supplied current item"),
		ImmortalInventoryPresentation::Compare(Candidate, Current, false).Contains(TEXT("攻击 +15.0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalInventoryEquipmentLayoutTest,
	"ImmortalPath.UI.InventoryPaperDollLayout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalInventoryEquipmentLayoutTest::RunTest(const FString& Parameters)
{
	using namespace ImmortalInventoryPresentation;
	TSet<FIntPoint> Cells;
	const FIntPoint Locations[] = {{0,0},{5,0},{0,1},{5,1},{0,2},{5,2},{0,3},{1,3},{2,3},{3,3}};
	for (const auto Cell : Locations)
	{
		Cells.Add(Cell);
		const FVector2D P = EquipmentPosition(Cell.X, Cell.Y);
		TestTrue(TEXT("Equipment stays in its 388x480 region"), P.X >= 0 && P.Y >= 0 && P.X + SlotSize <= 388 && P.Y + SlotSize <= 480);
		TestTrue(TEXT("Portrait area remains clear"), P.X + SlotSize <= 88 || P.X >= 288 || P.Y >= 320);
	}
	TestEqual(TEXT("Nine equipment and one artifact position"), Cells.Num(), 10);
	TestTrue(TEXT("Backpack has room for seven large padded cells and scrollbar"), BackpackColumns * (SlotSize + 4) + 10 <= 634);
	return true;
}
#endif
