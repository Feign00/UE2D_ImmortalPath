#include "ImmortalSectArt.h"
#include "ImmortalSectWidget.h"
#include "../Sects/ImmortalSectTypes.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalSectPresentationTest, "ImmortalPath.UI.SectPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalSectPresentationTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Ids = UImmortalSectLibrary::GetKnownSectIds();
	TestEqual(TEXT("Four known sects"), Ids.Num(), 4);
	TSet<int32> Cells;
	for (FName Id : Ids)
	{
		const int32 Cell = ImmortalSectArt::Cell(Id);
		TestTrue(TEXT("Known sect has an emblem"), Cell >= 0 && Cell < 4);
		Cells.Add(Cell);
		const FSlateBrush Brush = ImmortalSectArt::Brush(nullptr, Id);
		const FBox2f UV = Brush.GetUVRegion();
		TestTrue(TEXT("UV cell stays inside atlas"), UV.bIsValid && UV.Min.X >= 0 && UV.Min.Y >= 0 && UV.Max.X <= 1 && UV.Max.Y <= 1);
		TestEqual(TEXT("Missing texture preserves text-only fallback"), Brush.DrawAs, ESlateBrushDrawType::NoDrawType);
	}
	TestEqual(TEXT("Sect emblems are distinct"), Cells.Num(), 4);
	TestEqual(TEXT("Unknown sect does not borrow another emblem"), ImmortalSectArt::Cell(TEXT("Unknown")), INDEX_NONE);
	TestEqual(TEXT("Half progress"), ImmortalSectArt::ProgressFraction(10, 20, true), 0.5f);
	TestEqual(TEXT("No membership means no active progress"), ImmortalSectArt::ProgressFraction(20, 20, false), 0.0f);
	TestEqual(TEXT("Over-complete progress clamped"), ImmortalSectArt::ProgressFraction(99, 20, true), 1.0f);
	TestEqual(TEXT("Negative progress clamped"), ImmortalSectArt::ProgressFraction(-1, 20, true), 0.0f);
	TestEqual(TEXT("Zero target is safe"), ImmortalSectArt::ProgressFraction(1, 0, true), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalSectAtlasTest, "ImmortalPath.Art.SectAtlas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalSectAtlasTest::RunTest(const FString& Parameters)
{
	const UImmortalSectWidget* Defaults = GetDefault<UImmortalSectWidget>();
	const FSoftObjectProperty* Property = FindFProperty<FSoftObjectProperty>(Defaults->GetClass(), TEXT("SectAtlas"));
	if (!TestNotNull(TEXT("Reflected atlas reference"), Property)) return false;
	const FSoftObjectPtr* Reference = Property->ContainerPtrToValuePtr<FSoftObjectPtr>(Defaults);
	const UTexture2D* Texture = Cast<UTexture2D>(Reference->LoadSynchronous());
	if (!TestNotNull(TEXT("Sect atlas imported"), Texture)) return false;
	TestEqual(TEXT("Atlas width"), Texture->GetImportedSize().X, 1254);
	TestEqual(TEXT("Atlas height"), Texture->GetImportedSize().Y, 1254);
	TestEqual(TEXT("Lossless UI compression"), Texture->CompressionSettings, TC_EditorIcon);
	TestTrue(TEXT("Resident UI atlas"), Texture->NeverStream);
	return true;
}
#endif
