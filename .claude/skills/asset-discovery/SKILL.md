---
name: asset-discovery
description: story_generation 결과에서 필요한 캐릭터/아이템/VFX 어셋을 분석하고 명세를 생성하는 어셋 탐색 스텝.
allowed-tools: Bash, Read, Write, Grep, Glob
argument-hint: <project_id>
---

# asset_discovery 스텝 실행

story_generation 출력의 스토리 파일들을 분석하여 **필요한 어셋 명세(characters, items, vfx)**를 추출한다.

## 인자

- `$1` — project_id

## 선행 조건

- `story_generation` 스텝이 completed 상태여야 한다

## 실행 절차

### 1. 스텝 시작
MCP 도구 `step_begin`을 호출한다:
- `project_id`: $1
- `step_type`: "asset_discovery"

### 2. 스토리 파일 분석
story_generation 출력의 각 story_file을 읽고:
- `analyze_story_dependencies`로 의존 어셋 태그 추출
- 또는 Story JSON에서 직접 Entity/VFX/Anim 태그 파싱

### 3. 기존 어셋 확인
MCP 도구 `list_assets` 또는 `search_assets`로 이미 존재하는 어셋을 확인한다.
- 존재하는 어셋은 생성 대상에서 제외

### 4. 어셋 명세 생성

**characters** — 각 캐릭터:
- `tag`: `Entity.Character.{Name}` 형식
- `description`: 외형/역할 설명
- `skeleton_type`: 스켈레톤 타입 (humanoid, quadruped 등)
- `required_animations`: 필요한 애니메이션 태그 목록

**items** — 각 아이템:
- `tag`: `Entity.Item.{Cat}.{Sub}` 형식
- `description`: 아이템 설명
- `category`: Weapon, Armor, Consumable 등
- `sub_type`: 세부 분류
- `element`: 속성 (Fire, Ice 등)

**vfx** — 각 VFX:
- `tag`: `VFX.{Event}.{Element}` 형식
- `description`: 이펙트 설명
- `event_type`: Explosion, Hit, Buff 등
- `element`: Fire, Ice, Lightning 등

### 5. 출력 저장
MCP 도구 `step_save_output`을 호출:
- `step_type`: "asset_discovery"
- `output_json`:

```json
{
  "characters": [
    {
      "tag": "Entity.Character.Goblin",
      "description": "작은 녹색 고블린, 단검 소지",
      "skeleton_type": "humanoid",
      "required_animations": ["Anim.FullBody.Action.Attack", "Anim.FullBody.Action.Death"]
    }
  ],
  "items": [
    {
      "tag": "Entity.Item.Weapon.GoblinDagger",
      "description": "고블린이 사용하는 녹슨 단검",
      "category": "Weapon",
      "sub_type": "Dagger",
      "element": null
    }
  ],
  "vfx": [
    {
      "tag": "VFX.Hit.Slash",
      "description": "근접 베기 히트 이펙트",
      "event_type": "Hit",
      "element": "Physical"
    }
  ]
}
```

## 후속 스텝

asset_discovery 완료 후 다음 스텝이 병렬 실행 가능:
- `/char-gen $1` — 캐릭터 메시 + 애니메이션 생성
- `/item-gen $1` — 아이템 메시 + 아이콘 생성
- `/vfx-gen $1` — Niagara VFX 생성
