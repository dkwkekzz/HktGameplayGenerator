// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "IHktItemGeneratorModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FHktItemGeneratorModule"

DEFINE_LOG_CATEGORY_STATIC(LogHktItemGeneratorModule, Log, All);

class FHktItemGeneratorModule : public IHktItemGeneratorModule
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogHktItemGeneratorModule, Log, TEXT("HktItemGenerator Module Started"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogHktItemGeneratorModule, Log, TEXT("HktItemGenerator Module Shutdown"));
	}
};

IMPLEMENT_MODULE(FHktItemGeneratorModule, HktItemGenerator)

#undef LOCTEXT_NAMESPACE
