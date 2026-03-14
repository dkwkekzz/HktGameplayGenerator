// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class HKTANIMGENERATOR_API IHktAnimGeneratorModule : public IModuleInterface
{
public:
	static inline IHktAnimGeneratorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IHktAnimGeneratorModule>("HktAnimGenerator");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("HktAnimGenerator");
	}
};
