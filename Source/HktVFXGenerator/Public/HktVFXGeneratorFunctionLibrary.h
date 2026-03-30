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
 * === LLM 워크플로우 (4-Phase Pipeline) ===
 * Phase 1 — Design:
 *   McpGetVFXPromptGuide() → McpGetVFXExampleConfigs() → 에미터 레이어 설계
 * Phase 2 — Material Prep:
 *   McpCreateParticleMaterial() → 텍스처/머티리얼 생성
 * Phase 3 — Build:
 *   McpBuildNiagaraSystem(json) → McpAssignVFXMaterial() → Niagara 빌드
 * Phase 4 — Preview & Refine:
 *   McpPreviewVFX() → McpUpdateVFXEmitter() → 시각 검증 + 파라미터 튜닝
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
	// 머티리얼 / 텍스처 (Phase 2: Material Prep)
	// =========================================================================

	/**
	 * 파티클용 MaterialInstance 동적 생성.
	 * 마스터 머티리얼(Additive/Translucent) 기반으로 텍스처를 바인딩한 MI를 생성.
	 * 반환: {"success":true, "assetPath":"..."} 또는 에러.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator|MCP")
	static FString McpCreateParticleMaterial(
		const FString& MaterialName,
		const FString& TexturePath = TEXT(""),
		const FString& BlendMode = TEXT("additive"),
		float EmissiveIntensity = 1.f,
		const FString& OutputDir = TEXT(""));

	/**
	 * 기존 Niagara 시스템의 특정 에미터에 머티리얼을 할당.
	 * Phase 2에서 생성한 머티리얼을 Phase 3 빌드 후 적용할 때 사용.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator|MCP")
	static FString McpAssignVFXMaterial(
		const FString& NiagaraSystemPath,
		const FString& EmitterName,
		const FString& MaterialPath);

	// =========================================================================
	// 프리뷰 / 튜닝 (Phase 4: Preview & Refine)
	// =========================================================================

	/**
	 * VFX를 뷰포트에 스폰하고 스크린샷을 캡처.
	 * Duration초 후 자동 제거. 스크린샷 경로 반환.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator|MCP")
	static FString McpPreviewVFX(
		const FString& NiagaraSystemPath,
		float Duration = 2.f,
		const FString& ScreenshotPath = TEXT(""));

	/**
	 * 기존 Niagara 시스템의 에미터 파라미터를 부분 업데이트.
	 * 전체 리빌드 없이 파라미터만 변경 (Spawn/Init/Update/Render).
	 * JsonOverrides: 에미터 이름별 파라미터 오버라이드 JSON.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator|MCP")
	static FString McpUpdateVFXEmitter(
		const FString& NiagaraSystemPath,
		const FString& EmitterName,
		const FString& JsonOverrides);

	// =========================================================================
	// 조회 / 디버그
	// =========================================================================

	/** 생성된 VFX 에셋 목록 반환. */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator|MCP")
	static FString McpListGeneratedVFX(
		const FString& Directory = TEXT(""));

	/** 템플릿 에미터의 실제 RapidIterationParameter 이름 덤프. */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator|MCP")
	static FString McpDumpTemplateParameters(
		const FString& RendererType = TEXT("sprite"));

	/** 모든 템플릿 에미터의 모듈/파라미터 덤프 (Phase 2 문서화용). */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator|MCP")
	static FString McpDumpAllTemplateParameters();
};
