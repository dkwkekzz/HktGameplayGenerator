# HKT VFX Generator — Production-Quality Example Configs

이 문서는 실제 게임에서 사용 가능한 수준의 VFX Config 예제를 제공합니다.
기존 `McpGetVFXExampleConfigs()`의 예제보다 더 풍부한 레이어 구성과 미사용 템플릿 활용을 보여줍니다.

> **빌더 제약 참고**: 현재 빌더는 Min/Max 값을 평균으로 압축합니다. (`[BUILDER_LIMIT]` 표기)
> 이 제약이 해소되면 아래 Config의 랜덤 범위가 실제로 적용되어 훨씬 풍부해집니다.

---

## 1. AAA Explosion — 대형 폭발 (8 에미터)

핵심: core flash → explosion SubUV → sparks(1차+2차) → smoke → debris(mesh) → ground ring → distortion → dynamic light

```json
{
  "systemName": "AAA_Explosion_Large",
  "emitters": [
    {
      "name": "CoreFlash",
      "spawn": { "mode": "burst", "burstCount": 2 },
      "init": {
        "lifetimeMin": 0.03, "lifetimeMax": 0.12,
        "sizeMin": 120, "sizeMax": 300,
        "color": { "r": 8, "g": 5, "b": 1.5 }
      },
      "update": { "sizeScaleEnd": 2.5, "opacityEnd": 0 },
      "render": { "emitterTemplate": "core", "sortOrder": 15 }
    },
    {
      "name": "ExplosionBurst",
      "_comment": "SubUV animated explosion — 8x8 flipbook 텍스처가 내장된 rich template",
      "spawn": { "mode": "burst", "burstCount": 8 },
      "init": {
        "lifetimeMin": 0.2, "lifetimeMax": 0.6,
        "sizeMin": 80, "sizeMax": 200,
        "color": { "r": 3, "g": 1.5, "b": 0.3 }
      },
      "update": {
        "sizeScaleEnd": 1.8, "opacityEnd": 0,
        "useColorOverLife": true, "colorEnd": { "r": 0.5, "g": 0.1, "b": 0 }
      },
      "render": { "emitterTemplate": "explosion", "sortOrder": 10 }
    },
    {
      "name": "PrimarySparks",
      "_comment": "빠른 1차 스파크 — velocity_aligned로 궤적 스트릭",
      "spawn": { "mode": "burst", "burstCount": 120 },
      "init": {
        "lifetimeMin": 0.3, "lifetimeMax": 1.2,
        "sizeMin": 2, "sizeMax": 8,
        "velocityMin": { "x": -800, "y": -800, "z": -300 },
        "velocityMax": { "x": 800, "y": 800, "z": 1200 },
        "color": { "r": 2, "g": 1.2, "b": 0.3 }
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": -980 },
        "drag": 1.5,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "spark",
        "alignment": "velocity_aligned",
        "sortOrder": 7
      }
    },
    {
      "name": "SecondarySparks",
      "_comment": "느린 2차 스파크 — 1차보다 지연 발생, 더 작고 오래 지속",
      "spawn": { "mode": "burst", "burstCount": 60, "burstDelay": 0.08 },
      "init": {
        "lifetimeMin": 0.8, "lifetimeMax": 2.5,
        "sizeMin": 1, "sizeMax": 4,
        "velocityMin": { "x": -400, "y": -400, "z": -100 },
        "velocityMax": { "x": 400, "y": 400, "z": 600 },
        "color": { "r": 1.5, "g": 0.6, "b": 0.1 }
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": -980 },
        "drag": 2.5,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "spark_secondary",
        "alignment": "velocity_aligned",
        "sortOrder": 6
      }
    },
    {
      "name": "Smoke",
      "_comment": "지연된 연기 — 폭발 후 0.05초 뒤 상승",
      "spawn": { "mode": "burst", "burstCount": 20, "burstDelay": 0.05 },
      "init": {
        "lifetimeMin": 1.5, "lifetimeMax": 4,
        "sizeMin": 50, "sizeMax": 150,
        "velocityMin": { "x": -80, "y": -80, "z": 30 },
        "velocityMax": { "x": 80, "y": 80, "z": 120 },
        "color": { "r": 0.12, "g": 0.1, "b": 0.08, "a": 0.6 }
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": 40 },
        "drag": 2,
        "noiseStrength": 40, "noiseFrequency": 1.5,
        "sizeScaleEnd": 4,
        "opacityEnd": 0,
        "useColorOverLife": true, "colorEnd": { "r": 0.06, "g": 0.05, "b": 0.04 }
      },
      "render": {
        "emitterTemplate": "smoke",
        "sortOrder": 1
      }
    },
    {
      "name": "MeshDebris",
      "_comment": "3D 파편 — upward_mesh_burst 사용 (기존 예제에 없던 템플릿)",
      "spawn": { "mode": "burst", "burstCount": 12 },
      "init": {
        "lifetimeMin": 0.8, "lifetimeMax": 2.5,
        "sizeMin": 3, "sizeMax": 12,
        "velocityMin": { "x": -500, "y": -500, "z": 200 },
        "velocityMax": { "x": 500, "y": 500, "z": 800 },
        "spriteRotationMin": 0, "spriteRotationMax": 360
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": -980 },
        "drag": 0.5,
        "rotationRateMin": -180, "rotationRateMax": 180
      },
      "render": {
        "emitterTemplate": "upward_mesh_burst",
        "sortOrder": 4
      }
    },
    {
      "name": "GroundDust",
      "_comment": "지면 충격파 먼지 링 — ground_dust 템플릿 (기존 예제에 없던 템플릿)",
      "spawn": { "mode": "burst", "burstCount": 25, "burstDelay": 0.02 },
      "init": {
        "lifetimeMin": 0.8, "lifetimeMax": 2,
        "sizeMin": 30, "sizeMax": 80,
        "velocityMin": { "x": -300, "y": -300, "z": 0 },
        "velocityMax": { "x": 300, "y": 300, "z": 50 },
        "color": { "r": 0.4, "g": 0.35, "b": 0.25, "a": 0.5 }
      },
      "update": {
        "drag": 3,
        "sizeScaleEnd": 3,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "ground_dust",
        "blendMode": "translucent",
        "sortOrder": 0
      }
    },
    {
      "name": "ExplosionLight",
      "spawn": { "mode": "burst", "burstCount": 1 },
      "init": {
        "lifetimeMin": 0.1, "lifetimeMax": 0.5,
        "color": { "r": 4, "g": 2.5, "b": 0.8 }
      },
      "render": {
        "rendererType": "light",
        "lightRadiusScale": 8,
        "lightIntensity": 5
      }
    }
  ]
}
```

