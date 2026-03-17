# AI Agent Pipeline Guide — Modular Step System

> **대상**: AI Agent (Claude, GPT 등) — MCP 도구 사용법과 작업 절차
> **C++ 아키텍처 상세는 [GameStoryVisualPipeline.md](./GameStoryVisualPipeline.md) 참고**
>
> 코드를 모르는 AI Agent도 이 문서만 읽으면 작업할 수 있도록 작성되었습니다.
> 각 스텝은 독립적이므로, 다른 에이전트가 중간부터 이어서 작업할 수 있습니다.

---

## 1. 시스템 개요

### 핵심 원리

```
사용자 컨셉 → Map + Story 설계 → Story JSON → 의존 어셋 분석 → Generator가 리소스 생성
```

### 모듈식 스텝 구조

각 스텝은 **독립적**으로 실행 가능하며, JSON 파일로 입출력을 주고받습니다.

```
[Step 1] concept_design
         사용자 컨셉 → 지형 명세 + 스토리 리스트
              │
     ┌────────┴────────┐
     │                 │
[Step 2]           [Step 3]
map_generation     story_generation       ← 병렬 가능
HktMap JSON          Story JSON들
                       │
                  [Step 4]
                  asset_discovery
                  어셋 명세 (Character/Item/VFX)
                       │
            ┌──────────┼──────────┐
            │          │          │
        [Step 5]   [Step 6]   [Step 7]
        character  item       vfx           ← 병렬 가능
        _generation _generation _generation
        uasset들    uasset들    uasset들
```

### 스텝 MCP 도구 워크플로우

```python
# 1. 프로젝트 생성
project = step_create_project(name="고블린 던전", concept="...")

# 2. 스텝 시작 (입력 자동 해석)
step_begin(project_id, "concept_design")

# 3. 스텝 출력 저장 (완료 처리)
step_save_output(project_id, "concept_design", output_json)

# 4. 다음 스텝 — 다른 에이전트가 실행해도 됨
input_data = step_load_input(project_id, "story_generation")
step_begin(project_id, "story_generation")
# ... 작업 수행 ...
step_save_output(project_id, "story_generation", output_json)

# 5. 실패 시
step_fail(project_id, "vfx_generation", error="Niagara build failed")
```

### 스텝 데이터 파일 구조

```
.hkt_steps/{project_id}/
├── manifest.json              ← 전체 프로젝트 상태
├── concept_design/
│   ├── input.json
│   └── output.json            ← 지형 명세 + 스토리 리스트
├── map_generation/
│   └── output.json            ← HktMap JSON
├── story_generation/
│   └── output.json            ← Story JSON 파일 목록
├── asset_discovery/
│   └── output.json            ← 어셋 명세 (characters, items, vfx)
├── character_generation/
│   └── output.json            ← 생성된 uasset 경로들
├── item_generation/
│   └── output.json
└── vfx_generation/
    └── output.json
```

---

## 2. Tag 시스템 — 모든 것의 시작점

모든 리소스는 **GameplayTag**로 식별됩니다. Tag의 prefix가 어떤 Generator가 처리할지 결정합니다.

| Tag Prefix | Generator | 생성 결과물 | 예시 |
|-------------|-----------|-------------|------|
| `VFX.*` | VFX Generator | NiagaraSystem | `VFX.Explosion.Fire` |
| `Entity.*` | Mesh Generator | SkeletalMesh + Blueprint | `Entity.Character.Goblin` |
| `Anim.*` | Anim Generator | AnimSequence / Montage / BlendSpace | `Anim.FullBody.Action.Spawn` |
| `Entity.Item.*` | Item Generator | StaticMesh + Icon Texture | `Entity.Item.Weapon.Sword.Fire` |
| `Story.*` | Story Generator | Bytecode (VM 등록) | `Story.Combat.Fireball` |

### Tag 명명 규칙

