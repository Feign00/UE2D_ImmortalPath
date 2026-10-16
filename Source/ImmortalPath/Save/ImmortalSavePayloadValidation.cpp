// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalSavePayloadValidation.h"

#include "ImmortalPathSaveGame.h"
#include "ImmortalSaveHeaderValidation.h"
#include "UObject/PropertyTypeName.h"
#include "UObject/UnrealType.h"

namespace ImmortalSaveValidation
{
	namespace
	{
		constexpr int32 MaximumDepth = 32;
		constexpr int32 MaximumTags = 65536;
		constexpr int32 MaximumTypeNodes = 64;
		constexpr int32 MaximumTotalTypeNodes = 262144;
		constexpr int32 MaximumArrayElements = 65536;
		constexpr int64 MaximumContainerMemory = 64ll * 1024 * 1024;
		constexpr int32 MaximumStringCharacters = NAME_SIZE - 1;
		static_assert(PLATFORM_LITTLE_ENDIAN, "Recheck native SaveGame wire decoding before adding a big-endian target.");

		struct FBudget
		{
			int32 Tags = 0;
			int32 TypeNodes = 0;
			int64 Elements = 0;
			int64 ContainerMemory = 0;
		};

		class FBytes
		{
		public:
			const TArray<uint8>& Data;
			int64 Position;
			const int64 End;

			FBytes(const TArray<uint8>& InData, int64 Start, int64 InEnd)
				: Data(InData), Position(Start), End(InEnd) {}
			bool Has(int64 Count) const
			{
				return Position >= 0 && Position <= End && End <= Data.Num()
					&& Count >= 0 && Count <= End - Position;
			}
			bool Copy(void* Destination, int64 Count)
			{
				if (!Has(Count)) return false;
				if (Destination && Count) FMemory::Memcpy(Destination, Data.GetData() + Position, Count);
				Position += Count;
				return true;
			}
			template <typename T> bool Read(T& Value) { return Copy(&Value, sizeof(T)); }

			bool String(FString& Value)
			{
				int32 SignedCount = 0;
				if (!Read(SignedCount) || SignedCount == MIN_int32) return false;
				if (!SignedCount) { Value.Empty(); return true; }
				const bool bWide = SignedCount < 0;
				const int64 Count = bWide ? -static_cast<int64>(SignedCount) : SignedCount;
				const int64 Width = bWide ? 2 : 1;
				if (Count > MaximumStringCharacters + 1 || !Has(Count * Width)) return false;
				bool bLowSurrogateRequired = false;
				TArray<TCHAR, TInlineAllocator<NAME_SIZE>> Characters;
				for (int64 Index = 0; Index < Count; ++Index)
				{
					const int64 Offset = Position + Index * Width;
					const uint16 Unit = bWide ? static_cast<uint16>(Data[Offset] | (static_cast<uint16>(Data[Offset + 1]) << 8)) : Data[Offset];
					if (Index == Count - 1)
					{
						if (Unit != 0 || bLowSurrogateRequired) return false;
						continue;
					}
					if (!Unit) return false;
					if (bWide)
					{
						if (bLowSurrogateRequired)
						{
							if (Unit < 0xdc00 || Unit > 0xdfff) return false;
							bLowSurrogateRequired = false;
						}
						else if (Unit >= 0xd800 && Unit <= 0xdbff) bLowSurrogateRequired = true;
						else if ((Unit >= 0xdc00 && Unit <= 0xdfff) || Unit == 0xffff) return false;
					}
					// Windows persistent FString wire data is ANSI or UTF-16LE.
					Characters.Add(static_cast<TCHAR>(Unit));
				}
				Value = FString(Characters.Num(), Characters.GetData());
				Position += Count * Width;
				return true;
			}
		};

		struct FTypeNode { FString Name; int32 Children = 0; };

		class FDecoder
		{
		public:
			FBudget Budget;
			FString Reason;

			bool Type(FBytes& Reader, TArray<FTypeNode, TInlineAllocator<8>>& Nodes, int32 Depth = 0)
			{
				if (Depth >= MaximumDepth || Nodes.Num() >= MaximumTypeNodes
					|| ++Budget.TypeNodes > MaximumTotalTypeNodes) return false;
				FTypeNode Node;
				if (!Reader.String(Node.Name) || Node.Name.IsEmpty() || !Reader.Read(Node.Children)
					|| Node.Children < 0 || Node.Children > 8) return false;
				const int32 Children = Node.Children;
				Nodes.Add(MoveTemp(Node));
				for (int32 Index = 0; Index < Children; ++Index)
					if (!Type(Reader, Nodes, Depth + 1)) return false;
				return true;
			}

