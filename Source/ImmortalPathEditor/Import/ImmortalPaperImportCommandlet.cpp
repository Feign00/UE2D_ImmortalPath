// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPaperImportCommandlet.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImmortalPaperImportCommandlet)

DEFINE_LOG_CATEGORY_STATIC(LogImmortalPaperImport, Log, All);

namespace
{
	enum class EImmortalPaperTextureUsage : uint8
	{
		Background,
		Sprite
	};

	enum class EImmortalPaperPivotMode : uint8
	{
		CenterCenter,
		BottomCenter,
		AlphaBottomCenter,
		Custom
	};

	enum class EImmortalPaperMaterialMode : uint8
	{
		Opaque,
		Masked,
		Translucent
	};

	struct FImmortalPaperImportGroup
	{
		FString GroupName;
		bool bEnabled = true;
		TArray<FString> Filenames;
		FString DestinationPath;
		FString DestinationName;
		TArray<FString> DestinationNames;
		FString FactoryName = TEXT("TextureFactory");
		bool bReplaceExisting = true;
		bool bCreateSprites = false;
		bool bSpriteSheet = false;
		FString SpriteDestinationPath;
		FString SpriteName;
		TArray<FString> SpriteNames;
		int32 SheetColumns = 1;
		int32 SheetRows = 1;
		int32 SheetFrameCount = 0;
		float PixelsPerUnrealUnit = 1.0f;
		bool bHasSourceUV = false;
		FIntPoint SourceUV = FIntPoint::ZeroValue;
		bool bHasSourceDimension = false;
		FIntPoint SourceDimension = FIntPoint::ZeroValue;
		bool bHasCustomPivot = false;
		FVector2D CustomPivot = FVector2D::ZeroVector;
		EImmortalPaperTextureUsage TextureUsage =
			EImmortalPaperTextureUsage::Sprite;
		EImmortalPaperPivotMode PivotMode =
			EImmortalPaperPivotMode::BottomCenter;
		EImmortalPaperMaterialMode MaterialMode =
			EImmortalPaperMaterialMode::Masked;
		int32 ExpectedFrameCount = 0;
		int32 ExpectedWidth = 0;
		int32 ExpectedHeight = 0;
	};

	struct FImmortalPaperFlipbookGroup
	{
		FString GroupName;
		bool bEnabled = true;
		FString SourceImportGroup;
		FString DestinationPath;
		FString AssetName;
		float FramesPerSecond = 12.0f;
		int32 FrameRun = 1;
	};

	struct FImmortalPaperImportedGroup
	{
		TArray<UTexture2D*> Textures;
		TArray<UPaperSprite*> Sprites;
		UMaterialInterface* Material = nullptr;
	};

	FString NormalizeLongPackagePath(FString Path)
	{
		Path.TrimStartAndEndInline();
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (Path.EndsWith(TEXT("/")))
		{
			Path.LeftChopInline(1, EAllowShrinking::No);
		}
		return Path;
	}

	FString JoinLongPackagePath(const FString& Parent, const FString& Child)
	{
		return NormalizeLongPackagePath(Parent) + TEXT("/") + Child;
	}

	FString MakeObjectPath(const FString& PackagePath, const FString& AssetName)
	{
		return JoinLongPackagePath(PackagePath, AssetName)
			+ TEXT(".") + AssetName;
	}