```
VFX.{EventType}.{Element}              예: VFX.Explosion.Fire
VFX.{EventType}.{Element}.{Surface}    예: VFX.Hit.Ice.Stone
VFX.Custom.{Description}               예: VFX.Custom.MagicPortal

Entity.Character.{Name}                예: Entity.Character.Goblin
Entity.{Type}.{Name}                   예: Entity.Projectile.Fireball

Anim.{Layer}.{Type}.{Name}             예: Anim.FullBody.Locomotion.Run
                                           Anim.UpperBody.Combat.Attack
                                           Anim.Montage.Intro

Entity.Item.{Category}.{SubType}         예: Entity.Item.Weapon.Sword
Entity.Item.{Category}.{SubType}.{Elem}  예: Entity.Item.Weapon.Sword.Fire
```

### Convention Path — Tag에서 에셋 경로 자동 유도

Tag가 정해지면 에셋이 저장될 경로도 자동 결정됩니다. `/Game/Generated/` 하위에 규칙대로 배치됩니다.

| Tag | Convention Path |
|-----|----------------|
| `Entity.Character.Goblin` | `/Game/Generated/Characters/Goblin/BP_Goblin` |
| `VFX.Explosion.Fire` | `/Game/Generated/VFX/VFX_Explosion_Fire` |
| `Entity.Item.Weapon.Sword` | `/Game/Generated/Items/Weapon/SM_Sword` |
| `Anim.FullBody.Locomotion.Run` | `/Game/Generated/Animations/Anim_FullBody_Locomotion_Run` |

---

## 3. Step-by-Step 작업 절차 (스토리 생성 ~ 빌드)

> 아래는 `story_generation` 스텝 내에서 수행하는 세부 절차입니다.
> `step_begin(project_id, "story_generation")`으로 시작하고,
> 완료 후 `step_save_output()`으로 결과를 기록합니다.

### Step 1: Story JSON 작성

Story JSON의 기본 구조:

```json
{
  "storyTag": "Story.Combat.Fireball",
  "steps": [
    { "op": "SpawnEntity", "args": { "register": "Spawned", "tag": "Entity.Projectile.Fireball" } },
    { "op": "CopyPosition", "args": { "to": "Spawned", "from": "Self" } },
    { "op": "MoveForward",  "args": { "register": "Spawned", "distance": 500 } },
    { "op": "PlayVFX",      "args": { "register": "Spawned", "tag": "VFX.Launch.Fire" } },
    { "op": "ApplyDamage",  "args": { "attacker": "Self", "target": "Target", "amount": 25 } }
  ]
}
```

**시작 전에 반드시 호출할 API:**

```python
# 1. JSON 형식 학습
schema = McpGetStorySchema()

# 2. 실제 예제 학습
examples = McpGetStoryExamples()
```

**지원되는 Op 카테고리 (45+ ops):**

| 카테고리 | 대표 Op |
|----------|---------|
| control | `Label`, `Jump`, `JumpIf`, `Return` |
| wait | `WaitFrames`, `WaitUntilProperty` |
| entity | `SpawnEntity`, `Destroy`, `SetEntityType` (Entity.Item.* 포함) |
| position | `CopyPosition`, `MoveForward`, `SetPosition` |
| combat | `ApplyDamage`, `ApplyHeal`, `ApplyBuff` |
| vfx | `PlayVFX`, `StopVFX` |
| tags | `AddTag`, `RemoveTag`, `HasTag` |
| spatial | `ForEachInRadius`, `CountByTag` |
| data | `StoreInt`, `LoadInt`, `CopyProperty` |

### Step 2: 문법 검증

```python
result = McpValidateStory(json_story)
# 성공: {"valid": true}
# 실패: {"valid": false, "errors": ["..."]}
```

### Step 3: 의존성 분석

```python
result = McpAnalyzeDependencies(json_story)
```

반환 예시:
```json
{
  "VFX": ["VFX.Launch.Fire", "VFX.Explosion.Fire"],
  "Entity": ["Entity.Projectile.Fireball"],
  "Anim": ["Anim.UpperBody.Combat.Cast"],
  "Item": [],
  "Unknown": []
}
```

