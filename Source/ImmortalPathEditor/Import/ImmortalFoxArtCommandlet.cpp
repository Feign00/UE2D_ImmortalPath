// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalFoxArtCommandlet.h"

#include "Components/CapsuleComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "PaperCharacter.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImmortalFoxArtCommandlet)

DEFINE_LOG_CATEGORY_STATIC(LogImmortalFoxArt, Log, All);

namespace
{
	constexpr const TCHAR* BlueprintPath = TEXT("/Game/GAME/BP_Monster.BP_Monster");
	constexpr const TCHAR* BlueprintPackage = TEXT("/Game/GAME/BP_Monster");
	constexpr const TCHAR* FoxAssetDirectory = TEXT("/Game/GAME/Asset/Monsters/desktop_pixel_v2/fox/");
	const FVector2D FrameSize(512.0, 384.0);
	const FVector2D FootPivot(256.0, 336.0);
	constexpr float PixelsPerUnit = 2.56f;
	constexpr float FoxWindup = 0.25f;
	constexpr float FoxHurtDuration = 8.0f / 12.0f;
	constexpr float FoxVisualScale = 2.0f;
	// The ~85-unit silhouette at the scene's 2x scale is ~170 units tall.
	// This is the physical-floor baseline; runtime adds the reference player's
	// actual visual ground offset without modifying collisions or the camera.
	constexpr float FoxHealthBarHeightAboveFeet = 185.0f;

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

