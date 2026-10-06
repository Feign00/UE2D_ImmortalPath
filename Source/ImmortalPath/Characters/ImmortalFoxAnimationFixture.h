#pragma once

class AImmortalPlayerCharacter;

/** Isolated, non-shipping real-actor regressions. Never active in normal play. */
namespace ImmortalFoxAnimationFixture
{
	/** Must run before player BeginPlay initialization or any save access. */
	bool CheckPreflightIfRequested();
	void StartIfRequested(AImmortalPlayerCharacter& Player);
}
