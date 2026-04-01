---
name: asset-discovery
description: story_generation 결과에서 필요한 캐릭터/아이템/VFX 어셋을 분석하고 명세를 생성하는 어셋 탐색 스텝.
allowed-tools: Bash, Read, Write, Grep, Glob
argument-hint: <project_id> [feature_id]
---

# asset_discovery 스텝 실행

story_generation 출력의 스토리 파일들을 분석하여 **필요한 어셋 명세(characters, items, vfx)**를 추출한다.

## 인자

- `$1` — project_id
- `$2` — (선택) feature_id — 지정 시 해당 feature의 story_files만 분석

## 선행 조건

- `story_generation` 스텝이 completed 상태여야 한다 (또는 feature worker에서 story_files가 준비됨)

## 실행 절차

### 1. 스텝 시작
MCP 도구 `step_begin`을 호출한다:
- `project_id`: $1
- `step_type`: "asset_discovery"

### 2. 스토리 파일 분석
story_generation 출력의 각 story_file을 읽고:
- feature_id가 지정된 경우: 해당 feature_id의 story_files만 필터링
- `analyze_story_dependencies`로 의존 어셋 태그 추출
- 또는 Story JSON에서 직접 Entity/VFX/Anim 태그 파싱
- **각 에셋에 story_file의 `feature_id`를 전파** (추적용)

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

**vfx** — 각 VFX (디자인 명세 포함):

VFX 명세는 단순 태그 추출이 아니라, **스킬과 아이템 맥락을 반영한 디자인 명세**까지 포함해야 한다.
vfx-gen은 이 명세를 받아 Niagara 빌드만 수행하므로, 여기서 충분한 디자인 정보를 제공해야 한다.

#### 4-A. VFX 맥락 분석
각 VFX 태그에 대해 스토리를 역추적하여 맥락을 파악:
1. **어떤 스킬이 이 VFX를 사용하는가** — `PlayVFX`/`PlayVFXAttached` op이 포함된 스토리의 `storyTag`
2. **어떤 아이템이 이 스킬을 트리거하는가** — `SetItemSkillTag`로 바인딩된 아이템 태그
3. **사용 맥락** — VFX가 재생되는 시점:
   - `on_hit`: 히트 시 (WaitCollision 이후 PlayVFX)
   - `on_cast`: 시전 시 (PlayAnim + PlayVFXAttached)
   - `projectile_impact`: 투사체 충돌 시
   - `projectile_trail`: 투사체 이동 중
   - `buff_aura`: 지속 버프 효과
   - `death`: 사망 시
   - `ambient`: 환경 이펙트

#### 4-B. VFX 비주얼 디자인 도출
맥락에서 디자인 명세를 도출한다:

**에미터 레이어 구성** — event_type과 element에 따라:
| VFX 유형 | 최소 레이어 | 권장 레이어 |
|---|---|---|
| 히트/임팩트 | Core + Sparks | Core + Sparks + Flash + Dust |
| 폭발 | Core + Burst + Sparks | Core + Burst + Sparks + Smoke + Debris + Light |
| 지속형 (불, 토네이도) | Core + Ambient | Core + Secondary + Ambient + Glow |
| 마법/버프 | Core + Particles | Core + Particles + Aura + Light |
| 투사체 궤적 | Core + Trail | Core + Trail + Sparks |

**색상 팔레트** — element에 따라 자연스럽게 도출:
| Element | Primary (HDR) | Secondary | Accent |
|---|---|---|---|
| Fire | {r:5, g:2, b:0.3} | {r:3, g:0.5, b:0} | {r:8, g:6, b:1} |
| Ice | {r:0.5, g:2, b:5} | {r:1, g:3, b:4} | {r:3, g:5, b:8} |
| Lightning | {r:3, g:3, b:8} | {r:5, g:4, b:1} | {r:8, g:8, b:10} |
| Physical | {r:2, g:2, b:2} | {r:1.5, g:1.5, b:1.5} | {r:3, g:3, b:3} |
| Dark | {r:2, g:0, b:3} | {r:1, g:0, b:1.5} | {r:4, g:0, b:5} |
| Holy | {r:5, g:5, b:3} | {r:4, g:4, b:2} | {r:8, g:8, b:5} |

**스케일/강도** — 스킬 맥락에 따라:
- 기본 공격 히트: `small` / `normal`
- 스킬 폭발: `medium~large` / `intense`
- 궁극기/보스 스킬: `large~massive` / `extreme`
- 버프 오라: `medium` / `subtle~normal`

**루핑/지속시간** — usage_context에 따라:
- `on_hit`: looping=false, 0.3~0.8초
- `on_cast`: looping=false, 0.5~1.5초
- `projectile_trail`: looping=true, 지속
- `buff_aura`: looping=true, 지속
- `death`: looping=false, 1~2초
- `ambient`: looping=true, 지속

각 에미터 레이어에 대해 `needs_custom_texture`와 `needs_custom_material` 여부도 결정:
- 연기/불꽃 계열 → custom texture + material 권장
- spark/debris 계열 → 기본 템플릿으로 충분
- light → 머티리얼 불필요

VFX 명세 필드:
- `tag`: `VFX.{Event}.{Element}` 형식
- `description`: 이펙트 설명
- `event_type`: Explosion, Hit, Buff 등
- `element`: Fire, Ice, Lightning 등
- `source_skill`: 이 VFX를 사용하는 스킬 태그 (nullable)
- `source_items`: 이 스킬을 트리거하는 아이템 태그 목록
- `usage_context`: on_hit, on_cast, projectile_impact, projectile_trail, buff_aura, death, ambient
- `visual_design`: 비주얼 디자인 명세 (아래 참조)
  - `emitter_layers`: [{name, role, renderer, needs_custom_texture, needs_custom_material}]
  - `color_palette`: {primary, secondary, accent} — element 기반 HDR 색상
  - `looping`: bool
  - `duration_hint`: 초 단위
  - `scale_hint`: small | medium | large | massive
  - `intensity`: subtle | normal | intense | extreme

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
      "description": "근접 베기 히트 이펙트 — 고블린 단검 공격 시 재생",
      "event_type": "Hit",
      "element": "Physical",
      "source_skill": "Ability.Attack.Basic",
      "source_items": ["Entity.Item.Weapon.GoblinDagger"],
      "usage_context": "on_hit",
      "visual_design": {
        "emitter_layers": [
          {"name": "SlashCore", "role": "core", "renderer": "sprite", "needs_custom_texture": true, "needs_custom_material": true},
          {"name": "Sparks", "role": "spark", "renderer": "sprite", "needs_custom_texture": false, "needs_custom_material": false},
          {"name": "ImpactFlash", "role": "core", "renderer": "light", "needs_custom_texture": false, "needs_custom_material": false}
        ],
        "color_palette": {
          "primary": {"r": 2, "g": 2, "b": 2},
          "secondary": {"r": 1.5, "g": 1.5, "b": 1.5},
          "accent": {"r": 3, "g": 3, "b": 3}
        },
        "looping": false,
        "duration_hint": 0.5,
        "scale_hint": "small",
        "intensity": "normal"
      }
    }
  ]
}
```

## 후속 스텝

asset_discovery 완료 후 다음 스텝이 병렬 실행 가능:
- `/char-gen $1` — 캐릭터 메시 + 애니메이션 생성
- `/item-gen $1` — 아이템 메시 + 아이콘 생성
- `/vfx-gen $1` — Niagara VFX 생성