	bool ReadStringArray(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		TArray<FString>& OutValues)
	{
		OutValues.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->TryGetArrayField(FieldName, Values))
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString StringValue;
			if (!Value.IsValid() || !Value->TryGetString(StringValue))
			{
				return false;
			}
			OutValues.Add(MoveTemp(StringValue));
		}
		return true;
	}

	bool ReadOptionalBool(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		const bool DefaultValue)
	{
		bool Value = DefaultValue;
		Object->TryGetBoolField(FieldName, Value);
		return Value;
	}

	int32 ReadOptionalInt(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		const int32 DefaultValue)
	{
		double Value = static_cast<double>(DefaultValue);
		Object->TryGetNumberField(FieldName, Value);
		return FMath::RoundToInt(Value);
	}

	float ReadOptionalFloat(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		const float DefaultValue)
	{
		double Value = static_cast<double>(DefaultValue);
		Object->TryGetNumberField(FieldName, Value);
		return static_cast<float>(Value);
	}

	bool ReadOptionalIntPoint(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		bool& bOutSpecified,
		FIntPoint& OutValue,
		FString& OutError)
	{
		bOutSpecified = Object->HasField(FieldName);
		if (!bOutSpecified)
		{
			return true;
		}

		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->TryGetArrayField(FieldName, Values)
			|| Values->Num() != 2
			|| !(*Values)[0].IsValid()
			|| !(*Values)[1].IsValid()
			|| !(*Values)[0]->TryGetNumber(OutValue.X)
			|| !(*Values)[1]->TryGetNumber(OutValue.Y))
		{
			OutError = FString::Printf(
				TEXT("Field '%s' must be an array of two integers."),
				FieldName);
			return false;
		}
		return true;
	}

	bool ReadOptionalVector2D(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		bool& bOutSpecified,
		FVector2D& OutValue,
		FString& OutError)
	{
		bOutSpecified = Object->HasField(FieldName);
		if (!bOutSpecified)
		{
			return true;
		}

		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		double X = 0.0;
		double Y = 0.0;
		if (!Object->TryGetArrayField(FieldName, Values)
			|| Values->Num() != 2
			|| !(*Values)[0].IsValid()
			|| !(*Values)[1].IsValid()
			|| !(*Values)[0]->TryGetNumber(X)
			|| !(*Values)[1]->TryGetNumber(Y)
			|| !FMath::IsFinite(X)
			|| !FMath::IsFinite(Y))
		{
			OutError = FString::Printf(
				TEXT("Field '%s' must be an array of two finite numbers."),
				FieldName);
			return false;
		}
		OutValue = FVector2D(X, Y);
		return true;
	}

	bool ParseTextureUsage(
		const FString& Value,
		EImmortalPaperTextureUsage& OutUsage)
	{
		if (Value.Equals(TEXT("Background"), ESearchCase::IgnoreCase))
		{
			OutUsage = EImmortalPaperTextureUsage::Background;
			return true;
		}
		if (Value.Equals(TEXT("Sprite"), ESearchCase::IgnoreCase))
		{
			OutUsage = EImmortalPaperTextureUsage::Sprite;
			return true;
		}
		return false;
	}

	bool ParsePivotMode(
		const FString& Value,
		EImmortalPaperPivotMode& OutPivotMode)
	{
		if (Value.Equals(TEXT("CenterCenter"), ESearchCase::IgnoreCase))
		{
			OutPivotMode = EImmortalPaperPivotMode::CenterCenter;
			return true;
		}
		if (Value.Equals(TEXT("BottomCenter"), ESearchCase::IgnoreCase))
		{
			OutPivotMode = EImmortalPaperPivotMode::BottomCenter;
			return true;
		}
		if (Value.Equals(TEXT("AlphaBottomCenter"), ESearchCase::IgnoreCase))
		{
			OutPivotMode = EImmortalPaperPivotMode::AlphaBottomCenter;
			return true;
		}
		if (Value.Equals(TEXT("Custom"), ESearchCase::IgnoreCase))
		{
			OutPivotMode = EImmortalPaperPivotMode::Custom;
			return true;
		}
		return false;
	}

	bool ParseMaterialMode(
		const FString& Value,
		EImmortalPaperMaterialMode& OutMaterialMode)
	{
		if (Value.Equals(TEXT("Opaque"), ESearchCase::IgnoreCase))
		{
			OutMaterialMode = EImmortalPaperMaterialMode::Opaque;
			return true;
		}
		if (Value.Equals(TEXT("Masked"), ESearchCase::IgnoreCase))
		{
			OutMaterialMode = EImmortalPaperMaterialMode::Masked;
			return true;
		}
		if (Value.Equals(TEXT("Translucent"), ESearchCase::IgnoreCase))
		{
			OutMaterialMode = EImmortalPaperMaterialMode::Translucent;
			return true;
		}
		return false;
	}

	bool ParseImportGroup(
		const TSharedPtr<FJsonObject>& Object,
		FImmortalPaperImportGroup& OutGroup,
		FString& OutError)
	{
		if (!Object.IsValid())
		{
			OutError = TEXT("ImportGroups contains a non-object entry.");
			return false;
		}

		Object->TryGetStringField(TEXT("GroupName"), OutGroup.GroupName);
		OutGroup.bEnabled =
			ReadOptionalBool(Object, TEXT("bEnabled"), true);
		ReadStringArray(Object, TEXT("Filenames"), OutGroup.Filenames);
		Object->TryGetStringField(
			TEXT("DestinationPath"), OutGroup.DestinationPath);
		Object->TryGetStringField(
			TEXT("DestinationName"), OutGroup.DestinationName);
		ReadStringArray(
			Object, TEXT("DestinationNames"), OutGroup.DestinationNames);
		Object->TryGetStringField(
			TEXT("FactoryName"), OutGroup.FactoryName);
		if (OutGroup.FactoryName.IsEmpty())
		{
			OutGroup.FactoryName = TEXT("TextureFactory");
		}
		OutGroup.bReplaceExisting =
			ReadOptionalBool(Object, TEXT("bReplaceExisting"), true);
		OutGroup.bCreateSprites =
			ReadOptionalBool(Object, TEXT("bCreateSprites"), false);
		OutGroup.bSpriteSheet =
			ReadOptionalBool(Object, TEXT("bSpriteSheet"), false);
		Object->TryGetStringField(
			TEXT("SpriteDestinationPath"),
			OutGroup.SpriteDestinationPath);
		Object->TryGetStringField(
			TEXT("SpriteName"), OutGroup.SpriteName);
		ReadStringArray(
			Object, TEXT("SpriteNames"), OutGroup.SpriteNames);
		OutGroup.PixelsPerUnrealUnit = ReadOptionalFloat(
			Object, TEXT("PixelsPerUnrealUnit"), 1.0f);
		if (!ReadOptionalIntPoint(
				Object,
				TEXT("SourceUV"),
				OutGroup.bHasSourceUV,
				OutGroup.SourceUV,
				OutError)
			|| !ReadOptionalIntPoint(
				Object,
				TEXT("SourceDimension"),
				OutGroup.bHasSourceDimension,
				OutGroup.SourceDimension,
				OutError)
			|| !ReadOptionalVector2D(
				Object,
				TEXT("CustomPivot"),
				OutGroup.bHasCustomPivot,
				OutGroup.CustomPivot,
				OutError))
		{
			OutError = FString::Printf(
				TEXT("Import group '%s': %s"),
				*OutGroup.GroupName,
				*OutError);
			return false;
		}
		OutGroup.ExpectedFrameCount = ReadOptionalInt(
			Object, TEXT("ExpectedFrameCount"), 0);
		OutGroup.SheetColumns = ReadOptionalInt(
			Object, TEXT("SheetColumns"), 1);
		OutGroup.SheetRows = ReadOptionalInt(
			Object, TEXT("SheetRows"), 1);
		OutGroup.SheetFrameCount = ReadOptionalInt(
			Object, TEXT("SheetFrameCount"), 0);
		OutGroup.ExpectedWidth = ReadOptionalInt(
			Object, TEXT("ExpectedWidth"), 0);
		OutGroup.ExpectedHeight = ReadOptionalInt(
			Object, TEXT("ExpectedHeight"), 0);

		FString TextureUsage(TEXT("Sprite"));
		Object->TryGetStringField(TEXT("TextureUsage"), TextureUsage);
		if (!ParseTextureUsage(TextureUsage, OutGroup.TextureUsage))
		{
			OutError = FString::Printf(
				TEXT("Import group '%s' has unknown TextureUsage '%s'."),
				*OutGroup.GroupName,
				*TextureUsage);
			return false;
		}

		FString PivotMode(TEXT("BottomCenter"));
		Object->TryGetStringField(TEXT("PivotMode"), PivotMode);
		if (!ParsePivotMode(PivotMode, OutGroup.PivotMode))
		{
			OutError = FString::Printf(
				TEXT("Import group '%s' has unknown PivotMode '%s'."),
				*OutGroup.GroupName,
				*PivotMode);
			return false;
		}

		FString MaterialMode(TEXT("Masked"));
		Object->TryGetStringField(TEXT("MaterialMode"), MaterialMode);
		if (!ParseMaterialMode(MaterialMode, OutGroup.MaterialMode))
		{
			OutError = FString::Printf(
				TEXT("Import group '%s' has unknown MaterialMode '%s'."),
				*OutGroup.GroupName,
				*MaterialMode);
			return false;
		}
		return true;
	}

	bool ParseFlipbookGroup(
		const TSharedPtr<FJsonObject>& Object,
		FImmortalPaperFlipbookGroup& OutGroup,
		FString& OutError)
	{
		if (!Object.IsValid())
		{
			OutError = TEXT("FlipbookGroups contains a non-object entry.");
			return false;
		}

		Object->TryGetStringField(TEXT("GroupName"), OutGroup.GroupName);
		OutGroup.bEnabled =
			ReadOptionalBool(Object, TEXT("bEnabled"), true);
		Object->TryGetStringField(
			TEXT("SourceImportGroup"), OutGroup.SourceImportGroup);
		Object->TryGetStringField(
			TEXT("DestinationPath"), OutGroup.DestinationPath);
		Object->TryGetStringField(TEXT("AssetName"), OutGroup.AssetName);
		OutGroup.FramesPerSecond = ReadOptionalFloat(
			Object, TEXT("FramesPerSecond"), 12.0f);
		OutGroup.FrameRun = ReadOptionalInt(
			Object, TEXT("FrameRun"), 1);
		return true;
	}

	bool LoadManifest(
		const FString& ManifestPath,
		TArray<FImmortalPaperImportGroup>& OutImportGroups,
		TArray<FImmortalPaperFlipbookGroup>& OutFlipbookGroups,
		FString& OutError)
	{
		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *ManifestPath))
		{
			OutError = FString::Printf(
				TEXT("Could not read import settings '%s'."),
				*ManifestPath);
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader =
			TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root)
			|| !Root.IsValid())
		{
			OutError = FString::Printf(
				TEXT("Invalid JSON in '%s': %s"),
				*ManifestPath,
				*Reader->GetErrorMessage());
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* ImportGroupValues = nullptr;
		if (!Root->TryGetArrayField(
			TEXT("ImportGroups"), ImportGroupValues))
		{
			OutError = TEXT("Import settings has no ImportGroups array.");
			return false;
		}

		for (const TSharedPtr<FJsonValue>& Value : *ImportGroupValues)
		{
			FImmortalPaperImportGroup Group;
			if (!Value.IsValid()
				|| !ParseImportGroup(
					Value->AsObject(), Group, OutError))
			{
				return false;
			}
			OutImportGroups.Add(MoveTemp(Group));
		}

		const TArray<TSharedPtr<FJsonValue>>* FlipbookValues = nullptr;
		if (Root->TryGetArrayField(
			TEXT("FlipbookGroups"), FlipbookValues))
		{
			for (const TSharedPtr<FJsonValue>& Value : *FlipbookValues)
			{
				FImmortalPaperFlipbookGroup Group;
				if (!Value.IsValid()
					|| !ParseFlipbookGroup(
						Value->AsObject(), Group, OutError))
				{
					return false;
				}
				OutFlipbookGroups.Add(MoveTemp(Group));
			}
		}
		return true;
	}

	bool IsValidAssetName(
		const FString& AssetName,
		FString& OutError)
	{
		FText Reason;
		if (AssetName.IsEmpty()
			|| !FName(*AssetName).IsValidObjectName(Reason))
		{
			OutError = FString::Printf(
				TEXT("Invalid asset name '%s': %s"),
				*AssetName,
				*Reason.ToString());
			return false;
		}
		return true;
	}

	bool IsValidAssetPackage(
		const FString& PackagePath,
		const FString& AssetName,
		FString& OutError)
	{
		const FString PackageName =
			JoinLongPackagePath(PackagePath, AssetName);
		FText Reason;
		if (!FPackageName::IsValidLongPackageName(
			PackageName, false, &Reason))
		{
			OutError = FString::Printf(
				TEXT("Invalid package '%s': %s"),
				*PackageName,
				*Reason.ToString());
			return false;
		}
		return true;
	}

	FString ResolveSourceFilename(const FString& Filename)
	{
		FString Result = Filename;
		if (FPaths::IsRelative(Result))
		{
			Result = FPaths::ConvertRelativePathToFull(
				FPaths::ProjectDir(), Result);
		}
		else
		{
			Result = FPaths::ConvertRelativePathToFull(Result);
		}
		FPaths::NormalizeFilename(Result);
		return Result;
	}

	FString GetTextureAssetName(
		const FImmortalPaperImportGroup& Group,
		const int32 Index)
	{
		if (Group.DestinationNames.IsValidIndex(Index))
		{
			return Group.DestinationNames[Index];
		}
		if (Group.Filenames.Num() == 1
			&& !Group.DestinationName.IsEmpty())
		{
			return Group.DestinationName;
		}
		return FPaths::GetBaseFilename(Group.Filenames[Index]);
	}

	FString GetSpriteAssetName(
		const FImmortalPaperImportGroup& Group,
		const int32 Index,
		const FString& TextureAssetName)
	{
		if (Group.SpriteNames.IsValidIndex(Index))
		{
			return Group.SpriteNames[Index];
		}
		if (Group.bSpriteSheet
			&& !Group.SpriteName.IsEmpty())
		{
			return FString::Printf(
				TEXT("%s_%02d"), *Group.SpriteName, Index);
		}
		if (Group.Filenames.Num() == 1
			&& !Group.SpriteName.IsEmpty())
		{
			return Group.SpriteName;
		}
		return FString::Printf(TEXT("SP_%s"), *TextureAssetName);
	}

	int32 GetSpriteCount(
		const FImmortalPaperImportGroup& Group)
	{
		return Group.bSpriteSheet
			? Group.SheetFrameCount
			: Group.Filenames.Num();
	}

	bool ValidateManifest(
		TArray<FImmortalPaperImportGroup>& ImportGroups,
		TArray<FImmortalPaperFlipbookGroup>& FlipbookGroups,
		FString& OutError)
	{
		TSet<FString> AllGroupNames;
		TSet<FString> EnabledImportGroupNames;
		for (FImmortalPaperImportGroup& Group : ImportGroups)
		{
			Group.GroupName.TrimStartAndEndInline();
			Group.DestinationPath =
				NormalizeLongPackagePath(Group.DestinationPath);
			Group.SpriteDestinationPath =
				NormalizeLongPackagePath(Group.SpriteDestinationPath);

			if (Group.GroupName.IsEmpty()
				|| AllGroupNames.Contains(Group.GroupName))
			{
				OutError = FString::Printf(
					TEXT("Import group name is empty or duplicated: '%s'."),
					*Group.GroupName);
				return false;
			}
			AllGroupNames.Add(Group.GroupName);

			if (!Group.bEnabled)
			{
				UE_LOG(
					LogImmortalPaperImport,
					Display,
					TEXT("Skipping disabled import group '%s'."),
					*Group.GroupName);
				continue;
			}

			EnabledImportGroupNames.Add(Group.GroupName);
			if (Group.FactoryName != TEXT("TextureFactory"))
			{
				OutError = FString::Printf(
					TEXT("Import group '%s' only supports TextureFactory."),
					*Group.GroupName);
				return false;
			}
			if (Group.Filenames.IsEmpty())
			{
				OutError = FString::Printf(
					TEXT("Enabled import group '%s' has no files."),
					*Group.GroupName);
				return false;
			}
			if (Group.bSpriteSheet
				&& (!Group.bCreateSprites
					|| Group.Filenames.Num() != 1
					|| Group.SheetColumns <= 0
					|| Group.SheetRows <= 0
					|| Group.SheetFrameCount <= 0
					|| static_cast<int64>(Group.SheetFrameCount)
						> static_cast<int64>(Group.SheetColumns)
							* Group.SheetRows
					|| !Group.bHasSourceUV
					|| !Group.bHasSourceDimension))
			{
				OutError = FString::Printf(
					TEXT("Sprite-sheet group '%s' requires one file, sprites, a valid grid/frame count, SourceUV and SourceDimension."),
					*Group.GroupName);
				return false;
			}
			const int32 ActualFrameCount = GetSpriteCount(Group);
			if (Group.ExpectedFrameCount > 0
				&& ActualFrameCount != Group.ExpectedFrameCount)
			{
				OutError = FString::Printf(
					TEXT("Import group '%s' expected %d frames but defines %d."),
					*Group.GroupName,
					Group.ExpectedFrameCount,
					ActualFrameCount);
				return false;
			}
			if (!Group.DestinationNames.IsEmpty()
				&& Group.DestinationNames.Num() != Group.Filenames.Num())
			{
				OutError = FString::Printf(
					TEXT("Import group '%s' DestinationNames count mismatch."),
					*Group.GroupName);
				return false;
			}
			if (!Group.SpriteNames.IsEmpty()
				&& Group.SpriteNames.Num() != ActualFrameCount)
			{
				OutError = FString::Printf(
					TEXT("Import group '%s' SpriteNames count mismatch."),
					*Group.GroupName);
				return false;
			}
			if (Group.bCreateSprites
				&& Group.PixelsPerUnrealUnit <= 0.0f)
			{
				OutError = FString::Printf(
					TEXT("Import group '%s' has invalid PixelsPerUnrealUnit."),
					*Group.GroupName);
				return false;
			}
			if (Group.bCreateSprites
				&& Group.SpriteDestinationPath.IsEmpty())
			{
				OutError = FString::Printf(
					TEXT("Import group '%s' has no SpriteDestinationPath."),
					*Group.GroupName);
				return false;
			}
			if (Group.bCreateSprites
				&& Group.bHasSourceUV != Group.bHasSourceDimension)
			{
				OutError = FString::Printf(
					TEXT("Import group '%s' must specify SourceUV and SourceDimension together."),
					*Group.GroupName);
				return false;
			}
			if (Group.bCreateSprites
				&& Group.bHasSourceDimension
				&& (Group.SourceUV.X < 0
					|| Group.SourceUV.Y < 0
					|| Group.SourceDimension.X <= 0
					|| Group.SourceDimension.Y <= 0))
			{
				OutError = FString::Printf(
					TEXT("Import group '%s' has an invalid source rectangle."),
					*Group.GroupName);
				return false;
			}
			if (Group.bCreateSprites
				&& Group.bHasSourceDimension
				&& Group.ExpectedWidth > 0
				&& Group.ExpectedHeight > 0
				&& (static_cast<int64>(Group.SourceUV.X)
						+ static_cast<int64>(Group.SourceDimension.X)
							* (Group.bSpriteSheet
								? Group.SheetColumns
								: 1)
						> Group.ExpectedWidth
					|| static_cast<int64>(Group.SourceUV.Y)
						+ static_cast<int64>(Group.SourceDimension.Y)
							* (Group.bSpriteSheet
								? Group.SheetRows
								: 1)
						> Group.ExpectedHeight))
			{
				OutError = FString::Printf(
					TEXT("Import group '%s' source rectangle exceeds its expected texture size."),
					*Group.GroupName);
				return false;
			}
			if (Group.bCreateSprites
				&& (Group.PivotMode == EImmortalPaperPivotMode::Custom)
					!= Group.bHasCustomPivot)
			{
				OutError = FString::Printf(
					TEXT("Import group '%s' must use CustomPivot exactly when PivotMode is Custom."),
					*Group.GroupName);
				return false;
			}
			if (Group.bCreateSprites
				&& Group.bHasCustomPivot)
			{
				const FVector2D PivotBounds =
					Group.bHasSourceDimension
						? FVector2D(Group.SourceDimension)
						: FVector2D(
							Group.ExpectedWidth,
							Group.ExpectedHeight);
				if (Group.CustomPivot.X < 0.0
					|| Group.CustomPivot.Y < 0.0
					|| (PivotBounds.X > 0.0
						&& Group.CustomPivot.X > PivotBounds.X)
					|| (PivotBounds.Y > 0.0
						&& Group.CustomPivot.Y > PivotBounds.Y))
				{
					OutError = FString::Printf(
						TEXT("Import group '%s' CustomPivot lies outside the sprite source rectangle."),
						*Group.GroupName);
					return false;
				}
			}

			for (int32 Index = 0;
				Index < Group.Filenames.Num();
				++Index)
			{
				Group.Filenames[Index] =
					ResolveSourceFilename(Group.Filenames[Index]);
				if (!IFileManager::Get().FileExists(
					*Group.Filenames[Index]))
				{
					OutError = FString::Printf(
						TEXT("Import source does not exist: '%s'."),
						*Group.Filenames[Index]);
					return false;
				}

				const FString TextureAssetName =
					GetTextureAssetName(Group, Index);
				if (!IsValidAssetName(TextureAssetName, OutError)
					|| !IsValidAssetPackage(
						Group.DestinationPath,
						TextureAssetName,
						OutError))
				{
					return false;
				}

				if (Group.bCreateSprites)
				{
					const FString SpriteAssetName =
						GetSpriteAssetName(
							Group, Index, TextureAssetName);
					if (!IsValidAssetName(SpriteAssetName, OutError)
						|| !IsValidAssetPackage(
							Group.SpriteDestinationPath,
							SpriteAssetName,
							OutError))
					{
						return false;
					}
				}
			}
			if (Group.bCreateSprites && Group.bSpriteSheet)
			{
				const FString TextureAssetName =
					GetTextureAssetName(Group, 0);
				for (int32 FrameIndex = 1;
					FrameIndex < ActualFrameCount;
					++FrameIndex)
				{
					const FString SpriteAssetName =
						GetSpriteAssetName(
							Group,
							FrameIndex,
							TextureAssetName);
					if (!IsValidAssetName(
							SpriteAssetName, OutError)
						|| !IsValidAssetPackage(
							Group.SpriteDestinationPath,
							SpriteAssetName,
							OutError))
					{
						return false;
					}
				}
			}
		}

		TSet<FString> FlipbookNames;
		for (FImmortalPaperFlipbookGroup& Group : FlipbookGroups)
		{
			Group.GroupName.TrimStartAndEndInline();
			Group.DestinationPath =
				NormalizeLongPackagePath(Group.DestinationPath);

			if (Group.GroupName.IsEmpty()
				|| FlipbookNames.Contains(Group.GroupName))
			{
				OutError = FString::Printf(
					TEXT("Flipbook group name is empty or duplicated: '%s'."),
					*Group.GroupName);
				return false;
			}
			FlipbookNames.Add(Group.GroupName);

			if (!Group.bEnabled)
			{
				UE_LOG(
					LogImmortalPaperImport,
					Display,
					TEXT("Skipping disabled flipbook group '%s'."),
					*Group.GroupName);
				continue;
			}
			if (!EnabledImportGroupNames.Contains(
				Group.SourceImportGroup))
			{
				OutError = FString::Printf(
					TEXT("Flipbook group '%s' references disabled or missing import group '%s'."),
					*Group.GroupName,
					*Group.SourceImportGroup);
				return false;
			}
			if (Group.FramesPerSecond <= 0.0f
				|| Group.FrameRun <= 0)
			{
				OutError = FString::Printf(
					TEXT("Flipbook group '%s' has invalid timing."),
					*Group.GroupName);
				return false;
			}
			if (!IsValidAssetName(Group.AssetName, OutError)
				|| !IsValidAssetPackage(
					Group.DestinationPath,
					Group.AssetName,
					OutError))
			{
				return false;
			}
		}
		return true;
	}

	UMaterialInterface* LoadPaperMaterial(
		const EImmortalPaperMaterialMode MaterialMode)
	{
		const TCHAR* ObjectPath = nullptr;
		switch (MaterialMode)
		{
		case EImmortalPaperMaterialMode::Opaque:
			ObjectPath =
				TEXT("/Paper2D/OpaqueUnlitSpriteMaterial.OpaqueUnlitSpriteMaterial");
			break;
		case EImmortalPaperMaterialMode::Translucent:
			ObjectPath =
				TEXT("/Paper2D/TranslucentUnlitSpriteMaterial.TranslucentUnlitSpriteMaterial");
			break;
		case EImmortalPaperMaterialMode::Masked:
		default:
			ObjectPath =
				TEXT("/Paper2D/MaskedUnlitSpriteMaterial.MaskedUnlitSpriteMaterial");
			break;
		}
		return LoadObject<UMaterialInterface>(nullptr, ObjectPath);
	}

	bool ConfigureTexture(
		UTexture2D* Texture,
		const FImmortalPaperImportGroup& Group,
		FString& OutError)
	{
		if (!Texture)
		{
			OutError = FString::Printf(
				TEXT("Import group '%s' produced a null texture."),
				*Group.GroupName);
			return false;
		}

		const FIntPoint ImportedSize = Texture->GetImportedSize();
		if ((Group.ExpectedWidth > 0
				&& ImportedSize.X != Group.ExpectedWidth)
			|| (Group.ExpectedHeight > 0
				&& ImportedSize.Y != Group.ExpectedHeight))
		{
			OutError = FString::Printf(
				TEXT("Texture '%s' is %dx%d; expected %dx%d."),
				*Texture->GetPathName(),
				ImportedSize.X,
				ImportedSize.Y,
				Group.ExpectedWidth,
				Group.ExpectedHeight);
			return false;
		}

		Texture->Modify();
		const bool bIsBackground =
			Group.TextureUsage
				== EImmortalPaperTextureUsage::Background;
		Texture->LODGroup =
			bIsBackground
				? TEXTUREGROUP_World
				: TEXTUREGROUP_Pixels2D;
		Texture->MipGenSettings = TMGS_NoMipmaps;
		// Full-screen opaque scenery benefits from block compression. Animation
		// frames retain lossless RGBA so their transparent outlines stay clean.
		Texture->CompressionSettings =
			bIsBackground ? TC_Default : TC_EditorIcon;
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->NeverStream = true;
		Texture->SRGB = true;
		Texture->PostEditChange();
		Texture->MarkPackageDirty();
		return true;
	}

	UPaperSprite* LoadOrCreateSprite(
		IAssetTools& AssetTools,
		const FString& DestinationPath,
		const FString& AssetName,
		bool& bOutCreated,
		FString& OutError)
	{
		bOutCreated = false;
		const FString ObjectPath =
			MakeObjectPath(DestinationPath, AssetName);
		if (UObject* Existing = StaticLoadObject(
			UObject::StaticClass(),
			nullptr,
			*ObjectPath,
			nullptr,
			LOAD_NoWarn))
		{
			if (UPaperSprite* Sprite = Cast<UPaperSprite>(Existing))
			{
				return Sprite;
			}
			OutError = FString::Printf(
				TEXT("Asset '%s' exists but is not a PaperSprite."),
				*ObjectPath);
			return nullptr;
		}

		UPaperSprite* Sprite = Cast<UPaperSprite>(
			AssetTools.CreateAsset(
				AssetName,
				DestinationPath,
				UPaperSprite::StaticClass(),
				nullptr,
				TEXT("ImmortalPaperImport")));
		if (!Sprite)
		{
			OutError = FString::Printf(
				TEXT("Could not create PaperSprite '%s'."),
				*ObjectPath);
			return nullptr;
		}
		bOutCreated = true;
		return Sprite;
	}

	bool ConfigureSprite(
		UPaperSprite* Sprite,
		UTexture2D* Texture,
		const FImmortalPaperImportGroup& Group,
		UMaterialInterface* Material,
		FString& OutError)
	{
		if (!Sprite || !Texture || !Material)
		{
			OutError = FString::Printf(
				TEXT("Group '%s' cannot configure a null sprite, texture, or material."),
				*Group.GroupName);
			return false;
		}

		Sprite->Modify();
		FSpriteAssetInitParameters InitParams;
		InitParams.SetTextureAndFill(Texture);
		const FIntPoint ImportedSize = Texture->GetImportedSize();
		const FIntPoint SourceUV =
			Group.bHasSourceUV
				? Group.SourceUV
				: FIntPoint::ZeroValue;
		const FIntPoint SourceDimension =
			Group.bHasSourceDimension
				? Group.SourceDimension
				: ImportedSize;
		if (SourceUV.X < 0
			|| SourceUV.Y < 0
			|| SourceDimension.X <= 0
			|| SourceDimension.Y <= 0
			|| static_cast<int64>(SourceUV.X)
				+ SourceDimension.X > ImportedSize.X
			|| static_cast<int64>(SourceUV.Y)
				+ SourceDimension.Y > ImportedSize.Y)
		{
			OutError = FString::Printf(
				TEXT("Sprite source rectangle (%d,%d %dx%d) exceeds texture '%s' (%dx%d)."),
				SourceUV.X,
				SourceUV.Y,
				SourceDimension.X,
				SourceDimension.Y,
				*Texture->GetPathName(),
				ImportedSize.X,
				ImportedSize.Y);
			return false;
		}
		InitParams.Offset = SourceUV;
		InitParams.Dimension = SourceDimension;
		InitParams.SetPixelsPerUnrealUnit(
			Group.PixelsPerUnrealUnit);
		InitParams.DefaultMaterialOverride = Material;
		InitParams.AlternateMaterialOverride =
			LoadPaperMaterial(EImmortalPaperMaterialMode::Opaque);
		Sprite->InitializeSprite(InitParams, false);

		switch (Group.PivotMode)
		{
		case EImmortalPaperPivotMode::CenterCenter:
			Sprite->SetPivotMode(
				ESpritePivotMode::Center_Center,
				FVector2D::ZeroVector,
				true);
			break;
		case EImmortalPaperPivotMode::BottomCenter:
			Sprite->SetPivotMode(
				ESpritePivotMode::Bottom_Center,
				FVector2D::ZeroVector,
				true);
			break;
		case EImmortalPaperPivotMode::AlphaBottomCenter:
		{
			FVector2D AlphaPosition;
			FVector2D AlphaSize;
			Sprite->FindTextureBoundingBox(
				0.01f, AlphaPosition, AlphaSize);
			if (AlphaSize.X <= 0.0f || AlphaSize.Y <= 0.0f)
			{
				OutError = FString::Printf(
					TEXT("Sprite source '%s' has no visible alpha bounds."),
					*Texture->GetPathName());
				return false;
			}
			Sprite->SetPivotMode(
				ESpritePivotMode::Custom,
				FVector2D(
					AlphaPosition.X + AlphaSize.X * 0.5f,
					AlphaPosition.Y + AlphaSize.Y),
				true);
			break;
		}
		case EImmortalPaperPivotMode::Custom:
			Sprite->SetPivotMode(
				ESpritePivotMode::Custom,
				FVector2D(SourceUV) + Group.CustomPivot,
				true);
			break;
		default:
			OutError = TEXT("Unhandled PaperSprite pivot mode.");
			return false;
		}

		Sprite->PostEditChange();
		Sprite->MarkPackageDirty();
		if (Sprite->GetSourceTexture() != Texture)
		{
			OutError = FString::Printf(
				TEXT("Sprite '%s' did not retain its source texture."),
				*Sprite->GetPathName());
			return false;
		}
		if (!Sprite->GetSourceUV().Equals(FVector2D(SourceUV))
			|| !Sprite->GetSourceSize().Equals(
				FVector2D(SourceDimension)))
		{
			OutError = FString::Printf(
				TEXT("Sprite '%s' did not retain its source rectangle."),
				*Sprite->GetPathName());
			return false;
		}
		if (Group.PivotMode == EImmortalPaperPivotMode::Custom
			&& !Sprite->GetPivotPosition().Equals(
				FVector2D(SourceUV) + Group.CustomPivot))
		{
			OutError = FString::Printf(
				TEXT("Sprite '%s' did not retain its custom pivot."),
				*Sprite->GetPathName());
			return false;
		}
		return true;
	}

	bool ImportGroup(
		IAssetTools& AssetTools,
		const FImmortalPaperImportGroup& Group,
		FImmortalPaperImportedGroup& OutImportedGroup,
		TArray<UPackage*>& InOutPackagesToSave,
		int32& InOutCreatedSprites,
		int32& InOutUpdatedSprites,
		FString& OutError)
	{
		OutImportedGroup.Material =
			LoadPaperMaterial(Group.MaterialMode);
		if (Group.bCreateSprites
			&& !OutImportedGroup.Material)
		{
			OutError = FString::Printf(
				TEXT("Group '%s' could not load its Paper2D material."),
				*Group.GroupName);
			return false;
		}

		for (int32 Index = 0;
			Index < Group.Filenames.Num();
			++Index)
		{
			const FString TextureAssetName =
				GetTextureAssetName(Group, Index);
			UAssetImportTask* Task =
				NewObject<UAssetImportTask>();
			Task->Filename = Group.Filenames[Index];
			Task->DestinationPath = Group.DestinationPath;
			Task->DestinationName = TextureAssetName;
			Task->bReplaceExisting = Group.bReplaceExisting;
			Task->bReplaceExistingSettings = true;
			Task->bAutomated = true;
			Task->bSave = false;
			Task->bAsync = false;
			Task->Factory = NewObject<UTextureFactory>();

			TArray<UAssetImportTask*> Tasks;
			Tasks.Add(Task);
			AssetTools.ImportAssetTasks(Tasks);

			const FString TextureObjectPath =
				MakeObjectPath(
					Group.DestinationPath,
					TextureAssetName);
			UTexture2D* Texture = LoadObject<UTexture2D>(
				nullptr, *TextureObjectPath);
			if (!Texture)
			{
				OutError = FString::Printf(
					TEXT("Texture import did not create/update '%s'."),
					*TextureObjectPath);
				return false;
			}
			if (!ConfigureTexture(Texture, Group, OutError))
			{
				return false;
			}
			OutImportedGroup.Textures.Add(Texture);
			InOutPackagesToSave.AddUnique(
				Texture->GetOutermost());

			if (!Group.bCreateSprites)
			{
				continue;
			}

			const int32 SpriteCountForTexture =
				Group.bSpriteSheet
					? Group.SheetFrameCount
					: 1;
			for (int32 FrameIndex = 0;
				FrameIndex < SpriteCountForTexture;
				++FrameIndex)
			{
				const int32 SpriteIndex =
					Group.bSpriteSheet ? FrameIndex : Index;
				const FString SpriteAssetName =
					GetSpriteAssetName(
						Group,
						SpriteIndex,
						TextureAssetName);
				bool bCreated = false;
				UPaperSprite* Sprite = LoadOrCreateSprite(
					AssetTools,
					Group.SpriteDestinationPath,
					SpriteAssetName,
					bCreated,
					OutError);

				FImmortalPaperImportGroup SpriteGroup = Group;
				if (Group.bSpriteSheet)
				{
					const int32 Column =
						FrameIndex % Group.SheetColumns;
					const int32 Row =
						FrameIndex / Group.SheetColumns;
					SpriteGroup.bHasSourceUV = true;
					SpriteGroup.bHasSourceDimension = true;
					SpriteGroup.SourceUV = FIntPoint(
						Group.SourceUV.X
							+ Column * Group.SourceDimension.X,
						Group.SourceUV.Y
							+ Row * Group.SourceDimension.Y);
					SpriteGroup.SourceDimension =
						Group.SourceDimension;
				}
				if (!Sprite
					|| !ConfigureSprite(
						Sprite,
						Texture,
						SpriteGroup,
						OutImportedGroup.Material,
						OutError))
				{
					return false;
				}
				OutImportedGroup.Sprites.Add(Sprite);
				InOutPackagesToSave.AddUnique(
					Sprite->GetOutermost());
				if (bCreated)
				{
					++InOutCreatedSprites;
				}
				else
				{
					++InOutUpdatedSprites;
				}
			}
		}
		return true;
	}

	UPaperFlipbook* LoadOrCreateFlipbook(
		IAssetTools& AssetTools,
		const FString& DestinationPath,
		const FString& AssetName,
		bool& bOutCreated,
		FString& OutError)
	{
		bOutCreated = false;
		const FString ObjectPath =
			MakeObjectPath(DestinationPath, AssetName);
		if (UObject* Existing = StaticLoadObject(
			UObject::StaticClass(),
			nullptr,
			*ObjectPath,
			nullptr,
			LOAD_NoWarn))
		{
			if (UPaperFlipbook* Flipbook =
				Cast<UPaperFlipbook>(Existing))
			{
				return Flipbook;
			}
			OutError = FString::Printf(
				TEXT("Asset '%s' exists but is not a PaperFlipbook."),
				*ObjectPath);
			return nullptr;
		}

		UPaperFlipbook* Flipbook = Cast<UPaperFlipbook>(
			AssetTools.CreateAsset(
				AssetName,
				DestinationPath,
				UPaperFlipbook::StaticClass(),
				nullptr,
				TEXT("ImmortalPaperImport")));
		if (!Flipbook)
		{
			OutError = FString::Printf(
				TEXT("Could not create PaperFlipbook '%s'."),
				*ObjectPath);
			return nullptr;
		}
		bOutCreated = true;
		return Flipbook;
	}

	bool SetFlipbookMaterial(
		UPaperFlipbook* Flipbook,
		UMaterialInterface* Material)
	{
		if (!Flipbook || !Material)
		{
			return false;
		}
		FProperty* Property =
			UPaperFlipbook::StaticClass()->FindPropertyByName(
				TEXT("DefaultMaterial"));
		FObjectPropertyBase* ObjectProperty =
			CastField<FObjectPropertyBase>(Property);
		if (!ObjectProperty)
		{
			return false;
		}
		ObjectProperty->SetObjectPropertyValue_InContainer(
			Flipbook, Material);
		return true;
	}

	bool CreateOrUpdateFlipbook(
		IAssetTools& AssetTools,
		const FImmortalPaperFlipbookGroup& Group,
		const FImmortalPaperImportedGroup& SourceGroup,
		TArray<UPackage*>& InOutPackagesToSave,
		int32& InOutCreatedFlipbooks,
		int32& InOutUpdatedFlipbooks,
		FString& OutError)
	{
		if (SourceGroup.Sprites.IsEmpty()
			|| SourceGroup.Textures.IsEmpty())
		{
			OutError = FString::Printf(
				TEXT("Flipbook '%s' has no complete ordered sprite source."),
				*Group.GroupName);
			return false;
		}

		bool bCreated = false;
		UPaperFlipbook* Flipbook = LoadOrCreateFlipbook(
			AssetTools,
			Group.DestinationPath,
			Group.AssetName,
			bCreated,
			OutError);
		if (!Flipbook)
		{
			return false;
		}

		Flipbook->Modify();
		{
			FScopedFlipbookMutator Mutator(Flipbook);
			Mutator.FramesPerSecond =
				Group.FramesPerSecond;
			Mutator.KeyFrames.Reset();
			for (UPaperSprite* Sprite : SourceGroup.Sprites)
			{
				FPaperFlipbookKeyFrame& Frame =
					Mutator.KeyFrames.AddDefaulted_GetRef();
				Frame.Sprite = Sprite;
				Frame.FrameRun = Group.FrameRun;
			}
		}
		if (SourceGroup.Material
			&& !SetFlipbookMaterial(
				Flipbook, SourceGroup.Material))
		{
			OutError = FString::Printf(
				TEXT("Could not apply material to Flipbook '%s'."),
				*Flipbook->GetPathName());
			return false;
		}
		Flipbook->PostEditChange();
		Flipbook->MarkPackageDirty();

		if (Flipbook->GetNumKeyFrames()
			!= SourceGroup.Sprites.Num()
			|| !FMath::IsNearlyEqual(
				Flipbook->GetFramesPerSecond(),
				Group.FramesPerSecond))
		{
			OutError = FString::Printf(
				TEXT("Flipbook '%s' failed post-build validation."),
				*Flipbook->GetPathName());
			return false;
		}
		for (int32 Index = 0;
			Index < SourceGroup.Sprites.Num();
			++Index)
		{
			const FPaperFlipbookKeyFrame& Frame =
				Flipbook->GetKeyFrameChecked(Index);
			if (Frame.Sprite != SourceGroup.Sprites[Index]
				|| Frame.FrameRun != Group.FrameRun)
			{
				OutError = FString::Printf(
					TEXT("Flipbook '%s' frame %d is invalid."),
					*Flipbook->GetPathName(),
					Index);
				return false;
			}
		}

		InOutPackagesToSave.AddUnique(
			Flipbook->GetOutermost());
		if (bCreated)
		{
			++InOutCreatedFlipbooks;
		}
		else
		{
			++InOutUpdatedFlipbooks;
		}
		return true;
	}

	bool SaveAndVerifyPackages(
		const TArray<UPackage*>& PackagesToSave,
		FString& OutError)
	{
		if (PackagesToSave.IsEmpty())
		{
			OutError = TEXT("No enabled import produced packages to save.");
			return false;
		}

		for (UPackage* Package : PackagesToSave)
		{
			if (!Package)
			{
				OutError = TEXT("Save list contains a null package.");
				return false;
			}
			FString PackageFilename;
			if (!FPackageName::TryConvertLongPackageNameToFilename(
				Package->GetName(),
				PackageFilename,
				FPackageName::GetAssetPackageExtension()))
			{
				OutError = FString::Printf(
					TEXT("Could not resolve package filename for '%s'."),
					*Package->GetName());
				return false;
			}
			if (IFileManager::Get().FileExists(*PackageFilename)
				&& IFileManager::Get().IsReadOnly(*PackageFilename))
			{
				OutError = FString::Printf(
					TEXT("Refusing to overwrite read-only package '%s'."),
					*PackageFilename);
				return false;
			}
		}

		if (!UEditorLoadingAndSavingUtils::SavePackages(
			PackagesToSave, true))
		{
			OutError = TEXT("UEditorLoadingAndSavingUtils::SavePackages failed.");
			return false;
		}

		for (UPackage* Package : PackagesToSave)
		{
			FString PackageFilename;
			FPackageName::TryConvertLongPackageNameToFilename(
				Package->GetName(),
				PackageFilename,
				FPackageName::GetAssetPackageExtension());
			if (Package->IsDirty()
				|| !IFileManager::Get().FileExists(
					*PackageFilename))
			{
				OutError = FString::Printf(
					TEXT("Package save verification failed for '%s'."),
					*Package->GetName());
				return false;
			}
		}
		return true;
	}
}

