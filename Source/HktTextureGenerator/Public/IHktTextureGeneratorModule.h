// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/**
 * HktTextureGenerator Module Interface
 *
 * 공유 텍스처 생성 모듈 — VFX, Item, Mesh 등 다른 Generator에서 참조.
 * AI 이미지 생성 → UTexture2D 임포트 → Usage별 자동 설정까지 수행.
 */
class HKTTEXTUREGENERATOR_API IHktTextureGeneratorModule : public IModuleInterface
{
public:
	static inline IHktTextureGeneratorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IHktTextureGeneratorModule>("HktTextureGenerator");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("HktTextureGenerator");
	}
};
