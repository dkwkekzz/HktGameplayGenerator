// Copyright Hkt Studios, Inc. All Rights Reserved.
// Story Generator EditorSubsystem — JSON Story → Bytecode 파이프라인

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "HktStoryJsonCompiler.h"
#include "HktStoryGeneratorSubsystem.generated.h"

/**
 * UHktStoryGeneratorSubsystem
 *
 * AI Agent가 JSON으로 게임 Story를 정의하면
 * FHktStoryBuilder 호출로 컴파일하여 VM에 등록.
 *
 * === 핵심 기능 ===
 * 1. JSON → Bytecode 컴파일 + 등록
 * 2. 태그 의존성 분석 (참조 태그의 에셋 존재 여부 확인)
 * 3. Generator 연동 (없는 태그는 해당 Generator에 생성 요청)
 * 4. 스키마/예제 제공 (AI 학습용)
 *
 * === MCP 워크플로우 ===
 * 1. McpGetStorySchema()   → Story JSON 형식 + 모든 op 스키마
 * 2. McpGetStoryExamples() → 실제 Story 패턴 예제 (BasicAttack, Fireball, Spawn, Wave)
 * 3. AI Agent가 JSON Story 작성
 * 4. McpBuildStory(json)   → 컴파일 + 등록 + 의존 태그 분석
 * 5. McpAnalyzeDependencies(json) → 필요한 에셋 생성 트리거
 */
UCLASS()
class HKTSTORYGENERATOR_API UHktStoryGeneratorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// =========================================================================
	// 빌드 API
	// =========================================================================

	/** JSON Story를 컴파일하여 VM에 등록 */
	UFUNCTION(BlueprintCallable, Category = "HKT|StoryGenerator")
	FString McpBuildStory(const FString& JsonStory);

	/** JSON Story 문법 검증 (등록 안 함) */
	UFUNCTION(BlueprintCallable, Category = "HKT|StoryGenerator")
	FString McpValidateStory(const FString& JsonStory);

	/** Story 의존성 분석 — 참조 태그 중 에셋이 없는 것 식별 + Generator 트리거 */
	UFUNCTION(BlueprintCallable, Category = "HKT|StoryGenerator")
	FString McpAnalyzeDependencies(const FString& JsonStory);

	// =========================================================================
	// 학습 API (AI Agent가 Story 작성 전에 호출)
	// =========================================================================

	/** Story JSON 스키마 반환 — 모든 op, register, property 정의 */
	UFUNCTION(BlueprintCallable, Category = "HKT|StoryGenerator")
	FString McpGetStorySchema();

	/** 실제 Story 패턴 예제 반환 — BasicAttack, Fireball, Spawn, Wave */
	UFUNCTION(BlueprintCallable, Category = "HKT|StoryGenerator")
	FString McpGetStoryExamples();

	// =========================================================================
	// 조회 API
	// =========================================================================

	/** 생성된 (동적 등록된) Story 목록 */
	UFUNCTION(BlueprintCallable, Category = "HKT|StoryGenerator")
	FString McpListGeneratedStories();

private:
	/** 의존 태그 분석 — 에셋 존재 여부 확인 */
	TArray<FGameplayTag> FindMissingAssetTags(const TArray<FGameplayTag>& ReferencedTags);

	/** 동적 등록된 Story 태그 추적 */
	TArray<FString> GeneratedStoryTags;
};
