// Copyright Hkt Studios, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HktTextureGenerator : ModuleRules
{
	public HktTextureGenerator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
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
				"ImageWrapper",
				"RenderCore",
				"HTTP",
			}
		);
	}
}
