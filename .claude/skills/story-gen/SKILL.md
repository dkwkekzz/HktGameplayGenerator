---
name: story-gen
description: concept_design의 스토리 리스트를 기반으로 HktCore용 Story JSON 파일들을 생성하고 빌드하는 스토리 생성 스텝.
allowed-tools: Bash, Read, Write, Grep, Glob
argument-hint: <project_id>
---

# story_generation 스텝 실행

concept_design 출력의 stories 리스트를 받아 **HktCore 주입용 Story JSON 파일들**을 생성한다.

## 인자

- `$1` — project_id

## 선행 조건

- `concept_design` 스텝이 completed 상태여야 한다

## 실행 절차

### 1. 스텝 시작
MCP 도구 `step_begin`을 호출한다:
- `project_id`: $1
- `step_type`: "story_generation"

### 2. 스키마 및 예제 확인
순서대로 호출:
1. `get_story_schema` → Story JSON 스키마 확인
2. `get_story_examples` → 기존 패턴 학습

### 3. 스토리별 JSON 생성
concept_design 출력의 각 story에 대해:
- `title`, `description`, `story_tag`, `region` 정보를 활용
- Story 스키마에 맞는 JSON을 생성
- 스토리 내 등장하는 캐릭터/아이템/VFX의 Tag를 Tag 규칙에 맞게 부여:
  - 캐릭터: `Entity.Character.{Name}`
  - 아이템: `Entity.Item.{Cat}.{Sub}`
  - VFX: `VFX.{Event}.{Element}`
  - 애니메이션: `Anim.{Layer}.{Type}.{Name}`

### 4. 스토리 검증 및 빌드
각 스토리에 대해:
1. `validate_story` → 문법 검증
2. `analyze_story_dependencies` → 의존 어셋 목록 확인
3. `build_story` → 컴파일 & VM 등록

### 5. 출력 저장
MCP 도구 `step_save_output`을 호출:
- `step_type`: "story_generation"
- `output_json`:

```json
{
  "story_files": [
    {
      "story_tag": "Story.Quest.GoblinRaid",
      "json_path": ".hkt_steps/{project_id}/story_generation/Story.Quest.GoblinRaid.json",
      "built": true,
      "build_errors": []
    }
  ]
}
```

## 주의사항

- Tag 명명 규칙을 반드시 준수한다
- validate_story 실패 시 오류를 수정하고 재검증
- build_story 실패 시 build_errors에 기록하고 계속 진행
- 모든 스토리가 빌드 실패하면 `step_fail` 호출

## 후속 스텝

story_generation 완료 후:
- `/asset-discovery $1` — 스토리에서 의존 어셋 분석
