// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "IHktTextureGeneratorModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FHktTextureGeneratorModule"

DEFINE_LOG_CATEGORY_STATIC(LogHktTextureGeneratorModule, Log, All);

class FHktTextureGeneratorModule : public IHktTextureGeneratorModule
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogHktTextureGeneratorModule, Log, TEXT("HktTextureGenerator Module Started"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogHktTextureGeneratorModule, Log, TEXT("HktTextureGenerator Module Shutdown"));
	}
};

IMPLEMENT_MODULE(FHktTextureGeneratorModule, HktTextureGenerator)

#undef LOCTEXT_NAMESPACE
