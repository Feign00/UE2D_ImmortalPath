#include "ImmortalDesktopGroundWidget.h"
#include "../Settings/ImmortalDesktopWindow.h"
#include "../Characters/ImmortalPlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "PaperFlipbookComponent.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"

void UImmortalDesktopGroundWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>();
}

int32 UImmortalDesktopGroundWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
	const FSlateRect& CullingRect, FSlateWindowElementList& Out, int32 Layer,
	const FWidgetStyle& Style, bool bEnabled) const
{
	if (!ImmortalDesktopWindow::IsTransparent(GetWorld())) return Layer;
	const FVector2D Size = Geometry.GetLocalSize();
	float Y = Size.Y - 18.0f;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (const AImmortalPlayerCharacter* Player = PC ? Cast<AImmortalPlayerCharacter>(PC->GetPawn()) : nullptr)
	{
		FVector2D Feet; int32 W=0,H=0; PC->GetViewportSize(W,H);
		if (H > 0 && PC->ProjectWorldLocationToScreen(Player->GetSprite()->GetComponentLocation(), Feet))
			Y = FMath::Clamp(Feet.Y / H * Size.Y, Size.Y*0.65f, Size.Y-8.0f);
	}
	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
	auto Box = [&](FVector2D P, FVector2D S, FLinearColor C)
	{
		FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(S,FSlateLayoutTransform(P)),
			Brush, ESlateDrawEffect::None, C);
	};
	Box(FVector2D(0,Y), FVector2D(Size.X,Size.Y-Y), FLinearColor(0.055f,0.06f,0.06f));
	Box(FVector2D(0,Y), FVector2D(Size.X,3), FLinearColor(0.40f,0.50f,0.22f));
	for (int32 X=0; X<Size.X; X+=24)
	{
		Box(FVector2D(X+2,Y+5), FVector2D(20,6), FLinearColor(0.17f,0.16f,0.13f));
		if ((X/24)%4==0) Box(FVector2D(X+8,Y-3), FVector2D(3,4),FLinearColor(0.47f,0.61f,0.27f));
	}
	return Layer+1;
}
