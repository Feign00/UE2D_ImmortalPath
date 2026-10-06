#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "ImmortalMonsterCharacter.h"
#include "../Pets/ImmortalPetTypes.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"
#include "UObject/UnrealType.h"

namespace
{
	FString FoxClipPath(const TCHAR* Action)
	{
		const FString Name = FString::Printf(TEXT("FB_Fox_Pixel_%s"), Action);
		return FString::Printf(TEXT("/Game/GAME/Asset/Monsters/desktop_pixel_v2/fox/%s.%s"), *Name, *Name);
	}

	UPaperFlipbook* BoundClip(const UObject* Object, const FName PropertyName)
	{
		const FObjectProperty* Property = FindFProperty<FObjectProperty>(Object->GetClass(), PropertyName);
		return Property ? Cast<UPaperFlipbook>(Property->GetObjectPropertyValue_InContainer(Object)) : nullptr;
	}

	float BoundFloat(const UObject* Object, const FName PropertyName)
	{
		const FFloatProperty* Property = FindFProperty<FFloatProperty>(Object->GetClass(), PropertyName);
		return Property ? Property->GetPropertyValue_InContainer(Object) : -1000.0f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalDesktopPixelFoxAnimationSetTest,
	"ImmortalPath.Art.DesktopPixelFoxAnimationSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalDesktopPixelFoxAnimationSetTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Action : {TEXT("Idle"), TEXT("Move"), TEXT("Attack"), TEXT("Hurt"), TEXT("Death")})
	{
		UPaperFlipbook* Clip = LoadObject<UPaperFlipbook>(nullptr, *FoxClipPath(Action));
		if (!TestNotNull(FString::Printf(TEXT("Fox %s loads"), Action), Clip)) continue;
		const FString ActionName(Action);
		const float ExpectedFPS = ActionName == TEXT("Death") ? 10.0f
			: (ActionName == TEXT("Attack") || ActionName == TEXT("Hurt")) ? 12.0f : 8.0f;
		TestEqual(FString::Printf(TEXT("%s has eight authored poses"), Action), Clip->GetNumKeyFrames(), 8);
		TestEqual(FString::Printf(TEXT("%s authored FPS"), Action), Clip->GetFramesPerSecond(), ExpectedFPS);
		TSet<const UPaperSprite*> UniqueSprites;
		TSet<FVector2D> UniqueRegions;
		for (int32 Frame = 0; Frame < Clip->GetNumKeyFrames(); ++Frame)
		{
			const FPaperFlipbookKeyFrame& Key = Clip->GetKeyFrameChecked(Frame);
			const UPaperSprite* Sprite = Key.Sprite;
			if (!TestNotNull(FString::Printf(TEXT("%s frame %d has a sprite"), Action, Frame), Sprite)) continue;
			UniqueSprites.Add(Sprite);
			UniqueRegions.Add(Sprite->GetSourceUV());
			TestEqual(TEXT("No duplicated frame runs"), Key.FrameRun, 1);
			TestTrue(TEXT("Uniform 512x384 cell"), Sprite->GetSourceSize().Equals(FVector2D(512, 384), 0.01));
			TestTrue(TEXT("Exact 4x2 chronological atlas region"), Sprite->GetSourceUV().Equals(
				FVector2D((Frame % 4) * 512, (Frame / 4) * 384), 0.01));
			TestTrue(TEXT("Atlas-local grounded foot pivot"),
				(Sprite->GetPivotPosition() - Sprite->GetSourceUV()).Equals(FVector2D(256, 336), 0.01));
			TestTrue(TEXT("Shared player/fox world density"), FMath::IsNearlyEqual(Sprite->GetPixelsPerUnrealUnit(), 2.56f));
			if (TestNotNull(TEXT("Atlas source texture present"), Sprite->GetSourceTexture()))
			{
				// These are editor metadata tests, including NullRHI runs.
				// Render-resource dimensions may be zero before platform data
				// is built; the authored import dimensions remain authoritative.
				const FIntPoint ImportedSize = Sprite->GetSourceTexture()->GetImportedSize();
				TestEqual(TEXT("Authored atlas is exactly four cells wide"), ImportedSize.X, 2048);
				TestEqual(TEXT("Authored atlas is exactly two cells tall"), ImportedSize.Y, 768);
			}
			if (TestNotNull(TEXT("Masked material present"), Sprite->GetDefaultMaterial()))
				TestEqual(TEXT("Hard transparent edges"), Sprite->GetDefaultMaterial()->GetBlendMode(), BLEND_Masked);
		}
		TestEqual(TEXT("Eight separate sprite objects"), UniqueSprites.Num(), 8);
		TestEqual(TEXT("Eight separate atlas regions"), UniqueRegions.Num(), 8);
	}
	UPaperFlipbook* Ascension = LoadObject<UPaperFlipbook>(nullptr,
		TEXT("/Game/GAME/Asset/Player/ascension/generated/FB_Player_Ascension.FB_Player_Ascension"));
	if (TestNotNull(TEXT("Original ascension preserved"), Ascension))
		TestEqual(TEXT("Original ascension remains seventeen frames"), Ascension->GetNumKeyFrames(), 17);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalDesktopPixelFoxBindingsTest,
	"ImmortalPath.Art.DesktopPixelFoxBindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalDesktopPixelFoxBindingsTest::RunTest(const FString& Parameters)
{
	UClass* FoxClass = LoadClass<AImmortalMonsterCharacter>(nullptr, TEXT("/Game/GAME/BP_Monster.BP_Monster_C"));
	if (TestNotNull(TEXT("Real map fox Blueprint loads"), FoxClass))
	{
		const AImmortalMonsterCharacter* Fox = FoxClass->GetDefaultObject<AImmortalMonsterCharacter>();
		for (const TCHAR* Action : {TEXT("Idle"), TEXT("Move"), TEXT("Attack"), TEXT("Hurt"), TEXT("Death")})
		{
			const UPaperFlipbook* Clip = BoundClip(Fox, FName(*FString::Printf(TEXT("%sFlipbook"), Action)));
			if (TestNotNull(FString::Printf(TEXT("BP binds %s"), Action), Clip))
				TestEqual(TEXT("BP points at the new authored set"), Clip->GetPathName(), FoxClipPath(Action));
		}
		TestTrue(TEXT("Fox scene scale matches the reviewed player presentation"), FMath::IsNearlyEqual(BoundFloat(Fox, TEXT("VisualScale")), 2.0f));
		const FBoolProperty* AlignToPlayerLane = FindFProperty<FBoolProperty>(Fox->GetClass(), TEXT("bAlignSpriteToPlayerLane"));
		TestTrue(TEXT("Fox uses the actual player sprite foot plane and depth"),
			AlignToPlayerLane && AlignToPlayerLane->GetPropertyValue_InContainer(Fox));
		TestTrue(TEXT("Attack contact is authored frame three"), FMath::IsNearlyEqual(BoundFloat(Fox, TEXT("AttackWindup")), 0.25f));
		TestTrue(TEXT("Hurt duration matches the complete clip"), FMath::IsNearlyEqual(BoundFloat(Fox, TEXT("HurtAnimationDuration")), 8.0f / 12.0f));
		TestTrue(TEXT("Health bar baseline fits the reviewed two-times standing art"),
			FMath::IsNearlyEqual(BoundFloat(Fox, TEXT("HealthBarHeight")),
				185.0f - Fox->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), 0.1f));
	}
	UClass* DogClass = LoadClass<AImmortalMonsterCharacter>(nullptr, TEXT("/Game/GAME/BP_DogMonster.BP_DogMonster_C"));
	if (TestNotNull(TEXT("Dog Blueprint remains available"), DogClass))
	{
		const UObject* Dog = DogClass->GetDefaultObject();
		for (const TCHAR* Action : {TEXT("Move"), TEXT("Attack"), TEXT("Hurt"), TEXT("Death")})
		{
			const UPaperFlipbook* Clip = BoundClip(Dog, FName(*FString::Printf(TEXT("%sFlipbook"), Action)));
			if (TestNotNull(TEXT("Legacy dog action remains present"), Clip))
				TestFalse(TEXT("Fox batch cannot silently replace dog/Boss art"), Clip->GetPathName().Contains(TEXT("/desktop_pixel_v2/fox/")));
		}
	}
	FImmortalPetDefinition FoxPet;
	if (TestTrue(TEXT("SpiritFox catalog exists"), UImmortalPetLibrary::GetPetDefinition(TEXT("SpiritFox"), FoxPet)))
	{
		TestEqual(TEXT("Pet Idle binds fox Idle"), FoxPet.IdleFlipbook.ToSoftObjectPath().ToString(), FoxClipPath(TEXT("Idle")));
		TestEqual(TEXT("Pet Move binds fox Move"), FoxPet.MoveFlipbook.ToSoftObjectPath().ToString(), FoxClipPath(TEXT("Move")));
		TestEqual(TEXT("Pet Attack binds fox Attack"), FoxPet.AttackFlipbook.ToSoftObjectPath().ToString(), FoxClipPath(TEXT("Attack")));
		TestEqual(TEXT("Pet Hurt binds fox Hurt"), FoxPet.HurtFlipbook.ToSoftObjectPath().ToString(), FoxClipPath(TEXT("Hurt")));
		TestEqual(TEXT("Pet Death binds fox Death"), FoxPet.DeathFlipbook.ToSoftObjectPath().ToString(), FoxClipPath(TEXT("Death")));
		TestTrue(TEXT("Pet distinct readable scale"), FMath::IsNearlyEqual(FoxPet.VisualScale, 1.25f));
		TestTrue(TEXT("Pet preserves authored palette"), FoxPet.DisplayColor.Equals(FLinearColor::White));
		TestTrue(TEXT("Pet foot pivot uses the actual player visual lane"), FoxPet.FootAnchorMode == EImmortalPetFootAnchorMode::PlayerBattleLane);
		TestTrue(TEXT("Pet contact matches frame three"), FMath::IsNearlyEqual(FoxPet.AttackContactSeconds, 0.25f));
	}
	FImmortalPetDefinition DogPet;
	if (TestTrue(TEXT("SpiritHound remains available"), UImmortalPetLibrary::GetPetDefinition(TEXT("SpiritHound"), DogPet)))
	{
		TestTrue(TEXT("Dog remains on the original dog Move"), DogPet.MoveFlipbook.ToSoftObjectPath().ToString().Contains(TEXT("dog_move_left")));
		TestTrue(TEXT("Dog keeps legacy anchoring"), DogPet.FootAnchorMode == EImmortalPetFootAnchorMode::LegacyOffset);
		TestTrue(TEXT("Dog keeps legacy contact timing"), DogPet.AttackContactSeconds < 0.0f);
	}
	return true;
}

#endif
