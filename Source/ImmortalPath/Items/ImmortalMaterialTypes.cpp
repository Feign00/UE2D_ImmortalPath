// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalMaterialTypes.h"

#include "Engine/DataTable.h"
#include "Misc/PackageName.h"

namespace
{
	const FName SpiritGrassId(TEXT("SpiritGrass"));
	const FName DemonCoreId(TEXT("DemonCore"));
	const FName SpiritLiquidId(TEXT("SpiritLiquid"));
	const FName OreId(TEXT("Ore"));
	const FName DemonBoneId(TEXT("DemonBone"));
	const FName ArtifactFragmentId(TEXT("ArtifactFragment"));
	const FName SpiritIronId(TEXT("SpiritIron"));

	FImmortalMaterialDefinition MakeDefinition(
		const TCHAR* Name,
		const TCHAR* Description,
		const EImmortalMaterialCategory Category,
		const FLinearColor& Color,
		const TCHAR* Glyph,
		const int32 MinimumStage,
		const float Weight)
	{
		FImmortalMaterialDefinition Definition;
		Definition.DisplayName = FText::FromString(Name);
		Definition.Description = FText::FromString(Description);
		Definition.Category = Category;
		Definition.DisplayColor = Color;
		Definition.IconGlyph = FText::FromString(Glyph);
		Definition.MinimumQingyunStage = MinimumStage;
		Definition.DropWeight = Weight;
		return Definition;
	}

	const TMap<FName, FImmortalMaterialDefinition>& GetFallbackCatalog()
	{
		static const TMap<FName, FImmortalMaterialDefinition> Catalog =
		{
			{SpiritGrassId, MakeDefinition(TEXT("灵草"), TEXT("吸收山野灵气生长的草药，是炼制基础丹药的主要材料。"), EImmortalMaterialCategory::Herb, FLinearColor(0.28f, 0.95f, 0.38f), TEXT("草"), 1, 30.0f)},
			{DemonCoreId, MakeDefinition(TEXT("妖丹"), TEXT("妖兽体内凝结的精华，可用于炼丹、炼器与境界相关配方。"), EImmortalMaterialCategory::Monster, FLinearColor(0.95f, 0.32f, 0.25f), TEXT("丹"), 1, 24.0f)},
			{SpiritLiquidId, MakeDefinition(TEXT("灵液"), TEXT("蕴含纯净灵力的液体，可稳定炼丹过程并提升成丹品质。"), EImmortalMaterialCategory::Essence, FLinearColor(0.25f, 0.82f, 1.0f), TEXT("液"), 1, 18.0f)},
			{OreId, MakeDefinition(TEXT("矿石"), TEXT("青云山出产的普通灵矿，是打造武器与护甲的基础材料。"), EImmortalMaterialCategory::Mineral, FLinearColor(0.72f, 0.74f, 0.78f), TEXT("矿"), 1, 22.0f)},
			{DemonBoneId, MakeDefinition(TEXT("妖骨"), TEXT("坚韧的妖兽骨骼，适合用于强化护甲与法宝结构。"), EImmortalMaterialCategory::Monster, FLinearColor(0.92f, 0.84f, 0.66f), TEXT("骨"), 20, 10.0f)},
			{ArtifactFragmentId, MakeDefinition(TEXT("法宝碎片"), TEXT("破损法宝残留的灵性碎片，后续可用于打造与升星法宝。"), EImmortalMaterialCategory::Artifact, FLinearColor(0.78f, 0.35f, 1.0f), TEXT("片"), 10, 2.0f)},
			{SpiritIronId, MakeDefinition(TEXT("灵铁"), TEXT("被灵脉长期淬炼的金属，是炼制高阶装备的重要材料。"), EImmortalMaterialCategory::Mineral, FLinearColor(1.0f, 0.67f, 0.22f), TEXT("铁"), 50, 6.0f)}
		};
		return Catalog;
	}

