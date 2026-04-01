---
name: story-gen
description: feature_design의 feature별 스토리를 기반으로 HktCore용 Story JSON 파일들을 생성하고 빌드하는 스토리 생성 스텝.
allowed-tools: Bash, Read, Write, Grep, Glob
argument-hint: <project_id> [feature_id]
---

# story_generation 스텝 실행

feature_design 출력의 features[].stories를 받아 **HktCore 주입용 Story JSON 파일들**을 생성한다.

## 인자

- `$1` — project_id
- `$2` — (선택) feature_id — 지정 시 해당 feature의 스토리만 처리

## 선행 조건

- `feature_design` 스텝이 completed 상태여야 한다

## 실행 절차

### 1. 스텝 시작
MCP 도구 `step_begin`을 호출한다:
- `project_id`: $1
- `step_type`: "story_generation"
- `input_json`: feature_id가 있으면 `{"feature_id": "$2"}` 포함

### 2. 스키마 확인
`story_schema.json` 파일을 읽어 사용 가능한 operations, registers, propertyIds를 확인한다:
- 경로: `.claude/skills/story-gen/story_schema.json`

### 3. Feature별 스토리 로드
feature_design output에서:
- feature_id가 지정된 경우: 해당 feature의 stories만 추출
- 지정되지 않은 경우: 모든 features의 stories를 순차 처리

### 4. 스토리별 JSON 생성
각 story에 대해:
- `title`, `description`, `story_tag`, `region` 정보를 활용
- Story 스키마에 맞는 JSON을 생성
- 스토리 내 등장하는 캐릭터/아이템/VFX의 Tag를 Tag 규칙에 맞게 부여:
  - 캐릭터: `Entity.Character.{Name}`
  - 아이템: `Entity.Item.{Cat}.{Sub}`
  - VFX: `VFX.{Event}.{Element}`
  - 애니메이션: `Anim.{Layer}.{Type}.{Name}`

### 5. 스토리 검증 및 빌드
각 스토리에 대해:
1. `validate_story` → 문법 검증
2. `analyze_story_dependencies` → 의존 어셋 목록 확인
3. `build_story` → 컴파일 & VM 등록

### 6. 출력 저장
MCP 도구 `step_save_output`을 호출:
- `step_type`: "story_generation"
- `output_json`:

```json
{
  "story_files": [
    {
      "story_tag": "Story.Quest.GoblinRaid",
      "feature_id": "goblin-raid",
      "json_path": ".hkt_steps/{project_id}/story_generation/Story.Quest.GoblinRaid.json",
      "built": true,
      "build_errors": []
    }
  ]
}
```

**중요**: 각 story_file에 `feature_id`를 반드시 포함하여 추적 가능하게 한다.

## Worker Agent에서 호출 시

`/feature-worker`가 이 스킬을 호출할 때는:
- feature_id가 지정됨
- 해당 feature의 stories만 처리
- 결과를 `feature_save_work`의 story_files로 저장 (step_save_output 대신)

## 주의사항

- Tag 명명 규칙을 반드시 준수한다
- validate_story 실패 시 오류를 수정하고 재검증
- build_story 실패 시 build_errors에 기록하고 계속 진행
- 모든 스토리가 빌드 실패하면 `step_fail` 호출

## 후속 스텝

story_generation 완료 후:
- `/asset-discovery $1` — 스토리에서 의존 어셋 분석
