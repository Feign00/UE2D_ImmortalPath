// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalProtectedMonsterArtBinding.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogImmortalProtectedMonsterArt, Log, All);

namespace ImmortalProtectedMonsterArtBinding
{
namespace
{
	const FConfig* ActiveConfig = nullptr;

	const FConfig& GetConfig()
	{
		check(ActiveConfig);
		return *ActiveConfig;
	}

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
		FBoolProperty* AuthoredMapBossPalette = nullptr;
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
		const FConfig& Config = GetConfig();
		const FString LegacyName = FPackageName::GetLongPackageAssetName(Config.LegacyPackage);
		Value.ReplaceInline(*FString::Printf(TEXT("%s.%s"), Config.BlueprintPackage, Config.BlueprintName),
			Config.LegacyPath, ESearchCase::CaseSensitive);
		Value.ReplaceInline(*FString::Printf(TEXT("%s.Default__%s"), Config.BlueprintPackage, Config.BlueprintName),
			*FString::Printf(TEXT("%s.Default__%s"), Config.LegacyPackage, *LegacyName), ESearchCase::CaseSensitive);
		Value.ReplaceInline(Config.BlueprintPackage, Config.LegacyPackage, ESearchCase::CaseSensitive);
		return Value;
	}

	TSet<FName> PresentationProperties()
	{
		TSet<FName> Result { TEXT("IdleFlipbook"), TEXT("MoveFlipbook"), TEXT("AttackFlipbook"),
			TEXT("HurtFlipbook"), TEXT("DeathFlipbook"), TEXT("bArtworkFacesRight"),
			TEXT("bAlignSpriteToPlayerLane"), TEXT("VisualScale"), TEXT("HealthBarHeight") };
		// The already-delivered Dog binding intentionally updates reaction timing.
		// Boss bindings exclude both fields, compare them as gameplay and never write them.
		if (GetConfig().bReplaceReactionTimings)
		{
			Result.Add(TEXT("AttackWindup"));
			Result.Add(TEXT("HurtAnimationDuration"));
		}
		if (GetConfig().bPreserveAuthoredMapBossPalette)
		{
			// The ordinary Dog keeps this new inherited default protected. Only the
			// dedicated map-Boss destination may opt into its authored palette.
			Result.Add(TEXT("bPreserveAuthoredMapBossPalette"));
		}
		return Result;
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
			Error = TEXT("The fixed monster Blueprint has no inherited PaperCharacter sprite.");
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
				Error = TEXT("Refusing to change a sprite template outside the fixed monster Blueprint package.");
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
			const FString Name = FString::Printf(TEXT("%s%s"), GetConfig().FlipbookPrefix, Action.Action);
			const FString Path = FString(GetConfig().AssetDirectory) + Name + TEXT(".") + Name;
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
				const FVector2D ExpectedUV((Index % 4) * GetConfig().FrameSize.X, (Index / 4) * GetConfig().FrameSize.Y);
				if (!Sprite || Frame.FrameRun != 1 || AllSprites.Contains(Sprite)
					|| !Sprite->GetPathName().StartsWith(GetConfig().AssetDirectory)
					|| !Sprite->GetSourceSize().Equals(GetConfig().FrameSize)
					|| !Sprite->GetSourceUV().Equals(ExpectedUV)
					|| !(Sprite->GetPivotPosition() - Sprite->GetSourceUV()).Equals(GetConfig().FootPivot)
					|| !FMath::IsNearlyEqual(Sprite->GetPixelsPerUnrealUnit(), GetConfig().PixelsPerUnit)
					|| Sprite->GetDefaultMaterial() != OutMaterial)
				{
					Error = FString::Printf(TEXT("%s frame %d has invalid ownership, uniqueness, grid, foot pivot, density or material."), *Name, Index);
					return false;
				}
				AllSprites.Add(Sprite);
				UTexture2D* Source = Sprite->GetSourceTexture();
				// NullRHI does not build platform render data. Imported dimensions
				// describe the authored atlas even when GetSizeX/Y return zero.
				if (!Source || !Source->GetPathName().StartsWith(GetConfig().AssetDirectory)
					|| Source->GetImportedSize() != FIntPoint(2048, 768)
					|| (Texture && Source != Texture))
				{
					Error = FString::Printf(TEXT("%s must use its own new 2048x768 action atlas."), *Name);
					return false;
				}
				Texture = Source;
			}
			if (DistinctActionTextures.Contains(Texture))
			{
				Error = TEXT("Different monster actions must not reuse the same atlas.");
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
		Scalars.AuthoredMapBossPalette = FindFProperty<FBoolProperty>(Class, TEXT("bPreserveAuthoredMapBossPalette"));
		Scalars.VisualScale = FindFProperty<FFloatProperty>(Class, TEXT("VisualScale"));
		Scalars.Windup = FindFProperty<FFloatProperty>(Class, TEXT("AttackWindup"));
		Scalars.HurtDuration = FindFProperty<FFloatProperty>(Class, TEXT("HurtAnimationDuration"));
		Scalars.HealthBarHeight = FindFProperty<FFloatProperty>(Class, TEXT("HealthBarHeight"));
		if (!Scalars.FacesRight || !Scalars.AlignToPlayerLane || !Scalars.AuthoredMapBossPalette || !Scalars.VisualScale
			|| !Scalars.Windup || !Scalars.HurtDuration || !Scalars.HealthBarHeight)
		{
			Error = TEXT("The monster Blueprint does not expose all required presentation fields.");
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
			|| Scalars.AuthoredMapBossPalette->GetPropertyValue_InContainer(Defaults) != GetConfig().bPreserveAuthoredMapBossPalette
			|| !FMath::IsNearlyEqual(Scalars.VisualScale->GetPropertyValue_InContainer(Defaults), GetConfig().VisualScale)
			|| !FMath::IsNearlyEqual(Scalars.Windup->GetPropertyValue_InContainer(Defaults), GetConfig().Windup)
			|| !FMath::IsNearlyEqual(Scalars.HurtDuration->GetPropertyValue_InContainer(Defaults), GetConfig().HurtDuration)
			|| !FMath::IsNearlyEqual(Scalars.HealthBarHeight->GetPropertyValue_InContainer(Defaults),
				GetConfig().HealthBarHeightAboveFeet + static_cast<float>(FloorZ)))
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

int32 Run(const FString& Params, const FConfig& Config)
{
	if (!IsInGameThread())
	{
		UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("Protected art binding requires the editor game thread; no package created or saved."));
		return 1;
	}
	TGuardValue<const FConfig*> ConfigGuard(ActiveConfig, &Config);
	// Blueprint PostDuplicate may compile synchronously. Never allow that
	// intermediate compile to save a partially bound asset, regardless of
	// the user's editor preference; restore the in-memory preference on exit.
	TGuardValue<TEnumAsByte<ESaveOnCompile>> NoIntermediateSave(
		GetMutableDefault<UBlueprintEditorSettings>()->SaveOnCompile, SoC_Never);
	const bool bVerifyOnly = FParse::Param(*Params, TEXT("VerifyOnly"));
	const bool bCreateFromLegacy = FParse::Param(*Params, TEXT("CreateFromLegacy"));
	if (bVerifyOnly && bCreateFromLegacy)
	{
		UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("VerifyOnly and CreateFromLegacy are mutually exclusive; no changes made."));
		return 1;
	}
	TArray<FActionBinding> Actions = {
		{ TEXT("Idle"), TEXT("IdleFlipbook"), Config.FramesPerSecond[0] },
		{ TEXT("Move"), TEXT("MoveFlipbook"), Config.FramesPerSecond[1] },
		{ TEXT("Attack"), TEXT("AttackFlipbook"), Config.FramesPerSecond[2] },
		{ TEXT("Hurt"), TEXT("HurtFlipbook"), Config.FramesPerSecond[3] },
		{ TEXT("Death"), TEXT("DeathFlipbook"), Config.FramesPerSecond[4] }
	};
	FString Error;
	UMaterialInterface* Material = nullptr;
	// Do not create even an in-memory duplicate before the entire art set passes.
	if (!LoadAndValidateActions(Actions, Material, Error))
	{
		UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("Art preflight failed; no package created or saved: %s"), *Error);
		return 1;
	}
	UBlueprint* Legacy = LoadObject<UBlueprint>(nullptr, GetConfig().LegacyPath);
	UClass* MonsterClass = LoadObject<UClass>(nullptr, TEXT("/Script/ImmortalPath.ImmortalMonsterCharacter"));
	if (!Legacy || !Legacy->GeneratedClass || !MonsterClass
		|| Legacy->GetOutermost()->GetName() != GetConfig().LegacyPackage
		|| !Legacy->GeneratedClass->IsChildOf(MonsterClass) || Legacy->Status == BS_Error)
	{
		UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("The protected legacy monster Blueprint is missing or invalid; no package created or saved."));
		return 1;
	}
	FString LegacyFilename;
	FString LegacyDigest;
	if (!FPackageName::TryConvertLongPackageNameToFilename(GetConfig().LegacyPackage, LegacyFilename, FPackageName::GetAssetPackageExtension())
		|| !ReadPackageDigest(LegacyFilename, LegacyDigest))
	{
		UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("Cannot read the protected legacy package digest; no package created or saved."));
		return 1;
	}
	APaperCharacter* LegacyDefaults = Cast<APaperCharacter>(Legacy->GeneratedClass->GetDefaultObject());
	TArray<UPaperFlipbookComponent*> LegacyTemplates;
	if (!LegacyDefaults || !LegacyDefaults->GetCapsuleComponent()
		|| !CollectSpriteTemplates(Legacy, LegacyDefaults, LegacyTemplates, Error))
	{
		UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("Legacy component preflight failed; no package created or saved: %s"), *Error);
		return 1;
	}
	if (!Config.bReplaceReactionTimings)
	{
		FScalarBindings LegacyScalars;
		if (!ResolveProperties(Legacy->GeneratedClass, Actions, LegacyScalars, Error)
			|| !FMath::IsNearlyEqual(LegacyScalars.Windup->GetPropertyValue_InContainer(LegacyDefaults), Config.Windup)
			|| !FMath::IsNearlyEqual(LegacyScalars.HurtDuration->GetPropertyValue_InContainer(LegacyDefaults), Config.HurtDuration))
		{
			UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("Protected legacy reaction timing does not match the fixed binding specification; no package created or saved: %s"), *Error);
			return 1;
		}
	}
	if (Config.bPreserveHurtClipDuration)
	{
		const FObjectPropertyBase* LegacyHurtProperty = FindFProperty<FObjectPropertyBase>(
			Legacy->GeneratedClass, TEXT("HurtFlipbook"));
		const UPaperFlipbook* LegacyHurt = LegacyHurtProperty
			? Cast<UPaperFlipbook>(LegacyHurtProperty->GetObjectPropertyValue_InContainer(LegacyDefaults))
			: nullptr;
		// GetAnimationDuration uses the actual Flipbook duration when available,
		// not HurtAnimationDuration's fallback. Protect the live animation window
		// by comparing to the loaded legacy clip, not a hard-coded frame/FPS guess.
		if (!LegacyHurt || !FMath::IsFinite(LegacyHurt->GetTotalDuration())
			|| LegacyHurt->GetTotalDuration() <= 0.0f
			|| !FMath::IsNearlyEqual(Actions[3].Clip->GetTotalDuration(), LegacyHurt->GetTotalDuration(), 0.0001f))
		{
			UE_LOG(LogImmortalProtectedMonsterArt, Error,
				TEXT("Protected Hurt clip duration mismatch: legacy=%.9f new=%.9f; no package created or saved."),
				LegacyHurt ? LegacyHurt->GetTotalDuration() : -1.0f, Actions[3].Clip->GetTotalDuration());
			return 1;
		}
		UE_LOG(LogImmortalProtectedMonsterArt, Display,
			TEXT("Protected Hurt animation window verified: legacy=%.9f new=%.9f; timing fallback remains %.3f."),
			LegacyHurt->GetTotalDuration(), Actions[3].Clip->GetTotalDuration(), Config.HurtDuration);
	}
	const float CapsuleHalfHeight = LegacyDefaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	if (!FMath::IsFinite(CapsuleHalfHeight) || CapsuleHalfHeight <= 0.0f)
	{
		UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("Legacy capsule height is invalid; no package created or saved."));
		return 1;
	}
	for (UPaperFlipbookComponent* Template : LegacyTemplates)
	{
		if (!Template->GetRelativeScale3D().Equals(FVector::OneVector))
		{
			UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("Legacy sprite component scale requires review; expected unit scale, no package created or saved."));
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
	if (!FPackageName::TryConvertLongPackageNameToFilename(GetConfig().BlueprintPackage, Filename, FPackageName::GetAssetPackageExtension()))
	{
		UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("The fixed destination package path is invalid; no package created or saved."));
		return 1;
	}
	const bool bDestinationExists = IFileManager::Get().FileExists(*Filename);
	UBlueprint* Blueprint = nullptr;
	if (bDestinationExists)
	{
		if (!bVerifyOnly && IFileManager::Get().IsReadOnly(*Filename))
		{
			UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("The fixed destination monster package is read-only; no package saved."));
			return 1;
		}
		Blueprint = LoadObject<UBlueprint>(nullptr, GetConfig().BlueprintPath);
	}
	else
	{
		if (bVerifyOnly || !bCreateFromLegacy)
		{
			UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("%s does not exist; first creation requires CreateFromLegacy. No package created or saved."), Config.BlueprintName);
			return 1;
		}
		if (FindPackage(nullptr, GetConfig().BlueprintPackage))
		{
			UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("The destination has an unexpected loaded package; refusing any overwrite."));
			return 1;
		}
		// The same formal editor duplicate API used underneath AssetTools, but
		// without AssetTools' optional source-control auto-save. There can be no
		// partially bound file on disk before all post-compile checks pass.
		ObjectTools::FPackageGroupName Destination;
		Destination.PackageName = GetConfig().BlueprintPackage;
		Destination.GroupName = TEXT("");
		Destination.ObjectName = Config.BlueprintName;
		TSet<UPackage*> RefusedPackages;
		Blueprint = Cast<UBlueprint>(ObjectTools::DuplicateSingleObject(Legacy, Destination, RefusedPackages, false));
	}
	if (!Blueprint || !Blueprint->GeneratedClass || Blueprint->GetOutermost()->GetName() != GetConfig().BlueprintPackage
		|| Blueprint->ParentClass != Legacy->ParentClass
		|| !Blueprint->GeneratedClass->IsChildOf(MonsterClass) || Blueprint->Status == BS_Error
		|| !IsLegacyUnchanged())
	{
		UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("Destination or protected-source validation failed; no package saved: %s"), *Error);
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
		UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("Destination monster preflight failed; no package saved: %s"), *Error);
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
		UE_LOG(LogImmortalProtectedMonsterArt, Display, TEXT("%s binding verified: five complete masked actions, %d sprite templates; legacy dog/Boss package unchanged, already up to date, no package saved."), Config.BlueprintName, Templates.Num());
		return 0;
	}
	if (bVerifyOnly)
	{
		UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("%s does not match the required binding; VerifyOnly made no changes."), Config.BlueprintName);
		return 1;
	}
	const int32 TemplateCount = Templates.Num();
	Blueprint->Modify();
	Defaults->Modify();
	for (const FActionBinding& Action : Actions) { Action.ReflectedProperty->SetObjectPropertyValue_InContainer(Defaults, Action.Clip); }
	Scalars.FacesRight->SetPropertyValue_InContainer(Defaults, false);
	Scalars.AlignToPlayerLane->SetPropertyValue_InContainer(Defaults, true);
	if (Config.bPreserveAuthoredMapBossPalette)
	{
		Scalars.AuthoredMapBossPalette->SetPropertyValue_InContainer(Defaults, true);
	}
	Scalars.VisualScale->SetPropertyValue_InContainer(Defaults, GetConfig().VisualScale);
	if (Config.bReplaceReactionTimings)
	{
		Scalars.Windup->SetPropertyValue_InContainer(Defaults, Config.Windup);
		Scalars.HurtDuration->SetPropertyValue_InContainer(Defaults, Config.HurtDuration);
	}
	Scalars.HealthBarHeight->SetPropertyValue_InContainer(Defaults, GetConfig().HealthBarHeightAboveFeet - CapsuleHalfHeight);
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
		UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("Destination monster Blueprint compile failed; no package saved."));
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
		UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("Post-compile defaults, component transforms or protected legacy validation failed; no package saved: %s"), *Error);
		return 1;
	}
	if (Blueprint->Status == BS_UpToDateWithWarnings)
	{
		UE_LOG(LogImmortalProtectedMonsterArt, Warning, TEXT("Destination monster compiled with warnings; inspect the compiler log."));
	}
	UPackage* Package = Blueprint->GetOutermost();
	Package->MarkPackageDirty();
	TArray<UPackage*> PackagesToSave { Package };
	if (!UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true) || !IsLegacyUnchanged())
	{
		UE_LOG(LogImmortalProtectedMonsterArt, Error, TEXT("Saving the single destination monster package or final protected-source check failed: %s"), *Error);
		return 1;
	}
	UE_LOG(LogImmortalProtectedMonsterArt, Display, TEXT("%s binding saved: five masked actions, %d sprite templates, foot pivot Z=%.3f, health baseline=%.3f. Legacy package digest %s unchanged; all copied gameplay, capsule, other components and sprite X/Y, rotation and scale preserved. Run again or VerifyOnly to check persisted binding."),
		Config.BlueprintName, Templates.Num(), FloorZ, Config.HealthBarHeightAboveFeet - CapsuleHalfHeight, *LegacyDigest);
	return 0;
}

} // namespace ImmortalProtectedMonsterArtBinding
