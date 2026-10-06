#pragma once

class APaperCharacter;

/** Placement for authored foot-pivot sprites; never changes gameplay collision. */
namespace ImmortalBattleSpritePlacement
{
	float GetReferenceGroundOffset(const APaperCharacter& Reference);
	bool AlignToReferenceFootPlane(APaperCharacter& Actor, const APaperCharacter& Reference);
}
