#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogHktMcp, Log, All);

class FHktMcpBridgeModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Get the module instance */
	static FHktMcpBridgeModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FHktMcpBridgeModule>("HktMcpBridge");
	}

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("HktMcpBridge");
	}
};

