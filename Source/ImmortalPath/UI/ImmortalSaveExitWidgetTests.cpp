#include "ImmortalSaveExitWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalSaveExitWidgetStateTest,
	"ImmortalPath.UI.SaveExitStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalSaveExitWidgetStateTest::RunTest(const FString& Parameters)
{
	UImmortalSaveExitWidget* Widget = NewObject<UImmortalSaveExitWidget>();
	if (!TestNotNull(TEXT("widget created"), Widget)
		|| !TestTrue(TEXT("widget initialized"), Widget->Initialize())
		|| !TestNotNull(TEXT("widget tree exists"), Widget->WidgetTree.Get()))
	{
		return false;
	}

	// NativeOnInitialized requires a player context. Match the recovery-widget
	// test by building the native tree directly without Slate or a viewport.
	Widget->BuildWidgetTree();
	Widget->InitializeForPlayer(nullptr);
	UTextBlock* Title = Widget->WidgetTree->FindWidget<UTextBlock>(
		FName(TEXT("SaveExitTitle")));
	UTextBlock* Status = Widget->WidgetTree->FindWidget<UTextBlock>(
		FName(TEXT("SaveExitStatus")));
	UButton* Retry = Widget->WidgetTree->FindWidget<UButton>(
		FName(TEXT("SaveExitRetryButton")));
	UButton* Continue = Widget->WidgetTree->FindWidget<UButton>(
		FName(TEXT("SaveExitContinueButton")));
	UButton* Discard = Widget->WidgetTree->FindWidget<UButton>(
		FName(TEXT("SaveExitWithoutSavingButton")));
	UCanvasPanel* Canvas = Widget->WidgetTree->FindWidget<UCanvasPanel>(
		FName(TEXT("SaveExitCanvas")));
	if (!TestNotNull(TEXT("title exists"), Title)
		|| !TestNotNull(TEXT("status exists"), Status)
		|| !TestNotNull(TEXT("retry exists"), Retry)
		|| !TestNotNull(TEXT("continue exists"), Continue)
		|| !TestNotNull(TEXT("discard exists"), Discard)
		|| !TestNotNull(TEXT("transparent canvas exists"), Canvas)
		|| !TestNotNull(TEXT("discard label exists"), Widget->ExitWithoutSavingLabel.Get()))
	{
		return false;
	}

	TestEqual(TEXT("initial title"), Title->GetText().ToString(), FString(TEXT("退出前保存失败")));
	TestTrue(TEXT("initial failure message"),
		Status->GetText().ToString().Contains(TEXT("窗口仍在")));
	TestEqual(TEXT("initial discard label"),
		Widget->ExitWithoutSavingLabel->GetText().ToString(), FString(TEXT("跳过保存并退出")));
	TestFalse(TEXT("discard initially disarmed"), Widget->bExitWithoutSavingArmed);
	TestEqual(TEXT("transparent root does not intercept outside panel"),
		Canvas->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);

	Widget->HandleExitWithoutSavingClicked();
	TestTrue(TEXT("first discard click arms only"), Widget->bExitWithoutSavingArmed);
	TestEqual(TEXT("first discard click shows explicit confirmation"),
		Widget->ExitWithoutSavingLabel->GetText().ToString(),
		FString(TEXT("确认跳过保存并退出")));
	TestTrue(TEXT("first discard click warns of possible progress loss"),
		Status->GetText().ToString().Contains(TEXT("未保存的进度可能丢失")));
	TestTrue(TEXT("first discard click preserves existing autosaves"),
		Status->GetText().ToString().Contains(TEXT("已有自动存档仍保留")));

	Widget->HandleRetryClicked();
	TestFalse(TEXT("retry disarms discard"), Widget->bExitWithoutSavingArmed);
	TestEqual(TEXT("retry restores discard label"),
		Widget->ExitWithoutSavingLabel->GetText().ToString(), FString(TEXT("跳过保存并退出")));
	TestTrue(TEXT("null-player retry does not disable retry button"), Retry->GetIsEnabled());

	Widget->HandleExitWithoutSavingClicked();
	Widget->HandleContinueClicked();
	TestFalse(TEXT("continue disarms discard"), Widget->bExitWithoutSavingArmed);
	TestEqual(TEXT("continue restores discard label"),
		Widget->ExitWithoutSavingLabel->GetText().ToString(), FString(TEXT("跳过保存并退出")));

	Widget->HandleExitWithoutSavingClicked();
	Widget->ShowFailureMessage();
	TestFalse(TEXT("showing failure again disarms discard"), Widget->bExitWithoutSavingArmed);
	TestTrue(TEXT("failure message restored"),
		Status->GetText().ToString().Contains(TEXT("窗口仍在")));

	Widget->HandleExitWithoutSavingClicked();
	Widget->HandleExitWithoutSavingClicked();
	TestFalse(TEXT("second click with no player cannot quit"), Widget->bExitWithoutSavingArmed);
	TestTrue(TEXT("second click without player reports no exit"),
		Status->GetText().ToString().Contains(TEXT("未执行退出")));

	return true;
}

#endif
