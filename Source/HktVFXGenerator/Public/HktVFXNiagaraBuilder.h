// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktVFXNiagaraConfig.h"
#include "NiagaraCommon.h"

class UNiagaraSystem;
class UNiagaraEmitter;
class UMaterialInterface;
struct FHktVFXDataInterfaceBinding;
struct FHktVFXCollisionConfig;
struct FHktVFXEventSpawnConfig;
struct FHktVFXSpawnPerUnitConfig;

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

	// Phase 4: 개별 파라미터 setter (부분 오버라이드용)
	void SetParticleParamFloat(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, float Value);
	void SetParticleParamVec3(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, FVector Value);
	void SetParticleParamColor(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, FLinearColor Value);
	void SetEmitterParamFloat(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, float Value);
	void SetEmitterParamInt(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, int32 Value);

private:
	void ConfigureEmitter(UNiagaraSystem* System, const FHktVFXEmitterConfig& Config);
	UNiagaraEmitter* LoadEmitterTemplate(const FString& RendererType);

	void SetupSpawnModule(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXEmitterSpawnConfig& Config);
	void SetupInitializeModule(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXEmitterInitConfig& Config);
	void SetupUpdateModules(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXEmitterUpdateConfig& Config);
	void SetupRenderer(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXEmitterRenderConfig& Config);
	void SetupShapeLocation(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXShapeLocationConfig& Config);
	void SetupCollision(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXCollisionConfig& Config);
	void SetupEventSpawn(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXEventSpawnConfig& Config, const FString& EmitterName);
	void SetupSpawnPerUnit(UNiagaraSystem* System, int32 EmitterIndex, const FHktVFXSpawnPerUnitConfig& Config);
	void SetupGPUSim(UNiagaraSystem* System, int32 EmitterIndex);
	void SetupSystemProperties(UNiagaraSystem* System, const FHktVFXNiagaraConfig& Config);

	UMaterialInterface* GetVFXMaterial(const FString& BlendMode);
	void ApplyMaterialOverride(UNiagaraSystem* System, int32 EmitterIndex, const FString& MaterialPath);

	// 동적 모듈 주입 — 템플릿에 없는 모듈을 Config 요구에 따라 그래프에 추가
	bool AddModuleToEmitter(UNiagaraSystem* System, int32 EmitterIndex,
		ENiagaraScriptUsage ScriptUsage, const FString& ModuleScriptPath);
	void EnsureRequiredModules(UNiagaraSystem* System, int32 EmitterIndex,
		const FHktVFXEmitterUpdateConfig& Config);

	// 데이터 인터페이스 — User Parameter로 DI를 시스템에 추가하고 모듈에 바인딩
	void SetupDataInterfaces(UNiagaraSystem* System, int32 EmitterIndex,
		const TArray<FHktVFXDataInterfaceBinding>& DataInterfaces);

	// private 파라미터 setter (public에 없는 것들)
	void SetParticleParamVec4(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, FVector4 Value);

	// 디버그: 에미터의 기존 RapidIterationParameter 이름을 모두 로그 출력
	void LogExistingParameters(UNiagaraSystem* System, int32 EmitterIndex);
};
