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
#include "NiagaraDataInterfaceSkeletalMesh.h"
#include "NiagaraDataInterfaceSpline.h"

#include "NiagaraSystemFactoryNew.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

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

	// 루프 설정 — 각 에미터의 LoopBehavior를 제어
	// UNiagaraSystem 자체에는 루프 플래그가 없고,
	// 각 에미터의 EmitterState 모듈에서 Loop Behavior를 설정해야 함.
	// 이 설정은 ConfigureEmitter에서 에미터별로 처리.
	UE_LOG(LogHktVFXBuilder, Log, TEXT("System properties: WarmupTime=%.2f, Looping=%d"),
		Config.WarmupTime, Config.bLooping);
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

	// 기존 RapidIterationParameters 로그 (디버그용)
	LogExistingParameters(System, ActualIndex);

	// 템플릿에 없는 모듈을 Config 요구에 따라 동적 주입
	EnsureRequiredModules(System, ActualIndex, Config.Update);

	// RapidIterationParameters 설정 (모듈이 없으면 무시됨)
	SetupSpawnModule(System, ActualIndex, Config.Spawn);
	SetupInitializeModule(System, ActualIndex, Config.Init);
	SetupUpdateModules(System, ActualIndex, Config.Update);
	SetupRenderer(System, ActualIndex, Config.Render);

	// 데이터 인터페이스 바인딩 (스켈레톤, 스태틱 메시, 스플라인 등)
	if (Config.DataInterfaces.Num() > 0)
	{
		SetupDataInterfaces(System, ActualIndex, Config.DataInterfaces);
	}

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

	// Velocity — Min/Max 평균 벡터 사용
	FVector AvgVelocity = (Config.VelocityMin + Config.VelocityMax) * 0.5f;
	if (!AvgVelocity.IsNearlyZero(1.f))
	{
		SetParticleParamVec3(System, EmitterIndex, Module, TEXT("Velocity"), AvgVelocity);
	}

	// Sprite Rotation — Min/Max 평균값 사용 (도 단위)
	float AvgRotation = (Config.SpriteRotationMin + Config.SpriteRotationMax) * 0.5f;
	if (AvgRotation != 0.f)
	{
		SetParticleParamFloat(System, EmitterIndex, Module, TEXT("Sprite Rotation Angle"), AvgRotation);
	}

	// Mass — Min/Max 평균값 사용
	float AvgMass = (Config.MassMin + Config.MassMax) * 0.5f;
	if (AvgMass != 1.f)
	{
		SetParticleParamFloat(System, EmitterIndex, Module, TEXT("Mass"), AvgMass);
	}
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

	// --- Wind Force 모듈 ---
	if (!Config.WindForce.IsNearlyZero(1.f))
	{
		SetParticleParamVec3(System, EmitterIndex,
			TEXT("Wind Force"), TEXT("Wind Velocity"), Config.WindForce);
	}

	// --- SolveForcesAndVelocity 모듈 ---
	// Speed Limit — 거의 모든 템플릿에 존재
	if (Config.SpeedLimit > 0.f)
	{
		SetParticleParamFloat(System, EmitterIndex,
			TEXT("SolveForcesAndVelocity"), TEXT("Speed Limit"), Config.SpeedLimit);
	}
	else if (Config.Drag > 0.f)
	{
		// Drag 기반 간접 속도 제한 (Speed Limit으로 폴백)
		float DerivedLimit = FMath::Max(100.f / Config.Drag, 10.f);
		SetParticleParamFloat(System, EmitterIndex,
			TEXT("SolveForcesAndVelocity"), TEXT("Speed Limit"), DerivedLimit);
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

		// Vortex 회전축 (기본 Z축이 아닌 경우)
		if (!(Config.VortexAxis - FVector(0, 0, 1)).IsNearlyZero(0.01f))
		{
			SetParticleParamVec3(System, EmitterIndex,
				TEXT("Vortex Velocity"), TEXT("Vortex Axis"), Config.VortexAxis);
		}
	}

	// --- Acceleration Force 모듈 ---
	// Gravity와 독립적인 일정 가속도 (예: 방사형 가속, 미사일 추진 등)
	if (!Config.AccelerationForce.IsNearlyZero(1.f))
	{
		SetParticleParamVec3(System, EmitterIndex,
			TEXT("Acceleration Force"), TEXT("Acceleration"), Config.AccelerationForce);
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

			// SubUV 플립북 설정
			if (Config.SubImageRows > 0 && Config.SubImageColumns > 0)
			{
				SR->SubImageSize = FVector2D(Config.SubImageColumns, Config.SubImageRows);
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

	UE_LOG(LogHktVFXBuilder, Log, TEXT("  Set %s = %f"), *FullName, Value);
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

	UE_LOG(LogHktVFXBuilder, Log, TEXT("  SetEmitter %s = %f"), *FullName, Value);
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

	UE_LOG(LogHktVFXBuilder, Log, TEXT("  SetEmitter %s = %d"), *FullName, Value);
}

// ============================================================================
// 동적 모듈 주입
// 템플릿에 없는 모듈을 Config 요구에 따라 에미터 그래프에 추가.
// 이미 존재하는 모듈은 중복 추가하지 않음.
// ============================================================================

bool FHktVFXNiagaraBuilder::AddModuleToEmitter(
	UNiagaraSystem* System, int32 EmitterIndex,
	ENiagaraScriptUsage ScriptUsage, const FString& ModuleScriptPath)
{
	const auto& Handles = System->GetEmitterHandles();
	if (!Handles.IsValidIndex(EmitterIndex)) return false;

	FVersionedNiagaraEmitterData* EmitterData = Handles[EmitterIndex].GetEmitterData();
	if (!EmitterData) return false;

	// 모듈 스크립트 로드
	UNiagaraScript* ModuleScript = LoadObject<UNiagaraScript>(nullptr, *ModuleScriptPath);
	if (!ModuleScript)
	{
		UE_LOG(LogHktVFXBuilder, Warning,
			TEXT("Module script not found (will rely on template): %s"), *ModuleScriptPath);
		return false;
	}

	// 대상 스크립트 선택
	UNiagaraScript* TargetScript = nullptr;
	switch (ScriptUsage)
	{
	case ENiagaraScriptUsage::ParticleUpdateScript:
		TargetScript = EmitterData->UpdateScriptProps.Script;
		break;
	case ENiagaraScriptUsage::ParticleSpawnScript:
		TargetScript = EmitterData->SpawnScriptProps.Script;
		break;
	case ENiagaraScriptUsage::EmitterUpdateScript:
		TargetScript = EmitterData->EmitterUpdateScriptProps.Script;
		break;
	default:
		return false;
	}
	if (!TargetScript) return false;

	// 공개 API로 Output 노드 획득
	UNiagaraNodeOutput* OutputNode = FNiagaraEditorUtilities::GetScriptOutputNode(*TargetScript);
	if (!OutputNode)
	{
		UE_LOG(LogHktVFXBuilder, Warning,
			TEXT("No output node for script usage %d: %s"), (int32)ScriptUsage, *ModuleScriptPath);
		return false;
	}

	// 이미 같은 모듈이 있는지 확인 (그래프 노드 순회)
	UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(TargetScript->GetLatestSource());
	if (ScriptSource && ScriptSource->NodeGraph)
	{
		for (UEdGraphNode* Node : ScriptSource->NodeGraph->Nodes)
		{
			if (UNiagaraNodeFunctionCall* FuncNode = Cast<UNiagaraNodeFunctionCall>(Node))
			{
				if (FuncNode->FunctionScript == ModuleScript)
				{
					UE_LOG(LogHktVFXBuilder, Log,
						TEXT("Module already exists: %s"), *ModuleScriptPath);
					return true;
				}
			}
		}
	}

	// 공개 API로 모듈을 스택 끝에 추가
	UNiagaraNodeFunctionCall* NewNode = FNiagaraStackGraphUtilities::AddScriptModuleToStack(
		ModuleScript, *OutputNode, INDEX_NONE);

	if (!NewNode)
	{
		UE_LOG(LogHktVFXBuilder, Warning,
			TEXT("Failed to inject module via StackGraphUtilities: %s"), *ModuleScriptPath);
		return false;
	}

	UE_LOG(LogHktVFXBuilder, Log, TEXT("Injected module '%s' into emitter %d"),
		*ModuleScriptPath, EmitterIndex);
	return true;
}

// ============================================================================
// Config 요구사항에 따라 누락된 모듈 주입
// ============================================================================

void FHktVFXNiagaraBuilder::EnsureRequiredModules(
	UNiagaraSystem* System, int32 EmitterIndex,
	const FHktVFXEmitterUpdateConfig& Config)
{
	const UHktVFXGeneratorSettings* Settings = UHktVFXGeneratorSettings::Get();

	auto TryInject = [&](const FString& ModuleKey, bool bNeeded)
	{
		if (!bNeeded) return;
		if (const FSoftObjectPath* Path = Settings->ModuleScriptPaths.Find(ModuleKey))
		{
			if (Path->IsValid())
			{
				AddModuleToEmitter(System, EmitterIndex,
					ENiagaraScriptUsage::ParticleUpdateScript,
					Path->GetAssetPathString());
			}
		}
	};

	// 힘/물리 모듈
	TryInject(TEXT("GravityForce"), !Config.Gravity.IsNearlyZero(1.f));
	TryInject(TEXT("Drag"), Config.Drag > 0.f);
	TryInject(TEXT("CurlNoiseForce"), Config.NoiseStrength > 0.f);
	TryInject(TEXT("VortexVelocity"), Config.VortexStrength > 0.f);
	TryInject(TEXT("PointAttractionForce"), Config.AttractionStrength > 0.f);
	TryInject(TEXT("WindForce"), !Config.WindForce.IsNearlyZero(1.f));
	TryInject(TEXT("AccelerationForce"), !Config.AccelerationForce.IsNearlyZero(1.f));

	// 크기/회전 모듈 (비기본값일 때)
	TryInject(TEXT("SpriteRotationRate"),
		Config.RotationRateMin != 0.f || Config.RotationRateMax != 0.f);
	TryInject(TEXT("ScaleSpriteSize"),
		Config.SizeScaleStart != 1.f || Config.SizeScaleEnd != 1.f);
	TryInject(TEXT("ScaleColor"),
		Config.OpacityStart != 1.f || Config.OpacityEnd != 0.f || Config.bUseColorOverLife);
}

// ============================================================================
// 데이터 인터페이스 설정
// System-level User Parameter로 DI를 등록하여 런타임에 바인딩 가능하게 함.
// 에미터 모듈(SampleSkeletalMesh 등)이 이 User Parameter를 참조.
// ============================================================================

void FHktVFXNiagaraBuilder::SetupDataInterfaces(
	UNiagaraSystem* System, int32 EmitterIndex,
	const TArray<FHktVFXDataInterfaceBinding>& DataInterfaces)
{
	for (const FHktVFXDataInterfaceBinding& DI : DataInterfaces)
	{
		if (DI.Type.IsEmpty() || DI.ParameterName.IsEmpty())
		{
			UE_LOG(LogHktVFXBuilder, Warning, TEXT("DataInterface binding missing type or parameterName"));
			continue;
		}

		// DI 타입에 따라 적절한 UNiagaraDataInterface 서브클래스 생성
		UNiagaraDataInterface* NewDI = nullptr;
		FNiagaraTypeDefinition TypeDef;

		if (DI.Type == TEXT("skeletal_mesh"))
		{
			UNiagaraDataInterfaceSkeletalMesh* SkelDI =
				NewObject<UNiagaraDataInterfaceSkeletalMesh>(System);
			// FilteredBones / FilteredSockets 설정
			for (const FString& FilterName : DI.FilterNames)
			{
				SkelDI->FilteredBones.Add(FName(*FilterName));
			}
			NewDI = SkelDI;
			TypeDef = FNiagaraTypeDefinition(UNiagaraDataInterfaceSkeletalMesh::StaticClass());
		}
		else if (DI.Type == TEXT("spline"))
		{
			NewDI = NewObject<UNiagaraDataInterfaceSpline>(System);
			TypeDef = FNiagaraTypeDefinition(UNiagaraDataInterfaceSpline::StaticClass());
		}
		else
		{
			UE_LOG(LogHktVFXBuilder, Warning, TEXT("Unknown DataInterface type: %s"), *DI.Type);
			continue;
		}

		if (!NewDI)
		{
			UE_LOG(LogHktVFXBuilder, Warning,
				TEXT("Failed to create DataInterface for type: %s"), *DI.Type);
			continue;
		}

		// System의 ExposedParameters(User Parameter)에 DI를 등록
		// 이름 형식: "User.{ParameterName}"
		FString UserParamName = FString::Printf(TEXT("User.%s"), *DI.ParameterName);
		FNiagaraVariable UserVar(TypeDef, FName(*UserParamName));

		System->GetExposedParameters().AddParameter(UserVar, true);
		System->GetExposedParameters().SetDataInterface(NewDI, UserVar);

		UE_LOG(LogHktVFXBuilder, Log,
			TEXT("Added DataInterface User Parameter: %s (type=%s)"),
			*UserParamName, *DI.Type);

		// 타입별 모듈 자동 주입
		const UHktVFXGeneratorSettings* Settings = UHktVFXGeneratorSettings::Get();

		// skeletal_mesh의 경우 SampleSkeletalMesh / InitializeMeshReproduction 등의 모듈 주입이 필요.
		// 이 모듈들은 EnsureRequiredModules와 동일한 방식으로 TryInject 가능.
		// 단, 이 모듈들은 Spawn 스크립트에 추가되어야 하므로 별도 처리.
		if (DI.Type == TEXT("skeletal_mesh"))
		{
			// Initialize Mesh Reproduction Sprite — 메시 표면에서 파티클 위치 초기화
			if (const FSoftObjectPath* Path = Settings->ModuleScriptPaths.Find(TEXT("InitializeMeshReproductionSprite")))
			{
				if (Path->IsValid())
				{
					AddModuleToEmitter(System, EmitterIndex,
						ENiagaraScriptUsage::ParticleSpawnScript,
						Path->GetAssetPathString());
				}
			}

			// Sample Skeletal Mesh — 업데이트 시 메시 위치 추적
			if (const FSoftObjectPath* Path = Settings->ModuleScriptPaths.Find(TEXT("SampleSkeletalMesh")))
			{
				if (Path->IsValid())
				{
					AddModuleToEmitter(System, EmitterIndex,
						ENiagaraScriptUsage::ParticleUpdateScript,
						Path->GetAssetPathString());
				}
			}
		}
		else if (DI.Type == TEXT("spline"))
		{
			// SampleSpline — 스플라인 위의 위치를 샘플링하여 파티클 배치
			if (const FSoftObjectPath* Path = Settings->ModuleScriptPaths.Find(TEXT("SampleSpline")))
			{
				if (Path->IsValid())
				{
					AddModuleToEmitter(System, EmitterIndex,
						ENiagaraScriptUsage::ParticleSpawnScript,
						Path->GetAssetPathString());
				}
			}
		}
	}
}

// ============================================================================
// 디버그: 에미터의 기존 RapidIterationParameter 이름을 모두 출력
// 템플릿이 실제로 어떤 파라미터 이름을 사용하는지 확인용.
// ============================================================================

void FHktVFXNiagaraBuilder::LogExistingParameters(
	UNiagaraSystem* System, int32 EmitterIndex)
{
	const auto& EmitterHandles = System->GetEmitterHandles();
	if (!EmitterHandles.IsValidIndex(EmitterIndex)) return;

	FVersionedNiagaraEmitterData* EmitterData = EmitterHandles[EmitterIndex].GetEmitterData();
	if (!EmitterData) return;

	FString HandleName = EmitterHandles[EmitterIndex].GetName().ToString();

	auto LogParams = [&](const TCHAR* ScriptLabel, UNiagaraScript* Script)
	{
		if (!Script) return;

		TArray<FNiagaraVariable> Params;
		Script->RapidIterationParameters.GetParameters(Params);

		if (Params.Num() == 0)
		{
			UE_LOG(LogHktVFXBuilder, Log, TEXT("  [%s] %s: (no RI parameters)"),
				*HandleName, ScriptLabel);
			return;
		}

		UE_LOG(LogHktVFXBuilder, Log, TEXT("  [%s] %s: %d RI parameters"),
			*HandleName, ScriptLabel, Params.Num());
		for (const FNiagaraVariable& Param : Params)
		{
			UE_LOG(LogHktVFXBuilder, Log, TEXT("    - [%s] %s"),
				*Param.GetType().GetName(), *Param.GetName().ToString());
		}
	};

	LogParams(TEXT("SpawnScript"), EmitterData->SpawnScriptProps.Script);
	LogParams(TEXT("UpdateScript"), EmitterData->UpdateScriptProps.Script);
	LogParams(TEXT("EmitterUpdateScript"), EmitterData->EmitterUpdateScriptProps.Script);
}
