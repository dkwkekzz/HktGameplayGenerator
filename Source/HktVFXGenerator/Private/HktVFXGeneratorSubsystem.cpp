// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVFXGeneratorSubsystem.h"
#include "HktVFXGeneratorSettings.h"
#include "HktVFXGeneratorHandler.h"
#include "HktGeneratorRouter.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "NiagaraParameterStore.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraComponent.h"

#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Engine/Texture2D.h"

#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "HighResScreenshot.h"
#include "ImageUtils.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktVFXGenerator, Log, All);

void UHktVFXGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Handler 생성 및 Router 등록
	if (UHktGeneratorRouter* Router = GEditor->GetEditorSubsystem<UHktGeneratorRouter>())
	{
		VFXHandler = NewObject<UHktVFXGeneratorHandler>(this);
		Router->RegisterHandler(TScriptInterface<IHktGeneratorHandler>(VFXHandler));
		UE_LOG(LogHktVFXGenerator, Log, TEXT("VFXGeneratorHandler registered with Router"));
	}

	const UHktVFXGeneratorSettings* Settings = UHktVFXGeneratorSettings::Get();
	UE_LOG(LogHktVFXGenerator, Log, TEXT("HktVFXGeneratorSubsystem Initialized (templates: %d, outputDir: %s)"),
		Settings->EmitterTemplates.Num(), *Settings->DefaultOutputDirectory);
}

void UHktVFXGeneratorSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

FString UHktVFXGeneratorSubsystem::ResolveOutputDir(const FString& OutputDir) const
{
	if (!OutputDir.IsEmpty())
	{
		return OutputDir;
	}
	return UHktVFXGeneratorSettings::Get()->DefaultOutputDirectory;
}

UNiagaraSystem* UHktVFXGeneratorSubsystem::BuildNiagaraFromConfig(
	const FHktVFXNiagaraConfig& Config,
	const FString& OutputDir)
{
	const FString ResolvedDir = ResolveOutputDir(OutputDir);
	UE_LOG(LogHktVFXGenerator, Log, TEXT("BuildNiagaraFromConfig: %s (%d emitters) -> %s"),
		*Config.SystemName, Config.Emitters.Num(), *ResolvedDir);

	UNiagaraSystem* Result = Builder.BuildNiagaraSystem(Config, ResolvedDir);

	if (Result)
	{
		UE_LOG(LogHktVFXGenerator, Log, TEXT("Successfully built: %s"), *Result->GetPathName());
	}
	else
	{
		UE_LOG(LogHktVFXGenerator, Error, TEXT("Failed to build NiagaraSystem from config"));
	}

	return Result;
}

UNiagaraSystem* UHktVFXGeneratorSubsystem::BuildNiagaraFromJson(
	const FString& JsonStr,
	const FString& OutputDir)
{
	FHktVFXNiagaraConfig Config;
	if (!FHktVFXNiagaraConfig::FromJson(JsonStr, Config))
	{
		UE_LOG(LogHktVFXGenerator, Error, TEXT("Failed to parse JSON config"));
		return nullptr;
	}

	return BuildNiagaraFromConfig(Config, OutputDir);
}

