// Copyright Hkt Studios, Inc. All Rights Reserved.
// Config → UNiagaraSystem 에셋 빌드 (Phase 0: 텍스처 없이 기본 머티리얼 사용)

#pragma once

#include "CoreMinimal.h"
#include "HktVFXNiagaraConfig.h"

class UNiagaraSystem;
class UNiagaraEmitter;
class UMaterialInterface;
class UHktVFXGeneratorSettings;

/**
 * FHktVFXNiagaraConfig로 UNiagaraSystem .uasset을 생성.
 * 템플릿 경로 등 설정은 UHktVFXGeneratorSettings (Project Settings) 에서 읽음.
 */
class HKTVFXGENERATOR_API FHktVFXNiagaraBuilder
{
public:
	/**
	 * Config로 Niagara 시스템 에셋 빌드 및 디스크 저장.
	 * @param Config 에미터 설정
	 * @param OutputDirectory Content 기준 경로 (예: "/Game/GeneratedVFX")
	 * @return 생성된 UNiagaraSystem, 실패 시 nullptr
	 */
	UNiagaraSystem* BuildNiagaraSystem(
		const FHktVFXNiagaraConfig& Config,
		const FString& OutputDirectory);

private:
	void ConfigureEmitter(UNiagaraSystem* System, const FHktVFXEmitterConfig& Config);

	UNiagaraEmitter* LoadEmitterTemplate(const FString& RendererType);

	void SetupSpawnModule(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXEmitterSpawnConfig& Config);
	void SetupInitializeModule(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXEmitterInitConfig& Config);
	void SetupUpdateModules(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXEmitterUpdateConfig& Config);
	void SetupRenderer(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXEmitterRenderConfig& Config);

	UMaterialInterface* GetVFXMaterial(const FString& BlendMode);

	void SetNiagaraVariableFloat(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, float Value);
	void SetNiagaraVariableVec3(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, FVector Value);
	void SetNiagaraVariableColor(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, FLinearColor Value);
};
