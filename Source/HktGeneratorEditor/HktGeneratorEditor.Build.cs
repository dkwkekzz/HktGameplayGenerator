using UnrealBuildTool;

public class HktGeneratorEditor : ModuleRules
{
	public HktGeneratorEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Json",
				"JsonUtilities"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"InputCore",
				"UnrealEd",
				"EditorSubsystem",
				"LevelEditor",
				"WorkspaceMenuStructure",
				"DesktopPlatform",
				"DeveloperSettings",
				"HktMcpBridge",
				"HktMcpBridgeEditor",
				"HktTextureGenerator",
				"Settings"
			}
		);
	}
}
