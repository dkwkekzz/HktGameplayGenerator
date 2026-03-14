// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVFXGeneratorHandler.h"
#include "HktVFXAutoResolver.h"
#include "HktVFXGeneratorSubsystem.h"
#include "HktVFXGeneratorSettings.h"
#include "HktVFXNiagaraConfig.h"
#include "HktVFXIntent.h"
#include "HktAssetSubsystem.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktVFXGeneratorHandler, Log, All);

bool UHktVFXGeneratorHandler::CanHandle(const FGameplayTag& Tag) const
{
	return Tag.IsValid() && Tag.ToString().StartsWith(TEXT("VFX."));
}

FSoftObjectPath UHktVFXGeneratorHandler::HandleTagMiss(const FGameplayTag& Tag)
{
	FHktVFXIntent Intent;
	if (!FHktVFXAutoResolver::ParseTagToIntent(Tag, Intent))
	{
		UE_LOG(LogHktVFXGeneratorHandler, Warning, TEXT("Failed to parse VFX tag: %s"), *Tag.ToString());
		return FSoftObjectPath();
	}

	// VFXGeneratorSubsystem으로 빌드
	UHktVFXGeneratorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UHktVFXGeneratorSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogHktVFXGeneratorHandler, Error, TEXT("VFXGeneratorSubsystem not available"));
		return FSoftObjectPath();
	}

	// Convention Path에서 출력 경로와 에셋 이름 결정
	// Settings 규칙: VFX.* → {Root}/VFX/{TagPath}
	// Builder가 NS_ 접두사를 붙이므로 SystemName에서 제외
	FSoftObjectPath ConventionPath = UHktAssetSubsystem::ResolveConventionPath(Tag);
	FString OutputDir;
	FString SystemName;

	if (ConventionPath.IsValid())
	{
		// Convention Path에서 디렉토리와 에셋 이름 분리
		// 예: /Game/Generated/VFX/VFX_Explosion_Fire.VFX_Explosion_Fire
		FString FullPath = ConventionPath.GetAssetPathString();
		int32 LastSlash;
		if (FullPath.FindLastChar('/', LastSlash))
		{
			OutputDir = FullPath.Left(LastSlash);
			// Builder가 NS_ prefix를 추가하므로, convention 이름 그대로 사용
			// Builder: NS_{SystemName} → 에셋명이 NS_VFX_Explosion_Fire 가 됨
			// Convention path는 VFX_Explosion_Fire → 맞추려면 접두사 제거
			SystemName = FullPath.Mid(LastSlash + 1);
		}
	}

	if (SystemName.IsEmpty())
	{
		// Fallback
		FString TagStr = Tag.ToString();
		SystemName = TagStr;
		SystemName.ReplaceInline(TEXT("."), TEXT("_"));
	}

	// Intent → 기본 Config 생성
	// Builder가 NS_ 접두사를 자동으로 붙이므로 SystemName에서 제거해서 전달
	FString CleanName = SystemName;
	if (CleanName.StartsWith(TEXT("NS_")))
	{
		CleanName = CleanName.Mid(3);
	}
	FHktVFXNiagaraConfig Config = BuildDefaultConfig(Intent, CleanName);

	// Niagara 시스템 빌드 (Convention Path와 일치하는 디렉토리 지정)
	UNiagaraSystem* System = Subsystem->BuildNiagaraFromConfig(Config, OutputDir);
	if (!System)
	{
		UE_LOG(LogHktVFXGeneratorHandler, Warning, TEXT("Failed to build Niagara for tag: %s"), *Tag.ToString());
		return FSoftObjectPath();
	}

	// Builder가 생성한 실제 에셋 경로를 반환
	// 에셋 경로: {OutputDir}/NS_{CleanName}.NS_{CleanName}
	FString ActualAssetName = FString::Printf(TEXT("NS_%s"), *CleanName);
	FString ActualPath = FString::Printf(TEXT("%s/%s.%s"), *OutputDir, *ActualAssetName, *ActualAssetName);
	FSoftObjectPath ResultPath(ActualPath);

	UE_LOG(LogHktVFXGeneratorHandler, Log, TEXT("VFX generated: %s → %s"), *Tag.ToString(), *ResultPath.ToString());
	return ResultPath;
}

