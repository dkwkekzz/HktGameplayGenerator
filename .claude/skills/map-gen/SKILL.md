---
name: map-gen
description: concept_design의 terrain_spec을 기반으로 HktMap JSON을 생성하고 UE5에 빌드하는 맵 생성 스텝. Landscape, Spawner, Region, Story 참조를 정의한다.
allowed-tools: Bash, Read, Write, Grep, Glob
argument-hint: <project_id>
---

# map_generation 스텝 실행

concept_design 출력의 terrain_spec을 받아 **HktMap JSON**을 생성하고 UE5에 빌드한다.

## 인자

- `$1` — project_id

## 선행 조건

- `concept_design` 스텝이 completed 상태여야 한다

## 실행 절차

### 1. 스텝 시작
MCP 도구 `step_begin`을 호출한다:
- `project_id`: $1
- `step_type`: "map_generation"
- `input_json`: 비워두면 concept_design 출력에서 자동 해석

### 2. 맵 스키마 확인
MCP 도구 `get_map_schema`로 HktMap JSON 스키마를 확인한다.

### 3. HktMap JSON 생성
terrain_spec의 landscape, spawners, regions 데이터를 HktMap 스키마에 맞게 변환:

- **Landscape**: heightmap, 크기, biome, material layer 설정
- **Spawners**: entity_tag, position, spawn_rules 매핑
- **Regions**: 이름, bounds, properties 매핑
- **Story 참조**: concept_design의 stories에서 story_tag 연결
- **Props**: 필요 시 정적 오브젝트 배치

### 4. 맵 유효성 검사
MCP 도구 `validate_map`으로 생성된 JSON을 검증한다.

### 5. 맵 저장
MCP 도구 `save_map`으로 HktMap JSON을 저장한다.

### 6. (선택) 맵 빌드
MCP 도구 `build_map`으로 UE5에 맵을 빌드한다.
- UE5 에디터가 연결되어 있을 때만 실행

### 7. 출력 저장
MCP 도구 `step_save_output`을 호출:
- `step_type`: "map_generation"
- `output_json`:

```json
{
  "map_id": "dark-forest-20260318",
  "map_path": ".hkt_maps/dark-forest-20260318.json",
  "hkt_map": { ... }
}
```

## 실패 처리

- validate_map 실패 시 오류를 수정하고 재검증
- 3회 이상 실패 시 `step_fail` 호출
