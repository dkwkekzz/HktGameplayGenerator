---
name: editor-guide
description: Monolith과 HktMcpBridge 두 MCP 서버의 도구 목록과 용도를 안내한다.
allowed-tools: Read
argument-hint: <작업 설명>
---

# MCP 서버 도구 안내

## Monolith — 범용 UE5 에디터

| 도구 | 용도 |
|---|---|
| `project_query` | 에셋 검색 (FTS5), 상호 참조 |
| `blueprint_query` | BP 그래프 생성/편집, 변수, 컴파일 |
| `material_query` | 머티리얼 그래프, PBR, HLSL 커스텀 노드 |
| `animation_query` | 커브/본 트랙, 몽타주, 블렌드스페이스, 소켓 |
| `niagara_query` | 이미터/모듈/렌더러 편집, 파라미터 바인딩 |
| `editor_query` | 라이브 코딩, 빌드 오류 로그 |
| `source_query` | C++ 클래스/함수 선언, 호출 그래프 |
| `config_query` | INI 설정 탐색, 기본값 추적 |

## HktMcpBridge — Generator Skill 도구 + 에디터 보조

**Generator 도구** (각 Skill이 사용):
- Step 파이프라인: `step_create_project`, `step_begin`, `step_save_output` 등
- Map: `build_map`, `validate_map`, `save_map` 등
- Story: `build_story`, `validate_story` 등
- VFX: `build_vfx_system` (Niagara 시스템 생성)
- Mesh/Anim/Item/Texture: `request_*` + `import_*`

**에디터 보조 도구:**
- 레벨 액터: `list_actors`, `spawn_actor`, `modify_actor`, `delete_actor`, `select_actor`
- 뷰포트: `get_viewport_camera`, `set_viewport_camera`
- 런타임: `start_pie`, `stop_pie`, `execute_console_command`, `get_game_state`
- 에셋: `list_assets`, `get_asset_info`, `search_assets`, `modify_asset`
- 쿼리: `search_classes`, `get_class_properties`, `get_project_structure`, `get_level_info`
