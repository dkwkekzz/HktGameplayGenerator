# HktGameplayGenerator - Claude Code 가이드

## 프로젝트 개요

LLM 기반 UE5 게임플레이 어셋 자동 생성 시스템. MCP(Model Context Protocol)를 통해 언리얼 에디터와 통신하며, Map/Story/VFX/Mesh/Animation/Texture/Item을 생성한다.

## HktMcpBridge

각 Generator 모듈(Map/Story/VFX/Mesh/Anim/Item/Texture)의 Skill 도구를 제공하는 MCP 서버. 레벨 액터, PIE, 뷰포트 카메라 제어도 포함한다.

## 모듈식 스텝 시스템

생성 파이프라인은 **독립적인 8개 스텝**으로 구성된다. 각 스텝은 서로 다른 에이전트가 독립적으로 실행할 수 있으며, JSON 파일을 통해 입출력을 주고받는다.

### 스텝과 Skill 커맨드

| 스텝 | Skill | 설명 |
|---|---|---|
| `concept_design` | `/concept-design` | 세계관 + feature outlines 설계 |
| `feature_design` | `/feature-design` | feature별 스토리/에셋/맵 상세 설계 |
| `map_generation` | `/map-gen` | Landscape, Spawner, Region 정의 |
| `story_generation` | `/story-gen` | HktCore 주입용 스토리 |
| `asset_discovery` | `/asset-discovery` | 의존 어셋 분석 |
| `character_generation` | `/char-gen` | Mesh + Animation 생성 |
| `item_generation` | `/item-gen` | Item Mesh + Icon 생성 |
| `vfx_generation` | `/vfx-gen` | Niagara VFX 생성 |
| `texture_generation` | `/texture-gen` | SD WebUI 텍스처 생성 |

전체 파이프라인: `/full-pipeline <프로젝트이름> <컨셉>`

### 의존 관계

```
concept_design
  ├─→ map_generation              (병렬)
  └─→ feature_design
        └─→ [feature별 Worker Agent 병렬 스폰]
              ├─→ story_generation
              ├─→ asset_discovery
              └─→ char/item/vfx_generation (병렬)
```

### 멀티 에이전트 오케스트레이션

`/full-pipeline`은 Orchestrator 패턴을 사용한다:
1. concept_design → feature_outlines[] 생성 (직접 실행)
2. feature_design → features[] 상세 설계 (직접 실행)
3. feature별 `/feature-worker` Agent를 **병렬 스폰**
4. 각 Worker는 자기 feature의 story→asset_discovery→asset_gen을 독립 실행
5. 모든 Worker 완료 후 `feature_aggregate`로 결과 집계

### Feature 시스템

프로젝트의 모든 작업은 **feature** 단위로 추적된다:
- **Pipeline feature**: concept_design에서 feature_outlines로 도출, feature_design에서 상세화
- **Manual feature**: 개별 탭에서 직접 생성 시 ad-hoc feature 자동 등록 (`manual-{type}-{timestamp}`)
- **Per-feature work.json**: `.hkt_steps/{project_id}/features/{feature_id}/work.json` — Worker 간 충돌 방지
- **Manifest 추적**: `FeatureStatus`로 feature별 진행 상황 (stories/assets 완료 수) 추적

### 규칙

- 각 스텝은 독립적 — 다른 에이전트가 이어서 실행 가능
- 상세 실행 절차는 `.claude/skills/`의 Skill 파일에 정의
- 스텝 간 데이터는 `.hkt_steps/{project_id}/{step_type}/output.json`으로 전달
- Feature별 데이터는 `.hkt_steps/{project_id}/features/{feature_id}/work.json`으로 격리
- 스텝 실패 시 `step_fail`로 에러 기록

## HktMap

JSON 기반 맵 정의 (UMap 아님). 런타임에 동적 로드/언로드 가능. Landscape, Region, Spawner, Story 참조, Props로 구성.

## Tag 시스템과 에셋 연결

