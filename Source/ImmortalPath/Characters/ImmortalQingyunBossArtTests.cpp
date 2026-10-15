// Copyright Epic Games, Inc. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "ImmortalMonsterCharacter.h"
#include "../Maps/ImmortalMapTypes.h"
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
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

// Named namespace keeps these reflection helpers separate from Fox/Dog tests
// when Unreal's normal unity-build mode combines their translation units.
namespace ImmortalQingyunBossArtTests
{
	constexpr const TCHAR* AssetDirectory = TEXT("/Game/GAME/Asset/DesktopPixelV2/QingyunBoss/");
	constexpr const TCHAR* LegacyBlueprintPath = TEXT("/Game/GAME/BP_DogMonster.BP_DogMonster");
	constexpr const TCHAR* BossBlueprintPath = TEXT("/Game/GAME/BP_DesktopQingyunBoss.BP_DesktopQingyunBoss");
	constexpr const TCHAR* BossClassPath = TEXT("/Game/GAME/BP_DesktopQingyunBoss.BP_DesktopQingyunBoss_C");

	FString ClipPath(const TCHAR* Action)
	{
		const FString Name = FString::Printf(TEXT("FB_QingyunBoss_%s"), Action);
		return FString(AssetDirectory) + Name + TEXT(".") + Name;
	}

