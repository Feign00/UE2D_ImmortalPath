#pragma once

class AImmortalPlayerCharacter;

/** Isolated, non-shipping real-actor dog/hound regressions. Never active in normal play. */
namespace ImmortalDogAnimationFixture
{
	/** Must run before player BeginPlay initialization or any save access. */
	bool CheckPreflightIfRequested();
	void StartIfRequested(AImmortalPlayerCharacter& Player);
}
