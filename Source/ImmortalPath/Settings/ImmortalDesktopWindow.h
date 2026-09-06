#pragma once

#include "CoreMinimal.h"

class UWorld;

/** Native composition only; never changes collision or simulation state. */
namespace ImmortalDesktopWindow
{
	bool ApplyTransparency(UWorld* World, bool bEnabled);
	bool IsTransparent(const UWorld* World);
	/** Hide the last color-key frame before native composition is dismantled. */
	void PrepareForExit(UWorld* World);
	void Restore(UWorld* World);
}
