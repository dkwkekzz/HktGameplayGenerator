// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/**
 * HktGeneratorCore Module Interface
 *
 * Generator 공유 기반 모듈:
 * - GeneratorRouter: Tag miss → 적절한 Generator로 라우팅
 * - VFX/Mesh/Anim/Item Generator 공통 인터페이스
 */
class HKTGENERATORCORE_API IHktGeneratorCoreModule : public IModuleInterface
{
public:
	static inline IHktGeneratorCoreModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IHktGeneratorCoreModule>("HktGeneratorCore");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("HktGeneratorCore");
	}
};
