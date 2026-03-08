// Copyright Hkt Studios, Inc. All Rights Reserved.
// MCP 호출용 BlueprintFunctionLibrary — Remote Control API를 통해 MCP에서 접근

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HktVFXGeneratorFunctionLibrary.generated.h"

/**
 * UHktVFXGeneratorFunctionLibrary
 *
 * MCP에서 Remote Control API를 통해 호출 가능한 함수 모음.
 * 모든 함수는 JSON 문자열을 반환 (MCP 통신 표준).
 * OutputDir이 비어있으면 UHktVFXGeneratorSettings::DefaultOutputDirectory 사용.
 */
UCLASS()
class HKTVFXGENERATOR_API UHktVFXGeneratorFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** JSON Config로 Niagara 시스템 빌드. */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator|MCP")
	static FString McpBuildNiagaraSystem(
		const FString& JsonConfig,
		const FString& OutputDir = TEXT(""));

	/** 프리셋 폭발 이펙트 빌드. */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator|MCP")
	static FString McpBuildPresetExplosion(
		const FString& Name,
		float R = 1.f, float G = 0.5f, float B = 0.1f,
		float Intensity = 0.5f,
		const FString& OutputDir = TEXT(""));

	/** VFX Config JSON 스키마 반환. */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator|MCP")
	static FString McpGetVFXConfigSchema();

	/** 생성된 VFX 에셋 목록 반환. Directory 비어있으면 Settings 기본값 사용. */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator|MCP")
	static FString McpListGeneratedVFX(
		const FString& Directory = TEXT(""));
};