			bool Matches(const TArray<FTypeNode, TInlineAllocator<8>>& Nodes, int32& Index,
				UE::FPropertyTypeName Expected) const
			{
				if (!Nodes.IsValidIndex(Index)) return false;
				const FTypeNode& Node = Nodes[Index++];
				if (!Node.Name.Equals(Expected.GetName().ToString(), ESearchCase::IgnoreCase)
					|| Node.Children != Expected.GetParameterCount()) return false;
				for (int32 Child = 0; Child < Node.Children; ++Child)
					if (!Matches(Nodes, Index, Expected.GetParameter(Child))) return false;
				return true;
			}

			const FProperty* FindProperty(const UStruct* Schema, const FString& Name) const
			{
				// Do not construct an input-derived FName just to discover that a
				// legacy/unknown field is absent from this trusted schema.
				for (TFieldIterator<FProperty> It(Schema); It; ++It)
					if (Name.Equals(It->GetName(), ESearchCase::IgnoreCase)) return *It;
				return nullptr;
			}

			bool Tagged(FBytes& Reader, const UStruct* Schema, void* Target, int32 Depth)
			{
				if (!Schema || Depth >= MaximumDepth) return false;
				TSet<FString> Seen;
				while (Reader.Has(4))
				{
					if (++Budget.Tags > MaximumTags) return false;
					FString Name;
					if (!Reader.String(Name) || Name.IsEmpty()) return false;
					if (Name.Equals(TEXT("None"), ESearchCase::IgnoreCase)) return true;
					Reason = FString::Printf(TEXT("%s.%s at byte %lld"), *Schema->GetName(), *Name, Reader.Position);
					TArray<FTypeNode, TInlineAllocator<8>> Nodes;
					int32 Size = 0, ArrayIndex = 0;
					uint8 Flags = 0;
					if (!Type(Reader, Nodes) || !Reader.Read(Size) || Size < 0 || !Reader.Read(Flags)
						// Native project snapshots do not use overridable or skipped
						// property extensions. Never feed those into engine managers.
						|| (Flags & ~(0x01 | 0x02 | 0x08 | 0x10)) != 0) return false;
					if ((Flags & 0x01) && (!Reader.Read(ArrayIndex) || ArrayIndex < 0)) return false;
					if ((Flags & 0x02) && !Reader.Copy(nullptr, 16)) return false;
					if (!Reader.Has(Size)) return false;
					const int64 End = Reader.Position + Size;
					const FProperty* Property = FindProperty(Schema, Name);
					if (Property)
					{
						if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)
							|| ArrayIndex >= Property->ArrayDim) return false;
						const FString Key = Name.ToLower() + FString::Printf(TEXT("[%d]"), ArrayIndex);
						if (Seen.Contains(Key)) return false;
						Seen.Add(Key);
						int32 TypeIndex = 0;
						if (!Matches(Nodes, TypeIndex, UE::FPropertyTypeName(Property)) || TypeIndex != Nodes.Num()) return false;
						void* Value = Target ? Property->ContainerPtrToValuePtr<void>(Target, ArrayIndex) : nullptr;
						FBytes Payload(Reader.Data, Reader.Position, End);
						if (const FBoolProperty* Bool = CastField<FBoolProperty>(Property))
						{
							if (Size != 0) return false;
							if (Value) Bool->SetPropertyValue(Value, (Flags & 0x10) != 0);
						}
						else if ((Flags & 0x10) || !ValueOf(Payload, Property, Value, Depth + 1) || Payload.Position != End) return false;
					}
					// Removed legacy fields are opaque length-delimited values. Do
					// not construct a temporary property, load a type or inspect them.
					Reader.Position = End;
				}
				return false;
			}

			bool EnumValue(FBytes& Reader, const UEnum* Enum, const FNumericProperty* Storage, void* Target)
			{
				FString Name;
				if (!Reader.String(Name) || !Enum || !Storage) return false;
				if (Target)
				{
					int64 Value = 0;
					if (Enum->HasAnyEnumFlags(EEnumFlags::Flags))
					{
						Value = Enum->GetValueOrBitfieldFromString(Name);
						if (!Enum->IsValidEnumValueOrBitfield(Value)) Value = 0;
					}
					else
					{
						const int32 Index = Enum->GetIndexByName(FName(*Name));
						Value = Index == INDEX_NONE ? Enum->GetMaxEnumValue() : Enum->GetValueByIndex(Index);
					}
					Storage->SetIntPropertyValue(Target, static_cast<uint64>(Value));
				}
				return true;
			}

