---
name: editor-guide
description: UE5 에디터 작업 시 Monolith과 HktMcpBridge 중 어떤 MCP 서버 도구를 사용할지 안내한다.
allowed-tools: Read
argument-hint: <작업 설명>
---

# MCP 서버 선택 가이드

사용자가 UE5 에디터 작업을 요청할 때, 어떤 MCP 서버의 어떤 도구를 사용할지 안내한다.

## 2개 MCP 서버 구성

### Monolith (기본 — 범용 에디터)
HTTP API 기반 UE5 MCP 플러그인. 에셋 검색, Blueprint/Material/Animation/Niagara 그래프 편집, 빌드/컴파일, C++ 소스 분석에 사용.

| 도구 | 용도 |
|---|---|
| `project_query` | 에셋 검색 (FTS5), 상호 참조 |
| `blueprint_query` | BP 그래프 생성/편집, 변수, 컴파일 |
| `material_query` | 머티리얼 그래프, PBR, HLSL 커스텀 노드 |
| `animation_query` | 커브/본 트랙, 몽타주, 블렌드스페이스, 소켓 |
| `niagara_query` | 이미터/모듈/렌더러 편집, 파라미터 바인딩, HLSL |
| `editor_query` | 라이브 코딩, 빌드 오류 로그 |
| `source_query` | C++ 클래스/함수 선언, 호출 그래프, 상속 계층 |
| `config_query` | INI 설정 탐색, 기본값 추적 |

### HktMcpBridge (보조 — HKT 전용 + Monolith 미지원)

**HKT 전용 생성기:**
- Step 파이프라인: `step_create_project`, `step_begin`, `step_save_output` 등
- HktMap: `build_map`, `validate_map`, `save_map` 등
- Story: `build_story`, `validate_story` 등
- VFX 시스템 생성: `build_vfx_system` (완전한 Niagara 시스템을 새로 생성)
- Mesh/Anim/Item/Texture: `request_*` + `import_*`

**Monolith에 없는 에디터 기능:**
- 레벨 액터: `list_actors`, `spawn_actor`, `modify_actor`, `delete_actor`, `select_actor`
- 뷰포트: `get_viewport_camera`, `set_viewport_camera`
- 런타임: `start_pie`, `stop_pie`, `execute_console_command`, `get_game_state`
- 에셋 CRUD: `list_assets` (경로별 리스팅), `get_asset_info`, `modify_asset`

## 판단 흐름

```
사용자 요청 분석
  │
  ├─ 에셋 검색/탐색 → Monolith project_query
  ├─ Blueprint 작업 → Monolith blueprint_query
  ├─ 머티리얼 작업 → Monolith material_query
  ├─ 기존 Niagara 수정 → Monolith niagara_query
  ├─ 기존 애니메이션 수정 → Monolith animation_query
  ├─ 빌드/컴파일 → Monolith editor_query
  ├─ C++ 분석 → Monolith source_query
  │
  ├─ 새 VFX 시스템 생성 → HktMcpBridge build_vfx_system
  ├─ 새 어셋 생성 (mesh/anim/item) → HktMcpBridge request_*
  ├─ Story 생성/빌드 → HktMcpBridge story_tools
  ├─ Map 생성/빌드 → HktMcpBridge map_tools
  ├─ 생성 파이프라인 → HktMcpBridge step_tools
  │
  ├─ 레벨에 액터 배치/수정 → HktMcpBridge level_tools
  ├─ PIE 실행/중지 → HktMcpBridge runtime_tools
  └─ 뷰포트 카메라 → HktMcpBridge viewport tools
```

## 핵심 구분: "새로 생성" vs "기존 편집"

- **새 Niagara 시스템** (VFX.Explosion.Fire 같은 태그로) → `build_vfx_system` (HktMcpBridge)
- **기존 Niagara 에셋 파라미터 수정** → `niagara_query` (Monolith)
- **새 애니메이션 생성 요청** → `request_animation` (HktMcpBridge)
- **기존 애니메이션 커브/몽타주 편집** → `animation_query` (Monolith)
