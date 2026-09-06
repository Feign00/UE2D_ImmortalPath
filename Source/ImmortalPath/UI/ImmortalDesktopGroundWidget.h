#pragma once
#include "Blueprint/UserWidget.h"
#include "ImmortalDesktopGroundWidget.generated.h"

UCLASS()
class IMMORTALPATH_API UImmortalDesktopGroundWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeOnInitialized() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
		const FSlateRect& CullingRect, FSlateWindowElementList& Out, int32 Layer,
		const FWidgetStyle& Style, bool bEnabled) const override;
};
