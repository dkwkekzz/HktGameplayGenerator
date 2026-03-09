// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktVFXNiagaraConfig.h"

class UNiagaraSystem;
class UNiagaraEmitter;
class UMaterialInterface;

/**
 * FHktVFXNiagaraConfig로 UNiagaraSystem .uasset을 생성.
 *
 * RapidIterationParameters 이름 형식:
 *   Constants.{HandleName}.{ModuleName}.{ParamName}
 *
 * 파라미터는 스크립트 종류에 따라 다른 곳에 기록:
 *   - Particle Spawn/Update Script → InitializeParticle, ScaleColor 등
 *   - Emitter Update Script → SpawnBurst_Instantaneous, EmitterState 등
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

	void SetupSpawnModule(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXEmitterSpawnConfig& Config);
	void SetupInitializeModule(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXEmitterInitConfig& Config);
	void SetupUpdateModules(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXEmitterUpdateConfig& Config);
	void SetupRenderer(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXEmitterRenderConfig& Config);
	void SetupSystemProperties(UNiagaraSystem* System, const FHktVFXNiagaraConfig& Config);

	UMaterialInterface* GetVFXMaterial(const FString& BlendMode);
	void ApplyMaterialOverride(UNiagaraSystem* System, int32 EmitterIndex, const FString& MaterialPath);

	// Particle-level 파라미터 (SpawnScript + UpdateScript)
	void SetParticleParamFloat(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, float Value);
	void SetParticleParamVec3(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, FVector Value);
	void SetParticleParamVec4(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, FVector4 Value);
	void SetParticleParamColor(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, FLinearColor Value);

	// Emitter-level 파라미터 (EmitterUpdateScript)
	void SetEmitterParamFloat(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, float Value);
	void SetEmitterParamInt(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, int32 Value);
};