**기존 대비 개선점:**
- 8개 에미터 (기존 Explosion_Fire: 6개)
- `spark_secondary` — 2차 스파크 레이어 추가
- `upward_mesh_burst` — 3D 파편 (기존 예제 미사용)
- `ground_dust` — 지면 충격파 링 (기존 예제 미사용)
- Smoke에 curl noise 추가로 자연스러운 난류
- Spark 120개 + Secondary 60개로 밀도 향상
- Color Over Life로 연기 색상 변화

---

## 2. Muzzle Flash — 총구 화염 (5 에미터)

핵심: 순간 플래시 → muzzle SubUV → 전방 스파크 → 연기 잔류 → 다이나믹 라이트

```json
{
  "systemName": "MuzzleFlash_Rifle",
  "emitters": [
    {
      "name": "FlashCore",
      "_comment": "0.02~0.05초의 극히 짧은 밝은 플래시",
      "spawn": { "mode": "burst", "burstCount": 1 },
      "init": {
        "lifetimeMin": 0.02, "lifetimeMax": 0.05,
        "sizeMin": 30, "sizeMax": 60,
        "color": { "r": 10, "g": 8, "b": 3 }
      },
      "update": { "sizeScaleEnd": 1.5, "opacityEnd": 0 },
      "render": { "emitterTemplate": "core", "sortOrder": 15 }
    },
    {
      "name": "MuzzleFlare",
      "_comment": "muzzle_flash 리치 템플릿 — SubUV 총구 화염 (기존 예제 미사용)",
      "spawn": { "mode": "burst", "burstCount": 2 },
      "init": {
        "lifetimeMin": 0.03, "lifetimeMax": 0.08,
        "sizeMin": 20, "sizeMax": 50,
        "color": { "r": 5, "g": 3, "b": 1 }
      },
      "update": { "opacityEnd": 0 },
      "render": {
        "emitterTemplate": "muzzle_flash",
        "sortOrder": 12
      }
    },
    {
      "name": "ForwardSparks",
      "_comment": "전방으로 편향된 스파크 — directional_burst 사용",
      "spawn": { "mode": "burst", "burstCount": 15 },
      "init": {
        "lifetimeMin": 0.1, "lifetimeMax": 0.4,
        "sizeMin": 1, "sizeMax": 3,
        "velocityMin": { "x": 200, "y": -100, "z": -50 },
        "velocityMax": { "x": 800, "y": 100, "z": 150 },
        "color": { "r": 3, "g": 2, "b": 0.5 }
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": -300 },
        "drag": 5,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "directional_burst",
        "blendMode": "additive",
        "alignment": "velocity_aligned",
        "sortOrder": 8
      }
    },
    {
      "name": "GunSmoke",
      "_comment": "총구 잔류 연기 — 느리게 상승하며 확산",
      "spawn": { "mode": "burst", "burstCount": 3, "burstDelay": 0.02 },
      "init": {
        "lifetimeMin": 0.5, "lifetimeMax": 1.5,
        "sizeMin": 5, "sizeMax": 15,
        "velocityMin": { "x": 20, "y": -10, "z": 10 },
        "velocityMax": { "x": 80, "y": 10, "z": 40 },
        "color": { "r": 0.3, "g": 0.3, "b": 0.3, "a": 0.3 }
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": 15 },
        "drag": 2,
        "sizeScaleEnd": 4,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "smoke",
        "sortOrder": 0
      }
    },
    {
      "name": "MuzzleLight",
      "spawn": { "mode": "burst", "burstCount": 1 },
      "init": {
        "lifetimeMin": 0.03, "lifetimeMax": 0.08,
        "color": { "r": 5, "g": 3.5, "b": 1 }
      },
      "render": {
        "rendererType": "light",
        "lightRadiusScale": 4,
        "lightIntensity": 4
      }
    }
  ]
}
```

