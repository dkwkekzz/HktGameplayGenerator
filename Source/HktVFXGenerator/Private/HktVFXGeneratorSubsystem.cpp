// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVFXGeneratorSubsystem.h"
#include "HktVFXGeneratorSettings.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "NiagaraParameterStore.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktVFXGenerator, Log, All);

void UHktVFXGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

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
