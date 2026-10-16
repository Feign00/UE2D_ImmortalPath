#include "ImmortalPathSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#include <limits>

namespace ImmortalMalformedSaveTests
{
	// This parser only records offsets in bytes produced by SaveGameToMemory in
	// this process. It is a fixture builder, not the production validator, and is
	// never used to parse the deliberately malformed variants below.
	struct FTagOffsets
	{
		FString Name;
		int32 NameOffset = INDEX_NONE;
		int32 NameEnd = INDEX_NONE;
		int32 TypeOffset = INDEX_NONE;
		int32 TypeNameEnd = INDEX_NONE;
		int32 InnerCountOffset = INDEX_NONE;
		int32 SizeOffset = INDEX_NONE;
		int32 FlagsOffset = INDEX_NONE;
		int32 PayloadOffset = INDEX_NONE;
		int32 Size = 0;
	};

	struct FFixtureOffsets
	{
		int32 BranchOffset = 26;
		int32 BranchEnd = INDEX_NONE;
		int32 CustomFormatOffset = INDEX_NONE;
		int32 CustomCountOffset = INDEX_NONE;
		int32 CustomDataOffset = INDEX_NONE;
		int32 ClassOffset = INDEX_NONE;
		int32 ClassEnd = INDEX_NONE;
		int32 TaggedStart = INDEX_NONE;
		int32 NoneOffset = INDEX_NONE;
		FTagOffsets Scalar;
		FTagOffsets Inventory;
		FTagOffsets NestedName;
		FTagOffsets NestedGuid;
		TArray<FTagOffsets> TopTags;
	};

	const TArray<FString>& Cases()
	{
		static const TArray<FString> Names = {
			TEXT("Ascii43"), TEXT("Zero64"), TEXT("BadMagic"),
			TEXT("HeaderTruncated"), TEXT("FileVersionZero"), TEXT("FileVersionFuture"),
			TEXT("BranchLengthMin"), TEXT("BranchLengthHuge"), TEXT("BranchUnterminated"),
			TEXT("CustomFormatInvalid"), TEXT("CustomCountNegative"), TEXT("CustomCountHuge"),
			TEXT("CustomGuidTruncated"), TEXT("ClassLengthMin"), TEXT("ClassLengthHuge"),
			TEXT("ClassUnterminated"), TEXT("WrongClass"),
			TEXT("PropertyNameLengthMin"), TEXT("PropertyNameLengthHuge"),
			TEXT("PropertyNameOverLimit"), TEXT("PropertyNameUnterminated"),
			TEXT("TypeNameLengthHuge"), TEXT("TypeNameOverLimit"),
			TEXT("TypeInnerCountNegative"), TEXT("TypeInnerCountHuge"),
			TEXT("PropertySizeNegative"), TEXT("PropertySizeHuge"),
			TEXT("PropertyGuidTruncated"), TEXT("ArrayCountNegative"), TEXT("ArrayCountHuge"),
			TEXT("NestedNameLengthMin"), TEXT("NestedNameOverLimit"),
			TEXT("ActualNameValueLengthMin"), TEXT("ActualNameValueOverLimit"),
			TEXT("NestedGuidTruncated"), TEXT("MissingNone"), TEXT("TrailingGarbage")
		};
		return Names;
	}

	bool ReadInt32(const TArray<uint8>& Bytes, const int32 Offset, int32& Out)
	{
		if (Offset < 0 || Offset > Bytes.Num() - 4) return false;
		FMemory::Memcpy(&Out, Bytes.GetData() + Offset, sizeof(Out));
		return true;
	}

	bool SetInt32(TArray<uint8>& Bytes, const int32 Offset, const int32 Value)
	{
		if (Offset < 0 || Offset > Bytes.Num() - 4) return false;
		FMemory::Memcpy(Bytes.GetData() + Offset, &Value, sizeof(Value));
		return true;
	}

	bool ReadTrustedString(const TArray<uint8>& Bytes, int32& Cursor, FString& Out)
	{
		int32 Count = 0;
		if (!ReadInt32(Bytes, Cursor, Count) || Count == MIN_int32) return false;
		Cursor += 4;
		const int64 ByteCount = Count < 0 ? -int64(Count) * 2 : int64(Count);
		if (ByteCount > Bytes.Num() - Cursor || ByteCount > 65536) return false;
		Out.Reset();
		if (Count > 0)
		{
			if (Bytes[Cursor + Count - 1] != 0) return false;
			for (int32 Index = 0; Index < Count - 1; ++Index)
			{
				Out.AppendChar(TCHAR(Bytes[Cursor + Index]));
			}
		}
		else if (Count < 0)
		{
			for (int32 Index = 0; Index < -Count - 1; ++Index)
			{
				uint16 Character = 0;
				FMemory::Memcpy(&Character, Bytes.GetData() + Cursor + Index * 2, 2);
				Out.AppendChar(TCHAR(Character));
			}
		}
		Cursor += int32(ByteCount);
		return true;
	}

