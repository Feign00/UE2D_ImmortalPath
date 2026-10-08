// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalDogArtCommandlet.h"
#include "BlueprintEditorSettings.h"

#include "Components/ActorComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/SecureHash.h"
#include "ObjectTools.h"
#include "PaperCharacter.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImmortalDogArtCommandlet)

DEFINE_LOG_CATEGORY_STATIC(LogImmortalDogArt, Log, All);

namespace ImmortalDogArt
{
	constexpr const TCHAR* LegacyPath = TEXT("/Game/GAME/BP_DogMonster.BP_DogMonster");
	constexpr const TCHAR* LegacyPackage = TEXT("/Game/GAME/BP_DogMonster");
	constexpr const TCHAR* BlueprintPath = TEXT("/Game/GAME/BP_DesktopDogMonster.BP_DesktopDogMonster");
	constexpr const TCHAR* BlueprintPackage = TEXT("/Game/GAME/BP_DesktopDogMonster");
	constexpr const TCHAR* DogAssetDirectory = TEXT("/Game/GAME/Asset/Monsters/desktop_pixel_v2/dog/");
	const FVector2D FrameSize(512.0, 384.0);
	const FVector2D FootPivot(256.0, 336.0);
	constexpr float PixelsPerUnit = 2.56f;
	constexpr float DogWindup = 0.25f;
	constexpr float DogHurtDuration = 8.0f / 12.0f;
	constexpr float DogVisualScale = 2.0f;
	constexpr float DogHealthBarHeightAboveFeet = 185.0f;

	struct FActionBinding
	{
		const TCHAR* Action;
		const TCHAR* Property;
		float FramesPerSecond;
		UPaperFlipbook* Clip = nullptr;
		FObjectPropertyBase* ReflectedProperty = nullptr;
	};

	struct FScalarBindings
	{
		FBoolProperty* FacesRight = nullptr;
		FBoolProperty* AlignToPlayerLane = nullptr;
		FFloatProperty* VisualScale = nullptr;
		FFloatProperty* Windup = nullptr;
		FFloatProperty* HurtDuration = nullptr;
		FFloatProperty* HealthBarHeight = nullptr;
	};

	// References to the duplicated CDO/components necessarily acquire a new
	// package/object name. Canonicalize only these two fixed Blueprint identities;
	// all external assets, numerical gameplay defaults and component names remain
	// exact. No arbitrary path remapping is supported.
	FString CanonicalizeIdentity(FString Value)
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

