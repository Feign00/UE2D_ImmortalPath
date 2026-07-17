// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalCraftingTypes.h"

#include "Engine/DataTable.h"
#include "Misc/PackageName.h"

namespace
{
	FImmortalCraftingMaterialCost CraftingIngredient(const TCHAR* MaterialId, const int32 Quantity)
	{
		FImmortalCraftingMaterialCost Result;
		Result.MaterialId = FName(MaterialId);
		Result.Quantity = Quantity;
		return Result;
	}

	FImmortalCraftingCost CraftingCost(
		const int32 SpiritStones,
		std::initializer_list<FImmortalCraftingMaterialCost> Materials)
	{
		FImmortalCraftingCost Result;
		Result.SpiritStones = SpiritStones;
		for (const FImmortalCraftingMaterialCost& Material : Materials) Result.Materials.Add(Material);
		return Result;
	}

	FImmortalCraftingRecipeDefinition CraftingRecipe(
		const TCHAR* Name,
		const TCHAR* Description,
		const EImmortalEquipmentSlot Slot,
		const EImmortalEquipmentQuality Quality,
		const int32 Stage,
		const FImmortalCraftingCost& Cost)
	{
		FImmortalCraftingRecipeDefinition Result;
		Result.DisplayName = FText::FromString(Name);
		Result.Description = FText::FromString(Description);
		Result.OutputSlot = Slot;
		Result.OutputQuality = Quality;
		Result.MinimumQingyunStage = Stage;
		Result.Cost = Cost;
		return Result;
	}

	const TMap<FName, FImmortalCraftingRecipeDefinition>& GetCraftingFallbackCatalog()
	{
		static const TMap<FName, FImmortalCraftingRecipeDefinition> Catalog =
		{
			{TEXT("QingyunSword"), CraftingRecipe(TEXT("青云剑"), TEXT("基础攻击较高的青云山制式灵剑。"),
				EImmortalEquipmentSlot::Weapon, EImmortalEquipmentQuality::Uncommon, 1,
				CraftingCost(80, {CraftingIngredient(TEXT("Ore"), 4), CraftingIngredient(TEXT("DemonBone"), 1)}))},
			{TEXT("SpiritCrown"), CraftingRecipe(TEXT("蕴灵冠"), TEXT("兼顾生命与防御的护首。"),
				EImmortalEquipmentSlot::Head, EImmortalEquipmentQuality::Uncommon, 1,
				CraftingCost(70, {CraftingIngredient(TEXT("Ore"), 3), CraftingIngredient(TEXT("DemonBone"), 2)}))},
			{TEXT("CloudRobe"), CraftingRecipe(TEXT("流云法衣"), TEXT("以灵铁加固的稀有护甲。"),
				EImmortalEquipmentSlot::Chest, EImmortalEquipmentQuality::Rare, 20,
				CraftingCost(140, {CraftingIngredient(TEXT("Ore"), 5), CraftingIngredient(TEXT("DemonBone"), 3), CraftingIngredient(TEXT("SpiritIron"), 1)}))},
			{TEXT("WindBoots"), CraftingRecipe(TEXT("踏风履"), TEXT("蕴含风灵的轻便战靴。"),
				EImmortalEquipmentSlot::Boots, EImmortalEquipmentQuality::Uncommon, 10,
				CraftingCost(90, {CraftingIngredient(TEXT("Ore"), 3), CraftingIngredient(TEXT("DemonBone"), 1), CraftingIngredient(TEXT("SpiritIron"), 1)}))},
			{TEXT("HeartJade"), CraftingRecipe(TEXT("护心灵玉"), TEXT("以法宝碎片炼成的稀有随身饰物；独立法宝将在法宝系统中打造。"),
				EImmortalEquipmentSlot::Accessory, EImmortalEquipmentQuality::Rare, 10,
				CraftingCost(160, {CraftingIngredient(TEXT("SpiritIron"), 2), CraftingIngredient(TEXT("ArtifactFragment"), 1)}))}
		};
		return Catalog;
	}

	UDataTable* GetOptionalCraftingRecipeTable()
	{
		static TWeakObjectPtr<UDataTable> CachedTable;
		static bool bAttemptedLoad = false;
		if (!bAttemptedLoad)
		{
			bAttemptedLoad = true;
			const FString PackageName(TEXT("/Game/GAME/Data/DT_CraftingRecipes"));
			if (FPackageName::DoesPackageExist(PackageName))
			{
				CachedTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/GAME/Data/DT_CraftingRecipes.DT_CraftingRecipes"));
			}
		}
		return CachedTable.Get();
	}
}

TArray<FName> UImmortalCraftingLibrary::GetKnownRecipeIds()
{
	TArray<FName> Result;
	GetCraftingFallbackCatalog().GetKeys(Result);
	if (const UDataTable* Table = GetOptionalCraftingRecipeTable())
	{
		for (const FName RowName : Table->GetRowNames()) Result.AddUnique(RowName);
	}
	Result.Sort(FNameLexicalLess());
	return Result;
}