**포인트:**
- `muzzle_flash` 리치 템플릿 첫 사용
- 총구 화염의 핵심: 극히 짧은 수명 (0.02~0.08초)
- `directional_burst`로 전방 편향 스파크

---

## 3. Sword Slash Trail — 근접 무기 궤적 (5 에미터)

핵심: ribbon trail → 슬래시 잔상 → 에지 스파크 → impact flash → light

```json
{
  "systemName": "SwordSlash_Fire",
  "emitters": [
    {
      "name": "SlashTrail",
      "_comment": "ribbon 템플릿 — 검의 궤적 (기존 예제에서 ribbon 미사용)",
      "spawn": { "mode": "rate", "rate": 60 },
      "init": {
        "lifetimeMin": 0.15, "lifetimeMax": 0.3,
        "color": { "r": 5, "g": 2, "b": 0.3 }
      },
      "update": { "opacityEnd": 0 },
      "render": {
        "emitterTemplate": "ribbon",
        "blendMode": "additive",
        "ribbonWidth": 30,
        "sortOrder": 10
      }
    },
    {
      "name": "SlashGlow",
      "_comment": "궤적을 따라가는 글로우 스프라이트",
      "spawn": { "mode": "rate", "rate": 40 },
      "init": {
        "lifetimeMin": 0.1, "lifetimeMax": 0.25,
        "sizeMin": 15, "sizeMax": 35,
        "color": { "r": 4, "g": 1.5, "b": 0.2 }
      },
      "update": { "sizeScaleEnd": 0.3, "opacityEnd": 0 },
      "render": {
        "emitterTemplate": "core",
        "sortOrder": 12
      }
    },
    {
      "name": "EdgeSparks",
      "_comment": "검날 주변으로 튀는 스파크",
      "spawn": { "mode": "rate", "rate": 25 },
      "init": {
        "lifetimeMin": 0.1, "lifetimeMax": 0.4,
        "sizeMin": 1, "sizeMax": 3,
        "velocityMin": { "x": -200, "y": -200, "z": -100 },
        "velocityMax": { "x": 200, "y": 200, "z": 200 },
        "color": { "r": 3, "g": 1.5, "b": 0.3 }
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": -400 },
        "drag": 3,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "spark",
        "alignment": "velocity_aligned",
        "sortOrder": 7
      }
    },
    {
      "name": "HeatDistortion",
      "_comment": "[BUILDER_LIMIT] distortion 효과 — MI_Distortion 머티리얼 사용. 현재 빌더에서 SubUV 애니메이션 제어 불가",
      "spawn": { "mode": "rate", "rate": 15 },
      "init": {
        "lifetimeMin": 0.1, "lifetimeMax": 0.3,
        "sizeMin": 10, "sizeMax": 25,
        "color": { "r": 1, "g": 1, "b": 1, "a": 0.3 }
      },
      "update": { "sizeScaleEnd": 2, "opacityEnd": 0 },
      "render": {
        "emitterTemplate": "simple_sprite_burst",
        "materialPath": "/Game/NiagaraExamples/Materials/MI_Distortion",
        "blendMode": "translucent",
        "sortOrder": 2
      }
    },
    {
      "name": "TrailLight",
      "spawn": { "mode": "rate", "rate": 20 },
      "init": {
        "lifetimeMin": 0.05, "lifetimeMax": 0.15,
        "color": { "r": 3, "g": 1.5, "b": 0.3 }
      },
      "render": {
        "rendererType": "light",
        "lightRadiusScale": 2,
        "lightIntensity": 2
      }
    }
  ]
}
```

**포인트:**
- `ribbon` 템플릿 첫 사용 — 무기 궤적의 핵심
- `MI_Distortion` 머티리얼로 열 왜곡 효과
- rate 기반 연속 이펙트 (looping 시스템은 아님 — 공격 시 activate/deactivate로 제어)

---

## 4. Bullet Impact — 콘크리트 피탄 (7 에미터)

핵심: impact flash → spark(1차+2차) → chip debris(mesh) → dust puff → 연기 잔류 → scorch mark → light

