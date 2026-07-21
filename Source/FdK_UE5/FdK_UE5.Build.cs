// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FdK_UE5 : ModuleRules
{
	public FdK_UE5(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"FdK_UE5",
			"FdK_UE5/Scheduler",
			"FdK_UE5/Variant_Strategy",
			"FdK_UE5/Variant_Strategy/UI",
			"FdK_UE5/Variant_TwinStick",
			"FdK_UE5/Variant_TwinStick/AI",
			"FdK_UE5/Variant_TwinStick/Gameplay",
			"FdK_UE5/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
