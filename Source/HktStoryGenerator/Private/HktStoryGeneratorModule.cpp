// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "IHktStoryGeneratorModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FHktStoryGeneratorModule"

DEFINE_LOG_CATEGORY_STATIC(LogHktStoryGeneratorModule, Log, All);

class FHktStoryGeneratorModule : public IHktStoryGeneratorModule
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogHktStoryGeneratorModule, Log, TEXT("HktStoryGenerator Module Started"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogHktStoryGeneratorModule, Log, TEXT("HktStoryGenerator Module Shutdown"));
	}
};

IMPLEMENT_MODULE(FHktStoryGeneratorModule, HktStoryGenerator)

#undef LOCTEXT_NAMESPACE