UNiagaraSystem* UHktVFXGeneratorSubsystem::BuildPresetExplosion(
	const FString& Name,
	FLinearColor Color,
	float Intensity,
	const FString& OutputDir)
{
	Intensity = FMath::Clamp(Intensity, 0.0f, 1.0f);

	FHktVFXNiagaraConfig Config;
	Config.SystemName = Name;

	// === 1. Core Flash — NE_Core 템플릿 (중심부 강렬한 섬광) ===
	{
		FHktVFXEmitterConfig E;
		E.Name = TEXT("CoreFlash");
		E.Spawn.Mode = TEXT("burst");
		E.Spawn.BurstCount = FMath::RoundToInt(FMath::Lerp(3.f, 8.f, Intensity));

		E.Init.LifetimeMin = 0.05f;
		E.Init.LifetimeMax = FMath::Lerp(0.15f, 0.3f, Intensity);
		E.Init.SizeMin = FMath::Lerp(50.f, 150.f, Intensity);
		E.Init.SizeMax = FMath::Lerp(100.f, 300.f, Intensity);
		E.Init.VelocityMin = FVector(-50.f, -50.f, -50.f);
		E.Init.VelocityMax = FVector(50.f, 50.f, 50.f);
		E.Init.Color = FLinearColor(
			FMath::Min(Color.R * 3.f, 10.f),
			FMath::Min(Color.G * 3.f, 10.f),
			FMath::Min(Color.B * 3.f, 10.f),
			1.f);

		E.Update.Gravity = FVector::ZeroVector;
		E.Update.SizeScaleStart = 0.3f;
		E.Update.SizeScaleEnd = 2.0f;
		E.Update.OpacityStart = 1.f;
		E.Update.OpacityEnd = 0.f;

		E.Render.EmitterTemplate = TEXT("core");
		E.Render.SortOrder = 10;

		Config.Emitters.Add(MoveTemp(E));
	}

	// === 2. Explosion Burst — NE_Explosion 템플릿 (SubUV 폭발) ===
	{
		FHktVFXEmitterConfig E;
		E.Name = TEXT("ExplosionBurst");
		E.Spawn.Mode = TEXT("burst");
		E.Spawn.BurstCount = FMath::RoundToInt(FMath::Lerp(3.f, 8.f, Intensity));

		E.Init.LifetimeMin = FMath::Lerp(0.2f, 0.4f, Intensity);
		E.Init.LifetimeMax = FMath::Lerp(0.5f, 1.0f, Intensity);
		E.Init.SizeMin = FMath::Lerp(40.f, 100.f, Intensity);
		E.Init.SizeMax = FMath::Lerp(80.f, 200.f, Intensity);
		E.Init.Color = FLinearColor(
			FMath::Min(Color.R * 2.f, 5.f),
			FMath::Min(Color.G * 2.f, 5.f),
			FMath::Min(Color.B * 1.f, 3.f),
			1.f);

		E.Update.OpacityEnd = 0.f;

		E.Render.EmitterTemplate = TEXT("explosion");
		E.Render.SortOrder = 8;

		Config.Emitters.Add(MoveTemp(E));
	}

	// === 3. Main Sparks — NE_Sparks 템플릿 (Gravity+Drag 포함) ===
	{
		FHktVFXEmitterConfig E;
		E.Name = TEXT("MainSparks");
		E.Spawn.Mode = TEXT("burst");
		E.Spawn.BurstCount = FMath::RoundToInt(FMath::Lerp(30.f, 150.f, Intensity));

		E.Init.LifetimeMin = FMath::Lerp(0.3f, 0.5f, Intensity);
		E.Init.LifetimeMax = FMath::Lerp(0.8f, 2.0f, Intensity);
		E.Init.SizeMin = FMath::Lerp(3.f, 8.f, Intensity);
		E.Init.SizeMax = FMath::Lerp(8.f, 25.f, Intensity);

		float Speed = FMath::Lerp(300.f, 1000.f, Intensity);
		E.Init.VelocityMin = FVector(-Speed, -Speed, -Speed * 0.3f);
		E.Init.VelocityMax = FVector(Speed, Speed, Speed * 1.2f);
		E.Init.Color = Color;

		E.Update.Gravity = FVector(0.f, 0.f, FMath::Lerp(-490.f, -980.f, Intensity));
		E.Update.Drag = FMath::Lerp(1.0f, 3.0f, Intensity);
		E.Update.OpacityEnd = 0.f;
		E.Update.bUseColorOverLife = true;
		E.Update.ColorEnd = FLinearColor(Color.R * 0.3f, Color.G * 0.1f, 0.f, 1.f);

		E.Render.EmitterTemplate = TEXT("spark");
		E.Render.SortOrder = 5;

		Config.Emitters.Add(MoveTemp(E));
	}

	// === 4. Smoke — NE_Smoke 템플릿 (Noise+Rotation 내장) ===
	if (Intensity > 0.3f)
	{
		FHktVFXEmitterConfig E;
		E.Name = TEXT("Smoke");
		E.Spawn.Mode = TEXT("burst");
		E.Spawn.BurstCount = FMath::RoundToInt(FMath::Lerp(5.f, 30.f, Intensity));
		E.Spawn.BurstDelay = 0.05f;

		E.Init.LifetimeMin = FMath::Lerp(0.5f, 1.0f, Intensity);
		E.Init.LifetimeMax = FMath::Lerp(1.5f, 4.0f, Intensity);
		E.Init.SizeMin = FMath::Lerp(30.f, 80.f, Intensity);
		E.Init.SizeMax = FMath::Lerp(60.f, 200.f, Intensity);

		float SmokeSpeed = FMath::Lerp(50.f, 200.f, Intensity);
		E.Init.VelocityMin = FVector(-SmokeSpeed, -SmokeSpeed, 0.f);
		E.Init.VelocityMax = FVector(SmokeSpeed, SmokeSpeed, SmokeSpeed * 1.5f);
		E.Init.Color = FLinearColor(0.15f, 0.12f, 0.1f, 0.6f);

		E.Update.Gravity = FVector(0.f, 0.f, 50.f);
		E.Update.Drag = 2.0f;
		E.Update.SizeScaleStart = 0.5f;
		E.Update.SizeScaleEnd = 3.0f;
		E.Update.OpacityStart = 0.6f;
		E.Update.OpacityEnd = 0.f;

		E.Render.EmitterTemplate = TEXT("smoke");
		E.Render.SortOrder = 0;

		Config.Emitters.Add(MoveTemp(E));
	}

	// === 5. Debris — NE_Debris 템플릿 (중력 파편) ===
	if (Intensity > 0.4f)
	{
		FHktVFXEmitterConfig E;
		E.Name = TEXT("Debris");
		E.Spawn.Mode = TEXT("burst");
		E.Spawn.BurstCount = FMath::RoundToInt(FMath::Lerp(5.f, 20.f, Intensity));

		E.Init.LifetimeMin = FMath::Lerp(0.5f, 1.0f, Intensity);
		E.Init.LifetimeMax = FMath::Lerp(1.5f, 3.0f, Intensity);
		E.Init.SizeMin = FMath::Lerp(3.f, 10.f, Intensity);
		E.Init.SizeMax = FMath::Lerp(10.f, 30.f, Intensity);

		float DebrisSpeed = FMath::Lerp(200.f, 600.f, Intensity);
		E.Init.VelocityMin = FVector(-DebrisSpeed, -DebrisSpeed, DebrisSpeed * 0.2f);
		E.Init.VelocityMax = FVector(DebrisSpeed, DebrisSpeed, DebrisSpeed * 1.0f);
		E.Init.Color = FLinearColor(Color.R * 0.4f, Color.G * 0.3f, Color.B * 0.2f, 1.f);

		E.Update.Gravity = FVector(0.f, 0.f, -980.f);
		E.Update.Drag = FMath::Lerp(0.5f, 2.0f, Intensity);

		E.Render.EmitterTemplate = TEXT("debris");
		E.Render.SortOrder = 3;

		Config.Emitters.Add(MoveTemp(E));
	}

	// === 6. Ground Dust — NE_GroundDust 템플릿 ===
	if (Intensity > 0.3f)
	{
		FHktVFXEmitterConfig E;
		E.Name = TEXT("GroundDust");
		E.Spawn.Mode = TEXT("burst");
		E.Spawn.BurstCount = FMath::RoundToInt(FMath::Lerp(3.f, 15.f, Intensity));
		E.Spawn.BurstDelay = 0.02f;

		E.Init.LifetimeMin = FMath::Lerp(0.8f, 1.5f, Intensity);
		E.Init.LifetimeMax = FMath::Lerp(2.0f, 4.0f, Intensity);
		E.Init.SizeMin = FMath::Lerp(20.f, 50.f, Intensity);
		E.Init.SizeMax = FMath::Lerp(50.f, 120.f, Intensity);
		E.Init.Color = FLinearColor(0.2f, 0.18f, 0.15f, 0.4f);

		E.Update.SizeScaleEnd = 3.0f;
		E.Update.OpacityEnd = 0.f;

		E.Render.EmitterTemplate = TEXT("ground_dust");
		E.Render.SortOrder = 1;

		Config.Emitters.Add(MoveTemp(E));
	}

	// === 7. Dynamic Light — 동적 조명 ===
	if (Intensity > 0.2f)
	{
		FHktVFXEmitterConfig E;
		E.Name = TEXT("FlashLight");
		E.Spawn.Mode = TEXT("burst");
		E.Spawn.BurstCount = 1;

		E.Init.LifetimeMin = 0.1f;
		E.Init.LifetimeMax = FMath::Lerp(0.3f, 1.0f, Intensity);
		E.Init.Color = FLinearColor(
			FMath::Min(Color.R * 2.f, 5.f),
			FMath::Min(Color.G * 2.f, 5.f),
			FMath::Min(Color.B * 2.f, 5.f),
			1.f);

		E.Update.Gravity = FVector::ZeroVector;

		E.Render.RendererType = TEXT("light");
		E.Render.LightRadiusScale = FMath::Lerp(2.f, 8.f, Intensity);
		E.Render.LightIntensity = FMath::Lerp(1.f, 5.f, Intensity);
		E.Render.SortOrder = 15;

		Config.Emitters.Add(MoveTemp(E));
	}

	UE_LOG(LogHktVFXGenerator, Log, TEXT("BuildPresetExplosion: %s (Intensity=%.2f, %d emitters)"),
		*Name, Intensity, Config.Emitters.Num());

	return BuildNiagaraFromConfig(Config, OutputDir);
}

