// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalPetTypes.h"

#include "Engine/DataTable.h"
#include "Misc/PackageName.h"
#include "PaperFlipbook.h"

#include <initializer_list>

namespace
{
	const FName SpiritFoxId(TEXT("SpiritFox"));
	const FName SpiritHoundId(TEXT("SpiritHound"));
	const FString PetTablePackageName(TEXT("/Game/GAME/Data/DT_Pets"));
	const FString PetTableObjectPath(TEXT("/Game/GAME/Data/DT_Pets.DT_Pets"));

	FImmortalCraftingMaterialCost PetMaterial(
		const TCHAR* MaterialId,
		const int32 Quantity)
	{
		FImmortalCraftingMaterialCost Result;
		Result.MaterialId = FName(MaterialId);
		Result.Quantity = Quantity;
		return Result;
	}

	FImmortalCraftingCost PetCost(
		const int32 SpiritStones,
		std::initializer_list<FImmortalCraftingMaterialCost> Materials)
	{
		FImmortalCraftingCost Result;
		Result.SpiritStones = SpiritStones;
		for (const FImmortalCraftingMaterialCost& Material : Materials)
		{
			Result.Materials.Add(Material);
		}
		return Result;
	}

	FImmortalPetDefinition MakePetDefinition(
		const TCHAR* DisplayName,
		const TCHAR* Description,
		const TCHAR* Glyph,
		const EImmortalPetAttackStyle AttackStyle,
		const bool bInitiallyOwned,
		const FImmortalCraftingCost& UnlockCost,
		const float BaseDamageRatio,
		const float DamagePerLevel,
		const float DamagePerStar,
		const float AttackInterval,
		const float AttackRange,
		const float SearchRange,
		const float CriticalChance,
		const float MovementSpeed,
		const float FollowOffsetX,
		const float VisualScale,
		const FLinearColor& DisplayColor,
		const TCHAR* MoveFlipbookPath,
		const TCHAR* AttackFlipbookPath,
		const TCHAR* HurtFlipbookPath,
		const TCHAR* DeathFlipbookPath)
	{
		FImmortalPetDefinition Result;
		Result.DisplayName = FText::FromString(DisplayName);
		Result.Description = FText::FromString(Description);
		Result.IconGlyph = FText::FromString(Glyph);
		Result.AttackStyle = AttackStyle;
		Result.bInitiallyOwned = bInitiallyOwned;
		Result.UnlockCost = UnlockCost;
		Result.BaseDamageRatio = BaseDamageRatio;
		Result.DamageRatioPerLevel = DamagePerLevel;
		Result.DamageRatioPerStar = DamagePerStar;
		Result.AttackInterval = AttackInterval;
		Result.AttackRange = AttackRange;
		Result.SearchRange = SearchRange;
		Result.CriticalChance = CriticalChance;
		Result.MovementSpeed = MovementSpeed;
		Result.FollowOffsetX = FollowOffsetX;
		Result.VisualScale = VisualScale;
		Result.DisplayColor = DisplayColor;
		Result.MoveFlipbook = TSoftObjectPtr<UPaperFlipbook>(
			FSoftObjectPath(MoveFlipbookPath));
		Result.AttackFlipbook = TSoftObjectPtr<UPaperFlipbook>(
			FSoftObjectPath(AttackFlipbookPath));
		Result.HurtFlipbook = TSoftObjectPtr<UPaperFlipbook>(
			FSoftObjectPath(HurtFlipbookPath));
		Result.DeathFlipbook = TSoftObjectPtr<UPaperFlipbook>(
			FSoftObjectPath(DeathFlipbookPath));
		return Result;
	}

