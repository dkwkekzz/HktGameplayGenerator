// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class HKTMESHGENERATOR_API IHktMeshGeneratorModule : public IModuleInterface
{
public:
	static inline IHktMeshGeneratorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IHktMeshGeneratorModule>("HktMeshGenerator");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("HktMeshGenerator");
	}
};
