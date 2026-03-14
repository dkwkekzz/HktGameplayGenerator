// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class HKTSTORYGENERATOR_API IHktStoryGeneratorModule : public IModuleInterface
{
public:
	static inline IHktStoryGeneratorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IHktStoryGeneratorModule>("HktStoryGenerator");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("HktStoryGenerator");
	}
};
