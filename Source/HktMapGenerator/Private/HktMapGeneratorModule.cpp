// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "IHktMapGeneratorModule.h"

class FHktMapGeneratorModule : public IHktMapGeneratorModule
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogTemp, Log, TEXT("[HktMapGenerator] Module loaded"));
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FHktMapGeneratorModule, HktMapGenerator)
