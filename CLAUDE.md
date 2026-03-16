# HktGameplayGenerator - Claude Code 가이드

## 프로젝트 개요

LLM 기반 UE5 게임플레이 어셋 자동 생성 시스템. MCP(Model Context Protocol)를 통해 언리얼 에디터와 통신하며, Story/VFX/Mesh/Animation/Texture/Item을 생성한다.

## 파이프라인 모니터 (필수 사용)

복수의 Story나 어셋을 생성하는 작업에는 **반드시 파이프라인 모니터 MCP 도구를 사용**할 것.
프롬프트만으로 관리하지 말고, 아래 도구로 상태를 구조적으로 추적한다.

### 워크플로우

1. `pipeline_create` — 파이프라인 생성
2. `pipeline_add_tasks` — 현재 페이즈에 작업 추가
3. 각 작업 수행 후 `pipeline_update_task` — 결과 기록
4. 페이즈 완료 시 `pipeline_request_advance` — 체크포인트 생성, 사용자 리뷰 대기
5. 사용자 승인 후 `pipeline_resolve_checkpoint` — 다음 페이즈로 진행

### 5단계 페이즈

| 페이즈 | 하는 일 |
|---|---|
| design | 맵 요소 분석, 스토리 설계 |
| task_planning | 작업 목록 생성, 사용자 확인 |
| story_building | Story JSON 작성 → validate → build |
| asset_discovery | 의존 어셋 분석, 어셋 작업 목록 생성 |
| verification | 전체 검증, 누락 어셋 확인 |

### 규칙

- 페이즈를 건너뛰지 않는다
- 체크포인트에서 반드시 사용자 승인을 받는다
- 각 작업의 `mcp_tool_hint`를 설정하여 어떤 MCP 도구를 쓸지 명시한다
- 작업 실패 시 `status: "failed"`와 `error`를 기록한다

## MCP 도구 참고

### Story 생성
1. `get_story_schema` → 스키마 확인
2. `get_story_examples` → 패턴 학습
3. `validate_story` → 문법 검증
4. `analyze_story_dependencies` → 의존 어셋 확인
5. `build_story` → 컴파일 & VM 등록

### 어셋 생성
- `request_character_mesh` / `import_mesh` — 캐릭터 메시
- `request_animation` / `import_animation` — 애니메이션
- `build_vfx_system` — VFX/Niagara
- `generate_texture` / `import_texture` — 텍스처
- `request_item` / `import_item_mesh` — 아이템

## 코드 구조

- `McpServer/src/hkt_mcp/` — Python MCP 서버
  - `pipeline/` — 파이프라인 모니터 (models, store, state_machine, reporter)
  - `tools/` — MCP 도구 함수
  - `bridge/` — UE5 통신 (Remote Control API)
  - `server.py` — 도구 등록 & 디스패치
- `Source/` — C++ UE5 플러그인 모듈
  - `HktMcpBridgeEditor/` — 에디터 서브시스템 + Pipeline Monitor Slate 패널
  - `HktStoryGenerator/` — Story 바이트코드 VM
  - `HktVFXGenerator/` — Niagara VFX 생성
  - `HktMeshGenerator/`, `HktAnimGenerator/`, `HktItemGenerator/`, `HktTextureGenerator/`
