// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ImmortalPath : ModuleRules
{
	public ImmortalPath(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Several independent gameplay systems intentionally keep file-local
		// helpers in anonymous namespaces. Compiling this module without unity
		// keeps those translation units isolated and makes clean/CI builds
		// deterministic regardless of the adaptive working set.
		bUseUnity = false;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Paper2D", "UMG" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "ApplicationCore" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
