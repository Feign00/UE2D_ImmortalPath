// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalAlchemyTypes.h"

#include "Engine/DataTable.h"
#include "Misc/PackageName.h"

namespace
{
	const FName HealingPillId(TEXT("HealingPill"));
	const FName QiGatheringPillId(TEXT("QiGatheringPill"));
	const FName FoundationPillId(TEXT("FoundationPill"));
	const FName EnlightenmentPillId(TEXT("EnlightenmentPill"));
	const FName BreakthroughPillId(TEXT("BreakthroughPill"));

	FImmortalAlchemyIngredient Ingredient(const TCHAR* MaterialId, const int32 Quantity)
	{
		FImmortalAlchemyIngredient Result;
		Result.MaterialId = FName(MaterialId);
		Result.Quantity = Quantity;
		return Result;
	}

	FImmortalPillDefinition MakeDefinition(
		const TCHAR* Name,
		const TCHAR* Description,
		const TCHAR* Glyph,
		const FLinearColor& Color,
		const TArray<FImmortalAlchemyIngredient>& Ingredients,
		const float SuccessChance,
		const float ExceptionalChance,
		const int32 RealmIndex,
		const int32 MinorStage,
		const EImmortalPillEffect Effect,
		const float OrdinaryMagnitude,
		const float ExceptionalMagnitude,
		const float OrdinaryDuration = 0.0f,
		const float ExceptionalDuration = 0.0f)
	{
		FImmortalPillDefinition Definition;
		Definition.DisplayName = FText::FromString(Name);
		Definition.Description = FText::FromString(Description);
		Definition.IconGlyph = FText::FromString(Glyph);
		Definition.DisplayColor = Color;
		Definition.Ingredients = Ingredients;
		Definition.BaseSuccessChance = SuccessChance;
		Definition.ExceptionalChance = ExceptionalChance;
		Definition.MinimumRealmIndex = RealmIndex;
		Definition.MinimumMinorStage = MinorStage;
		Definition.Effect = Effect;
		Definition.OrdinaryMagnitude = OrdinaryMagnitude;
		Definition.ExceptionalMagnitude = ExceptionalMagnitude;
		Definition.OrdinaryDurationSeconds = OrdinaryDuration;
		Definition.ExceptionalDurationSeconds = ExceptionalDuration;
		return Definition;
	}

	const TMap<FName, FImmortalPillDefinition>& GetAlchemyFallbackCatalog()
	{
		static const TMap<FName, FImmortalPillDefinition> Catalog =
		{
			{HealingPillId, MakeDefinition(
				TEXT("回血丹"), TEXT("温养经脉并恢复生命。满生命或死亡时不会浪费丹药。"), TEXT("血"),
				FLinearColor(1.0f, 0.28f, 0.30f),
				{Ingredient(TEXT("SpiritGrass"), 2), Ingredient(TEXT("SpiritLiquid"), 1)},
				0.90f, 0.15f, 0, 1, EImmortalPillEffect::RestoreHealth, 0.50f, 1.00f)},
			{QiGatheringPillId, MakeDefinition(
				TEXT("聚气丹"), TEXT("迅速炼化灵气，直接补充当前境界所需修为。"), TEXT("气"),
				FLinearColor(0.25f, 0.88f, 1.0f),
				{Ingredient(TEXT("SpiritGrass"), 3), Ingredient(TEXT("SpiritLiquid"), 2), Ingredient(TEXT("DemonCore"), 1)},
				0.82f, 0.12f, 0, 1, EImmortalPillEffect::GrantCultivationPercent, 0.20f, 0.45f)},
			{FoundationPillId, MakeDefinition(
				TEXT("筑基丹"), TEXT("稳固道基并提供大量修为，炼气后期即可学习丹方。"), TEXT("基"),
				FLinearColor(0.92f, 0.78f, 0.24f),
				{Ingredient(TEXT("SpiritGrass"), 4), Ingredient(TEXT("DemonCore"), 3), Ingredient(TEXT("SpiritLiquid"), 2)},
				0.72f, 0.10f, 0, 5, EImmortalPillEffect::GrantCultivationPercent, 0.50f, 1.00f)},
			{EnlightenmentPillId, MakeDefinition(
				TEXT("悟道丹"), TEXT("进入悟道状态，提高在线修炼速度；离线期间增益时间暂停。"), TEXT("悟"),
				FLinearColor(0.72f, 0.36f, 1.0f),
				{Ingredient(TEXT("SpiritGrass"), 4), Ingredient(TEXT("SpiritLiquid"), 3), Ingredient(TEXT("DemonCore"), 2)},
				0.65f, 0.08f, 1, 1, EImmortalPillEffect::CultivationRateBoost, 1.50f, 2.00f, 300.0f, 600.0f)},
			{BreakthroughPillId, MakeDefinition(
				TEXT("破境丹"), TEXT("补足当前层级全部修为并立即突破；极品丹额外补充下一层修为。"), TEXT("破"),
				FLinearColor(1.0f, 0.43f, 0.12f),
				{Ingredient(TEXT("SpiritGrass"), 5), Ingredient(TEXT("DemonCore"), 4), Ingredient(TEXT("SpiritLiquid"), 3), Ingredient(TEXT("Ore"), 1)},
				0.55f, 0.06f, 2, 1, EImmortalPillEffect::CompleteCurrentStage, 0.0f, 0.25f)}
		};
		return Catalog;
	}

