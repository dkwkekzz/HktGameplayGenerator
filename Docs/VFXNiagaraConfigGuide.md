# VFX Niagara Config JSON Guide

LLM이 MCP를 통해 Niagara VFX를 생성할 때 사용하는 Config JSON 포맷.

## JSON 스키마

```json
{
  "systemName": "FireExplosion",
  "emitters": [
    {
      "name": "MainBurst",
      "spawn": {
        "mode": "burst",
        "rate": 0,
        "burstCount": 50
      },
      "init": {
        "lifetimeMin": 0.3,
        "lifetimeMax": 1.5,
        "sizeMin": 10,
        "sizeMax": 60,
        "velocityMin": { "x": -500, "y": -500, "z": -200 },
        "velocityMax": { "x": 500, "y": 500, "z": 800 },
        "color": { "r": 1.0, "g": 0.5, "b": 0.1, "a": 1.0 }
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": -980 },
        "drag": 1.5
      },
      "render": {
        "rendererType": "sprite",
        "blendMode": "additive",
        "sortOrder": 0
      }
    }
  ]
}
```

## 필드 설명

### systemName
에셋 이름. `NS_` 접두사가 자동으로 추가됨.
예: `"FireExplosion"` → `NS_FireExplosion`

### emitters[]

하나의 Niagara 시스템은 여러 에미터를 가질 수 있음.

#### spawn
| 필드 | 타입 | 설명 |
|------|------|------|
| mode | string | `"burst"` (한번에 생성) 또는 `"rate"` (지속 생성) |
| rate | float | rate 모드: 초당 파티클 수 |
| burstCount | int | burst 모드: 한번에 생성할 파티클 수 |

#### init
| 필드 | 타입 | 설명 |
|------|------|------|
| lifetimeMin/Max | float | 파티클 수명 (초) |
| sizeMin/Max | float | 파티클 크기 (UE 유닛, 1 유닛 ≈ 1cm) |
| velocityMin/Max | {x,y,z} | 초기 속도 범위 (UE 유닛/초) |
| color | {r,g,b,a} | 파티클 색상 (0~1, HDR 값도 가능) |

#### update
| 필드 | 타입 | 설명 |
|------|------|------|
| gravity | {x,y,z} | 중력 (기본: z=-980) |
| drag | float | 공기 저항 (0=없음, 높을수록 빨리 감속) |

#### render
| 필드 | 타입 | 설명 |
|------|------|------|
| rendererType | string | `"sprite"`, `"ribbon"`, `"light"`, `"mesh"` |
| blendMode | string | `"additive"` (빛나는 효과), `"translucent"` (알파 블렌딩) |
| sortOrder | int | 렌더 순서 (높을수록 앞에 그려짐) |

## MCP 호출 예제

### 1. 스키마 조회
```
McpGetVFXConfigSchema() → JSON 스키마 문자열
```

### 2. Config로 빌드
```
McpBuildNiagaraSystem(jsonConfig, "/Game/GeneratedVFX")
→ { "success": true, "assetPath": "/Game/GeneratedVFX/NS_FireExplosion" }
```

### 3. 프리셋 폭발
```
McpBuildPresetExplosion("TestExplosion", 1.0, 0.3, 0.0, 0.8, "/Game/GeneratedVFX")
→ { "success": true, "assetPath": "/Game/GeneratedVFX/NS_TestExplosion" }
```

### 4. 생성된 VFX 목록
```
McpListGeneratedVFX("/Game/GeneratedVFX")
→ { "assets": [ { "name": "NS_FireExplosion", "path": "..." } ] }
```

## 이펙트 디자인 가이드라인

### 폭발 (Explosion)
- burst 모드, 파티클 30~100개
- 높은 초기 속도 (300~1000), 높은 드래그 (1~3)
- 밝은 색상 + additive 블렌딩
- light 에미터 추가로 플래시 효과

### 지속 효과 (Buff/AreaEffect)
- rate 모드, 초당 10~30개
- 낮은 초기 속도, 약한 중력 또는 위로 향하는 속도
- 반투명 또는 additive

### 투사체 궤적 (ProjectileTrail)
- rate 모드, 초당 20~60개
- ribbon 렌더러 사용
- 짧은 수명 (0.2~0.5초)

### 힐 (Heal)
- burst + rate 조합 (2개 에미터)
- 위로 향하는 속도 (z > 0)
- 녹색/흰색 계열 색상
