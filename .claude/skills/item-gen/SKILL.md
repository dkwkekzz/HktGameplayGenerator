---
name: item-gen
description: asset_discovery의 아이템 명세를 기반으로 아이템 메시와 아이콘을 생성하는 아이템 생성 스텝.
allowed-tools: Bash, Read, Write, Grep, Glob
argument-hint: <project_id>
---

# item_generation 스텝 실행

asset_discovery 출력의 items 명세를 받아 **아이템 메시, 머티리얼, 아이콘**을 생성한다.

## 인자

- `$1` — project_id

## 선행 조건

- `asset_discovery` 스텝이 completed 상태여야 한다

## 실행 절차

### 1. 스텝 시작
MCP 도구 `step_begin`을 호출한다:
- `project_id`: $1
- `step_type`: "item_generation"

### 2. 소켓 매핑 확인
MCP 도구 `get_socket_mappings`로 아이템 장착 소켓 정보를 확인한다.

### 3. 아이템별 생성
각 아이템 명세에 대해:

**메시 생성:**
1. `request_item` — 외부 아이템 생성 요청 (description, category, sub_type 전달)
2. 생성 완료 대기 또는 `get_pending_item_requests`로 상태 확인
3. `import_item_mesh` — 생성된 메시를 UE5에 임포트

**아이콘 생성 (선택):**
1. `generate_texture` — 아이콘 텍스처 생성 (SD WebUI 사용)
2. `import_texture` — 생성된 텍스처 임포트

### 4. 어셋 경로 확인
Tag 규칙 준수 확인:
- 메시: `{Root}/Items/{Cat}/SM_{Sub}`
- 예: `Entity.Item.Weapon.Sword` → `/Game/Generated/Items/Weapon/SM_Sword`

### 5. 출력 저장
MCP 도구 `step_save_output`을 호출:
- `step_type`: "item_generation"
- `output_json`:

```json
{
  "generated_assets": [
    {
      "tag": "Entity.Item.Weapon.GoblinDagger",
      "asset_path": "/Game/Generated/Items/Weapon/SM_GoblinDagger",
      "mesh_path": "/Game/Generated/Items/Weapon/SM_GoblinDagger",
      "icon_path": "/Game/Generated/Items/Weapon/T_GoblinDagger_Icon",
      "material_path": "/Game/Generated/Items/Weapon/M_GoblinDagger"
    }
  ]
}
```

## 실패 처리

- 개별 아이템 생성 실패 시 해당 항목만 건너뛰고 계속 진행
- 모든 아이템이 실패하면 `step_fail` 호출
