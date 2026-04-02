---
name: vfx-gen
description: asset_discovery의 VFX 디자인 명세를 기반으로 Niagara VFX 시스템을 빌드하는 VFX 생성 스텝. 디자인은 asset_discovery가 담당하며, 이 스텝은 머티리얼 준비 → Niagara 빌드 → 프리뷰 검증에 집중한다.
allowed-tools: Bash, Read, Write, Grep, Glob
argument-hint: <project_id>
---

# vfx_generation 스텝 실행

asset_discovery 출력의 **VFX 디자인 명세**를 받아 Niagara VFX 시스템을 빌드한다.
VFX의 설계(에미터 구성, 색상, 스케일 등)는 이미 asset_discovery에서 스킬/아이템/속성 맥락을 반영하여 결정되었으므로,
이 스텝은 **빌드와 품질 검증에만 집중**한다.

## 인자

- `$1` — project_id

## 선행 조건

- `asset_discovery` 스텝이 completed 상태여야 한다
- 커스텀 텍스처가 필요한 VFX가 있으면 SD WebUI 서버가 필요하다 → 실행 절차 Phase 1 참조

## 입력 데이터

각 VFX 명세에는 다음 디자인 정보가 포함되어 있다:
- `tag`, `description`, `event_type`, `element` — 기본 식별 정보
- `source_skill` — 이 VFX를 사용하는 스킬 태그
- `source_items` — 이 스킬을 트리거하는 아이템 태그 목록
- `usage_context` — VFX 재생 시점 (on_hit, on_cast, projectile_impact 등)
- `visual_design` — 에미터 레이어, 색상 팔레트, 루핑, 지속시간, 스케일, 강도

## 실행 절차

### 0. 스텝 시작
MCP 도구 `step_begin`을 호출한다:
- `project_id`: $1
- `step_type`: "vfx_generation"

### 1. 리소스 준비

1. `get_vfx_config_schema` → VFX 설정 스키마 확인
2. `get_vfx_prompt_guide` → 파라미터 값 범위 참조
3. `get_vfx_examples` → 참조용 예제 Config 확인

---

### Phase 1: 텍스처/머티리얼 준비 (Material Prep)

**목표**: `visual_design.emitter_layers`에서 `needs_custom_texture`/`needs_custom_material`이 true인 레이어의 리소스를 생성

**SD WebUI 서버 사전 점검**: 커스텀 텍스처가 필요한 레이어가 하나라도 있으면 텍스처 생성 **전에** 반드시 수행:
1. `check_sd_server_status` 호출
2. `alive: false`이면 `launch_sd_server` 호출하여 자동 시작
3. 시작 실패 또는 batch file 미설정 시 → 사용자에게 안내하고 해당 텍스처는 **템플릿 기본 머티리얼로 폴백**

#### 1-A. 텍스처 생성 (needs_custom_texture=true인 레이어)

1. `generate_texture` — SD WebUI를 통한 텍스처 생성
   - `json_intent`: 텍스처 의도 JSON (usage, prompt, resolution 등)
   - usage 예시:
     - `"ParticleSprite"` — 단일 파티클 스프라이트
     - `"Flipbook4x4"` — 4x4 SubUV 시퀀스 (16프레임)
     - `"Noise"` — 타일링 노이즈 텍스처
     - `"Gradient"` — 그래디언트 램프

프롬프트 작성 팁:
- 파티클 스프라이트: "seamless particle sprite, [element], alpha channel, black background, centered"
- 연기: "smoke puff sprite sheet, volumetric, soft edges, alpha, 8x8 grid"
- 불꽃: "fire flame sprite, bright orange, emissive, alpha channel"

#### 1-B. 머티리얼 생성 (needs_custom_material=true인 레이어)

1. `create_particle_material` — 파티클용 MaterialInstance 생성
   - `material_name`: MI 이름 (예: "Fire_Core", "Smoke_Soft")
   - `texture_path`: 1-A에서 생성한 텍스처 경로 (없으면 빈 문자열)
   - `blend_mode`: "additive" (발광 계열) 또는 "translucent" (연기/먼지)
   - `emissive_intensity`: HDR 발광 강도 (기본 1.0, 불꽃은 3~5 권장)

**참고**: `needs_custom_material=false`인 레이어는 Config의 `color` + 템플릿 기본 머티리얼로 충분하다.

---

### Phase 2: Niagara 빌드 (Build)

**목표**: 디자인 명세 + 머티리얼로 최종 Niagara 시스템 생성

각 VFX 명세에 대해:

