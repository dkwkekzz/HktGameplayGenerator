---
name: char-gen
description: asset_discovery의 캐릭터 명세를 기반으로 메시와 애니메이션을 생성하는 캐릭터 생성 스텝.
allowed-tools: Bash, Read, Write, Grep, Glob
argument-hint: <project_id>
---

# character_generation 스텝 실행

asset_discovery 출력의 characters 명세를 받아 **캐릭터 메시와 애니메이션**을 생성한다.

## 인자

- `$1` — project_id

## 선행 조건

- `asset_discovery` 스텝이 completed 상태여야 한다

## 실행 절차

### 1. 스텝 시작
MCP 도구 `step_begin`을 호출한다:
- `project_id`: $1
- `step_type`: "character_generation"

### 2. 스켈레톤 풀 확인
MCP 도구 `get_skeleton_pool`로 사용 가능한 스켈레톤 목록을 확인한다.

### 3. 캐릭터별 생성
각 캐릭터 명세에 대해:

**메시 생성:**
1. `request_character_mesh` — 외부 생성 요청 (description, skeleton_type 전달)
2. 생성 완료 대기 또는 `get_pending_mesh_requests`로 상태 확인
3. `import_mesh` — 생성된 메시를 UE5에 임포트

**애니메이션 생성:**
각 required_animation에 대해:
1. `request_animation` — 애니메이션 생성 요청
2. `import_animation` — 생성된 애니메이션 임포트

### 4. 어셋 경로 확인
생성된 어셋의 경로가 Tag 규칙을 따르는지 확인:
- 메시: `{Root}/Characters/{Name}/BP_{Name}`
- 애니메이션: `{Root}/Animations/Anim_{Layer}_{Type}_{Name}`

### 5. 출력 저장
MCP 도구 `step_save_output`을 호출:
- `step_type`: "character_generation"
- `output_json`:

```json
{
  "generated_assets": [
    {
      "tag": "Entity.Character.Goblin",
      "asset_path": "/Game/Generated/Characters/Goblin/BP_Goblin",
      "mesh_path": "/Game/Generated/Characters/Goblin/SK_Goblin",
      "animations": [
        {"tag": "Anim.FullBody.Action.Attack", "path": "/Game/Generated/Animations/Anim_FullBody_Action_Attack"},
        {"tag": "Anim.FullBody.Action.Death", "path": "/Game/Generated/Animations/Anim_FullBody_Action_Death"}
      ]
    }
  ]
}
```

## 실패 처리

- 개별 캐릭터 생성 실패 시 해당 항목만 건너뛰고 계속 진행
- 모든 캐릭터가 실패하면 `step_fail` 호출
