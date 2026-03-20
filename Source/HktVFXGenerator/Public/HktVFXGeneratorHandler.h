// Copyright Hkt Studios, Inc. All Rights Reserved.
// VFX Generator Handler — "VFX.*" Tag miss 처리

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HktGeneratorRouter.h"
#include "HktVFXNiagaraConfig.h"
#include "HktVFXGeneratorHandler.generated.h"

class UHktVFXGeneratorSubsystem;

/**
 * UHktVFXGeneratorHandler
 *
 * "VFX.*" 태그에 대한 IHktGeneratorHandler 구현.
 * VFXAutoResolver로 태그 파싱 → VFXGeneratorSubsystem으로 Niagara 빌드.
 * 빌드 후 UHktVFXVisualDataAsset을 생성하여 TagDataAsset 시스템과 연결.
 */
UCLASS()
class HKTVFXGENERATOR_API UHktVFXGeneratorHandler : public UObject, public IHktGeneratorHandler
{
	GENERATED_BODY()

public:
	virtual bool CanHandle(const FGameplayTag& Tag) const override;
	virtual FSoftObjectPath HandleTagMiss(const FGameplayTag& Tag) override;

private:
	/** Intent에서 기본 Config를 생성 */
	FHktVFXNiagaraConfig BuildDefaultConfig(const struct FHktVFXIntent& Intent, const FString& SystemName);

	/** NiagaraSystem을 참조하는 UHktVFXVisualDataAsset 생성 및 저장 */
	FSoftObjectPath CreateVFXDataAsset(const FGameplayTag& Tag, const FSoftObjectPath& NiagaraSystemPath, const FString& OutputDir);
};
