// Copyright Hkt Studios, Inc. All Rights Reserved.
// JSON → FHktStoryBuilder 컴파일러 (FHktStoryOpRegistry 기반 — 자동 dispatch)

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class FHktStoryBuilder;

/**
 * Story JSON 컴파일 결과
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
 * FHktStoryJsonCompiler
 *
 * JSON Story 정의를 FHktStoryBuilder 호출로 변환.
 * FHktStoryOpRegistry(HktCore)에 등록된 Operation 정의를 기반으로 자동 dispatch.
 *
 * === HktStoryGenerator 무변경 보장 ===
 * 새 Builder 메서드는 FHktStoryOpRegistry에 등록하면
 * 이 컴파일러가 자동으로 인식 — 코드 수정 불필요.
 *
 * JSON 포맷:
 * {
 *   "storyTag": "Ability.Skill.IceBlast",
 *   "description": "Ice blast skill",
 *   "cancelOnDuplicate": false,
 *   "tags": { "alias": "Full.Tag.Name", ... },
 *   "steps": [
 *     { "op": "AddTag", "entity": "Self", "tag": "alias_or_full_tag" },
 *     ...
 *   ]
 * }
 */
struct HKTSTORYGENERATOR_API FHktStoryJsonCompiler
{
	/**
	 * JSON 문자열로부터 Story 컴파일 및 등록.
	 * @param JsonStr JSON Story 정의
	 * @return 컴파일 결과 (성공 여부, 에러, 참조 태그 등)
	 */
	static FHktStoryCompileResult CompileAndRegister(const FString& JsonStr);

	/**
	 * JSON 문법 검증만 수행 (등록하지 않음).
	 */
	static FHktStoryCompileResult Validate(const FString& JsonStr);

	/**
	 * Story Builder API 스키마 (JSON) — AI Agent 학습용
	 * FHktStoryOpRegistry에서 자동 생성.
	 */
	static FString GetStorySchema();

	/**
	 * 기존 Story 예제들을 JSON 형태로 반환 — AI Agent 학습용
	 */
	static FString GetStoryExamples();

private:
	/** 태그 alias 해결 (tags 맵에서 찾거나 직접 GameplayTag 생성) */
	static FGameplayTag ResolveTag(const FString& TagStr, const TMap<FString, FGameplayTag>& TagAliases);

	/**
	 * 개별 step을 Builder에 적용 — FHktStoryOpRegistry 기반 자동 dispatch.
	 * Op 이름으로 레지스트리 검색 → 파라미터 파싱 → 핸들러 호출.
	 */
	static bool ApplyStep(
		FHktStoryBuilder& Builder,
		const TSharedPtr<class FJsonObject>& StepObj,
		const TMap<FString, FGameplayTag>& TagAliases,
		TArray<FGameplayTag>& OutReferencedTags,
		TArray<FString>& OutErrors,
		int32 StepIndex);
};
