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

	// === 1. Core Flash — 중심부 강렬한 섬광 ===
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

		E.Render.RendererType = TEXT("sprite");
		E.Render.BlendMode = TEXT("additive");
		E.Render.SortOrder = 10;

		Config.Emitters.Add(MoveTemp(E));
	}

	// === 2. Main Sparks — 폭발 파편 ===
	{
		FHktVFXEmitterConfig E;
		E.Name = TEXT("MainSparks");
		E.Spawn.Mode = TEXT("burst");
		E.Spawn.BurstCount = FMath::RoundToInt(FMath::Lerp(30.f, 150.f, Intensity));

		E.Init.LifetimeMin = FMath::Lerp(0.3f, 0.5f, Intensity);
		E.Init.LifetimeMax = FMath::Lerp(0.8f, 2.0f, Intensity);
		E.Init.SizeMin = FMath::Lerp(3.f, 8.f, Intensity);
		E.Init.SizeMax = FMath::Lerp(8.f, 25.f, Intensity);
		E.Init.SpriteRotationMin = 0.f;
		E.Init.SpriteRotationMax = 360.f;

		float Speed = FMath::Lerp(300.f, 1000.f, Intensity);
		E.Init.VelocityMin = FVector(-Speed, -Speed, -Speed * 0.3f);
		E.Init.VelocityMax = FVector(Speed, Speed, Speed * 1.2f);
		E.Init.Color = Color;

		E.Update.Gravity = FVector(0.f, 0.f, FMath::Lerp(-490.f, -980.f, Intensity));
		E.Update.Drag = FMath::Lerp(1.0f, 3.0f, Intensity);
		E.Update.RotationRateMin = -180.f;
		E.Update.RotationRateMax = 180.f;
		E.Update.OpacityStart = 1.f;
		E.Update.OpacityEnd = 0.f;
		E.Update.SizeScaleStart = 1.f;
		E.Update.SizeScaleEnd = 0.2f;

		E.Update.bUseColorOverLife = true;
		E.Update.ColorEnd = FLinearColor(Color.R * 0.3f, Color.G * 0.1f, 0.f, 1.f);

		E.Render.RendererType = TEXT("sprite");
		E.Render.BlendMode = TEXT("additive");
		E.Render.Alignment = TEXT("velocity_aligned");
		E.Render.SortOrder = 5;

		Config.Emitters.Add(MoveTemp(E));
	}

	// === 3. Smoke — 연기 (중간 강도 이상) ===
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
		E.Init.SpriteRotationMin = 0.f;
		E.Init.SpriteRotationMax = 360.f;

		float SmokeSpeed = FMath::Lerp(50.f, 200.f, Intensity);
		E.Init.VelocityMin = FVector(-SmokeSpeed, -SmokeSpeed, 0.f);
		E.Init.VelocityMax = FVector(SmokeSpeed, SmokeSpeed, SmokeSpeed * 1.5f);
		E.Init.Color = FLinearColor(0.15f, 0.12f, 0.1f, 0.6f);

		E.Update.Gravity = FVector(0.f, 0.f, 50.f);
		E.Update.Drag = 2.0f;
		E.Update.RotationRateMin = -30.f;
		E.Update.RotationRateMax = 30.f;
		E.Update.SizeScaleStart = 0.5f;
		E.Update.SizeScaleEnd = 3.0f;
		E.Update.OpacityStart = 0.6f;
		E.Update.OpacityEnd = 0.f;
		E.Update.NoiseStrength = FMath::Lerp(20.f, 80.f, Intensity);
		E.Update.NoiseFrequency = 2.f;

		E.Render.RendererType = TEXT("sprite");
		E.Render.BlendMode = TEXT("translucent");
		E.Render.SortOrder = 0;

		Config.Emitters.Add(MoveTemp(E));
	}

	// === 4. Shockwave Ring — 충격파 링 ===
	if (Intensity > 0.4f)
	{
		FHktVFXEmitterConfig E;
		E.Name = TEXT("Shockwave");
		E.Spawn.Mode = TEXT("burst");
		E.Spawn.BurstCount = 1;

		E.Init.LifetimeMin = 0.2f;
		E.Init.LifetimeMax = FMath::Lerp(0.3f, 0.6f, Intensity);
		E.Init.SizeMin = FMath::Lerp(10.f, 30.f, Intensity);
		E.Init.SizeMax = FMath::Lerp(20.f, 50.f, Intensity);
		E.Init.VelocityMin = FVector::ZeroVector;
		E.Init.VelocityMax = FVector::ZeroVector;
		E.Init.Color = FLinearColor(Color.R * 2.f, Color.G * 2.f, Color.B * 2.f, 0.8f);

		E.Update.Gravity = FVector::ZeroVector;
		E.Update.SizeScaleStart = 1.0f;
		E.Update.SizeScaleEnd = FMath::Lerp(10.f, 25.f, Intensity);
		E.Update.OpacityStart = 0.8f;
		E.Update.OpacityEnd = 0.f;

		E.Render.RendererType = TEXT("sprite");
		E.Render.BlendMode = TEXT("additive");
		E.Render.SortOrder = 2;

		Config.Emitters.Add(MoveTemp(E));
	}

	// === 5. Dynamic Light — 동적 조명 ===
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
