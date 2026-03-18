# HktGameplayGenerator - Claude Code 가이드

## 프로젝트 개요

LLM 기반 UE5 게임플레이 어셋 자동 생성 시스템. MCP(Model Context Protocol)를 통해 언리얼 에디터와 통신하며, Map/Story/VFX/Mesh/Animation/Texture/Item을 생성한다.

## 모듈식 스텝 시스템

생성 파이프라인은 **독립적인 7개 스텝**으로 구성된다. 각 스텝은 서로 다른 에이전트가 독립적으로 실행할 수 있으며, JSON 파일을 통해 입출력을 주고받는다.

### 7개 스텝과 Skill 커맨드

| 스텝 | Skill | 입력 | 출력 | 설명 |
|---|---|---|---|---|
| `concept_design` | `/concept-design` | 사용자 컨셉 텍스트 | 지형 명세 + 스토리 리스트 | 맵/스토리 전체 설계 |
| `map_generation` | `/map-gen` | concept_design 출력 | HktMap JSON | Landscape, Spawner, Region 정의 |
| `story_generation` | `/story-gen` | concept_design 출력 | Story JSON 파일들 | HktCore 주입용 스토리 |
| `asset_discovery` | `/asset-discovery` | story_generation 출력 | 어셋 명세 (Character/Item/VFX) | 의존 어셋 분석 |
| `character_generation` | `/char-gen` | asset_discovery 출력 (characters) | uasset 경로들 | Mesh + Animation 생성 |
| `item_generation` | `/item-gen` | asset_discovery 출력 (items) | uasset 경로들 | Item Mesh + Icon 생성 |
| `vfx_generation` | `/vfx-gen` | asset_discovery 출력 (vfx) | uasset 경로들 | Niagara VFX 생성 |

전체 파이프라인을 한 번에 실행하려면 `/full-pipeline <프로젝트이름> <컨셉>` 사용.

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

### 스텝 MCP 도구

#### 프로젝트 관리
- `step_create_project` — 프로젝트 생성 (이름, 컨셉, 설정)
- `step_list_projects` — 프로젝트 목록 조회
- `step_get_status` — 프로젝트 전체 상태 조회
- `step_delete_project` — 프로젝트 삭제

#### 스텝 실행
- `step_begin` — 스텝 시작 (in_progress 표시, 상위 스텝에서 입력 자동 해석)
- `step_save_output` — 스텝 출력 저장 (완료 표시)
- `step_load_input` — 스텝 입력 로드 (상위 스텝 출력에서 자동 해석)
- `step_fail` — 스텝 실패 기록

#### 스키마
- `step_get_schema` — 스텝의 입출력 JSON 스키마 조회
- `step_list_types` — 전체 스텝 타입 목록

### 규칙

- 각 스텝은 독립적이다 — 다른 에이전트가 이어서 실행할 수 있다
- 각 스텝의 **상세 실행 절차**는 `.claude/skills/` 의 Skill 파일에 정의되어 있다
- 스텝 간 데이터는 `.hkt_steps/{project_id}/{step_type}/output.json` 파일로 전달된다
- MCP 도구로도, 파일 직접 접근으로도 스텝 데이터를 읽을 수 있다
- 스텝 실패 시 `step_fail`로 에러를 기록한다
- 출력 경로는 환경설정(`HKT_STEPS_DIR`, `HKT_MAPS_DIR`)으로 변경 가능하다

## HktMap

HktMap은 **UMap이 아닌 JSON 기반 맵 정의**로, 런타임에 동적 로드/언로드 가능하다.

### 구성 요소
- **Landscape**: 지형 (heightmap, 크기, biome, 레이어)
- **Region**: 이름 있는 영역 (bounds, 속성)
- **Spawner**: 엔티티 스폰 지점 (위치, 스폰 규칙, 수량)
- **Story 참조**: 맵 로드 시 연결된 스토리도 로드
- **Props**: 정적 오브젝트 배치

### Map MCP 도구
- `get_map_schema` — HktMap JSON 스키마
- `validate_map` — 맵 유효성 검사
- `save_map` / `load_map` / `list_maps` / `delete_map` — CRUD
- `build_map` — UE5에 맵 빌드 (Landscape + Spawner + Story 등록)

## Story 생성 도구

1. `get_story_schema` → 스키마 확인
2. `get_story_examples` → 패턴 학습
3. `validate_story` → 문법 검증
4. `analyze_story_dependencies` → 의존 어셋 확인
5. `build_story` → 컴파일 & VM 등록

## 어셋 생성 도구

- `request_character_mesh` / `import_mesh` — 캐릭터 메시
- `request_animation` / `import_animation` — 애니메이션
- `build_vfx_system` — VFX/Niagara
- `generate_texture` / `import_texture` — 텍스처
- `request_item` / `import_item_mesh` — 아이템

## Tag 시스템과 명명 규칙

| Tag 패턴 | 경로 패턴 | 예시 |
|---|---|---|
| `Entity.Character.{Name}` | `{Root}/Characters/{Name}/BP_{Name}` | Entity.Character.Goblin → /Game/Generated/Characters/Goblin/BP_Goblin |
| `Entity.Item.{Cat}.{Sub}` | `{Root}/Items/{Cat}/SM_{Sub}` | Entity.Item.Weapon.Sword → /Game/Generated/Items/Weapon/SM_Sword |
| `VFX.{Event}.{Element}` | `{Root}/VFX/VFX_{Event}_{Element}` | VFX.Explosion.Fire → /Game/Generated/VFX/VFX_Explosion_Fire |
| `Anim.{Layer}.{Type}.{Name}` | `{Root}/Animations/Anim_{Layer}_{Type}_{Name}` | Anim.FullBody.Action.Spawn → /Game/Generated/Animations/Anim_FullBody_Action_Spawn |

`{Root}`은 Project Settings > HktGameplay > HktAsset > ConventionRootDirectory (기본: `/Game/Generated`)

## 코드 구조

- `McpServer/src/hkt_mcp/` — Python MCP 서버
  - `steps/` — 모듈식 스텝 시스템 (models, store)
  - `tools/` — MCP 도구 함수 (step_tools, map_tools, story_tools, vfx_tools 등)
  - `bridge/` — UE5 통신 (Remote Control API)
  - `server.py` — 도구 등록 & 디스패치
- `Source/` — C++ UE5 플러그인 모듈
  - `HktMcpBridgeEditor/` — 에디터 서브시스템 + Step Viewer Slate 패널
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

UE5 측 출력 경로는 Project Settings에서 설정:
- HktAsset > ConventionRootDirectory
- HktMapGenerator > MapOutputDirectory
- HktVFXGenerator > DefaultOutputDirectory
- HktMeshGenerator, HktAnimGenerator, HktItemGenerator, HktTextureGenerator 각각 설정 가능