FHktVFXNiagaraConfig UHktVFXGeneratorHandler::BuildDefaultConfig(const FHktVFXIntent& Intent, const FString& SystemName)
{
	FHktVFXNiagaraConfig Config;
	Config.SystemName = SystemName;

	// 기본 에미터 추가
	FHktVFXEmitterConfig& Emitter = Config.Emitters.AddDefaulted_GetRef();
	Emitter.Name = TEXT("Main");

	// Spawn 설정 (Intent.Intensity 기반)
	Emitter.Spawn.Mode = TEXT("burst");
	Emitter.Spawn.BurstCount = FMath::Lerp(10, 100, Intent.Intensity);

	// Init 설정
	Emitter.Init.LifetimeMin = Intent.Duration * 0.5f;
	Emitter.Init.LifetimeMax = Intent.Duration;
	Emitter.Init.SizeMin = FMath::Lerp(5.f, 20.f, Intent.Intensity);
	Emitter.Init.SizeMax = FMath::Lerp(10.f, 50.f, Intent.Intensity);

	float SpeedMin = FMath::Lerp(50.f, 200.f, Intent.Intensity);
	float SpeedMax = FMath::Lerp(100.f, 500.f, Intent.Intensity);
	Emitter.Init.VelocityMin = FVector(-SpeedMin, -SpeedMin, SpeedMin * 0.5f);
	Emitter.Init.VelocityMax = FVector(SpeedMax, SpeedMax, SpeedMax);

	// Element에 따른 색상 매핑 (Init.Color + Update ColorOverLife)
	FLinearColor ColorStart;
	FLinearColor ColorEnd;
	switch (Intent.Element)
	{
	case EHktVFXElement::Fire:
		ColorStart = FLinearColor(1.f, 0.3f, 0.0f, 1.f);
		ColorEnd = FLinearColor(1.f, 0.8f, 0.1f, 0.f);
		break;
	case EHktVFXElement::Ice:
		ColorStart = FLinearColor(0.5f, 0.8f, 1.0f, 1.f);
		ColorEnd = FLinearColor(0.8f, 0.95f, 1.0f, 0.f);
		break;
	case EHktVFXElement::Lightning:
		ColorStart = FLinearColor(0.7f, 0.7f, 1.0f, 1.f);
		ColorEnd = FLinearColor(1.0f, 1.0f, 1.0f, 0.f);
		break;
	case EHktVFXElement::Dark:
		ColorStart = FLinearColor(0.1f, 0.0f, 0.2f, 1.f);
		ColorEnd = FLinearColor(0.3f, 0.0f, 0.5f, 0.f);
		break;
	case EHktVFXElement::Holy:
		ColorStart = FLinearColor(1.0f, 0.9f, 0.6f, 1.f);
		ColorEnd = FLinearColor(1.0f, 1.0f, 0.8f, 0.f);
		break;
	case EHktVFXElement::Poison:
		ColorStart = FLinearColor(0.2f, 0.8f, 0.1f, 1.f);
		ColorEnd = FLinearColor(0.4f, 1.0f, 0.2f, 0.f);
		break;
	default:
		ColorStart = FLinearColor(0.8f, 0.8f, 0.8f, 1.f);
		ColorEnd = FLinearColor(1.0f, 1.0f, 1.0f, 0.f);
		break;
	}
	Emitter.Init.Color = ColorStart;
	Emitter.Update.bUseColorOverLife = true;
	Emitter.Update.ColorEnd = ColorEnd;

	// Update 설정
	float GravityScale = (Intent.EventType == EHktVFXEventType::Explosion) ? 0.5f : 0.0f;
	Emitter.Update.Gravity = FVector(0.f, 0.f, -980.f * GravityScale);
	Emitter.Update.Drag = 0.5f;
	// OpacityStart=1, OpacityEnd=0 이 기본값이므로 별도 설정 불필요

	// Render 설정
	Emitter.Render.RendererType = TEXT("sprite");

	// EventType에 따른 Radius 반영 (SpawnShape)
	if (Intent.Radius > 0.f)
	{
		Emitter.Init.ShapeLocation.Shape = TEXT("sphere");
		Emitter.Init.ShapeLocation.SphereRadius = Intent.Radius;
	}

	return Config;
}