**이 결과에서 각 카테고리에 나열된 Tag가 아직 에셋이 없는(누락된) 것입니다.**
각 Generator를 호출하여 생성해야 합니다.

### Step 4: 리소스 생성 (Generator별 MCP API 호출)

아래 섹션 4~8을 참고하여 각 Generator API를 호출합니다.
**생성 순서 권장: Texture → VFX / Mesh → Anim → Item → Story Build**
(Texture는 다른 Generator의 하위 의존성이므로 먼저 생성)

### Step 5: Story 컴파일 및 등록

모든 의존 리소스가 생성된 후:

```python
result = McpBuildStory(json_story)
# {"success": true, "storyTag": "Story.Combat.Fireball", "opCount": 6}
```

### Step 6: 검증

```python
stories = McpListGeneratedStories()
```

---

## 4. VFX Generator — VFX.* Tag 처리

### 개요

VFX Tag → Niagara System 에셋 자동 생성.
AI Agent가 JSON Config를 만들어 API에 전달하면 UE5 Niagara System이 빌드됩니다.

### MCP API

| 함수 | 용도 | 파라미터 |
|------|------|----------|
| `McpGetVFXPromptGuide()` | 디자인 가이드 + 스키마 (최초 1회 읽기) | 없음 |
| `McpGetVFXExampleConfigs()` | 예제 Config JSON (패턴 학습용) | 없음 |
| `McpGetVFXConfigSchema()` | JSON 스키마만 | 없음 |
| `McpBuildNiagaraSystem(json, outputDir?)` | Config JSON → Niagara 에셋 빌드 | `json`: Config JSON |
| `McpBuildPresetExplosion(name, r, g, b, intensity, outputDir?)` | 빠른 테스트용 폭발 프리셋 | 색상+강도 |
| `McpListGeneratedVFX(directory?)` | 생성된 VFX 목록 | 선택적 경로 |

### 작업 절차

```
1. McpGetVFXPromptGuide()        ← 가이드 읽기 (세션당 1회)
2. McpGetVFXExampleConfigs()     ← 예제 학습
3. VFX Config JSON 작성          ← emitter 구성
4. McpBuildNiagaraSystem(json)   ← 빌드
5. McpListGeneratedVFX()         ← 확인
```

### VFX Config JSON 구조

```json
{
  "systemName": "Explosion_Fire",
  "warmupTime": 0,
  "looping": false,
  "emitters": [
    {
      "name": "CoreFlash",
      "spawn": { "mode": "burst", "burstCount": 3 },
      "init": {
        "lifetimeMin": 0.05, "lifetimeMax": 0.2,
        "sizeMin": 80, "sizeMax": 200,
        "color": { "r": 5, "g": 3, "b": 0.5 }
      },
      "update": { "sizeScaleEnd": 2.0, "opacityEnd": 0 },
      "render": { "emitterTemplate": "core", "sortOrder": 10 }
    }
  ]
}
```

**핵심 규칙:**
- 3~6개의 emitter를 레이어링하여 풍부한 효과 구성
- `emitterTemplate`으로 물리 특성이 결정되는 템플릿 선택 (guide 참고)
- 텍스처가 필요한 경우 HktTextureGenerator를 먼저 호출

### VFX Tag 자동 해결

Story에서 `PlayVFX("VFX.Explosion.Fire")`가 호출되면:
1. `FHktVFXAutoResolver`가 Tag → `EventType=Explosion, Element=Fire` 파싱
2. VFX Generator가 적절한 Config로 NiagaraSystem 자동 빌드
3. `/Game/Generated/VFX/VFX_Explosion_Fire`에 저장

---

## 5. Mesh Generator — Entity.* Tag 처리

### 개요

Entity Tag → SkeletalMesh + Blueprint 생성.
현재는 외부 3D 생성 도구(Meshy, Rodin 등)와 연동하는 2-phase 방식.