1. **Config JSON 작성** — `visual_design`를 `FHktVFXNiagaraConfig` JSON으로 변환
   - `systemName`: VFX 태그에서 도출
   - `looping`: `visual_design.looping` 값 사용
   - `emitters`: `visual_design.emitter_layers`를 기반으로 구성
     - 각 레이어의 `role`에 맞는 `render.emitterTemplate` 선택
     - `color_palette`에서 primary/secondary 색상 적용
     - `scale_hint`와 `intensity`에 따라 spawn count, size, emissive 조정
     - `duration_hint`에 따라 lifetime 설정
     - Phase 1에서 생성한 머티리얼은 `render.materialPath`에 설정

   **scale_hint → 파라미터 매핑 가이드:**
   | scale_hint | sizeMin~Max | spawn count 배수 |
   |---|---|---|
   | small | 5~30 | 1x |
   | medium | 20~80 | 1.5x |
   | large | 50~200 | 2x |
   | massive | 100~500 | 3x |

   **intensity → 파라미터 매핑 가이드:**
   | intensity | emissive_intensity | particle density |
   |---|---|---|
   | subtle | 0.5~1.0 | 낮음 |
   | normal | 1.0~3.0 | 보통 |
   | intense | 3.0~6.0 | 높음 |
   | extreme | 5.0~10.0 | 매우 높음 |

2. `build_vfx_system` — Niagara VFX 시스템 빌드
   - `json_config`: 위에서 작성한 Config JSON
   - `output_dir`: 기본값 사용

3. **(선택) Phase 1에서 생성한 머티리얼 후적용**
   - `assign_vfx_material` — 빌드 후 특정 에미터에 머티리얼 교체
   - 빌드 시 `materialPath`를 사용하면 이 단계 불필요

---

### Phase 3: 프리뷰 & 튜닝 (Preview & Refine)

**목표**: 시각적 검증 후 파라미터 미세 조정

1. `preview_vfx` — VFX를 뷰포트에 스폰하고 스크린샷 캡처
   - `niagara_system_path`: Phase 2에서 생성된 에셋 경로
   - `duration`: `visual_design.duration_hint` 기반 (루핑은 더 길게)
   - 스크린샷으로 시각적 품질 확인

2. **스크린샷 분석** — 다음 항목 검증:
   - 파티클 밀도: 너무 적으면 `spawn.burstCount` 또는 `spawn.rate` 증가
   - 색상 밸런스: `color_palette`와 비교하여 Core가 너무 밝거나 어두우면 조정
   - 크기 분포: 파티클 크기가 단조로우면 `init.sizeMin/Max` 범위 확대
   - 움직임: 너무 정적이면 `update.noiseStrength` 또는 `update.vortexStrength` 추가
   - 페이드: 갑자기 사라지면 `update.opacityEnd` 커브 조정

3. `update_vfx_emitter` — 문제 발견 시 파라미터 부분 업데이트
   - `emitter_name`: 조정할 에미터 이름
   - `json_overrides`: 변경할 섹션만 포함한 JSON
   ```json
   {
     "spawn": {"burstCount": 50},
     "init": {"sizeMin": 5.0, "sizeMax": 20.0},
     "update": {"opacityEnd": 0.0, "drag": 2.0}
   }
   ```

4. 필요시 2~3번 반복 (최대 3회)

---

### 4. 어셋 경로 확인
Tag 규칙 준수 확인:
- VFX: `{Root}/VFX/VFX_{Event}_{Element}`
- 머티리얼: `{Root}/VFX/Materials/MI_{Name}`
- 예: `VFX.Explosion.Fire` → `/Game/Generated/VFX/VFX_Explosion_Fire`

### 5. 출력 저장
MCP 도구 `step_save_output`을 호출:
- `step_type`: "vfx_generation"
- `output_json`:

```json
{
  "generated_assets": [
    {
      "tag": "VFX.Hit.Slash",
      "asset_path": "/Game/Generated/VFX/VFX_Hit_Slash",
      "niagara_system_path": "/Game/Generated/VFX/VFX_Hit_Slash",
      "materials": [
        "/Game/Generated/VFX/Materials/MI_Slash_Core"
      ],
      "textures": [
        "/Game/Generated/Textures/T_Slash_Sprite"
      ]
    },
    {
      "tag": "VFX.Explosion.Fire",
      "asset_path": "/Game/Generated/VFX/VFX_Explosion_Fire",
      "niagara_system_path": "/Game/Generated/VFX/VFX_Explosion_Fire",
      "materials": [],
      "textures": []
    }
  ]
}
```

## 실패 처리

- Phase 1 텍스처 생성 실패 시 → 템플릿 기본 머티리얼로 폴백
- Phase 2 개별 VFX 빌드 실패 시 → 해당 항목만 건너뛰고 계속 진행
- Phase 3 프리뷰 실패 시 → 스킵하고 다음 VFX로 진행
- 모든 VFX가 실패하면 `step_fail` 호출