	UObject* BoundObject(const UObject* Object, FName PropertyName)
	{
		const FObjectPropertyBase* Property = Object
			? FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName) : nullptr;
		return Property ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
	}

	float BoundFloat(const UObject* Object, FName PropertyName)
	{
		const FFloatProperty* Property = Object
			? FindFProperty<FFloatProperty>(Object->GetClass(), PropertyName) : nullptr;
		return Property ? Property->GetPropertyValue_InContainer(Object) : -1000.0f;
	}

	bool BoundBool(const UObject* Object, FName PropertyName)
	{
		const FBoolProperty* Property = Object
			? FindFProperty<FBoolProperty>(Object->GetClass(), PropertyName) : nullptr;
		return Property && Property->GetPropertyValue_InContainer(Object);
	}

	FString CanonicalIdentity(FString Value)
	{
		Value.ReplaceInline(TEXT("/Game/GAME/BP_DesktopQingyunBoss.BP_DesktopQingyunBoss"),
			TEXT("/Game/GAME/BP_DogMonster.BP_DogMonster"), ESearchCase::CaseSensitive);
		Value.ReplaceInline(TEXT("/Game/GAME/BP_DesktopQingyunBoss.Default__BP_DesktopQingyunBoss"),
			TEXT("/Game/GAME/BP_DogMonster.Default__BP_DogMonster"), ESearchCase::CaseSensitive);
		Value.ReplaceInline(TEXT("/Game/GAME/BP_DesktopQingyunBoss"),
			TEXT("/Game/GAME/BP_DogMonster"), ESearchCase::CaseSensitive);
		return Value;
	}

	TSet<FName> PresentationProperties()
	{
		// AttackWindup and HurtAnimationDuration deliberately remain protected.
		return { TEXT("IdleFlipbook"), TEXT("MoveFlipbook"), TEXT("AttackFlipbook"),
			TEXT("HurtFlipbook"), TEXT("DeathFlipbook"), TEXT("bArtworkFacesRight"),
			TEXT("bAlignSpriteToPlayerLane"), TEXT("VisualScale"), TEXT("HealthBarHeight"),
			TEXT("bPreserveAuthoredMapBossPalette") };
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
				Result.Add(Property->GetName(), CanonicalIdentity(MoveTemp(Value)));
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
		TSet<UActorComponent*> Sprites { Defaults->GetSprite() };
		const FName SpriteName = Defaults->GetSprite()->GetFName();
		if (Blueprint->SimpleConstructionScript)
		{
			for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
			{
				if (Node && Node->GetVariableName() == SpriteName && Node->ComponentTemplate)
				{
					Sprites.Add(Node->ComponentTemplate);
				}
			}
		}
		if (UInheritableComponentHandler* Handler = Blueprint->GetInheritableComponentHandler(false))
		{
			for (auto It = Handler->CreateRecordIterator(); It; ++It)
			{
				if (It->ComponentKey.GetSCSVariableName() == SpriteName && It->ComponentTemplate)
				{
					Sprites.Add(It->ComponentTemplate);
				}
			}
		}
		for (UActorComponent* Component : ComponentTemplates(Blueprint, Defaults))
		{
			const bool bSprite = Sprites.Contains(Component) && Component->IsA<UPaperFlipbookComponent>();
			TSet<FName> Allowed;
			if (bSprite)
			{
				Allowed = { TEXT("RelativeLocation"), TEXT("SourceFlipbook"),
					TEXT("OverrideMaterials"), TEXT("PlayRate") };
			}
			const FString Key = CanonicalIdentity(Component->GetPathName());
			Result.Add(Key + TEXT(".@Class"), Component->GetClass()->GetPathName());
			for (const auto& Entry : EditableSnapshot(Component, Allowed))
			{
				Result.Add(Key + TEXT(".") + Entry.Key, Entry.Value);
			}
			if (bSprite)
			{
				const UPaperFlipbookComponent* Sprite = CastChecked<UPaperFlipbookComponent>(Component);
				FVector XY = Sprite->GetRelativeLocation();
				XY.Z = 0.0;
				Result.Add(Key + TEXT(".@ProtectedLocationXY"), XY.ToString());
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalQingyunBossAnimationSetTest,
	"ImmortalPath.Art.DesktopQingyunBossAnimationSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalQingyunBossAnimationSetTest::RunTest(const FString& Parameters)
{
	using namespace ImmortalQingyunBossArtTests;
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Paper2D/MaskedUnlitSpriteMaterial.MaskedUnlitSpriteMaterial"));
	if (!TestNotNull(TEXT("Canonical masked material loads"), Material)) { return false; }
	TestTrue(TEXT("Canonical material uses hard alpha masking"), Material->GetBlendMode() == BLEND_Masked);
	const TCHAR* Actions[] = { TEXT("Idle"), TEXT("Move"), TEXT("Attack"), TEXT("Hurt"), TEXT("Death") };
	const float Rates[] = { 8.0f, 8.0f, 12.0f, 120.0f / 7.0f, 10.0f };
	TSet<const UPaperSprite*> AllSprites;
	TSet<const UTexture2D*> Atlases;
	for (int32 ActionIndex = 0; ActionIndex < UE_ARRAY_COUNT(Actions); ++ActionIndex)
	{
		UPaperFlipbook* Clip = LoadObject<UPaperFlipbook>(nullptr, *ClipPath(Actions[ActionIndex]));
		if (!TestNotNull(FString::Printf(TEXT("Qingyun Boss %s loads"), Actions[ActionIndex]), Clip)) { continue; }
		TestEqual(TEXT("Eight authored key frames"), Clip->GetNumKeyFrames(), 8);
		TestEqual(TEXT("Exactly eight playback frames"), Clip->GetNumFrames(), 8);
		TestTrue(TEXT("Fixed authored FPS"), FMath::IsNearlyEqual(Clip->GetFramesPerSecond(), Rates[ActionIndex], 0.00001f));
		TestTrue(TEXT("Complete action duration matches eight frames"),
			FMath::IsNearlyEqual(Clip->GetTotalDuration(), 8.0f / Rates[ActionIndex], 0.00001f));
		TestTrue(TEXT("Flipbook uses canonical masked material"), Clip->GetDefaultMaterial() == Material);
		TSet<FVector2D> Regions;
		const UTexture2D* Atlas = nullptr;
		for (int32 Index = 0; Index < Clip->GetNumKeyFrames(); ++Index)
		{
			const FPaperFlipbookKeyFrame& Frame = Clip->GetKeyFrameChecked(Index);
			const UPaperSprite* Sprite = Frame.Sprite;
			if (!TestNotNull(TEXT("Every key frame has a sprite"), Sprite)) { continue; }
			TestFalse(TEXT("Forty sprite objects remain independent"), AllSprites.Contains(Sprite));
			AllSprites.Add(Sprite);
			Regions.Add(Sprite->GetSourceUV());
			TestTrue(TEXT("Sprite belongs to Qingyun-only art"), Sprite->GetPathName().StartsWith(AssetDirectory));
			TestEqual(TEXT("One playback frame per authored pose"), Frame.FrameRun, 1);
			TestTrue(TEXT("Uniform 512x384 cells"), Sprite->GetSourceSize().Equals(FVector2D(512, 384), 0.01));
			TestTrue(TEXT("Exact chronological 4x2 grid"), Sprite->GetSourceUV().Equals(
				FVector2D((Index % 4) * 512, (Index / 4) * 384), 0.01));
			TestTrue(TEXT("Foot contact pivot"),
				(Sprite->GetPivotPosition() - Sprite->GetSourceUV()).Equals(FVector2D(256, 336), 0.01));
			TestTrue(TEXT("World density remains 2.56"), FMath::IsNearlyEqual(Sprite->GetPixelsPerUnrealUnit(), 2.56f));
			TestTrue(TEXT("Sprite uses canonical masked material"), Sprite->GetDefaultMaterial() == Material);
			const UTexture2D* Source = Sprite->GetSourceTexture();
			if (TestNotNull(TEXT("Atlas source exists"), Source))
			{
				TestTrue(TEXT("Atlas is the full 2048x768 imported image"), Source->GetImportedSize() == FIntPoint(2048, 768));
				TestTrue(TEXT("Atlas belongs to Qingyun-only art"), Source->GetPathName().StartsWith(AssetDirectory));
				TestTrue(TEXT("One atlas per action"), !Atlas || Source == Atlas);
				Atlas = Source;
			}
		}
		TestEqual(TEXT("Eight separate source regions"), Regions.Num(), 8);
		if (Atlas)
		{
			TestFalse(TEXT("Actions do not share an atlas"), Atlases.Contains(Atlas));
			Atlases.Add(Atlas);
		}
	}
	TestEqual(TEXT("Five action atlases"), Atlases.Num(), 5);
	TestEqual(TEXT("Forty unique sprite objects"), AllSprites.Num(), 40);

	UClass* LegacyClass = LoadClass<AImmortalMonsterCharacter>(nullptr,
		TEXT("/Game/GAME/BP_DogMonster.BP_DogMonster_C"));
	UPaperFlipbook* Hurt = LoadObject<UPaperFlipbook>(nullptr, *ClipPath(TEXT("Hurt")));
	if (TestNotNull(TEXT("Protected legacy class loads for duration comparison"), LegacyClass)
		&& TestNotNull(TEXT("New Hurt loads for duration comparison"), Hurt))
	{
		const UPaperFlipbook* LegacyHurt = Cast<UPaperFlipbook>(
			BoundObject(LegacyClass->GetDefaultObject(), TEXT("HurtFlipbook")));
		if (TestNotNull(TEXT("Protected legacy Hurt is bound"), LegacyHurt))
		{
			TestTrue(TEXT("Actual Hurt reaction duration is preserved, not the .35 fallback"),
				FMath::IsNearlyEqual(Hurt->GetTotalDuration(), LegacyHurt->GetTotalDuration(), 0.0001f));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalQingyunBossBindingsAndIsolationTest,
	"ImmortalPath.Art.DesktopQingyunBossBindingsAndIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalQingyunBossBindingsAndIsolationTest::RunTest(const FString& Parameters)
{
	using namespace ImmortalQingyunBossArtTests;
	UBlueprint* LegacyBP = LoadObject<UBlueprint>(nullptr, LegacyBlueprintPath);
	UBlueprint* BossBP = LoadObject<UBlueprint>(nullptr, BossBlueprintPath);
	if (!TestNotNull(TEXT("Protected legacy Blueprint loads"), LegacyBP)
		|| !TestNotNull(TEXT("Independent Qingyun Boss Blueprint loads"), BossBP)
		|| !TestNotNull(TEXT("Legacy generated class exists"), LegacyBP->GeneratedClass.Get())
		|| !TestNotNull(TEXT("Boss generated class exists"), BossBP->GeneratedClass.Get())) { return false; }
	const AImmortalMonsterCharacter* Legacy = Cast<AImmortalMonsterCharacter>(LegacyBP->GeneratedClass->GetDefaultObject());
	const AImmortalMonsterCharacter* Boss = Cast<AImmortalMonsterCharacter>(BossBP->GeneratedClass->GetDefaultObject());
	if (!TestNotNull(TEXT("Legacy monster CDO exists"), Legacy)
		|| !TestNotNull(TEXT("Boss monster CDO exists"), Boss)) { return false; }
	TestTrue(TEXT("New Boss is in a separate package"), BossBP->GetOutermost() != LegacyBP->GetOutermost());
	TestTrue(TEXT("New Boss retains the protected native parent"), BossBP->ParentClass == LegacyBP->ParentClass);
	const TMap<FString, FString> Gameplay = EditableSnapshot(Legacy, PresentationProperties());
	TestTrue(TEXT("Complete gameplay snapshot includes inherited editable values"), Gameplay.Num() > 30);
	TestTrue(TEXT("Snapshot protects attack contact timing"), Gameplay.Contains(TEXT("AttackWindup")));
	TestTrue(TEXT("Snapshot protects Hurt fallback timing"), Gameplay.Contains(TEXT("HurtAnimationDuration")));
	CompareSnapshots(*this, Gameplay, EditableSnapshot(Boss, PresentationProperties()), TEXT("Boss gameplay defaults"));
	TestTrue(TEXT("Actor transform is unchanged"), Boss->GetActorTransform().Equals(Legacy->GetActorTransform()));
	for (const TCHAR* Action : { TEXT("Idle"), TEXT("Move"), TEXT("Attack"), TEXT("Hurt"), TEXT("Death") })
	{
		const UPaperFlipbook* Clip = Cast<UPaperFlipbook>(BoundObject(Boss,
			FName(*FString::Printf(TEXT("%sFlipbook"), Action))));
		if (TestNotNull(TEXT("Every Boss action is bound"), Clip))
		{
			TestEqual(TEXT("Boss binds the corresponding new action"), Clip->GetPathName(), ClipPath(Action));
		}
	}
	TestFalse(TEXT("Boss source artwork faces left"), BoundBool(Boss, TEXT("bArtworkFacesRight")));
	TestTrue(TEXT("Boss opts into actual player foot and depth placement"), BoundBool(Boss, TEXT("bAlignSpriteToPlayerLane")));
	TestTrue(TEXT("Only new Boss opts into authored map-Boss palette"), BoundBool(Boss, TEXT("bPreserveAuthoredMapBossPalette")));
	TestFalse(TEXT("Legacy Boss palette behavior stays disabled"), BoundBool(Legacy, TEXT("bPreserveAuthoredMapBossPalette")));
	TestTrue(TEXT("Boss presentation scale"), FMath::IsNearlyEqual(BoundFloat(Boss, TEXT("VisualScale")), 2.0f));
	TestTrue(TEXT("Legacy .35 attack contact preserved"), FMath::IsNearlyEqual(BoundFloat(Boss, TEXT("AttackWindup")), 0.35f));
	TestTrue(TEXT("Legacy .35 Hurt fallback preserved"), FMath::IsNearlyEqual(BoundFloat(Boss, TEXT("HurtAnimationDuration")), 0.35f));
	const TPair<const TCHAR*, float> Baselines[] = {
		{ TEXT("MaxHealth"), 45.0f }, { TEXT("MovementSpeed"), 100.0f },
		{ TEXT("AttackRange"), 320.0f }, { TEXT("AttackDamage"), 6.0f },
		{ TEXT("AttackInterval"), 1.7f }
	};
	for (const auto& Entry : Baselines)
	{
		TestTrue(FString::Printf(TEXT("Legacy base %s stays fixed"), Entry.Key),
			FMath::IsNearlyEqual(BoundFloat(Legacy, Entry.Key), Entry.Value, 0.001f));
		TestTrue(FString::Printf(TEXT("New Boss preserves base %s"), Entry.Key),
			FMath::IsNearlyEqual(BoundFloat(Boss, Entry.Key), Entry.Value, 0.001f));
	}
	if (TestNotNull(TEXT("Boss capsule exists"), Boss->GetCapsuleComponent())
		&& TestNotNull(TEXT("Legacy capsule exists"), Legacy->GetCapsuleComponent())
		&& TestNotNull(TEXT("Boss sprite template exists"), Boss->GetSprite())
		&& TestNotNull(TEXT("Legacy sprite template exists"), Legacy->GetSprite()))
	{
		const float HalfHeight = Boss->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		TestEqual(TEXT("Capsule half-height is unchanged"), HalfHeight, Legacy->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
		TestEqual(TEXT("Capsule radius is unchanged"), Boss->GetCapsuleComponent()->GetUnscaledCapsuleRadius(),
			Legacy->GetCapsuleComponent()->GetUnscaledCapsuleRadius());
		TestTrue(TEXT("Health bar uses the reviewed 275 foot baseline above the nine-tail fan"),
			FMath::IsNearlyEqual(BoundFloat(Boss, TEXT("HealthBarHeight")), 275.0f - HalfHeight, 0.1f));
		TestTrue(TEXT("Template starts at its physical capsule bottom"),
			FMath::IsNearlyEqual(Boss->GetSprite()->GetRelativeLocation().Z, -HalfHeight));
		TestTrue(TEXT("Visual scale is not baked twice"), Boss->GetSprite()->GetRelativeScale3D().Equals(FVector::OneVector));
		TestTrue(TEXT("Sprite starts at new Idle"), Boss->GetSprite()->GetFlipbook() ==
			LoadObject<UPaperFlipbook>(nullptr, *ClipPath(TEXT("Idle"))));
		TestTrue(TEXT("Sprite starts a forward Idle loop"), Boss->GetSprite()->IsLooping()
			&& Boss->GetSprite()->IsPlaying() && !Boss->GetSprite()->IsReversing());
		TestTrue(TEXT("Sprite playback rate is one"), FMath::IsNearlyEqual(Boss->GetSprite()->GetPlayRate(), 1.0f));
		const TMap<FString, FString> Components = ComponentSnapshot(LegacyBP, Legacy);
		TestTrue(TEXT("Full component snapshot includes capsule and movement"), Components.Num() > 50);
		CompareSnapshots(*this, Components, ComponentSnapshot(BossBP, Boss), TEXT("Boss components"));
	}
	const UPaperFlipbook* Attack = Cast<UPaperFlipbook>(BoundObject(Boss, TEXT("AttackFlipbook")));
	if (TestNotNull(TEXT("Attack clip exists for contact and cadence checks"), Attack))
	{
		TestEqual(TEXT("Preserved .35 contact falls on authored zero-based frame four"),
			Attack->GetKeyFrameIndexAtTime(BoundFloat(Boss, TEXT("AttackWindup")), true), 4);
		const float PhaseThreeCooldown = BoundFloat(Boss, TEXT("AttackInterval"))
			/ (BoundFloat(Boss, TEXT("AttackSpeedMultiplier")) * 1.05f * 1.12f * 1.18f);
		TestTrue(TEXT("Complete Attack duration stays below map-Boss phase-three cooldown"),
			Attack->GetTotalDuration() < PhaseThreeCooldown);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImmortalQingyunBossSpawnerRoutingTest,
	"ImmortalPath.Art.DesktopQingyunBossSpawnerRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalQingyunBossSpawnerRoutingTest::RunTest(const FString& Parameters)
{
	using namespace ImmortalQingyunBossArtTests;
	const AImmortalMonsterSpawner* Spawner = GetDefault<AImmortalMonsterSpawner>();
	UClass* BossClass = LoadClass<AImmortalMonsterCharacter>(nullptr, BossClassPath);
	UClass* LegacyClass = LoadClass<AImmortalMonsterCharacter>(nullptr,
		TEXT("/Game/GAME/BP_DogMonster.BP_DogMonster_C"));
	UClass* DogClass = LoadClass<AImmortalMonsterCharacter>(nullptr,
		TEXT("/Game/GAME/BP_DesktopDogMonster.BP_DesktopDogMonster_C"));
	if (!TestNotNull(TEXT("Default native spawner exists"), Spawner)
		|| !TestNotNull(TEXT("New Qingyun class exists"), BossClass)
		|| !TestNotNull(TEXT("Protected legacy class exists"), LegacyClass)
		|| !TestNotNull(TEXT("Ordinary desktop dog class exists"), DogClass)) { return false; }
	TestTrue(TEXT("Global optional-encounter Boss default stays legacy"),
		BoundObject(Spawner, TEXT("DefaultBossMonsterClass")) == LegacyClass);
	TestTrue(TEXT("Qingyun startup constructor loads its dedicated class dependency"),
		BoundObject(Spawner, TEXT("DefaultQingyunBossMonsterClass")) == BossClass);
	TestTrue(TEXT("Ordinary alternate dog default stays new"),
		BoundObject(Spawner, TEXT("DefaultAlternateMonsterClass")) == DogClass);
	TestTrue(TEXT("Ordinary dog keeps map-Boss palette opt-in disabled"),
		!BoundBool(DogClass->GetDefaultObject(), TEXT("bPreserveAuthoredMapBossPalette")));
	const TArray<FName> MapIds = UImmortalMapLibrary::GetKnownMapIds();
	TestEqual(TEXT("Routing covers all eight existing adventure maps"), MapIds.Num(), 8);
	for (const FName MapId : MapIds)
	{
		const UClass* Expected = MapId == UImmortalMapLibrary::GetQingyunMountainId() ? BossClass : LegacyClass;
		TestTrue(FString::Printf(TEXT("%s has the intended map-only Boss class"), *MapId.ToString()),
			Spawner->ResolveMapBossMonsterClass(MapId).Get() == Expected);
	}
	TestTrue(TEXT("Unknown map keeps protected fallback"),
		Spawner->ResolveMapBossMonsterClass(TEXT("R07CUnknownMap")).Get() == LegacyClass);
	TestTrue(TEXT("Empty map keeps protected fallback"),
		Spawner->ResolveMapBossMonsterClass(NAME_None).Get() == LegacyClass);
	const UObject* OriginalOverride = BoundObject(Spawner, TEXT("BossMonsterClass"));
	// Duplicate into the transient package instead of modifying the actual CDO
	// while LoadSynchronous may initialize other classes during a routing check.
	TStrongObjectPtr<AImmortalMonsterSpawner> OverrideSpawner(
		DuplicateObject<AImmortalMonsterSpawner>(Spawner, GetTransientPackage()));
	if (!TestNotNull(TEXT("Transient spawner copy exists for explicit overrides"), OverrideSpawner.Get())) { return false; }
	FObjectPropertyBase* OverrideProperty = FindFProperty<FObjectPropertyBase>(
		OverrideSpawner->GetClass(), TEXT("BossMonsterClass"));
	if (!TestNotNull(TEXT("Boss override property exists"), OverrideProperty)) { return false; }
	OverrideProperty->SetObjectPropertyValue_InContainer(OverrideSpawner.Get(), DogClass);
	for (const FName MapId : MapIds)
	{
		TestTrue(FString::Printf(TEXT("Explicit override wins on %s"), *MapId.ToString()),
			OverrideSpawner->ResolveMapBossMonsterClass(MapId).Get() == DogClass);
	}
	TestTrue(TEXT("Explicit override also wins for an unknown map"),
		OverrideSpawner->ResolveMapBossMonsterClass(TEXT("R07CUnknownMap")).Get() == DogClass);
	OverrideProperty->SetObjectPropertyValue_InContainer(OverrideSpawner.Get(), nullptr);
	TestTrue(TEXT("Clearing the transient override restores Qingyun's map-specific route"),
		OverrideSpawner->ResolveMapBossMonsterClass(UImmortalMapLibrary::GetQingyunMountainId()).Get() == BossClass);
	TestTrue(TEXT("Actual spawner CDO was never modified"),
		BoundObject(Spawner, TEXT("BossMonsterClass")) == OriginalOverride);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
