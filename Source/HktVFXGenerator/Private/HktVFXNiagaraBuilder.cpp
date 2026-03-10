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
#include "NiagaraMeshRendererProperties.h"

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

	SetupSystemProperties(System, Config);

	for (const FHktVFXEmitterConfig& EmitterConfig : Config.Emitters)
	{
		UE_LOG(LogHktVFXBuilder, Log, TEXT("Building emitter: %s"), *EmitterConfig.Name);
		ConfigureEmitter(System, EmitterConfig);
	}

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
		System->ResolveWarmupTickCount();
	}
}

// ============================================================================
// 에미터 템플릿 로드
// 우선순위: EmitterTemplate > RendererType > Fallback
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
				UE_LOG(LogHktVFXBuilder, Log, TEXT("Loaded template '%s' for key '%s'"),
					*TemplatePath->GetAssetPathString(), *RendererType);
				return Emitter;
			}
			else
			{
				UE_LOG(LogHktVFXBuilder, Warning,
					TEXT("Template asset not found: '%s' (key='%s'). Trying fallback."),
					*TemplatePath->GetAssetPathString(), *RendererType);
			}
		}
	}

	if (Settings->FallbackEmitterTemplate.IsValid())
	{
		UNiagaraEmitter* Fallback = Cast<UNiagaraEmitter>(Settings->FallbackEmitterTemplate.TryLoad());
		if (Fallback)
		{
			UE_LOG(LogHktVFXBuilder, Log, TEXT("Using fallback template for key '%s'"), *RendererType);
			return Fallback;
		}
	}

	UE_LOG(LogHktVFXBuilder, Error, TEXT("No emitter template for key '%s'"), *RendererType);
	return nullptr;
}

// ============================================================================
// 에미터 전체 구성
// ============================================================================
void FHktVFXNiagaraBuilder::ConfigureEmitter(
	UNiagaraSystem* System,
	const FHktVFXEmitterConfig& Config)
{
	// EmitterTemplate 키가 있으면 그걸로 조회, 없으면 RendererType으로 폴백
	const FString& TemplateKey = Config.Render.EmitterTemplate.IsEmpty()
		? Config.Render.RendererType
		: Config.Render.EmitterTemplate;

	UNiagaraEmitter* TemplateEmitter = LoadEmitterTemplate(TemplateKey);
	if (!TemplateEmitter)
	{
		return;
	}

	// EmitterTemplate이 지정된 경우 = NiagaraExamples 에미터 (이미 모듈+머티리얼 완비)
	const bool bUsingRichTemplate = !Config.Render.EmitterTemplate.IsEmpty();

	FNiagaraEditorUtilities::AddEmitterToSystem(*System, *TemplateEmitter, FGuid());

	int32 ActualIndex = System->GetEmitterHandles().Num() - 1;
	FString HandleName = System->GetEmitterHandles()[ActualIndex].GetName().ToString();

	UE_LOG(LogHktVFXBuilder, Log, TEXT("Added emitter '%s' as '%s' (index %d, template='%s', rich=%d)"),
		*Config.Name, *HandleName, ActualIndex, *TemplateKey, bUsingRichTemplate);

	// Rich 템플릿(NE_)은 이미 모듈이 완비되어 있으므로 RapidIterationParameters 설정은
	// 적용 가능한 것만 시도 (실패해도 무시). 기본 템플릿은 기존 로직 사용.
	SetupSpawnModule(System, ActualIndex, Config.Spawn);
	SetupInitializeModule(System, ActualIndex, Config.Init);
	SetupUpdateModules(System, ActualIndex, Config.Update);
	SetupRenderer(System, ActualIndex, Config.Render);

	// 머티리얼 오버라이드 (materialPath가 지정된 경우)
	if (!Config.Render.MaterialPath.IsEmpty())
	{
		ApplyMaterialOverride(System, ActualIndex, Config.Render.MaterialPath);
	}
}