	UDataTable* GetOptionalMaterialTable()
	{
		static TWeakObjectPtr<UDataTable> CachedTable;
		static bool bAttemptedLoad = false;
		if (!bAttemptedLoad)
		{
			bAttemptedLoad = true;
			const FString PackageName(TEXT("/Game/GAME/Data/DT_Materials"));
			if (FPackageName::DoesPackageExist(PackageName))
			{
				CachedTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/GAME/Data/DT_Materials.DT_Materials"));
			}
		}
		return CachedTable.Get();
	}
}

TArray<FName> UImmortalMaterialLibrary::GetKnownMaterialIds()
{
	TArray<FName> Result;
	GetFallbackCatalog().GetKeys(Result);
	if (const UDataTable* Table = GetOptionalMaterialTable())
	{
		for (const FName RowName : Table->GetRowNames())
		{
			Result.AddUnique(RowName);
		}
	}
	Result.Sort(FNameLexicalLess());
	return Result;
}

bool UImmortalMaterialLibrary::GetMaterialDefinition(
	const FName MaterialId,
	FImmortalMaterialDefinition& OutDefinition)
{
	if (MaterialId.IsNone())
	{
		return false;
	}
	if (const UDataTable* Table = GetOptionalMaterialTable())
	{
		if (const FImmortalMaterialDefinition* Row = Table->FindRow<FImmortalMaterialDefinition>(MaterialId, TEXT("Material lookup"), false))
		{
			OutDefinition = *Row;
			return true;
		}
	}
	if (const FImmortalMaterialDefinition* Definition = GetFallbackCatalog().Find(MaterialId))
	{
		OutDefinition = *Definition;
		return true;
	}
	return false;
}

FText UImmortalMaterialLibrary::GetCategoryText(const EImmortalMaterialCategory Category)
{
	switch (Category)
	{
	case EImmortalMaterialCategory::Herb: return FText::FromString(TEXT("灵植"));
	case EImmortalMaterialCategory::Monster: return FText::FromString(TEXT("妖兽材料"));
	case EImmortalMaterialCategory::Essence: return FText::FromString(TEXT("灵液精华"));
	case EImmortalMaterialCategory::Mineral: return FText::FromString(TEXT("矿物"));
	case EImmortalMaterialCategory::Artifact: return FText::FromString(TEXT("法宝材料"));
	default: return FText::FromString(TEXT("材料"));
	}
}

int32 UImmortalMaterialLibrary::GetMaterialQuantity(
	const TArray<FImmortalMaterialStack>& Inventory,
	const FName MaterialId)
{
	int64 Total = 0;
	for (const FImmortalMaterialStack& Stack : Inventory)
	{
		if (Stack.MaterialId == MaterialId && Stack.Quantity > 0)
		{
			Total = FMath::Min<int64>(Total + Stack.Quantity, MAX_int32);
		}
	}
	return static_cast<int32>(Total);
}

int32 UImmortalMaterialLibrary::AddMaterialStack(
	TArray<FImmortalMaterialStack>& Inventory,
	const FName MaterialId,
	const int32 Amount)
{
	FImmortalMaterialDefinition Definition;
	if (Amount <= 0 || !GetMaterialDefinition(MaterialId, Definition))
	{
		return 0;
	}
	FImmortalMaterialStack* Existing = Inventory.FindByPredicate([MaterialId](const FImmortalMaterialStack& Stack)
	{
		return Stack.MaterialId == MaterialId;
	});
	if (!Existing)
	{
		FImmortalMaterialStack& NewStack = Inventory.AddDefaulted_GetRef();
		NewStack.MaterialId = MaterialId;
		NewStack.Quantity = FMath::Min(Amount, FMath::Max(Definition.MaximumStack, 1));
		return NewStack.Quantity;
	}

	const int32 MaximumStack = FMath::Max(Definition.MaximumStack, 1);
	const int32 Previous = FMath::Clamp(Existing->Quantity, 0, MaximumStack);
	Existing->Quantity = static_cast<int32>(FMath::Min<int64>(
		static_cast<int64>(Previous) + Amount,
		MaximumStack));
	return Existing->Quantity - Previous;
}

