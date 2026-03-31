// Copyright Hkt Studios, Inc. All Rights Reserved.
// Generator Prompt 공용 타입 정의

#pragma once

#include "CoreMinimal.h"

/** Generator 종류 — 각 탭에 대응 */
enum class EHktGeneratorType : uint8
{
	VFX,
	Character,
	Item,
	Map,
	Story,
	Texture,
	COUNT
};

/** Generator 종류별 메타 정보 */
struct FHktGeneratorTypeInfo
{
	EHktGeneratorType Type;
	FString StepType;      // "vfx_generation"
	FString SkillName;     // "vfx-gen" — .claude/skills/{SkillName}/SKILL.md
	FString DisplayName;   // "VFX"
	FString InputKey;      // asset_discovery output에서 읽을 키 ("vfx", "characters", ...)
};

/** 전체 Generator 메타 정보 테이블 */
inline const TArray<FHktGeneratorTypeInfo>& GetGeneratorTypeInfos()
{
	static const TArray<FHktGeneratorTypeInfo> Infos = {
		{ EHktGeneratorType::VFX,       TEXT("vfx_generation"),       TEXT("vfx-gen"),       TEXT("VFX"),       TEXT("vfx") },
		{ EHktGeneratorType::Character, TEXT("character_generation"), TEXT("char-gen"),      TEXT("Character"), TEXT("characters") },
		{ EHktGeneratorType::Item,      TEXT("item_generation"),      TEXT("item-gen"),      TEXT("Item"),      TEXT("items") },
		{ EHktGeneratorType::Map,       TEXT("map_generation"),       TEXT("map-gen"),       TEXT("Map"),       TEXT("terrain_spec") },
		{ EHktGeneratorType::Story,     TEXT("story_generation"),     TEXT("story-gen"),     TEXT("Story"),     TEXT("stories") },
		{ EHktGeneratorType::Texture,   TEXT("texture_generation"),   TEXT("texture-gen"),   TEXT("Texture"),   TEXT("textures") },
	};
	return Infos;
}

/** 생성 Phase 진행 상태 */
enum class EHktPhaseStatus : uint8
{
	NotStarted,
	InProgress,
	Completed,
	Failed
};

/** 생성 Phase — Progress 섹션에 표시되는 단계 */
struct FHktGenerationPhase
{
	FString PhaseName;    // "Material Prep", "Niagara Build"
	FString Description;  // 현재 진행 상세
	EHktPhaseStatus Status = EHktPhaseStatus::NotStarted;
};

/** 피드백 액션 */
enum class EHktFeedbackAction : uint8
{
	Accept,
	Reject,
	Refine
};

/** CLI stream-json 이벤트 (파싱된 JSON 라인) */
struct FHktClaudeEvent
{
	FString Type;       // "assistant", "tool_use", "tool_result", "result"
	FString Content;    // 텍스트 내용
	FString ToolName;   // 도구 이름 (tool_use)
	FString ToolInput;  // 도구 입력 JSON (tool_use)
};