// ============================================================================
// 스폰 설정 — EmitterUpdateScript에 기록
// 템플릿에 따라 SpawnBurst_Instantaneous 또는 SpawnRate 모듈이 존재.
// 양쪽 다 설정 시도 — 해당 모듈이 있는 쪽만 적용됨.
// ============================================================================
void FHktVFXNiagaraBuilder::SetupSpawnModule(UNiagaraSystem* System, int32 EmitterIndex,
	const FHktVFXEmitterSpawnConfig& Config)
{
	if (Config.Mode == TEXT("burst"))
	{
		// SpawnBurst_Instantaneous 모듈 (burst 템플릿)
		SetEmitterParamInt(System, EmitterIndex,
			TEXT("SpawnBurst_Instantaneous"), TEXT("Spawn Count"), Config.BurstCount);

		if (Config.BurstDelay > 0.f)
		{
			SetEmitterParamFloat(System, EmitterIndex,
				TEXT("SpawnBurst_Instantaneous"), TEXT("Spawn Time"), Config.BurstDelay);
		}
	}
	else if (Config.Mode == TEXT("rate"))
	{
		// SpawnRate 모듈 (rate 템플릿: fountain, blowing_particles, hanging_particulates 등)
		SetEmitterParamFloat(System, EmitterIndex,
			TEXT("SpawnRate"), TEXT("SpawnRate"), Config.Rate);
	}
}

// ============================================================================
// 초기화 설정 — Particle SpawnScript에 기록
// ============================================================================
void FHktVFXNiagaraBuilder::SetupInitializeModule(UNiagaraSystem* System, int32 EmitterIndex,
	const FHktVFXEmitterInitConfig& Config)
{
	const FString Module = TEXT("InitializeParticle");

	// Lifetime — 단일 float (Min/Max 평균값 사용)
	float AvgLifetime = (Config.LifetimeMin + Config.LifetimeMax) * 0.5f;
	SetParticleParamFloat(System, EmitterIndex, Module, TEXT("Lifetime"), AvgLifetime);

	// Uniform Sprite Size — 단일 float (Min/Max 평균값 사용)
	float AvgSize = (Config.SizeMin + Config.SizeMax) * 0.5f;
	SetParticleParamFloat(System, EmitterIndex, Module, TEXT("Uniform Sprite Size"), AvgSize);

	// Color
	SetParticleParamColor(System, EmitterIndex, Module, TEXT("Color"), Config.Color);
}

