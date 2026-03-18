---
name: vfx-gen
description: asset_discovery의 VFX 명세를 기반으로 Niagara VFX 시스템을 생성하는 VFX 생성 스텝.
allowed-tools: Bash, Read, Write, Grep, Glob
argument-hint: <project_id>
---

# vfx_generation 스텝 실행

asset_discovery 출력의 vfx 명세를 받아 **Niagara VFX 시스템**을 생성한다.

## 인자

- `$1` — project_id

## 선행 조건

- `asset_discovery` 스텝이 completed 상태여야 한다

## 실행 절차

### 1. 스텝 시작
MCP 도구 `step_begin`을 호출한다:
- `project_id`: $1
- `step_type`: "vfx_generation"

### 2. VFX 가이드 확인
순서대로 호출:
1. `get_vfx_config_schema` → VFX 설정 스키마 확인
2. `get_vfx_prompt_guide` → VFX 생성 가이드 확인
3. `get_vfx_examples` → 기존 VFX 예제 학습

### 3. 템플릿 파라미터 확인
`dump_template_parameters`로 사용 가능한 Niagara 템플릿과 파라미터를 확인한다.

### 4. VFX별 생성
각 VFX 명세에 대해:

1. event_type과 element에 적합한 VFX 설정 JSON을 작성
2. `build_vfx_system` — Niagara VFX 시스템 생성
   - 폭발 계열은 `build_preset_explosion` 프리셋 활용 가능

### 5. 어셋 경로 확인
Tag 규칙 준수 확인:
- VFX: `{Root}/VFX/VFX_{Event}_{Element}`
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
      "niagara_system_path": "/Game/Generated/VFX/VFX_Hit_Slash"
    },
    {
      "tag": "VFX.Explosion.Fire",
      "asset_path": "/Game/Generated/VFX/VFX_Explosion_Fire",
      "niagara_system_path": "/Game/Generated/VFX/VFX_Explosion_Fire"
    }
  ]
}
```

## 실패 처리

- 개별 VFX 생성 실패 시 해당 항목만 건너뛰고 계속 진행
- 모든 VFX가 실패하면 `step_fail` 호출
