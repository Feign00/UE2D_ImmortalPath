#pragma once

#include "CoreMinimal.h"

// The original 2176x724 PNG contains 16 separated poses, not a uniform
// 17-column grid. Sample each original pose without rewriting the texture or
// stretching individual frames. These 128px windows include each full pose.
namespace ImmortalAscensionSequenceLayout
{
	inline constexpr int32 FrameLeftPixels[] = {
		7, 133, 266, 412, 545, 675, 806, 937,
		1071, 1204, 1341, 1479, 1614, 1747, 1882, 2022
	};
	inline constexpr int32 FrameCount = UE_ARRAY_COUNT(FrameLeftPixels);
	inline constexpr float FramesPerSecond = 12.0f;
	inline FBox2f GetFrameUV(int32 Frame)
	{
		const float Left = static_cast<float>(FrameLeftPixels[FMath::Clamp(Frame, 0, FrameCount - 1)]);
		return FBox2f(FVector2f(Left / 2176.0f, 190.0f / 724.0f),
			FVector2f((Left + 128.0f) / 2176.0f, 620.0f / 724.0f));
	}
}
