// Copyright Hkt Studios, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class HktGeneratorCore : ModuleRules
{
	public HktGeneratorCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// HktAsset, HktVFX Public 헤더 접근
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(PluginDirectory, "..", "HktGameplay", "Source", "HktAsset", "Public"),
				Path.Combine(PluginDirectory, "..", "HktGameplay", "Source", "HktVFX", "Public"),
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
				"HktVFX",
				"HktTextureGenerator",
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
			}
		);
	}
}
