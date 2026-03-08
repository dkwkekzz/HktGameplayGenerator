// Copyright Hkt Studios, Inc. All Rights Reserved.
// Config → 실제 UNiagaraSystem 에셋 빌드 (Phase 0)

#include "HktVFXNiagaraBuilder.h"
#include "HktVFXGeneratorSettings.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "NiagaraParameterStore.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraLightRendererProperties.h"

#include "NiagaraSystemFactoryNew.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktVFXBuilder, Log, All);

// ============================================================================
// 메인 빌드 엔트리
// ============================================================================
UNiagaraSystem* FHktVFXNiagaraBuilder::BuildNiagaraSystem(
	const FHktVFXNiagaraConfig& Config,
	const FString& OutputDirectory)
{
	if (!Config.IsValid())
	{
		UE_LOG(LogHktVFXBuilder, Error, TEXT("Invalid config"));
		return nullptr;
	}

	// 패키지 & 시스템 생성 (팩토리를 통해 내부 초기화 보장)
	FString SystemName = FString::Printf(TEXT("NS_%s"), *Config.SystemName);
	FString PackagePath = OutputDirectory / SystemName;

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE_LOG(LogHktVFXBuilder, Error, TEXT("Failed to create package: %s"), *PackagePath);
		return nullptr;
	}

	UNiagaraSystemFactoryNew* SystemFactory = NewObject<UNiagaraSystemFactoryNew>();
	UNiagaraSystem* System = Cast<UNiagaraSystem>(SystemFactory->FactoryCreateNew(
		UNiagaraSystem::StaticClass(), Package, *SystemName,
		RF_Public | RF_Standalone, nullptr, GWarn));
	if (!System)
	{
		UE_LOG(LogHktVFXBuilder, Error, TEXT("Failed to create NiagaraSystem via Factory"));
		return nullptr;
	}

	// 각 에미터 구성
	for (const FHktVFXEmitterConfig& EmitterConfig : Config.Emitters)
	{
		UE_LOG(LogHktVFXBuilder, Log, TEXT("Building emitter: %s"), *EmitterConfig.Name);
		ConfigureEmitter(System, EmitterConfig);
	}

	// 컴파일 & 저장
	System->RequestCompile(false);

	FString PackageFileName = FPackageName::LongPackageNameToFilename(
		PackagePath, FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, System, *PackageFileName, SaveArgs);

	FAssetRegistryModule::AssetCreated(System);

	UE_LOG(LogHktVFXBuilder, Log, TEXT("Built NiagaraSystem: %s (%d emitters)"),
		*PackagePath, Config.Emitters.Num());

	return System;
}

// ============================================================================
// 에미터 템플릿 로드 — UHktVFXGeneratorSettings에서 경로 읽기
// ============================================================================
UNiagaraEmitter* FHktVFXNiagaraBuilder::LoadEmitterTemplate(const FString& RendererType)
{
	const UHktVFXGeneratorSettings* Settings = UHktVFXGeneratorSettings::Get();

	// Settings 매핑에서 찾기
	if (const FSoftObjectPath* TemplatePath = Settings->EmitterTemplates.Find(RendererType))
	{
		if (TemplatePath->IsValid())
		{
			UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(TemplatePath->TryLoad());
			if (Emitter)
			{
				UE_LOG(LogHktVFXBuilder, Log, TEXT("Loaded template '%s' for type '%s'"),
					*TemplatePath->GetAssetPathString(), *RendererType);
				return Emitter;
			}
			UE_LOG(LogHktVFXBuilder, Warning, TEXT("Failed to load template '%s' for type '%s'"),
				*TemplatePath->GetAssetPathString(), *RendererType);
		}
	}

	// 폴백
	if (Settings->FallbackEmitterTemplate.IsValid())
	{
		UNiagaraEmitter* Fallback = Cast<UNiagaraEmitter>(Settings->FallbackEmitterTemplate.TryLoad());
		if (Fallback)
		{
			UE_LOG(LogHktVFXBuilder, Log, TEXT("Using fallback template for type '%s'"), *RendererType);
			return Fallback;
		}
	}

	UE_LOG(LogHktVFXBuilder, Error, TEXT("No emitter template available for type '%s'. Check Project Settings > Plugins > HKT VFX Generator."), *RendererType);
	return nullptr;
}