FString UHktVFXGeneratorSubsystem::GetConfigSchemaJson() const
{
	return FHktVFXNiagaraConfig::GetSchemaJson();
}

FString UHktVFXGeneratorSubsystem::DumpTemplateParameters(const FString& RendererType)
{
	// 템플릿 로드
	const UHktVFXGeneratorSettings* Settings = UHktVFXGeneratorSettings::Get();
	UNiagaraEmitter* TemplateEmitter = nullptr;

	if (const FSoftObjectPath* Path = Settings->EmitterTemplates.Find(RendererType))
	{
		if (Path->IsValid())
		{
			TemplateEmitter = Cast<UNiagaraEmitter>(Path->TryLoad());
		}
	}

	if (!TemplateEmitter)
	{
		return FString::Printf(TEXT("ERROR: No template for type '%s'"), *RendererType);
	}

	FString Result;
	Result += FString::Printf(TEXT("=== Template Parameters for '%s' ===\n"), *RendererType);
	Result += FString::Printf(TEXT("Template: %s\n\n"), *TemplateEmitter->GetPathName());

	// 에미터 데이터 접근
	FVersionedNiagaraEmitterData* EmitterData = TemplateEmitter->GetLatestEmitterData();
	if (!EmitterData)
	{
		return Result + TEXT("ERROR: No emitter data\n");
	}

	// 각 스크립트의 RapidIterationParameters 덤프
	auto DumpScript = [&](const TCHAR* ScriptName, UNiagaraScript* Script)
	{
		if (!Script) return;

		Result += FString::Printf(TEXT("--- %s ---\n"), ScriptName);

		TArray<FNiagaraVariable> Params;
		Script->RapidIterationParameters.GetParameters(Params);

		if (Params.Num() == 0)
		{
			Result += TEXT("  (no parameters)\n");
		}

		for (const FNiagaraVariable& Param : Params)
		{
			Result += FString::Printf(TEXT("  [%s] %s\n"),
				*Param.GetType().GetName(), *Param.GetName().ToString());
		}
		Result += TEXT("\n");
	};

	DumpScript(TEXT("SpawnScript"), EmitterData->SpawnScriptProps.Script);
	DumpScript(TEXT("UpdateScript"), EmitterData->UpdateScriptProps.Script);

	// EmitterSpawn/Update 스크립트도 확인
	DumpScript(TEXT("EmitterSpawnScript"), EmitterData->EmitterSpawnScriptProps.Script);
	DumpScript(TEXT("EmitterUpdateScript"), EmitterData->EmitterUpdateScriptProps.Script);

	return Result;
}