### MCP API

| 함수 | 용도 | 파라미터 |
|------|------|----------|
| `McpRequestCharacterMesh(jsonIntent)` | 메시 생성 요청 → Convention Path + 프롬프트 | Intent JSON |
| `McpImportMesh(filePath, jsonIntent)` | 외부 파일 UE5 임포트 | 파일 경로 + Intent |
| `McpGetPendingMeshRequests()` | 펜딩 요청 목록 | 없음 |
| `McpListGeneratedMeshes(directory?)` | 생성된 메시 목록 | 선택적 경로 |
| `McpGetSkeletonPool()` | 사용 가능한 Base Skeleton 조회 | 없음 |

### 작업 절차

```
1. McpGetSkeletonPool()                  ← 사용 가능한 Skeleton 확인
2. McpRequestCharacterMesh(intent)       ← 요청 → 프롬프트 + 경로 반환
3. 외부 도구로 3D 메시 생성               ← Meshy/Rodin 등
4. McpImportMesh(filePath, intent)       ← UE5 임포트
5. McpListGeneratedMeshes()              ← 확인
```

### Intent JSON 구조

```json
{
  "entityTag": "Entity.Character.Goblin",
  "name": "Goblin",
  "skeletonType": "Humanoid",
  "styleKeywords": ["fantasy", "green_skin", "small"]
}
```

### Skeleton Pool 전략

모든 캐릭터는 소수의 Base Skeleton을 공유합니다:

| Skeleton | 용도 |
|----------|------|
| `SK_Humanoid_Base` | 인간형 (60+ bones, UE5 Mannequin 호환) |
| `SK_Quadruped_Base` | 사족보행 (동물, 마운트) |
| `SK_Custom_{N}` | 특수 형태 (슬라임, 드래곤 등) |

**Skeleton을 공유하므로 한 Skeleton에 만든 AnimSequence는 같은 Skeleton의 모든 캐릭터에 즉시 사용 가능합니다.**

---

## 6. Animation Generator — Anim.* Tag 처리

### 개요

Anim Tag → AnimSequence / AnimMontage / BlendSpace 생성.
외부 모션 생성 도구(Mixamo, Motion Diffusion 등)와 연동.

### MCP API

| 함수 | 용도 | 파라미터 |
|------|------|----------|
| `McpRequestAnimation(jsonIntent)` | 생성 요청 → Convention Path + 프롬프트 + 예상 타입 | Intent JSON |
| `McpImportAnimation(filePath, jsonIntent)` | 외부 파일 UE5 임포트 | 파일 경로 + Intent |
| `McpGetPendingAnimRequests()` | 펜딩 요청 목록 | 없음 |
| `McpListGeneratedAnimations(directory?)` | 생성된 에셋 목록 | 선택적 경로 |

### 작업 절차

```
1. McpRequestAnimation(intent)           ← 요청 → 프롬프트 + 예상 에셋 타입
2. 외부 도구로 애니메이션 생성             ← Mixamo / Motion Diffusion
3. McpImportAnimation(filePath, intent)  ← UE5 임포트
4. McpListGeneratedAnimations()          ← 확인
```

### Intent JSON 구조

```json
{
  "animTag": "Anim.FullBody.Action.Spawn",
  "layer": "FullBody",
  "type": "Action",
  "name": "Spawn",
  "skeletonType": "Humanoid",
  "styleKeywords": ["dramatic", "slow"]
}
```

### Anim Tag 구조

| Tag 패턴 | Layer | 에셋 타입 |
|-----------|-------|-----------|
| `Anim.FullBody.Locomotion.*` | FullBody | BlendSpace |
| `Anim.FullBody.Action.*` | FullBody | Montage / Sequence |
| `Anim.UpperBody.Combat.*` | UpperBody | Montage |
| `Anim.Montage.*` | Montage | One-shot Montage |

### 동적 매핑

생성된 Animation은 `RegisterAnimMapping()`으로 AnimInstance에 동적 등록됩니다.
AnimBP 리컴파일은 불필요합니다.

