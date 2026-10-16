// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalSaveHeaderValidation.h"

#include "ImmortalPathSaveGame.h"
#include "UObject/Class.h"

namespace ImmortalSaveValidation
{
	namespace
	{
		constexpr int64 MaximumFileBytes = 16ll * 1024 * 1024;
		constexpr int32 SaveGameMagic = 0x53415647;
		constexpr int32 SupportedSaveHeaderVersion = 3;
		constexpr int32 MaximumHeaderStringCharacters = 512;
		constexpr int32 MaximumCustomVersions = 4096;
		constexpr int64 CustomVersionEntryBytes = 16 + 4;
		static_assert(static_cast<int32>(ECustomVersionSerializationFormat::Optimized) == 3,
			"Recheck the supported custom-version wire format after an engine upgrade.");

		/** No unchecked seeks, archive allocations, or name interning. */
		class FHeaderBytes
		{
		public:
			explicit FHeaderBytes(const TArray<uint8>& InBytes)
				: Bytes(InBytes)
			{
			}

			bool Has(const int64 Count) const
			{
				return Count >= 0 && Position >= 0
					&& Position <= Bytes.Num()
					&& Count <= static_cast<int64>(Bytes.Num()) - Position;
			}

			int64 Tell() const { return Position; }

			template <typename T>
			bool Read(T& Value)
			{
				static_assert(PLATFORM_LITTLE_ENDIAN,
					"The supported Windows SaveGame format is little endian.");
				static_assert(sizeof(T) == 2 || sizeof(T) == 4,
					"Only checked fixed-width header primitives are supported.");
				if (!Has(sizeof(T))) return false;
				FMemory::Memcpy(&Value, Bytes.GetData() + Position, sizeof(T));
				Position += sizeof(T);
				return true;
			}

			bool ReadString(FString& Value)
			{
				int32 SignedCount = 0;
				if (!Read(SignedCount) || SignedCount == MIN_int32) return false;
				if (SignedCount == 0)
				{
					Value.Empty();
					return true;
				}

				const bool bUnicode = SignedCount < 0;
				const int64 UnitCount = bUnicode
					? -static_cast<int64>(SignedCount)
					: static_cast<int64>(SignedCount);
				if (UnitCount > MaximumHeaderStringCharacters + 1) return false;
				const int64 UnitBytes = bUnicode ? 2 : 1;
				if (!Has(UnitCount * UnitBytes)) return false;

				// Validate the entire encoded range before constructing a string.
				// The engine normally repairs the final terminator; a file reader
				// must instead reject missing or interior terminators.
				bool bNeedsLowSurrogate = false;
				for (int64 Index = 0; Index < UnitCount; ++Index)
				{
					const int64 Offset = Position + Index * UnitBytes;
					const uint16 Unit = bUnicode
						? static_cast<uint16>(Bytes.GetData()[Offset]
							| (static_cast<uint16>(Bytes.GetData()[Offset + 1]) << 8))
						: Bytes.GetData()[Offset];
					if (Index == UnitCount - 1)
					{
						if (Unit != 0 || bNeedsLowSurrogate) return false;
						continue;
					}
					if (Unit == 0) return false;
					if (bUnicode)
					{
						if (bNeedsLowSurrogate)
						{
							if (Unit < 0xdc00 || Unit > 0xdfff) return false;
							bNeedsLowSurrogate = false;
						}
						else if (Unit >= 0xd800 && Unit <= 0xdbff)
						{
							bNeedsLowSurrogate = true;
						}
						else if ((Unit >= 0xdc00 && Unit <= 0xdfff)
							|| Unit == 0xffff)
						{
							return false;
						}
					}
				}

				const int32 CharacterCount = static_cast<int32>(UnitCount - 1);
				if (bUnicode)
				{
					// Copy through aligned storage instead of reinterpreting a
					// potentially unaligned byte position as a UTF-16 pointer.
					TArray<UCS2CHAR, TInlineAllocator<MaximumHeaderStringCharacters + 1>> Units;
					Units.AddUninitialized(CharacterCount);
					for (int32 Index = 0; Index < CharacterCount; ++Index)
					{
						const int64 Offset = Position + static_cast<int64>(Index) * 2;
						Units[Index] = static_cast<UCS2CHAR>(Bytes.GetData()[Offset]
							| (static_cast<uint16>(Bytes.GetData()[Offset + 1]) << 8));
					}
					Value = FString::ConstructFromPtrSize(Units.GetData(), CharacterCount);
				}
				else
				{
					Value = FString::ConstructFromPtrSize(
						reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + Position),
						CharacterCount);
				}
				Position += UnitCount * UnitBytes;
				return true;
			}

