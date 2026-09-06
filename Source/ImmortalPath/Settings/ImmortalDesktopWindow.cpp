#include "ImmortalDesktopWindow.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GenericPlatform/GenericWindow.h"
#include "Widgets/SWindow.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
	TWeakObjectPtr<UWorld> TransparentWorld;
	FTimerHandle PointerTimer;
#if PLATFORM_WINDOWS
	HWND OwnedWindow = nullptr;
	LONG_PTR OriginalExStyle = 0;
	bool bOwnsLayeredStyle = false;

	void AuditPointerMode()
	{
		if (!OwnedWindow || !IsWindow(OwnedWindow)) return;
		POINT Cursor; RECT Client;
		if (!GetCursorPos(&Cursor) || !ScreenToClient(OwnedWindow, &Cursor)
			|| !GetClientRect(OwnedWindow, &Client) || Client.right <= 0 || Client.bottom <= 0) return;
		FViewport* Viewport = GEngine->GameViewport->Viewport;
		if (!Viewport) return;
		const FIntPoint Size = Viewport->GetSizeXY();
		const FVector2D Position(Cursor.x * static_cast<double>(Size.X) / Client.right,
			Cursor.y * static_cast<double>(Size.Y) / Client.bottom);
#if !UE_BUILD_SHIPPING
		if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestDesktopPointerLog")) && GFrameCounter % 120 == 0)
		{
			POINT ScreenPoint = Cursor; ClientToScreen(OwnedWindow, &ScreenPoint);
			UE_LOG(LogTemp, Display, TEXT("Desktop pointer audit: client=%ld,%ld clientSize=%ld,%ld viewport=%d,%d mapped=%.1f,%.1f nativeHitGame=%s nativePassStyle=%s"),
				Cursor.x, Cursor.y, Client.right, Client.bottom, Size.X, Size.Y, Position.X, Position.Y,
				WindowFromPoint(ScreenPoint) == OwnedWindow ? TEXT("true") : TEXT("false"),
				(GetWindowLongPtr(OwnedWindow,GWL_EXSTYLE) & WS_EX_TRANSPARENT) ? TEXT("true") : TEXT("false"));
		}
#endif
	}
#endif
}