FString UHktVFXGeneratorSubsystem::DumpAllTemplateParameters()
{
	const UHktVFXGeneratorSettings* Settings = UHktVFXGeneratorSettings::Get();
	FString Result;

	Result += TEXT("=== All Template Emitter Parameters ===\n\n");

	for (const auto& Pair : Settings->EmitterTemplates)
	{
		const FString& Key = Pair.Key;
		const FSoftObjectPath& Path = Pair.Value;

		Result += FString::Printf(TEXT("### Template: '%s' ###\n"), *Key);
		Result += FString::Printf(TEXT("Path: %s\n"), *Path.GetAssetPathString());

		if (!Path.IsValid())
		{
			Result += TEXT("  (invalid path)\n\n");
			continue;
		}

		UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(Path.TryLoad());
		if (!Emitter)
		{
			Result += TEXT("  (asset not found)\n\n");
			continue;
		}

		FVersionedNiagaraEmitterData* EmitterData = Emitter->GetLatestEmitterData();
		if (!EmitterData)
		{
			Result += TEXT("  (no emitter data)\n\n");
			continue;
		}

		auto DumpScript = [&](const TCHAR* ScriptName, UNiagaraScript* Script)
		{
			if (!Script) return;

			TArray<FNiagaraVariable> Params;
			Script->RapidIterationParameters.GetParameters(Params);

			if (Params.Num() == 0) return;

			Result += FString::Printf(TEXT("  [%s]\n"), ScriptName);
			for (const FNiagaraVariable& Param : Params)
			{
				Result += FString::Printf(TEXT("    %s : %s\n"),
					*Param.GetName().ToString(), *Param.GetType().GetName());
			}
		};

		DumpScript(TEXT("SpawnScript"), EmitterData->SpawnScriptProps.Script);
		DumpScript(TEXT("UpdateScript"), EmitterData->UpdateScriptProps.Script);
		DumpScript(TEXT("EmitterSpawnScript"), EmitterData->EmitterSpawnScriptProps.Script);
		DumpScript(TEXT("EmitterUpdateScript"), EmitterData->EmitterUpdateScriptProps.Script);

		// Renderer 정보
		for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
		{
			Result += FString::Printf(TEXT("  [Renderer] %s\n"), *Renderer->GetClass()->GetName());
		}

		Result += TEXT("\n");
	}

	return Result;
}