	const TMap<FName, FImmortalPetDefinition>& GetFallbackPetCatalog()
	{
		static const TMap<FName, FImmortalPetDefinition> Catalog =
		{
			{
				SpiritFoxId,
				MakePetDefinition(
					TEXT("青丘灵狐"),
					TEXT("擅长远程灵焰，攻击频率快、暴击率高。新角色默认拥有并出战。"),
					TEXT("狐"),
					EImmortalPetAttackStyle::Ranged,
					true,
					FImmortalCraftingCost(),
					0.18f,
					0.006f,
					0.045f,
					1.15f,
					560.0f,
					980.0f,
					0.16f,
					560.0f,
					150.0f,
					0.55f,
					FLinearColor(0.96f, 0.56f, 0.80f, 1.0f),
					TEXT("/Game/GAME/Asset/Player/fox_move/fox_moveleft.fox_moveleft"),
					TEXT("/Game/GAME/Asset/Player/fox_attcak/foxattack.foxattack"),
					TEXT("/Game/GAME/Asset/Player/fox_hurt/foxhurt.foxhurt"),
					TEXT("/Game/GAME/Asset/Player/fox_death/death_fox.death_fox"))
			},
			{
				SpiritHoundId,
				MakePetDefinition(
					TEXT("镇岳灵犬"),
					TEXT("追近妖兽后发动重击，单次伤害更高，升星后的首领输出更强。"),
					TEXT("犬"),
					EImmortalPetAttackStyle::Melee,
					false,
					PetCost(400, {PetMaterial(TEXT("DemonCore"), 12)}),
					0.26f,
					0.008f,
					0.055f,
					1.45f,
					205.0f,
					900.0f,
					0.08f,
					620.0f,
					135.0f,
					0.55f,
					FLinearColor(0.86f, 0.70f, 0.42f, 1.0f),
					TEXT("/Game/GAME/Asset/Player/dog_move_left/dog_moveleft.dog_moveleft"),
					TEXT("/Game/GAME/Asset/Player/dog_attcak/dogattack.dogattack"),
					TEXT("/Game/GAME/Asset/Player/doghurt/doghurt.doghurt"),
					TEXT("/Game/GAME/Asset/Player/dog_death/death_dog.death_dog"))
			}
		};
		return Catalog;
	}

	UDataTable* GetOptionalPetTable()
	{
		static TWeakObjectPtr<UDataTable> CachedTable;
		static bool bAttemptedLoad = false;
		if (!bAttemptedLoad)
		{
			bAttemptedLoad = true;
			if (FPackageName::DoesPackageExist(PetTablePackageName))
			{
				CachedTable = LoadObject<UDataTable>(nullptr, *PetTableObjectPath);
			}
		}
		return CachedTable.Get();
	}

	void AdvancePetRevision(FImmortalPetState& State)
	{
		if (State.Revision < MAX_int32)
		{
			++State.Revision;
		}
	}

	int64 SaturatingAddNonNegative(
		const int64 Value,
		const int64 Amount)
	{
		const int64 SafeValue = FMath::Max<int64>(Value, 0);
		const int64 SafeAmount = FMath::Max<int64>(Amount, 0);
		return SafeValue > MAX_int64 - SafeAmount
			? MAX_int64
			: SafeValue + SafeAmount;
	}

	bool ArePetProgressValuesEqual(
		const FImmortalPetProgress& Left,
		const FImmortalPetProgress& Right)
	{
		return Left.PetId == Right.PetId
			&& Left.bOwned == Right.bOwned
			&& Left.Level == Right.Level
			&& Left.Experience == Right.Experience
			&& Left.Stars == Right.Stars
			&& Left.TotalCombatKills == Right.TotalCombatKills;
	}
}

bool FImmortalPetDefinition::IsValid() const
{
	if (UnlockCost.SpiritStones < 0)
	{
		return false;
	}
	TSet<FName> CostMaterialIds;
	for (const FImmortalCraftingMaterialCost& Material :
		UnlockCost.Materials)
	{
		FImmortalMaterialDefinition MaterialDefinition;
		if (Material.MaterialId.IsNone()
			|| Material.Quantity <= 0
			|| CostMaterialIds.Contains(Material.MaterialId)
			|| !UImmortalMaterialLibrary::GetMaterialDefinition(
				Material.MaterialId, MaterialDefinition))
		{
			return false;
		}
		CostMaterialIds.Add(Material.MaterialId);
	}
	return !DisplayName.IsEmpty()
		&& !IconGlyph.IsEmpty()
		&& FMath::IsFinite(BaseDamageRatio)
		&& BaseDamageRatio > 0.0f
		&& FMath::IsFinite(DamageRatioPerLevel)
		&& DamageRatioPerLevel >= 0.0f
		&& FMath::IsFinite(DamageRatioPerStar)
		&& DamageRatioPerStar >= 0.0f
		&& FMath::IsFinite(AttackInterval)
		&& AttackInterval >= 0.1f
		&& FMath::IsFinite(AttackRange)
		&& AttackRange >= 1.0f
		&& FMath::IsFinite(SearchRange)
		&& SearchRange >= AttackRange
		&& FMath::IsFinite(CriticalChance)
		&& CriticalChance >= 0.0f
		&& CriticalChance <= 1.0f
		&& FMath::IsFinite(MovementSpeed)
		&& MovementSpeed >= 1.0f
		&& FMath::IsFinite(FollowOffsetX)
		&& FMath::IsFinite(MaximumLeashDistance)
		&& MaximumLeashDistance >= AttackRange
		&& FMath::IsFinite(VisualScale)
		&& VisualScale > 0.0f;
}

