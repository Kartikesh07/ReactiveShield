// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ReactiveShield : ModuleRules
{
	public ReactiveShield(ReadOnlyTargetRules Target) : base(Target)
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

		PrivateDependencyModuleNames.AddRange(new string[] { "Niagara" });

		PublicIncludePaths.AddRange(new string[] {
			"ReactiveShield",
			"ReactiveShield/Variant_Platforming",
			"ReactiveShield/Variant_Platforming/Animation",
			"ReactiveShield/Variant_Combat",
			"ReactiveShield/Variant_Combat/AI",
			"ReactiveShield/Variant_Combat/Animation",
			"ReactiveShield/Variant_Combat/Gameplay",
			"ReactiveShield/Variant_Combat/Interfaces",
			"ReactiveShield/Variant_Combat/UI",
			"ReactiveShield/Variant_SideScrolling",
			"ReactiveShield/Variant_SideScrolling/AI",
			"ReactiveShield/Variant_SideScrolling/Gameplay",
			"ReactiveShield/Variant_SideScrolling/Interfaces",
			"ReactiveShield/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
