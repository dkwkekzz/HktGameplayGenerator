// Copyright Hkt Studios, Inc. All Rights Reserved.
// MCP/LLM 호출용 Animation Generator API

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HktAnimGeneratorFunctionLibrary.generated.h"

/**
 * UHktAnimGeneratorFunctionLibrary
 *
 * MCP Agent가 호출하는 애니메이션 생성 함수 모음.
 *
 * === MCP 워크플로우 ===
 * 1. McpRequestAnimation(intent) → Convention Path + 프롬프트 + 예상 타입
 * 2. Agent가 외부 도구 (Mixamo, Motion Diffusion) 로 애니메이션 생성
 * 3. McpImportAnimation(filePath, intent) → UE5 에셋 임포트
 * 4. McpListGeneratedAnimations() → 생성된 에셋 확인
 */
UCLASS()
class HKTANIMGENERATOR_API UHktAnimGeneratorFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 애니메이션 생성 요청 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpRequestAnimation(const FString& JsonIntent);

	/** 외부 파일 임포트 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpImportAnimation(const FString& FilePath, const FString& JsonIntent);

	/** 펜딩 요청 목록 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpGetPendingAnimRequests();

	/** 생성된 애니메이션 목록 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpListGeneratedAnimations(const FString& Directory = TEXT(""));
};
