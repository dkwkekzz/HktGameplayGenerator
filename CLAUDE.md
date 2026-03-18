# HktGameplayGenerator - Claude Code 가이드

## 프로젝트 개요

LLM 기반 UE5 게임플레이 어셋 자동 생성 시스템. MCP(Model Context Protocol)를 통해 언리얼 에디터와 통신하며, Map/Story/VFX/Mesh/Animation/Texture/Item을 생성한다.

## HktMcpBridge

각 Generator 모듈(Map/Story/VFX/Mesh/Anim/Item/Texture)의 Skill 도구를 제공하는 MCP 서버. 레벨 액터, PIE, 뷰포트 카메라 제어도 포함한다.

## 모듈식 스텝 시스템

생성 파이프라인은 **독립적인 7개 스텝**으로 구성된다. 각 스텝은 서로 다른 에이전트가 독립적으로 실행할 수 있으며, JSON 파일을 통해 입출력을 주고받는다.

### 스텝과 Skill 커맨드

| 스텝 | Skill | 설명 |
|---|---|---|
| `concept_design` | `/concept-design` | 맵/스토리 전체 설계 |
| `map_generation` | `/map-gen` | Landscape, Spawner, Region 정의 |
| `story_generation` | `/story-gen` | HktCore 주입용 스토리 |
| `asset_discovery` | `/asset-discovery` | 의존 어셋 분석 |
| `character_generation` | `/char-gen` | Mesh + Animation 생성 |
| `item_generation` | `/item-gen` | Item Mesh + Icon 생성 |
| `vfx_generation` | `/vfx-gen` | Niagara VFX 생성 |

전체 파이프라인: `/full-pipeline <프로젝트이름> <컨셉>`

### 의존 관계

```
concept_design
  ├─→ map_generation      (병렬 가능)
  └─→ story_generation    (병렬 가능)
        └─→ asset_discovery
              ├─→ character_generation  (병렬 가능)
              ├─→ item_generation       (병렬 가능)
              └─→ vfx_generation        (병렬 가능)
```

### 규칙

- 각 스텝은 독립적 — 다른 에이전트가 이어서 실행 가능
- 상세 실행 절차는 `.claude/skills/`의 Skill 파일에 정의
- 스텝 간 데이터는 `.hkt_steps/{project_id}/{step_type}/output.json`으로 전달
- 스텝 실패 시 `step_fail`로 에러 기록

## HktMap

JSON 기반 맵 정의 (UMap 아님). 런타임에 동적 로드/언로드 가능. Landscape, Region, Spawner, Story 참조, Props로 구성.

## Tag 시스템과 명명 규칙

| Tag 패턴 | 경로 패턴 |
|---|---|
| `Entity.Character.{Name}` | `{Root}/Characters/{Name}/BP_{Name}` |
| `Entity.Item.{Cat}.{Sub}` | `{Root}/Items/{Cat}/SM_{Sub}` |
| `VFX.{Event}.{Element}` | `{Root}/VFX/VFX_{Event}_{Element}` |
| `Anim.{Layer}.{Type}.{Name}` | `{Root}/Animations/Anim_{Layer}_{Type}_{Name}` |

`{Root}` = Project Settings > HktGameplay > HktAsset > ConventionRootDirectory (기본: `/Game/Generated`)

## 코드 구조

- `McpServer/src/hkt_mcp/` — Python MCP 서버 (tools/, steps/, bridge/, server.py)
- `Source/` — C++ UE5 플러그인 모듈
  - `HktMcpBridgeEditor/` — 에디터 서브시스템 + Step Viewer
  - `HktMapGenerator/` — HktMap JSON 파싱, Landscape/Spawner 빌드
  - `HktStoryGenerator/` — Story 바이트코드 VM
  - `HktVFXGenerator/` — Niagara VFX 생성
  - `HktMeshGenerator/`, `HktAnimGenerator/`, `HktItemGenerator/`, `HktTextureGenerator/`
  - `HktGeneratorCore/` — Tag 해석, ConventionPath, GeneratorRouter

## 환경설정

| 환경변수 | 설명 | 기본값 |
|---|---|---|
| `HKT_STEPS_DIR` | 스텝 데이터 저장 경로 | `.hkt_steps/` |
| `HKT_MAPS_DIR` | HktMap JSON 저장 경로 | `.hkt_maps/` |

UE5 측 출력 경로는 Project Settings에서 설정 (HktAsset, HktMapGenerator, HktVFXGenerator 등 각 모듈별).