```json
{
  "systemName": "BulletImpact_Concrete",
  "emitters": [
    {
      "name": "ImpactFlash",
      "_comment": "impact 리치 템플릿 (기존 예제 미사용)",
      "spawn": { "mode": "burst", "burstCount": 2 },
      "init": {
        "lifetimeMin": 0.02, "lifetimeMax": 0.06,
        "sizeMin": 8, "sizeMax": 20,
        "color": { "r": 6, "g": 4, "b": 1.5 }
      },
      "update": { "opacityEnd": 0 },
      "render": { "emitterTemplate": "impact", "sortOrder": 15 }
    },
    {
      "name": "HotSparks",
      "_comment": "뜨거운 금속/석재 스파크 — 위쪽으로 편향, 빠르게 소멸",
      "spawn": { "mode": "burst", "burstCount": 25 },
      "init": {
        "lifetimeMin": 0.1, "lifetimeMax": 0.5,
        "sizeMin": 1, "sizeMax": 3,
        "velocityMin": { "x": -250, "y": -250, "z": 100 },
        "velocityMax": { "x": 250, "y": 250, "z": 600 },
        "color": { "r": 3, "g": 2, "b": 0.8 }
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": -980 },
        "drag": 2,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "spark",
        "alignment": "velocity_aligned",
        "sortOrder": 10
      }
    },
    {
      "name": "DebrisSparks",
      "_comment": "spark_debris 리치 템플릿 — 스파크형 잔해 (기존 예제 미사용)",
      "spawn": { "mode": "burst", "burstCount": 15, "burstDelay": 0.02 },
      "init": {
        "lifetimeMin": 0.3, "lifetimeMax": 1.0,
        "sizeMin": 1, "sizeMax": 5,
        "velocityMin": { "x": -200, "y": -200, "z": 50 },
        "velocityMax": { "x": 200, "y": 200, "z": 400 },
        "color": { "r": 0.6, "g": 0.5, "b": 0.4 }
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": -980 },
        "drag": 1.5
      },
      "render": { "emitterTemplate": "spark_debris", "sortOrder": 8 }
    },
    {
      "name": "ChipMesh",
      "_comment": "impact_mesh 리치 템플릿 — 3D 콘크리트 파편 (기존 예제 미사용)",
      "spawn": { "mode": "burst", "burstCount": 5 },
      "init": {
        "lifetimeMin": 0.5, "lifetimeMax": 1.5,
        "sizeMin": 2, "sizeMax": 6,
        "velocityMin": { "x": -150, "y": -150, "z": 100 },
        "velocityMax": { "x": 150, "y": 150, "z": 350 },
        "spriteRotationMin": 0, "spriteRotationMax": 360
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": -980 },
        "drag": 0.5,
        "rotationRateMin": -300, "rotationRateMax": 300
      },
      "render": { "emitterTemplate": "impact_mesh", "sortOrder": 6 }
    },
    {
      "name": "DustPuff",
      "_comment": "먼지 구름 — dust 리치 템플릿 (기존 예제 미사용 템플릿)",
      "spawn": { "mode": "burst", "burstCount": 8 },
      "init": {
        "lifetimeMin": 0.3, "lifetimeMax": 1.0,
        "sizeMin": 8, "sizeMax": 25,
        "velocityMin": { "x": -100, "y": -100, "z": 0 },
        "velocityMax": { "x": 100, "y": 100, "z": 80 },
        "color": { "r": 0.5, "g": 0.45, "b": 0.35, "a": 0.5 }
      },
      "update": {
        "drag": 4,
        "sizeScaleEnd": 3,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "dust",
        "sortOrder": 1
      }
    },
    {
      "name": "SmokeWisp",
      "_comment": "잔류 연기 — 임팩트 후 살짝 피어오름",
      "spawn": { "mode": "burst", "burstCount": 2, "burstDelay": 0.1 },
      "init": {
        "lifetimeMin": 1, "lifetimeMax": 2.5,
        "sizeMin": 5, "sizeMax": 12,
        "velocityMin": { "x": -5, "y": -5, "z": 10 },
        "velocityMax": { "x": 5, "y": 5, "z": 30 },
        "color": { "r": 0.2, "g": 0.18, "b": 0.15, "a": 0.25 }
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": 8 },
        "sizeScaleEnd": 3,
        "opacityEnd": 0,
        "noiseStrength": 15, "noiseFrequency": 2
      },
      "render": {
        "emitterTemplate": "smoke",
        "sortOrder": 0
      }
    },
    {
      "name": "ImpactLight",
      "spawn": { "mode": "burst", "burstCount": 1 },
      "init": {
        "lifetimeMin": 0.03, "lifetimeMax": 0.1,
        "color": { "r": 3, "g": 2, "b": 0.8 }
      },
      "render": {
        "rendererType": "light",
        "lightRadiusScale": 3,
        "lightIntensity": 3
      }
    }
  ]
}
```

**포인트:**
- 7개 에미터 (기존 GunImpact_Concrete: 3개)
- `impact`, `spark_debris`, `impact_mesh`, `dust` — 기존 예제 미사용 리치 템플릿 4종 활용
- 잔류 연기에 curl noise 추가
- 3D 메시 파편 회전

---

## 5. Electric Arc — 전기 방전 (5 에미터)

핵심: arc ribbon → 코어 글로우 → 보조 스파크 → ambient noise → light