// ============================================================================
// 업데이트 모듈 설정
// 모든 가능한 모듈 파라미터를 시도 — 해당 모듈이 있는 템플릿에만 적용.
//
// 모듈별 존재 여부 (템플릿):
//   ScaleColor            — 대부분 (SimpleSpriteBurst, Fountain, OmniBurst 등)
//   SolveForcesAndVelocity — 대부분
//   GravityForce          — Fountain, OmniBurst, DirectionalBurst, ConfettiBurst, UpwardMeshBurst
//   Drag                  — OmniBurst, DirectionalBurst, ConfettiBurst
//   CurlNoiseForce        — BlowingParticles, HangingParticulates
//   SpriteRotationRate    — ConfettiBurst
//   ScaleSpriteSize       — 일부 템플릿
//   ScaleMeshSize         — UpwardMeshBurst
// ============================================================================
void FHktVFXNiagaraBuilder::SetupUpdateModules(UNiagaraSystem* System, int32 EmitterIndex,
	const FHktVFXEmitterUpdateConfig& Config)
{
	// --- ScaleColor 모듈 ---
	// Opacity → Scale RGBA
	if (Config.OpacityStart != 1.f || Config.OpacityEnd != 0.f)
	{
		FVector4 ScaleRGBA(1.f, 1.f, 1.f, Config.OpacityStart);
		SetParticleParamVec4(System, EmitterIndex,
			TEXT("ScaleColor"), TEXT("Scale RGBA"), ScaleRGBA);
	}

	// Color Over Life → Scale RGB
	if (Config.bUseColorOverLife)
	{
		FVector ScaleRGB(Config.ColorEnd.R, Config.ColorEnd.G, Config.ColorEnd.B);
		SetParticleParamVec3(System, EmitterIndex,
			TEXT("ScaleColor"), TEXT("Scale RGB"), ScaleRGB);
	}

	// --- GravityForce 모듈 ---
	// Fountain, OmniBurst, DirectionalBurst, ConfettiBurst, UpwardMeshBurst에 존재
	if (!Config.Gravity.IsNearlyZero(1.f))
	{
		SetParticleParamVec3(System, EmitterIndex,
			TEXT("Gravity Force"), TEXT("Gravity"), Config.Gravity);
	}

	// --- Drag 모듈 ---
	// OmniBurst, DirectionalBurst, ConfettiBurst에 존재
	if (Config.Drag > 0.f)
	{
		SetParticleParamFloat(System, EmitterIndex,
			TEXT("Drag"), TEXT("Drag"), Config.Drag);
	}

	// --- CurlNoiseForce 모듈 ---
	// BlowingParticles, HangingParticulates에 존재
	if (Config.NoiseStrength > 0.f)
	{
		SetParticleParamFloat(System, EmitterIndex,
			TEXT("Curl Noise Force"), TEXT("Noise Strength"), Config.NoiseStrength);
		SetParticleParamFloat(System, EmitterIndex,
			TEXT("Curl Noise Force"), TEXT("Noise Frequency"), Config.NoiseFrequency);
	}

	// --- SpriteRotationRate 모듈 ---
	// ConfettiBurst에 존재
	if (Config.RotationRateMin != 0.f || Config.RotationRateMax != 0.f)
	{
		float AvgRate = (Config.RotationRateMin + Config.RotationRateMax) * 0.5f;
		SetParticleParamFloat(System, EmitterIndex,
			TEXT("Sprite Rotation Rate"), TEXT("Rotation Rate"), AvgRate);
	}

	// --- ScaleSpriteSize / ScaleMeshSize 모듈 ---
	// Size over life
	if (Config.SizeScaleStart != 1.f || Config.SizeScaleEnd != 1.f)
	{
		// ScaleSpriteSize.Scale Factor (Vec2)
		// ScaleMeshSize.Scale Factor (Vec3) — 메시 버전
		// 여기서는 둘 다 시도
		FVector SizeScale(Config.SizeScaleEnd, Config.SizeScaleEnd, Config.SizeScaleEnd);
		SetParticleParamVec3(System, EmitterIndex,
			TEXT("Scale Sprite Size"), TEXT("Scale Factor"), FVector(Config.SizeScaleEnd, Config.SizeScaleEnd, 0.f));
		SetParticleParamVec3(System, EmitterIndex,
			TEXT("Scale Mesh Size"), TEXT("Scale Factor"), SizeScale);
	}

	// --- SolveForcesAndVelocity 모듈 ---
	// Speed Limit — 거의 모든 템플릿에 존재
	// Drag 모듈이 없는 SimpleSpriteBurst에서 Speed Limit으로 간접 감속
	if (Config.Drag > 0.f)
	{
		float SpeedLimit = FMath::Max(100.f / Config.Drag, 10.f);
		SetParticleParamFloat(System, EmitterIndex,
			TEXT("SolveForcesAndVelocity"), TEXT("Speed Limit"), SpeedLimit);
	}

	// --- Point Attraction 모듈 ---
	// 일부 커스텀 템플릿에 존재 가능
	if (Config.AttractionStrength > 0.f)
	{
		SetParticleParamFloat(System, EmitterIndex,
			TEXT("Point Attraction Force"), TEXT("Attraction Strength"), Config.AttractionStrength);
		SetParticleParamFloat(System, EmitterIndex,
			TEXT("Point Attraction Force"), TEXT("Attraction Radius"), Config.AttractionRadius);
		SetParticleParamVec3(System, EmitterIndex,
			TEXT("Point Attraction Force"), TEXT("Attraction Position Offset"),
			Config.AttractionPosition);
	}

	// --- Vortex Velocity 모듈 ---
	if (Config.VortexStrength > 0.f)
	{
		SetParticleParamFloat(System, EmitterIndex,
			TEXT("Vortex Velocity"), TEXT("Vortex Strength"), Config.VortexStrength);
		SetParticleParamFloat(System, EmitterIndex,
			TEXT("Vortex Velocity"), TEXT("Vortex Radius"), Config.VortexRadius);
	}
}

// ============================================================================
// 렌더러 설정
// 렌더러 타입은 Config가 아닌 실제 템플릿의 렌더러를 기반으로 감지.
// ============================================================================
void FHktVFXNiagaraBuilder::SetupRenderer(UNiagaraSystem* System, int32 EmitterIndex,
	const FHktVFXEmitterRenderConfig& Config)
{
	const auto& EmitterHandles = System->GetEmitterHandles();
	if (!EmitterHandles.IsValidIndex(EmitterIndex)) return;

	FVersionedNiagaraEmitterData* EmitterData = EmitterHandles[EmitterIndex].GetEmitterData();
	if (!EmitterData) return;

	// 모든 렌더러에 SortOrder 설정
	for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
	{
		Renderer->SortOrderHint = Config.SortOrder;

		// --- Sprite 렌더러 ---
		if (UNiagaraSpriteRendererProperties* SR = Cast<UNiagaraSpriteRendererProperties>(Renderer))
		{
			if (Config.Alignment == TEXT("velocity_aligned"))
			{
				SR->Alignment = ENiagaraSpriteAlignment::VelocityAligned;
			}

			// EmitterTemplate이 지정되면 템플릿 머티리얼 유지 (텍스처 포함)
			if (Config.EmitterTemplate.IsEmpty())
			{
				UMaterialInterface* Mat = GetVFXMaterial(Config.BlendMode);
				if (Mat)
				{
					SR->Material = Mat;
				}
			}
		}
		// --- Light 렌더러 ---
		else if (UNiagaraLightRendererProperties* LR = Cast<UNiagaraLightRendererProperties>(Renderer))
		{
			LR->RadiusScale = Config.LightRadiusScale;
			LR->ColorAdd = FVector3f(
				Config.LightIntensity, Config.LightIntensity, Config.LightIntensity);
		}
		// --- Ribbon 렌더러 ---
		else if (UNiagaraRibbonRendererProperties* RR = Cast<UNiagaraRibbonRendererProperties>(Renderer))
		{
			if (Config.EmitterTemplate.IsEmpty())
			{
				UMaterialInterface* Mat = GetVFXMaterial(Config.BlendMode);
				if (Mat)
				{
					RR->Material = Mat;
				}
			}
		}
		// --- Mesh 렌더러 ---
		else if (UNiagaraMeshRendererProperties* MR = Cast<UNiagaraMeshRendererProperties>(Renderer))
		{
			// Mesh 렌더러는 템플릿의 기본 메시/머티리얼 유지
			// materialPath로 오버라이드 가능
		}
	}
}

