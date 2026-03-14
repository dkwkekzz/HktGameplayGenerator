// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "IHktAnimGeneratorModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FHktAnimGeneratorModule"

DEFINE_LOG_CATEGORY_STATIC(LogHktAnimGeneratorModule, Log, All);

class FHktAnimGeneratorModule : public IHktAnimGeneratorModule
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogHktAnimGeneratorModule, Log, TEXT("HktAnimGenerator Module Started"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogHktAnimGeneratorModule, Log, TEXT("HktAnimGenerator Module Shutdown"));
	}
};

IMPLEMENT_MODULE(FHktAnimGeneratorModule, HktAnimGenerator)

#undef LOCTEXT_NAMESPACE