		private:
			const TArray<uint8>& Bytes;
			int64 Position = 0;
		};
	}

	bool ReadHeader(const TArray<uint8>& Bytes, FHeader& Out)
	{
		Out = FHeader();
		if (Bytes.Num() == 0 || Bytes.Num() > MaximumFileBytes) return false;

		FHeaderBytes Reader(Bytes);
		int32 Magic = 0;
		int32 HeaderVersion = 0;
		FHeader Validated;
		if (!Reader.Read(Magic) || Magic != SaveGameMagic
			|| !Reader.Read(HeaderVersion) || HeaderVersion != SupportedSaveHeaderVersion
			|| !Reader.Read(Validated.PackageVersion.FileVersionUE4)
			|| !Reader.Read(Validated.PackageVersion.FileVersionUE5))
		{
			return false;
		}
		if (Validated.PackageVersion.FileVersionUE4 != GPackageFileUEVersion.FileVersionUE4
			|| Validated.PackageVersion < EUnrealEngineObjectUE5Version::PROPERTY_TAG_COMPLETE_TYPE_NAME
			|| !GPackageFileUEVersion.IsCompatible(Validated.PackageVersion))
		{
			return false;
		}

		uint16 Major = 0;
		uint16 Minor = 0;
		uint16 Patch = 0;
		uint32 Changelist = 0;
		FString Branch;
		if (!Reader.Read(Major) || !Reader.Read(Minor) || !Reader.Read(Patch)
			|| !Reader.Read(Changelist) || !Reader.ReadString(Branch))
		{
			return false;
		}
		Validated.EngineVersion.Set(Major, Minor, Patch, Changelist, Branch);

		int32 CustomVersionFormat = 0;
		int32 CustomVersionCount = 0;
		if (!Reader.Read(CustomVersionFormat)
			|| CustomVersionFormat != static_cast<int32>(ECustomVersionSerializationFormat::Optimized)
			|| !Reader.Read(CustomVersionCount)
			|| CustomVersionCount < 0 || CustomVersionCount > MaximumCustomVersions
			// Every optimized entry is four uint32 GUID words plus int32 version.
			// Leave at least the following class-name length before accepting count.
			|| !Reader.Has(static_cast<int64>(CustomVersionCount) * CustomVersionEntryBytes + 4))
		{
			return false;
		}
		for (int32 Index = 0; Index < CustomVersionCount; ++Index)
		{
			uint32 A = 0;
			uint32 B = 0;
			uint32 C = 0;
			uint32 D = 0;
			int32 Version = 0;
			if (!Reader.Read(A) || !Reader.Read(B) || !Reader.Read(C)
				|| !Reader.Read(D) || !Reader.Read(Version)) return false;
			const FGuid Key(A, B, C, D);
			if (!Key.IsValid() || Validated.CustomVersions.GetVersion(Key)) return false;
			Validated.CustomVersions.SetVersion(Key, Version, NAME_None);
		}

		FString ClassName;
		if (!Reader.ReadString(ClassName)
			|| ClassName != UImmortalPathSaveGame::StaticClass()->GetPathName()
			|| !Reader.Has(1))
		{
			return false;
		}
		Validated.PayloadOffset = Reader.Tell();
		Out = MoveTemp(Validated);
		return true;
	}
}