// ============================================================================
// 에미터 전체 구성
// ============================================================================
void FHktVFXNiagaraBuilder::ConfigureEmitter(
	UNiagaraSystem* System,
	const FHktVFXEmitterConfig& Config)
{
	UNiagaraEmitter* TemplateEmitter = LoadEmitterTemplate(Config.Render.RendererType);
	if (!TemplateEmitter)
	{
		return;
	}

	// 시스템에 에미터 핸들 추가
	FName EmitterName = FName(*Config.Name);
	System->AddEmitterHandle(*TemplateEmitter, EmitterName, FGuid());

	int32 ActualIndex = System->GetEmitterHandles().Num() - 1;
	UE_LOG(LogHktVFXBuilder, Log, TEXT("Added emitter '%s' (index %d)"),
		*Config.Name, ActualIndex);

	// 모듈별 파라미터 오버라이드
	SetupSpawnModule(System, ActualIndex, Config.Spawn);
	SetupInitializeModule(System, ActualIndex, Config.Init);
	SetupUpdateModules(System, ActualIndex, Config.Update);
	SetupRenderer(System, ActualIndex, Config.Render);
}

// ============================================================================
// 스폰 설정
// ============================================================================
void FHktVFXNiagaraBuilder::SetupSpawnModule(UNiagaraSystem* System, int32 EmitterIndex,
	const FHktVFXEmitterSpawnConfig& Config)
{
	if (Config.Mode == TEXT("rate"))
	{
		SetNiagaraVariableFloat(System, EmitterIndex,
			TEXT("SpawnRate"), TEXT("SpawnRate"), Config.Rate);
	}
	else // burst
	{
		SetNiagaraVariableFloat(System, EmitterIndex,
			TEXT("SpawnBurstInstantaneous"), TEXT("SpawnCount"),
			static_cast<float>(Config.BurstCount));
	}
}

// ============================================================================
// 초기화 설정
// ============================================================================
void FHktVFXNiagaraBuilder::SetupInitializeModule(UNiagaraSystem* System, int32 EmitterIndex,
	const FHktVFXEmitterInitConfig& Config)
{
	const FString Module = TEXT("InitializeParticle");

	SetNiagaraVariableFloat(System, EmitterIndex, Module, TEXT("Lifetime.Minimum"), Config.LifetimeMin);
	SetNiagaraVariableFloat(System, EmitterIndex, Module, TEXT("Lifetime.Maximum"), Config.LifetimeMax);

	SetNiagaraVariableFloat(System, EmitterIndex, Module, TEXT("SpriteSize.Minimum.X"), Config.SizeMin);
	SetNiagaraVariableFloat(System, EmitterIndex, Module, TEXT("SpriteSize.Minimum.Y"), Config.SizeMin);
	SetNiagaraVariableFloat(System, EmitterIndex, Module, TEXT("SpriteSize.Maximum.X"), Config.SizeMax);
	SetNiagaraVariableFloat(System, EmitterIndex, Module, TEXT("SpriteSize.Maximum.Y"), Config.SizeMax);

	SetNiagaraVariableVec3(System, EmitterIndex, Module, TEXT("Velocity.Minimum"), Config.VelocityMin);
	SetNiagaraVariableVec3(System, EmitterIndex, Module, TEXT("Velocity.Maximum"), Config.VelocityMax);

	SetNiagaraVariableColor(System, EmitterIndex, Module, TEXT("Color"), Config.Color);
}

// ============================================================================
// 업데이트 모듈 설정
// ============================================================================
void FHktVFXNiagaraBuilder::SetupUpdateModules(UNiagaraSystem* System, int32 EmitterIndex,
	const FHktVFXEmitterUpdateConfig& Config)
{
	if (!Config.Gravity.IsNearlyZero())
	{
		SetNiagaraVariableVec3(System, EmitterIndex,
			TEXT("Gravity Force"), TEXT("Gravity"), Config.Gravity);
	}

	if (Config.Drag > 0.f)
	{
		SetNiagaraVariableFloat(System, EmitterIndex,
			TEXT("Drag"), TEXT("Drag"), Config.Drag);
	}
}

// ============================================================================
// 렌더러 설정
// ============================================================================
void FHktVFXNiagaraBuilder::SetupRenderer(UNiagaraSystem* System, int32 EmitterIndex,
	const FHktVFXEmitterRenderConfig& Config)
{
	const auto& EmitterHandles = System->GetEmitterHandles();
	if (!EmitterHandles.IsValidIndex(EmitterIndex)) return;

	FVersionedNiagaraEmitterData* EmitterData = EmitterHandles[EmitterIndex].GetEmitterData();
	if (!EmitterData) return;

	const auto& RendererProperties = EmitterData->GetRenderers();
	if (RendererProperties.Num() == 0) return;

	UNiagaraRendererProperties* Renderer = RendererProperties[0];

	if (Config.RendererType == TEXT("sprite"))
	{
		if (UNiagaraSpriteRendererProperties* SpriteRenderer =
			Cast<UNiagaraSpriteRendererProperties>(Renderer))
		{
			SpriteRenderer->SortOrderHint = Config.SortOrder;
		}
	}
	else if (Config.RendererType == TEXT("light"))
	{
		if (UNiagaraLightRendererProperties* LightRenderer =
			Cast<UNiagaraLightRendererProperties>(Renderer))
		{
			LightRenderer->RadiusScale = 2.0f;
		}
	}
}

