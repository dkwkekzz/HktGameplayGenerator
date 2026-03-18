---
name: concept-design
description: 사용자 컨셉 텍스트에서 지형 명세(terrain_spec)와 스토리 리스트를 설계하는 첫 번째 파이프라인 스텝. 맵과 스토리의 전체 구조를 결정한다.
allowed-tools: Bash, Read, Write, Grep, Glob
argument-hint: <project_id> <concept_text>
---

# concept_design 스텝 실행

사용자가 제공한 컨셉을 분석하여 **지형 명세(terrain_spec)**와 **스토리 리스트(stories)**를 생성한다.

## 인자

- `$1` — project_id (step_create_project로 생성된 프로젝트 ID)
- 나머지 인자 — 컨셉 텍스트 (없으면 프로젝트의 concept 필드 사용)

## 실행 절차

### 1. 스텝 시작
MCP 도구 `step_begin`을 호출한다:
- `project_id`: $1
- `step_type`: "concept_design"
- `input_json`: `{"concept": "<컨셉 텍스트>"}`

### 2. 컨셉 분석 및 설계
사용자 컨셉을 분석하여 다음을 설계한다:

**terrain_spec** (지형 명세):
- `landscape`: heightmap 설정, 크기, biome, material layer
- `spawners`: 엔티티 스폰 위치와 규칙 (위치, entity tag, 수량, 스폰 조건)
- `regions`: 이름 있는 영역과 경계 (bounds, 속성)
- `reuse_map_id`: 기존 맵 재사용 시 ID (없으면 null)

**stories** (스토리 리스트):
각 스토리는 다음을 포함:
- `title`: 스토리 제목
- `description`: 상세 설명
- `story_tag`: GameplayTag (예: `Story.Quest.DragonSlayer`)
- `region`: 해당 스토리가 진행되는 region 이름

### 3. 출력 저장
MCP 도구 `step_save_output`을 호출한다:
- `project_id`: $1
- `step_type`: "concept_design"
- `output_json`: 아래 스키마를 따르는 JSON

## 출력 스키마

```json
{
  "terrain_spec": {
    "reuse_map_id": null,
    "landscape": {
      "size_x": 8161, "size_y": 8161,
      "heightmap_type": "procedural",
      "biome": "forest",
      "material_layers": ["grass", "dirt", "rock"]
    },
    "spawners": [
      {
        "entity_tag": "Entity.Character.Goblin",
        "position": [1000, 2000, 0],
        "spawn_rules": {"min_count": 3, "max_count": 5, "respawn_time": 60},
        "region": "DarkForest"
      }
    ],
    "regions": [
      {
        "name": "DarkForest",
        "bounds": {"min": [0, 0, 0], "max": [4000, 4000, 1000]},
        "properties": {"difficulty": "hard", "ambient": "dark_forest"}
      }
    ]
  },
  "stories": [
    {
      "title": "고블린 소탕",
      "description": "마을을 위협하는 고블린 무리를 처치하라",
      "story_tag": "Story.Quest.GoblinRaid",
      "region": "DarkForest"
    }
  ]
}
```

## 설계 지침

- 컨셉에서 지형 특성을 추출하여 적절한 biome, 크기, 레이어를 결정
- 스토리와 region을 연결하여 공간적 서사가 자연스럽게 흐르도록 설계
- Spawner의 entity_tag는 Tag 규칙을 따름: `Entity.Character.{Name}`, `Entity.Item.{Cat}.{Sub}`
- 실패 시 `step_fail` 호출로 에러를 기록

## 후속 스텝

concept_design 완료 후 다음 스텝이 병렬 실행 가능:
- `/map-gen $1` — terrain_spec으로 HktMap 생성
- `/story-gen $1` — stories 리스트로 Story JSON 생성
