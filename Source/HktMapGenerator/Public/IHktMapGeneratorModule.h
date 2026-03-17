// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/**
 * HktMapGenerator Module Interface
 *
 * Map Generator 레이어 - HktMap JSON을 기반으로 Landscape, Spawner, Story 연결을 관리.
 * HktMap은 UMap이 아닌 JSON 기반 동적 로드/언로드 가능한 맵 정의.
 */
class HKTMAPGENERATOR_API IHktMapGeneratorModule : public IModuleInterface
{
public:
	static inline IHktMapGeneratorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IHktMapGeneratorModule>("HktMapGenerator");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("HktMapGenerator");
	}
};
