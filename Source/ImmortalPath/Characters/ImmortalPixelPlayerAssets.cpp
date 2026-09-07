#include "ImmortalPixelPlayerAssets.h"
#include "PaperFlipbook.h"

FImmortalPixelPlayerAssets::FImmortalPixelPlayerAssets()
{
	Idle = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(TEXT("/Game/GAME/Asset/Player/desktop_pixel_v2/FB_Player_Pixel_Idle.FB_Player_Pixel_Idle")));
	Move = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(TEXT("/Game/GAME/Asset/Player/desktop_pixel_v2/FB_Player_Pixel_Move.FB_Player_Pixel_Move")));
	Attack = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(TEXT("/Game/GAME/Asset/Player/desktop_pixel_v2/FB_Player_Pixel_Attack.FB_Player_Pixel_Attack")));
	Hurt = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(TEXT("/Game/GAME/Asset/Player/desktop_pixel_v2/FB_Player_Pixel_Hurt.FB_Player_Pixel_Hurt")));
	Death = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(TEXT("/Game/GAME/Asset/Player/desktop_pixel_v2/FB_Player_Pixel_Death.FB_Player_Pixel_Death")));
}

bool FImmortalPixelPlayerAssets::LoadComplete(TArray<UPaperFlipbook*>& Out) const
{
	Out = { Idle.LoadSynchronous(), Move.LoadSynchronous(), Attack.LoadSynchronous(), Hurt.LoadSynchronous(), Death.LoadSynchronous() };
	for (const UPaperFlipbook* Clip : Out)
	{
		if (!Clip || Clip->GetNumFrames() != 8 || Clip->GetTotalDuration() <= 0.0f)
		{
			Out.Reset();
			return false;
		}
	}
	return true;
}