```json
{
  "systemName": "ElectricArc_Discharge",
  "looping": true,
  "warmupTime": 0.5,
  "emitters": [
    {
      "name": "MainArc",
      "_comment": "arc 리치 템플릿 — 전기 아크 리본 (기존 예제 미사용)",
      "spawn": { "mode": "rate", "rate": 80 },
      "init": {
        "lifetimeMin": 0.05, "lifetimeMax": 0.15,
        "color": { "r": 2, "g": 3, "b": 8 }
      },
      "update": {
        "noiseStrength": 150, "noiseFrequency": 8,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "arc",
        "blendMode": "additive",
        "sortOrder": 10
      }
    },
    {
      "name": "ArcCore",
      "_comment": "아크 중심부 글로우",
      "spawn": { "mode": "rate", "rate": 15 },
      "init": {
        "lifetimeMin": 0.03, "lifetimeMax": 0.1,
        "sizeMin": 8, "sizeMax": 20,
        "color": { "r": 4, "g": 5, "b": 10 }
      },
      "update": { "sizeScaleEnd": 0.5, "opacityEnd": 0 },
      "render": { "emitterTemplate": "core", "sortOrder": 12 }
    },
    {
      "name": "ElectricSparks",
      "_comment": "아크에서 튀는 불꽃",
      "spawn": { "mode": "rate", "rate": 20 },
      "init": {
        "lifetimeMin": 0.05, "lifetimeMax": 0.2,
        "sizeMin": 1, "sizeMax": 3,
        "velocityMin": { "x": -300, "y": -300, "z": -300 },
        "velocityMax": { "x": 300, "y": 300, "z": 300 },
        "color": { "r": 2, "g": 3, "b": 6 }
      },
      "update": {
        "drag": 8,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "spark",
        "alignment": "velocity_aligned",
        "blendMode": "additive",
        "sortOrder": 8
      }
    },
    {
      "name": "AmbientGlow",
      "_comment": "주변 분위기 — 느리게 떠다니는 전기 입자",
      "spawn": { "mode": "rate", "rate": 8 },
      "init": {
        "lifetimeMin": 0.3, "lifetimeMax": 0.8,
        "sizeMin": 2, "sizeMax": 6,
        "velocityMin": { "x": -30, "y": -30, "z": -10 },
        "velocityMax": { "x": 30, "y": 30, "z": 10 },
        "color": { "r": 1, "g": 1.5, "b": 4 }
      },
      "update": {
        "noiseStrength": 50, "noiseFrequency": 4,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "hanging_particulates",
        "blendMode": "additive",
        "sortOrder": 5
      }
    },
    {
      "name": "ArcLight",
      "spawn": { "mode": "rate", "rate": 30 },
      "init": {
        "lifetimeMin": 0.03, "lifetimeMax": 0.08,
        "color": { "r": 1.5, "g": 2, "b": 5 }
      },
      "render": {
        "rendererType": "light",
        "lightRadiusScale": 3,
        "lightIntensity": 3
      }
    }
  ]
}
```

**포인트:**
- `arc` 리치 템플릿 첫 사용
- 높은 noise frequency (8)로 전기의 불규칙한 움직임 표현
- 높은 drag (8)로 스파크가 급격히 감속
- 라이트 spawn rate 30으로 전기 특유의 깜빡임

---

## 6. Healing Aura — 힐링/버프 이펙트 (6 에미터)

핵심: 상승 파티클 → 보텍스 링 → 코어 글로우 → ribbon trail → ground glow → light

```json
{
  "systemName": "HealingAura_Nature",
  "looping": true,
  "warmupTime": 2,
  "emitters": [
    {
      "name": "RisingParticles",
      "_comment": "부드럽게 상승하는 치유 입자 — vortex로 나선 궤적",
      "spawn": { "mode": "rate", "rate": 20 },
      "init": {
        "lifetimeMin": 1.5, "lifetimeMax": 3,
        "sizeMin": 3, "sizeMax": 8,
        "velocityMin": { "x": -20, "y": -20, "z": 50 },
        "velocityMax": { "x": 20, "y": 20, "z": 120 },
        "color": { "r": 0.5, "g": 2, "b": 0.8 }
      },
      "update": {
        "vortexStrength": 80, "vortexRadius": 100,
        "drag": 0.5,
        "sizeScaleEnd": 0.2,
        "opacityEnd": 0,
        "useColorOverLife": true, "colorEnd": { "r": 1, "g": 3, "b": 1.5 }
      },
      "render": {
        "emitterTemplate": "hanging_particulates",
        "blendMode": "additive",
        "sortOrder": 7
      }
    },
    {
      "name": "VortexRing",
      "_comment": "중간 높이에서 회전하는 빛 고리",
      "spawn": { "mode": "rate", "rate": 30 },
      "init": {
        "lifetimeMin": 0.5, "lifetimeMax": 1.5,
        "sizeMin": 2, "sizeMax": 5,
        "velocityMin": { "x": -80, "y": -80, "z": -5 },
        "velocityMax": { "x": 80, "y": 80, "z": 5 },
        "color": { "r": 0.3, "g": 1.5, "b": 0.5 }
      },
      "update": {
        "vortexStrength": 200, "vortexRadius": 80,
        "attractionStrength": 100, "attractionRadius": 120,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "hanging_particulates",
        "blendMode": "additive",
        "sortOrder": 6
      }
    },
    {
      "name": "CoreGlow",
      "_comment": "캐릭터 중심부 지속 글로우",
      "spawn": { "mode": "rate", "rate": 3 },
      "init": {
        "lifetimeMin": 0.3, "lifetimeMax": 0.6,
        "sizeMin": 40, "sizeMax": 60,
        "color": { "r": 1, "g": 3, "b": 1.2 }
      },
      "update": { "sizeScaleEnd": 1.3, "opacityEnd": 0.3 },
      "render": { "emitterTemplate": "core", "sortOrder": 3 }
    },
    {
      "name": "SpiralTrail",
      "_comment": "나선형 리본 궤적",
      "spawn": { "mode": "rate", "rate": 40 },
      "init": {
        "lifetimeMin": 0.2, "lifetimeMax": 0.5,
        "velocityMin": { "x": -60, "y": -60, "z": 30 },
        "velocityMax": { "x": 60, "y": 60, "z": 80 },
        "color": { "r": 0.8, "g": 2.5, "b": 1 }
      },
      "update": {
        "vortexStrength": 150, "vortexRadius": 70,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "ribbon",
        "blendMode": "additive",
        "ribbonWidth": 8,
        "sortOrder": 8
      }
    },
    {
      "name": "GroundGlow",
      "_comment": "발 아래 은은한 빛 확산",
      "spawn": { "mode": "rate", "rate": 5 },
      "init": {
        "lifetimeMin": 0.5, "lifetimeMax": 1,
        "sizeMin": 60, "sizeMax": 100,
        "velocityMin": { "x": -5, "y": -5, "z": 0 },
        "velocityMax": { "x": 5, "y": 5, "z": 3 },
        "color": { "r": 0.3, "g": 1.5, "b": 0.5, "a": 0.3 }
      },
      "update": { "sizeScaleEnd": 1.5, "opacityEnd": 0 },
      "render": {
        "emitterTemplate": "ground_dust",
        "blendMode": "additive",
        "sortOrder": 0
      }
    },
    {
      "name": "HealLight",
      "spawn": { "mode": "rate", "rate": 5 },
      "init": {
        "lifetimeMin": 0.2, "lifetimeMax": 0.5,
        "color": { "r": 0.5, "g": 2, "b": 0.8 }
      },
      "render": {
        "rendererType": "light",
        "lightRadiusScale": 4,
        "lightIntensity": 1.5
      }
    }
  ]
}
```

