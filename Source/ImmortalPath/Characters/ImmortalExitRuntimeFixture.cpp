// Development-only, real-window save-and-exit regression fixture.
#include "ImmortalPlayerCharacter.h"

#if !UE_BUILD_SHIPPING

#include "../Save/ImmortalPathSaveGame.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
	bool bExitFixtureClaimed = false;

	struct FExitSaveBytes
	{
		TArray<uint8> Main;
		TArray<uint8> Backup;
		bool bHasBackup = false;
	};

	void FailExitFixture(const TCHAR* Reason)
	{
		UE_LOG(LogTemp, Error, TEXT("ExitR04Fixture RESULT: FAIL | %s"), Reason);
		UImmortalPathSaveGame::ClearDevelopmentWriteFailureOnAttempt();
		FPlatformMisc::RequestExitWithStatus(false, 1);
	}

	bool CaptureSaveBytes(FExitSaveBytes& Out)
	{
		const FString MainSlot = UImmortalPathSaveGame::GetSlotName();
		if (!UGameplayStatics::LoadDataFromSlot(Out.Main, MainSlot, 0))
		{
			return false;
		}
		const FString BackupSlot = MainSlot + TEXT("_Backup");
		Out.bHasBackup = UGameplayStatics::DoesSaveGameExist(BackupSlot, 0);
		return !Out.bHasBackup
			|| UGameplayStatics::LoadDataFromSlot(Out.Backup, BackupSlot, 0);
	}

	bool IsExpectedUserDir(const FString& CaseName)
	{
		FString UserDir;
		if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir))
		{
			return false;
		}
		FString Actual = FPaths::ConvertRelativePathToFull(UserDir);
		FString Expected = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectDir(), TEXT("Saved/Automation/ExitR04"), CaseName));
		FPaths::NormalizeDirectoryName(Actual);
		FPaths::NormalizeDirectoryName(Expected);
		return Actual.Equals(Expected, ESearchCase::IgnoreCase)
			|| FPaths::IsUnderDirectory(Actual, Expected);
	}

#if PLATFORM_WINDOWS
	HWND GetGameWindowHandle()
	{
		if (!GEngine || !GEngine->GameViewport) return nullptr;
		const TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
		if (!Window.IsValid() || !Window->GetNativeWindow().IsValid())
		{
			return nullptr;
		}
		return static_cast<HWND>(Window->GetNativeWindow()->GetOSWindowHandle());
	}
#endif
}

