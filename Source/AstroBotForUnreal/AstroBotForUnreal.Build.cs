// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AstroBotForUnreal : ModuleRules
{
	public AstroBotForUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"AstroBotForUnreal",
			"AstroBotForUnreal/Variant_Platforming",
			"AstroBotForUnreal/Variant_Platforming/Animation",
			"AstroBotForUnreal/Variant_Combat",
			"AstroBotForUnreal/Variant_Combat/AI",
			"AstroBotForUnreal/Variant_Combat/Animation",
			"AstroBotForUnreal/Variant_Combat/Gameplay",
			"AstroBotForUnreal/Variant_Combat/Interfaces",
			"AstroBotForUnreal/Variant_Combat/UI",
			"AstroBotForUnreal/Variant_SideScrolling",
			"AstroBotForUnreal/Variant_SideScrolling/AI",
			"AstroBotForUnreal/Variant_SideScrolling/Gameplay",
			"AstroBotForUnreal/Variant_SideScrolling/Interfaces",
			"AstroBotForUnreal/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
