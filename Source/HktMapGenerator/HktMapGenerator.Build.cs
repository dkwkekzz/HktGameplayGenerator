// Copyright Hkt Studios, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class HktMapGenerator : ModuleRules
{
	public HktMapGenerator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
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
				"HktGeneratorCore",
				"Json",
				"JsonUtilities",
				"Landscape",
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
				"LandscapeEditor",
				"HktStoryGenerator",
			}
		);
	}
}
