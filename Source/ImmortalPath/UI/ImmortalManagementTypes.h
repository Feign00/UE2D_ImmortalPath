// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImmortalManagementTypes.generated.h"

/** Every page hosted by the unified cultivation/management interface. */
UENUM(BlueprintType)
enum class EImmortalManagementFeature : uint8
{
	Home UMETA(DisplayName = "仙府主页"),
	Cultivation UMETA(DisplayName = "修炼"),
	Inventory UMETA(DisplayName = "装备"),
	Alchemy UMETA(DisplayName = "炼丹"),
	Crafting UMETA(DisplayName = "炼器"),
	Artifact UMETA(DisplayName = "法宝"),
	Technique UMETA(DisplayName = "功法"),
	CharacterBuild UMETA(DisplayName = "灵根与流派"),
	Shop UMETA(DisplayName = "百宝阁"),
	Map UMETA(DisplayName = "地图"),
	Quest UMETA(DisplayName = "任务"),
	Cave UMETA(DisplayName = "洞府"),
	Farming UMETA(DisplayName = "灵田"),
	Sect UMETA(DisplayName = "宗门"),
	WorldBoss UMETA(DisplayName = "世界妖王"),
	EndlessDungeon UMETA(DisplayName = "无尽秘境"),
	Pet UMETA(DisplayName = "灵宠"),
	Settings UMETA(DisplayName = "设置")
};

/**
 * Full-width visual locations used by the management interface.
 *
 * A scene is the complete TBH-strip background that owns several diegetic
 * feature hotspots. Individual feature pages open inside the same scene;
 * they are not separate viewport windows.
 */
UENUM(BlueprintType)
enum class EImmortalManagementScene : uint8
{
	SectSanctuary UMETA(DisplayName = "宗门山门"),
	MarketTown UMETA(DisplayName = "仙城坊市"),
	CaveEstate UMETA(DisplayName = "洞府"),
	AdventureHall UMETA(DisplayName = "历练台")
};
