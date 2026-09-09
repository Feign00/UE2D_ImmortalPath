#include "ImmortalDesktopPanelLayout.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalDesktopPanelLayoutTest, "ImmortalPath.UI.DesktopPanelLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalDesktopPanelLayoutTest::RunTest(const FString& Parameters)
{
	using namespace ImmortalDesktopPanelLayout;
	for (const FIntPoint Work : { FIntPoint(1280, 720), FIntPoint(1707, 1018), FIntPoint(2560, 1400) })
	{
		for (int32 Requested : {180, 320, 600})
		{
			const int32 Battle = BattleHeight(Requested, Work.Y);
			const int32 Height = WindowHeight(Battle, Work.Y, true);
			const auto Panel = Fit(FVector2D(Work.X, Height), ManagementSize, Battle);
			TestTrue(TEXT("Expanded native window stays in work area"), Height <= Work.Y);
			TestEqual(TEXT("Closing restores battle height"), WindowHeight(Battle, Work.Y, false), Battle);
			TestTrue(TEXT("Panel leaves battle and side margins visible"), Panel.Position.X >= 24
				&& Panel.Position.Y + ManagementSize.Y * Panel.Scale <= Height - Battle - 12 + 0.01f);
		}
	}
	TestTrue(TEXT("Large desktop has a full-height management area"),
		Fit(FVector2D(1920, WindowHeight(320,1080,true)), ManagementSize,320).Scale >= .99f);
	TestTrue(TEXT("Content fits below navigation inside enlarged shell"),
		ContentPosition.Y >= 42 && ContentPosition.Y + ContentSize.Y <= ManagementSize.Y);
	FMatrix Projection = FMatrix::Identity;
	AnchorBattleProjection(Projection, 680, 320);
	TestTrue(TEXT("Orthographic shift equals half of extra window height in pixels"),
		FMath::IsNearlyEqual(-Projection.M[3][1] * 680 / 2, 180.0, 1.e-6));
	Projection = FMatrix::Identity; Projection.M[2][3] = 1; Projection.M[3][3] = 0;
	AnchorBattleProjection(Projection, 680, 320);
	TestTrue(TEXT("Perspective shift uses clip W"), FMath::IsNearlyEqual(Projection.M[2][1], -360.0 / 680, 1.e-6));
	TestTrue(TEXT("PIE small viewport remains usable"), Fit(FVector2D(1280,320), FVector2D(1707,320),320).Scale > .5f);
	return true;
}
#endif