	TMap<FString, FString> CaptureEditableProperties(const UObject* Object,
		const TSet<FName>& Allowed = {})
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
				// UE's export parent parameter is non-const; this read-only export
				// does not change the object used as its naming context.
				Property->ExportText_InContainer(0, Value, Object, nullptr, const_cast<UObject*>(Object), PPF_None);
				Result.Add(Property->GetName(), CanonicalizeIdentity(MoveTemp(Value)));
			}
		}
		return Result;
	}

	bool CompareSnapshots(const TMap<FString, FString>& Expected,
		const TMap<FString, FString>& Actual, const TCHAR* Label, FString& Error)
	{
		if (Expected.Num() != Actual.Num())
		{
			Error = FString::Printf(TEXT("%s property count changed (%d -> %d)."), Label, Expected.Num(), Actual.Num());
			return false;
		}
		for (const auto& Entry : Expected)
		{
			const FString* Value = Actual.Find(Entry.Key);
			if (!Value || *Value != Entry.Value)
			{
				Error = FString::Printf(TEXT("%s property %s changed outside the presentation whitelist."), Label, *Entry.Key);
				return false;
			}
		}
		return true;
	}

	void CollectComponentTemplates(UBlueprint* Blueprint, APaperCharacter* Defaults,
		TArray<UActorComponent*>& OutTemplates)
	{
		Defaults->GetComponents(OutTemplates);
		if (Blueprint->SimpleConstructionScript)
		{
			for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
			{
				if (Node && Node->ComponentTemplate) { OutTemplates.AddUnique(Node->ComponentTemplate); }
			}
		}
		if (UInheritableComponentHandler* Handler = Blueprint->GetInheritableComponentHandler(false))
		{
			for (auto It = Handler->CreateRecordIterator(); It; ++It)
			{
				if (It->ComponentTemplate) { OutTemplates.AddUnique(It->ComponentTemplate); }
			}
		}
	}

	bool CollectSpriteTemplates(UBlueprint* Blueprint, APaperCharacter* Defaults,
		TArray<UPaperFlipbookComponent*>& OutTemplates, FString& Error)
	{
		UPaperFlipbookComponent* Sprite = Defaults->GetSprite();
		if (!Sprite)
		{
			Error = TEXT("The fixed dog Blueprint has no inherited PaperCharacter sprite.");
			return false;
		}
		OutTemplates.AddUnique(Sprite);
		const FName SpriteName = Sprite->GetFName();
		if (Blueprint->SimpleConstructionScript)
		{
			for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
			{
				if (Node && Node->GetVariableName() == SpriteName)
				{
					if (UPaperFlipbookComponent* Template = Cast<UPaperFlipbookComponent>(Node->ComponentTemplate))
					{
						OutTemplates.AddUnique(Template);
					}
				}
			}
		}
		if (UInheritableComponentHandler* Handler = Blueprint->GetInheritableComponentHandler(false))
		{
			for (auto It = Handler->CreateRecordIterator(); It; ++It)
			{
				if (It->ComponentKey.GetSCSVariableName() == SpriteName)
				{
					if (UPaperFlipbookComponent* Template = Cast<UPaperFlipbookComponent>(It->ComponentTemplate))
					{
						OutTemplates.AddUnique(Template);
					}
				}
			}
		}
		for (UPaperFlipbookComponent* Template : OutTemplates)
		{
			if (Template->GetOutermost() != Blueprint->GetOutermost())
			{
				Error = TEXT("Refusing to change a sprite template outside the fixed dog Blueprint package.");
				return false;
			}
		}
		return !OutTemplates.IsEmpty();
	}

	TMap<FString, FString> CaptureComponentProperties(UBlueprint* Blueprint, APaperCharacter* Defaults,
		bool bAllowPresentationChanges)
	{
		TMap<FString, FString> Result;
		TArray<UActorComponent*> Components;
		CollectComponentTemplates(Blueprint, Defaults, Components);
		TArray<UPaperFlipbookComponent*> Sprites;
		FString UnusedError;
		CollectSpriteTemplates(Blueprint, Defaults, Sprites, UnusedError);
		for (UActorComponent* Component : Components)
		{
			TSet<FName> Allowed;
			if (bAllowPresentationChanges && Sprites.Contains(Cast<UPaperFlipbookComponent>(Component)))
			{
				// Relative X/Y, rotation and scale are protected separately below.
				Allowed = { TEXT("RelativeLocation"), TEXT("SourceFlipbook"),
					TEXT("OverrideMaterials"), TEXT("PlayRate") };
			}
			const FString ComponentKey = CanonicalizeIdentity(Component->GetPathName());
			Result.Add(ComponentKey + TEXT(".@Class"), Component->GetClass()->GetPathName());
			for (const auto& Entry : CaptureEditableProperties(Component, Allowed))
			{
				Result.Add(ComponentKey + TEXT(".") + Entry.Key, Entry.Value);
			}
		}
		return Result;
	}

	bool ReadPackageDigest(const FString& Filename, FString& OutDigest)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Filename)) { return false; }
		OutDigest = FSHA1::HashBuffer(Bytes.GetData(), static_cast<uint64>(Bytes.Num())).ToString();
		return true;
	}

	bool LoadAndValidateActions(TArray<FActionBinding>& Actions, UMaterialInterface*& OutMaterial, FString& Error)
	{
		OutMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Paper2D/MaskedUnlitSpriteMaterial.MaskedUnlitSpriteMaterial"));
		if (!OutMaterial || OutMaterial->GetBlendMode() != BLEND_Masked)
		{
			Error = TEXT("The canonical Paper2D masked material could not be loaded.");
			return false;
		}
		TSet<UTexture2D*> DistinctActionTextures;
		TSet<UPaperSprite*> AllSprites;
		for (FActionBinding& Action : Actions)
		{
			const FString Name = FString::Printf(TEXT("FB_Dog_Pixel_%s"), Action.Action);
			const FString Path = FString(DogAssetDirectory) + Name + TEXT(".") + Name;
			Action.Clip = LoadObject<UPaperFlipbook>(nullptr, *Path);
			if (!Action.Clip || Action.Clip->GetNumKeyFrames() != 8 || Action.Clip->GetNumFrames() != 8
				|| !FMath::IsNearlyEqual(Action.Clip->GetFramesPerSecond(), Action.FramesPerSecond)
				|| Action.Clip->GetDefaultMaterial() != OutMaterial)
			{
				Error = FString::Printf(TEXT("%s requires eight unique single-run frames, exact FPS and canonical masked material."), *Path);
				return false;
			}
			UTexture2D* Texture = nullptr;
			for (int32 Index = 0; Index < 8; ++Index)
			{
				const FPaperFlipbookKeyFrame& Frame = Action.Clip->GetKeyFrameChecked(Index);
				UPaperSprite* Sprite = Frame.Sprite;
				const FVector2D ExpectedUV((Index % 4) * FrameSize.X, (Index / 4) * FrameSize.Y);
				if (!Sprite || Frame.FrameRun != 1 || AllSprites.Contains(Sprite)
					|| !Sprite->GetPathName().StartsWith(DogAssetDirectory)
					|| !Sprite->GetSourceSize().Equals(FrameSize)
					|| !Sprite->GetSourceUV().Equals(ExpectedUV)
					|| !(Sprite->GetPivotPosition() - Sprite->GetSourceUV()).Equals(FootPivot)
					|| !FMath::IsNearlyEqual(Sprite->GetPixelsPerUnrealUnit(), PixelsPerUnit)
					|| Sprite->GetDefaultMaterial() != OutMaterial)
				{
					Error = FString::Printf(TEXT("%s frame %d has invalid ownership, uniqueness, grid, foot pivot, density or material."), *Name, Index);
					return false;
				}
				AllSprites.Add(Sprite);
				UTexture2D* Source = Sprite->GetSourceTexture();
				// NullRHI does not build platform render data. Imported dimensions
				// describe the authored atlas even when GetSizeX/Y return zero.
				if (!Source || !Source->GetPathName().StartsWith(DogAssetDirectory)
					|| Source->GetImportedSize() != FIntPoint(2048, 768)
					|| (Texture && Source != Texture))
				{
					Error = FString::Printf(TEXT("%s must use its own new 2048x768 dog atlas."), *Name);
					return false;
				}
				Texture = Source;
			}
			if (DistinctActionTextures.Contains(Texture))
			{
				Error = TEXT("Different dog actions must not reuse the same atlas.");
				return false;
			}
			DistinctActionTextures.Add(Texture);
		}
		return true;
	}

	bool ResolveProperties(UClass* Class, TArray<FActionBinding>& Actions, FScalarBindings& Scalars, FString& Error)
	{
		for (FActionBinding& Action : Actions)
		{
			Action.ReflectedProperty = FindFProperty<FObjectPropertyBase>(Class, Action.Property);
			if (!Action.ReflectedProperty || Action.ReflectedProperty->PropertyClass != UPaperFlipbook::StaticClass())
			{
				Error = FString::Printf(TEXT("Missing or incompatible property %s."), Action.Property);
				return false;
			}
		}
		Scalars.FacesRight = FindFProperty<FBoolProperty>(Class, TEXT("bArtworkFacesRight"));
		Scalars.AlignToPlayerLane = FindFProperty<FBoolProperty>(Class, TEXT("bAlignSpriteToPlayerLane"));
		Scalars.VisualScale = FindFProperty<FFloatProperty>(Class, TEXT("VisualScale"));
		Scalars.Windup = FindFProperty<FFloatProperty>(Class, TEXT("AttackWindup"));
		Scalars.HurtDuration = FindFProperty<FFloatProperty>(Class, TEXT("HurtAnimationDuration"));
		Scalars.HealthBarHeight = FindFProperty<FFloatProperty>(Class, TEXT("HealthBarHeight"));
		if (!Scalars.FacesRight || !Scalars.AlignToPlayerLane || !Scalars.VisualScale
			|| !Scalars.Windup || !Scalars.HurtDuration || !Scalars.HealthBarHeight)
		{
			Error = TEXT("The dog Blueprint does not expose all required presentation fields.");
			return false;
		}
		return true;
	}

	bool IsBound(UObject* Defaults, const TArray<FActionBinding>& Actions, const FScalarBindings& Scalars,
		const TArray<UPaperFlipbookComponent*>& Templates, UMaterialInterface* Material, double FloorZ)
	{
		for (const FActionBinding& Action : Actions)
		{
			if (Action.ReflectedProperty->GetObjectPropertyValue_InContainer(Defaults) != Action.Clip) { return false; }
		}
		if (Scalars.FacesRight->GetPropertyValue_InContainer(Defaults)
			|| !Scalars.AlignToPlayerLane->GetPropertyValue_InContainer(Defaults)
			|| !FMath::IsNearlyEqual(Scalars.VisualScale->GetPropertyValue_InContainer(Defaults), DogVisualScale)
			|| !FMath::IsNearlyEqual(Scalars.Windup->GetPropertyValue_InContainer(Defaults), DogWindup)
			|| !FMath::IsNearlyEqual(Scalars.HurtDuration->GetPropertyValue_InContainer(Defaults), DogHurtDuration)
			|| !FMath::IsNearlyEqual(Scalars.HealthBarHeight->GetPropertyValue_InContainer(Defaults),
				DogHealthBarHeightAboveFeet + static_cast<float>(FloorZ)))
		{
			return false;
		}
		for (UPaperFlipbookComponent* Template : Templates)
		{
			if (Template->GetFlipbook() != Actions[0].Clip || Template->GetMaterial(0) != Material
				|| !Template->IsLooping() || !Template->IsPlaying() || Template->IsReversing()
				|| !FMath::IsNearlyEqual(Template->GetPlayRate(), 1.0f)
				|| !FMath::IsNearlyZero(Template->GetPlaybackPosition())
				|| !FMath::IsNearlyEqual(Template->GetRelativeLocation().Z, FloorZ)
				|| !Template->GetRelativeScale3D().Equals(FVector::OneVector))
			{
				return false;
			}
		}
		return true;
	}
}