bool ImmortalDesktopWindow::ApplyTransparency(UWorld* World, const bool bEnabled)
{
#if PLATFORM_WINDOWS
	// Never change the editor's native window, including new-editor-window PIE.
	if (!World || World->WorldType != EWorldType::Game
		|| !GEngine || !GEngine->GameViewport) return false;
	const TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
	if (!Window.IsValid() || !Window->GetNativeWindow().IsValid()) return false;
	const HWND Handle = static_cast<HWND>(Window->GetNativeWindow()->GetOSWindowHandle());
	if (!Handle || !IsWindow(Handle)) return false;
	if (!bEnabled)
	{
		Restore(World);
		return true;
	}
	if (OwnedWindow != Handle)
	{
		OriginalExStyle = GetWindowLongPtr(Handle, GWL_EXSTYLE);
		bOwnsLayeredStyle = (OriginalExStyle & WS_EX_LAYERED) == 0;
		OwnedWindow = Handle;
	}
	SetLastError(0);
	const LONG_PTR Previous = SetWindowLongPtr(Handle, GWL_EXSTYLE,
		(GetWindowLongPtr(Handle, GWL_EXSTYLE) | WS_EX_LAYERED) & ~WS_EX_TRANSPARENT);
	if (Previous == 0 && GetLastError() != 0) return false;
	// A reserved chroma key, not black: dark sprite outlines stay opaque.
	// Blit-model layered windows provide native per-pixel hit testing. Do not use
	// whole-window WS_EX_TRANSPARENT, which races mouse movement over HUD buttons.
	if (!SetLayeredWindowAttributes(Handle, RGB(255, 0, 255), 255, LWA_COLORKEY))
	{
		Restore(World);
		UE_LOG(LogTemp, Warning, TEXT("Desktop transparency unavailable; keeping opaque mode."));
		return false;
	}
	TransparentWorld = World;
	UE_LOG(LogTemp, Display, TEXT("Desktop transparency enabled: key=255,0,255; foreground alpha=255; native color-key hit testing."));
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestDesktopPointerLog")))
		World->GetTimerManager().SetTimer(PointerTimer, FTimerDelegate::CreateWeakLambda(World,
			[] { AuditPointerMode(); }), 1.0f/60.0f, true);
	if (FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestDesktopTransparency")))
	{
		FTimerHandle AuditTimer;
		World->GetTimerManager().SetTimer(AuditTimer, FTimerDelegate::CreateWeakLambda(World, [World, Handle]
		{
			RECT Client = {}; GetClientRect(Handle, &Client);
			POINT Point = {Client.right-16, 16}; ClientToScreen(Handle, &Point);
			HDC Screen = GetDC(nullptr);
			const COLORREF ScreenColor = Screen ? GetPixel(Screen, Point.x, Point.y) : CLR_INVALID;
			if (Screen) ReleaseDC(nullptr, Screen);
			const bool bDesktopVisible = ScreenColor != CLR_INVALID && ScreenColor != RGB(255,0,255);
			COLORREF Key = 0; BYTE Alpha = 0; DWORD Flags = 0;
			const bool bNativeKey = GetLayeredWindowAttributes(Handle, &Key, &Alpha, &Flags)
				&& Key == RGB(255,0,255) && (Flags & LWA_COLORKEY);
			const TSharedRef<FDelegateHandle> CaptureHandle = MakeShared<FDelegateHandle>();
			*CaptureHandle = UGameViewportClient::OnScreenshotCaptured().AddWeakLambda(World,
				[World, Handle, CaptureHandle, bNativeKey, bDesktopVisible](int32 Width, int32 Height, const TArray<FColor>& Pixels)
				{
					UGameViewportClient::OnScreenshotCaptured().Remove(*CaptureHandle);
					const int32 Index = Height > 16 && Width > 16 ? 16*Width+Width-16 : INDEX_NONE;
					const bool bSourceKey = Pixels.IsValidIndex(Index) && Pixels[Index].R == 255
						&& Pixels[Index].G == 0 && Pixels[Index].B == 255;
					POINT Foreground = {50,50}; ClientToScreen(Handle, &Foreground);
					HDC Screen = GetDC(nullptr);
					const COLORREF ForegroundColor = Screen ? GetPixel(Screen, Foreground.x, Foreground.y) : CLR_INVALID;
					if (Screen) ReleaseDC(nullptr, Screen);
					const int32 ForegroundIndex = 50*Width+50;
					const bool bForegroundVisible = Pixels.IsValidIndex(ForegroundIndex)
						&& ForegroundColor == RGB(Pixels[ForegroundIndex].R, Pixels[ForegroundIndex].G, Pixels[ForegroundIndex].B);
					UE_LOG(LogTemp, Display, TEXT("Desktop composition audit: sourceKey=%s nativeKey=%s desktopVisible=%s foregroundVisible=%s"),
						bSourceKey ? TEXT("true") : TEXT("false"), bNativeKey ? TEXT("true") : TEXT("false"),
						bDesktopVisible ? TEXT("true") : TEXT("false"), bForegroundVisible ? TEXT("true") : TEXT("false"));
					TArray64<uint8> PNG;
					FImageUtils::PNGCompressImageArray(Width, Height, Pixels, PNG);
					FFileHelper::SaveArrayToFile(PNG, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DesktopTransparentRender.png")));
					FTimerHandle ExitTimer;
					if (!FParse::Param(FCommandLine::Get(), TEXT("ImmortalTestDesktopKeepRunning")))
						World->GetTimerManager().SetTimer(ExitTimer, [bSourceKey,bNativeKey,bDesktopVisible,bForegroundVisible]
						{ FPlatformMisc::RequestExitWithStatus(false, bSourceKey && bNativeKey && bDesktopVisible && bForegroundVisible ? 0 : 1); }, 1.0f, false);
				});
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),
				TEXT("Screenshots/DesktopTransparentRender.png")), true, false);
		}), 3.0f, false);
	}
#endif
	return true;
#else
	return false;
#endif
}

bool ImmortalDesktopWindow::IsTransparent(const UWorld* World)
{
	return World && TransparentWorld.IsValid() && TransparentWorld.Get() == World;
}

void ImmortalDesktopWindow::PrepareForExit(UWorld* World)
{
	// A null/editor/foreign world must never hide another application's window.
	if (!World || World->WorldType != EWorldType::Game || TransparentWorld.Get() != World) return;
#if PLATFORM_WINDOWS
	if (OwnedWindow && IsWindow(OwnedWindow))
	{
		// No more opaque frame will be presented during shutdown. Restoring styles
		// on a visible HWND would expose the retained magenta swap-chain image.
		ShowWindow(OwnedWindow, SW_HIDE);
		UE_LOG(LogTemp, Display, TEXT("Desktop exit: window hidden before color-key teardown; visible=%s"),
			IsWindowVisible(OwnedWindow) ? TEXT("true") : TEXT("false"));
	}
#endif
}

void ImmortalDesktopWindow::Restore(UWorld* World)
{
	if (TransparentWorld.IsValid() && TransparentWorld.Get() != World) return;
	if (World) World->GetTimerManager().ClearTimer(PointerTimer);
#if PLATFORM_WINDOWS
	if (OwnedWindow && IsWindow(OwnedWindow))
	{
		SetLayeredWindowAttributes(OwnedWindow, 0, 255, LWA_ALPHA);
		const LONG_PTR Style = GetWindowLongPtr(OwnedWindow, GWL_EXSTYLE);
		SetWindowLongPtr(OwnedWindow, GWL_EXSTYLE, (Style & ~WS_EX_TRANSPARENT) | (OriginalExStyle & WS_EX_TRANSPARENT));
		if (bOwnsLayeredStyle)
			SetWindowLongPtr(OwnedWindow, GWL_EXSTYLE,
				GetWindowLongPtr(OwnedWindow, GWL_EXSTYLE) & ~WS_EX_LAYERED);
	}
	OwnedWindow = nullptr;
	bOwnsLayeredStyle = false;
#endif
	TransparentWorld.Reset();
}