---

## 7. Item Generator — Entity.Item.* Tag 처리

### 개요

Entity.Item Tag → StaticMesh + Icon Texture + MaterialInstance 생성.
메시는 외부 도구, 아이콘은 HktTextureGenerator를 활용.

### MCP API

| 함수 | 용도 | 파라미터 |
|------|------|----------|
| `McpRequestItem(jsonIntent)` | 아이템 생성 요청 → Convention Path + 프롬프트 | Intent JSON |
| `McpImportItemMesh(filePath, jsonIntent)` | 외부 메시 UE5 임포트 | 파일 경로 + Intent |
| `McpGetPendingItemRequests()` | 펜딩 요청 목록 | 없음 |
| `McpListGeneratedItems(directory?)` | 생성된 아이템 목록 | 선택적 경로 |
| `McpGetSocketMappings()` | 장착 소켓 정보 조회 | 없음 |

### 작업 절차

```
1. McpRequestItem(intent)                ← 요청 → 프롬프트 + 경로
2. 외부 도구로 3D 메시 생성               ← StaticMesh
3. McpImportItemMesh(filePath, intent)   ← UE5 임포트
4. McpGenerateTexture({usage:"ItemIcon"})← 아이콘 텍스처 생성 (Texture Generator)
5. McpListGeneratedItems()               ← 확인
```

### Intent JSON 구조

```json
{
  "itemTag": "Entity.Item.Weapon.Sword.Fire",
  "category": "Weapon",
  "subType": "Sword",
  "element": "Fire",
  "rarity": 0.5,
  "styleKeywords": ["fantasy", "flame_enchanted"]
}
```

### 장착 시각화

| Category | 방식 |
|----------|------|
| Weapon | Socket Attach (`hand_r`) |
| Armor | Material Overlay / Modular Mesh |
| Accessory | Socket Attach (소형) / VFX (버프) |

---

## 8. Texture Generator — 공유 모듈

### 개요

모든 Generator가 텍스처가 필요할 때 이 모듈을 통해 생성합니다.
AI Agent가 외부 이미지 생성 도구(SD/DALL-E/ComfyUI 등)로 이미지를 만들고, UE5에 임포트합니다.

### MCP API

| 함수 | 용도 | 파라미터 |
|------|------|----------|
| `McpGenerateTexture(jsonIntent, outputDir?)` | 텍스처 요청 (캐시 히트 시 즉시 반환) | Intent JSON |
| `McpImportTexture(imagePath, jsonIntent, outputDir?)` | 외부 이미지 → UTexture2D 임포트 | 파일 경로 + Intent |
| `McpGetPendingRequests(jsonRequests)` | 배치 요청 중 미처리 목록 | 요청 배열 JSON |
| `McpListGeneratedTextures(directory?)` | 생성된 텍스처 목록 | 선택적 경로 |

### 작업 절차

```
1. McpGenerateTexture(intent)
   → 캐시 히트: 즉시 에셋 경로 반환
   → 캐시 미스: pending=true + 완성된 프롬프트 + imagePath 반환

2. Agent가 외부 도구로 이미지 생성 (SD/DALL-E/ComfyUI 등)
   → imagePath에 .png 저장

3. McpImportTexture(imagePath, intent)
   → UTexture2D 임포트 + Usage별 설정 자동 적용 + 캐시 등록

4. McpListGeneratedTextures()
   → 확인
```

### Intent JSON 구조

```json
{
  "usage": "ParticleSprite",
  "prompt": "fire particle, orange glow, black background",
  "negativePrompt": "text, watermark",
  "resolution": 256,
  "alphaChannel": true,
  "tileable": false,
  "styleKeywords": ["painterly"]
}
```

### Usage 종류

