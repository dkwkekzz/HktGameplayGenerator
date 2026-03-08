// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/**
 * HktVFXGenerator Module Interface
 * 
 * VFX Generator 레이어 - 게임 로직 없음
 * HktRuntime의 IHktModelProvider를 통해 데이터를 읽고 VFX Generator만 수행
 */
class HKTVFXGENERATOR_API IHktVFXGeneratorModule : public IModuleInterface
{
public:
	static inline IHktVFXGeneratorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IHktVFXGeneratorModule>("HktVFXGenerator");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("HktVFXGenerator");
	}
};
