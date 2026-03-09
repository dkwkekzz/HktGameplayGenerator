// Copyright Hkt Studios, Inc. All Rights Reserved.
// Config -> UNiagaraSystem 에셋 빌드

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
	UNiagaraSystem* BuildNiagaraSystem(
		const FHktVFXNiagaraConfig& Config,
		const FString& OutputDirectory);

private:
	void ConfigureEmitter(UNiagaraSystem* System, const FHktVFXEmitterConfig& Config);
	UNiagaraEmitter* LoadEmitterTemplate(const FString& RendererType);

	// 모듈별 설정
	void SetupSpawnModule(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXEmitterSpawnConfig& Config);
	void SetupInitializeModule(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXEmitterInitConfig& Config);
	void SetupUpdateModules(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXEmitterUpdateConfig& Config);
	void SetupRenderer(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXEmitterRenderConfig& Config);

	// 시스템 레벨 설정
	void SetupSystemProperties(UNiagaraSystem* System, const FHktVFXNiagaraConfig& Config);

	UMaterialInterface* GetVFXMaterial(const FString& BlendMode);

	// 파라미터 유틸리티
	void SetNiagaraVariableFloat(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, float Value);
	void SetNiagaraVariableVec3(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, FVector Value);
	void SetNiagaraVariableColor(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, FLinearColor Value);
};
