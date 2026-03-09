// Copyright Hkt Studios, Inc. All Rights Reserved.

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
#include "NiagaraEditorUtilities.h"

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

	// 시스템 레벨 속성
	SetupSystemProperties(System, Config);

	// 각 에미터 구성
	for (const FHktVFXEmitterConfig& EmitterConfig : Config.Emitters)
	{
		UE_LOG(LogHktVFXBuilder, Log, TEXT("Building emitter: %s"), *EmitterConfig.Name);
		ConfigureEmitter(System, EmitterConfig);
	}

	// 컴파일 & 저장
	System->RequestCompile(false);
	System->MarkPackageDirty();

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
// 시스템 레벨 속성
// ============================================================================
void FHktVFXNiagaraBuilder::SetupSystemProperties(
	UNiagaraSystem* System,
	const FHktVFXNiagaraConfig& Config)
{
	if (Config.WarmupTime > 0.f)
	{
		System->SetWarmupTime(Config.WarmupTime);
		System->SetWarmupTickDelta(1.f / 30.f);
		//System->SetWarmupTickCount(FMath::CeilToInt(Config.WarmupTime * 30.f));
		System->ResolveWarmupTickCount();
	}
}

// ============================================================================
// 에미터 템플릿 로드
// ============================================================================
UNiagaraEmitter* FHktVFXNiagaraBuilder::LoadEmitterTemplate(const FString& RendererType)
{
	const UHktVFXGeneratorSettings* Settings = UHktVFXGeneratorSettings::Get();

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

	if (Settings->FallbackEmitterTemplate.IsValid())
	{
		UNiagaraEmitter* Fallback = Cast<UNiagaraEmitter>(Settings->FallbackEmitterTemplate.TryLoad());
		if (Fallback)
		{
			UE_LOG(LogHktVFXBuilder, Log, TEXT("Using fallback template for type '%s'"), *RendererType);
			return Fallback;
		}
	}

	UE_LOG(LogHktVFXBuilder, Error, TEXT("No emitter template for type '%s'"), *RendererType);
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

	FNiagaraEditorUtilities::AddEmitterToSystem(*System, *TemplateEmitter, FGuid());

	int32 ActualIndex = System->GetEmitterHandles().Num() - 1;
	FString HandleName = System->GetEmitterHandles()[ActualIndex].GetName().ToString();

	UE_LOG(LogHktVFXBuilder, Log, TEXT("Added emitter '%s' as '%s' (index %d)"),
		*Config.Name, *HandleName, ActualIndex);

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

		if (Config.BurstDelay > 0.f)
		{
			SetNiagaraVariableFloat(System, EmitterIndex,
				TEXT("SpawnBurstInstantaneous"), TEXT("SpawnTime"), Config.BurstDelay);
		}
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

	// 초기 회전
	if (Config.SpriteRotationMax > 0.f || Config.SpriteRotationMin != 0.f)
	{
		SetNiagaraVariableFloat(System, EmitterIndex, Module,
			TEXT("SpriteRotation.Minimum"), Config.SpriteRotationMin);
		SetNiagaraVariableFloat(System, EmitterIndex, Module,
			TEXT("SpriteRotation.Maximum"), Config.SpriteRotationMax);
	}

	// 질량
	if (Config.MassMin != 1.f || Config.MassMax != 1.f)
	{
		SetNiagaraVariableFloat(System, EmitterIndex, Module,
			TEXT("Mass.Minimum"), Config.MassMin);
		SetNiagaraVariableFloat(System, EmitterIndex, Module,
			TEXT("Mass.Maximum"), Config.MassMax);
	}
}

