#pragma once

class AImmortalPlayerCharacter;

namespace ImmortalPixelAnimationPreview
{
	// Opt-in, isolated development fixture. Never replaces the default animation set.
	void StartIfRequested(AImmortalPlayerCharacter& Player);
}
