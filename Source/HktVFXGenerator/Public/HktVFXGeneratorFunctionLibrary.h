// Copyright Hkt Studios, Inc. All Rights Reserved.
// MCP/LLM 호출용 BlueprintFunctionLibrary

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HktVFXGeneratorFunctionLibrary.generated.h"

/**
 * UHktVFXGeneratorFunctionLibrary
 *
 * LLM/MCP에서 호출 가능한 VFX 생성 함수 모음.
 * 모든 함수는 JSON 문자열을 반환.
 *
 * === LLM 워크플로우 ===
 * 1. McpGetVFXPromptGuide()   → 디자인 가이드 + 스키마 + 에미터 레이어 패턴
 * 2. McpGetVFXExampleConfigs() → 예제 Config JSON (패턴 학습용)
 * 3. LLM이 사용자 요청 기반으로 Config JSON 생성
 * 4. McpBuildNiagaraSystem(json) → Niagara 에셋 빌드
 * 5. McpListGeneratedVFX()     → 생성된 에셋 확인
 */
UCLASS()
class HKTVFXGENERATOR_API UHktVFXGeneratorFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// =========================================================================
	// 빌드
	// =========================================================================

	/** JSON Config로 Niagara 시스템 빌드. */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator|MCP")
	static FString McpBuildNiagaraSystem(
		const FString& JsonConfig,
		const FString& OutputDir = TEXT(""));

	/** 프리셋 폭발 이펙트 빌드 (테스트용). */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator|MCP")
	static FString McpBuildPresetExplosion(
		const FString& Name,
		float R = 1.f, float G = 0.5f, float B = 0.1f,
		float Intensity = 0.5f,
		const FString& OutputDir = TEXT(""));

	// =========================================================================
	// LLM 학습용 — Config 생성 전에 호출
	// =========================================================================

	/** VFX Config JSON 스키마 반환. */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator|MCP")
	static FString McpGetVFXConfigSchema();

	/**
	 * LLM용 종합 프롬프트 가이드 반환.
	 * 스키마 + 에미터 레이어 패턴 + 값 범위 가이드 + 디자인 팁.
	 * LLM이 Config를 생성하기 전에 이것을 먼저 읽어야 함.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator|MCP")
	static FString McpGetVFXPromptGuide();

	/**
	 * 예제 Config JSON 목록 반환.
	 * Explosion, Fire, Magic 등 다양한 패턴의 실제 Config.
	 * LLM이 이 예제를 참고하여 새로운 Config를 생성.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator|MCP")
	static FString McpGetVFXExampleConfigs();

	// =========================================================================
	// 조회
	// =========================================================================

	/** 생성된 VFX 에셋 목록 반환. */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator|MCP")
	static FString McpListGeneratedVFX(
		const FString& Directory = TEXT(""));
};