// ============================================================================
// Phase 2: 머티리얼 동적 생성
// ============================================================================

FString UHktVFXGeneratorSubsystem::CreateParticleMaterial(
	const FString& MaterialName,
	const FString& TexturePath,
	const FString& BlendMode,
	float EmissiveIntensity,
	const FString& OutputDir)
{
	const FString ResolvedDir = ResolveOutputDir(OutputDir);
	const FString MaterialDir = ResolvedDir / TEXT("Materials");

	// 마스터 머티리얼 로드
	const UHktVFXGeneratorSettings* Settings = UHktVFXGeneratorSettings::Get();
	const FSoftObjectPath& MasterMatPath = (BlendMode == TEXT("translucent"))
		? Settings->TranslucentMaterial
		: Settings->AdditiveMaterial;

	UMaterialInterface* MasterMat = nullptr;
	if (MasterMatPath.IsValid())
	{
		MasterMat = Cast<UMaterialInterface>(MasterMatPath.TryLoad());
	}

	if (!MasterMat)
	{
		UE_LOG(LogHktVFXGenerator, Error,
			TEXT("CreateParticleMaterial: Master material not found for BlendMode '%s'"), *BlendMode);
		return FString();
	}

	// MI 패키지 생성
	const FString MIName = FString::Printf(TEXT("MI_%s"), *MaterialName);
	const FString PackagePath = MaterialDir / MIName;

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE_LOG(LogHktVFXGenerator, Error, TEXT("Failed to create package: %s"), *PackagePath);
		return FString();
	}
	Package->FullyLoad();

	// MaterialInstanceConstant 생성
	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	Factory->InitialParent = MasterMat;

	UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(
		Factory->FactoryCreateNew(
			UMaterialInstanceConstant::StaticClass(),
			Package, *MIName,
			RF_Public | RF_Standalone,
			nullptr, GWarn));

	if (!MIC)
	{
		UE_LOG(LogHktVFXGenerator, Error, TEXT("Failed to create MaterialInstanceConstant"));
		return FString();
	}

	// 텍스처 바인딩 (TexturePath가 유효한 경우)
	if (!TexturePath.IsEmpty())
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *TexturePath);
		if (Texture)
		{
			static const FName TextureParamNames[] = {
				TEXT("BaseTexture"), TEXT("ParticleTexture"), TEXT("Texture"),
				TEXT("BaseColor"), TEXT("DiffuseTexture")
			};

			bool bBound = false;
			for (const FName& ParamName : TextureParamNames)
			{
				UTexture* Dummy = nullptr;
				if (MasterMat->GetTextureParameterValue(ParamName, Dummy))
				{
					MIC->SetTextureParameterValueEditorOnly(ParamName, Texture);
					bBound = true;
					UE_LOG(LogHktVFXGenerator, Log,
						TEXT("Bound texture to parameter '%s'"), *ParamName.ToString());
					break;
				}
			}

			if (!bBound)
			{
				UE_LOG(LogHktVFXGenerator, Warning,
					TEXT("Could not find matching texture parameter in master material"));
			}
		}
		else
		{
			UE_LOG(LogHktVFXGenerator, Warning, TEXT("Texture not found: %s"), *TexturePath);
		}
	}

	// Emissive Intensity 설정
	if (!FMath::IsNearlyEqual(EmissiveIntensity, 1.f))
	{
		static const FName EmissiveParamNames[] = {
			TEXT("EmissiveIntensity"), TEXT("EmissivePower"), TEXT("Intensity"),
			TEXT("EmissiveMultiplier"), TEXT("Brightness")
		};

		for (const FName& ParamName : EmissiveParamNames)
		{
			float Dummy = 0.f;
			if (MasterMat->GetScalarParameterValue(ParamName, Dummy))
			{
				MIC->SetScalarParameterValueEditorOnly(ParamName, EmissiveIntensity);
				UE_LOG(LogHktVFXGenerator, Log,
					TEXT("Set %s = %.2f"), *ParamName.ToString(), EmissiveIntensity);
				break;
			}
		}
	}

	// 저장
	MIC->MarkPackageDirty();
	FString PackageFileName = FPackageName::LongPackageNameToFilename(
		PackagePath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, MIC, *PackageFileName, SaveArgs);

	FAssetRegistryModule::AssetCreated(MIC);

	UE_LOG(LogHktVFXGenerator, Log, TEXT("Created particle material: %s"), *MIC->GetPathName());
	return MIC->GetPathName();
}

