#include "ImmortalDesktopLocalPlayer.h"
#include "ImmortalDesktopWindow.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "PaperTileMapActor.h"
#include "SceneView.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "../UI/ImmortalDesktopPanelLayout.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"

bool UImmortalDesktopLocalPlayer::GetProjectionData(FViewport* Viewport,
	FSceneViewProjectionData& ProjectionData, int32 StereoViewIndex) const
{
	if (!Super::GetProjectionData(Viewport, ProjectionData, StereoViewIndex)) return false;
	const auto* Player = PlayerController ? Cast<AImmortalPlayerCharacter>(PlayerController->GetPawn()) : nullptr;
	if (Player && (Player->IsManagementInterfaceOpen() || Player->IsAscensionScreenOpen()))
	{
		const FIntPoint ViewportSize = Viewport->GetSizeXY();
		const int32 BattleHeight = FMath::Min(Player->GetDesktopCombatViewportHeight(), ViewportSize.Y);
		// Rebuild the original battle-strip projection before embedding it in the
		// taller surface. MaintainYFOV otherwise enlarges actors when menus open.
		FMinimalViewInfo ViewInfo; GetViewPoint(ViewInfo);
		FSceneViewProjectionData BattleData = ProjectionData;
		BattleData.SetViewRectangle(FIntRect(0, 0, ViewportSize.X, BattleHeight));
		FMinimalViewInfo::CalculateProjectionMatrixGivenView(ViewInfo, AspectRatioAxisConstraint, Viewport, BattleData);
		ProjectionData.ProjectionMatrix = BattleData.ProjectionMatrix;
		for (int32 Row = 0; Row < 4; ++Row)
			ProjectionData.ProjectionMatrix.M[Row][1] *= double(BattleHeight) / FMath::Max(ViewportSize.Y, 1);
		ImmortalDesktopPanelLayout::AnchorBattleProjection(ProjectionData.ProjectionMatrix,
			ViewportSize.Y, BattleHeight);
	}
	return true;
}

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