	bool ReadTrustedTag(const TArray<uint8>& Bytes, int32& Cursor, FTagOffsets& Tag)
	{
		Tag.NameOffset = Cursor;
		if (!ReadTrustedString(Bytes, Cursor, Tag.Name)) return false;
		Tag.NameEnd = Cursor;
		if (Tag.Name == TEXT("None")) return true;
		Tag.TypeOffset = Cursor;
		int32 Remaining = 1;
		int32 NodeCount = 0;
		while (Remaining > 0)
		{
			FString Name;
			if (++NodeCount > 128 || !ReadTrustedString(Bytes, Cursor, Name)) return false;
			if (NodeCount == 1)
			{
				Tag.TypeNameEnd = Cursor;
				Tag.InnerCountOffset = Cursor;
			}
			int32 InnerCount = 0;
			if (!ReadInt32(Bytes, Cursor, InnerCount) || InnerCount < 0 || InnerCount > 32) return false;
			Cursor += 4;
			Remaining += InnerCount - 1;
		}
		Tag.SizeOffset = Cursor;
		if (!ReadInt32(Bytes, Cursor, Tag.Size) || Tag.Size < 0) return false;
		Cursor += 4;
		Tag.FlagsOffset = Cursor;
		if (!Bytes.IsValidIndex(Cursor)) return false;
		const uint8 Flags = Bytes[Cursor++];
		if ((Flags & 0x04) != 0) return false; // No extensions in this generated fixture.
		if ((Flags & 0x01) != 0) Cursor += 4;
		if ((Flags & 0x02) != 0) Cursor += 16;
		if (Cursor > Bytes.Num() || Tag.Size > Bytes.Num() - Cursor) return false;
		Tag.PayloadOffset = Cursor;
		Cursor += Tag.Size;
		return true;
	}

	bool FindTrustedString(const TArray<uint8>& Bytes, const FString& Value, int32& OutOffset)
	{
		TArray<uint8> Needle;
		FMemoryWriter Writer(Needle);
		FString Copy = Value;
		Writer << Copy;
		OutOffset = INDEX_NONE;
		for (int32 Offset = 0; Offset <= Bytes.Num() - Needle.Num(); ++Offset)
		{
			if (FMemory::Memcmp(Bytes.GetData() + Offset, Needle.GetData(), Needle.Num()) == 0)
			{
				if (OutOffset != INDEX_NONE) return false;
				OutOffset = Offset;
			}
		}
		return OutOffset != INDEX_NONE;
	}

	bool RecordTrustedOffsets(const TArray<uint8>& Bytes, FFixtureOffsets& Offsets,
		const bool bRecordNested = true)
	{
		int32 Version = 0;
		if (!ReadInt32(Bytes, 4, Version) || Version != 3) return false;
		int32 Cursor = Offsets.BranchOffset;
		FString Text;
		if (!ReadTrustedString(Bytes, Cursor, Text)) return false;
		Offsets.BranchEnd = Cursor;
		Offsets.CustomFormatOffset = Cursor;
		int32 Format = 0;
		if (!ReadInt32(Bytes, Cursor, Format) || Format != 3) return false;
		Cursor += 4;
		Offsets.CustomCountOffset = Cursor;
		int32 CustomCount = 0;
		if (!ReadInt32(Bytes, Cursor, CustomCount) || CustomCount <= 0 || CustomCount > 2048) return false;
		Cursor += 4;
		Offsets.CustomDataOffset = Cursor;
		Cursor += CustomCount * 20;
		Offsets.ClassOffset = Cursor;
		if (!ReadTrustedString(Bytes, Cursor, Text)
			|| Text != UImmortalPathSaveGame::StaticClass()->GetPathName()) return false;
		Offsets.ClassEnd = Cursor;
		// Current UE5 object snapshots begin with a serialization-control byte.
		// ClassEnd remains the end of the class FString for header mutations.
		if (!Bytes.IsValidIndex(Cursor) || Bytes[Cursor++] != 0) return false;
		Offsets.TaggedStart = Cursor;
		while (Cursor < Bytes.Num())
		{
			FTagOffsets Tag;
			if (!ReadTrustedTag(Bytes, Cursor, Tag)) return false;
			if (Tag.Name == TEXT("SaveVersion")
				|| (Tag.Name == TEXT("PlayerHealth") && Offsets.Scalar.NameOffset < 0)) Offsets.Scalar = Tag;
			if (Tag.Name == TEXT("InventoryItems")) Offsets.Inventory = Tag;
			if (Tag.Name == TEXT("None"))
			{
				Offsets.NoneOffset = Tag.NameOffset;
				break;
			}
			Offsets.TopTags.Add(Tag);
		}
		if (!bRecordNested)
		{
			return Offsets.Scalar.NameOffset >= 0 && Offsets.Inventory.PayloadOffset >= 0
				&& Offsets.NoneOffset >= 0;
		}
		int32 NestedNameOffset = INDEX_NONE;
		int32 NestedGuidOffset = INDEX_NONE;
		if (!FindTrustedString(Bytes, TEXT("DisplayName"), NestedNameOffset)
			|| !FindTrustedString(Bytes, TEXT("ItemId"), NestedGuidOffset)) return false;
		Cursor = NestedNameOffset;
		if (!ReadTrustedTag(Bytes, Cursor, Offsets.NestedName)) return false;
		Cursor = NestedGuidOffset;
		if (!ReadTrustedTag(Bytes, Cursor, Offsets.NestedGuid)) return false;
		return Offsets.Scalar.NameOffset >= 0 && Offsets.Inventory.PayloadOffset >= 0
			&& Offsets.NestedGuid.Size == 16 && Offsets.NoneOffset >= 0;
	}

