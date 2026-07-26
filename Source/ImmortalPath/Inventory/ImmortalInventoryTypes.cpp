// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalInventoryTypes.h"

#include "../Shop/ImmortalShopTypes.h"
#include "Engine/DataTable.h"
#include "Misc/PackageName.h"

namespace
{
	FImmortalQuestItemDefinition QuestItemDefinition(
		const TCHAR* Name,
		const TCHAR* Description,
		const TCHAR* Glyph,
		const FLinearColor Color,
		const int32 MaximumStack = 999)
	{
		FImmortalQuestItemDefinition Result;
		Result.DisplayName = FText::FromString(Name);
		Result.Description = FText::FromString(Description);
		Result.IconGlyph = FText::FromString(Glyph);
		Result.DisplayColor = Color;
		Result.MaximumStack = FMath::Max(MaximumStack, 1);
		return Result;
	}

	const TMap<FName, FImmortalQuestItemDefinition>& GetFallbackQuestItemCatalog()
	{
		static const TMap<FName, FImmortalQuestItemDefinition> Catalog =
		{
			{TEXT("QingyunTrialJadeSlip"), QuestItemDefinition(
				TEXT("青云试炼玉简"), TEXT("记录青云山历练进度的任务凭证，不可出售或分解。"), TEXT("简"),
				FLinearColor(0.38f, 0.88f, 1.0f, 1.0f), 99)},
			{TEXT("AncientMapFragment"), QuestItemDefinition(
				TEXT("残缺秘境图"), TEXT("主线任务所需的秘境地图残片，不占装备背包容量。"), TEXT("图"),
				FLinearColor(0.86f, 0.72f, 0.42f, 1.0f), 99)},
			{TEXT("DemonKingSeal"), QuestItemDefinition(
				TEXT("妖王印记"), TEXT("击败特定妖王后获得的任务证明，只能交付给任务发布者。"), TEXT("印"),
				FLinearColor(0.96f, 0.34f, 0.28f, 1.0f), 999)}
		};
		return Catalog;
	}

	UDataTable* GetOptionalQuestItemTable()
	{
		static TWeakObjectPtr<UDataTable> CachedTable;
		static bool bAttemptedLoad = false;
		if (!bAttemptedLoad)
		{
			bAttemptedLoad = true;
			const FString PackageName(TEXT("/Game/GAME/Data/DT_QuestItems"));
			if (FPackageName::DoesPackageExist(PackageName))
			{
				CachedTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/GAME/Data/DT_QuestItems.DT_QuestItems"));
			}
		}
		return CachedTable.Get();
	}

	int32 QualityRank(const EImmortalEquipmentQuality Quality)
	{
		return FMath::Clamp(static_cast<int32>(Quality),
			static_cast<int32>(EImmortalEquipmentQuality::Common),
			static_cast<int32>(EImmortalEquipmentQuality::Divine));
	}

	int32 SlotDisplayOrder(const EImmortalEquipmentSlot Slot)
	{
		switch (Slot)
		{
		case EImmortalEquipmentSlot::Weapon: return 0;
		case EImmortalEquipmentSlot::Head: return 1;
		case EImmortalEquipmentSlot::Chest: return 2;
		case EImmortalEquipmentSlot::Bracers: return 3;
		case EImmortalEquipmentSlot::Belt: return 4;
		case EImmortalEquipmentSlot::Boots: return 5;
		case EImmortalEquipmentSlot::RingLeft: return 6;
		case EImmortalEquipmentSlot::RingRight: return 7;
		case EImmortalEquipmentSlot::Accessory: return 8;
		default: return 9;
		}
	}

	FString GuidKey(const FGuid& Guid)
	{
		return Guid.ToString(EGuidFormats::Digits);
	}

	TArray<FGuid> EquipmentOrder(const TArray<FImmortalEquipmentItem>& Inventory)
	{
		TArray<FGuid> Result;
		Result.Reserve(Inventory.Num());
		for (const FImmortalEquipmentItem& Item : Inventory) Result.Add(Item.ItemId);
		return Result;
	}

	TArray<FGuid> ArtifactOrder(const TArray<FImmortalArtifactItem>& Inventory)
	{
		TArray<FGuid> Result;
		Result.Reserve(Inventory.Num());
		for (const FImmortalArtifactItem& Item : Inventory) Result.Add(Item.InstanceId);
		return Result;
	}

	void AddYield(TArray<FImmortalMaterialStack>& Yield, const FName MaterialId, const int32 Amount)
	{
		if (Amount > 0) UImmortalMaterialLibrary::AddMaterialStack(Yield, MaterialId, Amount);
	}
}

