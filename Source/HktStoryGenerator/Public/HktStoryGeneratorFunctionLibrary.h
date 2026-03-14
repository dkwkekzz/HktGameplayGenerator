// Copyright Hkt Studios, Inc. All Rights Reserved.
// MCP/LLM 호출용 Story Generator API

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HktStoryGeneratorFunctionLibrary.generated.h"

/**
 * UHktStoryGeneratorFunctionLibrary
 *
 * MCP Agent가 호출하는 Story 생성 함수 모음.
 *
 * === MCP 워크플로우 ===
 * 1. McpGetStorySchema()         → Story JSON 형식 학습
 * 2. McpGetStoryExamples()       → 실제 패턴 학습
 * 3. McpValidateStory(json)      → 문법 검증
 * 4. McpAnalyzeDependencies(json)→ 필요 에셋 확인 + Generator 트리거
 * 5. McpBuildStory(json)         → 컴파일 + VM 등록
 * 6. McpListGeneratedStories()   → 등록된 Story 확인
 */
UCLASS()
class HKTSTORYGENERATOR_API UHktStoryGeneratorFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** JSON Story 컴파일 + VM 등록 */
	UFUNCTION(BlueprintCallable, Category = "HKT|StoryGenerator|MCP")
	static FString McpBuildStory(const FString& JsonStory);

	/** JSON Story 문법 검증 */
	UFUNCTION(BlueprintCallable, Category = "HKT|StoryGenerator|MCP")
	static FString McpValidateStory(const FString& JsonStory);

	/** Story 의존성 분석 */
	UFUNCTION(BlueprintCallable, Category = "HKT|StoryGenerator|MCP")
	static FString McpAnalyzeDependencies(const FString& JsonStory);

	/** Story JSON 스키마 */
	UFUNCTION(BlueprintCallable, Category = "HKT|StoryGenerator|MCP")
	static FString McpGetStorySchema();

	/** Story 패턴 예제 */
	UFUNCTION(BlueprintCallable, Category = "HKT|StoryGenerator|MCP")
	static FString McpGetStoryExamples();

	/** 생성된 Story 목록 */
	UFUNCTION(BlueprintCallable, Category = "HKT|StoryGenerator|MCP")
	static FString McpListGeneratedStories();
};
