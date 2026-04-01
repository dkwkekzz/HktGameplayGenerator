---
name: feature-design
description: concept_design의 feature_outlines를 상세 설계하여, 각 feature별 스토리/예상 에셋/맵 요구사항을 구체화하는 스텝.
allowed-tools: Bash, Read, Write, Grep, Glob
argument-hint: <project_id>
---

# feature_design 스텝 실행

concept_design의 `feature_outlines[]`를 받아 각 feature의 **구현 명세**를 상세화한다.

## 인자

- `$1` — project_id

## 선행 조건

- `concept_design` 스텝이 completed 상태여야 한다

## 실행 절차

### 1. 스텝 시작
MCP 도구 `step_begin`을 호출한다:
- `project_id`: $1
- `step_type`: "feature_design"

### 2. Feature Outline 분석
concept_design 출력의 `feature_outlines[]`를 읽고 각 feature에 대해:
- 카테고리(combat, encounter, exploration, system)에 따른 구현 방식 결정
- 필요한 스토리 시나리오 도출
- 예상 에셋(캐릭터, 아이템, VFX, 애니메이션) 태그 도출
- 맵 요구사항(region, spawner) 도출

### 3. Feature별 상세 설계

각 feature에 대해 다음을 작성:

**stories** — 이 feature를 구현하는 Story 시나리오:
- `title`: 스토리 제목
- `description`: 상세 설명 (스토리 생성기에 전달할 구현 설명)
- `story_tag`: GameplayTag (예: `Story.Skill.FireBall`)
- `region`: 해당 스토리가 진행되는 region 이름

**expected_assets** — 예상 에셋 태그 (feature별 에셋 추적용):
- `characters`: Entity.Character.{Name} 태그 배열
- `items`: Entity.Item.{Cat}.{Sub} 태그 배열
- `vfx`: VFX.{Event}.{Element} 태그 배열
- `animations`: Anim.{Layer}.{Type}.{Name} 태그 배열

**map_requirements** — 추가 맵 요구사항:
- `regions`: 필요한 region 이름 배열
- `spawners`: 추가 spawner 정의 배열

### 4. Feature 등록
각 feature에 대해 MCP 도구 `step_add_feature`를 호출:
- `project_id`: $1
- `feature_id`: feature의 feature_id
- `name`: feature 이름
- `source`: "pipeline"

### 5. 출력 저장
MCP 도구 `step_save_output`을 호출한다:
- `project_id`: $1
- `step_type`: "feature_design"
- `output_json`: 아래 스키마를 따르는 JSON

## 출력 스키마

```json
{
  "features": [
    {
      "feature_id": "fire-magic",
      "name": "화염 마법 시스템",
      "category": "combat",
      "priority": "high",
      "stories": [
        {
          "title": "파이어볼 스킬",
          "description": "투사체형 화염 마법. 시전 시 화염구를 발사하고, 충돌 시 폭발. 범위 피해.",
          "story_tag": "Story.Skill.FireBall",
          "region": "VolcanicArea"
        }
      ],
      "expected_assets": {
        "characters": ["Entity.Character.FireMage"],
        "items": ["Entity.Item.Weapon.FireStaff"],
        "vfx": ["VFX.Hit.Fire", "VFX.Cast.Fire", "VFX.Projectile.FireBall"],
        "animations": ["Anim.Upper.Cast.FireBall"]
      },
      "map_requirements": {
        "regions": ["VolcanicArea"],
        "spawners": [
          {
            "entity_tag": "Entity.Character.FireMage",
            "region": "VolcanicArea",
            "spawn_rules": { "min_count": 1, "max_count": 2 }
          }
        ]
      }
    }
  ]
}
```

## 설계 지침

- 하나의 feature는 독립적으로 구현 가능해야 한다 (다른 feature 없이도 동작)
- 스토리 description은 story_generation이 HktCore 바이트코드를 작성할 수 있을 만큼 구체적으로
- expected_assets는 asset_discovery의 힌트 역할 — 정확할 필요 없음
- 태그 규칙 준수: Entity.Character.{Name}, Entity.Item.{Cat}.{Sub}, VFX.{Event}.{Element}
- priority가 high인 feature부터 먼저 배치
- 실패 시 `step_fail` 호출로 에러를 기록

## 후속 스텝

feature_design 완료 후:
- Orchestrator가 feature별 Worker Agent를 스폰하여 병렬 실행
- 각 Worker는 `/feature-worker {project_id} {feature_id}`를 실행