	bool ReplaceRange(TArray<uint8>& Bytes, const int32 Start, const int32 End, const TArray<uint8>& Replacement)
	{
		if (Start < 0 || End < Start || End > Bytes.Num()) return false;
		TArray<uint8> Result;
		Result.Reserve(Bytes.Num() - (End - Start) + Replacement.Num());
		Result.Append(Bytes.GetData(), Start);
		Result.Append(Replacement);
		Result.Append(Bytes.GetData() + End, Bytes.Num() - End);
		Bytes = MoveTemp(Result);
		return true;
	}

	TArray<uint8> SerializedString(FString Text)
	{
		TArray<uint8> Result;
		FMemoryWriter Writer(Result);
		Writer << Text;
		return Result;
	}

	bool MakeMalformed(const FString& Case, const TArray<uint8>& Valid,
		const FFixtureOffsets& Offsets, TArray<uint8>& Bytes)
	{
		Bytes = Valid;
		const FTagOffsets& First = Offsets.Scalar;
		if (Case == TEXT("Ascii43"))
		{
			static const ANSICHAR Sentinel[] = "preflight-residue-sentinel-not-a-game-save\n";
			Bytes.Reset();
			Bytes.Append(reinterpret_cast<const uint8*>(Sentinel), UE_ARRAY_COUNT(Sentinel) - 1);
			return Bytes.Num() == 43;
		}
		if (Case == TEXT("Zero64")) { Bytes.SetNumZeroed(64); FMemory::Memzero(Bytes.GetData(), 64); return true; }
		if (Case == TEXT("BadMagic")) return SetInt32(Bytes, 0, 0x11223344);
		if (Case == TEXT("HeaderTruncated")) { Bytes.SetNum(Offsets.ClassOffset + 6); return true; }
		if (Case == TEXT("FileVersionZero")) return SetInt32(Bytes, 4, 0);
		if (Case == TEXT("FileVersionFuture")) return SetInt32(Bytes, 4, 4);
		if (Case == TEXT("BranchLengthMin")) return SetInt32(Bytes, Offsets.BranchOffset, MIN_int32);
		if (Case == TEXT("BranchLengthHuge")) return SetInt32(Bytes, Offsets.BranchOffset, MAX_int32);
		if (Case == TEXT("BranchUnterminated")) { Bytes[Offsets.BranchEnd - 1] = 1; return true; }
		if (Case == TEXT("CustomFormatInvalid")) return SetInt32(Bytes, Offsets.CustomFormatOffset, 99);
		if (Case == TEXT("CustomCountNegative")) return SetInt32(Bytes, Offsets.CustomCountOffset, -1);
		if (Case == TEXT("CustomCountHuge")) return SetInt32(Bytes, Offsets.CustomCountOffset, MAX_int32);
		if (Case == TEXT("CustomGuidTruncated")) { Bytes.SetNum(Offsets.CustomDataOffset + 8); return true; }
		if (Case == TEXT("ClassLengthMin")) return SetInt32(Bytes, Offsets.ClassOffset, MIN_int32);
		if (Case == TEXT("ClassLengthHuge")) return SetInt32(Bytes, Offsets.ClassOffset, MAX_int32);
		if (Case == TEXT("ClassUnterminated")) { Bytes[Offsets.ClassEnd - 1] = 1; return true; }
		if (Case == TEXT("WrongClass")) return ReplaceRange(Bytes, Offsets.ClassOffset,
			Offsets.ClassEnd, SerializedString(TEXT("/Script/Engine.SaveGame")));
		if (Case == TEXT("PropertyNameLengthMin")) return SetInt32(Bytes, First.NameOffset, MIN_int32);
		if (Case == TEXT("PropertyNameLengthHuge")) return SetInt32(Bytes, First.NameOffset, MAX_int32);
		if (Case == TEXT("PropertyNameOverLimit")) return ReplaceRange(Bytes,
			First.NameOffset, First.NameEnd, SerializedString(FString::ChrN(1024, TEXT('N'))));
		if (Case == TEXT("PropertyNameUnterminated")) { Bytes[First.NameEnd - 1] = 1; return true; }
		if (Case == TEXT("TypeNameLengthHuge")) return SetInt32(Bytes, First.TypeOffset, MAX_int32);
		if (Case == TEXT("TypeNameOverLimit")) return ReplaceRange(Bytes,
			First.TypeOffset, First.TypeNameEnd, SerializedString(FString::ChrN(1024, TEXT('T'))));
		if (Case == TEXT("TypeInnerCountNegative")) return SetInt32(Bytes, First.InnerCountOffset, -1);
		if (Case == TEXT("TypeInnerCountHuge")) return SetInt32(Bytes, First.InnerCountOffset, MAX_int32);
		if (Case == TEXT("PropertySizeNegative")) return SetInt32(Bytes, First.SizeOffset, -1);
		if (Case == TEXT("PropertySizeHuge")) return SetInt32(Bytes, First.SizeOffset, MAX_int32);
		if (Case == TEXT("PropertyGuidTruncated"))
		{
			Bytes[First.FlagsOffset] |= 0x02;
			Bytes.SetNum(First.FlagsOffset + 1 + 8);
			return true;
		}
		if (Case == TEXT("ArrayCountNegative")) return SetInt32(Bytes, Offsets.Inventory.PayloadOffset, -1);
		if (Case == TEXT("ArrayCountHuge")) return SetInt32(Bytes, Offsets.Inventory.PayloadOffset, MAX_int32);
		if (Case == TEXT("NestedNameLengthMin")) return SetInt32(Bytes, Offsets.NestedName.NameOffset, MIN_int32);
		if (Case == TEXT("NestedNameOverLimit"))
		{
			const TArray<uint8> Replacement = SerializedString(FString::ChrN(1024, TEXT('N')));
			const int32 Delta = Replacement.Num() - (Offsets.NestedName.NameEnd - Offsets.NestedName.NameOffset);
			return SetInt32(Bytes, Offsets.Inventory.SizeOffset, Offsets.Inventory.Size + Delta)
				&& ReplaceRange(Bytes, Offsets.NestedName.NameOffset, Offsets.NestedName.NameEnd, Replacement);
		}
		if (Case == TEXT("ActualNameValueLengthMin"))
			return SetInt32(Bytes, Offsets.NestedName.PayloadOffset, MIN_int32);
		if (Case == TEXT("ActualNameValueOverLimit"))
		{
			// Corrupt the actual DisplayName FName value, not its property tag.
			// Both enclosing sizes stay internally consistent so rejection must
			// inspect the bounded value rather than merely notice a size mismatch.
			const TArray<uint8> Replacement = SerializedString(FString::ChrN(1024, TEXT('V')));
			const int32 Delta = Replacement.Num() - Offsets.NestedName.Size;
			return SetInt32(Bytes, Offsets.Inventory.SizeOffset, Offsets.Inventory.Size + Delta)
				&& SetInt32(Bytes, Offsets.NestedName.SizeOffset, Replacement.Num())
				&& ReplaceRange(Bytes, Offsets.NestedName.PayloadOffset,
					Offsets.NestedName.PayloadOffset + Offsets.NestedName.Size, Replacement);
		}
		if (Case == TEXT("NestedGuidTruncated")) { Bytes.SetNum(Offsets.NestedGuid.PayloadOffset + 8); return true; }
		if (Case == TEXT("MissingNone")) { Bytes.SetNum(Offsets.NoneOffset); return true; }
		if (Case == TEXT("TrailingGarbage")) { Bytes.Append({ 0x11, 0x22, 0x33 }); return true; }
		return false;
	}
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FImmortalMalformedSaveTest,
	"ImmortalPath.Save.MalformedPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FImmortalMalformedSaveTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	for (const FString& Case : ImmortalMalformedSaveTests::Cases())
	{
		OutBeautifiedNames.Add(Case);
		OutTestCommands.Add(Case);
	}
}