TArray<FName> UImmortalPetLibrary::GetKnownPetIds()
{
	TArray<FName> Result;
	GetFallbackPetCatalog().GetKeys(Result);
	if (const UDataTable* Table = GetOptionalPetTable())
	{
		for (const FName RowName : Table->GetRowNames())
		{
			const FImmortalPetDefinition* Row =
				Table->FindRow<FImmortalPetDefinition>(
					RowName, TEXT("Pet catalog enumeration"), false);
			if (Row && Row->IsValid())
			{
				Result.AddUnique(RowName);
			}
		}
	}
	Result.Sort(FNameLexicalLess());
	return Result;
}

bool UImmortalPetLibrary::GetPetDefinition(
	const FName PetId,
	FImmortalPetDefinition& OutDefinition)
{
	if (PetId.IsNone())
	{
		return false;
	}
	if (const UDataTable* Table = GetOptionalPetTable())
	{
		if (const FImmortalPetDefinition* Row =
			Table->FindRow<FImmortalPetDefinition>(
				PetId, TEXT("Pet definition lookup"), false))
		{
			if (Row->IsValid())
			{
				OutDefinition = *Row;
				return true;
			}
			UE_LOG(LogTemp, Warning,
				TEXT("DT_Pets row %s is invalid; using native fallback when available"),
				*PetId.ToString());
		}
	}
	if (const FImmortalPetDefinition* Found =
		GetFallbackPetCatalog().Find(PetId))
	{
		OutDefinition = *Found;
		return true;
	}
	return false;
}

FImmortalPetState UImmortalPetLibrary::CreateDefaultState()
{
	FImmortalPetState Result;
	Result.bInitialized = true;
	for (const FName PetId : GetKnownPetIds())
	{
		FImmortalPetDefinition Definition;
		if (!GetPetDefinition(PetId, Definition))
		{
			continue;
		}
		FImmortalPetProgress Progress;
		Progress.PetId = PetId;
		Progress.bOwned = Definition.bInitiallyOwned;
		Result.Pets.Add(Progress);
		if (Result.ActivePetId.IsNone() && Progress.bOwned)
		{
			Result.ActivePetId = PetId;
		}
	}
	Result.Revision = 1;
	return Result;
}

