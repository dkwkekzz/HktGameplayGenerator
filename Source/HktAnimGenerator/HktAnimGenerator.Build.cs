// Copyright Hkt Studios, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class HktAnimGenerator : ModuleRules
{
	public HktAnimGenerator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(PluginDirectory, "..", "HktGameplay", "Source", "HktAsset", "Public"),
				Path.Combine(PluginDirectory, "Source", "HktGeneratorCore", "Public"),
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"HktAsset",
				"HktGeneratorCore",
				"Json",
				"JsonUtilities",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"EditorSubsystem",
				"DeveloperSettings",
				"AssetRegistry",
				"AssetTools",
				// Animation Blueprint / State Machine / AnimGraph
				"AnimGraph",
				"AnimGraphRuntime",
				"BlueprintGraph",
				"Kismet",
			}
		);
	}
}