bool UHktVFXGeneratorSubsystem::AssignMaterialToEmitter(
	const FString& NiagaraSystemPath,
	const FString& EmitterName,
	const FString& MaterialPath)
{
	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *NiagaraSystemPath);
	if (!System)
	{
		UE_LOG(LogHktVFXGenerator, Error, TEXT("NiagaraSystem not found: %s"), *NiagaraSystemPath);
		return false;
	}

	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
	if (!Material)
	{
		UE_LOG(LogHktVFXGenerator, Error, TEXT("Material not found: %s"), *MaterialPath);
		return false;
	}

	// 에미터 찾기
	const TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
	int32 EmitterIndex = INDEX_NONE;
	for (int32 i = 0; i < Handles.Num(); ++i)
	{
		if (Handles[i].GetName().ToString() == EmitterName)
		{
			EmitterIndex = i;
			break;
		}
	}

	if (EmitterIndex == INDEX_NONE)
	{
		UE_LOG(LogHktVFXGenerator, Error, TEXT("Emitter '%s' not found in system"), *EmitterName);
		return false;
	}

	FVersionedNiagaraEmitterData* EmitterData = Handles[EmitterIndex].GetEmitterData();
	if (!EmitterData)
	{
		return false;
	}

	// 모든 렌더러에 머티리얼 적용
	bool bApplied = false;
	for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
	{
		if (UNiagaraSpriteRendererProperties* SR = Cast<UNiagaraSpriteRendererProperties>(Renderer))
		{
			SR->Material = Material;
			bApplied = true;
		}
		else if (UNiagaraRibbonRendererProperties* RR = Cast<UNiagaraRibbonRendererProperties>(Renderer))
		{
			RR->Material = Material;
			bApplied = true;
		}
	}

	if (bApplied)
	{
		System->RequestCompile(false);
		System->MarkPackageDirty();

		FString PackageFileName = FPackageName::LongPackageNameToFilename(
			System->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(System->GetOutermost(), System, *PackageFileName, SaveArgs);
	}

	return bApplied;
}

// ============================================================================
// Phase 4: 프리뷰 / 튜닝
// ============================================================================

