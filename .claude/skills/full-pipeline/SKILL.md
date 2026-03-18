---
name: full-pipeline
description: 전체 게임플레이 생성 파이프라인을 순서대로 오케스트레이션한다. 컨셉부터 어셋 생성까지 7개 스텝을 의존 관계에 맞게 실행한다.
allowed-tools: Bash, Read, Write, Grep, Glob
argument-hint: <project_name> <concept_text>
---

# 전체 파이프라인 실행

사용자 컨셉에서 완성된 게임플레이 어셋까지 **7개 스텝을 순서대로** 실행한다.

## 인자

- `$1` — 프로젝트 이름
- 나머지 인자 — 컨셉 텍스트

## 파이프라인 의존 관계

```
concept_design
  ├─→ map_generation      (병렬)
  └─→ story_generation    (병렬)
        └─→ asset_discovery
              ├─→ character_generation  (병렬)
              ├─→ item_generation       (병렬)
              └─→ vfx_generation        (병렬)
```

## 실행 절차

### Phase 0: 프로젝트 생성
MCP 도구 `step_create_project`을 호출:
- `name`: $1
- `concept`: 컨셉 텍스트

project_id를 기록해둔다.

### Phase 1: 컨셉 설계
`/concept-design {project_id} {concept_text}` 실행

완료 확인: `step_get_status`로 concept_design이 completed인지 확인

### Phase 2: 맵 + 스토리 (병렬)
다음 두 스텝을 **병렬로** 실행:
- `/map-gen {project_id}`
- `/story-gen {project_id}`

두 스텝 모두 completed가 될 때까지 대기

### Phase 3: 어셋 탐색
`/asset-discovery {project_id}` 실행

완료 확인 후 어셋 명세 확인

### Phase 4: 어셋 생성 (병렬)
다음 세 스텝을 **병렬로** 실행:
- `/char-gen {project_id}` — characters가 있을 때만
- `/item-gen {project_id}` — items가 있을 때만
- `/vfx-gen {project_id}` — vfx가 있을 때만

### Phase 5: 완료 보고
`step_get_status`로 전체 상태를 확인하고 사용자에게 보고:
- 각 스텝의 성공/실패 상태
- 생성된 어셋 목록
- 실패한 항목과 에러 메시지

## 중단 및 재개

- 각 스텝은 독립적이므로 실패한 스텝만 재실행 가능
- `step_get_status`로 어디서 중단되었는지 확인 후 해당 스텝부터 재개
- 이미 completed인 스텝은 건너뛴다

## 예시

```
/full-pipeline 고블린마을 중세 판타지 마을에 고블린 부족이 침공하는 시나리오. 마을 외곽에 고블린 캠프가 있고 보스 고블린 킹이 등장한다.
```