TArray<FName> UImmortalInventoryLibrary::GetKnownQuestItemIds()
{
	TArray<FName> Result;
	GetFallbackQuestItemCatalog().GetKeys(Result);
	if (const UDataTable* Table = GetOptionalQuestItemTable())
	{
		for (const FName RowName : Table->GetRowNames()) Result.AddUnique(RowName);
	}
	Result.Sort(FNameLexicalLess());
	return Result;
}

bool UImmortalInventoryLibrary::GetQuestItemDefinition(
	const FName QuestItemId,
	FImmortalQuestItemDefinition& OutDefinition)
{
	if (QuestItemId.IsNone()) return false;
	if (const UDataTable* Table = GetOptionalQuestItemTable())
	{
		if (const FImmortalQuestItemDefinition* Row = Table->FindRow<FImmortalQuestItemDefinition>(
			QuestItemId, TEXT("Quest item lookup"), false))
		{
			OutDefinition = *Row;
			return true;
		}
	}
	if (const FImmortalQuestItemDefinition* Found = GetFallbackQuestItemCatalog().Find(QuestItemId))
	{
		OutDefinition = *Found;
		return true;
	}
	return false;
}

int32 UImmortalInventoryLibrary::GetQuestItemQuantity(
	const TArray<FImmortalQuestItemStack>& Inventory,
	const FName QuestItemId)
{
	int64 Total = 0;
	for (const FImmortalQuestItemStack& Stack : Inventory)
	{
		if (Stack.QuestItemId == QuestItemId && Stack.Quantity > 0)
		{
			Total = FMath::Min<int64>(Total + Stack.Quantity, MAX_int32);
		}
	}
	return static_cast<int32>(Total);
}

int32 UImmortalInventoryLibrary::AddQuestItemStack(
	TArray<FImmortalQuestItemStack>& Inventory,
	const FName QuestItemId,
	const int32 Amount)
{
	FImmortalQuestItemDefinition Definition;
	if (Amount <= 0 || !GetQuestItemDefinition(QuestItemId, Definition)) return 0;
	FImmortalQuestItemStack* Existing = Inventory.FindByPredicate([QuestItemId](const FImmortalQuestItemStack& Stack)
	{
		return Stack.QuestItemId == QuestItemId;
	});
	const int32 Maximum = FMath::Max(Definition.MaximumStack, 1);
	if (!Existing)
	{
		FImmortalQuestItemStack& Added = Inventory.AddDefaulted_GetRef();
		Added.QuestItemId = QuestItemId;
		Added.Quantity = FMath::Min(Amount, Maximum);
		return Added.Quantity;
	}
	const int32 Previous = FMath::Clamp(Existing->Quantity, 0, Maximum);
	Existing->Quantity = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(Previous) + Amount, Maximum));
	return Existing->Quantity - Previous;
}

bool UImmortalInventoryLibrary::RemoveQuestItemStack(
	TArray<FImmortalQuestItemStack>& Inventory,
	const FName QuestItemId,
	const int32 Amount)
{
	if (Amount <= 0 || GetQuestItemQuantity(Inventory, QuestItemId) < Amount) return false;
	int32 Remaining = Amount;
	for (int32 Index = Inventory.Num() - 1; Index >= 0 && Remaining > 0; --Index)
	{
		FImmortalQuestItemStack& Stack = Inventory[Index];
		if (Stack.QuestItemId != QuestItemId || Stack.Quantity <= 0) continue;
		const int32 Removed = FMath::Min(Stack.Quantity, Remaining);
		Stack.Quantity -= Removed;
		Remaining -= Removed;
		if (Stack.Quantity <= 0) Inventory.RemoveAt(Index);
	}
	NormalizeQuestItemInventory(Inventory);
	return Remaining == 0;
}

void UImmortalInventoryLibrary::NormalizeQuestItemInventory(TArray<FImmortalQuestItemStack>& Inventory)
{
	TArray<FImmortalQuestItemStack> Normalized;
	for (const FImmortalQuestItemStack& Stack : Inventory)
	{
		if (Stack.Quantity > 0) AddQuestItemStack(Normalized, Stack.QuestItemId, Stack.Quantity);
	}
	Normalized.Sort([](const FImmortalQuestItemStack& Left, const FImmortalQuestItemStack& Right)
	{
		FImmortalQuestItemDefinition LeftDefinition;
		FImmortalQuestItemDefinition RightDefinition;
		GetQuestItemDefinition(Left.QuestItemId, LeftDefinition);
		GetQuestItemDefinition(Right.QuestItemId, RightDefinition);
		const FString LeftName = LeftDefinition.DisplayName.ToString();
		const FString RightName = RightDefinition.DisplayName.ToString();
		return LeftName == RightName
			? Left.QuestItemId.LexicalLess(Right.QuestItemId)
			: LeftName < RightName;
	});
	Inventory = MoveTemp(Normalized);
}

