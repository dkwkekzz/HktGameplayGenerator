---
name: feature-worker
description: 단일 feature에 대해 story_generation → asset_discovery → asset_generation을 실행하는 Worker Agent 스킬.
allowed-tools: Bash, Read, Write, Grep, Glob
argument-hint: <project_id> <feature_id>
---

# Feature Worker 실행

단일 feature에 대해 **스토리 생성 → 에셋 탐색 → 에셋 생성**을 순차 실행한다.
Orchestrator(full-pipeline)가 feature별로 이 Worker를 병렬 스폰한다.

## 인자

- `$1` — project_id
- `$2` — feature_id

## 선행 조건

- `feature_design` 스텝이 completed 상태여야 한다
- 해당 feature가 manifest에 등록되어 있어야 한다

## 실행 절차

### 1. Feature 데이터 로드
feature_design output에서 해당 feature_id의 데이터를 추출:
- `feature_load_work` 또는 `step_load_input`으로 feature_design output 로드
- features[] 배열에서 feature_id가 일치하는 항목을 찾음

### 2. Feature 상태 갱신
MCP 도구 `step_update_feature`:
- `project_id`: $1
- `feature_id`: $2
- `status`: "generating"

### 3. Story Generation
해당 feature의 stories[]에 대해 스토리 바이트코드를 생성:
- 각 story의 description을 기반으로 HktCore Story JSON 작성
- `build_story`로 유효성 검증
- 결과: story_files[] (story_tag, json_path, built, feature_id)

### 4. Asset Discovery
생성된 스토리 파일들을 분석하여 필요한 에셋 명세 추출:
- `analyze_story_dependencies`로 스토리 내 Entity/VFX/Anim 태그 파싱
- `list_assets` / `search_assets`로 기존 에셋 확인
- feature의 expected_assets를 참고하여 누락 에셋 확인
- VFX는 visual_design 명세까지 포함 (SKILL asset-discovery 참조)
- 결과: characters[], items[], vfx[] (각 항목에 feature_id 포함)

### 5. Asset Generation (병렬)
에셋 명세에 따라 생성 (있는 것만):
- characters가 있으면: 각 캐릭터 Mesh + Animation 생성
- items가 있으면: 각 아이템 Mesh + Icon 생성  
- vfx가 있으면: 각 VFX Niagara System 빌드

가능하면 병렬 실행.

### 6. Work 저장
MCP 도구 `feature_save_work`:
- `project_id`: $1
- `feature_id`: $2
- `work_json`: 아래 스키마

### 7. Feature 완료
MCP 도구 `step_update_feature`:
- `project_id`: $1
- `feature_id`: $2
- `status`: "completed"
- `stories_completed`: 완료된 스토리 수
- `assets_completed`: 생성된 에셋 수

## work.json 스키마

```json
{
  "feature_id": "fire-magic",
  "status": "completed",
  "agent_id": "worker-uuid",
  "started_at": "2026-04-01T10:00:00Z",
  "completed_at": "2026-04-01T10:05:00Z",
  "story_files": [
    {
      "story_tag": "Story.Skill.FireBall",
      "feature_id": "fire-magic",
      "json_path": ".hkt_steps/.../story_fire_ball.json",
      "built": true,
      "build_errors": []
    }
  ],
  "asset_discovery": {
    "characters": [
      {
        "tag": "Entity.Character.FireMage",
        "feature_id": "fire-magic",
        "description": "화염 마법사",
        "skeleton_type": "humanoid",
        "required_animations": ["Anim.FullBody.Action.Cast"]
      }
    ],
    "items": [],
    "vfx": [
      {
        "tag": "VFX.Hit.Fire",
        "feature_id": "fire-magic",
        "description": "화염 히트 이펙트",
        "event_type": "Hit",
        "element": "Fire",
        "usage_context": "on_hit",
        "visual_design": { ... }
      }
    ]
  },
  "generated_assets": [
    { "tag": "VFX.Hit.Fire", "type": "vfx", "asset_path": "/Game/Generated/VFX/NS_VFX_Hit_Fire" },
    { "tag": "Entity.Character.FireMage", "type": "character", "asset_path": "/Game/Generated/Characters/FireMage/BP_FireMage" }
  ],
  "errors": []
}
```

## 에러 처리

- 스토리 빌드 실패: build_errors에 기록하고 계속 진행
- 에셋 생성 실패: errors에 기록하고 계속 진행
- 전체 실패: `step_update_feature`로 status를 "failed"로 설정
- 부분 성공: 성공한 것만 generated_assets에 기록, 실패는 errors에 기록

## 격리 보장

- 이 Worker는 **자신의 feature work.json만** 읽고 씀
- manifest.json은 `step_update_feature`를 통해서만 갱신 (atomic write)
- 다른 Worker의 work.json에 접근하지 않음
