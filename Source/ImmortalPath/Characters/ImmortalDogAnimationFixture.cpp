#include "ImmortalDogAnimationFixture.h"
#include "ImmortalBattleAnimationFixture.h"

namespace
{
	const ImmortalBattleAnimationFixture::FConfig Config
	{
		TEXT("Dog"),
		TEXT("ImmortalTestDogArt"),
		TEXT("ImmortalTestDogWalking"),
		TEXT("Saved/Automation/DogArt"),
		TEXT("/Game/GAME/BP_DesktopDogMonster.BP_DesktopDogMonster_C"),
		TEXT("SpiritHound"),
		TEXT("/Game/GAME/Asset/Monsters/desktop_pixel_v2/dog"),
		TEXT("/Game/GAME/Asset/Monsters/desktop_pixel_v2/dog/FB_Dog_Pixel_"),
		TEXT("R07Dog"),
		TEXT("R07_Dog"),
		false,
		0.5f
	};
}

bool ImmortalDogAnimationFixture::CheckPreflightIfRequested()
{
	return ImmortalBattleAnimationFixture::CheckPreflightIfRequested(Config);
}

void ImmortalDogAnimationFixture::StartIfRequested(AImmortalPlayerCharacter& Player)
{
	ImmortalBattleAnimationFixture::StartIfRequested(Player, Config);
}