bool FImmortalMalformedSaveTest::RunTest(const FString& Parameters)
{
	using namespace ImmortalMalformedSaveTests;
	if (!Cases().Contains(Parameters))
	{
		AddError(TEXT("Unknown malformed-save case"));
		return false;
	}
	// Rejection logs are expected at the three explicit public read boundaries:
	// main load, recovery's main read, and the final malformed backup read.
	AddExpectedError(TEXT("Save slot exists or is inaccessible but could not be loaded"),
		EAutomationExpectedErrorFlags::Contains, 3);
	AddExpectedError(TEXT("Save refused: existing slot is invalid or from a newer version"),
		EAutomationExpectedErrorFlags::Contains, 1);

	const FString Slot = TEXT("ImmortalPath_MalformedTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString BackupSlot = Slot + TEXT("_Backup");
	ON_SCOPE_EXIT
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
		UGameplayStatics::DeleteGameInSlot(BackupSlot, 0);
	};

	UImmortalPathSaveGame* Stale = NewObject<UImmortalPathSaveGame>();
	// Keep one deliberately nondefault scalar tag available for wire mutations.
	// The separate Schema23 case proves the default version may be omitted.
	Stale->SaveVersion = UImmortalPathSaveGame::CurrentSaveVersion - 1;
	Stale->bHasPlayerData = true;
	Stale->PlayerHealth = 91.0f;
	FImmortalEquipmentItem Item;
	Item.ItemId = FGuid(0x12345678, 0x23456789, 0x34567890, 0x45678901);
	Item.DisplayName = FName(TEXT("青云测试剑"));
	Stale->InventoryItems.Add(Item);
	TArray<uint8> Valid;
	if (!TestTrue(TEXT("Trusted fixture is serialized by the engine"), UGameplayStatics::SaveGameToMemory(Stale, Valid))) return false;
	FFixtureOffsets Offsets;
	if (!TestTrue(TEXT("Trusted current-format fixture offsets are available"), RecordTrustedOffsets(Valid, Offsets))) return false;
	TArray<uint8> Damaged;
	if (!TestTrue(TEXT("The selected malformed case was constructed"), MakeMalformed(Parameters, Valid, Offsets, Damaged))) return false;
	if (!TestTrue(TEXT("Fixture actually differs from its valid baseline"), Damaged != Valid)) return false;

	TestTrue(TEXT("Valid baseline backup is written only to an isolated GUID slot"),
		UGameplayStatics::SaveDataToSlot(Valid, BackupSlot, 0));
	TestTrue(TEXT("Malformed main bytes are written to the isolated GUID slot"),
		UGameplayStatics::SaveDataToSlot(Damaged, Slot, 0));
	EImmortalSaveLoadStatus Status = EImmortalSaveLoadStatus::Missing;
	TestNull(TEXT("Malformed main is rejected without creating new progress"),
		UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status));
	TestEqual(TEXT("Malformed main reports unreadable"), Status, EImmortalSaveLoadStatus::Unreadable);
	TestFalse(TEXT("Stale object cannot overwrite malformed main"), Stale->SaveToDisk(Slot));
	TArray<uint8> Preserved;
	TestTrue(TEXT("Malformed main remains present"), UGameplayStatics::LoadDataFromSlot(Preserved, Slot, 0));
	TestTrue(TEXT("Rejected load and stale save preserve exact damaged bytes"), Preserved == Damaged);
	TestTrue(TEXT("Valid backup is offered for explicit recovery"), UImmortalPathSaveGame::HasRestorableBackup(Slot));
	TestTrue(TEXT("Explicit recovery can replace the unreadable main"), UImmortalPathSaveGame::RestoreBackup(Slot));
	UImmortalPathSaveGame* Restored = UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status);
	TestTrue(TEXT("Recovered player and Unicode inventory survive"), Restored && Status == EImmortalSaveLoadStatus::Loaded
		&& Restored->bHasPlayerData && Restored->PlayerHealth == 91.0f && Restored->InventoryItems.Num() == 1
		&& Restored->InventoryItems[0].DisplayName == Item.DisplayName
		&& Restored->InventoryItems[0].ItemId == Item.ItemId);

	TestTrue(TEXT("Same malformed payload is written as old backup"),
		UGameplayStatics::SaveDataToSlot(Damaged, BackupSlot, 0));
	TestFalse(TEXT("Malformed backup is never offered as restorable"), UImmortalPathSaveGame::HasRestorableBackup(Slot));
	UImmortalPathSaveGame* Main = UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status);
	TestTrue(TEXT("Malformed old backup does not hide a readable main"), Main && Status == EImmortalSaveLoadStatus::Loaded
		&& Main->PlayerHealth == 91.0f);
	Preserved.Reset();
	TestTrue(TEXT("Malformed backup remains present before a legitimate save"),
		UGameplayStatics::LoadDataFromSlot(Preserved, BackupSlot, 0));
	TestTrue(TEXT("Read-only backup checks preserve exact malformed bytes"), Preserved == Damaged);
	TestTrue(TEXT("Legitimate main save may replace a malformed old backup"), Stale->SaveToDisk(Slot));
	TestTrue(TEXT("Previous valid main becomes a readable backup"), UImmortalPathSaveGame::HasRestorableBackup(Slot));
	Preserved.Reset();
	TestTrue(TEXT("Repaired backup bytes are available"), UGameplayStatics::LoadDataFromSlot(Preserved, BackupSlot, 0));
	TestTrue(TEXT("Backup repair keeps the previous valid snapshot exactly"), Preserved == Valid);
	UE_LOG(LogTemp, Display, TEXT("R01MalformedSave case=%s completed: fixtureBytes=%d"), *Parameters, Damaged.Num());
	return true;
}

