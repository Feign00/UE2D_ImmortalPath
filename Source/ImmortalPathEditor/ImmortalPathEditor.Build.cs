// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ImmortalPathEditor : ModuleRules
{
	public ImmortalPathEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"AssetTools",
			"AssetRegistry",
			"Paper2D",
			"Json",
			"JsonUtilities",
			"Projects"
		});
	}
}
