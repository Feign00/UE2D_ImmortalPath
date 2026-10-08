#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "ImmortalMonsterCharacter.h"
#include "../Pets/ImmortalPetTypes.h"
#include "../Spawning/ImmortalMonsterSpawner.h"
#include "Components/ActorComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"
#include "UObject/UnrealType.h"

namespace
{
	FString AuthoredClipPath(const TCHAR* Family, const TCHAR* Action)
	{
		const FString Name = FString::Printf(TEXT("FB_%s_Pixel_%s"), Family, Action);
		return FString::Printf(TEXT("/Game/GAME/Asset/Monsters/desktop_pixel_v2/%s/%s.%s"),
			*FString(Family).ToLower(), *Name, *Name);
	}

	UObject* BoundObject(const UObject* Object, const FName PropertyName)
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName);
		return Property ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
	}

	float BoundFloat(const UObject* Object, const FName PropertyName)
	{
		const FFloatProperty* Property = FindFProperty<FFloatProperty>(Object->GetClass(), PropertyName);
		return Property ? Property->GetPropertyValue_InContainer(Object) : -1000.0f;
	}

	bool BoundBool(const UObject* Object, const FName PropertyName)
	{
		const FBoolProperty* Property = FindFProperty<FBoolProperty>(Object->GetClass(), PropertyName);
		return Property && Property->GetPropertyValue_InContainer(Object);
	}

	FString CanonicalDogIdentity(FString Value)
	{
		Value.ReplaceInline(TEXT("/Game/GAME/BP_DesktopDogMonster.BP_DesktopDogMonster"),
			TEXT("/Game/GAME/BP_DogMonster.BP_DogMonster"), ESearchCase::CaseSensitive);
		Value.ReplaceInline(TEXT("/Game/GAME/BP_DesktopDogMonster.Default__BP_DesktopDogMonster"),
			TEXT("/Game/GAME/BP_DogMonster.Default__BP_DogMonster"), ESearchCase::CaseSensitive);
		Value.ReplaceInline(TEXT("/Game/GAME/BP_DesktopDogMonster"),
			TEXT("/Game/GAME/BP_DogMonster"), ESearchCase::CaseSensitive);
		return Value;
	}

	TSet<FName> PresentationProperties()
	{
		return { TEXT("IdleFlipbook"), TEXT("MoveFlipbook"), TEXT("AttackFlipbook"),
			TEXT("HurtFlipbook"), TEXT("DeathFlipbook"), TEXT("bArtworkFacesRight"),
			TEXT("bAlignSpriteToPlayerLane"), TEXT("VisualScale"), TEXT("AttackWindup"),
			TEXT("HurtAnimationDuration"), TEXT("HealthBarHeight") };
	}

	TMap<FString, FString> EditableSnapshot(const UObject* Object, const TSet<FName>& Allowed)
	{
		TMap<FString, FString> Result;
		for (TFieldIterator<FProperty> It(Object->GetClass(), EFieldIterationFlags::IncludeSuper); It; ++It)
		{
			const FProperty* Property = *It;
			if (Property->HasAnyPropertyFlags(CPF_Edit)
				&& !Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)
				&& !Allowed.Contains(Property->GetFName()))
			{
				FString Value;
				Property->ExportText_InContainer(0, Value, Object, nullptr, const_cast<UObject*>(Object), PPF_None);
				Result.Add(Property->GetName(), CanonicalDogIdentity(MoveTemp(Value)));
			}
		}
		return Result;
	}

	TArray<UActorComponent*> ComponentTemplates(UBlueprint* Blueprint, const AImmortalMonsterCharacter* Defaults)
	{
		TArray<UActorComponent*> Result;
		Defaults->GetComponents(Result);
		if (Blueprint->SimpleConstructionScript)
		{
			for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
			{
				if (Node && Node->ComponentTemplate) { Result.AddUnique(Node->ComponentTemplate); }
			}
		}
		if (UInheritableComponentHandler* Handler = Blueprint->GetInheritableComponentHandler(false))
		{
			for (auto It = Handler->CreateRecordIterator(); It; ++It)
			{
				if (It->ComponentTemplate) { Result.AddUnique(It->ComponentTemplate); }
			}
		}
		return Result;
	}

	TMap<FString, FString> ComponentSnapshot(UBlueprint* Blueprint, const AImmortalMonsterCharacter* Defaults)
	{
		TMap<FString, FString> Result;
		TSet<UActorComponent*> SpriteTemplates { Defaults->GetSprite() };
		const FName SpriteName = Defaults->GetSprite()->GetFName();
		if (Blueprint->SimpleConstructionScript)
		{
			for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
			{
				if (Node && Node->GetVariableName() == SpriteName && Node->ComponentTemplate)
				{
					SpriteTemplates.Add(Node->ComponentTemplate);
				}
			}
		}
		if (UInheritableComponentHandler* Handler = Blueprint->GetInheritableComponentHandler(false))
		{
			for (auto It = Handler->CreateRecordIterator(); It; ++It)
			{
				if (It->ComponentKey.GetSCSVariableName() == SpriteName && It->ComponentTemplate)
				{
					SpriteTemplates.Add(It->ComponentTemplate);
				}
			}
		}
		for (UActorComponent* Component : ComponentTemplates(Blueprint, Defaults))
		{
			const bool bIsSprite = SpriteTemplates.Contains(Component)
				&& Component->IsA<UPaperFlipbookComponent>();
			TSet<FName> Allowed;
			if (bIsSprite)
			{
				Allowed = { TEXT("RelativeLocation"), TEXT("SourceFlipbook"),
					TEXT("OverrideMaterials"), TEXT("PlayRate") };
			}
			const FString Key = CanonicalDogIdentity(Component->GetPathName());
			Result.Add(Key + TEXT(".@Class"), Component->GetClass()->GetPathName());
			for (const auto& Entry : EditableSnapshot(Component, Allowed))
			{
				Result.Add(Key + TEXT(".") + Entry.Key, Entry.Value);
			}
			if (bIsSprite)
			{
				const UPaperFlipbookComponent* Sprite = CastChecked<UPaperFlipbookComponent>(Component);
				FVector ProtectedLocation = Sprite->GetRelativeLocation();
				ProtectedLocation.Z = 0.0;
				Result.Add(Key + TEXT(".@ProtectedLocationXY"), ProtectedLocation.ToString());
				Result.Add(Key + TEXT(".@ProtectedRotation"), Sprite->GetRelativeRotation().ToString());
				Result.Add(Key + TEXT(".@ProtectedScale"), Sprite->GetRelativeScale3D().ToString());
			}
		}
		return Result;
	}

	void CompareSnapshots(FAutomationTestBase& Test, const TMap<FString, FString>& Expected,
		const TMap<FString, FString>& Actual, const TCHAR* Label)
	{
		Test.TestEqual(FString::Printf(TEXT("%s retains the complete property set"), Label), Actual.Num(), Expected.Num());
		for (const auto& Entry : Expected)
		{
			const FString* Value = Actual.Find(Entry.Key);
			if (Test.TestNotNull(FString::Printf(TEXT("%s retains %s"), Label, *Entry.Key), Value))
			{
				Test.TestEqual(FString::Printf(TEXT("%s preserves %s"), Label, *Entry.Key), *Value, Entry.Value);
			}
		}
	}

	void TestPetArt(FAutomationTestBase& Test, const TCHAR* PetId, const TCHAR* Family)
	{
		FImmortalPetDefinition Pet;
		if (!Test.TestTrue(FString::Printf(TEXT("%s catalog entry exists"), PetId),
			UImmortalPetLibrary::GetPetDefinition(FName(PetId), Pet))) { return; }
		Test.TestEqual(TEXT("Pet Idle points at its own authored Idle"), Pet.IdleFlipbook.ToSoftObjectPath().ToString(), AuthoredClipPath(Family, TEXT("Idle")));
		Test.TestEqual(TEXT("Pet Move points at its own authored Move"), Pet.MoveFlipbook.ToSoftObjectPath().ToString(), AuthoredClipPath(Family, TEXT("Move")));
		Test.TestEqual(TEXT("Pet Attack points at its own authored Attack"), Pet.AttackFlipbook.ToSoftObjectPath().ToString(), AuthoredClipPath(Family, TEXT("Attack")));
		Test.TestEqual(TEXT("Pet Hurt points at its own authored Hurt"), Pet.HurtFlipbook.ToSoftObjectPath().ToString(), AuthoredClipPath(Family, TEXT("Hurt")));
		Test.TestEqual(TEXT("Pet Death points at its own authored Death"), Pet.DeathFlipbook.ToSoftObjectPath().ToString(), AuthoredClipPath(Family, TEXT("Death")));
		Test.TestTrue(TEXT("Readable companion scale"), FMath::IsNearlyEqual(Pet.VisualScale, 1.25f));
		Test.TestTrue(TEXT("No palette tint added"), Pet.DisplayColor.Equals(FLinearColor::White));
		Test.TestTrue(TEXT("Pet shares the actual player visual foot and depth lane"), Pet.FootAnchorMode == EImmortalPetFootAnchorMode::PlayerBattleLane);
		Test.TestTrue(TEXT("Pet authored contact time"), FMath::IsNearlyEqual(Pet.AttackContactSeconds, 0.25f));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalDesktopPixelDogAnimationSetTest,
	"ImmortalPath.Art.DesktopPixelDogAnimationSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalDesktopPixelDogAnimationSetTest::RunTest(const FString& Parameters)
{
	UMaterialInterface* MaskedMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Paper2D/MaskedUnlitSpriteMaterial.MaskedUnlitSpriteMaterial"));
	if (!TestNotNull(TEXT("Canonical masked material loads"), MaskedMaterial)) { return false; }
	TSet<const UPaperSprite*> AllSprites;
	TSet<const UTexture2D*> ActionTextures;
	for (const TCHAR* Action : { TEXT("Idle"), TEXT("Move"), TEXT("Attack"), TEXT("Hurt"), TEXT("Death") })
	{
		UPaperFlipbook* Clip = LoadObject<UPaperFlipbook>(nullptr, *AuthoredClipPath(TEXT("Dog"), Action));
		if (!TestNotNull(FString::Printf(TEXT("Dog %s loads"), Action), Clip)) { continue; }
		const FString ActionName(Action);
		const float ExpectedFPS = ActionName == TEXT("Death") ? 10.0f
			: (ActionName == TEXT("Attack") || ActionName == TEXT("Hurt")) ? 12.0f : 8.0f;
		TestEqual(TEXT("Eight authored key frames"), Clip->GetNumKeyFrames(), 8);
		TestEqual(TEXT("Exactly eight playback frames"), Clip->GetNumFrames(), 8);
		TestEqual(TEXT("Authored action cadence"), Clip->GetFramesPerSecond(), ExpectedFPS);
		TestTrue(TEXT("Flipbook uses the canonical hard-alpha material"), Clip->GetDefaultMaterial() == MaskedMaterial);
		TSet<FVector2D> Regions;
		const UTexture2D* ActionTexture = nullptr;
		for (int32 Index = 0; Index < Clip->GetNumKeyFrames(); ++Index)
		{
			const FPaperFlipbookKeyFrame& Key = Clip->GetKeyFrameChecked(Index);
			const UPaperSprite* Sprite = Key.Sprite;
			if (!TestNotNull(TEXT("Key frame has a sprite"), Sprite)) { continue; }
			TestFalse(TEXT("No action duplicates another sprite object"), AllSprites.Contains(Sprite));
			AllSprites.Add(Sprite);
			Regions.Add(Sprite->GetSourceUV());
			TestTrue(TEXT("Sprite belongs to the new dog-only family"),
				Sprite->GetPathName().StartsWith(TEXT("/Game/GAME/Asset/Monsters/desktop_pixel_v2/dog/")));
			TestEqual(TEXT("No repeated playback runs"), Key.FrameRun, 1);
			TestTrue(TEXT("Uniform frame cell"), Sprite->GetSourceSize().Equals(FVector2D(512, 384), 0.01));
			TestTrue(TEXT("Exact chronological 4x2 atlas region"), Sprite->GetSourceUV().Equals(
				FVector2D((Index % 4) * 512, (Index / 4) * 384), 0.01));
			TestTrue(TEXT("Ground-contact pivot"),
				(Sprite->GetPivotPosition() - Sprite->GetSourceUV()).Equals(FVector2D(256, 336), 0.01));
			TestTrue(TEXT("Shared world density"), FMath::IsNearlyEqual(Sprite->GetPixelsPerUnrealUnit(), 2.56f));
			TestTrue(TEXT("Sprite uses the canonical masked material"), Sprite->GetDefaultMaterial() == MaskedMaterial);
			const UTexture2D* Texture = Sprite->GetSourceTexture();
			if (TestNotNull(TEXT("Atlas source is present"), Texture))
			{
				TestTrue(TEXT("Authored imported dimensions also valid in NullRHI"), Texture->GetImportedSize() == FIntPoint(2048, 768));
				TestTrue(TEXT("Texture belongs to the new dog family"),
					Texture->GetPathName().StartsWith(TEXT("/Game/GAME/Asset/Monsters/desktop_pixel_v2/dog/")));
				TestTrue(TEXT("One consistent texture per action"), !ActionTexture || ActionTexture == Texture);
				ActionTexture = Texture;
			}
		}
		TestEqual(TEXT("Eight separate atlas regions"), Regions.Num(), 8);
		if (ActionTexture)
		{
			TestFalse(TEXT("Actions do not reuse an atlas"), ActionTextures.Contains(ActionTexture));
			ActionTextures.Add(ActionTexture);
		}
	}
	TestEqual(TEXT("Five separate action atlases"), ActionTextures.Num(), 5);
	TestEqual(TEXT("Forty separate authored sprite objects"), AllSprites.Num(), 40);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalDesktopPixelDogBindingsTest,
	"ImmortalPath.Art.DesktopPixelDogBindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalDesktopPixelDogBindingsTest::RunTest(const FString& Parameters)
{
	UClass* DogClass = LoadClass<AImmortalMonsterCharacter>(nullptr,
		TEXT("/Game/GAME/BP_DesktopDogMonster.BP_DesktopDogMonster_C"));
	if (TestNotNull(TEXT("Independent desktop dog Blueprint loads"), DogClass))
	{
		const AImmortalMonsterCharacter* Dog = DogClass->GetDefaultObject<AImmortalMonsterCharacter>();
		for (const TCHAR* Action : { TEXT("Idle"), TEXT("Move"), TEXT("Attack"), TEXT("Hurt"), TEXT("Death") })
		{
			const UPaperFlipbook* Clip = Cast<UPaperFlipbook>(BoundObject(Dog, FName(*FString::Printf(TEXT("%sFlipbook"), Action))));
			if (TestNotNull(TEXT("New dog action is bound"), Clip))
			{
				TestEqual(TEXT("New dog owns its corresponding authored action"), Clip->GetPathName(), AuthoredClipPath(TEXT("Dog"), Action));
			}
		}
		TestFalse(TEXT("Source artwork faces left"), BoundBool(Dog, TEXT("bArtworkFacesRight")));
		TestTrue(TEXT("Dog sprite uses the actual player foot plane and depth"), BoundBool(Dog, TEXT("bAlignSpriteToPlayerLane")));
		TestTrue(TEXT("Scene dog scale"), FMath::IsNearlyEqual(BoundFloat(Dog, TEXT("VisualScale")), 2.0f));
		TestTrue(TEXT("Dog authored contact frame three"), FMath::IsNearlyEqual(BoundFloat(Dog, TEXT("AttackWindup")), 0.25f));
		TestTrue(TEXT("Complete Hurt clip duration"), FMath::IsNearlyEqual(BoundFloat(Dog, TEXT("HurtAnimationDuration")), 8.0f / 12.0f));
		if (TestNotNull(TEXT("Dog capsule exists"), Dog->GetCapsuleComponent()))
		{
			const float CapsuleHalfHeight = Dog->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
			TestTrue(TEXT("Health bar baseline above reviewed standing art"),
				FMath::IsNearlyEqual(BoundFloat(Dog, TEXT("HealthBarHeight")), 185.0f - CapsuleHalfHeight, 0.1f));
			if (TestNotNull(TEXT("Dog sprite template exists"), Dog->GetSprite()))
			{
				TestTrue(TEXT("Template foot pivot starts at physical capsule bottom"),
					FMath::IsNearlyEqual(Dog->GetSprite()->GetRelativeLocation().Z, -CapsuleHalfHeight));
				TestTrue(TEXT("Runtime visual scale is not baked into the component twice"), Dog->GetSprite()->GetRelativeScale3D().Equals(FVector::OneVector));
				TestTrue(TEXT("Template starts at authored Idle"), Dog->GetSprite()->GetFlipbook() ==
					LoadObject<UPaperFlipbook>(nullptr, *AuthoredClipPath(TEXT("Dog"), TEXT("Idle"))));
				TestTrue(TEXT("Template starts playing a forward Idle loop"), Dog->GetSprite()->IsLooping()
					&& Dog->GetSprite()->IsPlaying() && !Dog->GetSprite()->IsReversing());
				TestTrue(TEXT("Template play rate preserved"), FMath::IsNearlyEqual(Dog->GetSprite()->GetPlayRate(), 1.0f));
			}
		}
	}
	TestPetArt(*this, TEXT("SpiritHound"), TEXT("Dog"));
	TestPetArt(*this, TEXT("SpiritFox"), TEXT("Fox"));
	UClass* FoxClass = LoadClass<AImmortalMonsterCharacter>(nullptr, TEXT("/Game/GAME/BP_Monster.BP_Monster_C"));
	if (TestNotNull(TEXT("Existing R07A fox remains loadable"), FoxClass))
	{
		const UObject* Fox = FoxClass->GetDefaultObject();
		for (const TCHAR* Action : { TEXT("Idle"), TEXT("Move"), TEXT("Attack"), TEXT("Hurt"), TEXT("Death") })
		{
			const UPaperFlipbook* Clip = Cast<UPaperFlipbook>(BoundObject(Fox, FName(*FString::Printf(TEXT("%sFlipbook"), Action))));
			if (TestNotNull(TEXT("Fox action remains bound"), Clip))
			{
				TestEqual(TEXT("Dog batch leaves Fox family bindings intact"), Clip->GetPathName(), AuthoredClipPath(TEXT("Fox"), Action));
			}
		}
		TestTrue(TEXT("Fox scene scale preserved"), FMath::IsNearlyEqual(BoundFloat(Fox, TEXT("VisualScale")), 2.0f));
		TestTrue(TEXT("Fox placement strategy preserved"), BoundBool(Fox, TEXT("bAlignSpriteToPlayerLane")));
		TestTrue(TEXT("Fox contact time preserved"), FMath::IsNearlyEqual(BoundFloat(Fox, TEXT("AttackWindup")), 0.25f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalDesktopDogLegacyBossIsolationTest,
	"ImmortalPath.Art.DesktopDogLegacyBossIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalDesktopDogLegacyBossIsolationTest::RunTest(const FString& Parameters)
{
	UBlueprint* LegacyBlueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/GAME/BP_DogMonster.BP_DogMonster"));
	UBlueprint* DesktopBlueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/GAME/BP_DesktopDogMonster.BP_DesktopDogMonster"));
	if (!TestNotNull(TEXT("Protected legacy dog/Boss Blueprint loads"), LegacyBlueprint)
		|| !TestNotNull(TEXT("New desktop dog Blueprint loads"), DesktopBlueprint)) { return false; }
	UClass* LegacyClass = LegacyBlueprint->GeneratedClass;
	UClass* DesktopClass = DesktopBlueprint->GeneratedClass;
	if (!TestNotNull(TEXT("Legacy generated class present"), LegacyClass)
		|| !TestNotNull(TEXT("Desktop generated class present"), DesktopClass)) { return false; }
	const AImmortalMonsterCharacter* Legacy = Cast<AImmortalMonsterCharacter>(LegacyClass->GetDefaultObject());
	const AImmortalMonsterCharacter* Desktop = Cast<AImmortalMonsterCharacter>(DesktopClass->GetDefaultObject());
	if (!TestNotNull(TEXT("Legacy monster defaults load"), Legacy)
		|| !TestNotNull(TEXT("Desktop monster defaults load"), Desktop)) { return false; }
	TestTrue(TEXT("Desktop BP is a separate package, not a modification of the Boss source"),
		DesktopBlueprint->GetOutermost() != LegacyBlueprint->GetOutermost());
	TestTrue(TEXT("Protected duplication retains the original native parent"), DesktopBlueprint->ParentClass == LegacyBlueprint->ParentClass);
	TestFalse(TEXT("Desktop dog does not inherit modified fox defaults"),
		DesktopClass->IsChildOf(LoadClass<AImmortalMonsterCharacter>(nullptr, TEXT("/Game/GAME/BP_Monster.BP_Monster_C"))));
	const TMap<FString, FString> LegacyGameplay = EditableSnapshot(Legacy, PresentationProperties());
	TestTrue(TEXT("Complete inherited editable gameplay snapshot is populated"), LegacyGameplay.Num() > 30);
	CompareSnapshots(*this, LegacyGameplay, EditableSnapshot(Desktop, PresentationProperties()), TEXT("Copied gameplay"));
	if (TestNotNull(TEXT("Legacy sprite present"), Legacy->GetSprite())
		&& TestNotNull(TEXT("Desktop sprite present"), Desktop->GetSprite()))
	{
		const TMap<FString, FString> LegacyComponents = ComponentSnapshot(LegacyBlueprint, Legacy);
		TestTrue(TEXT("Capsule/movement/render component snapshot is populated"), LegacyComponents.Num() > 50);
		CompareSnapshots(*this, LegacyComponents, ComponentSnapshot(DesktopBlueprint, Desktop), TEXT("Copied components"));
	}
	TestTrue(TEXT("Copied Actor transform unchanged"), Desktop->GetActorTransform().Equals(Legacy->GetActorTransform()));
	if (TestNotNull(TEXT("Legacy capsule present"), Legacy->GetCapsuleComponent())
		&& TestNotNull(TEXT("Desktop capsule present"), Desktop->GetCapsuleComponent()))
	{
		TestEqual(TEXT("Capsule radius preserved"), Desktop->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), Legacy->GetCapsuleComponent()->GetUnscaledCapsuleRadius());
		TestEqual(TEXT("Capsule half-height preserved"), Desktop->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), Legacy->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
		TestTrue(TEXT("Capsule transform preserved"), Desktop->GetCapsuleComponent()->GetRelativeTransform().Equals(Legacy->GetCapsuleComponent()->GetRelativeTransform()));
	}
	const TPair<const TCHAR*, const TCHAR*> LegacyClips[] = {
		{ TEXT("MoveFlipbook"), TEXT("/Game/GAME/Asset/Player/dog_move_left/dog_moveleft.dog_moveleft") },
		{ TEXT("AttackFlipbook"), TEXT("/Game/GAME/Asset/Player/dog_attcak/dogattack.dogattack") },
		{ TEXT("HurtFlipbook"), TEXT("/Game/GAME/Asset/Player/doghurt/doghurt.doghurt") },
		{ TEXT("DeathFlipbook"), TEXT("/Game/GAME/Asset/Player/dog_death/death_dog.death_dog") }
	};
	for (const auto& Entry : LegacyClips)
	{
		const UPaperFlipbook* Clip = Cast<UPaperFlipbook>(BoundObject(Legacy, FName(Entry.Key)));
		if (TestNotNull(TEXT("Legacy dog/Boss action preserved"), Clip))
		{
			TestEqual(TEXT("Legacy dog/Boss still uses the original action asset"), Clip->GetPathName(), FString(Entry.Value));
		}
	}
	TestFalse(TEXT("Legacy dog/Boss keeps its original center-pivot lane behavior"), BoundBool(Legacy, TEXT("bAlignSpriteToPlayerLane")));
	TestTrue(TEXT("Legacy scene scale untouched"), FMath::IsNearlyEqual(BoundFloat(Legacy, TEXT("VisualScale")), 3.0f));
	TestTrue(TEXT("Legacy visual depth untouched"), FMath::IsNearlyEqual(BoundFloat(Legacy, TEXT("VisualDepthOffset")), -100.0f));
	TestTrue(TEXT("Legacy health bar height untouched"), FMath::IsNearlyEqual(BoundFloat(Legacy, TEXT("HealthBarHeight")), 135.0f));
	TestTrue(TEXT("Legacy contact timing untouched"), FMath::IsNearlyEqual(BoundFloat(Legacy, TEXT("AttackWindup")), 0.35f));
	TestTrue(TEXT("Legacy Hurt duration untouched"), FMath::IsNearlyEqual(BoundFloat(Legacy, TEXT("HurtAnimationDuration")), 0.35f));
	// These numerical values are the pre-R07B BP_DogMonster baseline, also
	// guarded by the dynamic complete copied-default snapshot above.
	TestTrue(TEXT("Legacy max health baseline"), FMath::IsNearlyEqual(BoundFloat(Legacy, TEXT("MaxHealth")), 45.0f));
	TestTrue(TEXT("Legacy movement speed baseline"), FMath::IsNearlyEqual(BoundFloat(Legacy, TEXT("MovementSpeed")), 100.0f));
	TestTrue(TEXT("Legacy attack range baseline"), FMath::IsNearlyEqual(BoundFloat(Legacy, TEXT("AttackRange")), 320.0f));
	TestTrue(TEXT("Legacy attack damage baseline"), FMath::IsNearlyEqual(BoundFloat(Legacy, TEXT("AttackDamage")), 6.0f));
	TestTrue(TEXT("Legacy attack interval baseline"), FMath::IsNearlyEqual(BoundFloat(Legacy, TEXT("AttackInterval")), 1.7f, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalDesktopDogSpawnerRoutingTest,
	"ImmortalPath.Art.DesktopDogSpawnerRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalDesktopDogSpawnerRoutingTest::RunTest(const FString& Parameters)
{
	const AImmortalMonsterSpawner* Spawner = GetDefault<AImmortalMonsterSpawner>();
	UClass* DesktopDog = LoadClass<AImmortalMonsterCharacter>(nullptr,
		TEXT("/Game/GAME/BP_DesktopDogMonster.BP_DesktopDogMonster_C"));
	UClass* LegacyDogBoss = LoadClass<AImmortalMonsterCharacter>(nullptr,
		TEXT("/Game/GAME/BP_DogMonster.BP_DogMonster_C"));
	if (!TestNotNull(TEXT("Desktop dog class present"), DesktopDog)
		|| !TestNotNull(TEXT("Legacy Boss class present"), LegacyDogBoss)) { return false; }
	const UObject* DefaultAlternate = BoundObject(Spawner, TEXT("DefaultAlternateMonsterClass"));
	const UObject* DefaultBoss = BoundObject(Spawner, TEXT("DefaultBossMonsterClass"));
	TestTrue(TEXT("Default ordinary alternate uses the independent new dog"), DefaultAlternate == DesktopDog);
	TestTrue(TEXT("Default Boss keeps the protected old dog class"), DefaultBoss == LegacyDogBoss);
	TestTrue(TEXT("Ordinary and Boss fallback routes are separate"), DefaultAlternate != DefaultBoss);
	return true;
}

#endif
