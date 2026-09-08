#include "ImmortalFeaturePageLayout.h"
#include "ImmortalUITheme.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalFeatureLabelLayoutTest,
	"ImmortalPath.UI.FeatureButtonLabels", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalFeatureLabelLayoutTest::RunTest(const FString& Parameters)
{
	UWidgetTree* Tree = NewObject<UWidgetTree>();
	UButton* Button = Tree->ConstructWidget<UButton>();
	UTextBlock* Label = Tree->ConstructWidget<UTextBlock>();
	const FText Caption = FText::FromString(TEXT("青云宗\n点击查看"));
	Label->SetText(Caption);
	Label->SetAutoWrapText(true);
	Button->AddChild(Label);
	ImmortalFeaturePageLayout::StabilizeButtonLabel(Button, Label);
	UButtonSlot* Slot = CastChecked<UButtonSlot>(Label->Slot);
	TestFalse(TEXT("Short captions cannot auto-wrap into vertical text"), Label->GetAutoWrapText());
	TestEqual(TEXT("Caption fills the available button width"), Slot->GetHorizontalAlignment(), HAlign_Fill);
	TestEqual(TEXT("Caption is vertically centered"), Slot->GetVerticalAlignment(), VAlign_Center);
	TestEqual(TEXT("Explicit line breaks retained"), Label->GetText().ToString(), Caption.ToString());
	TestEqual(TEXT("Long future captions do not cover neighbors"), Label->GetTextOverflowPolicy(), ETextOverflowPolicy::Ellipsis);
	TestEqual(TEXT("Original theme uses scalable borders"), ImmortalUITheme::ButtonStyle().Normal.DrawAs, ESlateBrushDrawType::RoundedBox);
	TestTrue(TEXT("Selection remains visibly distinct"), ImmortalUITheme::ButtonStyle(true).Normal.TintColor
		!= ImmortalUITheme::ButtonStyle(false).Normal.TintColor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalFeatureScrollLayoutTest,
	"ImmortalPath.UI.FeatureBoundedDetails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImmortalFeatureScrollLayoutTest::RunTest(const FString& Parameters)
{
	UWidgetTree* Tree = NewObject<UWidgetTree>();
	UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>();
	Tree->RootWidget = Canvas;
	UTextBlock* Detail = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LongDetail"));
	const FText Content = FText::FromString(TEXT("材料一\n材料二\n材料三\n完整效果与解锁要求"));
	Detail->SetText(Content);
	UCanvasPanelSlot* Before = Canvas->AddChildToCanvas(Detail);
	Before->SetPosition(FVector2D(300, 121)); Before->SetSize(FVector2D(430, 55)); Before->SetZOrder(3);
	ImmortalFeaturePageLayout::MakeScrollable(Tree, Detail);
	UScrollBox* Scroll = Cast<UScrollBox>(Detail->GetParent());
	if (!TestNotNull(TEXT("Long details live in their own scroll region"), Scroll)) return false;
	UCanvasPanelSlot* After = CastChecked<UCanvasPanelSlot>(Scroll->Slot);
	TestEqual(TEXT("Position retained"), After->GetPosition(), FVector2D(300, 121));
	TestEqual(TEXT("Adjacent actions keep their space"), After->GetSize(), FVector2D(430, 55));
	TestEqual(TEXT("Layer order retained"), After->GetZOrder(), 3);
	TestEqual(TEXT("Text cannot paint outside the region"), Scroll->GetClipping(), EWidgetClipping::ClipToBounds);
	TestEqual(TEXT("All detail lines retained"), Detail->GetText().ToString(), Content.ToString());
	ImmortalFeaturePageLayout::MakeScrollable(Tree, Detail);
	TestEqual(TEXT("Repeated application does not nest scroll boxes"), Detail->GetParent(), static_cast<UPanelWidget*>(Scroll));
	return true;
}
#endif
