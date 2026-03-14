// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class HKTITEMGENERATOR_API IHktItemGeneratorModule : public IModuleInterface
{
public:
	static inline IHktItemGeneratorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IHktItemGeneratorModule>("HktItemGenerator");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("HktItemGenerator");
	}
};
