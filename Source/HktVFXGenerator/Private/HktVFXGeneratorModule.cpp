// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "IHktVFXGeneratorModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FHktVFXGeneratorModule"

DEFINE_LOG_CATEGORY_STATIC(LogHktVFXGeneratorModule, Log, All);

class FHktVFXGeneratorModule : public IHktVFXGeneratorModule
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogHktVFXGeneratorModule, Log, TEXT("HktVFXGenerator Module Started"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogHktVFXGeneratorModule, Log, TEXT("HktVFXGenerator Module Shutdown"));
	}
};

IMPLEMENT_MODULE(FHktVFXGeneratorModule, HktVFXGenerator)

#undef LOCTEXT_NAMESPACE