### 런타임 에셋 로딩 (TagDataAsset 시스템)
런타임에서 에셋은 `UHktTagDataAsset` 파생 클래스를 통해 로드됨:
- `UHktActorVisualDataAsset` — ActorClass 참조 (캐릭터/엔티티)
- `UHktVFXVisualDataAsset` — NiagaraSystem 하드 참조 (VFX, DataAsset 비동기 로드 시 함께 로드)
- `UHktAssetSubsystem::LoadAssetAsync()` 로 비동기 로드

### Convention Path (Generator 출력 경로용)
Generator가 에셋을 생성할 때 출력 경로를 결정하는 규칙. 런타임 로딩에는 사용되지 않음.

| Tag 패턴 | Generator 출력 경로 패턴 |
|---|---|
| `Entity.Character.{Name}` | `{Root}/Characters/{Name}/BP_{Name}` |
| `Entity.Item.{Cat}.{Sub}` | `{Root}/Items/{Cat}/SM_{Sub}` |
| `VFX.Niagara.{Name}` | `{Root}/VFX/NS_{Name}` |
| `VFX.{Event}.{Element}` | `{Root}/VFX/NS_VFX_{Event}_{Element}` |
| `Anim.{Layer}.{Type}.{Name}` | `{Root}/Animations/Anim_{Layer}_{Type}_{Name}` |

`{Root}` = Project Settings > HktGameplay > HktAsset > ConventionRootDirectory (기본: `/Game/Generated`)

### Generator → TagDataAsset 연결
Generator의 `HandleTagMiss`는 에셋 생성 후 해당 TagDataAsset도 함께 생성하여 런타임에서 발견 가능하게 함.
예: VFX Generator → NiagaraSystem 빌드 → `UHktVFXVisualDataAsset` 생성 (IdentifierTag + NiagaraSystem 참조)

## 코드 구조

- `McpServer/src/hkt_mcp/` — Python MCP 서버 (tools/, steps/, bridge/, prompt/, server.py)
- `Source/` — C++ UE5 플러그인 모듈
  - `HktGeneratorEditor/` — **Generator Prompt 패널** (Claude CLI subprocess, 탭 UI, 피드백 루프)
  - `HktMcpBridgeEditor/` — 에디터 서브시스템 + Function Library (MCP 통신)
  - `HktMapGenerator/` — HktMap JSON 파싱, Landscape/Spawner 빌드
  - `HktStoryGenerator/` — Story 바이트코드 VM
  - `HktVFXGenerator/` — Niagara VFX 생성
  - `HktMeshGenerator/`, `HktAnimGenerator/`, `HktItemGenerator/`, `HktTextureGenerator/`
  - `HktGeneratorCore/` — Tag 해석, ConventionPath, GeneratorRouter

## Generator Prompt 패널

에디터 내 Generator Prompt 패널 (`HktGen.Prompt` 콘솔 커맨드로 열기).
Claude Code CLI의 OAuth 토큰을 활용하여 subprocess로 생성을 실행한다.

- Generator별 탭: VFX, Character, Item, Map, Story, Texture, Feature
- Intent 편집 → Generate → 스트리밍 진행 로그 → 결과 확인 → Accept/Refine/Reject
- `FHktClaudeProcess`: `claude --print --output-format stream-json` subprocess 래퍼
- SKILL.md를 system prompt로, Intent JSON을 user prompt로 전달
- Refine 시 이전 결과 + 피드백을 포함하여 재실행
- 개별 탭에서 직접 생성 시 ad-hoc feature 자동 등록

## 환경설정

| 환경변수 | 설명 | 기본값 |
|---|---|---|
| `HKT_STEPS_DIR` | 스텝 데이터 저장 경로 | `.hkt_steps/` |
| `HKT_MAPS_DIR` | HktMap JSON 저장 경로 | `.hkt_maps/` |

UE5 측 출력 경로는 Project Settings에서 설정 (HktAsset, HktMapGenerator, HktVFXGenerator 등 각 모듈별).
