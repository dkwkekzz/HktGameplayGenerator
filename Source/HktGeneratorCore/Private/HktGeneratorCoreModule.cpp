// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "IHktGeneratorCoreModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FHktGeneratorCoreModule"

DEFINE_LOG_CATEGORY_STATIC(LogHktGeneratorCoreModule, Log, All);

class FHktGeneratorCoreModule : public IHktGeneratorCoreModule
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogHktGeneratorCoreModule, Log, TEXT("HktGeneratorCore Module Started"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogHktGeneratorCoreModule, Log, TEXT("HktGeneratorCore Module Shutdown"));
	}
};

IMPLEMENT_MODULE(FHktGeneratorCoreModule, HktGeneratorCore)

#undef LOCTEXT_NAMESPACE