| Usage | 용도 | 기본 해상도 |
|-------|------|-------------|
| `ParticleSprite` | VFX 파티클 스프라이트 | 256 |
| `Flipbook4x4` | 4x4 SubUV 시퀀스 | 256 |
| `Flipbook8x8` | 8x8 SubUV 시퀀스 | 1024 |
| `Noise` | 노이즈 텍스처 (tileable) | 256 |
| `Gradient` | 그라디언트 램프 | 256 |
| `ItemIcon` | 아이템 아이콘 (UI) | 256 |
| `MaterialBase` | Albedo / BaseColor | 1024 |
| `MaterialNormal` | Normal Map | 1024 |
| `MaterialMask` | R/M/AO Packed Mask | 1024 |

---

## 9. 종합 시나리오 예제

### 시나리오 A: "화염구 스킬 전체 생성"

```
[Step 1] Story JSON 작성
  {
    "storyTag": "Story.Combat.Fireball",
    "steps": [
      { "op": "SpawnEntity",  "args": { "register": "Proj", "tag": "Entity.Projectile.Fireball" } },
      { "op": "SetEntityType","args": { "register": "Proj", "type": "Projectile" } },
      { "op": "CopyPosition", "args": { "to": "Proj", "from": "Self" } },
      { "op": "MoveForward",  "args": { "register": "Proj", "distance": 500 } },
      { "op": "PlayVFX",      "args": { "register": "Proj", "tag": "VFX.Launch.Fire" } },
      { "op": "PlayVFX",      "args": { "register": "Self", "tag": "VFX.Explosion.Fire" } },
      { "op": "AddTag",       "args": { "register": "Self", "tag": "Anim.UpperBody.Combat.Cast" } },
      { "op": "ApplyDamage",  "args": { "attacker": "Self", "target": "Target", "amount": 25 } }
    ]
  }

[Step 2] McpValidateStory(json) → {"valid": true}

[Step 3] McpAnalyzeDependencies(json)
  → VFX:    ["VFX.Launch.Fire", "VFX.Explosion.Fire"]
  → Entity: ["Entity.Projectile.Fireball"]
  → Anim:   ["Anim.UpperBody.Combat.Cast"]

[Step 4] 리소스 생성

  4a. VFX 생성:
      McpGetVFXPromptGuide()
      McpBuildNiagaraSystem(launch_fire_config)
      McpBuildNiagaraSystem(explosion_fire_config)

  4b. Mesh 생성:
      McpRequestCharacterMesh({"entityTag":"Entity.Projectile.Fireball", ...})
      → 외부 도구로 메시 생성
      McpImportMesh(filePath, intent)

  4c. Animation 생성:
      McpRequestAnimation({"animTag":"Anim.UpperBody.Combat.Cast", ...})
      → 외부 도구로 애니메이션 생성
      McpImportAnimation(filePath, intent)

[Step 5] McpBuildStory(json) → 컴파일 + VM 등록

[Step 6] 검증
  McpListGeneratedVFX()
  McpListGeneratedMeshes()
  McpListGeneratedAnimations()
  McpListGeneratedStories()
```

### 시나리오 B: "고블린 캐릭터 추가"

```
[Step 1] 텍스처 생성
  McpGenerateTexture({"usage":"MaterialBase", "prompt":"goblin skin, green, scaly"})
  → Agent가 이미지 생성
  McpImportTexture(imagePath, intent)

[Step 2] 메시 생성
  McpGetSkeletonPool()  → SK_Humanoid_Base 확인
  McpRequestCharacterMesh({
    "entityTag": "Entity.Character.Goblin",
    "name": "Goblin",
    "skeletonType": "Humanoid"
  })
  → 외부 3D 도구로 메시 생성 (SK_Humanoid_Base Skeleton 사용)
  McpImportMesh(filePath, intent)

[Step 3] Animation (선택)
  McpRequestAnimation({"animTag":"Anim.FullBody.Locomotion.Run", ...})
  → 외부 도구로 생성
  McpImportAnimation(filePath, intent)

[Step 4] 런타임
  Story에서 .SpawnEntity("Entity.Character.Goblin") 호출
  → Convention Path: /Game/Generated/Characters/Goblin/BP_Goblin
  → 자동 스폰 + 동적 애니메이션 매핑
```

