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
	Feature,
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
		{ EHktGeneratorType::Feature,   TEXT("feature_design"),       TEXT("feature-design"), TEXT("Feature"),  TEXT("feature_outlines") },
	};
	return Infos;
}

/** Generator별 Intent 도움말 — 필드 설명 */
inline FString GetIntentHelpText(EHktGeneratorType Type)
{
	switch (Type)
	{
	case EHktGeneratorType::VFX:
		return TEXT(
			"VFX Intent — asset_discovery의 vfx 배열에서 로드하거나 직접 작성.\n"
			"\n"
			"[필수 필드]\n"
			"  tag            VFX 태그 (예: VFX.Hit.Slash, VFX.Cast.Fire)\n"
			"  description    이펙트 설명 (한글 가능)\n"
			"  visual_design  이미터 구성, 색상, 스케일 등 시각 명세\n"
			"\n"
			"[visual_design 하위]\n"
			"  emitter_layers[]  이미터 레이어 배열\n"
			"    - name           이미터 이름 (예: SlashCore, Sparks)\n"
			"    - role           역할 (core|spark|trail|smoke|debris|ring|shockwave)\n"
			"    - renderer       렌더 방식 (sprite|mesh|ribbon)\n"
			"    - needs_custom_texture   커스텀 텍스처 필요 여부\n"
			"    - needs_custom_material  커스텀 머티리얼 필요 여부\n"
			"  color_palette     primary/secondary/accent RGB (HDR, 1.0 이상 가능)\n"
			"  looping           반복 여부 (hit=false, ambient=true)\n"
			"  duration_hint     지속 시간 초 (0.3~무한)\n"
			"  scale_hint        크기 (small|medium|large|massive)\n"
			"  intensity         강도 (subtle|normal|intense|extreme)\n"
			"\n"
			"[선택 필드]\n"
			"  event_type     이벤트 종류 (Hit|Cast|Buff|Ambient|Projectile)\n"
			"  element        속성 (Physical|Fire|Ice|Lightning|Dark|Holy)\n"
			"  source_skill   스킬 태그 (예: Ability.Attack.Basic)\n"
			"  source_items   아이템 태그 배열\n"
			"  usage_context  사용 맥락 (on_hit|on_cast|while_active|on_death)"
		);

	case EHktGeneratorType::Character:
		return TEXT(
			"Character Intent — asset_discovery의 characters 배열에서 로드하거나 직접 작성.\n"
			"\n"
			"[필수 필드]\n"
			"  tag                  엔티티 태그 (예: Entity.Character.Goblin)\n"
			"  description          외형 설명 (한글 가능)\n"
			"  skeleton_type        스켈레톤 (humanoid|quadruped|custom)\n"
			"\n"
			"[선택 필드]\n"
			"  required_animations  필요 애니메이션 태그 배열\n"
			"                       (예: Anim.FullBody.Action.Attack)"
		);

	case EHktGeneratorType::Item:
		return TEXT(
			"Item Intent — asset_discovery의 items 배열에서 로드하거나 직접 작성.\n"
			"\n"
			"[필수 필드]\n"
			"  tag          아이템 태그 (예: Entity.Item.Weapon.Sword.Fire)\n"
			"  description  외형 설명 (한글 가능)\n"
			"  category     분류 (Weapon|Armor|Accessory|Consumable)\n"
			"  sub_type     세부 종류 (Sword|Shield|Ring|Potion 등)\n"
			"\n"
			"[선택 필드]\n"
			"  element      속성 (Fire|Ice|Lightning 등, 없으면 null)\n"
			"  rarity       희귀도 0.0(Common) ~ 1.0(Legendary)"
		);

	case EHktGeneratorType::Map:
		return TEXT(
			"Map Intent — concept_design 출력 전체를 로드하거나 직접 작성.\n"
			"\n"
			"[terrain_spec 필수 필드]\n"
			"  landscape\n"
			"    size_x/size_y      랜드스케이프 크기 (예: 8161)\n"
			"    heightmap_type     높이맵 (procedural|flat|imported)\n"
			"    biome              환경 (forest|desert|snow|volcanic 등)\n"
			"    material_layers    머티리얼 레이어 배열 (예: [grass, dirt, rock])\n"
			"  regions[]\n"
			"    name               지역 이름\n"
			"    bounds             min/max [x,y,z] 좌표\n"
			"    properties         난이도, 분위기 등\n"
			"  spawners[]\n"
			"    entity_tag         스폰할 엔티티 태그\n"
			"    position           [x,y,z] 좌표\n"
			"    spawn_rules        min_count, max_count, respawn_time\n"
			"    region             소속 지역 이름\n"
			"\n"
			"[선택 필드]\n"
			"  reuse_map_id   기존 맵 재사용 시 ID\n"
			"  stories[]      스토리 참조 목록 (story_tag, region)"
		);

	case EHktGeneratorType::Story:
		return TEXT(
			"Story Intent — concept_design의 stories 배열에서 로드하거나 직접 작성.\n"
			"\n"
			"[필수 필드]\n"
			"  title         스토리 제목\n"
			"  description   스토리 설명 (한글 가능)\n"
			"  story_tag     스토리 태그 (예: Story.Quest.GoblinRaid)\n"
			"\n"
			"[선택 필드]\n"
			"  region        관련 지역 이름\n"
			"\n"
			"Story Generator는 이 정보를 바탕으로 story_schema.json의\n"
			"오퍼레이션(SpawnEntity, ApplyDamage, PlayVFX 등)을 사용해\n"
			"실행 가능한 Story JSON 바이트코드를 생성합니다."
		);

	case EHktGeneratorType::Texture:
		return TEXT(
			"Texture Intent — 직접 작성하거나 다른 Generator가 자동 요청.\n"
			"\n"
			"[필수 필드]\n"
			"  usage          용도 (아래 참조)\n"
			"  prompt         생성 프롬프트 (영어 권장)\n"
			"\n"
			"[usage 종류와 권장 해상도]\n"
			"  ParticleSprite   파티클 스프라이트 (256px)\n"
			"  Flipbook4x4      4x4 플립북 (512px)\n"
			"  Flipbook8x8      8x8 플립북 (1024px)\n"
			"  ItemIcon          아이템 아이콘 (256px)\n"
			"  MaterialBase      PBR 알베도 (512~1024px)\n"
			"  MaterialNormal    노멀맵 (512~1024px)\n"
			"  MaterialMask      마스크맵 (512~1024px)\n"
			"  Noise             노이즈 텍스처 (256px)\n"
			"  Gradient          그래디언트 (256px)\n"
			"\n"
			"[선택 필드]\n"
			"  negativePrompt   제외할 요소\n"
			"  resolution       해상도 (128/256/512/1024)\n"
			"  alphaChannel     알파 채널 여부\n"
			"  tileable         타일링 가능 여부\n"
			"  styleKeywords    스타일 키워드 배열"
		);

	case EHktGeneratorType::Feature:
		return TEXT(
			"Feature Intent — concept_design의 feature_outlines에서 로드하거나 직접 작성.\n"
			"\n"
			"[필수 필드]\n"
			"  feature_id     고유 식별자 (예: fire-magic)\n"
			"  name           사람이 읽을 수 있는 이름\n"
			"  description    고수준 설명\n"
			"\n"
			"[선택 필드]\n"
			"  category       분류 (combat, encounter, exploration, system)\n"
			"  priority       우선순위 (high, medium, low)"
		);

	default:
		return TEXT("");
	}
}