bool UImmortalPetLibrary::NormalizeState(FImmortalPetState& State)
{
	const bool bPreviousInitialized = State.bInitialized;
	const int32 PreviousRevision = State.Revision;
	const FName PreviousActivePetId = State.ActivePetId;
	const int64 PreviousTotalKills = State.TotalCombatKills;
	const TArray<FImmortalPetProgress> PreviousPets = State.Pets;

	State.bInitialized = true;
	State.Revision = FMath::Max(State.Revision, 0);
	State.TotalCombatKills = FMath::Max<int64>(State.TotalCombatKills, 0);

	TMap<FName, FImmortalPetProgress> UniqueProgress;
	for (FImmortalPetProgress Progress : State.Pets)
	{
		if (!Progress.IsValid())
		{
			continue;
		}
		Progress.Level = FMath::Clamp(
			Progress.Level, 1, MaximumPetLevel);
		Progress.Stars = FMath::Clamp(
			Progress.Stars, 0, MaximumPetStars);
		Progress.TotalCombatKills =
			FMath::Max<int64>(Progress.TotalCombatKills, 0);
		if (!Progress.bOwned
			&& (Progress.Level > 1
				|| Progress.Experience > 0
				|| Progress.Stars > 0
				|| Progress.TotalCombatKills > 0
				|| Progress.PetId == State.ActivePetId))
		{
			// A damaged ownership bit must not erase already earned or paid
			// growth. Unknown IDs remain dormant and are never selected below.
			Progress.bOwned = true;
		}
		if (Progress.Level >= MaximumPetLevel)
		{
			Progress.Experience = 0;
		}
		else
		{
			Progress.Experience = FMath::Clamp(
				Progress.Experience,
				0,
				FMath::Max(
					GetExperienceRequiredForLevel(Progress.Level) - 1,
					0));
		}
		if (FImmortalPetProgress* Existing =
			UniqueProgress.Find(Progress.PetId))
		{
			Existing->bOwned = Existing->bOwned || Progress.bOwned;
			if (Progress.Level > Existing->Level
				|| (Progress.Level == Existing->Level
					&& Progress.Experience > Existing->Experience))
			{
				Existing->Level = Progress.Level;
				Existing->Experience = Progress.Experience;
			}
			Existing->Stars = FMath::Max(
				Existing->Stars, Progress.Stars);
			Existing->TotalCombatKills = FMath::Max(
				Existing->TotalCombatKills,
				Progress.TotalCombatKills);
		}
		else
		{
			UniqueProgress.Add(Progress.PetId, Progress);
		}
	}

	for (const FName KnownPetId : GetKnownPetIds())
	{
		FImmortalPetDefinition Definition;
		if (!GetPetDefinition(KnownPetId, Definition))
		{
			continue;
		}
		FImmortalPetProgress& Progress =
			UniqueProgress.FindOrAdd(KnownPetId);
		Progress.PetId = KnownPetId;
		if (Definition.bInitiallyOwned)
		{
			Progress.bOwned = true;
		}
		if (Progress.Level <= 0)
		{
			Progress.Level = 1;
		}
	}

	State.Pets.Reset(UniqueProgress.Num());
	UniqueProgress.GenerateValueArray(State.Pets);
	State.Pets.Sort([](
		const FImmortalPetProgress& Left,
		const FImmortalPetProgress& Right)
	{
		return Left.PetId.LexicalLess(Right.PetId);
	});
	int64 AuditedTotalKills = 0;
	for (const FImmortalPetProgress& Progress : State.Pets)
	{
		AuditedTotalKills = SaturatingAddNonNegative(
			AuditedTotalKills,
			Progress.TotalCombatKills);
	}
	State.TotalCombatKills = FMath::Max(
		State.TotalCombatKills,
		AuditedTotalKills);

	const FImmortalPetProgress* Active =
		State.Pets.FindByPredicate(
			[&State](const FImmortalPetProgress& Progress)
			{
				FImmortalPetDefinition Definition;
				return Progress.PetId == State.ActivePetId
					&& Progress.bOwned
					&& UImmortalPetLibrary::GetPetDefinition(
						Progress.PetId, Definition);
			});
	if (!Active)
	{
		State.ActivePetId = NAME_None;
		if (const FImmortalPetProgress* FirstOwned =
			State.Pets.FindByPredicate(
				[](const FImmortalPetProgress& Progress)
				{
					FImmortalPetDefinition Definition;
					return Progress.bOwned
						&& UImmortalPetLibrary::GetPetDefinition(
							Progress.PetId, Definition);
				}))
		{
			State.ActivePetId = FirstOwned->PetId;
		}
	}

	bool bChanged = bPreviousInitialized != State.bInitialized
		|| PreviousRevision != State.Revision
		|| PreviousActivePetId != State.ActivePetId
		|| PreviousTotalKills != State.TotalCombatKills
		|| PreviousPets.Num() != State.Pets.Num();
	if (!bChanged)
	{
		for (int32 Index = 0; Index < State.Pets.Num(); ++Index)
		{
			if (!ArePetProgressValuesEqual(
				PreviousPets[Index], State.Pets[Index]))
			{
				bChanged = true;
				break;
			}
		}
	}
	if (bChanged)
	{
		AdvancePetRevision(State);
	}
	return bChanged;
}

bool UImmortalPetLibrary::GetPetProgress(
	const FImmortalPetState& State,
	const FName PetId,
	FImmortalPetProgress& OutProgress)
{
	if (const FImmortalPetProgress* Found =
		State.Pets.FindByPredicate(
			[PetId](const FImmortalPetProgress& Progress)
			{
				return Progress.PetId == PetId;
			}))
	{
		OutProgress = *Found;
		return true;
	}
	return false;
}

