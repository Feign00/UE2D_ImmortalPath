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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalPixelRecoveryTimingTest,
	"ImmortalPath.Animation.PixelAttackRecoveryTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalPixelRecoveryTimingTest::RunTest(const FString& Parameters)
{
	constexpr float ClipDuration = 8.0f / 12.0f;
	constexpr float ContactPosition = 3.0f / 12.0f;
	constexpr float Windup = 0.25f;
	const auto Initial = ImmortalPixelPlayerTiming::Attack(ClipDuration, 12.0f, Windup);
	TestTrue(TEXT("Original pre-contact timing remains aligned"),
		FMath::IsNearlyEqual(ContactPosition / Initial.Rate, Windup));

	const auto Normal = ImmortalPixelPlayerTiming::AttackRecovery(
		ClipDuration, ContactPosition, Initial.Rate, Windup, 1.0f);
	TestTrue(TEXT("1x recovery uses the original rate"), FMath::IsNearlyEqual(Normal.Rate, Initial.Rate));
	TestTrue(TEXT("1x attack finishes before the next period"),
		Normal.bFitsCycle && Windup + Normal.Duration <= 1.0f + UE_SMALL_NUMBER);

	const auto DoubleSpeed = ImmortalPixelPlayerTiming::AttackRecovery(
		ClipDuration, ContactPosition, Initial.Rate, Windup, 0.5f);
	TestTrue(TEXT("2x accelerates only post-contact frames"), DoubleSpeed.Rate > Initial.Rate);
	TestTrue(TEXT("2x full recovery finishes before the next attack timer"),
		DoubleSpeed.bFitsCycle && Windup + DoubleSpeed.Duration < 0.5f
		&& Windup + DoubleSpeed.Duration > 0.45f);
	TestTrue(TEXT("2x still reaches contact at the original windup"),
		FMath::IsNearlyEqual(ContactPosition / Initial.Rate, Windup));
	// A late contact callback has less time until the already-scheduled finish timer.
	const float LateRecoveryWindow = DoubleSpeed.Duration - 0.08f;
	const auto LateRecovery = ImmortalPixelPlayerTiming::AttackRecovery(
		ClipDuration, ContactPosition, Initial.Rate, 0.0f, LateRecoveryWindow);
	TestTrue(TEXT("Late contact still reaches the last recovery frame before the fixed deadline"),
		LateRecovery.bFitsCycle && LateRecovery.Rate > DoubleSpeed.Rate
		&& LateRecovery.Duration < LateRecoveryWindow);

	// A faster authored windup makes a 4x cycle feasible without moving contact.
	constexpr float FastWindup = 0.10f;
	const auto FastInitial = ImmortalPixelPlayerTiming::Attack(ClipDuration, 12.0f, FastWindup);
	const auto QuadrupleSpeed = ImmortalPixelPlayerTiming::AttackRecovery(
		ClipDuration, ContactPosition, FastInitial.Rate, FastWindup, 0.25f);
	TestTrue(TEXT("4x retains pre-contact alignment"),
		FMath::IsNearlyEqual(ContactPosition / FastInitial.Rate, FastWindup));
	TestTrue(TEXT("4x recovery accelerates without slowing initial playback"),
		QuadrupleSpeed.Rate >= FastInitial.Rate && QuadrupleSpeed.bFitsCycle
		&& FastWindup + QuadrupleSpeed.Duration <= 0.25f + UE_SMALL_NUMBER);

	const auto AlreadyFast = ImmortalPixelPlayerTiming::AttackRecovery(
		ClipDuration, ContactPosition, 3.0f, Windup, 1.0f);
	TestTrue(TEXT("Spare recovery window never slows a fast attack"),
		FMath::IsNearlyEqual(AlreadyFast.Rate, 3.0f));
	const auto Impossible = ImmortalPixelPlayerTiming::AttackRecovery(
		ClipDuration, ContactPosition, Initial.Rate, Windup, Windup);
	TestTrue(TEXT("No post-contact window is reported as infeasible"),
		!Impossible.bFitsCycle && FMath::IsFinite(Impossible.Rate)
		&& FMath::IsFinite(Impossible.Duration) && Impossible.Rate >= Initial.Rate);
	const auto Invalid = ImmortalPixelPlayerTiming::AttackRecovery(-1.0f, -1.0f, 0.0f, -1.0f, -1.0f);
	TestTrue(TEXT("Invalid inputs remain bounded and finite"),
		FMath::IsFinite(Invalid.Rate) && FMath::IsFinite(Invalid.Duration)
		&& Invalid.Rate > 0.0f && Invalid.Duration == 0.0f);
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