// ============================================================================
// VFX 머티리얼 로드
// ============================================================================
UMaterialInterface* FHktVFXNiagaraBuilder::GetVFXMaterial(const FString& BlendMode)
{
	const UHktVFXGeneratorSettings* Settings = UHktVFXGeneratorSettings::Get();

	const FSoftObjectPath& MatPath = (BlendMode == TEXT("translucent"))
		? Settings->TranslucentMaterial
		: Settings->AdditiveMaterial;

	if (MatPath.IsValid())
	{
		UMaterialInterface* Mat = Cast<UMaterialInterface>(MatPath.TryLoad());
		if (Mat) return Mat;
	}

	return nullptr;
}

// ============================================================================
// 파라미터 설정 유틸리티
// ============================================================================
void FHktVFXNiagaraBuilder::SetNiagaraVariableFloat(
	UNiagaraSystem* System, int32 EmitterIndex,
	const FString& ModuleName, const FString& ParamName, float Value)
{
	const auto& EmitterHandles = System->GetEmitterHandles();
	if (!EmitterHandles.IsValidIndex(EmitterIndex)) return;

	FVersionedNiagaraEmitterData* EmitterData =
		EmitterHandles[EmitterIndex].GetEmitterData();
	if (!EmitterData) return;

	FString FullParamName = FString::Printf(TEXT("%s.%s.%s"),
		*EmitterHandles[EmitterIndex].GetName().ToString(),
		*ModuleName, *ParamName);

	FNiagaraVariable Var;
	Var.SetName(FName(*FullParamName));
	Var.SetType(FNiagaraTypeDefinition::GetFloatDef());

	for (UNiagaraScript* Script : {
		EmitterData->SpawnScriptProps.Script,
		EmitterData->UpdateScriptProps.Script })
	{
		if (Script)
		{
			Script->RapidIterationParameters.SetParameterValue<float>(Value, Var);
		}
	}
}

void FHktVFXNiagaraBuilder::SetNiagaraVariableVec3(
	UNiagaraSystem* System, int32 EmitterIndex,
	const FString& ModuleName, const FString& ParamName, FVector Value)
{
	const auto& EmitterHandles = System->GetEmitterHandles();
	if (!EmitterHandles.IsValidIndex(EmitterIndex)) return;

	FVersionedNiagaraEmitterData* EmitterData =
		EmitterHandles[EmitterIndex].GetEmitterData();
	if (!EmitterData) return;

	FString FullParamName = FString::Printf(TEXT("%s.%s.%s"),
		*EmitterHandles[EmitterIndex].GetName().ToString(),
		*ModuleName, *ParamName);

	FNiagaraVariable Var;
	Var.SetName(FName(*FullParamName));
	Var.SetType(FNiagaraTypeDefinition::GetVec3Def());

	for (UNiagaraScript* Script : {
		EmitterData->SpawnScriptProps.Script,
		EmitterData->UpdateScriptProps.Script })
	{
		if (Script)
		{
			Script->RapidIterationParameters.SetParameterValue<FVector3f>(
				FVector3f(Value), Var);
		}
	}
}

void FHktVFXNiagaraBuilder::SetNiagaraVariableColor(
	UNiagaraSystem* System, int32 EmitterIndex,
	const FString& ModuleName, const FString& ParamName, FLinearColor Value)
{
	const auto& EmitterHandles = System->GetEmitterHandles();
	if (!EmitterHandles.IsValidIndex(EmitterIndex)) return;

	FVersionedNiagaraEmitterData* EmitterData =
		EmitterHandles[EmitterIndex].GetEmitterData();
	if (!EmitterData) return;

	FString FullParamName = FString::Printf(TEXT("%s.%s.%s"),
		*EmitterHandles[EmitterIndex].GetName().ToString(),
		*ModuleName, *ParamName);

	FNiagaraVariable Var;
	Var.SetName(FName(*FullParamName));
	Var.SetType(FNiagaraTypeDefinition::GetColorDef());

	for (UNiagaraScript* Script : {
		EmitterData->SpawnScriptProps.Script,
		EmitterData->UpdateScriptProps.Script })
	{
		if (Script)
		{
			Script->RapidIterationParameters.SetParameterValue<FLinearColor>(Value, Var);
		}
	}
}
