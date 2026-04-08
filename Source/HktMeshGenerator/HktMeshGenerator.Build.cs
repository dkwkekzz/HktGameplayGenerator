// Copyright Hkt Studios, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class HktMeshGenerator : ModuleRules
{
	public HktMeshGenerator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(PluginDirectory, "..", "HktGameplay", "Source", "HktAsset", "Public"),
				Path.Combine(PluginDirectory, "..", "HktGameplay", "Source", "HktPresentation", "Public"),
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
				"HktPresentation",
				"HktGeneratorCore",
				"HktTextureGenerator",
				"MeshDescription",
				"StaticMeshDescription",
				"Json",
				"JsonUtilities",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"EditorSubsystem",
				"EditorScriptingUtilities",
				"DeveloperSettings",
				"AssetRegistry",
				"AssetTools",
			}
		);
	}
}
