# HktVFXGenerator 모듈

LLM이 MCP를 통해 Niagara VFX 에셋을 자동 생성하는 에디터 전용 모듈.

## 아키텍처

```
LLM (Claude/MCP)
  │
  ▼ JSON Config
UHktVFXGeneratorFunctionLibrary (MCP API 표면)
  │
  ▼
UHktVFXGeneratorSubsystem (에디터 서브시스템)
  │
  ▼
FHktVFXNiagaraBuilder (Config → UNiagaraSystem .uasset)
  │
  ▼
UNiagaraSystem (Content Browser 에셋)
```

## 핵심 전략

- C++ 내에서 LLM을 호출하지 않음
- C++는 `Config → Niagara` 빌드 기능만 제공
- LLM이 MCP를 통해 Config JSON을 채워서 호출하는 구조

## 주요 클래스

| 클래스 | 역할 |
|--------|------|
| `FHktVFXNiagaraConfig` | JSON ↔ C++ 직렬화 가능한 설정 구조체 |
| `FHktVFXNiagaraBuilder` | Config로 UNiagaraSystem 에셋 빌드 |
| `UHktVFXGeneratorSubsystem` | 에디터 서브시스템 API |
| `UHktVFXGeneratorFunctionLibrary` | MCP Remote Control API 표면 |

## MCP 함수

- `McpBuildNiagaraSystem(JsonConfig, OutputDir)` — JSON Config로 빌드
- `McpBuildPresetExplosion(Name, R, G, B, Intensity, OutputDir)` — 프리셋 폭발
- `McpGetVFXConfigSchema()` — Config JSON 스키마 조회
- `McpListGeneratedVFX(Directory)` — 생성된 VFX 목록

## Phase 로드맵

- **Phase 0** (현재): 템플릿 기반 Config→Niagara 빌드
- **Phase 1**: MCP Python 도구 추가 (`vfx_tools.py`)
- **Phase 2**: 커브, 노이즈, 텍스처 생성 등 고급 기능
