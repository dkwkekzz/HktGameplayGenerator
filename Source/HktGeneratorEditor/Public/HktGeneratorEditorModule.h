// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogHktGenEditor, Log, All);

class FHktGeneratorEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FHktGeneratorEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FHktGeneratorEditorModule>("HktGeneratorEditor");
	}

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("HktGeneratorEditor");
	}

	/** Generator Prompt tab name */
	static const FName GeneratorPromptTabName;

private:
	void RegisterGeneratorPromptTab();
	void UnregisterGeneratorPromptTab();
	TSharedRef<class SDockTab> SpawnGeneratorPromptTab(const class FSpawnTabArgs& Args);

	void RegisterConsoleCommands();
	void UnregisterConsoleCommands();

	TArray<IConsoleObject*> ConsoleCommands;
};