/** Generator별 Intent 예제 JSON */
inline FString GetIntentExample(EHktGeneratorType Type)
{
	switch (Type)
	{
	case EHktGeneratorType::VFX:
		return TEXT(
			"{\n"
			"  \"vfx\": [\n"
			"    {\n"
			"      \"tag\": \"VFX.Hit.Slash\",\n"
			"      \"description\": \"근접 베기 히트 이펙트\",\n"
			"      \"event_type\": \"Hit\",\n"
			"      \"element\": \"Physical\",\n"
			"      \"usage_context\": \"on_hit\",\n"
			"      \"visual_design\": {\n"
			"        \"emitter_layers\": [\n"
			"          {\n"
			"            \"name\": \"SlashCore\",\n"
			"            \"role\": \"core\",\n"
			"            \"renderer\": \"sprite\",\n"
			"            \"needs_custom_texture\": true,\n"
			"            \"needs_custom_material\": true\n"
			"          },\n"
			"          {\n"
			"            \"name\": \"Sparks\",\n"
			"            \"role\": \"spark\",\n"
			"            \"renderer\": \"sprite\",\n"
			"            \"needs_custom_texture\": false,\n"
			"            \"needs_custom_material\": false\n"
			"          }\n"
			"        ],\n"
			"        \"color_palette\": {\n"
			"          \"primary\": { \"r\": 2.0, \"g\": 2.0, \"b\": 2.0 },\n"
			"          \"secondary\": { \"r\": 1.5, \"g\": 1.5, \"b\": 1.5 },\n"
			"          \"accent\": { \"r\": 3.0, \"g\": 3.0, \"b\": 3.0 }\n"
			"        },\n"
			"        \"looping\": false,\n"
			"        \"duration_hint\": 0.5,\n"
			"        \"scale_hint\": \"small\",\n"
			"        \"intensity\": \"normal\"\n"
			"      }\n"
			"    }\n"
			"  ]\n"
			"}"
		);

	case EHktGeneratorType::Character:
		return TEXT(
			"{\n"
			"  \"characters\": [\n"
			"    {\n"
			"      \"tag\": \"Entity.Character.Goblin\",\n"
			"      \"description\": \"작은 녹색 고블린, 단검을 들고 있다\",\n"
			"      \"skeleton_type\": \"humanoid\",\n"
			"      \"required_animations\": [\n"
			"        \"Anim.FullBody.Action.Attack\",\n"
			"        \"Anim.FullBody.Action.Death\"\n"
			"      ]\n"
			"    }\n"
			"  ]\n"
			"}"
		);

	case EHktGeneratorType::Item:
		return TEXT(
			"{\n"
			"  \"items\": [\n"
			"    {\n"
			"      \"tag\": \"Entity.Item.Weapon.Sword.Fire\",\n"
			"      \"description\": \"불꽃이 감싸는 전설의 장검\",\n"
			"      \"category\": \"Weapon\",\n"
			"      \"sub_type\": \"Sword\",\n"
			"      \"element\": \"Fire\",\n"
			"      \"rarity\": 0.8\n"
			"    }\n"
			"  ]\n"
			"}"
		);

	case EHktGeneratorType::Map:
		return TEXT(
			"{\n"
			"  \"terrain_spec\": {\n"
			"    \"landscape\": {\n"
			"      \"size_x\": 8161,\n"
			"      \"size_y\": 8161,\n"
			"      \"heightmap_type\": \"procedural\",\n"
			"      \"biome\": \"forest\",\n"
			"      \"material_layers\": [\"grass\", \"dirt\", \"rock\"]\n"
			"    },\n"
			"    \"regions\": [\n"
			"      {\n"
			"        \"name\": \"DarkForest\",\n"
			"        \"bounds\": {\n"
			"          \"min\": [0, 0, 0],\n"
			"          \"max\": [4000, 4000, 1000]\n"
			"        },\n"
			"        \"properties\": {\n"
			"          \"difficulty\": \"hard\",\n"
			"          \"ambient\": \"dark_forest\"\n"
			"        }\n"
			"      }\n"
			"    ],\n"
			"    \"spawners\": [\n"
			"      {\n"
			"        \"entity_tag\": \"Entity.Character.Goblin\",\n"
			"        \"position\": [1000, 2000, 0],\n"
			"        \"spawn_rules\": {\n"
			"          \"min_count\": 3,\n"
			"          \"max_count\": 5,\n"
			"          \"respawn_time\": 60\n"
			"        },\n"
			"        \"region\": \"DarkForest\"\n"
			"      }\n"
			"    ]\n"
			"  },\n"
			"  \"stories\": [\n"
			"    {\n"
			"      \"title\": \"고블린 소탕\",\n"
			"      \"story_tag\": \"Story.Quest.GoblinRaid\",\n"
			"      \"region\": \"DarkForest\"\n"
			"    }\n"
			"  ]\n"
			"}"
		);

	case EHktGeneratorType::Story:
		return TEXT(
			"{\n"
			"  \"stories\": [\n"
			"    {\n"
			"      \"title\": \"고블린 소탕\",\n"
			"      \"description\": \"마을을 위협하는 고블린 무리를 처치하라\",\n"
			"      \"story_tag\": \"Story.Quest.GoblinRaid\",\n"
			"      \"region\": \"DarkForest\"\n"
			"    }\n"
			"  ]\n"
			"}"
		);

	case EHktGeneratorType::Texture:
		return TEXT(
			"{\n"
			"  \"usage\": \"ParticleSprite\",\n"
			"  \"prompt\": \"fire particle sprite, orange flame, vfx, game asset, black background\",\n"
			"  \"negativePrompt\": \"text, watermark, realistic photo\",\n"
			"  \"resolution\": 256,\n"
			"  \"alphaChannel\": true,\n"
			"  \"tileable\": false,\n"
			"  \"styleKeywords\": [\"stylized\", \"hand-painted\"]\n"
			"}"
		);

	case EHktGeneratorType::Feature:
		return TEXT(
			"{\n"
			"  \"feature_outlines\": [\n"
			"    {\n"
			"      \"feature_id\": \"fire-magic\",\n"
			"      \"name\": \"화염 마법 시스템\",\n"
			"      \"category\": \"combat\",\n"
			"      \"description\": \"화염 속성 스킬 3종과 관련 이펙트\",\n"
			"      \"priority\": \"high\"\n"
			"    }\n"
			"  ]\n"
			"}"
		);

	default:
		return TEXT("{\n  \n}");
	}
}

/** Generator별 자연어 입력 힌트 예시 */
inline FString GetNLPlaceholder(EHktGeneratorType Type)
{
	switch (Type)
	{
	case EHktGeneratorType::VFX:
		return TEXT("빨간 불꽃이 튀는 검기 베기 이펙트");
	case EHktGeneratorType::Character:
		return TEXT("녹색 피부의 작은 고블린, 단검을 들고 있다");
	case EHktGeneratorType::Item:
		return TEXT("불꽃이 감싸는 전설의 장검");
	case EHktGeneratorType::Map:
		return TEXT("어두운 숲 지역, 고블린 3~5마리 스폰, 난이도 hard");
	case EHktGeneratorType::Story:
		return TEXT("마을을 위협하는 고블린을 처치하는 퀘스트");
	case EHktGeneratorType::Texture:
		return TEXT("불꽃 파티클 스프라이트, 검정 배경, 스타일라이즈");
	case EHktGeneratorType::Feature:
		return TEXT("화염 마법, 고블린 캠프 조우, 보물 탐색 시스템");
	default:
		return TEXT("");
	}
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