---

## 10. MCP API 레퍼런스 (전체 목록)

### Story Generator

| 함수 | 파라미터 | 반환 |
|------|----------|------|
| `McpGetStorySchema()` | 없음 | Story JSON 스키마 |
| `McpGetStoryExamples()` | 없음 | 4개 완성 예제 |
| `McpValidateStory(json)` | Story JSON | 검증 결과 |
| `McpAnalyzeDependencies(json)` | Story JSON | Generator별 누락 Tag 분류 |
| `McpBuildStory(json)` | Story JSON | 컴파일 결과 (storyTag, opCount) |
| `McpListGeneratedStories()` | 없음 | 등록된 Story 목록 |

### VFX Generator

| 함수 | 파라미터 | 반환 |
|------|----------|------|
| `McpGetVFXPromptGuide()` | 없음 | 디자인 가이드 + 스키마 + 팁 |
| `McpGetVFXExampleConfigs()` | 없음 | 예제 Config JSON 목록 |
| `McpGetVFXConfigSchema()` | 없음 | JSON 스키마 |
| `McpBuildNiagaraSystem(json, outputDir?)` | Config JSON | 빌드 결과 + assetPath |
| `McpBuildPresetExplosion(name, r, g, b, intensity, outputDir?)` | 색상, 강도 | 빌드 결과 |
| `McpListGeneratedVFX(directory?)` | 선택적 경로 | 생성된 에셋 목록 |

### Mesh Generator

| 함수 | 파라미터 | 반환 |
|------|----------|------|
| `McpRequestCharacterMesh(jsonIntent)` | Character Intent JSON | Convention Path + 프롬프트 |
| `McpImportMesh(filePath, jsonIntent)` | 파일 경로 + Intent | 임포트 결과 |
| `McpGetPendingMeshRequests()` | 없음 | 펜딩 목록 |
| `McpListGeneratedMeshes(directory?)` | 선택적 경로 | 생성된 메시 목록 |
| `McpGetSkeletonPool()` | 없음 | Base Skeleton 목록 |

### Animation Generator

| 함수 | 파라미터 | 반환 |
|------|----------|------|
| `McpRequestAnimation(jsonIntent)` | Anim Intent JSON | Convention Path + 프롬프트 + 예상 타입 |
| `McpImportAnimation(filePath, jsonIntent)` | 파일 경로 + Intent | 임포트 결과 |
| `McpGetPendingAnimRequests()` | 없음 | 펜딩 목록 |
| `McpListGeneratedAnimations(directory?)` | 선택적 경로 | 생성된 에셋 목록 |

### Item Generator

| 함수 | 파라미터 | 반환 |
|------|----------|------|
| `McpRequestItem(jsonIntent)` | Item Intent JSON | Convention Path + 프롬프트 |
| `McpImportItemMesh(filePath, jsonIntent)` | 파일 경로 + Intent | 임포트 결과 |
| `McpGetPendingItemRequests()` | 없음 | 펜딩 목록 |
| `McpListGeneratedItems(directory?)` | 선택적 경로 | 생성된 아이템 목록 |
| `McpGetSocketMappings()` | 없음 | 장착 소켓 정보 |

### Texture Generator

| 함수 | 파라미터 | 반환 |
|------|----------|------|
| `McpGenerateTexture(jsonIntent, outputDir?)` | Texture Intent JSON | 에셋 경로 또는 pending 정보 |
| `McpImportTexture(imagePath, jsonIntent, outputDir?)` | 파일 경로 + Intent | 임포트 결과 |
| `McpGetPendingRequests(jsonRequests)` | 요청 배열 JSON | 미처리 목록 |
| `McpListGeneratedTextures(directory?)` | 선택적 경로 | 생성된 텍스처 목록 |

---

## 11. 의존 관계 & 생성 순서

