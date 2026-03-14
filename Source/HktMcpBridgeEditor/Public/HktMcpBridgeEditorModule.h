#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogHktMcpEditor, Log, All);

class FHktMcpBridgeEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Get the module instance */
	static FHktMcpBridgeEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FHktMcpBridgeEditorModule>("HktMcpBridgeEditor");
	}

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("HktMcpBridgeEditor");
	}

private:
	void RegisterConsoleCommands();
	void UnregisterConsoleCommands();

	TArray<IConsoleObject*> ConsoleCommands;
};

