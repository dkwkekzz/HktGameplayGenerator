// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "IHktMeshGeneratorModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FHktMeshGeneratorModule"

DEFINE_LOG_CATEGORY_STATIC(LogHktMeshGeneratorModule, Log, All);

class FHktMeshGeneratorModule : public IHktMeshGeneratorModule
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogHktMeshGeneratorModule, Log, TEXT("HktMeshGenerator Module Started"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogHktMeshGeneratorModule, Log, TEXT("HktMeshGenerator Module Shutdown"));
	}
};

IMPLEMENT_MODULE(FHktMeshGeneratorModule, HktMeshGenerator)

#undef LOCTEXT_NAMESPACE