	bool LoadAndValidateActions(TArray<FActionBinding>& Actions, UMaterialInterface*& OutMaterial, FString& Error)
	{
		OutMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Paper2D/MaskedUnlitSpriteMaterial.MaskedUnlitSpriteMaterial"));
		if (!OutMaterial || OutMaterial->GetBlendMode() != BLEND_Masked)
		{
			Error = TEXT("The canonical Paper2D masked material could not be loaded.");
			return false;
		}
		for (FActionBinding& Action : Actions)
		{
			const FString Name = FString::Printf(TEXT("FB_Fox_Pixel_%s"), Action.Action);
			const FString Path = FString(FoxAssetDirectory) + Name + TEXT(".") + Name;
			Action.Clip = LoadObject<UPaperFlipbook>(nullptr, *Path);
			if (!Action.Clip || Action.Clip->GetNumKeyFrames() != 8
				|| Action.Clip->GetNumFrames() != 8
				|| !FMath::IsNearlyEqual(Action.Clip->GetFramesPerSecond(), Action.FramesPerSecond)
				|| Action.Clip->GetDefaultMaterial() != OutMaterial)
			{
				Error = FString::Printf(TEXT("%s is missing or does not have eight single-run frames, the required FPS and masked material."), *Path);
				return false;
			}
			TSet<UPaperSprite*> DistinctSprites;
			UTexture2D* Texture = nullptr;
			for (int32 Index = 0; Index < 8; ++Index)
			{
				const FPaperFlipbookKeyFrame& Frame = Action.Clip->GetKeyFrameChecked(Index);
				UPaperSprite* Sprite = Frame.Sprite;
				const FVector2D ExpectedUV((Index % 4) * FrameSize.X, (Index / 4) * FrameSize.Y);
				if (!Sprite || Frame.FrameRun != 1 || DistinctSprites.Contains(Sprite)
					|| !Sprite->GetPathName().StartsWith(FoxAssetDirectory)
					|| !Sprite->GetSourceSize().Equals(FrameSize)
					|| !Sprite->GetSourceUV().Equals(ExpectedUV)
					|| !(Sprite->GetPivotPosition() - Sprite->GetSourceUV()).Equals(FootPivot)
					|| !FMath::IsNearlyEqual(Sprite->GetPixelsPerUnrealUnit(), PixelsPerUnit)
					|| Sprite->GetDefaultMaterial() != OutMaterial)
				{
					Error = FString::Printf(TEXT("%s frame %d has an invalid sprite, grid, local pivot, density or material."), *Name, Index);
					return false;
				}
				DistinctSprites.Add(Sprite);
				UTexture2D* Source = Sprite->GetSourceTexture();
				// Commandlets with NullRHI intentionally do not build platform
				// texture data. Validate authored import dimensions, not the
				// zero/placeholder render-resource size returned by GetSizeX/Y.
				if (!Source || !Source->GetPathName().StartsWith(FoxAssetDirectory)
					|| Source->GetImportedSize() != FIntPoint(2048, 768)
					|| (Texture && Source != Texture))
				{
					const FIntPoint Size = Source ? Source->GetImportedSize() : FIntPoint::ZeroValue;
					Error = FString::Printf(TEXT("%s must use one new 2048x768 fox atlas; actual source=%s imported=%dx%d."),
						*Name, Source ? *Source->GetPathName() : TEXT("none"), Size.X, Size.Y);
					return false;
				}
				Texture = Source;
			}
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
				Error = FString::Printf(TEXT("Missing or incompatible Blueprint property %s."), Action.Property);
				return false;
			}
		}
		Scalars.FacesRight = FindFProperty<FBoolProperty>(Class, TEXT("bArtworkFacesRight"));
		Scalars.AlignToPlayerLane = FindFProperty<FBoolProperty>(Class, TEXT("bAlignSpriteToPlayerLane"));
		Scalars.VisualScale = FindFProperty<FFloatProperty>(Class, TEXT("VisualScale"));
		Scalars.Windup = FindFProperty<FFloatProperty>(Class, TEXT("AttackWindup"));
		Scalars.HurtDuration = FindFProperty<FFloatProperty>(Class, TEXT("HurtAnimationDuration"));
		Scalars.HealthBarHeight = FindFProperty<FFloatProperty>(Class, TEXT("HealthBarHeight"));
		if (!Scalars.FacesRight || !Scalars.AlignToPlayerLane || !Scalars.VisualScale || !Scalars.Windup || !Scalars.HurtDuration || !Scalars.HealthBarHeight)
		{
			Error = TEXT("The Blueprint does not expose all required fox presentation fields.");
			return false;
		}
		return true;
	}

	bool CollectSpriteTemplates(UBlueprint* Blueprint, APaperCharacter* Defaults,
		TArray<UPaperFlipbookComponent*>& OutTemplates, FString& Error)
	{
		UPaperFlipbookComponent* NativeSprite = Defaults ? Defaults->GetSprite() : nullptr;
		if (!NativeSprite)
		{
			Error = TEXT("BP_Monster has no inherited PaperCharacter sprite.");
			return false;
		}
		const FName SpriteName = NativeSprite->GetFName();
		OutTemplates.AddUnique(NativeSprite);
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
		// Never create an override or modify a parent Blueprint's template.
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
				Error = TEXT("Refusing to change a sprite template outside BP_Monster's package.");
				return false;
			}
		}
		return true;
	}

	bool IsBound(UObject* Defaults, const TArray<FActionBinding>& Actions, const FScalarBindings& Scalars,
		const TArray<UPaperFlipbookComponent*>& Templates, UMaterialInterface* Material, double FloorZ)
	{
		for (const FActionBinding& Action : Actions)
		{
			if (Action.ReflectedProperty->GetObjectPropertyValue_InContainer(Defaults) != Action.Clip)
			{
				return false;
			}
		}
		if (Scalars.FacesRight->GetPropertyValue_InContainer(Defaults)
			|| !Scalars.AlignToPlayerLane->GetPropertyValue_InContainer(Defaults)
			|| !FMath::IsNearlyEqual(Scalars.VisualScale->GetPropertyValue_InContainer(Defaults), FoxVisualScale)
			|| !FMath::IsNearlyEqual(Scalars.Windup->GetPropertyValue_InContainer(Defaults), FoxWindup)
			|| !FMath::IsNearlyEqual(Scalars.HurtDuration->GetPropertyValue_InContainer(Defaults), FoxHurtDuration)
			|| !FMath::IsNearlyEqual(Scalars.HealthBarHeight->GetPropertyValue_InContainer(Defaults),
				FoxHealthBarHeightAboveFeet + static_cast<float>(FloorZ)))
		{
			return false;
		}
		for (UPaperFlipbookComponent* Template : Templates)
		{
			if (Template->GetFlipbook() != Actions[0].Clip || Template->GetMaterial(0) != Material
				|| !Template->IsLooping() || !Template->IsPlaying() || Template->IsReversing()
				|| !FMath::IsNearlyEqual(Template->GetPlayRate(), 1.0f)
				|| !FMath::IsNearlyZero(Template->GetPlaybackPosition())
				|| !FMath::IsNearlyEqual(Template->GetRelativeLocation().Z, FloorZ))
			{
				return false;
			}
		}
		return true;
	}

	TMap<FName, FString> CaptureOtherDefaults(UObject* Defaults, UClass* MonsterClass,
		const TArray<FActionBinding>& Actions, const FScalarBindings& Scalars)
	{
		TSet<const FProperty*> Allowed;
		for (const FActionBinding& Action : Actions) { Allowed.Add(Action.ReflectedProperty); }
		Allowed.Add(Scalars.FacesRight);
		Allowed.Add(Scalars.AlignToPlayerLane);
		Allowed.Add(Scalars.VisualScale);
		Allowed.Add(Scalars.Windup);
		Allowed.Add(Scalars.HurtDuration);
		Allowed.Add(Scalars.HealthBarHeight);
		TMap<FName, FString> Result;
		for (TFieldIterator<FProperty> It(MonsterClass, EFieldIterationFlags::None); It; ++It)
		{
			const FProperty* Property = *It;
			if (Property->HasAnyPropertyFlags(CPF_Edit)
				&& !Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)
				&& !Allowed.Contains(Property))
			{
				FString Value;
				Property->ExportText_InContainer(0, Value, Defaults, nullptr, Defaults, PPF_None);
				Result.Add(Property->GetFName(), MoveTemp(Value));
			}
		}
		return Result;
	}
}