namespace ImmortalCompatibleSaveTests
{
	const TArray<FString>& Cases()
	{
		static const TArray<FString> Names = {
			TEXT("Schema1"), TEXT("Schema6"), TEXT("Schema15"), TEXT("Schema20"),
			TEXT("Schema22"), TEXT("Schema23"), TEXT("Schema24"),
			TEXT("LegacyOmittedFields"), TEXT("UnknownRemovedField")
		};
		return Names;
	}

	struct FPopulationStats
	{
		int32 Sequence = 0;
		int32 ScalarValues = 0;
		int32 Arrays = 0;
		int32 Structs = 0;
		int32 TopFields = 0;
		FString Failure;
	};

	bool PopulateValue(FProperty* Property, void* Value, FPopulationStats& Stats, const int32 Depth);

	bool PopulateStruct(UStruct* Type, void* Container, FPopulationStats& Stats,
		const int32 Depth, const bool bTopLevel)
	{
		if (Depth > 32)
		{
			Stats.Failure = TEXT("Fixture population exceeded its reflection recursion limit");
			return false;
		}
		for (TFieldIterator<FProperty> It(Type); It; ++It)
		{
			FProperty* Property = *It;
			if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)
				|| (bTopLevel && !Property->HasAnyPropertyFlags(CPF_SaveGame))) continue;
			if (bTopLevel) ++Stats.TopFields;
			for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ++ArrayIndex)
			{
				if (!PopulateValue(Property, Property->ContainerPtrToValuePtr<void>(Container, ArrayIndex), Stats, Depth + 1)) return false;
			}
		}
		return true;
	}

	int64 OtherEnumValue(const UEnum* Enum, const int64 Current)
	{
		for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
		{
			const FString Name = Enum->GetNameStringByIndex(Index);
			const int64 Candidate = Enum->GetValueByIndex(Index);
			if (Candidate != Current && !Name.EndsWith(TEXT("MAX"))) return Candidate;
		}
		return Current;
	}

	bool PopulateValue(FProperty* Property, void* Value, FPopulationStats& Stats, const int32 Depth)
	{
		if (Depth > 32)
		{
			Stats.Failure = TEXT("Fixture population exceeded its reflection recursion limit");
			return false;
		}
		const int32 Sequence = ++Stats.Sequence;
		if (FBoolProperty* Bool = CastField<FBoolProperty>(Property))
		{
			Bool->SetPropertyValue(Value, !Bool->GetPropertyValue(Value));
		}
		else if (FEnumProperty* Enum = CastField<FEnumProperty>(Property))
		{
			FNumericProperty* Underlying = Enum->GetUnderlyingProperty();
			Underlying->SetIntPropertyValue(Value, OtherEnumValue(Enum->GetEnum(), Underlying->GetSignedIntPropertyValue(Value)));
		}
		else if (FByteProperty* Byte = CastField<FByteProperty>(Property); Byte && Byte->Enum)
		{
			Byte->SetPropertyValue(Value, uint8(OtherEnumValue(Byte->Enum, Byte->GetPropertyValue(Value))));
		}
		else if (FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
		{
			if (Numeric->IsFloatingPoint()) Numeric->SetFloatingPointPropertyValue(Value, 100.375 + Sequence);
			else Numeric->SetIntPropertyValue(Value, uint64(1000 + Sequence * 13));
		}
		else if (FNameProperty* Name = CastField<FNameProperty>(Property))
		{
			// The non-BMP character intentionally exercises a valid UTF-16 pair.
			Name->SetPropertyValue(Value, FName(*FString::Printf(TEXT("青云😀_%s_%d"), *Property->GetName(), Sequence)));
		}
		else if (FStrProperty* String = CastField<FStrProperty>(Property))
		{
			String->SetPropertyValue(Value, FString::Printf(TEXT("灵石😀_%s_%d"), *Property->GetName(), Sequence));
		}
		else if (FTextProperty* Text = CastField<FTextProperty>(Property))
		{
			Text->SetPropertyValue(Value, FText::FromString(FString::Printf(TEXT("仙途😀_%d"), Sequence)));
		}
		else if (FArrayProperty* Array = CastField<FArrayProperty>(Property))
		{
			++Stats.Arrays;
			FScriptArrayHelper Helper(Array, Value);
			Helper.Resize(2);
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				if (!PopulateValue(Array->Inner, Helper.GetRawPtr(Index), Stats, Depth + 1)) return false;
			}
			return true;
		}
		else if (FStructProperty* Struct = CastField<FStructProperty>(Property))
		{
			++Stats.Structs;
			if (Struct->Struct->GetFName() == FName(TEXT("Guid")))
			{
				*static_cast<FGuid*>(Value) = FGuid(0x12340000u + uint32(Sequence),
					0x34560000u + uint32(Sequence), 0x56780000u + uint32(Sequence), 0x789A0000u + uint32(Sequence));
			}
			else
			{
				return PopulateStruct(Struct->Struct, Value, Stats, Depth + 1, false);
			}
		}
		else
		{
			Stats.Failure = FString::Printf(TEXT("Unhandled persistent fixture type: %s (%s)"),
				*Property->GetPathName(), *Property->GetClass()->GetName());
			return false;
		}
		++Stats.ScalarValues;
		return true;
	}

	bool MakeLegacyOmittedFields(const TArray<uint8>& Rich, TArray<uint8>& Out)
	{
		using namespace ImmortalMalformedSaveTests;
		FFixtureOffsets Offsets;
		if (!RecordTrustedOffsets(Rich, Offsets, false)) return false;
		const TSet<FString> Retained = { TEXT("SaveVersion"), TEXT("bHasPlayerData"), TEXT("PlayerHealth"),
			TEXT("PlayerMana"), TEXT("InventoryItems"), TEXT("EquippedItems") };
		Out.Reset();
		Out.Append(Rich.GetData(), Offsets.TaggedStart);
		for (const FTagOffsets& Tag : Offsets.TopTags)
		{
			if (!Retained.Contains(Tag.Name)) continue;
			Out.Append(Rich.GetData() + Tag.NameOffset, Tag.PayloadOffset + Tag.Size - Tag.NameOffset);
		}
		Out.Append(Rich.GetData() + Offsets.NoneOffset, Rich.Num() - Offsets.NoneOffset);
		return Out.Num() < Rich.Num();
	}

	bool AddUnknownRemovedField(const TArray<uint8>& Rich, TArray<uint8>& Out)
	{
		using namespace ImmortalMalformedSaveTests;
		FFixtureOffsets Offsets;
		if (!RecordTrustedOffsets(Rich, Offsets, false)) return false;
		const FTagOffsets& Source = Offsets.Scalar;
		TArray<uint8> Unknown;
		Unknown.Append(Rich.GetData() + Source.NameOffset, Source.PayloadOffset + Source.Size - Source.NameOffset);
		if (!ReplaceRange(Unknown, 0, Source.NameEnd - Source.NameOffset,
			SerializedString(TEXT("RemovedLegacyField_R01Fixture")))) return false;
		Out = Rich;
		// This is a well-formed tag copied from the trusted writer, not an
		// unbounded property stream manufactured for the engine to deserialize.
		return ReplaceRange(Out, Offsets.NoneOffset, Offsets.NoneOffset, Unknown);
	}
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FImmortalCompatibleSaveTest,
	"ImmortalPath.Save.CompatiblePayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FImmortalCompatibleSaveTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	for (const FString& Case : ImmortalCompatibleSaveTests::Cases())
	{
		OutBeautifiedNames.Add(Case);
		OutTestCommands.Add(Case);
	}
}