bool UImmortalCraftingLibrary::GetRecipeDefinition(
	const FName RecipeId,
	FImmortalCraftingRecipeDefinition& OutDefinition)
{
	if (RecipeId.IsNone()) return false;
	if (const UDataTable* Table = GetOptionalCraftingRecipeTable())
	{
		if (const FImmortalCraftingRecipeDefinition* Row = Table->FindRow<FImmortalCraftingRecipeDefinition>(
			RecipeId, TEXT("Crafting recipe lookup"), false))
		{
			OutDefinition = *Row;
			return true;
		}
	}
	if (const FImmortalCraftingRecipeDefinition* Found = GetCraftingFallbackCatalog().Find(RecipeId))
	{
		OutDefinition = *Found;
		return true;
	}
	return false;
}

bool UImmortalCraftingLibrary::IsRecipeUnlocked(
	const FImmortalCraftingRecipeDefinition& Definition,
	const int32 QingyunStage)
{
	return QingyunStage >= FMath::Max(Definition.MinimumQingyunStage, 1);
}

bool UImmortalCraftingLibrary::CanAfford(
	const TArray<FImmortalMaterialStack>& Materials,
	const int32 SpiritStones,
	const FImmortalCraftingCost& Cost)
{
	if (SpiritStones < FMath::Max(Cost.SpiritStones, 0)) return false;
	for (const FImmortalCraftingMaterialCost& Material : Cost.Materials)
	{
		if (Material.MaterialId.IsNone() || Material.Quantity <= 0
			|| UImmortalMaterialLibrary::GetMaterialQuantity(Materials, Material.MaterialId) < Material.Quantity)
		{
			return false;
		}
	}
	return true;
}

bool UImmortalCraftingLibrary::ConsumeCost(
	TArray<FImmortalMaterialStack>& Materials,
	int32& SpiritStones,
	const FImmortalCraftingCost& Cost)
{
	if (!CanAfford(Materials, SpiritStones, Cost)) return false;
	for (const FImmortalCraftingMaterialCost& Material : Cost.Materials)
	{
		int32 Remaining = Material.Quantity;
		for (int32 Index = Materials.Num() - 1; Index >= 0 && Remaining > 0; --Index)
		{
			FImmortalMaterialStack& Stack = Materials[Index];
			if (Stack.MaterialId != Material.MaterialId || Stack.Quantity <= 0) continue;
			const int32 Removed = FMath::Min(Stack.Quantity, Remaining);
			Stack.Quantity -= Removed;
			Remaining -= Removed;
			if (Stack.Quantity <= 0) Materials.RemoveAt(Index);
		}
	}
	SpiritStones -= FMath::Max(Cost.SpiritStones, 0);
	UImmortalMaterialLibrary::NormalizeInventory(Materials);
	return true;
}

FImmortalCraftingCost UImmortalCraftingLibrary::GetEnhancementCost(const FImmortalEquipmentItem& Item)
{
	if (!Item.IsValid() || Item.EnhancementLevel >= 15) return FImmortalCraftingCost();
	const int32 NextLevel = Item.EnhancementLevel + 1;
	FImmortalCraftingCost Result = CraftingCost(
		25 * NextLevel * NextLevel,
		{CraftingIngredient(TEXT("Ore"), 2 + Item.EnhancementLevel)});
	if (NextLevel >= 4) Result.Materials.Add(CraftingIngredient(TEXT("SpiritIron"), 1 + NextLevel / 6));
	return Result;
}

FImmortalCraftingCost UImmortalCraftingLibrary::GetRefinementCost(const FImmortalEquipmentItem& Item)
{
	if (!Item.IsValid()) return FImmortalCraftingCost();
	const int32 QualityRank = static_cast<int32>(Item.Quality) + 1;
	FImmortalCraftingCost Result = CraftingCost(
		60 * QualityRank + FMath::Min(Item.RefinementCount, 20) * 10,
		{CraftingIngredient(TEXT("DemonBone"), 1 + QualityRank)});
	if (Item.Quality >= EImmortalEquipmentQuality::Rare) Result.Materials.Add(CraftingIngredient(TEXT("SpiritIron"), 1));
	if (Item.Quality >= EImmortalEquipmentQuality::Epic) Result.Materials.Add(CraftingIngredient(TEXT("ArtifactFragment"), 1));
	return Result;
}

FText UImmortalCraftingLibrary::FormatCost(
	const FImmortalCraftingCost& Cost,
	const TArray<FImmortalMaterialStack>& Materials,
	const int32 SpiritStones)
{
	FString Result = FString::Printf(TEXT("灵石 %d / %d"), SpiritStones, Cost.SpiritStones);
	for (const FImmortalCraftingMaterialCost& Entry : Cost.Materials)
	{
		FImmortalMaterialDefinition Material;
		UImmortalMaterialLibrary::GetMaterialDefinition(Entry.MaterialId, Material);
		Result += FString::Printf(TEXT("\n%s %d / %d"),
			*Material.DisplayName.ToString(),
			UImmortalMaterialLibrary::GetMaterialQuantity(Materials, Entry.MaterialId),
			Entry.Quantity);
	}
	return FText::FromString(Result);
}