FString UHktVFXGeneratorSubsystem::PreviewVFX(
	const FString& NiagaraSystemPath,
	float Duration,
	const FString& ScreenshotPath)
{
	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *NiagaraSystemPath);
	if (!System)
	{
		UE_LOG(LogHktVFXGenerator, Error, TEXT("NiagaraSystem not found: %s"), *NiagaraSystemPath);
		return FString();
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		UE_LOG(LogHktVFXGenerator, Error, TEXT("No editor world available"));
		return FString();
	}

	// 카메라 위치 기준 전방에 스폰
	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;

	if (GCurrentLevelEditingViewportClient)
	{
		const FVector CameraLocation = GCurrentLevelEditingViewportClient->GetViewLocation();
		const FRotator CameraRotation = GCurrentLevelEditingViewportClient->GetViewRotation();
		SpawnLocation = CameraLocation + CameraRotation.Vector() * 500.f;
	}

	// 임시 액터에 NiagaraComponent 부착
	AActor* TempActor = World->SpawnActor<AActor>(AActor::StaticClass(), SpawnLocation, SpawnRotation);
	if (!TempActor)
	{
		UE_LOG(LogHktVFXGenerator, Error, TEXT("Failed to spawn preview actor"));
		return FString();
	}

	UNiagaraComponent* NiagaraComp = NewObject<UNiagaraComponent>(TempActor);
	NiagaraComp->SetAsset(System);
	NiagaraComp->RegisterComponentWithWorld(World);
	NiagaraComp->AttachToComponent(TempActor->GetRootComponent(),
		FAttachmentTransformRules::KeepRelativeTransform);
	NiagaraComp->Activate(true);

	// 스크린샷 경로 결정
	FString FinalScreenshotPath = ScreenshotPath;
	if (FinalScreenshotPath.IsEmpty())
	{
		const FString ScreenshotDir = FPaths::ProjectSavedDir() / TEXT("VFXPreviews");
		IFileManager::Get().MakeDirectory(*ScreenshotDir, true);
		FinalScreenshotPath = ScreenshotDir / FString::Printf(
			TEXT("VFXPreview_%s_%s.png"),
			*FPaths::GetBaseFilename(NiagaraSystemPath),
			*FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	}

	// 워밍업 후 스크린샷 캡처
	if (GCurrentLevelEditingViewportClient)
	{
		const float WarmupTime = FMath::Min(Duration * 0.5f, 1.0f);
		const int32 WarmupFrames = FMath::Max(1, FMath::RoundToInt(WarmupTime / 0.033f));
		for (int32 i = 0; i < WarmupFrames; ++i)
		{
			World->Tick(LEVELTICK_ViewportsOnly, 0.033f);
			NiagaraComp->TickComponent(0.033f, ELevelTick::LEVELTICK_TimeOnly, nullptr);
		}

		FScreenshotRequest::RequestScreenshot(FinalScreenshotPath, false, false);
		UE_LOG(LogHktVFXGenerator, Log, TEXT("VFX Preview screenshot: %s"), *FinalScreenshotPath);
	}

	// Duration 후 자동 정리
	FTimerHandle TimerHandle;
	TWeakObjectPtr<AActor> WeakActor = TempActor;
	World->GetTimerManager().SetTimer(TimerHandle, [WeakActor]()
	{
		if (AActor* Actor = WeakActor.Get())
		{
			Actor->Destroy();
		}
	}, Duration, false);

	return FinalScreenshotPath;
}