FImmortalPetProgress* UImmortalPetLibrary::FindMutablePetProgress(
	FImmortalPetState& State,
	const FName PetId)
{
	return State.Pets.FindByPredicate(
		[PetId](const FImmortalPetProgress& Progress)
		{
			return Progress.PetId == PetId;
		});
}

int32 UImmortalPetLibrary::GetExperienceRequiredForLevel(
	const int32 Level)
{
	if (Level < 1 || Level >= MaximumPetLevel)
	{
		return 0;
	}
	const int64 Index = static_cast<int64>(Level - 1);
	return static_cast<int32>(FMath::Min<int64>(
		100 + Index * 42 + Index * Index * 5,
		MAX_int32));
}

float UImmortalPetLibrary::CalculateDamageRatio(
	const FImmortalPetDefinition& Definition,
	const FImmortalPetProgress& Progress)
{
	if (!Definition.IsValid() || !Progress.bOwned)
	{
		return 0.0f;
	}
	const int32 SafeLevel = FMath::Clamp(
		Progress.Level, 1, MaximumPetLevel);
	const int32 SafeStars = FMath::Clamp(
		Progress.Stars, 0, MaximumPetStars);
	const float Result = FMath::Max(
		Definition.BaseDamageRatio
			+ Definition.DamageRatioPerLevel
				* static_cast<float>(SafeLevel - 1)
			+ Definition.DamageRatioPerStar
				* static_cast<float>(SafeStars),
		0.0f);
	return FMath::IsFinite(Result) ? Result : 0.0f;
}

float UImmortalPetLibrary::CalculateCombatPower(
	const FImmortalPetDefinition& Definition,
	const FImmortalPetProgress& Progress,
	const float OwnerAttack)
{
	const float SafeOwnerAttack =
		FMath::IsFinite(OwnerAttack)
			? FMath::Max(OwnerAttack, 0.0f)
			: 0.0f;
	const float DamagePerHit = SafeOwnerAttack
		* CalculateDamageRatio(Definition, Progress)
		* (1.0f + FMath::Clamp(
			Definition.CriticalChance, 0.0f, 1.0f) * 0.5f);
	const float Result = Definition.IsValid()
		? DamagePerHit / FMath::Max(Definition.AttackInterval, 0.1f)
		: 0.0f;
	return FMath::IsFinite(Result) ? Result : 0.0f;
}

FImmortalCraftingCost UImmortalPetLibrary::GetStarUpCost(
	const FImmortalPetProgress& Progress)
{
	FImmortalPetDefinition Definition;
	if (!Progress.IsValid() || !Progress.bOwned
		|| Progress.Stars < 0
		|| Progress.Stars >= MaximumPetStars
		|| !GetPetDefinition(Progress.PetId, Definition))
	{
		return FImmortalCraftingCost();
	}
	const int32 NextStar = Progress.Stars + 1;
	return PetCost(
		250 * NextStar,
		{PetMaterial(TEXT("DemonCore"), 3 * NextStar)});
}

bool UImmortalPetLibrary::UnlockPet(
	FImmortalPetState& State,
	const FName PetId)
{
	FImmortalPetDefinition Definition;
	FImmortalPetProgress* Progress =
		FindMutablePetProgress(State, PetId);
	if (!Progress || !GetPetDefinition(PetId, Definition)
		|| Progress->bOwned)
	{
		return false;
	}
	Progress->bOwned = true;
	Progress->Level = FMath::Clamp(
		Progress->Level, 1, MaximumPetLevel);
	if (State.ActivePetId.IsNone())
	{
		State.ActivePetId = PetId;
	}
	AdvancePetRevision(State);
	return true;
}

bool UImmortalPetLibrary::SetActivePet(
	FImmortalPetState& State,
	const FName PetId)
{
	FImmortalPetDefinition Definition;
	const FImmortalPetProgress* Progress =
		State.Pets.FindByPredicate(
			[PetId](const FImmortalPetProgress& Entry)
			{
				return Entry.PetId == PetId && Entry.bOwned;
			});
	if (!Progress || State.ActivePetId == PetId
		|| !GetPetDefinition(PetId, Definition))
	{
		return false;
	}
	State.ActivePetId = PetId;
	AdvancePetRevision(State);
	return true;
}