// ============================================================================
// 업데이트 모듈 설정
// ============================================================================
void FHktVFXNiagaraBuilder::SetupUpdateModules(UNiagaraSystem* System, int32 EmitterIndex,
	const FHktVFXEmitterUpdateConfig& Config)
{
	// Gravity
	if (!Config.Gravity.IsNearlyZero())
	{
		SetNiagaraVariableVec3(System, EmitterIndex,
			TEXT("Gravity Force"), TEXT("Gravity"), Config.Gravity);
	}

	// Drag
	if (Config.Drag > 0.f)
	{
		SetNiagaraVariableFloat(System, EmitterIndex,
			TEXT("Drag"), TEXT("Drag"), Config.Drag);
	}

	// Rotation Rate
	if (Config.RotationRateMax > 0.f || Config.RotationRateMin != 0.f)
	{
		SetNiagaraVariableFloat(System, EmitterIndex,
			TEXT("SpriteRotationRate"), TEXT("RotationRate.Minimum"), Config.RotationRateMin);
		SetNiagaraVariableFloat(System, EmitterIndex,
			TEXT("SpriteRotationRate"), TEXT("RotationRate.Maximum"), Config.RotationRateMax);
	}

	// Size Scale Over Life
	if (Config.SizeScaleStart != 1.f || Config.SizeScaleEnd != 1.f)
	{
		SetNiagaraVariableFloat(System, EmitterIndex,
			TEXT("ScaleSpriteSize"), TEXT("ScaleFactor.Minimum"), Config.SizeScaleStart);
		SetNiagaraVariableFloat(System, EmitterIndex,
			TEXT("ScaleSpriteSize"), TEXT("ScaleFactor.Maximum"), Config.SizeScaleEnd);
	}

	// Opacity Over Life
	if (Config.OpacityStart != 1.f || Config.OpacityEnd != 0.f)
	{
		SetNiagaraVariableFloat(System, EmitterIndex,
			TEXT("ScaleColor"), TEXT("AlphaScale.Start"), Config.OpacityStart);
		SetNiagaraVariableFloat(System, EmitterIndex,
			TEXT("ScaleColor"), TEXT("AlphaScale.End"), Config.OpacityEnd);
	}

	// Color Over Life
	if (Config.bUseColorOverLife)
	{
		SetNiagaraVariableColor(System, EmitterIndex,
			TEXT("ScaleColor"), TEXT("ColorScale.Start"), FLinearColor::White);
		SetNiagaraVariableColor(System, EmitterIndex,
			TEXT("ScaleColor"), TEXT("ColorScale.End"), Config.ColorEnd);
	}

	// Curl Noise
	if (Config.NoiseStrength > 0.f)
	{
		SetNiagaraVariableFloat(System, EmitterIndex,
			TEXT("Curl Noise Force"), TEXT("NoisStrength"), Config.NoiseStrength);
		SetNiagaraVariableFloat(System, EmitterIndex,
			TEXT("Curl Noise Force"), TEXT("NoiseFrequency"), Config.NoiseFrequency);
	}

	// Point Attractor
	if (Config.AttractionStrength > 0.f)
	{
		SetNiagaraVariableFloat(System, EmitterIndex,
			TEXT("Point Attraction Force"), TEXT("AttractionStrength"), Config.AttractionStrength);
		SetNiagaraVariableFloat(System, EmitterIndex,
			TEXT("Point Attraction Force"), TEXT("AttractionRadius"), Config.AttractionRadius);
		SetNiagaraVariableVec3(System, EmitterIndex,
			TEXT("Point Attraction Force"), TEXT("AttractorPosition"), Config.AttractionPosition);
	}

	// Vortex
	if (Config.VortexStrength > 0.f)
	{
		SetNiagaraVariableFloat(System, EmitterIndex,
			TEXT("Vortex Force"), TEXT("VortexStrength"), Config.VortexStrength);
		SetNiagaraVariableFloat(System, EmitterIndex,
			TEXT("Vortex Force"), TEXT("VortexRadius"), Config.VortexRadius);
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
	Renderer->SortOrderHint = Config.SortOrder;

	// Sprite
	if (Config.RendererType == TEXT("sprite"))
	{
		if (UNiagaraSpriteRendererProperties* SR =
			Cast<UNiagaraSpriteRendererProperties>(Renderer))
		{
			if (Config.Alignment == TEXT("velocity_aligned"))
			{
				SR->Alignment = ENiagaraSpriteAlignment::VelocityAligned;
			}

			// 머티리얼 적용
			UMaterialInterface* Mat = GetVFXMaterial(Config.BlendMode);
			if (Mat)
			{
				SR->Material = Mat;
			}
		}
	}
	// Light
	else if (Config.RendererType == TEXT("light"))
	{
		if (UNiagaraLightRendererProperties* LR =
			Cast<UNiagaraLightRendererProperties>(Renderer))
		{
			LR->RadiusScale = Config.LightRadiusScale;
			LR->ColorAdd = FVector3f(
				Config.LightIntensity, Config.LightIntensity, Config.LightIntensity);
		}
	}
	// Ribbon
	else if (Config.RendererType == TEXT("ribbon"))
	{
		if (UNiagaraRibbonRendererProperties* RR =
			Cast<UNiagaraRibbonRendererProperties>(Renderer))
		{
			UMaterialInterface* Mat = GetVFXMaterial(Config.BlendMode);
			if (Mat)
			{
				RR->Material = Mat;
			}
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