```
Texture Generator (독립, 최하위)
     ↑
Generator Core (VFX AutoResolver, Intent Types, Router)
     ↑
├── VFX Generator      (Texture Generator 의존)
├── Mesh Generator     (Texture Generator 의존)
├── Anim Generator     (독립)
├── Item Generator     (Texture Generator 의존 — 아이콘)
└── Story Generator    (모든 Generator의 결과를 참조)
```

**권장 생성 순서:**
1. Texture (다른 Generator가 텍스처를 필요로 함)
2. VFX / Mesh / Anim (서로 독립 — 병렬 가능)
3. Item (메시 + 아이콘 텍스처 필요)
4. Story Build (모든 리소스 준비 후 컴파일)

---

## 12. 에러 처리 & 주의사항

### 캐시 히트 확인
- `McpGenerateTexture()` 호출 시 캐시 히트면 `pending=false`로 즉시 경로 반환됩니다.
- 이미 존재하는 리소스를 다시 생성할 필요 없습니다.

### Convention Path 충돌
- 같은 Tag에 대해 중복 생성하면 기존 에셋을 덮어씁니다.
- `McpList*()` 계열 함수로 기존 에셋 유무를 먼저 확인하세요.

### Skeleton 호환성
- 캐릭터 메시를 생성할 때 반드시 `McpGetSkeletonPool()`에서 반환된 Base Skeleton을 사용하세요.
- 같은 Skeleton을 사용하는 캐릭터들은 AnimSequence를 공유할 수 있습니다.

### Story 의존성 미해결
- `McpBuildStory()`는 누락 Tag가 있어도 컴파일은 성공합니다.
- 런타임에 Convention Path에 에셋이 없으면 해당 기능이 동작하지 않습니다.
- 반드시 `McpAnalyzeDependencies()`로 확인 후 모든 리소스를 생성하세요.

### MCP 연결
- MCP Server는 Python으로 UE5 에디터 외부에서 실행됩니다.
- UE5 에디터가 실행 중이어야 API가 동작합니다.
- 연결 정보: `hkt_mcp.server` 모듈, stdio JSON-RPC 방식.

---

## 13. 파일 구조 요약

```
HktGameplayGenerator/
├── Source/
│   ├── HktTextureGenerator/     ← 공유 텍스처 생성 (가장 하위 모듈)
│   ├── HktGeneratorCore/        ← Router + Intent 타입 + VFX AutoResolver
│   ├── HktVFXGenerator/         ← VFX.* Tag → NiagaraSystem
│   ├── HktMeshGenerator/        ← Entity.* Tag → SkeletalMesh + Blueprint
│   ├── HktAnimGenerator/        ← Anim.* Tag → AnimSequence/Montage/BlendSpace
│   ├── HktItemGenerator/        ← Entity.Item.* Tag → StaticMesh + Icon
│   ├── HktStoryGenerator/       ← Story JSON → Bytecode 컴파일
│   ├── HktMapGenerator/         ← HktMap JSON → Landscape + Spawner + Story 연결
│   ├── HktMcpBridge/            ← MCP 런타임 브릿지
│   └── HktMcpBridgeEditor/      ← MCP 에디터 브릿지 + Step Viewer 패널
├── McpServer/                   ← Python MCP Server (Claude Desktop/Cursor 연동)
│   └── src/hkt_mcp/
│       ├── steps/               ← 모듈식 스텝 시스템 (models, store)
│       ├── tools/               ← MCP 도구 (step_tools, map_tools, story_tools 등)
│       ├── bridge/              ← UE5 통신 (Remote Control API)
│       └── server.py            ← 도구 등록 & 디스패치
└── Docs/
    ├── GameStoryVisualPipeline.md   ← 아키텍처 상세 문서
    ├── VFXNiagaraConfigGuide.md     ← VFX Config 필드 레퍼런스
    ├── VFXConfigPrompt.md           ← VFX LLM 프롬프트 가이드
    └── AIAgentPipelineGuide.md      ← 이 문서
```
