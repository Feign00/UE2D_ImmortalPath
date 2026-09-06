#pragma once

#include "Blueprint/UserWidget.h"
#include "ImmortalIconWidget.generated.h"

/** Original code-native pixel symbols: no missing-font glyphs or external texture dependency. */
UCLASS()
class IMMORTALPATH_API UImmortalIconWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetIcon(int32 InIndex) { IconIndex = FMath::Clamp(InIndex, 0, 19); }
protected:
	virtual void NativeOnInitialized() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
		const FSlateRect& CullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override;
private:
	int32 IconIndex = 0;
};
