// Copyright Hkt Studios, Inc. All Rights Reserved.
// VFX Generator 설정 — Project Settings > Plugins > HktVFXGenerator

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HktVFXGeneratorSettings.generated.h"

/**
 * UHktVFXGeneratorSettings
 *
 * Project Settings > Plugins > HKT VFX Generator 에서 설정 가능.
 * 에미터 템플릿 경로, 머티리얼 경로, 기본 출력 디렉토리 등.
 */
UCLASS(config = Game, DefaultConfig, DisplayName = "Hkt VFX Generator")
class HKTVFXGENERATOR_API UHktVFXGeneratorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UHktVFXGeneratorSettings();

	// ============================================================================
	// 에미터 템플릿
	// ============================================================================

	/** 렌더러 타입별 에미터 템플릿 에셋 경로. key: "sprite", "ribbon", "light", "mesh" */
	UPROPERTY(Config, EditAnywhere, Category = "Templates",
		meta = (AllowedClasses = "/Script/Niagara.NiagaraEmitter"))
	TMap<FString, FSoftObjectPath> EmitterTemplates;

	/** 매핑 실패 시 사용할 폴백 에미터 템플릿 */
	UPROPERTY(Config, EditAnywhere, Category = "Templates",
		meta = (AllowedClasses = "/Script/Niagara.NiagaraEmitter"))
	FSoftObjectPath FallbackEmitterTemplate;

	// ============================================================================
	// 머티리얼
	// ============================================================================

	/** Additive 블렌딩 마스터 머티리얼 (없으면 Niagara 기본 머티리얼 사용) */
	UPROPERTY(Config, EditAnywhere, Category = "Materials",
		meta = (AllowedClasses = "/Script/Engine.MaterialInterface"))
	FSoftObjectPath AdditiveMaterial;

	/** Translucent 블렌딩 마스터 머티리얼 (없으면 Niagara 기본 머티리얼 사용) */
	UPROPERTY(Config, EditAnywhere, Category = "Materials",
		meta = (AllowedClasses = "/Script/Engine.MaterialInterface"))
	FSoftObjectPath TranslucentMaterial;

	// ============================================================================
	// 출력
	// ============================================================================

	/** 생성된 VFX 에셋의 기본 출력 디렉토리 */
	UPROPERTY(Config, EditAnywhere, Category = "Output")
	FString DefaultOutputDirectory = TEXT("/Game/GeneratedVFX");

	// ============================================================================
	// UDeveloperSettings
	// ============================================================================

	virtual FName GetContainerName() const override { return FName("Project"); }
	virtual FName GetCategoryName() const override { return FName("HktGameplay"); }
	virtual FName GetSectionName() const override { return FName("HktVFXGenerator"); }

	/** 싱글턴 접근 */
	static const UHktVFXGeneratorSettings* Get()
	{
		return GetDefault<UHktVFXGeneratorSettings>();
	}
};
