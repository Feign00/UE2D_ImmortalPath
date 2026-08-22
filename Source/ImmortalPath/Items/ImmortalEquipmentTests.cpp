// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImmortalEquipmentTypes.h"
#include "../Progression/ImmortalCultivationComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalEquipmentExpansionTest,
	"ImmortalPath.Equipment.SlotsQualitiesAffixesAndNormalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalEquipmentExpansionTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Legacy accessory slot remains numeric four"), static_cast<int32>(EImmortalEquipmentSlot::Accessory), 4);
	TestEqual(TEXT("Nine ordinary equipment slots exist"), static_cast<int32>(EImmortalEquipmentSlot::MAX), 9);
	TestEqual(TEXT("Legacy legendary quality remains numeric four"), static_cast<int32>(EImmortalEquipmentQuality::Legendary), 4);
	TestEqual(TEXT("Divine is the seventh quality"), static_cast<int32>(EImmortalEquipmentQuality::Divine), 6);

	const TCHAR* ExpectedQualityNames[] = {TEXT("凡品"), TEXT("灵品"), TEXT("玄品"), TEXT("地品"), TEXT("天品"), TEXT("仙品"), TEXT("神品")};
	for (int32 QualityIndex = 0; QualityIndex <= static_cast<int32>(EImmortalEquipmentQuality::Divine); ++QualityIndex)
	{
		TestEqual(*FString::Printf(TEXT("Quality %d has the required cultivation name"), QualityIndex),
			UImmortalEquipmentLibrary::GetQualityText(static_cast<EImmortalEquipmentQuality>(QualityIndex)).ToString(),
			FString(ExpectedQualityNames[QualityIndex]));
	}

	TSet<EImmortalEquipmentAffixType> SeenAffixes;
	for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EImmortalEquipmentSlot::MAX); ++SlotIndex)
	{
		const EImmortalEquipmentSlot Slot = static_cast<EImmortalEquipmentSlot>(SlotIndex);
		const FImmortalEquipmentItem Item = UImmortalEquipmentLibrary::GenerateCraftedEquipment(
			50, Slot, EImmortalEquipmentQuality::Divine);
		TestTrue(*FString::Printf(TEXT("Slot %d generates valid equipment"), SlotIndex), Item.IsValid());
		TestEqual(*FString::Printf(TEXT("Slot %d remains exact"), SlotIndex), Item.Slot, Slot);
		TestTrue(*FString::Printf(TEXT("Slot %d has a core stat"), SlotIndex),
			Item.BaseAttackBonus + Item.BaseDefenseBonus + Item.BaseHealthBonus
				+ Item.BaseAttackSpeedBonus + Item.BaseCriticalChanceBonus > 0.0f);
		TestEqual(*FString::Printf(TEXT("Divine slot %d has six affixes"), SlotIndex), Item.Affixes.Num(), 6);
		for (const FImmortalEquipmentAffix& Affix : Item.Affixes)
		{
			TestTrue(TEXT("Generated affix is finite and positive"), FMath::IsFinite(Affix.Value) && Affix.Value > 0.0f);
			TestFalse(TEXT("Generated item has unique affix types"), SeenAffixes.Contains(Affix.Type));
			SeenAffixes.Add(Affix.Type);
		}
		SeenAffixes.Reset();
	}

	for (int32 TypeIndex = 0; TypeIndex < static_cast<int32>(EImmortalEquipmentAffixType::MAX); ++TypeIndex)
	{
		FImmortalEquipmentAffix Affix;
		Affix.Type = static_cast<EImmortalEquipmentAffixType>(TypeIndex);
		Affix.Value = 0.1f;
		TestFalse(*FString::Printf(TEXT("Affix %d has display text"), TypeIndex),
			UImmortalEquipmentLibrary::GetAffixText(Affix).IsEmpty());
	}

	FImmortalEquipmentItem Corrupted = UImmortalEquipmentLibrary::GenerateCraftedEquipment(
		10, EImmortalEquipmentSlot::Weapon, EImmortalEquipmentQuality::Common);
	Corrupted.Slot = static_cast<EImmortalEquipmentSlot>(255);
	Corrupted.Quality = static_cast<EImmortalEquipmentQuality>(255);
	Corrupted.SetId = TEXT("UnknownSet");
	Corrupted.Affixes.Reset();
	for (int32 Index = 0; Index < 8; ++Index)
	{
		Corrupted.Affixes.Add({Index < 2 ? EImmortalEquipmentAffixType::Attack
			: static_cast<EImmortalEquipmentAffixType>(Index), Index == 7 ? NAN : 0.1f});
	}
	UImmortalEquipmentLibrary::NormalizeForgingState(Corrupted);
	TestEqual(TEXT("Invalid slot normalizes safely"), Corrupted.Slot, EImmortalEquipmentSlot::Weapon);
	TestEqual(TEXT("Invalid quality normalizes safely"), Corrupted.Quality, EImmortalEquipmentQuality::Divine);
	TestTrue(TEXT("Unknown set is removed"), Corrupted.SetId.IsNone());
	TestTrue(TEXT("Affix normalization enforces six-entry cap"), Corrupted.Affixes.Num() <= 6);
	TSet<EImmortalEquipmentAffixType> NormalizedTypes;
	for (const FImmortalEquipmentAffix& Affix : Corrupted.Affixes)
	{
		TestTrue(TEXT("Normalized affix remains finite and positive"), FMath::IsFinite(Affix.Value) && Affix.Value > 0.0f);
		TestFalse(TEXT("Normalized affix types are unique"), NormalizedTypes.Contains(Affix.Type));
		NormalizedTypes.Add(Affix.Type);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmortalEquipmentSetTest,
	"ImmortalPath.Equipment.FourSetsAndCumulativeThresholds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImmortalEquipmentSetTest::RunTest(const FString& Parameters)
{
	const TArray<FName> SetIds = UImmortalEquipmentLibrary::GetKnownSetIds();
	TestEqual(TEXT("Four built-in equipment sets exist"), SetIds.Num(), 4);
	const EImmortalEquipmentSlot Slots[] =
	{
		EImmortalEquipmentSlot::Weapon, EImmortalEquipmentSlot::Head, EImmortalEquipmentSlot::Chest,
		EImmortalEquipmentSlot::Bracers, EImmortalEquipmentSlot::Belt, EImmortalEquipmentSlot::Boots
	};
	for (const FName SetId : SetIds)
	{
		FImmortalEquipmentSetDefinition Definition;
		TestTrue(*FString::Printf(TEXT("Set definition resolves: %s"), *SetId.ToString()),
			UImmortalEquipmentLibrary::GetSetDefinition(SetId, Definition));
		TestEqual(TEXT("Every set has 2/4/6 tiers"), Definition.Tiers.Num(), 3);
		TestEqual(TEXT("First tier requires two"), Definition.Tiers[0].RequiredPieces, 2);
		TestEqual(TEXT("Second tier requires four"), Definition.Tiers[1].RequiredPieces, 4);
		TestEqual(TEXT("Third tier requires six"), Definition.Tiers[2].RequiredPieces, 6);

		TArray<FImmortalEquipmentItem> Equipped;
		for (int32 Piece = 0; Piece < 6; ++Piece)
		{
			Equipped.Add(UImmortalEquipmentLibrary::GenerateCraftedEquipment(
				30, Slots[Piece], EImmortalEquipmentQuality::Rare,
				EImmortalEquipmentDiscipline::Universal, SetId));
			const FImmortalEquipmentSetBonuses Bonuses = UImmortalEquipmentLibrary::CalculateSetBonuses(Equipped);
			TestEqual(TEXT("Set piece count is exact"), Bonuses.PieceCounts.FindRef(SetId), Piece + 1);
			auto TierScore = [](const FImmortalEquipmentSetTier& Tier)
			{
				return Tier.AttackMultiplierBonus + Tier.DefenseMultiplierBonus + Tier.HealthMultiplierBonus
					+ Tier.AttackSpeedBonus + Tier.CriticalChanceBonus + Tier.CriticalDamageBonus
					+ Tier.ThunderDamageBonus + Tier.CultivationGainBonus + Tier.BossDamageBonus
					+ Tier.FinalDamageBonus + Tier.DamageReductionBonus;
			};
			const float ActualScore = Bonuses.AttackMultiplierBonus + Bonuses.DefenseMultiplierBonus
				+ Bonuses.HealthMultiplierBonus + Bonuses.AttackSpeedBonus + Bonuses.CriticalChanceBonus
				+ Bonuses.CriticalDamageBonus + Bonuses.ThunderDamageBonus + Bonuses.CultivationGainBonus
				+ Bonuses.BossDamageBonus + Bonuses.FinalDamageBonus + Bonuses.DamageReductionBonus;
			float ExpectedScore = 0.0f;
			for (const FImmortalEquipmentSetTier& Tier : Definition.Tiers)
			{
				if (Piece + 1 >= Tier.RequiredPieces) ExpectedScore += TierScore(Tier);
			}
			TestTrue(TEXT("Only reached 2/4/6 tiers accumulate"), FMath::IsNearlyEqual(ActualScore, ExpectedScore));
			const bool bHasAnyBonus = Bonuses.AttackMultiplierBonus > 0.0f || Bonuses.DefenseMultiplierBonus > 0.0f
				|| Bonuses.HealthMultiplierBonus > 0.0f || Bonuses.AttackSpeedBonus > 0.0f
				|| Bonuses.CriticalChanceBonus > 0.0f || Bonuses.CriticalDamageBonus > 0.0f
				|| Bonuses.ThunderDamageBonus > 0.0f || Bonuses.CultivationGainBonus > 0.0f
				|| Bonuses.BossDamageBonus > 0.0f || Bonuses.FinalDamageBonus > 0.0f
				|| Bonuses.DamageReductionBonus > 0.0f;
			TestEqual(TEXT("No tier activates before two pieces"), bHasAnyBonus, Piece >= 1);
		}
		const FImmortalEquipmentSetBonuses SixPiece = UImmortalEquipmentLibrary::CalculateSetBonuses(Equipped);
		TArray<FImmortalEquipmentItem> WithDuplicate = Equipped;
		WithDuplicate.Add(Equipped[0]);
		const FImmortalEquipmentSetBonuses DuplicateIgnored = UImmortalEquipmentLibrary::CalculateSetBonuses(WithDuplicate);
		TestEqual(TEXT("Duplicate item ID never increases a set count"), DuplicateIgnored.PieceCounts.FindRef(SetId), 6);
		TestEqual(TEXT("Duplicate item ID never duplicates final-damage tiers"),
			DuplicateIgnored.FinalDamageBonus, SixPiece.FinalDamageBonus);
	}

	// Regression: automatic equipment comparison must not break Qingyun's six-piece
	// final-damage tier for a trivial +6 attack replacement.
	TArray<FImmortalEquipmentItem> QingyunSixPiece;
	for (int32 Piece = 0; Piece < 6; ++Piece)
	{
		FImmortalEquipmentItem Item;
		Item.ItemId = FGuid::NewGuid();
		Item.DisplayName = TEXT("QingyunRegressionPiece");
		Item.Slot = Slots[Piece];
		Item.Quality = EImmortalEquipmentQuality::Rare;
		Item.SetId = TEXT("QingyunSet");
		Item.AttackBonus = Piece == 5 ? 200.0f : 160.0f;
		QingyunSixPiece.Add(Item);
	}
	TArray<FImmortalEquipmentItem> TinyRawUpgrade = QingyunSixPiece;
	TinyRawUpgrade.Last().ItemId = FGuid::NewGuid();
	TinyRawUpgrade.Last().SetId = NAME_None;
	TinyRawUpgrade.Last().AttackBonus += 6.0f;
	TestTrue(TEXT("Six-piece final damage outranks a tiny raw-attack replacement"),
		UImmortalEquipmentLibrary::CalculateLoadoutPower(QingyunSixPiece)
			> UImmortalEquipmentLibrary::CalculateLoadoutPower(TinyRawUpgrade));
	for (FImmortalEquipmentItem& Item : QingyunSixPiece) Item.AttackBonus = 0.0f;
	TinyRawUpgrade = QingyunSixPiece;
	TinyRawUpgrade.Last().ItemId = FGuid::NewGuid();
	TinyRawUpgrade.Last().SetId = NAME_None;
	TinyRawUpgrade.Last().AttackBonus = 6.0f;
	TestTrue(TEXT("Six-piece final damage also protects character base attack"),
		UImmortalEquipmentLibrary::CalculateLoadoutPowerWithBaseStats(QingyunSixPiece, 1000.0f, 0.0f, 0.0f)
			> UImmortalEquipmentLibrary::CalculateLoadoutPowerWithBaseStats(TinyRawUpgrade, 1000.0f, 0.0f, 0.0f));

	const int32 SetThresholds[] = {2, 4, 6};
	for (const FName SetId : SetIds)
	{
		for (const int32 Threshold : SetThresholds)
		{
			TArray<FImmortalEquipmentItem> FullSetThreshold;
			for (int32 Piece = 0; Piece < Threshold; ++Piece)
			{
				FImmortalEquipmentItem Item;
				Item.ItemId = FGuid::NewGuid();
				Item.DisplayName = TEXT("SetThresholdRegressionPiece");
				Item.Slot = Slots[Piece];
				Item.Quality = EImmortalEquipmentQuality::Rare;
				Item.SetId = SetId;
				FullSetThreshold.Add(Item);
			}
			TArray<FImmortalEquipmentItem> BrokenThreshold = FullSetThreshold;
			BrokenThreshold.Last().ItemId = FGuid::NewGuid();
			BrokenThreshold.Last().SetId = NAME_None;
			BrokenThreshold.Last().AttackBonus = 4.0f;
			TestTrue(*FString::Printf(TEXT("%s %d-piece threshold beats a tiny off-set upgrade"),
				*SetId.ToString(), Threshold),
				UImmortalEquipmentLibrary::CalculateLoadoutPowerWithBaseStats(
					FullSetThreshold, 1000.0f, 100.0f, 1000.0f)
				> UImmortalEquipmentLibrary::CalculateLoadoutPowerWithBaseStats(
					BrokenThreshold, 1000.0f, 100.0f, 1000.0f));
		}
	}

	UImmortalCultivationComponent* Cultivation = NewObject<UImmortalCultivationComponent>();
	TestNotNull(TEXT("Cultivation component can be created for multiplier validation"), Cultivation);
	if (Cultivation)
	{
		const float BaseRate = Cultivation->GetCultivationPerSecondWithoutAlchemyBoost();
		Cultivation->SetEquipmentRateMultiplier(1.5f);
		TestEqual(TEXT("Equipment multiplier affects independent passive cultivation"),
			Cultivation->GetCultivationPerSecondWithoutAlchemyBoost(), BaseRate * 1.5f);
		Cultivation->SetAlchemyRateMultiplier(2.0f);
		TestEqual(TEXT("Temporary alchemy stacks only in online cultivation"),
			Cultivation->GetCultivationPerSecond(), BaseRate * 3.0f);
		TestEqual(TEXT("Offline cultivation excludes alchemy but keeps equipment"),
			Cultivation->GetCultivationPerSecondWithoutAlchemyBoost(), BaseRate * 1.5f);
	}
	return true;
}

#endif
