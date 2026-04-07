using UnrealBuildTool;

public class HktMcpBridgeEditor : ModuleRules
{
	public HktMcpBridgeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"Json",
				"JsonUtilities",
				"HktMcpBridge"
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
				"AssetTools",
				"AssetRegistry",
				"ContentBrowser",
				"EditorScriptingUtilities",
				"PythonScriptPlugin"
			}
		);
	}
}