UImmortalDogArtCommandlet::UImmortalDogArtCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UImmortalDogArtCommandlet::Main(const FString& Params)
{
	using namespace ImmortalDogArt;
	// Blueprint PostDuplicate may compile synchronously. Never allow that
	// intermediate compile to save a partially bound asset, regardless of
	// the user's editor preference; restore the in-memory preference on exit.
	TGuardValue<TEnumAsByte<ESaveOnCompile>> NoIntermediateSave(
		GetMutableDefault<UBlueprintEditorSettings>()->SaveOnCompile, SoC_Never);
	const bool bVerifyOnly = FParse::Param(*Params, TEXT("VerifyOnly"));
	const bool bCreateFromLegacy = FParse::Param(*Params, TEXT("CreateFromLegacy"));
	if (bVerifyOnly && bCreateFromLegacy)
	{
		UE_LOG(LogImmortalDogArt, Error, TEXT("VerifyOnly and CreateFromLegacy are mutually exclusive; no changes made."));
		return 1;
	}
	TArray<FActionBinding> Actions = {
		{ TEXT("Idle"), TEXT("IdleFlipbook"), 8.0f },
		{ TEXT("Move"), TEXT("MoveFlipbook"), 8.0f },
		{ TEXT("Attack"), TEXT("AttackFlipbook"), 12.0f },
		{ TEXT("Hurt"), TEXT("HurtFlipbook"), 12.0f },
		{ TEXT("Death"), TEXT("DeathFlipbook"), 10.0f }
	};
	FString Error;
	UMaterialInterface* Material = nullptr;
	// Do not create even an in-memory duplicate before the entire art set passes.
	if (!LoadAndValidateActions(Actions, Material, Error))
	{
		UE_LOG(LogImmortalDogArt, Error, TEXT("Art preflight failed; no package created or saved: %s"), *Error);
		return 1;
	}
	UBlueprint* Legacy = LoadObject<UBlueprint>(nullptr, LegacyPath);
	UClass* MonsterClass = LoadObject<UClass>(nullptr, TEXT("/Script/ImmortalPath.ImmortalMonsterCharacter"));
	if (!Legacy || !Legacy->GeneratedClass || !MonsterClass
		|| Legacy->GetOutermost()->GetName() != LegacyPackage
		|| !Legacy->GeneratedClass->IsChildOf(MonsterClass) || Legacy->Status == BS_Error)
	{
		UE_LOG(LogImmortalDogArt, Error, TEXT("The protected legacy dog Blueprint is missing or invalid; no package created or saved."));
		return 1;
	}
	FString LegacyFilename;
	FString LegacyDigest;
	if (!FPackageName::TryConvertLongPackageNameToFilename(LegacyPackage, LegacyFilename, FPackageName::GetAssetPackageExtension())
		|| !ReadPackageDigest(LegacyFilename, LegacyDigest))
	{
		UE_LOG(LogImmortalDogArt, Error, TEXT("Cannot read the protected legacy package digest; no package created or saved."));
		return 1;
	}
	APaperCharacter* LegacyDefaults = Cast<APaperCharacter>(Legacy->GeneratedClass->GetDefaultObject());
	TArray<UPaperFlipbookComponent*> LegacyTemplates;
	if (!LegacyDefaults || !LegacyDefaults->GetCapsuleComponent()
		|| !CollectSpriteTemplates(Legacy, LegacyDefaults, LegacyTemplates, Error))
	{
		UE_LOG(LogImmortalDogArt, Error, TEXT("Legacy component preflight failed; no package created or saved: %s"), *Error);
		return 1;
	}
	const float CapsuleHalfHeight = LegacyDefaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	if (!FMath::IsFinite(CapsuleHalfHeight) || CapsuleHalfHeight <= 0.0f)
	{
		UE_LOG(LogImmortalDogArt, Error, TEXT("Legacy capsule height is invalid; no package created or saved."));
		return 1;
	}
	for (UPaperFlipbookComponent* Template : LegacyTemplates)
	{
		if (!Template->GetRelativeScale3D().Equals(FVector::OneVector))
		{
			UE_LOG(LogImmortalDogArt, Error, TEXT("Legacy sprite component scale requires review; expected unit scale, no package created or saved."));
			return 1;
		}
	}
	const TMap<FString, FString> LegacyAllDefaults = CaptureEditableProperties(LegacyDefaults);
	const TMap<FString, FString> LegacyAllComponents = CaptureComponentProperties(Legacy, LegacyDefaults, false);
	const TMap<FString, FString> ExpectedOtherDefaults = CaptureEditableProperties(LegacyDefaults, PresentationProperties());
	const TMap<FString, FString> ExpectedOtherComponents = CaptureComponentProperties(Legacy, LegacyDefaults, true);
	const FTransform LegacyActorTransform = LegacyDefaults->GetActorTransform();
	const bool bLegacyDirty = Legacy->GetOutermost()->IsDirty();
	const auto IsLegacyUnchanged = [&]() -> bool
	{
		FString CurrentDigest;
		APaperCharacter* CurrentDefaults = Legacy->GeneratedClass
			? Cast<APaperCharacter>(Legacy->GeneratedClass->GetDefaultObject()) : nullptr;
		return CurrentDefaults && CurrentDefaults->GetSprite()
			&& ReadPackageDigest(LegacyFilename, CurrentDigest) && CurrentDigest == LegacyDigest
			&& Legacy->GetOutermost()->IsDirty() == bLegacyDirty
			&& CurrentDefaults->GetActorTransform().Equals(LegacyActorTransform)
			&& CompareSnapshots(LegacyAllDefaults, CaptureEditableProperties(CurrentDefaults), TEXT("Protected legacy defaults"), Error)
			&& CompareSnapshots(LegacyAllComponents, CaptureComponentProperties(Legacy, CurrentDefaults, false), TEXT("Protected legacy components"), Error);
	};

	FString Filename;
	if (!FPackageName::TryConvertLongPackageNameToFilename(BlueprintPackage, Filename, FPackageName::GetAssetPackageExtension()))
	{
		UE_LOG(LogImmortalDogArt, Error, TEXT("The fixed destination package path is invalid; no package created or saved."));
		return 1;
	}
	const bool bDestinationExists = IFileManager::Get().FileExists(*Filename);
	UBlueprint* Blueprint = nullptr;
	if (bDestinationExists)
	{
		if (!bVerifyOnly && IFileManager::Get().IsReadOnly(*Filename))
		{
			UE_LOG(LogImmortalDogArt, Error, TEXT("The fixed desktop dog package is read-only; no package saved."));
			return 1;
		}
		Blueprint = LoadObject<UBlueprint>(nullptr, BlueprintPath);
	}
	else
	{
		if (bVerifyOnly || !bCreateFromLegacy)
		{
			UE_LOG(LogImmortalDogArt, Error, TEXT("BP_DesktopDogMonster does not exist; first creation requires CreateFromLegacy. No package created or saved."));
			return 1;
		}
		if (FindPackage(nullptr, BlueprintPackage))
		{
			UE_LOG(LogImmortalDogArt, Error, TEXT("The destination has an unexpected loaded package; refusing any overwrite."));
			return 1;
		}
		// The same formal editor duplicate API used underneath AssetTools, but
		// without AssetTools' optional source-control auto-save. There can be no
		// partially bound file on disk before all post-compile checks pass.
		ObjectTools::FPackageGroupName Destination;
		Destination.PackageName = BlueprintPackage;
		Destination.GroupName = TEXT("");
		Destination.ObjectName = TEXT("BP_DesktopDogMonster");
		TSet<UPackage*> RefusedPackages;
		Blueprint = Cast<UBlueprint>(ObjectTools::DuplicateSingleObject(Legacy, Destination, RefusedPackages, false));
	}
	if (!Blueprint || !Blueprint->GeneratedClass || Blueprint->GetOutermost()->GetName() != BlueprintPackage
		|| Blueprint->ParentClass != Legacy->ParentClass
		|| !Blueprint->GeneratedClass->IsChildOf(MonsterClass) || Blueprint->Status == BS_Error
		|| !IsLegacyUnchanged())
	{
		UE_LOG(LogImmortalDogArt, Error, TEXT("Destination or protected-source validation failed; no package saved: %s"), *Error);
		return 1;
	}
	APaperCharacter* Defaults = Cast<APaperCharacter>(Blueprint->GeneratedClass->GetDefaultObject());
	FScalarBindings Scalars;
	TArray<UPaperFlipbookComponent*> Templates;
	if (!Defaults || !Defaults->GetCapsuleComponent()
		|| !ResolveProperties(Blueprint->GeneratedClass, Actions, Scalars, Error)
		|| !CollectSpriteTemplates(Blueprint, Defaults, Templates, Error)
		|| Templates.Num() != LegacyTemplates.Num()
		|| !Defaults->GetActorTransform().Equals(LegacyActorTransform)
		|| !CompareSnapshots(ExpectedOtherDefaults, CaptureEditableProperties(Defaults, PresentationProperties()), TEXT("Copied gameplay defaults"), Error)
		|| !CompareSnapshots(ExpectedOtherComponents, CaptureComponentProperties(Blueprint, Defaults, true), TEXT("Copied component defaults"), Error))
	{
		UE_LOG(LogImmortalDogArt, Error, TEXT("Desktop dog preflight failed; no package saved: %s"), *Error);
		return 1;
	}
	const double FloorZ = -static_cast<double>(CapsuleHalfHeight);
	TMap<FString, FTransform> ExpectedSpriteTransforms;
	for (UPaperFlipbookComponent* Template : LegacyTemplates)
	{
		FTransform Transform = Template->GetRelativeTransform();
		FVector Location = Transform.GetLocation();
		Location.Z = FloorZ;
		Transform.SetLocation(Location);
		ExpectedSpriteTransforms.Add(CanonicalizeIdentity(Template->GetPathName()), Transform);
	}
	const auto AreSpriteTransformsValid = [&]() -> bool
	{
		for (UPaperFlipbookComponent* Template : Templates)
		{
			const FTransform* Expected = ExpectedSpriteTransforms.Find(CanonicalizeIdentity(Template->GetPathName()));
			if (!Expected || !Template->GetRelativeTransform().Equals(*Expected)) { return false; }
		}
		return true;
	};
	if (IsBound(Defaults, Actions, Scalars, Templates, Material, FloorZ) && AreSpriteTransformsValid())
	{
		UE_LOG(LogImmortalDogArt, Display, TEXT("BP_DesktopDogMonster binding verified: five complete masked actions, %d sprite templates; legacy dog/Boss package unchanged, already up to date, no package saved."), Templates.Num());
		return 0;
	}
	if (bVerifyOnly)
	{
		UE_LOG(LogImmortalDogArt, Error, TEXT("BP_DesktopDogMonster does not match the required binding; VerifyOnly made no changes."));
		return 1;
	}
	const int32 TemplateCount = Templates.Num();
	Blueprint->Modify();
	Defaults->Modify();
	for (const FActionBinding& Action : Actions) { Action.ReflectedProperty->SetObjectPropertyValue_InContainer(Defaults, Action.Clip); }
	Scalars.FacesRight->SetPropertyValue_InContainer(Defaults, false);
	Scalars.AlignToPlayerLane->SetPropertyValue_InContainer(Defaults, true);
	Scalars.VisualScale->SetPropertyValue_InContainer(Defaults, DogVisualScale);
	Scalars.Windup->SetPropertyValue_InContainer(Defaults, DogWindup);
	Scalars.HurtDuration->SetPropertyValue_InContainer(Defaults, DogHurtDuration);
	Scalars.HealthBarHeight->SetPropertyValue_InContainer(Defaults, DogHealthBarHeightAboveFeet - CapsuleHalfHeight);
	for (UPaperFlipbookComponent* Template : Templates)
	{
		Template->Modify();
		FVector Location = Template->GetRelativeLocation();
		Location.Z = FloorZ;
		Template->SetRelativeLocation(Location);
		Template->SetFlipbook(Actions[0].Clip);
		Template->SetMaterial(0, Material);
		Template->SetLooping(true);
		Template->SetPlayRate(1.0f);
		Template->PlayFromStart();
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection | EBlueprintCompileOptions::SkipSave);
	if (Blueprint->Status != BS_UpToDate && Blueprint->Status != BS_UpToDateWithWarnings)
	{
		UE_LOG(LogImmortalDogArt, Error, TEXT("Desktop dog Blueprint compile failed; no package saved."));
		return 1;
	}
	Defaults = Blueprint->GeneratedClass ? Cast<APaperCharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
	Templates.Reset();
	if (!Defaults || !Defaults->GetCapsuleComponent()
		|| !ResolveProperties(Blueprint->GeneratedClass, Actions, Scalars, Error)
		|| !CollectSpriteTemplates(Blueprint, Defaults, Templates, Error)
		|| Templates.Num() != TemplateCount || !IsBound(Defaults, Actions, Scalars, Templates, Material, FloorZ)
		|| !AreSpriteTransformsValid() || !Defaults->GetActorTransform().Equals(LegacyActorTransform)
		|| !FMath::IsNearlyEqual(Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), CapsuleHalfHeight)
		|| !CompareSnapshots(ExpectedOtherDefaults, CaptureEditableProperties(Defaults, PresentationProperties()), TEXT("Compiled gameplay defaults"), Error)
		|| !CompareSnapshots(ExpectedOtherComponents, CaptureComponentProperties(Blueprint, Defaults, true), TEXT("Compiled component defaults"), Error)
		|| !IsLegacyUnchanged())
	{
		UE_LOG(LogImmortalDogArt, Error, TEXT("Post-compile defaults, component transforms or protected legacy validation failed; no package saved: %s"), *Error);
		return 1;
	}
	if (Blueprint->Status == BS_UpToDateWithWarnings)
	{
		UE_LOG(LogImmortalDogArt, Warning, TEXT("Desktop dog compiled with warnings; inspect the compiler log."));
	}
	UPackage* Package = Blueprint->GetOutermost();
	Package->MarkPackageDirty();
	TArray<UPackage*> PackagesToSave { Package };
	if (!UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true) || !IsLegacyUnchanged())
	{
		UE_LOG(LogImmortalDogArt, Error, TEXT("Saving the single desktop dog package or final protected-source check failed: %s"), *Error);
		return 1;
	}
	UE_LOG(LogImmortalDogArt, Display, TEXT("BP_DesktopDogMonster binding saved: five masked actions, %d sprite templates, foot pivot Z=%.3f, health baseline=%.3f. Legacy package digest %s unchanged; all copied gameplay, capsule, other components and sprite X/Y, rotation and scale preserved. Run again or VerifyOnly to check persisted binding."),
		Templates.Num(), FloorZ, DogHealthBarHeightAboveFeet - CapsuleHalfHeight, *LegacyDigest);
	return 0;
}
