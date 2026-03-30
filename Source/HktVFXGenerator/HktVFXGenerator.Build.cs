// Copyright Hkt Studios, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class HktVFXGenerator : ModuleRules
{
	public HktVFXGenerator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// HktVFX, HktPresentation, HktGeneratorCore Public 헤더 접근
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(PluginDirectory, "..", "HktGameplay", "Source", "HktVFX", "Public"),
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
				"HktVFX",
				"HktGeneratorCore",
				"HktTextureGenerator",
				"Niagara",
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
				"NiagaraEditor",
				"AssetRegistry",
				"AssetTools",
				"RenderCore",
				"Projects",
				"Slate",
				"SlateCore",
				"InputCore",
			}
		);
	}
}