			bool ValueOf(FBytes& Reader, const FProperty* Property, void* Target, int32 Depth)
			{
				if (Depth >= MaximumDepth) return false;
				if (const FEnumProperty* Enum = CastField<FEnumProperty>(Property))
					return EnumValue(Reader, Enum->GetEnum(), Enum->GetUnderlyingProperty(), Target);
				if (const FByteProperty* Byte = CastField<FByteProperty>(Property); Byte && Byte->Enum)
					return EnumValue(Reader, Byte->Enum, Byte, Target);
				if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
				{
					const int32 Size = Numeric->GetElementSize();
					if (Size <= 0 || Size > 8) return false;
					uint8 Raw[8] = {};
					if (!Reader.Copy(Raw, Size)) return false;
					if (Numeric->IsFloatingPoint())
					{
						if (Size == 4) { float V; FMemory::Memcpy(&V, Raw, 4); if (!FMath::IsFinite(V)) return false; }
						else if (Size == 8) { double V; FMemory::Memcpy(&V, Raw, 8); if (!FMath::IsFinite(V)) return false; }
						else return false;
					}
					if (Target) FMemory::Memcpy(Target, Raw, Size);
					return true;
				}
				if (const FNameProperty* Name = CastField<FNameProperty>(Property))
				{
					FString Value;
					if (!Reader.String(Value)) return false;
					if (Target) Name->SetPropertyValue(Target, FName(*Value));
					return true;
				}
				if (const FBoolProperty* Bool = CastField<FBoolProperty>(Property))
				{
					uint8 Value = 0;
					if (!Reader.Read(Value) || Value > 1) return false;
					if (Target) Bool->SetPropertyValue(Target, Value != 0);
					return true;
				}
				if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
				{
					if (Struct->Struct == TBaseStructure<FGuid>::Get()) return Reader.Copy(Target, 16);
					// No other native/custom binary serializers exist in the current
					// persistent graph. New types require explicit audited support.
					const UScriptStruct::ICppStructOps* Ops = Struct->Struct->GetCppStructOps();
					if ((Struct->Struct->StructFlags & STRUCT_SerializeNative) != 0
						|| (Ops && (Ops->HasSerializer() || Ops->HasStructuredSerializer()))) return false;
					return Tagged(Reader, Struct->Struct, Target, Depth + 1);
				}
				if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
				{
					int32 Count = 0;
					if (!Reader.Read(Count) || Count < 0 || Count > MaximumArrayElements
						|| Count > Reader.End - Reader.Position
						|| Budget.Elements + Count > MaximumArrayElements) return false;
					const int64 Memory = static_cast<int64>(Count) * Array->Inner->GetElementSize();
					if (Memory < 0 || Memory > MaximumContainerMemory - Budget.ContainerMemory) return false;
					Budget.Elements += Count;
					Budget.ContainerMemory += Memory;
					if (Target)
					{
						FScriptArrayHelper Helper(Array, Target);
						Helper.EmptyAndAddValues(Count);
						for (int32 Index = 0; Index < Count; ++Index)
							if (!ValueOf(Reader, Array->Inner, Helper.GetRawPtr(Index), Depth + 1)) return false;
					}
					else for (int32 Index = 0; Index < Count; ++Index)
						if (!ValueOf(Reader, Array->Inner, nullptr, Depth + 1)) return false;
					return true;
				}
				return false;
			}
		};
	}

	bool ReadPayload(const TArray<uint8>& Bytes, const FHeader& Header,
		UImmortalPathSaveGame* Target, FString* OutReason)
	{
		FBytes Reader(Bytes, Header.PayloadOffset, Bytes.Num());
		FDecoder Decoder;
		bool bValid = Header.PayloadOffset > 0 && Reader.Has(1);
		if (bValid && Header.PackageVersion >= EUnrealEngineObjectUE5Version::PROPERTY_TAG_EXTENSION_AND_OVERRIDABLE_SERIALIZATION)
		{
			uint8 Control = 0;
			bValid = Reader.Read(Control) && Control == 0;
		}
		// UE's delta serialization omits values equal to the class defaults,
		// including SaveVersion == CurrentSaveVersion. Missing tags must retain
		// the constructed defaults, exactly as the engine loader does.
		bValid = bValid && Decoder.Tagged(Reader, UImmortalPathSaveGame::StaticClass(), Target, 0);
		// UObject's binary structured archive writes a uint32 optional GUID
		// presence after the root None tag. The value snapshot has no references
		// to this transient UObject identity; persistent item GUID fields above
		// are decoded in full and are not confused with this annotation.
		uint32 HasObjectGuid = 0;
		bValid = bValid && Reader.Read(HasObjectGuid) && HasObjectGuid <= 1;
		if (bValid && HasObjectGuid)
		{
			FGuid Guid;
			bValid = Reader.Read(Guid) && Guid.IsValid();
		}
		bValid = bValid && Reader.Position == Reader.End;
		if (OutReason) *OutReason = bValid ? FString() : Decoder.Reason;
		return bValid;
	}
}