UImmortalFoxArtCommandlet::UImmortalFoxArtCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UImmortalFoxArtCommandlet::Main(const FString& Params)
{
	const bool bVerifyOnly = FParse::Param(*Params, TEXT("VerifyOnly"));
	TArray<FActionBinding> Actions = {
		{ TEXT("Idle"), TEXT("IdleFlipbook"), 8.0f },
		{ TEXT("Move"), TEXT("MoveFlipbook"), 8.0f },
		{ TEXT("Attack"), TEXT("AttackFlipbook"), 12.0f },
		{ TEXT("Hurt"), TEXT("HurtFlipbook"), 12.0f },
		{ TEXT("Death"), TEXT("DeathFlipbook"), 10.0f }
	};
	FString Error;
	UMaterialInterface* Material = nullptr;
	if (!LoadAndValidateActions(Actions, Material, Error))
	{
		UE_LOG(LogImmortalFoxArt, Error, TEXT("Preflight failed; no package saved: %s"), *Error);
		return 1;
	}
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, BlueprintPath);
	UClass* MonsterClass = LoadObject<UClass>(nullptr, TEXT("/Script/ImmortalPath.ImmortalMonsterCharacter"));
	if (!Blueprint || !Blueprint->GeneratedClass || !MonsterClass
		|| Blueprint->GetOutermost()->GetName() != BlueprintPackage
		|| !Blueprint->GeneratedClass->IsChildOf(MonsterClass)
		|| Blueprint->Status == BS_Error)
	{
		UE_LOG(LogImmortalFoxArt, Error, TEXT("The fixed BP_Monster Blueprint is missing, invalid or has compiler errors; no package saved."));
		return 1;
	}
	FString Filename;
	if (!FPackageName::TryConvertLongPackageNameToFilename(BlueprintPackage, Filename, FPackageName::GetAssetPackageExtension())
		|| !IFileManager::Get().FileExists(*Filename) || IFileManager::Get().IsReadOnly(*Filename))
	{
		UE_LOG(LogImmortalFoxArt, Error, TEXT("BP_Monster must already exist as a writable package; no package saved."));
		return 1;
	}
	APaperCharacter* Defaults = Cast<APaperCharacter>(Blueprint->GeneratedClass->GetDefaultObject());
	FScalarBindings Scalars;
	TArray<UPaperFlipbookComponent*> Templates;
	if (!Defaults || !ResolveProperties(Blueprint->GeneratedClass, Actions, Scalars, Error)
		|| !CollectSpriteTemplates(Blueprint, Defaults, Templates, Error))
	{
		UE_LOG(LogImmortalFoxArt, Error, TEXT("Blueprint preflight failed; no package saved: %s"), *Error);
		return 1;
	}
	const UCapsuleComponent* Capsule = Defaults->GetCapsuleComponent();
	const float CapsuleHalfHeight = Capsule ? Capsule->GetUnscaledCapsuleHalfHeight() : 0.0f;
	if (!Capsule || !FMath::IsFinite(CapsuleHalfHeight) || CapsuleHalfHeight <= 0.0f)
	{
		UE_LOG(LogImmortalFoxArt, Error, TEXT("BP_Monster has an invalid capsule height; no package saved."));
		return 1;
	}
	const double FloorZ = -static_cast<double>(CapsuleHalfHeight);
	for (UPaperFlipbookComponent* Template : Templates)
	{
		if (!Template->GetRelativeScale3D().Equals(FVector::OneVector))
		{
			UE_LOG(LogImmortalFoxArt, Error,
				TEXT("Refusing to bind before the existing sprite scale is reviewed: %s scale=%s; expected (1,1,1), no package saved."),
				*Template->GetPathName(), *Template->GetRelativeScale3D().ToString());
			return 1;
		}
	}
	if (IsBound(Defaults, Actions, Scalars, Templates, Material, FloorZ))
	{
		UE_LOG(LogImmortalFoxArt, Display, TEXT("BP_Monster fox binding verified: five complete masked actions and %d sprite templates; already up to date, no package saved."), Templates.Num());
		return 0;
	}
	if (bVerifyOnly)
	{
		UE_LOG(LogImmortalFoxArt, Error, TEXT("BP_Monster does not yet match the required fox binding; VerifyOnly made no changes."));
		return 1;
	}

	const TMap<FName, FString> OtherDefaults = CaptureOtherDefaults(Defaults, MonsterClass, Actions, Scalars);
	const int32 TemplateCount = Templates.Num();
	TMap<FName, FTransform> OriginalTransforms;
	for (UPaperFlipbookComponent* Template : Templates) { OriginalTransforms.Add(Template->GetFName(), Template->GetRelativeTransform()); }
	Blueprint->Modify();
	Defaults->Modify();
	for (const FActionBinding& Action : Actions) { Action.ReflectedProperty->SetObjectPropertyValue_InContainer(Defaults, Action.Clip); }
	Scalars.FacesRight->SetPropertyValue_InContainer(Defaults, false);
	Scalars.AlignToPlayerLane->SetPropertyValue_InContainer(Defaults, true);
	Scalars.VisualScale->SetPropertyValue_InContainer(Defaults, FoxVisualScale);
	Scalars.Windup->SetPropertyValue_InContainer(Defaults, FoxWindup);
	Scalars.HurtDuration->SetPropertyValue_InContainer(Defaults, FoxHurtDuration);
	Scalars.HealthBarHeight->SetPropertyValue_InContainer(Defaults, FoxHealthBarHeightAboveFeet - CapsuleHalfHeight);
	for (UPaperFlipbookComponent* Template : Templates)
	{
		Template->Modify();
		// The artwork's custom pivot is its authored ground contact. Move only
		// that pivot to the capsule floor; preserve horizontal/depth placement,
		// rotation and the explicitly preflighted unit component scale.
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
		UE_LOG(LogImmortalFoxArt, Error, TEXT("BP_Monster compile failed; no package saved."));
		return 1;
	}
	Defaults = Blueprint->GeneratedClass ? Cast<APaperCharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
	Templates.Reset();
	if (!Defaults || !ResolveProperties(Blueprint->GeneratedClass, Actions, Scalars, Error)
		|| !CollectSpriteTemplates(Blueprint, Defaults, Templates, Error)
		|| Templates.Num() != TemplateCount || !IsBound(Defaults, Actions, Scalars, Templates, Material, FloorZ)
		|| !Defaults->GetCapsuleComponent()
		|| !FMath::IsNearlyEqual(Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), CapsuleHalfHeight))
	{
		UE_LOG(LogImmortalFoxArt, Error, TEXT("Compiled fox defaults/templates failed validation; no package saved: %s"), *Error);
		return 1;
	}
	const TMap<FName, FString> NewOtherDefaults = CaptureOtherDefaults(Defaults, MonsterClass, Actions, Scalars);
	for (const auto& Entry : OtherDefaults)
	{
		const FString* Value = NewOtherDefaults.Find(Entry.Key);
		if (!Value || *Value != Entry.Value)
		{
			UE_LOG(LogImmortalFoxArt, Error, TEXT("Unrelated Monster default %s changed during compilation; no package saved."), *Entry.Key.ToString());
			return 1;
		}
	}
	for (UPaperFlipbookComponent* Template : Templates)
	{
		const FTransform* Transform = OriginalTransforms.Find(Template->GetFName());
		if (!Transform)
		{
			UE_LOG(LogImmortalFoxArt, Error, TEXT("A sprite template changed identity during compilation; no package saved."));
			return 1;
		}
		FTransform ExpectedTransform = *Transform;
		FVector ExpectedLocation = ExpectedTransform.GetLocation();
		ExpectedLocation.Z = FloorZ;
		ExpectedTransform.SetLocation(ExpectedLocation);
		if (!Template->GetRelativeTransform().Equals(ExpectedTransform))
		{
			UE_LOG(LogImmortalFoxArt, Error, TEXT("A sprite X/Y, rotation or scale changed, or its foot anchor no longer matches the capsule floor; no package saved."));
			return 1;
		}
	}
	if (Blueprint->Status == BS_UpToDateWithWarnings)
	{
		UE_LOG(LogImmortalFoxArt, Warning, TEXT("BP_Monster compiled with warnings; inspect the compiler log."));
	}
	UPackage* Package = Blueprint->GetOutermost();
	Package->MarkPackageDirty();
	TArray<UPackage*> PackagesToSave { Package };
	if (!UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true))
	{
		UE_LOG(LogImmortalFoxArt, Error, TEXT("Saving the single BP_Monster package failed."));
		return 1;
	}
	UE_LOG(LogImmortalFoxArt, Display, TEXT("BP_Monster fox binding saved: five masked actions, %d sprite templates, foot pivot Z=%.3f, health bar height=%.3f; other Monster defaults and sprite X/Y, rotation and scale unchanged. Run again or use -VerifyOnly to verify the persisted binding."), Templates.Num(), FloorZ, FoxHealthBarHeightAboveFeet - CapsuleHalfHeight);
	return 0;
}
