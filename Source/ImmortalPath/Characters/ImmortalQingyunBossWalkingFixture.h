#pragma once

class AImmortalPlayerCharacter;

/** Read-only, non-shipping observation of a naturally spawned Qingyun map Boss. */
namespace ImmortalQingyunBossWalkingFixture
{
	/** Must run before player initialization and any save access. */
	bool CheckPreflightIfRequested();
	void StartIfRequested(AImmortalPlayerCharacter& Player);
}