	UDataTable* GetOptionalRecipeTable()
	{
		static TWeakObjectPtr<UDataTable> CachedTable;
		static bool bAttemptedLoad = false;
		if (!bAttemptedLoad)
		{
			bAttemptedLoad = true;
			const FString PackageName(TEXT("/Game/GAME/Data/DT_AlchemyRecipes"));
			if (FPackageName::DoesPackageExist(PackageName))
			{
				CachedTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/GAME/Data/DT_AlchemyRecipes.DT_AlchemyRecipes"));
			}
		}
		return CachedTable.Get();
	}
}

TArray<FName> UImmortalAlchemyLibrary::GetKnownRecipeIds()
{
	TArray<FName> Result;
	GetAlchemyFallbackCatalog().GetKeys(Result);
	if (const UDataTable* Table = GetOptionalRecipeTable())
	{
		for (const FName RowName : Table->GetRowNames()) Result.AddUnique(RowName);
	}
	Result.Sort(FNameLexicalLess());
	return Result;
}

bool UImmortalAlchemyLibrary::GetPillDefinition(const FName PillId, FImmortalPillDefinition& OutDefinition)
{
	if (PillId.IsNone()) return false;
	if (const UDataTable* Table = GetOptionalRecipeTable())
	{
		if (const FImmortalPillDefinition* Row = Table->FindRow<FImmortalPillDefinition>(PillId, TEXT("Pill lookup"), false))
		{
			OutDefinition = *Row;
			return true;
		}
	}
	if (const FImmortalPillDefinition* Definition = GetAlchemyFallbackCatalog().Find(PillId))
	{
		OutDefinition = *Definition;
		return true;
	}
	return false;
}

bool UImmortalAlchemyLibrary::IsRecipeUnlocked(
	const FImmortalPillDefinition& Definition,
	const int32 RealmIndex,
	const int32 MinorStage)
{
	return RealmIndex > Definition.MinimumRealmIndex
		|| (RealmIndex == Definition.MinimumRealmIndex && MinorStage >= Definition.MinimumMinorStage);
}

bool UImmortalAlchemyLibrary::CanCraft(
	const TArray<FImmortalMaterialStack>& Materials,
	const FImmortalPillDefinition& Definition)
{
	if (Definition.Ingredients.IsEmpty()) return false;
	for (const FImmortalAlchemyIngredient& Cost : Definition.Ingredients)
	{
		if (Cost.MaterialId.IsNone() || Cost.Quantity <= 0
			|| UImmortalMaterialLibrary::GetMaterialQuantity(Materials, Cost.MaterialId) < Cost.Quantity)
		{
			return false;
		}
	}
	return true;
}

bool UImmortalAlchemyLibrary::ConsumeIngredients(
	TArray<FImmortalMaterialStack>& Materials,
	const FImmortalPillDefinition& Definition)
{
	if (!CanCraft(Materials, Definition)) return false;
	for (const FImmortalAlchemyIngredient& Cost : Definition.Ingredients)
	{
		int32 Remaining = Cost.Quantity;
		for (int32 Index = Materials.Num() - 1; Index >= 0 && Remaining > 0; --Index)
		{
			FImmortalMaterialStack& Stack = Materials[Index];
			if (Stack.MaterialId != Cost.MaterialId || Stack.Quantity <= 0) continue;
			const int32 Removed = FMath::Min(Stack.Quantity, Remaining);
			Stack.Quantity -= Removed;
			Remaining -= Removed;
			if (Stack.Quantity <= 0) Materials.RemoveAt(Index);
		}
	}
	UImmortalMaterialLibrary::NormalizeInventory(Materials);
	return true;
}

EImmortalAlchemyOutcome UImmortalAlchemyLibrary::CalculateOutcome(
	const FImmortalPillDefinition& Definition,
	const float Roll,
	const float SuccessChanceBonus,
	const float ExceptionalChanceBonus)
{
	const float SuccessChance = FMath::Clamp(Definition.BaseSuccessChance + SuccessChanceBonus, 0.0f, 1.0f);
	const float ExceptionalChance = FMath::Clamp(
		Definition.ExceptionalChance + ExceptionalChanceBonus, 0.0f, SuccessChance);
	const float SafeRoll = FMath::Clamp(Roll, 0.0f, 1.0f);
	if (SafeRoll >= SuccessChance) return EImmortalAlchemyOutcome::Failure;
	return SafeRoll < ExceptionalChance
		? EImmortalAlchemyOutcome::Exceptional
		: EImmortalAlchemyOutcome::Ordinary;
}