**포인트:**
- Vortex + Attraction 조합으로 나선형 상승 궤적
- `ribbon` 템플릿으로 나선 꼬리
- `ground_dust`를 additive 블렌드로 지면 글로우 효과
- Color Over Life로 녹색→밝은 녹색 변화

---

## 7. Campfire Deluxe — 고급 모닥불 (7 에미터)

기존 Campfire 예제(4 에미터)의 프로덕션 업그레이드.

```json
{
  "systemName": "Campfire_Deluxe",
  "looping": true,
  "warmupTime": 3,
  "emitters": [
    {
      "name": "FlameCore",
      "_comment": "불꽃 중심 — SubUV 화염 텍스처",
      "spawn": { "mode": "rate", "rate": 25 },
      "init": {
        "lifetimeMin": 0.2, "lifetimeMax": 0.6,
        "sizeMin": 20, "sizeMax": 50,
        "velocityMin": { "x": -20, "y": -20, "z": 120 },
        "velocityMax": { "x": 20, "y": 20, "z": 280 },
        "color": { "r": 4, "g": 2, "b": 0.3 }
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": 80 },
        "sizeScaleEnd": 0.1,
        "opacityEnd": 0,
        "useColorOverLife": true, "colorEnd": { "r": 1, "g": 0.1, "b": 0 }
      },
      "render": {
        "emitterTemplate": "fountain",
        "materialPath": "/Game/NiagaraExamples/Materials/MI_FireRoil_8x8",
        "sortOrder": 6
      }
    },
    {
      "name": "FlameOuter",
      "_comment": "외곽 불꽃 — 더 크고 투명, FireBall 텍스처",
      "spawn": { "mode": "rate", "rate": 12 },
      "init": {
        "lifetimeMin": 0.3, "lifetimeMax": 0.7,
        "sizeMin": 30, "sizeMax": 70,
        "velocityMin": { "x": -40, "y": -40, "z": 80 },
        "velocityMax": { "x": 40, "y": 40, "z": 200 },
        "color": { "r": 2.5, "g": 1, "b": 0.1, "a": 0.7 }
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": 60 },
        "noiseStrength": 30, "noiseFrequency": 3,
        "sizeScaleEnd": 0.3,
        "opacityEnd": 0,
        "useColorOverLife": true, "colorEnd": { "r": 0.8, "g": 0.05, "b": 0 }
      },
      "render": {
        "emitterTemplate": "fountain",
        "materialPath": "/Game/NiagaraExamples/Materials/MI_FireBall_8x8",
        "blendMode": "additive",
        "sortOrder": 5
      }
    },
    {
      "name": "Embers",
      "_comment": "상승하는 불티",
      "spawn": { "mode": "rate", "rate": 10 },
      "init": {
        "lifetimeMin": 1.5, "lifetimeMax": 4,
        "sizeMin": 1, "sizeMax": 3,
        "velocityMin": { "x": -30, "y": -30, "z": 60 },
        "velocityMax": { "x": 30, "y": 30, "z": 200 },
        "color": { "r": 3, "g": 1, "b": 0.2 }
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": 20 },
        "drag": 0.3,
        "noiseStrength": 20, "noiseFrequency": 2,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "spark",
        "sortOrder": 8
      }
    },
    {
      "name": "Smoke",
      "_comment": "상승 연기 — SmokeRoil 텍스처로 난류감",
      "spawn": { "mode": "rate", "rate": 4 },
      "init": {
        "lifetimeMin": 3, "lifetimeMax": 6,
        "sizeMin": 25, "sizeMax": 60,
        "velocityMin": { "x": -10, "y": -10, "z": 50 },
        "velocityMax": { "x": 10, "y": 10, "z": 100 },
        "color": { "r": 0.08, "g": 0.06, "b": 0.05, "a": 0.25 }
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": 15 },
        "drag": 0.8,
        "noiseStrength": 25, "noiseFrequency": 1,
        "sizeScaleEnd": 5,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "smoke",
        "materialPath": "/Game/NiagaraExamples/Materials/MI_SmokeRoil_8x8",
        "sortOrder": 0
      }
    },
    {
      "name": "HeatDistortion",
      "_comment": "열 왜곡 — 불 위로 아지랑이",
      "spawn": { "mode": "rate", "rate": 5 },
      "init": {
        "lifetimeMin": 0.5, "lifetimeMax": 1.5,
        "sizeMin": 20, "sizeMax": 40,
        "velocityMin": { "x": -5, "y": -5, "z": 30 },
        "velocityMax": { "x": 5, "y": 5, "z": 60 },
        "color": { "r": 1, "g": 1, "b": 1, "a": 0.15 }
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": 20 },
        "sizeScaleEnd": 2,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "simple_sprite_burst",
        "materialPath": "/Game/NiagaraExamples/Materials/MI_Distortion",
        "blendMode": "translucent",
        "sortOrder": 2
      }
    },
    {
      "name": "GroundGlow",
      "_comment": "바닥 조명 반사",
      "spawn": { "mode": "rate", "rate": 3 },
      "init": {
        "lifetimeMin": 0.3, "lifetimeMax": 0.7,
        "sizeMin": 80, "sizeMax": 120,
        "color": { "r": 1.5, "g": 0.5, "b": 0.1, "a": 0.2 }
      },
      "update": { "opacityEnd": 0.1 },
      "render": {
        "emitterTemplate": "ground_dust",
        "blendMode": "additive",
        "sortOrder": 1
      }
    },
    {
      "name": "FireLight",
      "_comment": "깜빡이는 화재 조명",
      "spawn": { "mode": "rate", "rate": 8 },
      "init": {
        "lifetimeMin": 0.1, "lifetimeMax": 0.3,
        "color": { "r": 2.5, "g": 1.2, "b": 0.3 }
      },
      "render": {
        "rendererType": "light",
        "lightRadiusScale": 5,
        "lightIntensity": 2.5
      }
    }
  ]
}
```

