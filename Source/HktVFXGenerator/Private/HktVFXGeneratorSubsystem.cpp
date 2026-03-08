// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVFXGeneratorSubsystem.h"
#include "HktVFXGeneratorSettings.h"
#include "NiagaraSystem.h"

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

	// === 메인 파티클 에미터 ===
	{
		FHktVFXEmitterConfig Emitter;
		Emitter.Name = TEXT("MainBurst");
		Emitter.Spawn.Mode = TEXT("burst");
		Emitter.Spawn.BurstCount = FMath::RoundToInt(FMath::Lerp(20.f, 100.f, Intensity));

		Emitter.Init.LifetimeMin = FMath::Lerp(0.3f, 0.5f, Intensity);
		Emitter.Init.LifetimeMax = FMath::Lerp(0.8f, 2.0f, Intensity);
		Emitter.Init.SizeMin = FMath::Lerp(5.f, 20.f, Intensity);
		Emitter.Init.SizeMax = FMath::Lerp(20.f, 80.f, Intensity);

		float Speed = FMath::Lerp(200.f, 800.f, Intensity);
		Emitter.Init.VelocityMin = FVector(-Speed, -Speed, -Speed * 0.5f);
		Emitter.Init.VelocityMax = FVector(Speed, Speed, Speed);
		Emitter.Init.Color = Color;

		Emitter.Update.Gravity = FVector(0.f, 0.f, -490.f);
		Emitter.Update.Drag = FMath::Lerp(0.5f, 2.0f, Intensity);

		Emitter.Render.RendererType = TEXT("sprite");
		Emitter.Render.BlendMode = TEXT("additive");

		Config.Emitters.Add(MoveTemp(Emitter));
	}

	// === 라이트 에미터 (강도 높을 때만) ===
	if (Intensity > 0.3f)
	{
		FHktVFXEmitterConfig LightEmitter;
		LightEmitter.Name = TEXT("FlashLight");
		LightEmitter.Spawn.Mode = TEXT("burst");
		LightEmitter.Spawn.BurstCount = 1;

		LightEmitter.Init.LifetimeMin = 0.1f;
		LightEmitter.Init.LifetimeMax = FMath::Lerp(0.3f, 0.8f, Intensity);
		LightEmitter.Init.Color = Color;

		LightEmitter.Render.RendererType = TEXT("light");

		Config.Emitters.Add(MoveTemp(LightEmitter));
	}

	UE_LOG(LogHktVFXGenerator, Log, TEXT("BuildPresetExplosion: %s (Intensity=%.2f)"),
		*Name, Intensity);

	return BuildNiagaraFromConfig(Config, OutputDir);
}

FString UHktVFXGeneratorSubsystem::GetConfigSchemaJson() const
{
	return FHktVFXNiagaraConfig::GetSchemaJson();
}
