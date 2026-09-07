#include "ImmortalPixelPlayerAssets.h"
#include "ImmortalPlayerCharacter.h"
#include "PaperFlipbook.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalPixelTimingTest, "ImmortalPath.Animation.PixelAttackTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalPixelTimingTest::RunTest(const FString& Parameters)
{
	for (const float Windup : { 0.1f, 0.25f, 0.5f, 2.0f })
	{
		const auto Playback = ImmortalPixelPlayerTiming::Attack(8.0f / 12.0f, 12, Windup);
		TestTrue(TEXT("Contact is synchronized without changing combat windup"), FMath::IsNearlyEqual(0.25f / Playback.Rate, Windup));
		TestTrue(TEXT("Recovery timer accounts for rate"), FMath::IsNearlyEqual(Playback.Duration * Playback.Rate, 8.0f / 12.0f));
	}
	for (const float Windup : { 0.0f, -1.0f })
	{
		const auto Playback = ImmortalPixelPlayerTiming::Attack(8.0f / 12.0f, 12, Windup);
		TestEqual(TEXT("Instant hit starts at contact"), Playback.Start, 0.25f);
		TestEqual(TEXT("Instant recovery uses normal rate"), Playback.Rate, 1.0f);
		TestTrue(TEXT("Instant hit retains recovery frames"), FMath::IsNearlyEqual(Playback.Duration, 5.0f / 12.0f, 1.e-6f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalPixelFamilyTest, "ImmortalPath.Art.PixelPlayerDefaultFamily",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalPixelFamilyTest::RunTest(const FString& Parameters)
{
	FImmortalPixelPlayerAssets Assets;
	TArray<UPaperFlipbook*> Clips;
	TestTrue(TEXT("Complete reflected family loads"), Assets.LoadComplete(Clips));
	TestEqual(TEXT("Five clips selected together"), Clips.Num(), 5);
	Assets.Death.Reset();
	TestFalse(TEXT("Missing one clip rejects new family"), Assets.LoadComplete(Clips));
	TestEqual(TEXT("No partial selection escapes"), Clips.Num(), 0);
	const UClass* PlayerClass = LoadClass<AImmortalPlayerCharacter>(nullptr, TEXT("/Game/GAME/Player/Player.Player_C"));
	if (TestNotNull(TEXT("Actual player Blueprint loads"), PlayerClass))
	{
		const auto* Defaults = Cast<AImmortalPlayerCharacter>(PlayerClass->GetDefaultObject());
		const FBoolProperty* Enabled = FindFProperty<FBoolProperty>(PlayerClass, TEXT("bUseDesktopPixelPlayer"));
		TestTrue(TEXT("Existing Blueprint opts into new C++ default"), Defaults && Enabled && Enabled->GetPropertyValue_InContainer(Defaults));
	}
	return true;
}
#endif