**기존 Campfire 대비 개선점:**
- 7개 에미터 (기존 4개)
- FlameCore + FlameOuter 2중 화염 레이어 (`MI_FireRoil_8x8` + `MI_FireBall_8x8`)
- Smoke에 `MI_SmokeRoil_8x8` 텍스처 + curl noise
- `MI_Distortion`으로 열 아지랑이
- `ground_dust` additive로 바닥 반사광
- 불티에 curl noise 추가

---

## 8. Magic Projectile Impact — 마법 투사체 충돌 (7 에미터)

핵심: 충돌 섬광 → 충격파 링 → 에너지 잔해 → 마법 스파크 → ribbon 잔류 → 연기 → light

```json
{
  "systemName": "MagicImpact_Arcane",
  "emitters": [
    {
      "name": "ImpactFlash",
      "_comment": "순간 섬광 — 강한 HDR 블룸",
      "spawn": { "mode": "burst", "burstCount": 2 },
      "init": {
        "lifetimeMin": 0.03, "lifetimeMax": 0.1,
        "sizeMin": 60, "sizeMax": 120,
        "color": { "r": 3, "g": 1, "b": 8 }
      },
      "update": { "sizeScaleEnd": 2, "opacityEnd": 0 },
      "render": { "emitterTemplate": "core", "sortOrder": 15 }
    },
    {
      "name": "Shockwave",
      "_comment": "확산 충격파 — 빠르게 커지며 사라짐",
      "spawn": { "mode": "burst", "burstCount": 1 },
      "init": {
        "lifetimeMin": 0.15, "lifetimeMax": 0.3,
        "sizeMin": 5, "sizeMax": 10,
        "color": { "r": 2, "g": 0.5, "b": 5, "a": 0.6 }
      },
      "update": { "sizeScaleEnd": 30, "opacityEnd": 0 },
      "render": {
        "emitterTemplate": "simple_sprite_burst",
        "blendMode": "additive",
        "sortOrder": 12
      }
    },
    {
      "name": "EnergyBurst",
      "_comment": "에너지 파편 — vortex로 나선 확산",
      "spawn": { "mode": "burst", "burstCount": 40 },
      "init": {
        "lifetimeMin": 0.3, "lifetimeMax": 1.0,
        "sizeMin": 3, "sizeMax": 10,
        "velocityMin": { "x": -500, "y": -500, "z": -200 },
        "velocityMax": { "x": 500, "y": 500, "z": 600 },
        "color": { "r": 1.5, "g": 0.5, "b": 4 }
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": -300 },
        "drag": 2,
        "vortexStrength": 100, "vortexRadius": 150,
        "opacityEnd": 0,
        "useColorOverLife": true, "colorEnd": { "r": 0.5, "g": 0.1, "b": 1 }
      },
      "render": {
        "emitterTemplate": "omnidirectional_burst",
        "blendMode": "additive",
        "sortOrder": 8
      }
    },
    {
      "name": "MagicSparks",
      "_comment": "작은 마법 불꽃 — 고속 확산 후 빠르게 감속",
      "spawn": { "mode": "burst", "burstCount": 60 },
      "init": {
        "lifetimeMin": 0.2, "lifetimeMax": 0.8,
        "sizeMin": 1, "sizeMax": 4,
        "velocityMin": { "x": -600, "y": -600, "z": -300 },
        "velocityMax": { "x": 600, "y": 600, "z": 800 },
        "color": { "r": 2, "g": 1, "b": 5 }
      },
      "update": {
        "gravity": { "x": 0, "y": 0, "z": -500 },
        "drag": 4,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "spark",
        "alignment": "velocity_aligned",
        "blendMode": "additive",
        "sortOrder": 7
      }
    },
    {
      "name": "EnergyTrails",
      "_comment": "잔류 에너지 리본 궤적",
      "spawn": { "mode": "burst", "burstCount": 15 },
      "init": {
        "lifetimeMin": 0.3, "lifetimeMax": 0.8,
        "velocityMin": { "x": -300, "y": -300, "z": -100 },
        "velocityMax": { "x": 300, "y": 300, "z": 400 },
        "color": { "r": 1, "g": 0.3, "b": 3 }
      },
      "update": {
        "drag": 3,
        "opacityEnd": 0
      },
      "render": {
        "emitterTemplate": "ribbon",
        "blendMode": "additive",
        "ribbonWidth": 5,
        "sortOrder": 9
      }
    },
    {
      "name": "MagicSmoke",
      "_comment": "마법 연기 — 보라색 연무",
      "spawn": { "mode": "burst", "burstCount": 8, "burstDelay": 0.03 },
      "init": {
        "lifetimeMin": 0.8, "lifetimeMax": 2,
        "sizeMin": 20, "sizeMax": 50,
        "velocityMin": { "x": -40, "y": -40, "z": 10 },
        "velocityMax": { "x": 40, "y": 40, "z": 40 },
        "color": { "r": 0.15, "g": 0.05, "b": 0.2, "a": 0.4 }
      },
      "update": {
        "drag": 2,
        "sizeScaleEnd": 3,
        "opacityEnd": 0,
        "noiseStrength": 30, "noiseFrequency": 2
      },
      "render": {
        "emitterTemplate": "smoke",
        "sortOrder": 1
      }
    },
    {
      "name": "ImpactLight",
      "spawn": { "mode": "burst", "burstCount": 1 },
      "init": {
        "lifetimeMin": 0.1, "lifetimeMax": 0.4,
        "color": { "r": 2, "g": 0.8, "b": 5 }
      },
      "render": {
        "rendererType": "light",
        "lightRadiusScale": 6,
        "lightIntensity": 4
      }
    }
  ]
}
```

