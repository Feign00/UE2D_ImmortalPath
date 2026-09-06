#pragma once

#include "Engine/LocalPlayer.h"
#include "ImmortalDesktopLocalPlayer.generated.h"

/** Per-view scenery visibility; gameplay actors and map collision are untouched. */
UCLASS()
class IMMORTALPATH_API UImmortalDesktopLocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()
public:
	virtual FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily,
		FVector& OutViewLocation, FRotator& OutViewRotation, FViewport* Viewport,
		FViewElementDrawer* ViewDrawer = nullptr, int32 StereoViewIndex = INDEX_NONE) override;
};
