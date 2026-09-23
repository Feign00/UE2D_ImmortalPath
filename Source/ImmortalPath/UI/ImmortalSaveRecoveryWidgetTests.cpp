#include "ImmortalSaveRecoveryWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalSaveRecoveryWidgetStateTest,
	"ImmortalPath.UI.SaveRecoveryStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalSaveRecoveryWidgetStateTest::RunTest(const FString& Parameters)
{
	struct FRecoveryCase
	{
		const TCHAR* Name;
		bool bHasBackup;
		bool bMainMissing;
		bool bIncompatibleVersion;
		const TCHAR* Title;
		const TCHAR* ExplanationNeedle;
		const TCHAR* BackupNeedle;
		bool bShowRestore;
	};

	const FRecoveryCase Cases[] = {
		{ TEXT("Valid backup"), true, false, false,
			TEXT("存档读取失败"), TEXT("无法读取"), TEXT("恢复上一份备份"), true },
		{ TEXT("No backup"), false, false, false,
			TEXT("存档读取失败"), TEXT("无法读取"), TEXT("没有找到可用备份"), false },
		{ TEXT("Missing main with backup"), true, true, false,
			TEXT("主存档缺失"), TEXT("不会建立新档"), TEXT("恢复上一份备份"), true },
		{ TEXT("Missing main with unusable backup"), false, true, false,
			TEXT("主存档缺失"), TEXT("备份文件也无法"), TEXT("没有找到可用备份"), false },
		{ TEXT("Newer version with backup"), true, false, true,
			TEXT("存档版本不兼容"), TEXT("存档或备份由更新版本"), TEXT("不会覆盖新版本备份"), false },
	};

	for (const FRecoveryCase& Case : Cases)
	{
		UImmortalSaveRecoveryWidget* Widget = NewObject<UImmortalSaveRecoveryWidget>();
		if (!TestNotNull(*FString::Printf(TEXT("%s: widget created"), Case.Name), Widget)
			|| !TestTrue(*FString::Printf(TEXT("%s: widget initialized"), Case.Name),
				Widget->Initialize())
			|| !TestNotNull(*FString::Printf(TEXT("%s: widget tree exists"), Case.Name),
				Widget->WidgetTree.Get()))
		{
			return false;
		}

		// A native UUserWidget without a player context does not invoke
		// NativeOnInitialized. Build the same tree without Slate or a viewport.
		Widget->BuildRecoveryWidgetTree();
		Widget->InitializeForPlayer(
			nullptr, Case.bHasBackup, Case.bMainMissing, Case.bIncompatibleVersion);

		UTextBlock* Title = Widget->WidgetTree->FindWidget<UTextBlock>(
			FName(TEXT("SaveRecoveryTitle")));
		UTextBlock* Explanation = Widget->WidgetTree->FindWidget<UTextBlock>(
			FName(TEXT("SaveRecoveryExplanation")));
		UTextBlock* BackupExplanation = Widget->WidgetTree->FindWidget<UTextBlock>(
			FName(TEXT("SaveRecoveryBackupExplanation")));
		UButton* Restore = Widget->WidgetTree->FindWidget<UButton>(
			FName(TEXT("SaveRecoveryRestoreButton")));
		UButton* Exit = Widget->WidgetTree->FindWidget<UButton>(
			FName(TEXT("SaveRecoveryExitButton")));
		if (!TestNotNull(*FString::Printf(TEXT("%s: title exists"), Case.Name), Title)
			|| !TestNotNull(*FString::Printf(TEXT("%s: explanation exists"), Case.Name), Explanation)
			|| !TestNotNull(*FString::Printf(TEXT("%s: backup explanation exists"), Case.Name), BackupExplanation)
			|| !TestNotNull(*FString::Printf(TEXT("%s: restore button exists"), Case.Name), Restore)
			|| !TestNotNull(*FString::Printf(TEXT("%s: exit button exists"), Case.Name), Exit)
			|| !TestNotNull(*FString::Printf(TEXT("%s: restore container exists"), Case.Name), Restore->GetParent())
			|| !TestNotNull(*FString::Printf(TEXT("%s: exit container exists"), Case.Name), Exit->GetParent()))
		{
			return false;
		}

		TestEqual(*FString::Printf(TEXT("%s: title"), Case.Name),
			Title->GetText().ToString(), FString(Case.Title));
		TestTrue(*FString::Printf(TEXT("%s: main explanation"), Case.Name),
			Explanation->GetText().ToString().Contains(Case.ExplanationNeedle));
		TestTrue(*FString::Printf(TEXT("%s: backup explanation"), Case.Name),
			BackupExplanation->GetText().ToString().Contains(Case.BackupNeedle));
		TestEqual(*FString::Printf(TEXT("%s: restore visibility"), Case.Name),
			Restore->GetParent()->GetVisibility(),
			Case.bShowRestore ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		TestEqual(*FString::Printf(TEXT("%s: exit visibility"), Case.Name),
			Exit->GetParent()->GetVisibility(), ESlateVisibility::Visible);
	}

	return true;
}

#endif