**포인트:**
- Shockwave: `sizeScaleEnd: 30`으로 극적인 확산 링
- EnergyBurst에 vortex 추가로 마법 특유의 나선 확산
- `ribbon`으로 에너지 잔류 궤적
- 보라색 마법 연기 (r:0.15, g:0.05, b:0.2)

---

## 템플릿 커버리지 요약

| 리치 템플릿 | 기존 예제 | 이 문서 |
|-------------|-----------|---------|
| spark | O | O |
| spark_secondary | X | O (AAA Explosion) |
| spark_debris | X | O (Bullet Impact) |
| smoke | O | O |
| explosion | O | O |
| core | O | O |
| debris | O | O (AAA Explosion) |
| dust | X | O (Bullet Impact) |
| ground_dust | X | O (AAA Explosion, Healing, Campfire) |
| impact | X | O (Bullet Impact) |
| impact_mesh | X | O (Bullet Impact) |
| muzzle_flash | X | O (Muzzle Flash) |
| arc | X | O (Electric Arc) |

| 빌트인 템플릿 | 기존 예제 | 이 문서 |
|---------------|-----------|---------|
| ribbon | X | O (Sword Slash, Healing, Magic Impact) |
| upward_mesh_burst | X | O (AAA Explosion) |
| directional_burst | O | O (Muzzle Flash) |

| 머티리얼 오버라이드 | 기존 예제 | 이 문서 |
|--------------------|-----------|---------|
| MI_Flames | O | O |
| MI_FireRoil_8x8 | X | O (Campfire) |
| MI_FireBall_8x8 | X | O (Campfire) |
| MI_SmokeRoil_8x8 | X | O (Campfire) |
| MI_Distortion | X | O (Sword Slash, Campfire) |

---

## 알려진 빌더 제약 사항

아래 제약이 해소되면 이 문서의 Config들이 더 풍부하게 렌더링됩니다.

### [BUILDER_LIMIT] Min/Max 평균 압축
현재 빌더는 `(min + max) / 2`로 단일 값을 설정합니다.
- `sizeMin:2, sizeMax:8` → 모든 파티클이 size=5
- `velocityMin/Max` → 모든 파티클이 동일 방향/속도
- `rotationRateMin:-300, rotationRateMax:300` → 평균=0이므로 회전 없음

**해결 방향**: Niagara `InitializeParticle` 모듈의 Uniform Random Range 파라미터를 활용하여 Min/Max를 각각 설정

### [BUILDER_LIMIT] 커브 미지원
`opacityEnd`, `sizeScaleEnd` 등이 단일 endpoint 값만 지원합니다. 비선형 커브(ease-in/out, pulse 등)를 표현할 수 없습니다.

### [BUILDER_LIMIT] SubUV 애니메이션 미제어
flipbook 머티리얼(MI_SmokePuff_8x8 등) 사용 시 재생 속도, 시작 프레임 랜덤화를 제어할 수 없습니다. 리치 템플릿 사용 시에는 템플릿에 내장된 설정이 적용됩니다.