bool UImmortalMaterialLibrary::RemoveMaterialStack(
	TArray<FImmortalMaterialStack>& Inventory,
	const FName MaterialId,
	const int32 Amount)
{
	if (Amount <= 0 || GetMaterialQuantity(Inventory, MaterialId) < Amount)
	{
		return false;
	}
	int32 Remaining = Amount;
	for (int32 Index = Inventory.Num() - 1; Index >= 0 && Remaining > 0; --Index)
	{
		FImmortalMaterialStack& Stack = Inventory[Index];
		if (Stack.MaterialId != MaterialId || Stack.Quantity <= 0) continue;
		const int32 Removed = FMath::Min(Stack.Quantity, Remaining);
		Stack.Quantity -= Removed;
		Remaining -= Removed;
		if (Stack.Quantity <= 0) Inventory.RemoveAt(Index);
	}
	NormalizeInventory(Inventory);
	return Remaining == 0;
}

void UImmortalMaterialLibrary::NormalizeInventory(TArray<FImmortalMaterialStack>& Inventory)
{
	TArray<FImmortalMaterialStack> Normalized;
	for (const FImmortalMaterialStack& Stack : Inventory)
	{
		if (Stack.Quantity > 0)
		{
			AddMaterialStack(Normalized, Stack.MaterialId, Stack.Quantity);
		}
	}
	Normalized.Sort([](const FImmortalMaterialStack& Left, const FImmortalMaterialStack& Right)
	{
		return Left.MaterialId.LexicalLess(Right.MaterialId);
	});
	Inventory = MoveTemp(Normalized);
}

FImmortalMaterialStack UImmortalMaterialLibrary::GenerateStageDrop(
	const int32 QingyunStage,
	const bool bBossDrop,
	const int32 DropIndex)
{
	const int32 SafeStage = FMath::Clamp(QingyunStage, 1, 999);
	FImmortalMaterialStack Result;
	if (bBossDrop && DropIndex == 0)
	{
		Result.MaterialId = DemonCoreId;
		Result.Quantity = 2 + SafeStage / 100;
		return Result;
	}
	if (bBossDrop && DropIndex == 2)
	{
		Result.MaterialId = ArtifactFragmentId;
		Result.Quantity = 1 + SafeStage / 300;
		return Result;
	}

	struct FCandidate
	{
		FName Id;
		float Weight = 0.0f;
	};
	TArray<FCandidate> Candidates;
	float TotalWeight = 0.0f;
	for (const FName Id : GetKnownMaterialIds())
	{
		FImmortalMaterialDefinition Definition;
		if (GetMaterialDefinition(Id, Definition)
			&& SafeStage >= FMath::Max(Definition.MinimumQingyunStage, 1)
			&& Definition.DropWeight > 0.0f)
		{
			Candidates.Add({Id, Definition.DropWeight});
			TotalWeight += Definition.DropWeight;
		}
	}
	if (Candidates.IsEmpty() || TotalWeight <= 0.0f)
	{
		Result.MaterialId = SpiritGrassId;
	}
	else
	{
		float Roll = FMath::FRandRange(0.0f, TotalWeight);
		Result.MaterialId = Candidates.Last().Id;
		for (const FCandidate& Candidate : Candidates)
		{
			Roll -= Candidate.Weight;
			if (Roll <= 0.0f)
			{
				Result.MaterialId = Candidate.Id;
				break;
			}
		}
	}

	Result.Quantity = 1 + SafeStage / 250;
	if (bBossDrop)
	{
		Result.Quantity *= 2;
	}
	if (Result.MaterialId == ArtifactFragmentId)
	{
		Result.Quantity = FMath::Max(Result.Quantity / 2, 1);
	}
	return Result;
}
