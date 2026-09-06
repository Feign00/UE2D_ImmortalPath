#include "ImmortalDesktopLocalPlayer.h"
#include "ImmortalDesktopWindow.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "PaperTileMapActor.h"
#include "SceneView.h"

FSceneView* UImmortalDesktopLocalPlayer::CalcSceneView(FSceneViewFamily* ViewFamily,
	FVector& OutViewLocation, FRotator& OutViewRotation, FViewport* Viewport,
	FViewElementDrawer* ViewDrawer, int32 StereoViewIndex)
{
	const bool bTransparent = ImmortalDesktopWindow::IsTransparent(GetWorld());
	if (bTransparent && ViewFamily)
	{
		// Exact key output: tonemapping and FXAA would contaminate sprite edges.
		ViewFamily->EngineShowFlags.SetPostProcessing(false);
		ViewFamily->EngineShowFlags.SetAntiAliasing(false);
		ViewFamily->EngineShowFlags.SetTonemapper(false);
		ViewFamily->EngineShowFlags.SetFog(false);
		ViewFamily->EngineShowFlags.SetAtmosphere(false);
		ViewFamily->EngineShowFlags.SetSkyLighting(false);
	}
	FSceneView* View = Super::CalcSceneView(ViewFamily, OutViewLocation, OutViewRotation,
		Viewport, ViewDrawer, StereoViewIndex);
	if (!View || !bTransparent) return View;
	View->BackgroundColor = FLinearColor(1.0f, 0.0f, 1.0f, 0.0f);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const bool bSceneryActor = It->IsA<APaperTileMapActor>()
			|| It->ActorHasTag(TEXT("DesktopBackground"));
		TInlineComponentArray<UPrimitiveComponent*> Components(*It);
		for (UPrimitiveComponent* Component : Components)
		{
			if (bSceneryActor || Component->GetFName() == TEXT("QingyunMountainBackground"))
				View->HiddenPrimitives.Add(Component->GetPrimitiveSceneId());
		}
	}
	return View;
}