bool UHktVFXGeneratorSubsystem::UpdateEmitterParameters(
	const FString& NiagaraSystemPath,
	const FString& EmitterName,
	const FString& JsonOverrides)
{
	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *NiagaraSystemPath);
	if (!System)
	{
		UE_LOG(LogHktVFXGenerator, Error, TEXT("NiagaraSystem not found: %s"), *NiagaraSystemPath);
		return false;
	}

	// JSON 파싱
	TSharedPtr<FJsonObject> RootObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonOverrides);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		UE_LOG(LogHktVFXGenerator, Error, TEXT("Failed to parse JSON overrides"));
		return false;
	}

	// 에미터 인덱스 찾기
	const TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
	int32 EmitterIndex = INDEX_NONE;
	for (int32 i = 0; i < Handles.Num(); ++i)
	{
		if (Handles[i].GetName().ToString() == EmitterName)
		{
			EmitterIndex = i;
			break;
		}
	}

	if (EmitterIndex == INDEX_NONE)
	{
		UE_LOG(LogHktVFXGenerator, Error, TEXT("Emitter '%s' not found"), *EmitterName);
		return false;
	}

	bool bAnyChanged = false;

	// === Spawn 오버라이드 ===
	const TSharedPtr<FJsonObject>* SpawnObj = nullptr;
	if (RootObj->TryGetObjectField(TEXT("spawn"), SpawnObj))
	{
		FHktVFXEmitterSpawnConfig SpawnConfig;
		FString ModeStr;
		if ((*SpawnObj)->TryGetStringField(TEXT("mode"), ModeStr))
			SpawnConfig.Mode = ModeStr;

		double TempVal = 0.0;
		if ((*SpawnObj)->TryGetNumberField(TEXT("rate"), TempVal))
			SpawnConfig.Rate = static_cast<float>(TempVal);
		int64 TempInt = 0;
		if ((*SpawnObj)->TryGetNumberField(TEXT("burstCount"), TempVal))
			SpawnConfig.BurstCount = static_cast<int32>(TempVal);

		Builder.SetupSpawnModule(System, EmitterIndex, SpawnConfig);
		bAnyChanged = true;
	}

	// === Init 오버라이드 ===
	const TSharedPtr<FJsonObject>* InitObj = nullptr;
	if (RootObj->TryGetObjectField(TEXT("init"), InitObj))
	{
		FHktVFXEmitterInitConfig InitConfig;
		double TempVal = 0.0;
		if ((*InitObj)->TryGetNumberField(TEXT("lifetimeMin"), TempVal))
			InitConfig.LifetimeMin = static_cast<float>(TempVal);
		if ((*InitObj)->TryGetNumberField(TEXT("lifetimeMax"), TempVal))
			InitConfig.LifetimeMax = static_cast<float>(TempVal);
		if ((*InitObj)->TryGetNumberField(TEXT("sizeMin"), TempVal))
			InitConfig.SizeMin = static_cast<float>(TempVal);
		if ((*InitObj)->TryGetNumberField(TEXT("sizeMax"), TempVal))
			InitConfig.SizeMax = static_cast<float>(TempVal);

		Builder.SetupInitializeModule(System, EmitterIndex, InitConfig);
		bAnyChanged = true;
	}

	// === Update 오버라이드 ===
	const TSharedPtr<FJsonObject>* UpdateObj = nullptr;
	if (RootObj->TryGetObjectField(TEXT("update"), UpdateObj))
	{
		FHktVFXEmitterUpdateConfig UpdateConfig;
		double TempVal = 0.0;
		if ((*UpdateObj)->TryGetNumberField(TEXT("drag"), TempVal))
			UpdateConfig.Drag = static_cast<float>(TempVal);
		if ((*UpdateObj)->TryGetNumberField(TEXT("opacityStart"), TempVal))
			UpdateConfig.OpacityStart = static_cast<float>(TempVal);
		if ((*UpdateObj)->TryGetNumberField(TEXT("opacityEnd"), TempVal))
			UpdateConfig.OpacityEnd = static_cast<float>(TempVal);
		if ((*UpdateObj)->TryGetNumberField(TEXT("sizeScaleStart"), TempVal))
			UpdateConfig.SizeScaleStart = static_cast<float>(TempVal);
		if ((*UpdateObj)->TryGetNumberField(TEXT("sizeScaleEnd"), TempVal))
			UpdateConfig.SizeScaleEnd = static_cast<float>(TempVal);
		if ((*UpdateObj)->TryGetNumberField(TEXT("noiseStrength"), TempVal))
			UpdateConfig.NoiseStrength = static_cast<float>(TempVal);
		if ((*UpdateObj)->TryGetNumberField(TEXT("vortexStrength"), TempVal))
			UpdateConfig.VortexStrength = static_cast<float>(TempVal);

		Builder.SetupUpdateModules(System, EmitterIndex, UpdateConfig);
		bAnyChanged = true;
	}

	if (bAnyChanged)
	{
		System->RequestCompile(false);
		System->MarkPackageDirty();

		FString PackageFileName = FPackageName::LongPackageNameToFilename(
			System->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(System->GetOutermost(), System, *PackageFileName, SaveArgs);

		UE_LOG(LogHktVFXGenerator, Log, TEXT("Updated emitter '%s' and saved"), *EmitterName);
	}

	return bAnyChanged;
}