int32 UImmortalAlchemyLibrary::GetPillQuantity(
	const TArray<FImmortalPillStack>& Inventory,
	const FName PillId,
	const EImmortalPillQuality Quality)
{
	int64 Total = 0;
	for (const FImmortalPillStack& Stack : Inventory)
	{
		if (Stack.PillId == PillId && Stack.Quality == Quality && Stack.Quantity > 0)
		{
			Total = FMath::Min<int64>(Total + Stack.Quantity, MAX_int32);
		}
	}
	return static_cast<int32>(Total);
}

int32 UImmortalAlchemyLibrary::AddPillStack(
	TArray<FImmortalPillStack>& Inventory,
	const FName PillId,
	const EImmortalPillQuality Quality,
	const int32 Amount)
{
	FImmortalPillDefinition Definition;
	if (Amount <= 0 || !GetPillDefinition(PillId, Definition)) return 0;
	FImmortalPillStack* Existing = Inventory.FindByPredicate([PillId, Quality](const FImmortalPillStack& Stack)
	{
		return Stack.PillId == PillId && Stack.Quality == Quality;
	});
	if (!Existing)
	{
		FImmortalPillStack& NewStack = Inventory.AddDefaulted_GetRef();
		NewStack.PillId = PillId;
		NewStack.Quality = Quality;
		NewStack.Quantity = FMath::Min(Amount, 9999);
		return NewStack.Quantity;
	}
	const int32 Previous = FMath::Clamp(Existing->Quantity, 0, 9999);
	Existing->Quantity = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(Previous) + Amount, 9999));
	return Existing->Quantity - Previous;
}

bool UImmortalAlchemyLibrary::RemovePill(
	TArray<FImmortalPillStack>& Inventory,
	const FName PillId,
	const EImmortalPillQuality Quality,
	const int32 Amount)
{
	if (Amount <= 0 || GetPillQuantity(Inventory, PillId, Quality) < Amount) return false;
	int32 Remaining = Amount;
	for (int32 Index = Inventory.Num() - 1; Index >= 0 && Remaining > 0; --Index)
	{
		FImmortalPillStack& Stack = Inventory[Index];
		if (Stack.PillId != PillId || Stack.Quality != Quality || Stack.Quantity <= 0) continue;
		const int32 Removed = FMath::Min(Stack.Quantity, Remaining);
		Stack.Quantity -= Removed;
		Remaining -= Removed;
		if (Stack.Quantity <= 0) Inventory.RemoveAt(Index);
	}
	return Remaining == 0;
}

void UImmortalAlchemyLibrary::NormalizePillInventory(TArray<FImmortalPillStack>& Inventory)
{
	TArray<FImmortalPillStack> Normalized;
	for (const FImmortalPillStack& Stack : Inventory)
	{
		if (Stack.Quantity > 0) AddPillStack(Normalized, Stack.PillId, Stack.Quality, Stack.Quantity);
	}
	Normalized.Sort([](const FImmortalPillStack& Left, const FImmortalPillStack& Right)
	{
		if (Left.PillId != Right.PillId) return Left.PillId.LexicalLess(Right.PillId);
		return static_cast<uint8>(Left.Quality) < static_cast<uint8>(Right.Quality);
	});
	Inventory = MoveTemp(Normalized);
}

FText UImmortalAlchemyLibrary::GetQualityText(const EImmortalPillQuality Quality)
{
	return Quality == EImmortalPillQuality::Exceptional
		? FText::FromString(TEXT("极品"))
		: FText::FromString(TEXT("普通"));
}

FLinearColor UImmortalAlchemyLibrary::GetQualityColor(const EImmortalPillQuality Quality)
{
	return Quality == EImmortalPillQuality::Exceptional
		? FLinearColor(1.0f, 0.58f, 0.12f, 1.0f)
		: FLinearColor(0.36f, 0.9f, 0.72f, 1.0f);
}

FText UImmortalAlchemyLibrary::GetEffectText(
	const FImmortalPillDefinition& Definition,
	const EImmortalPillQuality Quality)
{
	const bool bExceptional = Quality == EImmortalPillQuality::Exceptional;
	const float Magnitude = bExceptional ? Definition.ExceptionalMagnitude : Definition.OrdinaryMagnitude;
	const float Duration = bExceptional ? Definition.ExceptionalDurationSeconds : Definition.OrdinaryDurationSeconds;
	switch (Definition.Effect)
	{
	case EImmortalPillEffect::RestoreHealth:
		return FText::FromString(FString::Printf(TEXT("恢复最大生命的 %.0f%%"), Magnitude * 100.0f));
	case EImmortalPillEffect::GrantCultivationPercent:
		return FText::FromString(FString::Printf(TEXT("获得当前突破需求 %.0f%% 的修为"), Magnitude * 100.0f));
	case EImmortalPillEffect::CultivationRateBoost:
		return FText::FromString(FString::Printf(TEXT("在线修炼速度 ×%.1f，持续 %.0f 秒"), Magnitude, Duration));
	case EImmortalPillEffect::CompleteCurrentStage:
		return bExceptional
			? FText::FromString(FString::Printf(TEXT("立即突破，并获得下一层需求 %.0f%% 的修为"), Magnitude * 100.0f))
			: FText::FromString(TEXT("补足当前修为并立即突破"));
	default:
		return FText::GetEmpty();
	}
}