bool FImmortalCompatibleSaveTest::RunTest(const FString& Parameters)
{
	using namespace ImmortalCompatibleSaveTests;
	if (!Cases().Contains(Parameters))
	{
		AddError(TEXT("Unknown compatible-save case"));
		return false;
	}
	const bool bLegacyOmission = Parameters == TEXT("LegacyOmittedFields");
	const bool bUnknownField = Parameters == TEXT("UnknownRemovedField");
	const int32 Schema = Parameters.StartsWith(TEXT("Schema")) ? FCString::Atoi(*Parameters.Mid(6))
		: (bLegacyOmission ? 1 : UImmortalPathSaveGame::CurrentSaveVersion);
	if (Schema > UImmortalPathSaveGame::CurrentSaveVersion)
	{
		AddExpectedError(TEXT("Save slot uses newer version 24"), EAutomationExpectedErrorFlags::Contains, 1);
	}

	UImmortalPathSaveGame* Original = NewObject<UImmortalPathSaveGame>();
	FPopulationStats Stats;
	if (!PopulateStruct(Original->GetClass(), Original, Stats, 0, true))
	{
		AddError(Stats.Failure);
		return false;
	}
	const UImmortalPathSaveGame* Defaults = GetDefault<UImmortalPathSaveGame>();
	for (TFieldIterator<FProperty> It(Original->GetClass()); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property->HasAnyPropertyFlags(CPF_SaveGame) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)) continue;
		TestFalse(FString::Printf(TEXT("Populated top-level field differs from its default: %s"), *Property->GetName()),
			Property->Identical_InContainer(Original, Defaults));
	}
	// Only the schema discriminator is reset after proving all fields were
	// populated. These are old game-schema values in the supported UE envelope;
	// the omission case separately tests fields absent from an older snapshot.
	Original->SaveVersion = Schema;
	TArray<uint8> Rich;
	if (!TestTrue(TEXT("Full reflected fixture serializes through the engine"), UGameplayStatics::SaveGameToMemory(Original, Rich))) return false;
	if (Schema == UImmortalPathSaveGame::CurrentSaveVersion)
	{
		ImmortalMalformedSaveTests::FFixtureOffsets CurrentOffsets;
		if (!TestTrue(TEXT("Current default-version fixture has trusted tag offsets"),
			ImmortalMalformedSaveTests::RecordTrustedOffsets(Rich, CurrentOffsets, false))) return false;
		TestFalse(TEXT("Engine delta serialization may omit the default current SaveVersion tag"),
			CurrentOffsets.TopTags.ContainsByPredicate([](const ImmortalMalformedSaveTests::FTagOffsets& Tag)
				{ return Tag.Name == TEXT("SaveVersion"); }));
	}
	TArray<uint8> Input = Rich;
	if (bLegacyOmission && !TestTrue(TEXT("Legacy absent-field fixture is constructed"), MakeLegacyOmittedFields(Rich, Input))) return false;
	if (bUnknownField && !TestTrue(TEXT("Removed-field fixture has a legal bounded tag"), AddUnknownRemovedField(Rich, Input))) return false;

	// This is the only engine load in these tests, and its input is either the
	// trusted engine writer's own bytes or a bounded valid-tag transformation.
	// None of the malformed negative fixtures ever reach this engine path.
	UImmortalPathSaveGame* EngineLoaded = Cast<UImmortalPathSaveGame>(UGameplayStatics::LoadGameFromMemory(Input));
	if (!TestNotNull(TEXT("Trusted fixture loads through the engine reference path"), EngineLoaded)) return false;
	const FString Slot = TEXT("ImmortalPath_CompatibleTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	ON_SCOPE_EXIT { UGameplayStatics::DeleteGameInSlot(Slot, 0); };
	if (!TestTrue(TEXT("Trusted bytes are stored only in an isolated GUID slot"), UGameplayStatics::SaveDataToSlot(Input, Slot, 0))) return false;
	EImmortalSaveLoadStatus Status = EImmortalSaveLoadStatus::Unreadable;
	UImmortalPathSaveGame* BoundedLoaded = UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status);
	if (!TestNotNull(TEXT("Bounded production loader accepts the compatible fixture"), BoundedLoaded)) return false;
	TestEqual(TEXT("Schema remains loaded or explicitly protected as newer"), Status,
		Schema > UImmortalPathSaveGame::CurrentSaveVersion ? EImmortalSaveLoadStatus::NewerVersion : EImmortalSaveLoadStatus::Loaded);
	TestEqual(TEXT("Game-schema discriminator survives without changing its number"), BoundedLoaded->SaveVersion, Schema);
	for (TFieldIterator<FProperty> It(Original->GetClass()); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property->HasAnyPropertyFlags(CPF_SaveGame) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)) continue;
		TestTrue(FString::Printf(TEXT("Engine and bounded loader agree on complete field: %s"), *Property->GetName()),
			Property->Identical_InContainer(EngineLoaded, BoundedLoaded));
	}
	TArray<uint8> EngineBytes;
	TArray<uint8> BoundedBytes;
	TestTrue(TEXT("Engine reference object reserializes"), UGameplayStatics::SaveGameToMemory(EngineLoaded, EngineBytes));
	TestTrue(TEXT("Bounded result reserializes"), UGameplayStatics::SaveGameToMemory(BoundedLoaded, BoundedBytes));
	TestTrue(TEXT("Complete reserialized bytes match the engine reference exactly"), EngineBytes == BoundedBytes);
	if (!bLegacyOmission)
	{
		TestTrue(TEXT("Known reflected state roundtrips to its original writer bytes"), EngineBytes == Rich);
	}
	else
	{
		TestTrue(TEXT("Legacy absent fields retain constructor defaults"), !BoundedLoaded->bHasStageData
			&& BoundedLoaded->QingyunStage == Defaults->QingyunStage
			&& BoundedLoaded->SpiritStones == Defaults->SpiritStones);
		TestEqual(TEXT("Legacy retained inventory remains nonempty"), BoundedLoaded->InventoryItems.Num(), 2);
	}
	TArray<uint8> Preserved;
	TestTrue(TEXT("Read-only compatibility check leaves input present"), UGameplayStatics::LoadDataFromSlot(Preserved, Slot, 0));
	TestTrue(TEXT("Read-only compatibility check preserves exact input bytes"), Preserved == Input);
	TestTrue(TEXT("Fixture covers nested arrays and hundreds of distinct scalar values"), Stats.Arrays > 10 && Stats.ScalarValues > 100);
	UE_LOG(LogTemp, Display, TEXT("R01CompatibleSave case=%s completed: schema=%d topFields=%d arrays=%d structs=%d scalars=%d fixtureBytes=%d"),
		*Parameters, Schema, Stats.TopFields, Stats.Arrays, Stats.Structs, Stats.ScalarValues, Input.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalGeneratedSnapshotProtectionTest,
	"ImmortalPath.Save.GeneratedSnapshotProtection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalGeneratedSnapshotProtectionTest::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("Save refused: generated snapshot is outside supported format or resource limits"),
		EAutomationExpectedErrorFlags::Contains, 2);
	const FString Slot = TEXT("ImmortalPath_GeneratedSnapshotTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString BackupSlot = Slot + TEXT("_Backup");
	ON_SCOPE_EXIT
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
		UGameplayStatics::DeleteGameInSlot(BackupSlot, 0);
	};

	UImmortalPathSaveGame* Save = NewObject<UImmortalPathSaveGame>();
	Save->bHasPlayerData = true;
	Save->PlayerHealth = 91.0f;
	FImmortalEquipmentItem Item;
	Item.ItemId = FGuid(0x12345678, 0x23456789, 0x34567890, 0x45678901);
	Item.DisplayName = FName(TEXT("R01GeneratedSnapshotSword"));
	Save->InventoryItems.Add(Item);
	if (!TestTrue(TEXT("First legal generation is saved"), Save->SaveToDisk(Slot))) return false;
	Save->PlayerHealth = 37.0f;
	if (!TestTrue(TEXT("Second legal generation establishes main and backup"), Save->SaveToDisk(Slot))) return false;
	TArray<uint8> MainBefore;
	TArray<uint8> BackupBefore;
	if (!TestTrue(TEXT("Legal main bytes are captured"), UGameplayStatics::LoadDataFromSlot(MainBefore, Slot, 0))
		|| !TestTrue(TEXT("Legal previous generation bytes are captured"), UGameplayStatics::LoadDataFromSlot(BackupBefore, BackupSlot, 0))) return false;
	TestTrue(TEXT("The baseline contains two different generations"), MainBefore != BackupBefore);

	auto CheckUnchanged = [this, &Slot, &BackupSlot, &MainBefore, &BackupBefore](const FString& Label)
	{
		TArray<uint8> MainAfter;
		TArray<uint8> BackupAfter;
		TestTrue(Label + TEXT(": main remains present"), UGameplayStatics::LoadDataFromSlot(MainAfter, Slot, 0));
		TestTrue(Label + TEXT(": backup remains present"), UGameplayStatics::LoadDataFromSlot(BackupAfter, BackupSlot, 0));
		TestTrue(Label + TEXT(": exact main bytes are unchanged"), MainAfter == MainBefore);
		TestTrue(Label + TEXT(": exact backup bytes are unchanged"), BackupAfter == BackupBefore);
	};
	Save->PlayerHealth = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Generated non-finite health is rejected before either disk generation is touched"), Save->SaveToDisk(Slot));
	CheckUnchanged(TEXT("Non-finite snapshot rejection"));
	Save->PlayerHealth = 37.0f;
	Save->EquippedTechniqueIds.SetNum(65537);
	TArray<uint8> OverBudgetCandidate;
	TestTrue(TEXT("Over-count name array serializes through the trusted writer"),
		UGameplayStatics::SaveGameToMemory(Save, OverBudgetCandidate));
	TestTrue(TEXT("Array-limit case stays below the independent 16 MiB file-size cap"),
		OverBudgetCandidate.Num() < 16 * 1024 * 1024);
	TestFalse(TEXT("Generated snapshot exceeding the single-array limit is rejected"), Save->SaveToDisk(Slot));
	CheckUnchanged(TEXT("Array-budget snapshot rejection"));
	Save->EquippedTechniqueIds.Empty();
	Save->PlayerHealth = 73.0f;
	TestTrue(TEXT("A repaired legal snapshot can be saved normally"), Save->SaveToDisk(Slot));
	EImmortalSaveLoadStatus Status = EImmortalSaveLoadStatus::Unreadable;
	UImmortalPathSaveGame* Reopened = UImmortalPathSaveGame::LoadOrCreateSlot(Slot, &Status);
	TestTrue(TEXT("Freshly reopened main contains the repaired legal generation"), Reopened
		&& Status == EImmortalSaveLoadStatus::Loaded && Reopened != Save
		&& Reopened->PlayerHealth == 73.0f && Reopened->InventoryItems.Num() == 1
		&& Reopened->InventoryItems[0].ItemId == Item.ItemId);
	TArray<uint8> NewBackup;
	TestTrue(TEXT("Previous legal main is available as the new backup"), UGameplayStatics::LoadDataFromSlot(NewBackup, BackupSlot, 0));
	TestTrue(TEXT("Recovery from rejected generated snapshots preserves normal backup rotation"), NewBackup == MainBefore);
	UE_LOG(LogTemp, Display, TEXT("R01GeneratedSave case=GeneratedSnapshotProtection completed: rejected=2 repairedAndReopened=1"));
	return true;
}

#endif
