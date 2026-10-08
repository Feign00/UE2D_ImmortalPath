#include "ImmortalFoxAnimationFixture.h"
#include "ImmortalBattleAnimationFixture.h"

namespace
{
	const ImmortalBattleAnimationFixture::FConfig Config
	{
		TEXT("Fox"),
		TEXT("ImmortalTestFoxArt"),
		TEXT("ImmortalTestFoxWalking"),
		TEXT("Saved/Automation/FoxArt"),
		TEXT("/Game/GAME/BP_Monster.BP_Monster_C"),
		TEXT("SpiritFox"),
		TEXT("/Game/GAME/Asset/Monsters/desktop_pixel_v2/fox"),
		TEXT("/Game/GAME/Asset/Monsters/desktop_pixel_v2/fox/FB_Fox_Pixel_"),
		TEXT("R07Fox"),
		TEXT("R07_Fox"),
		true,
		0.0f
	};
}

bool ImmortalFoxAnimationFixture::CheckPreflightIfRequested()
{
	return ImmortalBattleAnimationFixture::CheckPreflightIfRequested(Config);
}

void ImmortalFoxAnimationFixture::StartIfRequested(AImmortalPlayerCharacter& Player)
{
	ImmortalBattleAnimationFixture::StartIfRequested(Player, Config);
}