bool UImmortalPetLibrary::RaiseStar(
	FImmortalPetState& State,
	const FName PetId)
{
	FImmortalPetProgress* Progress =
		FindMutablePetProgress(State, PetId);
	FImmortalPetDefinition Definition;
	if (!Progress || !Progress->bOwned
		|| Progress->Stars < 0
		|| Progress->Stars >= MaximumPetStars
		|| !GetPetDefinition(PetId, Definition))
	{
		return false;
	}
	++Progress->Stars;
	AdvancePetRevision(State);
	return true;
}

FImmortalPetExperienceResult
UImmortalPetLibrary::GrantActivePetExperience(
	FImmortalPetState& State,
	const int32 Experience,
	const int32 CombatKills)
{
	FImmortalPetExperienceResult Result;
	Result.PetId = State.ActivePetId;
	FImmortalPetProgress* Progress =
		FindMutablePetProgress(State, State.ActivePetId);
	FImmortalPetDefinition Definition;
	if (!Progress || !Progress->bOwned || Experience <= 0
		|| !GetPetDefinition(State.ActivePetId, Definition))
	{
		return Result;
	}

	Progress->Level = FMath::Clamp(
		Progress->Level, 1, MaximumPetLevel);
	if (Progress->Level >= MaximumPetLevel)
	{
		Progress->Experience = 0;
	}
	else
	{
		Progress->Experience = FMath::Clamp(
			Progress->Experience,
			0,
			FMath::Max(
				GetExperienceRequiredForLevel(Progress->Level) - 1,
				0));
	}
	Result.PreviousLevel = Progress->Level;
	int64 Remaining = Experience;
	int64 ExperienceAbsorbed = 0;
	while (Remaining > 0 && Progress->Level < MaximumPetLevel)
	{
		const int64 Required =
			GetExperienceRequiredForLevel(Progress->Level);
		const int64 Needed =
			FMath::Max<int64>(
				Required - static_cast<int64>(Progress->Experience),
				1);
		if (Remaining < Needed)
		{
			Progress->Experience += static_cast<int32>(Remaining);
			ExperienceAbsorbed += Remaining;
			Remaining = 0;
		}
		else
		{
			Remaining -= Needed;
			ExperienceAbsorbed += Needed;
			++Progress->Level;
			Progress->Experience = 0;
			++Result.LevelsGained;
		}
	}
	if (Progress->Level >= MaximumPetLevel)
	{
		Progress->Level = MaximumPetLevel;
		Progress->Experience = 0;
	}
	const int64 SafeKills = FMath::Max(CombatKills, 0);
	Progress->TotalCombatKills = SaturatingAddNonNegative(
		Progress->TotalCombatKills,
		SafeKills);
	State.TotalCombatKills = SaturatingAddNonNegative(
		State.TotalCombatKills,
		SafeKills);
	if (ExperienceAbsorbed <= 0 && SafeKills <= 0)
	{
		return Result;
	}
	AdvancePetRevision(State);

	Result.bSucceeded = true;
	Result.ExperienceGranted = static_cast<int32>(
		FMath::Min<int64>(ExperienceAbsorbed, MAX_int32));
	Result.CurrentLevel = Progress->Level;
	Result.CurrentExperience = Progress->Experience;
	Result.ExperienceToNextLevel =
		GetExperienceRequiredForLevel(Progress->Level);
	return Result;
}

int32 UImmortalPetLibrary::CalculateCombatExperienceReward(
	const int32 DifficultyIndex,
	const bool bElite,
	const bool bBoss,
	const bool bWorldBoss)
{
	const int32 SafeDifficulty =
		FMath::Clamp(DifficultyIndex, 1, 9999);
	int64 Result = 8 + SafeDifficulty / 8;
	if (bElite)
	{
		Result *= 2;
	}
	if (bBoss)
	{
		Result *= 5;
	}
	if (bWorldBoss)
	{
		Result = FMath::Max<int64>(Result * 3, 180);
	}
	return static_cast<int32>(FMath::Clamp<int64>(
		Result, 1, 1000000));
}
