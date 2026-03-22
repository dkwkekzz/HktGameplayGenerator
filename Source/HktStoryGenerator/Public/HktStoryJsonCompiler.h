// Copyright Hkt Studios, Inc. All Rights Reserved.
// Story JSON 에디터 컴파일러 — FHktStoryJsonParser(런타임) 위에 에디터 전용 기능 추가

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * Story JSON 컴파일 결과 (에디터 전용 필드 포함)
 */
struct HKTSTORYGENERATOR_API FHktStoryCompileResult
{
	bool bSuccess = false;
	FString StoryTag;
	TArray<FString> Errors;
	TArray<FString> Warnings;

	/** Story에서 참조하는 모든 태그 (의존성 분석용) */
	TArray<FGameplayTag> ReferencedTags;

	/** 참조 태그 중 에셋이 없는 태그 (Generator 트리거 대상) */
	TArray<FGameplayTag> MissingAssetTags;
};

/**
 * FHktStoryJsonCompiler — 에디터 전용 컴파일러
 *
 * FHktStoryJsonParser(런타임)에 위임하되, 에디터 전용 기능을 추가:
 * - 태그 자동등록 + INI 영속화 (EnsureTagRegistered)
 * - 스키마/예제 제공 (AI Agent 학습용)
 * - 의존성 분석 (에셋 존재 여부 확인)
 *
 * === 새 op 추가 시 ===
 * 1. FHktStoryBuilder에 메서드 추가
 * 2. FHktStoryJsonParser::InitializeCoreCommands()에 RegisterCommand 1줄 추가
 * 3. skill 스키마 JSON 업데이트
 * → HktStoryGenerator 코드 변경 불필요
 */
struct HKTSTORYGENERATOR_API FHktStoryJsonCompiler
{
	/**
	 * JSON 문자열로부터 Story 컴파일 및 등록.
	 * 태그 자동등록 + alias 해결 포함.
	 */
	static FHktStoryCompileResult CompileAndRegister(const FString& JsonStr);

	/**
	 * JSON 문법 검증만 수행 (등록하지 않음).
	 */
	static FHktStoryCompileResult Validate(const FString& JsonStr);

	/**
	 * Story Builder API 스키마 (JSON) — skill 디렉토리의 정적 파일에서 로드
	 */
	static FString GetStorySchema();

	/**
	 * 기존 Story 예제들을 JSON 형태로 반환 — AI Agent 학습용
	 */
	static FString GetStoryExamples();

private:
	/** 태그 등록 + INI 영속화 (에디터 전용) */
	static FGameplayTag EnsureTagRegistered(const FString& TagName);

	/** 태그 alias 해결 */
	static FGameplayTag ResolveTag(const FString& TagStr, const TMap<FString, FGameplayTag>& TagAliases);
};
