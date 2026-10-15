#pragma once

class AImmortalPlayerCharacter;

/** Non-shipping, isolated real Qingyun map-Boss checks. Not a normal-map observer. */
namespace ImmortalQingyunBossAnimationFixture
{
	/** Must run before player initialization or save access. */
	bool CheckPreflightIfRequested();
	void StartIfRequested(AImmortalPlayerCharacter& Player);
}
