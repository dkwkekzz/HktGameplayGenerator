// Copyright Hkt Studios, Inc. All Rights Reserved.
// JSON → FHktStoryBuilder 컴파일러

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
 *
 * 지원 Operations (op):
 *   Control: Label, Jump, JumpIf, JumpIfNot, Yield, WaitSeconds, Halt
 *   Wait: WaitCollision, WaitAnimEnd, WaitMoveEnd
 *   Data: LoadConst, LoadStore, LoadEntityProperty, SaveStore, SaveEntityProperty, Move
 *   Arithmetic: Add, Sub, Mul, Div, AddImm
 *   Comparison: CmpEq, CmpNe, CmpLt, CmpLe, CmpGt, CmpGe
 *   Entity: SpawnEntity, DestroyEntity
 *   Position: GetPosition, SetPosition, MoveToward, MoveForward, StopMovement, GetDistance
 *   Spatial: FindInRadius, NextFound, ForEachInRadius, EndForEach
 *   Combat: ApplyDamage, ApplyDamageConst, ApplyEffect, RemoveEffect
 *   VFX: PlayVFX, PlayVFXAttached
 *   Audio: PlaySound, PlaySoundAtLocation
 *   Item: SpawnItem
 *   Tags: AddTag, RemoveTag, HasTag, CountByTag
 *   Query: GetWorldTime, RandomInt, HasPlayerInGroup, CountByOwner, FindByOwner
 *   Utility: Log
 *   Policy: CancelOnDuplicate
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
	 */
	static FString GetStorySchema();

	/**
	 * 기존 Story 예제들을 JSON 형태로 반환 — AI Agent 학습용
	 */
	static FString GetStoryExamples();

private:
	/** Register 이름 → RegisterIndex 변환 */
	static int32 ParseRegister(const FString& RegStr);

	/** 태그 alias 해결 (tags 맵에서 찾거나 직접 GameplayTag 생성) */
	static FGameplayTag ResolveTag(const FString& TagStr, const TMap<FString, FGameplayTag>& TagAliases);

	/** PropertyId 이름 → uint16 변환 */
	static uint16 ParsePropertyId(const FString& PropStr);

	/** 개별 step을 Builder에 적용 */
	static bool ApplyStep(
		FHktStoryBuilder& Builder,
		const TSharedPtr<class FJsonObject>& StepObj,
		const TMap<FString, FGameplayTag>& TagAliases,
		TArray<FGameplayTag>& OutReferencedTags,
		TArray<FString>& OutErrors,
		int32 StepIndex);
};