// ============================================================================
// 머티리얼 오버라이드 — materialPath로 지정된 머티리얼을 렌더러에 적용
// ============================================================================
void FHktVFXNiagaraBuilder::ApplyMaterialOverride(
	UNiagaraSystem* System, int32 EmitterIndex, const FString& MaterialPath)
{
	const auto& EmitterHandles = System->GetEmitterHandles();
	if (!EmitterHandles.IsValidIndex(EmitterIndex)) return;

	FSoftObjectPath MatPath(MaterialPath);
	if (!MatPath.IsValid()) return;

	UMaterialInterface* Mat = Cast<UMaterialInterface>(MatPath.TryLoad());
	if (!Mat)
	{
		UE_LOG(LogHktVFXBuilder, Warning, TEXT("Material not found: %s"), *MaterialPath);
		return;
	}

	FVersionedNiagaraEmitterData* EmitterData = EmitterHandles[EmitterIndex].GetEmitterData();
	if (!EmitterData) return;

	for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
	{
		if (UNiagaraSpriteRendererProperties* SR = Cast<UNiagaraSpriteRendererProperties>(Renderer))
		{
			SR->Material = Mat;
			UE_LOG(LogHktVFXBuilder, Log, TEXT("Applied material override: %s"), *MaterialPath);
		}
		else if (UNiagaraRibbonRendererProperties* RR = Cast<UNiagaraRibbonRendererProperties>(Renderer))
		{
			RR->Material = Mat;
			UE_LOG(LogHktVFXBuilder, Log, TEXT("Applied ribbon material override: %s"), *MaterialPath);
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
// Particle-level 파라미터 (SpawnScript + UpdateScript)
// 이름 형식: Constants.{HandleName}.{Module}.{Param}
// ============================================================================

void FHktVFXNiagaraBuilder::SetParticleParamFloat(
	UNiagaraSystem* System, int32 EmitterIndex,
	const FString& ModuleName, const FString& ParamName, float Value)
{
	const auto& EmitterHandles = System->GetEmitterHandles();
	if (!EmitterHandles.IsValidIndex(EmitterIndex)) return;

	FVersionedNiagaraEmitterData* EmitterData =
		EmitterHandles[EmitterIndex].GetEmitterData();
	if (!EmitterData) return;

	FString FullName = FString::Printf(TEXT("Constants.%s.%s.%s"),
		*EmitterHandles[EmitterIndex].GetName().ToString(),
		*ModuleName, *ParamName);

	FNiagaraVariable Var;
	Var.SetName(FName(*FullName));
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

	UE_LOG(LogHktVFXBuilder, Verbose, TEXT("  Set %s = %f"), *FullName, Value);
}

void FHktVFXNiagaraBuilder::SetParticleParamVec3(
	UNiagaraSystem* System, int32 EmitterIndex,
	const FString& ModuleName, const FString& ParamName, FVector Value)
{
	const auto& EmitterHandles = System->GetEmitterHandles();
	if (!EmitterHandles.IsValidIndex(EmitterIndex)) return;

	FVersionedNiagaraEmitterData* EmitterData =
		EmitterHandles[EmitterIndex].GetEmitterData();
	if (!EmitterData) return;

	FString FullName = FString::Printf(TEXT("Constants.%s.%s.%s"),
		*EmitterHandles[EmitterIndex].GetName().ToString(),
		*ModuleName, *ParamName);

	FNiagaraVariable Var;
	Var.SetName(FName(*FullName));
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

void FHktVFXNiagaraBuilder::SetParticleParamVec4(
	UNiagaraSystem* System, int32 EmitterIndex,
	const FString& ModuleName, const FString& ParamName, FVector4 Value)
{
	const auto& EmitterHandles = System->GetEmitterHandles();
	if (!EmitterHandles.IsValidIndex(EmitterIndex)) return;

	FVersionedNiagaraEmitterData* EmitterData =
		EmitterHandles[EmitterIndex].GetEmitterData();
	if (!EmitterData) return;

	FString FullName = FString::Printf(TEXT("Constants.%s.%s.%s"),
		*EmitterHandles[EmitterIndex].GetName().ToString(),
		*ModuleName, *ParamName);

	FNiagaraVariable Var;
	Var.SetName(FName(*FullName));
	Var.SetType(FNiagaraTypeDefinition::GetVec4Def());

	for (UNiagaraScript* Script : {
		EmitterData->SpawnScriptProps.Script,
		EmitterData->UpdateScriptProps.Script })
	{
		if (Script)
		{
			Script->RapidIterationParameters.SetParameterValue<FVector4f>(
				FVector4f(Value), Var);
		}
	}
}

void FHktVFXNiagaraBuilder::SetParticleParamColor(
	UNiagaraSystem* System, int32 EmitterIndex,
	const FString& ModuleName, const FString& ParamName, FLinearColor Value)
{
	const auto& EmitterHandles = System->GetEmitterHandles();
	if (!EmitterHandles.IsValidIndex(EmitterIndex)) return;

	FVersionedNiagaraEmitterData* EmitterData =
		EmitterHandles[EmitterIndex].GetEmitterData();
	if (!EmitterData) return;

	FString FullName = FString::Printf(TEXT("Constants.%s.%s.%s"),
		*EmitterHandles[EmitterIndex].GetName().ToString(),
		*ModuleName, *ParamName);

	FNiagaraVariable Var;
	Var.SetName(FName(*FullName));
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

// ============================================================================
// Emitter-level 파라미터 (EmitterUpdateScript)
// ============================================================================

void FHktVFXNiagaraBuilder::SetEmitterParamFloat(
	UNiagaraSystem* System, int32 EmitterIndex,
	const FString& ModuleName, const FString& ParamName, float Value)
{
	const auto& EmitterHandles = System->GetEmitterHandles();
	if (!EmitterHandles.IsValidIndex(EmitterIndex)) return;

	FVersionedNiagaraEmitterData* EmitterData =
		EmitterHandles[EmitterIndex].GetEmitterData();
	if (!EmitterData) return;

	FString FullName = FString::Printf(TEXT("Constants.%s.%s.%s"),
		*EmitterHandles[EmitterIndex].GetName().ToString(),
		*ModuleName, *ParamName);

	FNiagaraVariable Var;
	Var.SetName(FName(*FullName));
	Var.SetType(FNiagaraTypeDefinition::GetFloatDef());

	UNiagaraScript* Script = EmitterData->EmitterUpdateScriptProps.Script;
	if (Script)
	{
		Script->RapidIterationParameters.SetParameterValue<float>(Value, Var);
	}

	UE_LOG(LogHktVFXBuilder, Verbose, TEXT("  SetEmitter %s = %f"), *FullName, Value);
}

void FHktVFXNiagaraBuilder::SetEmitterParamInt(
	UNiagaraSystem* System, int32 EmitterIndex,
	const FString& ModuleName, const FString& ParamName, int32 Value)
{
	const auto& EmitterHandles = System->GetEmitterHandles();
	if (!EmitterHandles.IsValidIndex(EmitterIndex)) return;

	FVersionedNiagaraEmitterData* EmitterData =
		EmitterHandles[EmitterIndex].GetEmitterData();
	if (!EmitterData) return;

	FString FullName = FString::Printf(TEXT("Constants.%s.%s.%s"),
		*EmitterHandles[EmitterIndex].GetName().ToString(),
		*ModuleName, *ParamName);

	FNiagaraVariable Var;
	Var.SetName(FName(*FullName));
	Var.SetType(FNiagaraTypeDefinition::GetIntDef());

	UNiagaraScript* Script = EmitterData->EmitterUpdateScriptProps.Script;
	if (Script)
	{
		Script->RapidIterationParameters.SetParameterValue<int32>(Value, Var);
	}

	UE_LOG(LogHktVFXBuilder, Verbose, TEXT("  SetEmitter %s = %d"), *FullName, Value);
}
