// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "ImmortalAscensionTypes.h"

#include "../Alchemy/ImmortalAlchemyTypes.h"
#include "../Progression/ImmortalCultivationComponent.h"
#include "../Save/ImmortalPathSaveGame.h"
#include "Misc/AutomationTest.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"

namespace
{
	FImmortalMapSystemState MakeCompletedAscensionCycle()
	{
		FImmortalMapSystemState Result =
			UImmortalMapLibrary::CreateMigratedState(
				1, 0, false);
		for (const FName MapId :
			UImmortalMapLibrary::GetKnownMapIds())
		{
			FImmortalMapDefinition Definition;
			FImmortalMapProgress Progress;
			if (UImmortalMapLibrary::GetMapDefinition(
					MapId, Definition)
				&& UImmortalMapLibrary::GetMapProgress(
					Result, MapId, Progress))
			{
				Progress.Stage =
					Definition.MaximumStage;
				Progress.StageKills = 0;
				Progress.bCompleted = true;
				UImmortalMapLibrary::SetMapProgress(
					Result, Progress);
			}
		}
		Result.ActiveMapId =
			UImmortalMapLibrary::GetQingyunMountainId();
		UImmortalMapLibrary::NormalizeState(Result);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalAscensionStateTest,
	"ImmortalPath.Ascension.StateNormalizationAndSaveSchemaV23",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalAscensionEligibilityTest,
	"ImmortalPath.Ascension.EligibilityGateMatrix",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalAscensionRewardTest,
	"ImmortalPath.Ascension.RepeatableRewardAndLimit",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalAscensionPathTest,
	"ImmortalPath.Ascension.PathInvestmentAndMultipliers",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalAscensionCycleResetTest,
	"ImmortalPath.Ascension.LifetimeMapRecordsAndCycleReset",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalAscensionReachabilityTest,
	"ImmortalPath.Ascension.CultivationAndBreakthroughPillReachability",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FImmortalAscensionStateTest::RunTest(
	const FString& Parameters)
{
	const FImmortalAscensionState DefaultState =
		UImmortalAscensionLibrary::CreateDefaultState();
	TestTrue(TEXT("New ascension state is initialized"),
		DefaultState.bInitialized);
	TestEqual(TEXT("New state has no completed ascension"),
		DefaultState.AscensionCount, 0);
	TestEqual(TEXT("New state has no retroactive seals"),
		DefaultState.ImmortalSeals, 0);
	TestEqual(TEXT("New state begins at revision one"),
		DefaultState.Revision, 1);
	TestEqual(TEXT("New state has one lifetime record per map"),
		DefaultState.LifetimeMapRecords.Num(),
		UImmortalMapLibrary::GetKnownMapIds().Num());
	TestEqual(TEXT("Save schema advances to v23"),
		UImmortalPathSaveGame::CurrentSaveVersion, 23);

	UImmortalPathSaveGame* SaveGame =
		NewObject<UImmortalPathSaveGame>();
	TestNotNull(TEXT("v23 save object can be created"), SaveGame);
	TestFalse(TEXT("Raw v23 save has no ascension marker until player writes"),
		SaveGame->bAscensionSystemInitialized);
	SaveGame->bAscensionSystemInitialized = true;
	SaveGame->AscensionState = DefaultState;
	TestTrue(TEXT("v23 marker and state are independent and serializable"),
		SaveGame->bAscensionSystemInitialized
			&& SaveGame->AscensionState.bInitialized);

	FImmortalAscensionState Dirty;
	Dirty.bInitialized = false;
	Dirty.Revision = -8;
	Dirty.AscensionCount = 1200;
	Dirty.ImmortalSeals = -9;
	Dirty.TotalImmortalSealsEarned = -20;
	Dirty.BattlePathRank = -4;
	Dirty.EnlightenmentPathRank = 80;
	Dirty.FortunePathRank = 6;
	Dirty.LastAscensionUtcTicks = -100;
	FImmortalAscensionMapLegacy& DirtyKnown =
		Dirty.LifetimeMapRecords.AddDefaulted_GetRef();
	DirtyKnown.MapId =
		UImmortalMapLibrary::GetQingyunMountainId();
	DirtyKnown.HighestStage = 5000;
	DirtyKnown.TimesCompleted = -4;
	FImmortalAscensionMapLegacy& DirtyDuplicate =
		Dirty.LifetimeMapRecords.AddDefaulted_GetRef();
	DirtyDuplicate.MapId = DirtyKnown.MapId;
	DirtyDuplicate.HighestStage = 400;
	DirtyDuplicate.TimesCompleted = 2;
	FImmortalAscensionMapLegacy& DirtyUnknown =
		Dirty.LifetimeMapRecords.AddDefaulted_GetRef();
	DirtyUnknown.MapId = TEXT("UnknownMap");
	DirtyUnknown.HighestStage = 999;
	DirtyUnknown.TimesCompleted = 999;
	TestTrue(TEXT("Malformed state is repaired"),
		UImmortalAscensionLibrary::NormalizeState(Dirty));
	TestTrue(TEXT("Repair restores initialized marker"),
		Dirty.bInitialized);
	TestEqual(TEXT("Ascension count is bounded"),
		Dirty.AscensionCount,
		UImmortalAscensionLibrary::MaximumAscensionCount);
	TestEqual(TEXT("Negative seals become zero"),
		Dirty.ImmortalSeals, 0);
	TestEqual(TEXT("Negative battle rank becomes zero"),
		Dirty.BattlePathRank, 0);
	TestEqual(TEXT("Enlightenment rank is capped"),
		Dirty.EnlightenmentPathRank,
		UImmortalAscensionLibrary::MaximumPathRank);
	TestEqual(TEXT("Valid fortune rank is preserved"),
		Dirty.FortunePathRank, 6);
	TestEqual(TEXT("Audit seals cover all invested ranks"),
		Dirty.TotalImmortalSealsEarned,
		static_cast<int64>(5016));
	TestEqual(TEXT("Invalid timestamp becomes zero"),
		Dirty.LastAscensionUtcTicks,
		static_cast<int64>(0));
	TestEqual(TEXT("Map records are canonicalized to eight entries"),
		Dirty.LifetimeMapRecords.Num(),
		UImmortalMapLibrary::GetKnownMapIds().Num());
	FImmortalAscensionMapLegacy QingyunLegacy;
	TestTrue(TEXT("Known map lifetime record remains queryable"),
		UImmortalAscensionLibrary::GetLifetimeMapRecord(
			Dirty,
			UImmortalMapLibrary::GetQingyunMountainId(),
			QingyunLegacy));
	TestEqual(TEXT("Lifetime highest stage is clamped"),
		QingyunLegacy.HighestStage, 999);
	TestEqual(TEXT("Duplicate completion high-water is retained"),
		QingyunLegacy.TimesCompleted, 2);
	const int32 RepairedRevision = Dirty.Revision;
	TestFalse(TEXT("A canonical state normalizes idempotently"),
		UImmortalAscensionLibrary::NormalizeState(Dirty));
	TestEqual(TEXT("Idempotent normalization preserves revision"),
		Dirty.Revision, RepairedRevision);
	return true;
}

bool FImmortalAscensionEligibilityTest::RunTest(
	const FString& Parameters)
{
	const FImmortalAscensionState State =
		UImmortalAscensionLibrary::CreateDefaultState();
	const FImmortalAscensionEligibility Ready =
		UImmortalAscensionLibrary::EvaluateEligibility(
			State, true, true, true, false, false);
	TestTrue(TEXT("All five gates allow ascension"),
		Ready.bEligible);
	TestEqual(TEXT("First ascension advertises three seals"),
		Ready.RewardImmortalSeals, 3);

	TestFalse(TEXT("Realm gate is mandatory"),
		UImmortalAscensionLibrary::EvaluateEligibility(
			State, false, true, true, false, false).bEligible);
	TestFalse(TEXT("Final map gate is mandatory"),
		UImmortalAscensionLibrary::EvaluateEligibility(
			State, true, false, true, false, false).bEligible);
	TestFalse(TEXT("Qingyun ritual location is mandatory"),
		UImmortalAscensionLibrary::EvaluateEligibility(
			State, true, true, false, false, false).bEligible);
	TestFalse(TEXT("Active World Boss blocks ascension"),
		UImmortalAscensionLibrary::EvaluateEligibility(
			State, true, true, true, true, false).bEligible);
	TestFalse(TEXT("Active Endless Dungeon blocks ascension"),
		UImmortalAscensionLibrary::EvaluateEligibility(
			State, true, true, true, false, true).bEligible);

	FImmortalAscensionState Capped = State;
	Capped.AscensionCount =
		UImmortalAscensionLibrary::MaximumAscensionCount;
	TestFalse(TEXT("The 999-cycle limit is mandatory"),
		UImmortalAscensionLibrary::EvaluateEligibility(
			Capped, true, true, true, false, false).bEligible);
	return true;
}

bool FImmortalAscensionRewardTest::RunTest(
	const FString& Parameters)
{
	TestEqual(TEXT("Cycles one through five grant three seals"),
		UImmortalAscensionLibrary::CalculateAscensionReward(0), 3);
	TestEqual(TEXT("Fifth completed cycle still previews four"),
		UImmortalAscensionLibrary::CalculateAscensionReward(5), 4);
	TestEqual(TEXT("Reward growth is capped at twelve"),
		UImmortalAscensionLibrary::CalculateAscensionReward(999), 12);
	int64 MaximumLifetimeRewards = 0;
	for (int32 Index = 0;
		Index < UImmortalAscensionLibrary
			::MaximumAscensionCount;
		++Index)
	{
		MaximumLifetimeRewards +=
			UImmortalAscensionLibrary
				::CalculateAscensionReward(Index);
	}
	const int64 ThreePathCapacity =
		3 * UImmortalAscensionLibrary
			::CalculatePathCumulativeCost(
				UImmortalAscensionLibrary
					::MaximumPathRank);
	TestTrue(TEXT("All 999 cycles still leave meaningful path choices"),
		MaximumLifetimeRewards < ThreePathCapacity);

	FImmortalAscensionState State =
		UImmortalAscensionLibrary::CreateDefaultState();
	const FImmortalMapSystemState CompletedCycle =
		MakeCompletedAscensionCycle();
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const FImmortalAscensionOperationResult Result =
			UImmortalAscensionLibrary::GrantAscension(
				State, CompletedCycle, 1000 + Index);
		TestTrue(*FString::Printf(
			TEXT("Ascension cycle %d succeeds"),
			Index + 1),
			Result.bSucceeded);
	}
	TestEqual(TEXT("Six cycles are recorded"),
		State.AscensionCount, 6);
	TestEqual(TEXT("Six cycles grant nineteen seals"),
		State.ImmortalSeals, 19);
	TestEqual(TEXT("Earned-seal audit matches grant total"),
		State.TotalImmortalSealsEarned,
		static_cast<int64>(19));
	TestEqual(TEXT("Timestamp advances monotonically"),
		State.LastAscensionUtcTicks,
		static_cast<int64>(1005));

	State.AscensionCount =
		UImmortalAscensionLibrary::MaximumAscensionCount;
	const int32 SealsBeforeCap = State.ImmortalSeals;
	const int32 RevisionBeforeCap = State.Revision;
	const FImmortalAscensionOperationResult Capped =
		UImmortalAscensionLibrary::GrantAscension(
			State, CompletedCycle, 9999);
	TestFalse(TEXT("A capped state rejects further ascension"),
		Capped.bSucceeded);
	TestEqual(TEXT("Rejected capped ascension preserves seals"),
		State.ImmortalSeals, SealsBeforeCap);
	TestEqual(TEXT("Rejected capped ascension preserves revision"),
		State.Revision, RevisionBeforeCap);
	return true;
}

bool FImmortalAscensionCycleResetTest::RunTest(
	const FString& Parameters)
{
	const FImmortalMapSystemState CompletedCycle =
		MakeCompletedAscensionCycle();
	FImmortalAscensionState State =
		UImmortalAscensionLibrary::CreateDefaultState();
	TestTrue(TEXT("First cycle grant succeeds"),
		UImmortalAscensionLibrary::GrantAscension(
			State, CompletedCycle, 1000).bSucceeded);
	TestEqual(TEXT("All completed maps enter lifetime records"),
		UImmortalAscensionLibrary
			::GetLifetimeCompletedMapCount(State),
		UImmortalMapLibrary::GetKnownMapIds().Num());
	for (const FName MapId :
		UImmortalMapLibrary::GetKnownMapIds())
	{
		FImmortalAscensionMapLegacy Record;
		TestTrue(*FString::Printf(
			TEXT("%s has a lifetime record"),
			*MapId.ToString()),
			UImmortalAscensionLibrary::GetLifetimeMapRecord(
				State, MapId, Record));
		TestEqual(*FString::Printf(
			TEXT("%s records stage 999"),
			*MapId.ToString()),
			Record.HighestStage, 999);
		TestEqual(*FString::Printf(
			TEXT("%s records one completion"),
			*MapId.ToString()),
			Record.TimesCompleted, 1);
	}
	TestTrue(TEXT("Second completed cycle grant succeeds"),
		UImmortalAscensionLibrary::GrantAscension(
			State, CompletedCycle, 2000).bSucceeded);
	FImmortalAscensionMapLegacy FinalRecord;
	UImmortalAscensionLibrary::GetLifetimeMapRecord(
		State,
		UImmortalMapLibrary::GetImmortalPalaceRuinsId(),
		FinalRecord);
	TestEqual(TEXT("Repeated completion increments lifetime count"),
		FinalRecord.TimesCompleted, 2);

	const FImmortalMapSystemState NewCycle =
		UImmortalAscensionLibrary::CreateNewCycleMapState();
	TestEqual(TEXT("New cycle always activates Qingyun Mountain"),
		NewCycle.ActiveMapId,
		UImmortalMapLibrary::GetQingyunMountainId());
	TestEqual(TEXT("New cycle contains all eight maps"),
		NewCycle.MapProgress.Num(),
		UImmortalMapLibrary::GetKnownMapIds().Num());
	for (const FName MapId :
		UImmortalMapLibrary::GetKnownMapIds())
	{
		FImmortalMapProgress Progress;
		TestTrue(*FString::Printf(
			TEXT("%s exists in the new cycle"),
			*MapId.ToString()),
			UImmortalMapLibrary::GetMapProgress(
				NewCycle, MapId, Progress));
		TestEqual(*FString::Printf(
			TEXT("%s starts at stage one"),
			*MapId.ToString()),
			Progress.Stage, 1);
		TestEqual(*FString::Printf(
			TEXT("%s starts with zero kills"),
			*MapId.ToString()),
			Progress.StageKills, 0);
		TestFalse(*FString::Printf(
			TEXT("%s starts incomplete"),
			*MapId.ToString()),
			Progress.bCompleted);
	}
	return true;
}

bool FImmortalAscensionReachabilityTest::RunTest(
	const FString& Parameters)
{
	UImmortalCultivationComponent* Cultivation =
		NewObject<UImmortalCultivationComponent>();
	TestNotNull(TEXT("Cultivation component can be created"),
		Cultivation);
	if (!Cultivation)
	{
		return false;
	}
	Cultivation->InitializeProgress(
		EImmortalCultivationRealm::QiRefining,
		1,
		0);
	for (int32 Step = 0; Step < 20; ++Step)
	{
		Cultivation->AddCultivation(
			Cultivation->GetRequiredCultivation()
				- Cultivation->GetCurrentCultivation());
	}
	TestEqual(TEXT("Twenty breakthroughs reach Golden Core one"),
		Cultivation->GetCurrentRealm(),
		EImmortalCultivationRealm::GoldenCore);
	TestEqual(TEXT("Golden Core begins at minor stage one"),
		Cultivation->GetCurrentMinorStage(), 1);

	FImmortalPillDefinition BreakthroughPill;
	TestTrue(TEXT("Breakthrough Pill definition exists"),
		UImmortalAlchemyLibrary::GetPillDefinition(
			TEXT("BreakthroughPill"),
			BreakthroughPill));
	TestEqual(TEXT("Breakthrough Pill unlocks at Golden Core"),
		BreakthroughPill.MinimumRealmIndex, 2);
	TestEqual(TEXT("Breakthrough Pill completes the current stage"),
		BreakthroughPill.Effect,
		EImmortalPillEffect::CompleteCurrentStage);

	// Ordinary Breakthrough Pills call this exact operation. Seventy more
	// successful stage completions cover Golden Core through Tribulation.
	for (int32 Step = 20; Step < 90; ++Step)
	{
		Cultivation->AddCultivation(
			Cultivation->GetRequiredCultivation()
				- Cultivation->GetCurrentCultivation());
	}
	TestTrue(TEXT("Ninety total breakthroughs reach Ascension"),
		Cultivation->HasReachedAscension());
	TestEqual(TEXT("Terminal realm is Ascension"),
		Cultivation->GetCurrentRealm(),
		EImmortalCultivationRealm::Ascension);
	TestEqual(TEXT("Terminal cultivation is normalized to zero"),
		Cultivation->GetCurrentCultivation(), 0);
	return true;
}

bool FImmortalAscensionPathTest::RunTest(
	const FString& Parameters)
{
	FImmortalAscensionState State =
		UImmortalAscensionLibrary::CreateDefaultState();
	State.ImmortalSeals = 3;
	State.TotalImmortalSealsEarned = 3;
	const FImmortalAscensionPathResult Battle =
		UImmortalAscensionLibrary::InvestPath(
			State, EImmortalAscensionPath::Battle);
	const FImmortalAscensionPathResult Enlightenment =
		UImmortalAscensionLibrary::InvestPath(
			State, EImmortalAscensionPath::Enlightenment);
	const FImmortalAscensionPathResult Fortune =
		UImmortalAscensionLibrary::InvestPath(
			State, EImmortalAscensionPath::Fortune);
	TestTrue(TEXT("Battle path accepts one seal"),
		Battle.bSucceeded);
	TestTrue(TEXT("Enlightenment path accepts one seal"),
		Enlightenment.bSucceeded);
	TestTrue(TEXT("Fortune path accepts one seal"),
		Fortune.bSucceeded);
	TestEqual(TEXT("All three seals are consumed"),
		State.ImmortalSeals, 0);
	TestEqual(TEXT("Battle reaches rank one"),
		State.BattlePathRank, 1);
	TestEqual(TEXT("Enlightenment reaches rank one"),
		State.EnlightenmentPathRank, 1);
	TestEqual(TEXT("Fortune reaches rank one"),
		State.FortunePathRank, 1);
	TestEqual(TEXT("The first rank costs one seal"),
		Battle.ImmortalSealsSpent, 1);
	TestTrue(TEXT("Battle rank one is x1.04 damage"),
		FMath::IsNearlyEqual(
			UImmortalAscensionLibrary::CalculateBattleDamageMultiplier(
				State),
			1.04f));
	TestTrue(TEXT("Enlightenment rank one is x1.06 cultivation"),
		FMath::IsNearlyEqual(
			UImmortalAscensionLibrary::CalculateCultivationRateMultiplier(
				State),
			1.06f));
	TestTrue(TEXT("Fortune rank one is x1.03 equipment drops"),
		FMath::IsNearlyEqual(
			UImmortalAscensionLibrary::CalculateEquipmentDropMultiplier(
				State),
			1.03f));

	const int32 RevisionBeforeNoSeal = State.Revision;
	const FImmortalAscensionPathResult NoSeal =
		UImmortalAscensionLibrary::InvestPath(
			State, EImmortalAscensionPath::Battle);
	TestFalse(TEXT("No seal means no additional rank"),
		NoSeal.bSucceeded);
	TestEqual(TEXT("Rejected investment preserves revision"),
		State.Revision, RevisionBeforeNoSeal);

	TestEqual(TEXT("Rank one to rank two costs five seals"),
		UImmortalAscensionLibrary
			::CalculatePathUpgradeCost(1),
		5);
	TestEqual(TEXT("A full path costs 4,950 seals"),
		UImmortalAscensionLibrary
			::CalculatePathCumulativeCost(
				UImmortalAscensionLibrary
					::MaximumPathRank),
		static_cast<int64>(4950));
	State.ImmortalSeals = 5;
	const FImmortalAscensionPathResult SecondBattle =
		UImmortalAscensionLibrary::InvestPath(
			State, EImmortalAscensionPath::Battle);
	TestTrue(TEXT("Five seals purchase battle rank two"),
		SecondBattle.bSucceeded);
	TestEqual(TEXT("Rank-two purchase consumes all five seals"),
		SecondBattle.ImmortalSealsSpent, 5);
	TestEqual(TEXT("Battle reaches rank two"),
		State.BattlePathRank, 2);

	State.ImmortalSeals = 1;
	State.BattlePathRank =
		UImmortalAscensionLibrary::MaximumPathRank;
	const FImmortalAscensionPathResult Capped =
		UImmortalAscensionLibrary::InvestPath(
			State, EImmortalAscensionPath::Battle);
	TestFalse(TEXT("A rank-50 path rejects investment"),
		Capped.bSucceeded);
	TestEqual(TEXT("Rank cap does not consume a seal"),
		State.ImmortalSeals, 1);
	return true;
}

// Source UVs and editable pivots are editor-only Paper2D metadata.
#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalPlayerAscensionAnimationAssetTest,
	"ImmortalPath.Ascension.PlayerAnimationAsset",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FImmortalPlayerAscensionAnimationAssetTest::RunTest(
	const FString& Parameters)
{
	UPaperFlipbook* Flipbook = LoadObject<UPaperFlipbook>(
		nullptr,
		TEXT("/Game/GAME/Asset/Player/ascension/generated/FB_Player_Ascension.FB_Player_Ascension"));
	if (!TestNotNull(
		TEXT("Player ascension Flipbook loads"), Flipbook))
	{
		return false;
	}

	TestEqual(TEXT("Ascension animation has 17 frames"),
		Flipbook->GetNumKeyFrames(), 17);
	TestTrue(TEXT("Ascension animation runs at 12 FPS"),
		FMath::IsNearlyEqual(
			Flipbook->GetFramesPerSecond(), 12.0f));
	TestTrue(TEXT("Ascension animation duration is about 1.42 seconds"),
		FMath::IsNearlyEqual(
			Flipbook->GetTotalDuration(), 17.0f / 12.0f,
			0.01f));

	for (int32 FrameIndex = 0;
		FrameIndex < Flipbook->GetNumKeyFrames();
		++FrameIndex)
	{
		const FPaperFlipbookKeyFrame& KeyFrame =
			Flipbook->GetKeyFrameChecked(FrameIndex);
		UPaperSprite* Sprite = KeyFrame.Sprite;
		if (!TestNotNull(
			*FString::Printf(
				TEXT("Ascension frame %d has a sprite"),
				FrameIndex),
			Sprite))
		{
			continue;
		}
		TestTrue(
			*FString::Printf(
				TEXT("Ascension frame %d has the expected source UV"),
				FrameIndex),
			Sprite->GetSourceUV().Equals(
				FVector2D(FrameIndex * 128, 0)));
		TestTrue(
			*FString::Printf(
				TEXT("Ascension frame %d has a 128x724 source rectangle"),
				FrameIndex),
			Sprite->GetSourceSize().Equals(
				FVector2D(128, 724)));
		TestTrue(
			*FString::Printf(
				TEXT("Ascension frame %d keeps the shared ground pivot"),
				FrameIndex),
			Sprite->GetPivotPosition().Equals(
				FVector2D(
					FrameIndex * 128 + 64,
					583)));
	}
	return true;
}
#endif // WITH_EDITOR

#endif // WITH_DEV_AUTOMATION_TESTS
