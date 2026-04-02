// Copyright Hkt Studios, Inc. All Rights Reserved.
// MCP/LLM 호출용 텍스처 생성 BlueprintFunctionLibrary

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HktTextureGeneratorFunctionLibrary.generated.h"

/**
 * UHktTextureGeneratorFunctionLibrary
 *
 * LLM/MCP에서 호출 가능한 텍스처 생성 함수 모음.
 *
 * === MCP Agent 워크플로우 ===
 * 1. McpGenerateTexture(intent) → 캐시 히트면 즉시 반환, miss면 pending + 프롬프트 반환
 * 2. Agent가 이미지 생성 (SD/DALL-E/ComfyUI 등)
 * 3. McpImportTexture(imagePath, intent) → 생성된 이미지를 UTexture2D로 임포트
 * 4. McpListGeneratedTextures() → 확인
 */
UCLASS()
class HKTTEXTUREGENERATOR_API UHktTextureGeneratorFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// =========================================================================
	// 생성 / 임포트
	// =========================================================================

	/**
	 * Intent JSON으로 텍스처 생성/조회.
	 * 캐시 히트 → 에셋 경로 반환.
	 * 캐시 미스 → pending=true + 완성된 프롬프트 + 이미지 저장 경로 반환.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator|MCP")
	static FString McpGenerateTexture(
		const FString& JsonIntent,
		const FString& OutputDir = TEXT(""));

	/**
	 * 외부에서 생성된 이미지 파일을 UTexture2D로 임포트.
	 * Usage에 따른 텍스처 설정이 자동 적용됨.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator|MCP")
	static FString McpImportTexture(
		const FString& ImageFilePath,
		const FString& JsonIntent,
		const FString& OutputDir = TEXT(""));

	/**
	 * 배치 요청에서 미처리(pending) 목록 반환.
	 * 이미 캐시에 있는 텍스처는 제외하여 생성이 필요한 것만 반환.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator|MCP")
	static FString McpGetPendingRequests(const FString& JsonRequests);

	// =========================================================================
	// SD WebUI 서버 관리
	// =========================================================================

	/**
	 * SD WebUI 서버 연결 상태 확인.
	 * alive, launching, serverURL, batchFilePath, batchFileExists 반환.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator|MCP")
	static FString McpCheckSDServerStatus();

	// =========================================================================
	// 조회
	// =========================================================================

	/** 생성된 텍스처 목록 반환 */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator|MCP")
	static FString McpListGeneratedTextures(const FString& Directory = TEXT(""));
};