void ScheduleImmortalExitRuntimeFixture(AImmortalPlayerCharacter* Player)
{
	FString CaseName;
	if (!FParse::Value(FCommandLine::Get(), TEXT("ImmortalTestExitCase="), CaseName)
		|| bExitFixtureClaimed)
	{
		return;
	}
	const bool bMenu = CaseName == TEXT("menu-success")
		|| CaseName == TEXT("menu-fail");
	const bool bClose = CaseName == TEXT("close-success")
		|| CaseName == TEXT("close-fail");
	const bool bAltF4 = CaseName == TEXT("altf4-success")
		|| CaseName == TEXT("altf4-fail");
	const bool bFail = CaseName.EndsWith(TEXT("-fail"));
	const bool bKnownCase = bMenu || bClose || bAltF4;
	if (!bKnownCase || !IsExpectedUserDir(CaseName))
	{
		// Do not start or terminate a real game when the test's save root is
		// absent or outside the isolated fixture directory.
		UE_LOG(LogTemp, Error,
			TEXT("ExitR04Fixture refused: requires a known case and isolated -UserDir under Saved/Automation/ExitR04/<case>"));
		return;
	}
	bExitFixtureClaimed = true;
#if !PLATFORM_WINDOWS
	FailExitFixture(TEXT("native exit fixture requires Windows"));
#else
	if (!Player || !Player->GetWorld()
		|| Player->GetWorld()->WorldType != EWorldType::Game)
	{
		FailExitFixture(TEXT("requires a standalone game world"));
		return;
	}
	TWeakObjectPtr<AImmortalPlayerCharacter> WeakPlayer(Player);
	FTimerHandle StartTimer;
	Player->GetWorld()->GetTimerManager().SetTimer(
		StartTimer,
		FTimerDelegate::CreateWeakLambda(Player, [WeakPlayer, CaseName, bMenu, bClose, bFail]()
		{
			AImmortalPlayerCharacter* RuntimePlayer = WeakPlayer.Get();
			if (!RuntimePlayer || !RuntimePlayer->GetWorld())
			{
				FailExitFixture(TEXT("player disappeared before test"));
				return;
			}
			if (!RuntimePlayer->SaveProgress())
			{
				FailExitFixture(TEXT("baseline save failed"));
				return;
			}
			FExitSaveBytes Before;
			if (!CaptureSaveBytes(Before))
			{
				FailExitFixture(TEXT("baseline slot unreadable"));
				return;
			}
			const HWND Window = GetGameWindowHandle();
			if (!Window || !IsWindow(Window) || !IsWindowVisible(Window))
			{
				FailExitFixture(TEXT("game window not visible at test start"));
				return;
			}
			if (bFail)
			{
				UImmortalPathSaveGame::SetDevelopmentWriteFailureOnAttempt(1);
			}
			else
			{
				UImmortalPathSaveGame::ClearDevelopmentWriteFailureOnAttempt();
			}
			UE_LOG(LogTemp, Display,
				TEXT("ExitR04Fixture initiated: case=%s mainBytes=%d backupBytes=%d"),
				*CaseName, Before.Main.Num(), Before.Backup.Num());
			if (bMenu)
			{
				const bool bQuitRequested = RuntimePlayer->SaveAndQuitDesktop();
				if (bQuitRequested == bFail)
				{
					FailExitFixture(TEXT("menu save-and-quit returned the wrong result"));
					return;
				}
			}
			else if (!PostMessageW(Window,
				bClose ? WM_CLOSE : WM_SYSCOMMAND,
				bClose ? 0 : SC_CLOSE, 0))
			{
				FailExitFixture(TEXT("native close message could not be queued"));
				return;
			}
			if (!bFail) return; // The caller checks process exit and committed save.
			FString UserDir;
			if (FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir))
			{
				// Source render verifies the prompt layout, while the separate
				// native composition audit checks the transparent HWND state.
				FTimerHandle ScreenshotTimer;
				RuntimePlayer->GetWorld()->GetTimerManager().SetTimer(
					ScreenshotTimer,
					FTimerDelegate::CreateWeakLambda(RuntimePlayer, [UserDir]
					{
						FScreenshotRequest::RequestScreenshot(
							FPaths::Combine(UserDir, TEXT("ExitSavePrompt.png")), true, false);
					}), 0.35f, false);
			}

			FTimerHandle VerifyTimer;
			RuntimePlayer->GetWorld()->GetTimerManager().SetTimer(
				VerifyTimer,
				FTimerDelegate::CreateWeakLambda(RuntimePlayer,
					[WeakPlayer, Before = MoveTemp(Before), Window]()
					{
						AImmortalPlayerCharacter* CheckPlayer = WeakPlayer.Get();
						FExitSaveBytes After;
						if (!CheckPlayer || !IsWindow(Window) || !IsWindowVisible(Window))
						{
							FailExitFixture(TEXT("failed save closed or hid the game window"));
							return;
						}
						if (UImmortalPathSaveGame::GetDevelopmentWriteAttemptCount() != 1
							|| !CaptureSaveBytes(After)
							|| Before.bHasBackup != After.bHasBackup
							|| Before.Main != After.Main
							|| Before.Backup != After.Backup)
						{
							FailExitFixture(TEXT("failed exit did not preserve original save bytes after one attempted write"));
							return;
						}
						UE_LOG(LogTemp, Display,
							TEXT("ExitR04Fixture initial failure verified: window retained, bytes unchanged"));
						// Cultivation keeps running behind the prompt and may later
						// autosave. Assert the failed write immediately, then leave
						// time to capture its UI before a clean retry.
						FTimerHandle RetryTimer;
						CheckPlayer->GetWorld()->GetTimerManager().SetTimer(
							RetryTimer,
							FTimerDelegate::CreateWeakLambda(CheckPlayer,
								[WeakPlayer, Window]()
								{
									AImmortalPlayerCharacter* RetryPlayer = WeakPlayer.Get();
									if (!RetryPlayer || !IsWindow(Window) || !IsWindowVisible(Window))
									{
										FailExitFixture(TEXT("failed-save prompt was hidden before retry"));
										return;
									}
									UImmortalPathSaveGame::ClearDevelopmentWriteFailureOnAttempt();
									if (!RetryPlayer->SaveAndQuitDesktop())
									{
										FailExitFixture(TEXT("clean retry failed after disarming injection"));
										return;
									}
									UE_LOG(LogTemp, Display,
										TEXT("ExitR04Fixture RESULT: PASS | failed write protected, prompt retained, clean retry requested exit"));
								}), 2.1f, false);
					}), 0.2f, false);
		}), 2.5f, false);
#endif
}

#endif // !UE_BUILD_SHIPPING
