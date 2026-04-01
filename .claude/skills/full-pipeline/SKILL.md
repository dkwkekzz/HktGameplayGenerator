---
name: full-pipeline
description: 전체 게임플레이 생성 파이프라인을 오케스트레이션한다. 컨셉부터 어셋 생성까지, feature별 Worker Agent를 병렬 스폰하여 효율적으로 실행한다.
allowed-tools: Bash, Read, Write, Grep, Glob
argument-hint: <project_name> <concept_text>
---

# 전체 파이프라인 실행 (Orchestrator)

사용자 컨셉에서 완성된 게임플레이 어셋까지, **feature별 Worker Agent를 병렬 스폰**하여 실행한다.

## 인자

- `$1` — 프로젝트 이름
- 나머지 인자 — 컨셉 텍스트

## 파이프라인 의존 관계

```
concept_design
  ├─→ map_generation              (병렬)
  └─→ feature_design
        └─→ [feature별 Worker Agent 병렬 스폰]
              ├─→ story_generation
              ├─→ asset_discovery
              └─→ char/item/vfx_generation (병렬)
```

## 실행 절차

### Phase 0: 프로젝트 생성
MCP 도구 `step_create_project`을 호출:
- `name`: $1
- `concept`: 컨셉 텍스트

project_id를 기록해둔다.

### Phase 1: 컨셉 설계 (직접 실행)
`/concept-design {project_id} {concept_text}` 실행

완료 확인: `step_get_status`로 concept_design이 completed인지 확인
결과: terrain_spec + feature_outlines[]

### Phase 2: Feature 설계 + 맵 생성 (병렬)
다음 두 스텝을 **병렬로** 실행:
- `/feature-design {project_id}` — feature_outlines를 상세 설계
- `/map-gen {project_id}` — terrain_spec으로 맵 생성

두 스텝 모두 completed가 될 때까지 대기.
결과: features[] (각 feature에 stories, expected_assets, map_requirements)

### Phase 3: Feature별 Worker Agent 스폰 (병렬)
feature_design output의 features[]를 읽고, 각 feature에 대해 Worker Agent를 **병렬 스폰**한다.

**중요: 단일 메시지에 모든 Agent 호출을 포함하여 진정한 병렬 실행을 보장한다.**

```
for each feature in features:
  Agent(
    prompt="/feature-worker {project_id} {feature_id}",
    description="Worker: {feature_name}",
    run_in_background=true
  )
```

각 Worker는 자신의 feature에 대해:
1. story_generation (해당 feature의 stories만)
2. asset_discovery (해당 feature의 story_files만)
3. char/item/vfx_generation (해당 feature의 에셋만, 병렬)
4. feature_save_work로 결과 저장

### Phase 4: 완료 대기
모든 Worker Agent의 완료를 대기한다.
(run_in_background=true로 스폰했으므로 자동 통지됨)

### Phase 5: 결과 집계
MCP 도구 `feature_aggregate`를 호출하여 모든 feature의 work.json을 통합:
- story_generation/output.json 생성
- asset_discovery/output.json 생성
- character/item/vfx_generation/output.json 생성

### Phase 6: 최종 보고
`step_get_status`와 `step_list_features`로 전체 상태를 확인하고 사용자에게 보고:
- 각 feature의 성공/실패 상태
- 생성된 에셋 목록 (feature별)
- 실패한 항목과 에러 메시지

## 중단 및 재개

- 각 feature Worker는 독립적이므로 실패한 feature만 재실행 가능
- `step_list_features`로 어떤 feature가 실패했는지 확인
- 해당 feature만 `/feature-worker {project_id} {feature_id}`로 재실행
- 이미 completed인 feature는 건너뛴다

## 예시

```
/full-pipeline 고블린마을 중세 판타지 마을에 고블린 부족이 침공하는 시나리오. 마을 외곽에 고블린 캠프가 있고 보스 고블린 킹이 등장한다. 화염 마법과 얼음 마법을 사용하는 마법사가 있다.
```

이 경우 concept_design이 다음과 같은 feature_outlines를 생성할 수 있다:
- `goblin-camp` — 고블린 캠프 조우 (encounter)
- `goblin-king-boss` — 고블린 킹 보스전 (encounter)
- `fire-magic` — 화염 마법 시스템 (combat)
- `ice-magic` — 얼음 마법 시스템 (combat)

4개의 Worker가 병렬로 스폰되어 동시에 작업한다.
