// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalMapTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../Items/ImmortalMaterialTypes.h"
#include "../Save/ImmortalPathSaveGame.h"
#include "Misc/AutomationTest.h"

namespace
{
	bool AreMapStatesEqual(const FImmortalMapSystemState& Left, const FImmortalMapSystemState& Right)
	{
		if (Left.bInitialized != Right.bInitialized
			|| Left.ActiveMapId != Right.ActiveMapId
			|| Left.MapProgress.Num() != Right.MapProgress.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Left.MapProgress.Num(); ++Index)
		{
			const FImmortalMapProgress& LeftProgress = Left.MapProgress[Index];
			const FImmortalMapProgress& RightProgress = Right.MapProgress[Index];
			if (LeftProgress.MapId != RightProgress.MapId
				|| LeftProgress.Stage != RightProgress.Stage
				|| LeftProgress.StageKills != RightProgress.StageKills
				|| LeftProgress.bCompleted != RightProgress.bCompleted)
			{
				return false;
			}
		}
		return true;
	}

	FString MakeMaterialPoolSignature(const TArray<FName>& MaterialIds)
	{
		TArray<FString> SortedIds;
		SortedIds.Reserve(MaterialIds.Num());
		for (const FName MaterialId : MaterialIds)
		{
			SortedIds.Add(MaterialId.ToString());
		}
		SortedIds.Sort();
		return FString::Join(SortedIds, TEXT("|"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalMapCatalogProgressAndMigrationTest,
	"ImmortalPath.Maps.CatalogProgressMigrationAndDrops",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalMapCatalogProgressAndMigrationTest::RunTest(const FString& Parameters)
{
	const TArray<FName> ExpectedMapIds = {
		TEXT("QingyunMountain"),
		TEXT("DemonWolfValley"),
		TEXT("MyriadBeastForest"),
		TEXT("BlackWindCave"),
		TEXT("AncientRuins"),
		TEXT("NetherValley"),
		TEXT("NineNetherSecretRealm"),
		TEXT("ImmortalPalaceRuins")
	};
	const TArray<int32> ExpectedRealmIndices = {0, 0, 1, 1, 2, 2, 3, 3};
	const TArray<FName> MapIds = UImmortalMapLibrary::GetKnownMapIds();

	TestEqual(TEXT("SaveGame version is v23 after death recovery persistence was added"),
		UImmortalPathSaveGame::CurrentSaveVersion, 23);
	TestEqual(TEXT("The complete catalog contains eight maps"), MapIds.Num(), ExpectedMapIds.Num());
	TestEqual(TEXT("Qingyun Mountain remains the stable default map ID"),
		UImmortalMapLibrary::GetQingyunMountainId(), ExpectedMapIds[0]);

	TSet<FName> UniqueMapIds;
	TSet<FString> UniqueMaterialPoolSignatures;
	float PreviousHealthMultiplier = 0.0f;
	float PreviousAttackMultiplier = 0.0f;
	float PreviousDefenseMultiplier = 0.0f;
	float PreviousStoneMultiplier = 0.0f;
	int32 PreviousEquipmentLevelBonus = -1;
	int32 PreviousEffectiveFirstStage = 0;
	int32 PreviousMinimumQuality = -1;
	int32 PreviousBossQuality = -1;

	for (int32 MapIndex = 0; MapIndex < ExpectedMapIds.Num(); ++MapIndex)
	{
		if (!MapIds.IsValidIndex(MapIndex))
		{
			AddError(FString::Printf(TEXT("Catalog is missing expected map index %d"), MapIndex));
			continue;
		}

		const FName MapId = MapIds[MapIndex];
		TestEqual(*FString::Printf(TEXT("Stable map order at index %d"), MapIndex), MapId, ExpectedMapIds[MapIndex]);
		TestFalse(*FString::Printf(TEXT("Map ID is unique: %s"), *MapId.ToString()), UniqueMapIds.Contains(MapId));
		UniqueMapIds.Add(MapId);

		FImmortalMapDefinition Definition;
		const bool bDefinitionFound = UImmortalMapLibrary::GetMapDefinition(MapId, Definition);
		TestTrue(*FString::Printf(TEXT("Definition exists: %s"), *MapId.ToString()), bDefinitionFound);
		if (!bDefinitionFound)
		{
			continue;
		}

		TestTrue(*FString::Printf(TEXT("Definition is valid: %s"), *MapId.ToString()), Definition.IsValid());
		TestEqual(TEXT("Definition preserves its stable ID"), Definition.MapId, MapId);
		TestEqual(TEXT("Definition order matches the catalog"), Definition.OrderIndex, MapIndex);
		TestEqual(TEXT("Definition uses the documented realm unlock"),
			Definition.RequiredRealmIndex, ExpectedRealmIndices[MapIndex]);
		TestFalse(TEXT("Map display name is present"), Definition.DisplayName.IsEmpty());
		TestFalse(TEXT("Normal monster name is present"), Definition.NormalMonsterName.IsEmpty());
		TestFalse(TEXT("Boss name is present"), Definition.BossName.IsEmpty());
		TestEqual(TEXT("Every map has independent stages 1 through 999"), Definition.MaximumStage, 999);
		TestEqual(TEXT("Every tenth stage is configured as a boss gate"), Definition.BossStageInterval, 10);

		TestTrue(TEXT("Health difficulty stays positive and increases by map order"),
			Definition.HealthMultiplier > PreviousHealthMultiplier);
		TestTrue(TEXT("Attack difficulty stays positive and increases by map order"),
			Definition.AttackMultiplier > PreviousAttackMultiplier);
		TestTrue(TEXT("Defense difficulty is non-negative and increases by map order"),
			Definition.DefenseMultiplier > PreviousDefenseMultiplier);
		TestTrue(TEXT("Spirit-stone multiplier increases by map order"),
			Definition.SpiritStoneMultiplier > PreviousStoneMultiplier);
		TestTrue(TEXT("Equipment-level bonus increases by map order"),
			Definition.EquipmentLevelBonus > PreviousEquipmentLevelBonus);
		TestTrue(TEXT("Minimum equipment quality never regresses"),
			static_cast<int32>(Definition.MinimumEquipmentQuality) >= PreviousMinimumQuality);
		TestTrue(TEXT("Boss equipment quality never regresses"),
			static_cast<int32>(Definition.BossMinimumEquipmentQuality) >= PreviousBossQuality);
		TestTrue(TEXT("Boss quality is never below the map's normal minimum"),
			static_cast<int32>(Definition.BossMinimumEquipmentQuality)
				>= static_cast<int32>(Definition.MinimumEquipmentQuality));

		const int32 EffectiveFirstStage = UImmortalMapLibrary::GetEffectiveAdventureStage(MapId, 1);
		TestEqual(TEXT("Effective first stage includes the exact map-order offset"),
			EffectiveFirstStage, 1 + MapIndex * 125);
		TestTrue(TEXT("A later map has a strictly harder effective first stage"),
			EffectiveFirstStage > PreviousEffectiveFirstStage);
		TestEqual(TEXT("A local stage below one normalizes to stage one before applying difficulty"),
			UImmortalMapLibrary::GetEffectiveAdventureStage(MapId, 0), EffectiveFirstStage);
		TestEqual(TEXT("Effective adventure stage is capped at 999"),
			UImmortalMapLibrary::GetEffectiveAdventureStage(MapId, 1000), 999);

		TestEqual(TEXT("Each map has a four-material local pool"), Definition.MaterialPoolIds.Num(), 4);
		TSet<FName> UniqueMaterialsInMap;
		for (int32 MaterialIndex = 0; MaterialIndex < Definition.MaterialPoolIds.Num(); ++MaterialIndex)
		{
			const FName MaterialId = Definition.MaterialPoolIds[MaterialIndex];
			FImmortalMaterialDefinition MaterialDefinition;
			TestTrue(*FString::Printf(TEXT("Map material exists: %s/%s"),
				*MapId.ToString(), *MaterialId.ToString()),
				UImmortalMaterialLibrary::GetMaterialDefinition(MaterialId, MaterialDefinition));
			TestFalse(TEXT("A map material pool has no duplicate IDs"), UniqueMaterialsInMap.Contains(MaterialId));
			UniqueMaterialsInMap.Add(MaterialId);

			// Boss material selection is intentionally deterministic by drop index. This
			// verifies the configured local table without asserting random frequencies.
			const FImmortalMaterialStack BossDrop = UImmortalMaterialLibrary::GenerateMapDrop(
				MapId, 999, true, MaterialIndex);
			TestTrue(TEXT("Boss material drop is valid"), BossDrop.IsValid());
			TestEqual(TEXT("Boss material drop rotates through the active map pool"),
				BossDrop.MaterialId, MaterialId);
			TestTrue(TEXT("Boss material quantity is positive"), BossDrop.Quantity > 0);
		}

		const FString PoolSignature = MakeMaterialPoolSignature(Definition.MaterialPoolIds);
		TestFalse(TEXT("Every map has a distinct material-pool signature"),
			UniqueMaterialPoolSignatures.Contains(PoolSignature));
		UniqueMaterialPoolSignatures.Add(PoolSignature);
		const FImmortalMaterialStack NormalDrop = UImmortalMaterialLibrary::GenerateMapDrop(MapId, 1, false, 0);
		TestTrue(TEXT("Normal map material drop is valid"), NormalDrop.IsValid());
		TestTrue(TEXT("Normal material always belongs to the active map pool"),
			Definition.MaterialPoolIds.Contains(NormalDrop.MaterialId));

		// NormalizeState/SetMapProgress use this same schedule: every multiple of
		// ten is a boss gate, and stage 999 is an additional final boss gate.
		FImmortalMapSystemState BossBoundaryState = UImmortalMapLibrary::CreateMigratedState(1, 0, false);
		for (int32 BossStage = Definition.BossStageInterval;
			BossStage < Definition.MaximumStage;
			BossStage += Definition.BossStageInterval)
		{
			FImmortalMapProgress BossProgress;
			BossProgress.MapId = MapId;
			BossProgress.Stage = BossStage;
			BossProgress.StageKills = 7;
			TestTrue(TEXT("Boss-stage progress can be written"),
				UImmortalMapLibrary::SetMapProgress(BossBoundaryState, BossProgress));
			FImmortalMapProgress StoredBossProgress;
			TestTrue(TEXT("Boss-stage progress can be read"),
				UImmortalMapLibrary::GetMapProgress(BossBoundaryState, MapId, StoredBossProgress));
			TestEqual(*FString::Printf(TEXT("Stage %d is a pending one-boss gate"), BossStage),
				StoredBossProgress.StageKills, 0);
		}

		FImmortalMapProgress FinalBossProgress;
		FinalBossProgress.MapId = MapId;
		FinalBossProgress.Stage = 999;
		FinalBossProgress.StageKills = 7;
		TestTrue(TEXT("Final-boss progress can be written"),
			UImmortalMapLibrary::SetMapProgress(BossBoundaryState, FinalBossProgress));
		TestTrue(TEXT("Final-boss progress can be read"),
			UImmortalMapLibrary::GetMapProgress(BossBoundaryState, MapId, FinalBossProgress));
		TestEqual(TEXT("Stage 999 is a pending final boss even though it is not divisible by ten"),
			FinalBossProgress.StageKills, 0);
		TestFalse(TEXT("Pending stage-999 boss is not prematurely complete"), FinalBossProgress.bCompleted);
		FinalBossProgress.bCompleted = true;
		TestTrue(TEXT("Completed final-boss progress can be written"),
			UImmortalMapLibrary::SetMapProgress(BossBoundaryState, FinalBossProgress));
		TestTrue(TEXT("Completed final-boss progress can be read"),
			UImmortalMapLibrary::GetMapProgress(BossBoundaryState, MapId, FinalBossProgress));
		TestTrue(TEXT("Defeating the stage-999 boss completes only that map"), FinalBossProgress.bCompleted);
		TestEqual(TEXT("Completed final boss stores its one required kill"), FinalBossProgress.StageKills, 1);

		PreviousHealthMultiplier = Definition.HealthMultiplier;
		PreviousAttackMultiplier = Definition.AttackMultiplier;
		PreviousDefenseMultiplier = Definition.DefenseMultiplier;
		PreviousStoneMultiplier = Definition.SpiritStoneMultiplier;
		PreviousEquipmentLevelBonus = Definition.EquipmentLevelBonus;
		PreviousEffectiveFirstStage = EffectiveFirstStage;
		PreviousMinimumQuality = static_cast<int32>(Definition.MinimumEquipmentQuality);
		PreviousBossQuality = static_cast<int32>(Definition.BossMinimumEquipmentQuality);
	}

	TestEqual(TEXT("All eight catalog IDs are unique"), UniqueMapIds.Num(), ExpectedMapIds.Num());
	TestEqual(TEXT("All eight material tables are distinct"),
		UniqueMaterialPoolSignatures.Num(), ExpectedMapIds.Num());
	FImmortalMapDefinition MissingDefinition;
	TestFalse(TEXT("Unknown map IDs have no definition"),
		UImmortalMapLibrary::GetMapDefinition(TEXT("MissingMap"), MissingDefinition));
	TestFalse(TEXT("Unknown map IDs never unlock"),
		UImmortalMapLibrary::IsMapUnlocked(TEXT("MissingMap"), 9));
	TestFalse(TEXT("Invalid negative realm never unlocks even entry maps"),
		UImmortalMapLibrary::IsMapUnlocked(ExpectedMapIds[0], -1));

	for (int32 RealmIndex = 0; RealmIndex <= 3; ++RealmIndex)
	{
		int32 UnlockedCount = 0;
		for (int32 MapIndex = 0; MapIndex < ExpectedMapIds.Num(); ++MapIndex)
		{
			const bool bExpectedUnlocked = ExpectedRealmIndices[MapIndex] <= RealmIndex;
			const bool bUnlocked = UImmortalMapLibrary::IsMapUnlocked(ExpectedMapIds[MapIndex], RealmIndex);
			TestEqual(*FString::Printf(TEXT("Realm %d unlock state for %s"),
				RealmIndex, *ExpectedMapIds[MapIndex].ToString()), bUnlocked, bExpectedUnlocked);
			UnlockedCount += bUnlocked ? 1 : 0;
		}
		TestEqual(*FString::Printf(TEXT("Realm %d unlocks the expected map count"), RealmIndex),
			UnlockedCount, (RealmIndex + 1) * 2);
	}

	// v11 stored only these three Qingyun fields. Migration must preserve them,
	// create seven independent defaults, and select Qingyun without leaking its
	// progress into any other map.
	FImmortalMapSystemState Migrated = UImmortalMapLibrary::CreateMigratedState(22, 7, false);
	TestTrue(TEXT("v11 migration initializes the map state"), Migrated.bInitialized);
	TestEqual(TEXT("v11 migration keeps Qingyun active"), Migrated.ActiveMapId, ExpectedMapIds[0]);
	TestEqual(TEXT("v11 migration creates one progress record per map"),
		Migrated.MapProgress.Num(), ExpectedMapIds.Num());
	for (int32 MapIndex = 0; MapIndex < ExpectedMapIds.Num(); ++MapIndex)
	{
		FImmortalMapProgress Progress;
		TestTrue(TEXT("Migrated map progress is readable"),
			UImmortalMapLibrary::GetMapProgress(Migrated, ExpectedMapIds[MapIndex], Progress));
		TestEqual(TEXT("Migrated record preserves its map ID"), Progress.MapId, ExpectedMapIds[MapIndex]);
		TestEqual(TEXT("Only Qingyun receives the legacy stage"), Progress.Stage, MapIndex == 0 ? 22 : 1);
		TestEqual(TEXT("Only Qingyun receives the legacy kills"), Progress.StageKills, MapIndex == 0 ? 7 : 0);
		TestFalse(TEXT("An incomplete v11 map does not migrate as complete"), Progress.bCompleted);
	}

	const FImmortalMapSystemState MigratedBoss = UImmortalMapLibrary::CreateMigratedState(10, 8, false);
	FImmortalMapProgress QingyunProgress;
	TestTrue(TEXT("Migrated Qingyun boss progress exists"),
		UImmortalMapLibrary::GetMapProgress(MigratedBoss, ExpectedMapIds[0], QingyunProgress));
	TestEqual(TEXT("Partial kills cannot survive on a migrated boss gate"), QingyunProgress.StageKills, 0);
	const FImmortalMapSystemState MigratedCompleted = UImmortalMapLibrary::CreateMigratedState(999, 999, true);
	TestTrue(TEXT("Migrated completed Qingyun progress exists"),
		UImmortalMapLibrary::GetMapProgress(MigratedCompleted, ExpectedMapIds[0], QingyunProgress));
	TestTrue(TEXT("v11 final completion is preserved"), QingyunProgress.bCompleted);
	TestEqual(TEXT("v11 final completion stores one boss kill"), QingyunProgress.StageKills, 1);
	const FImmortalMapSystemState MigratedCorrupt = UImmortalMapLibrary::CreateMigratedState(5000, 5000, false);
	TestTrue(TEXT("Clamped migrated Qingyun progress exists"),
		UImmortalMapLibrary::GetMapProgress(MigratedCorrupt, ExpectedMapIds[0], QingyunProgress));
	TestEqual(TEXT("Corrupt legacy stage is clamped to 999"), QingyunProgress.Stage, 999);
	TestEqual(TEXT("Incomplete final boss cannot keep corrupt kill progress"), QingyunProgress.StageKills, 0);
	TestFalse(TEXT("Legacy completion flag is authoritative"), QingyunProgress.bCompleted);

	// Dirty-state normalization removes unknown IDs, merges duplicates by the
	// furthest legitimate progress, repairs missing entries, and is idempotent.
	FImmortalMapSystemState DirtyState;
	DirtyState.ActiveMapId = TEXT("MissingMap");
	DirtyState.MapProgress = {
		{ExpectedMapIds[0], 24, 9, false},
		{ExpectedMapIds[0], 25, 4, false},
		{ExpectedMapIds[0], 25, 8, false},
		{ExpectedMapIds[1], -20, 999, true},
		{TEXT("MissingMap"), 900, 9, true}
	};
	UImmortalMapLibrary::NormalizeState(DirtyState);
	TestTrue(TEXT("Normalization marks the state initialized"), DirtyState.bInitialized);
	TestEqual(TEXT("Unknown active map falls back to Qingyun"), DirtyState.ActiveMapId, ExpectedMapIds[0]);
	TestEqual(TEXT("Normalization restores exactly eight known records"),
		DirtyState.MapProgress.Num(), ExpectedMapIds.Num());
	TestTrue(TEXT("Normalized Qingyun progress exists"),
		UImmortalMapLibrary::GetMapProgress(DirtyState, ExpectedMapIds[0], QingyunProgress));
	TestEqual(TEXT("Duplicate merge keeps the furthest stage"), QingyunProgress.Stage, 25);
	TestEqual(TEXT("Duplicate merge keeps the furthest kills at equal stage"), QingyunProgress.StageKills, 8);
	FImmortalMapProgress WolfProgress;
	TestTrue(TEXT("Normalized wolf progress exists"),
		UImmortalMapLibrary::GetMapProgress(DirtyState, ExpectedMapIds[1], WolfProgress));
	TestEqual(TEXT("Invalid low stage clamps to one"), WolfProgress.Stage, 1);
	TestEqual(TEXT("A false completion below stage 999 is cleared"), WolfProgress.bCompleted, false);
	TestEqual(TEXT("Normal-stage corrupt kills clamp to nine"), WolfProgress.StageKills, 9);
	const FImmortalMapSystemState OnceNormalized = DirtyState;
	UImmortalMapLibrary::NormalizeState(DirtyState);
	TestTrue(TEXT("Normalization is idempotent"), AreMapStatesEqual(DirtyState, OnceNormalized));

	// Updating one map must never overwrite another map's independent progress.
	FImmortalMapSystemState IndependentState = UImmortalMapLibrary::CreateMigratedState(123, 4, false);
	FImmortalMapProgress NewWolfProgress;
	NewWolfProgress.MapId = ExpectedMapIds[1];
	NewWolfProgress.Stage = 456;
	NewWolfProgress.StageKills = 7;
	TestTrue(TEXT("A second map accepts independent progress"),
		UImmortalMapLibrary::SetMapProgress(IndependentState, NewWolfProgress));
	TestTrue(TEXT("Independent Qingyun progress remains readable"),
		UImmortalMapLibrary::GetMapProgress(IndependentState, ExpectedMapIds[0], QingyunProgress));
	TestEqual(TEXT("Writing wolf progress does not change Qingyun stage"), QingyunProgress.Stage, 123);
	TestEqual(TEXT("Writing wolf progress does not change Qingyun kills"), QingyunProgress.StageKills, 4);
	TestTrue(TEXT("Independent wolf progress remains readable"),
		UImmortalMapLibrary::GetMapProgress(IndependentState, ExpectedMapIds[1], WolfProgress));
	TestEqual(TEXT("Wolf stage persists independently"), WolfProgress.Stage, 456);
	TestEqual(TEXT("Wolf kills persist independently"), WolfProgress.StageKills, 7);
	const FImmortalMapSystemState BeforeRejectedWrite = IndependentState;
	FImmortalMapProgress UnknownProgress;
	UnknownProgress.MapId = TEXT("MissingMap");
	UnknownProgress.Stage = 999;
	UnknownProgress.bCompleted = true;
	TestFalse(TEXT("Unknown map progress is rejected"),
		UImmortalMapLibrary::SetMapProgress(IndependentState, UnknownProgress));
	TestTrue(TEXT("Rejected map write makes no state changes"),
		AreMapStatesEqual(IndependentState, BeforeRejectedWrite));
	return true;
}

#endif