bool UImmortalInventoryLibrary::NormalizeEquipmentCollections(
	TArray<FImmortalEquipmentItem>& Inventory,
	TArray<FImmortalEquipmentItem>& Equipped)
{
	bool bChanged = false;
	auto NormalizeItem = [&bChanged](FImmortalEquipmentItem& Item)
	{
		const FImmortalEquipmentItem Before = Item;
		if (static_cast<int32>(Item.Slot) < 0
			|| static_cast<int32>(Item.Slot) >= static_cast<int32>(EImmortalEquipmentSlot::MAX))
		{
			Item.Slot = EImmortalEquipmentSlot::Weapon;
			bChanged = true;
		}
		if (static_cast<int32>(Item.Quality) < static_cast<int32>(EImmortalEquipmentQuality::Common)
			|| static_cast<int32>(Item.Quality) > static_cast<int32>(EImmortalEquipmentQuality::Divine))
		{
			Item.Quality = EImmortalEquipmentQuality::Common;
			bChanged = true;
		}
		if (Item.ItemLevel < 1 || Item.EnhancementLevel < 0 || Item.EnhancementLevel > 15
			|| Item.RefinementCount < 0
			|| static_cast<int32>(Item.Discipline) < static_cast<int32>(EImmortalEquipmentDiscipline::Universal)
			|| static_cast<int32>(Item.Discipline) >= static_cast<int32>(EImmortalEquipmentDiscipline::MAX))
		{
			bChanged = true;
		}
		UImmortalEquipmentLibrary::NormalizeForgingState(Item);
		if (Before.SetId != Item.SetId || Before.Affixes.Num() != Item.Affixes.Num()
			|| !FMath::IsNearlyEqual(Before.AttackBonus, Item.AttackBonus)
			|| !FMath::IsNearlyEqual(Before.DefenseBonus, Item.DefenseBonus)
			|| !FMath::IsNearlyEqual(Before.HealthBonus, Item.HealthBonus)
			|| !FMath::IsNearlyEqual(Before.AttackSpeedBonus, Item.AttackSpeedBonus)
			|| !FMath::IsNearlyEqual(Before.CriticalChanceBonus, Item.CriticalChanceBonus)
			|| !FMath::IsNearlyEqual(Before.CriticalDamageBonus, Item.CriticalDamageBonus)
			|| !FMath::IsNearlyEqual(Before.FireDamageBonus, Item.FireDamageBonus)
			|| !FMath::IsNearlyEqual(Before.ThunderDamageBonus, Item.ThunderDamageBonus)
			|| !FMath::IsNearlyEqual(Before.IceDamageBonus, Item.IceDamageBonus)
			|| !FMath::IsNearlyEqual(Before.LifeStealBonus, Item.LifeStealBonus)
			|| !FMath::IsNearlyEqual(Before.CultivationGainBonus, Item.CultivationGainBonus)
			|| !FMath::IsNearlyEqual(Before.LootFindBonus, Item.LootFindBonus)
			|| !FMath::IsNearlyEqual(Before.BossDamageBonus, Item.BossDamageBonus))
		{
			bChanged = true;
		}
	};

	TArray<FImmortalEquipmentItem> NormalizedEquipped;
	TArray<FImmortalEquipmentItem> InventoryCandidates = Inventory;
	TMap<FGuid, int32> EquippedById;
	TSet<EImmortalEquipmentSlot> EquippedSlots;
	for (FImmortalEquipmentItem Item : Equipped)
	{
		if (!Item.IsValid())
		{
			bChanged = true;
			continue;
		}
		NormalizeItem(Item);
		if (const int32* ExistingIndex = EquippedById.Find(Item.ItemId))
		{
			NormalizedEquipped[*ExistingIndex].bLocked |= Item.bLocked;
			bChanged = true;
			continue;
		}
		if (EquippedSlots.Contains(Item.Slot))
		{
			InventoryCandidates.Add(Item);
			bChanged = true;
			continue;
		}
		EquippedById.Add(Item.ItemId, NormalizedEquipped.Num());
		EquippedSlots.Add(Item.Slot);
		NormalizedEquipped.Add(Item);
	}

	TArray<FImmortalEquipmentItem> NormalizedInventory;
	TMap<FGuid, int32> InventoryById;
	for (FImmortalEquipmentItem Item : InventoryCandidates)
	{
		if (!Item.IsValid())
		{
			bChanged = true;
			continue;
		}
		NormalizeItem(Item);
		if (const int32* EquippedIndex = EquippedById.Find(Item.ItemId))
		{
			NormalizedEquipped[*EquippedIndex].bLocked |= Item.bLocked;
			bChanged = true;
			continue;
		}
		if (const int32* ExistingIndex = InventoryById.Find(Item.ItemId))
		{
			NormalizedInventory[*ExistingIndex].bLocked |= Item.bLocked;
			bChanged = true;
			continue;
		}
		InventoryById.Add(Item.ItemId, NormalizedInventory.Num());
		NormalizedInventory.Add(Item);
	}
	if (NormalizedEquipped.Num() != Equipped.Num() || NormalizedInventory.Num() != Inventory.Num()) bChanged = true;
	Equipped = MoveTemp(NormalizedEquipped);
	Inventory = MoveTemp(NormalizedInventory);
	return bChanged;
}