UImmortalPaperImportCommandlet::UImmortalPaperImportCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UImmortalPaperImportCommandlet::Main(const FString& Params)
{
	FString ManifestPath;
	if (!FParse::Value(
		*Params, TEXT("ImportSettings="), ManifestPath)
		|| ManifestPath.IsEmpty())
	{
		UE_LOG(
			LogImmortalPaperImport,
			Error,
			TEXT("Missing -ImportSettings=<json> argument."));
		return 1;
	}

	ManifestPath = ResolveSourceFilename(ManifestPath);
	TArray<FImmortalPaperImportGroup> ImportGroups;
	TArray<FImmortalPaperFlipbookGroup> FlipbookGroups;
	FString Error;
	if (!LoadManifest(
		ManifestPath,
		ImportGroups,
		FlipbookGroups,
		Error))
	{
		UE_LOG(
			LogImmortalPaperImport,
			Error,
			TEXT("%s"),
			*Error);
		return 1;
	}
	if (!ValidateManifest(
		ImportGroups, FlipbookGroups, Error))
	{
		UE_LOG(
			LogImmortalPaperImport,
			Error,
			TEXT("%s"),
			*Error);
		return 2;
	}

	IAssetTools& AssetTools =
		FAssetToolsModule::GetModule().Get();
	TMap<FString, FImmortalPaperImportedGroup> ImportedGroups;
	TArray<UPackage*> PackagesToSave;
	int32 ImportedTextures = 0;
	int32 CreatedSprites = 0;
	int32 UpdatedSprites = 0;
	int32 CreatedFlipbooks = 0;
	int32 UpdatedFlipbooks = 0;

	for (const FImmortalPaperImportGroup& Group : ImportGroups)
	{
		if (!Group.bEnabled)
		{
			continue;
		}
		FImmortalPaperImportedGroup ImportedGroup;
		if (!ImportGroup(
			AssetTools,
			Group,
			ImportedGroup,
			PackagesToSave,
			CreatedSprites,
			UpdatedSprites,
			Error))
		{
			UE_LOG(
				LogImmortalPaperImport,
				Error,
				TEXT("%s"),
				*Error);
			return 3;
		}
		ImportedTextures += ImportedGroup.Textures.Num();
		ImportedGroups.Add(
			Group.GroupName, MoveTemp(ImportedGroup));
	}

	for (const FImmortalPaperFlipbookGroup& Group : FlipbookGroups)
	{
		if (!Group.bEnabled)
		{
			continue;
		}
		const FImmortalPaperImportedGroup* SourceGroup =
			ImportedGroups.Find(Group.SourceImportGroup);
		if (!SourceGroup
			|| !CreateOrUpdateFlipbook(
				AssetTools,
				Group,
				*SourceGroup,
				PackagesToSave,
				CreatedFlipbooks,
				UpdatedFlipbooks,
				Error))
		{
			if (Error.IsEmpty())
			{
				Error = FString::Printf(
					TEXT("Missing imported source group '%s'."),
					*Group.SourceImportGroup);
			}
			UE_LOG(
				LogImmortalPaperImport,
				Error,
				TEXT("%s"),
				*Error);
			return 3;
		}
	}

	if (!SaveAndVerifyPackages(PackagesToSave, Error))
	{
		UE_LOG(
			LogImmortalPaperImport,
			Error,
			TEXT("%s"),
			*Error);
		return 4;
	}

	UE_LOG(
		LogImmortalPaperImport,
		Display,
		TEXT("Immortal Paper2D import complete: textures=%d sprites(created=%d updated=%d) flipbooks(created=%d updated=%d) packages=%d settings=%s"),
		ImportedTextures,
		CreatedSprites,
		UpdatedSprites,
		CreatedFlipbooks,
		UpdatedFlipbooks,
		PackagesToSave.Num(),
		*ManifestPath);
	return 0;
}
