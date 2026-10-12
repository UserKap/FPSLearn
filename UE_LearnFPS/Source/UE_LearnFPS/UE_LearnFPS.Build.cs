// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UE_LearnFPS : ModuleRules
{
	public UE_LearnFPS(ReadOnlyTargetRules Target) : base(Target)
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
			"UE_LearnFPS",
			"UE_LearnFPS/CustomFPS/Character",
			"UE_LearnFPS/CustomFPS/Input",
			"UE_LearnFPS/CustomFPS/Locomotion",
			"UE_LearnFPS/CustomFPS/View",
			"UE_LearnFPS/CustomFPS/Weapon",
			"UE_LearnFPS/CustomFPS/Animation",
			"UE_LearnFPS/Variant_Horror",
			"UE_LearnFPS/Variant_Horror/UI",
			"UE_LearnFPS/Variant_Shooter",
			"UE_LearnFPS/Variant_Shooter/AI",
			"UE_LearnFPS/Variant_Shooter/UI",
			"UE_LearnFPS/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