bool UImmortalInventoryLibrary::SortEquipmentInventory(TArray<FImmortalEquipmentItem>& Inventory)
{
	const TArray<FGuid> Before = EquipmentOrder(Inventory);
	Inventory.Sort([](const FImmortalEquipmentItem& Left, const FImmortalEquipmentItem& Right)
	{
		if (Left.bLocked != Right.bLocked) return Left.bLocked;
		if (Left.Slot != Right.Slot) return SlotDisplayOrder(Left.Slot) < SlotDisplayOrder(Right.Slot);
		if (Left.Quality != Right.Quality) return QualityRank(Left.Quality) > QualityRank(Right.Quality);
		if (Left.ItemLevel != Right.ItemLevel) return Left.ItemLevel > Right.ItemLevel;
		const float RawLeftPower = UImmortalEquipmentLibrary::CalculateEquipmentPower(Left);
		const float RawRightPower = UImmortalEquipmentLibrary::CalculateEquipmentPower(Right);
		const float LeftPower = FMath::IsFinite(RawLeftPower) ? RawLeftPower : -MAX_flt;
		const float RightPower = FMath::IsFinite(RawRightPower) ? RawRightPower : -MAX_flt;
		if (LeftPower != RightPower) return LeftPower > RightPower;
		if (Left.DisplayName != Right.DisplayName) return Left.DisplayName.LexicalLess(Right.DisplayName);
		return GuidKey(Left.ItemId) < GuidKey(Right.ItemId);
	});
	return Before != EquipmentOrder(Inventory);
}

bool UImmortalInventoryLibrary::SortArtifactInventory(
	TArray<FImmortalArtifactItem>& Inventory,
	const FGuid EquippedInstanceId)
{
	const TArray<FGuid> Before = ArtifactOrder(Inventory);
	Inventory.Sort([EquippedInstanceId](const FImmortalArtifactItem& Left, const FImmortalArtifactItem& Right)
	{
		const bool bLeftEquipped = Left.InstanceId == EquippedInstanceId;
		const bool bRightEquipped = Right.InstanceId == EquippedInstanceId;
		if (bLeftEquipped != bRightEquipped) return bLeftEquipped;
		if (Left.bLocked != Right.bLocked) return Left.bLocked;
		FImmortalArtifactDefinition LeftDefinition;
		FImmortalArtifactDefinition RightDefinition;
		UImmortalArtifactLibrary::GetArtifactDefinition(Left.ArtifactId, LeftDefinition);
		UImmortalArtifactLibrary::GetArtifactDefinition(Right.ArtifactId, RightDefinition);
		if (LeftDefinition.Quality != RightDefinition.Quality)
		{
			return static_cast<uint8>(LeftDefinition.Quality) > static_cast<uint8>(RightDefinition.Quality);
		}
		if (Left.Stars != Right.Stars) return Left.Stars > Right.Stars;
		if (Left.Level != Right.Level) return Left.Level > Right.Level;
		const FString LeftName = LeftDefinition.DisplayName.ToString();
		const FString RightName = RightDefinition.DisplayName.ToString();
		if (LeftName != RightName)
		{
			return LeftName < RightName;
		}
		return GuidKey(Left.InstanceId) < GuidKey(Right.InstanceId);
	});
	return Before != ArtifactOrder(Inventory);
}

bool UImmortalInventoryLibrary::IsEligibleForBulkAction(
	const FImmortalEquipmentItem& Item,
	const EImmortalEquipmentQuality MaximumQuality)
{
	return Item.IsValid() && !Item.bLocked && QualityRank(Item.Quality) <= QualityRank(MaximumQuality);
}

