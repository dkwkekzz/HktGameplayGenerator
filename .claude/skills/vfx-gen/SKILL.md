---
name: vfx-gen
description: asset_discovery의 VFX 명세를 기반으로 Niagara VFX 시스템을 생성하는 VFX 생성 스텝. 4-Phase 파이프라인으로 고품질 VFX를 생성한다.
allowed-tools: Bash, Read, Write, Grep, Glob
argument-hint: <project_id>
---

# vfx_generation 스텝 실행 (4-Phase Pipeline)

asset_discovery 출력의 vfx 명세를 받아 **고품질 Niagara VFX 시스템**을 생성한다.
텍스처/머티리얼 생성 → Niagara 빌드 → 프리뷰 검증의 단계를 거쳐 품질을 보장한다.

## 인자

- `$1` — project_id

## 선행 조건

- `asset_discovery` 스텝이 completed 상태여야 한다

## 실행 절차

### 0. 스텝 시작
MCP 도구 `step_begin`을 호출한다:
- `project_id`: $1
- `step_type`: "vfx_generation"

---

### Phase 1: VFX 설계 (Design)

**목표**: VFX 컨셉 분석 → 에미터 레이어 구성 결정 → 필요 리소스 파악

1. `get_vfx_config_schema` → VFX 설정 스키마 확인
2. `get_vfx_prompt_guide` → VFX 생성 가이드 확인
3. `get_vfx_examples` → 기존 VFX 예제 학습
4. `dump_vfx_template_parameters` → 사용 가능한 Niagara 템플릿과 파라미터 확인

각 VFX 명세에 대해 **설계 문서**를 작성:
- 에미터 레이어 목록 (core, secondary, ambient, light 등)
- 각 레이어의 역할 (폭발 본체, 스파크, 연기, 잔해 등)
- **텍스처 필요 여부**: 커스텀 텍스처가 필요한 이미터 식별
  - 연기/불꽃: 전용 sprite 텍스처 권장
  - 스파크/디브리: 템플릿 기본 텍스처로 충분
  - flipbook 효과: SD WebUI로 시퀀스 생성 필요
- **머티리얼 필요 여부**: 커스텀 머티리얼이 필요한 이미터 식별
  - Emissive 강도 조절이 필요한 경우
  - Translucent vs Additive 구분

---

### Phase 2: 텍스처/머티리얼 준비 (Material Prep)

**목표**: Phase 1에서 식별한 커스텀 리소스를 사전 생성

#### 2-A. 텍스처 생성 (필요한 경우)
커스텀 텍스처가 필요한 이미터에 대해:

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

#### 2-B. 머티리얼 생성
생성된 텍스처 또는 기존 텍스처를 이용하여 머티리얼 생성:

1. `create_particle_material` — 파티클용 MaterialInstance 생성
   - `material_name`: MI 이름 (예: "Fire_Core", "Smoke_Soft")
   - `texture_path`: 2-A에서 생성한 텍스처 경로 (없으면 빈 문자열)
   - `blend_mode`: "additive" (발광 계열) 또는 "translucent" (연기/먼지)
   - `emissive_intensity`: HDR 발광 강도 (기본 1.0, 불꽃은 3~5 권장)

**참고**: 단순 색상 변경만 필요한 이미터는 머티리얼 생성 없이 Config의 `color` + 템플릿 기본 머티리얼로 충분하다.

---

### Phase 3: Niagara 빌드 (Build)

**목표**: Phase 1 설계 + Phase 2 머티리얼로 최종 Niagara 시스템 생성

각 VFX 명세에 대해:

1. **Config JSON 작성** — Phase 1 설계를 기반으로 `FHktVFXNiagaraConfig` JSON 생성
   - 에미터별 `render.materialPath`에 Phase 2에서 생성한 MI 경로 설정
   - 에미터별 `render.emitterTemplate`으로 적절한 베이스 템플릿 선택
   - 텍스처가 필요한 에미터에 `texturePrompt`/`textureType` 설정
   - 폭발 계열은 `build_preset_explosion` 프리셋 활용 가능

2. `build_vfx_system` — Niagara VFX 시스템 빌드
   - `json_config`: 위에서 작성한 Config JSON
   - `output_dir`: 기본값 사용

3. **(선택) Phase 2에서 생성한 머티리얼 후적용**
   - `assign_vfx_material` — 빌드 후 특정 에미터에 머티리얼 교체
   - 빌드 시 `materialPath`를 사용하면 이 단계 불필요

---

### Phase 4: 프리뷰 & 튜닝 (Preview & Refine)

**목표**: 시각적 검증 후 파라미터 미세 조정

1. `preview_vfx` — VFX를 뷰포트에 스폰하고 스크린샷 캡처
   - `niagara_system_path`: Phase 3에서 생성된 에셋 경로
   - `duration`: 2~3초 (루핑 VFX는 더 길게)
   - 스크린샷으로 시각적 품질 확인

2. **스크린샷 분석** — 다음 항목 검증:
   - 파티클 밀도: 너무 적으면 `spawn.burstCount` 또는 `spawn.rate` 증가
   - 색상 밸런스: Core가 너무 밝거나 어두우면 `init.color` 또는 `emissiveIntensity` 조정
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

### 5. 어셋 경로 확인
Tag 규칙 준수 확인:
- VFX: `{Root}/VFX/VFX_{Event}_{Element}`
- 머티리얼: `{Root}/VFX/Materials/MI_{Name}`
- 예: `VFX.Explosion.Fire` → `/Game/Generated/VFX/VFX_Explosion_Fire`

### 6. 출력 저장
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

## VFX 품질 가이드라인

### 에미터 레이어 구성 원칙
| VFX 유형 | 최소 레이어 | 권장 레이어 |
|---|---|---|
| 히트/임팩트 | Core + Sparks | Core + Sparks + Flash + Dust |
| 폭발 | Core + Burst + Sparks | Core + Burst + Sparks + Smoke + Debris + Light |
| 지속형 (불, 토네이도) | Core + Ambient | Core + Secondary + Ambient + Glow |
| 마법/버프 | Core + Particles | Core + Particles + Aura + Light |

### 머티리얼 사용 기준
- **템플릿 기본 머티리얼 사용** (커스텀 불필요):
  - spark, debris 계열 (단순 색상 파티클)
  - light 렌더러 (머티리얼 없음)
- **커스텀 머티리얼 생성 권장**:
  - 연기/불꽃: 전용 텍스처 + translucent/additive MI
  - 코어 글로우: 높은 emissive_intensity (3~10)
  - 특수 효과: flipbook 텍스처 + SubUV 설정

## 실패 처리

- Phase 2 텍스처 생성 실패 시 → 템플릿 기본 머티리얼로 폴백
- Phase 3 개별 VFX 빌드 실패 시 → 해당 항목만 건너뛰고 계속 진행
- Phase 4 프리뷰 실패 시 → 스킵하고 다음 VFX로 진행
- 모든 VFX가 실패하면 `step_fail` 호출
