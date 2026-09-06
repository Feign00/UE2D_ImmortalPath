#pragma once

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"

namespace ImmortalFeaturePageLayout
{
	// Preserve every line of variable-length stats without covering nearby actions.
	inline void MakeScrollable(UWidgetTree* Tree, UTextBlock* Text)
	{
		UCanvasPanelSlot* OriginalSlot = Text ? Cast<UCanvasPanelSlot>(Text->Slot) : nullptr;
		UCanvasPanel* Canvas = Text ? Cast<UCanvasPanel>(Text->GetParent()) : nullptr;
		if (!OriginalSlot || !Canvas) return;
		const FAnchorData Layout = OriginalSlot->GetLayout();
		Text->RemoveFromParent();
		UScrollBox* Scroll = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Canvas->AddChildToCanvas(Scroll)->SetLayout(Layout);
		Text->SetAutoWrapText(true);
		Scroll->AddChild(Text);
	}

	// Detailed pages need a stable contrast independent of the scene artwork.
	inline void AddReadabilityBackground(UWidgetTree* Tree, UCanvasPanel* Canvas)
	{
		UBorder* Backdrop = Tree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("FeatureReadabilityBackground"));
		Backdrop->SetBrushColor(FLinearColor(0.012f, 0.021f, 0.028f, 0.91f));
		Backdrop->SetPadding(FMargin(0.0f));
		Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Backdrop);
		Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		Slot->SetOffsets(FMargin(0.0f));
	}
}