int32 UImmortalInventoryLibrary::GetBulkEligibleEquipmentCount(
	const TArray<FImmortalEquipmentItem>& Inventory,
	const EImmortalEquipmentQuality MaximumQuality)
{
	int32 Result = 0;
	for (const FImmortalEquipmentItem& Item : Inventory)
	{
		Result += IsEligibleForBulkAction(Item, MaximumQuality) ? 1 : 0;
	}
	return Result;
}

int32 UImmortalInventoryLibrary::GetBulkEquipmentSellValue(
	const TArray<FImmortalEquipmentItem>& Inventory,
	const EImmortalEquipmentQuality MaximumQuality)
{
	int64 Total = 0;
	for (const FImmortalEquipmentItem& Item : Inventory)
	{
		if (IsEligibleForBulkAction(Item, MaximumQuality))
		{
			Total = FMath::Min<int64>(Total + UImmortalShopLibrary::GetEquipmentSellPrice(Item), MAX_int32);
		}
	}
	return static_cast<int32>(Total);
}

TArray<FImmortalMaterialStack> UImmortalInventoryLibrary::GetEquipmentDismantleYield(
	const FImmortalEquipmentItem& Item)
{
	TArray<FImmortalMaterialStack> Result;
	if (!Item.IsValid()) return Result;
	const int32 Rank = QualityRank(Item.Quality);
	const int32 LevelTier = FMath::Max((FMath::Max(Item.ItemLevel, 1) - 1) / 10, 0);
	const int32 Enhancement = FMath::Clamp(Item.EnhancementLevel, 0, 15);
	AddYield(Result, TEXT("Ore"), 1 + LevelTier + Enhancement / 3);
	AddYield(Result, TEXT("SpiritIron"), Rank > 0 ? Rank + Enhancement / 5 : 0);
	AddYield(Result, TEXT("ArtifactFragment"), Rank >= 3 ? Rank - 2 : 0);
	UImmortalMaterialLibrary::NormalizeInventory(Result);
	return Result;
}

TArray<FImmortalMaterialStack> UImmortalInventoryLibrary::GetBulkEquipmentDismantleYield(
	const TArray<FImmortalEquipmentItem>& Inventory,
	const EImmortalEquipmentQuality MaximumQuality)
{
	TArray<FImmortalMaterialStack> Result;
	for (const FImmortalEquipmentItem& Item : Inventory)
	{
		if (!IsEligibleForBulkAction(Item, MaximumQuality)) continue;
		for (const FImmortalMaterialStack& Stack : GetEquipmentDismantleYield(Item))
		{
			AddYield(Result, Stack.MaterialId, Stack.Quantity);
		}
	}
	UImmortalMaterialLibrary::NormalizeInventory(Result);
	return Result;
}

bool UImmortalInventoryLibrary::TryAddMaterialRewards(
	TArray<FImmortalMaterialStack>& Inventory,
	const TArray<FImmortalMaterialStack>& Rewards)
{
	TArray<FImmortalMaterialStack> Candidate = Inventory;
	for (const FImmortalMaterialStack& Stack : Rewards)
	{
		if (!Stack.IsValid()
			|| UImmortalMaterialLibrary::AddMaterialStack(Candidate, Stack.MaterialId, Stack.Quantity) != Stack.Quantity)
		{
			return false;
		}
	}
	Inventory = MoveTemp(Candidate);
	return true;
}

bool UImmortalInventoryLibrary::CanReceiveSpiritStones(
	const int32 CurrentSpiritStones,
	const int64 Amount)
{
	return CurrentSpiritStones >= 0 && Amount > 0 && Amount <= MAX_int32
		&& static_cast<int64>(CurrentSpiritStones) + Amount <= MAX_int32;
}

FText UImmortalInventoryLibrary::GetCategoryText(const EImmortalInventoryCategory Category)
{
	switch (Category)
	{
	case EImmortalInventoryCategory::Equipment: return FText::FromString(TEXT("装备"));
	case EImmortalInventoryCategory::Material: return FText::FromString(TEXT("材料"));
	case EImmortalInventoryCategory::Pill: return FText::FromString(TEXT("丹药"));
	case EImmortalInventoryCategory::Artifact: return FText::FromString(TEXT("法宝"));
	case EImmortalInventoryCategory::QuestItem: return FText::FromString(TEXT("任务物品"));
	default: return FText::FromString(TEXT("背包"));
	}
}

FText UImmortalInventoryLibrary::GetMaximumQualityText(const EImmortalEquipmentQuality MaximumQuality)
{
	return FText::FromString(FString::Printf(TEXT("≤%s"),
		*UImmortalEquipmentLibrary::GetQualityText(MaximumQuality).ToString()));
}
