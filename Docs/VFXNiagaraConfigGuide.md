# VFX Niagara Config Guide

## Full Pipeline

```
[1] User:  "Create a fire tornado VFX"
            ↓
[2] LLM:   Reads PromptGuide + ExampleConfigs
            ↓
[3] LLM:   Generates VFXConfig JSON (multi-emitter composition)
            ↓
[4] Call:   McpBuildNiagaraSystem(jsonConfig)
            ↓
[5] UE5:   HktVFXNiagaraBuilder → Niagara System .uasset saved
```

## Step-by-Step Workflow

### Step 1: LLM Reads Guide (one-time per session)

```python
# Design guide: schema + emitter layer patterns + value ranges + tips
guide = unreal.HktVFXGeneratorFunctionLibrary.mcp_get_vfx_prompt_guide()

# Example configs: Explosion, Magic Portal, Campfire
examples = unreal.HktVFXGeneratorFunctionLibrary.mcp_get_vfx_example_configs()
```

The guide teaches the LLM:
- **Emitter Layer Patterns**: CoreGlow, Sparks, Smoke, Shockwave, Trail, Light, Debris, Ambient
- **Value Ranges**: Size (1-200+), Velocity (50-1500), Lifetime (0.05-5+), Color RGB (0-10+)
- **Design Tips**: blend modes, velocity_aligned, burstDelay staggering, colorOverLife
- **JSON Schema**: full field reference

### Step 2: LLM Generates Config JSON

User says: "Create a fire tornado VFX"

LLM composes:
```json
{
  "systemName": "FireTornado",
  "warmupTime": 2,
  "looping": true,
  "emitters": [
    {"name": "Flames",  "spawn": {"mode": "rate", "rate": 30}, ...},
    {"name": "Embers",  "spawn": {"mode": "rate", "rate": 10}, ...},
    {"name": "Smoke",   "spawn": {"mode": "rate", "rate": 5},  ...},
    {"name": "Light",   "spawn": {"mode": "rate", "rate": 2},  ...}
  ]
}
```

### Step 3: Build via API

```python
import json

config = { ... }  # LLM-generated config
result = unreal.HktVFXGeneratorFunctionLibrary.mcp_build_niagara_system(
    json.dumps(config)
)
print(result)  # {"success": true, "assetPath": "/Game/.../NS_FireTornado"}
```

### Step 4: Verify

```python
assets = unreal.HktVFXGeneratorFunctionLibrary.mcp_list_generated_vfx()
print(assets)
```

## Quick Test (No LLM needed)

```python
# Built-in explosion preset with 5 emitter layers
result = unreal.HktVFXGeneratorFunctionLibrary.mcp_build_preset_explosion(
    "TestExplosion", 1.0, 0.5, 0.1, 0.8
)
```

## API Reference

| Function | Purpose |
|----------|---------|
| `McpGetVFXPromptGuide()` | Design guide + schema (LLM reads first) |
| `McpGetVFXExampleConfigs()` | Example configs (LLM learns patterns) |
| `McpGetVFXConfigSchema()` | JSON schema only |
| `McpBuildNiagaraSystem(json, outputDir?)` | Build from config JSON |
| `McpBuildPresetExplosion(name, r, g, b, intensity, outputDir?)` | Quick test preset |
| `McpListGeneratedVFX(directory?)` | List generated assets |

## Config Field Reference

### System Level
| Field | Type | Description |
|-------|------|-------------|
| systemName | string | Asset name (NS_ prefix auto-added) |
| warmupTime | float | Pre-warm seconds (0=none) |
| looping | bool | System loops (default false) |

### Emitter: spawn
| Field | Type | Description |
|-------|------|-------------|
| mode | string | `"burst"` or `"rate"` |
| rate | float | Particles per second (rate mode) |
| burstCount | int | Particles per burst (burst mode) |
| burstDelay | float | Delay before burst fires (seconds) |

### Emitter: init
| Field | Type | Description |
|-------|------|-------------|
| lifetimeMin/Max | float | Particle lifetime — random per particle |
| sizeMin/Max | float | Particle size — random per particle |
| spriteRotationMin/Max | float | Initial rotation (degrees) — random per particle |
| massMin/Max | float | Mass (force calculation) — random per particle |
| velocityMin/Max | {x,y,z} | Initial velocity range — random per particle |
| color | {r,g,b,a} | Particle color (HDR: r/g/b > 1.0 for glow) |

### Emitter: shapeLocation (Emission Shape)
| Field | Type | Description |
|-------|------|-------------|
| shape | string | `"sphere"`, `"box"`, `"cylinder"`, `"cone"`, `"ring"`, `"torus"`, `"plane"` |
| sphereRadius | float | Sphere radius (sphere only) |
| boxSize | {x,y,z} | Box extents (box only) |
| cylinderRadius/Height | float | Cylinder dimensions |
| coneAngle/Length | float | Cone angle (degrees) and length |
| ringRadius/Width | float | Ring dimensions |
| torusRadius/SectionRadius | float | Torus dimensions |
| offset | {x,y,z} | Shape offset from emitter origin |
| surfaceOnly | bool | true = spawn on surface only |

### Emitter: update
| Field | Type | Description |
|-------|------|-------------|
| gravity | {x,y,z} | Gravity force (default z=-980) |
| drag | float | Air resistance (0=none) |
| rotationRateMin/Max | float | Rotation speed (deg/sec) |
| sizeScaleStart/End | float | Size multiplier over life (1.0=no change) |
| opacityStart/End | float | Alpha over life (0-1) |
| useColorOverLife | bool | Enable color interpolation |
| colorEnd | {r,g,b,a} | End color (when useColorOverLife=true) |
| noiseStrength | float | Curl noise turbulence (0=none) |
| noiseFrequency | float | Noise scale |
| attractionStrength | float | Point attractor force (0=none) |
| attractionRadius | float | Attractor effective radius |
| attractionPosition | {x,y,z} | Attractor position |
| vortexStrength | float | Vortex/swirl force (0=none) |
| vortexRadius | float | Vortex radius |

### Emitter: render
| Field | Type | Description |
|-------|------|-------------|
| rendererType | string | `"sprite"`, `"ribbon"`, `"light"`, `"mesh"` |
| blendMode | string | `"additive"` (glow), `"translucent"` (alpha) |
| sortOrder | int | Render order (higher = on top) |
| alignment | string | `"unaligned"`, `"velocity_aligned"` |
| facingMode | string | `"default"`, `"velocity"`, `"camera_position"`, `"camera_plane"`, `"custom_axis"` |
| lightRadiusScale | float | Light renderer radius |
| lightIntensity | float | Light renderer intensity |
| ribbonWidth | float | Ribbon renderer width |

### Emitter: collision
| Field | Type | Description |
|-------|------|-------------|
| enabled | bool | Activate collision module |
| response | string | `"bounce"`, `"kill"`, `"stick"` |
| restitution | float | Bounciness 0-1 (bounce mode only) |
| friction | float | Surface friction 0-1 |
| traceDistance | float | GPU ray trace distance (0=default) |

### Emitter: eventSpawn
| Field | Type | Description |
|-------|------|-------------|
| triggerEvent | string | `"death"` or `"collision"` |
| spawnCount | int | Secondary particles per event |
| targetEmitter | string | Target emitter name in same system |
| velocityScale | float | Inherited velocity multiplier |

### Emitter: spawnPerUnit
| Field | Type | Description |
|-------|------|-------------|
| enabled | bool | Activate distance-based spawning |
| spawnPerUnit | float | Particles per distance unit |
| maxFrameSpawn | float | Max particles per frame |
| movementTolerance | float | Minimum movement threshold |

### Emitter-level
| Field | Type | Description |
|-------|------|-------------|
| gpuSim | bool | GPU simulation (large particle counts) |

### Rendering Quality (render extensions)
| Field | Type | Description |
|-------|------|-------------|
| subImageRows | int | Flipbook row count (0=disabled) |
| subImageColumns | int | Flipbook column count (0=disabled) |
| subUVPlayRate | float | SubUV animation speed multiplier (default 1.0) |
| bSubUVRandomStartFrame | bool | Random start frame per particle |
| bSoftParticle | bool | Soft edges at geometry intersection |
| softParticleFadeDistance | float | Depth fade distance in cm (default 100) |
| cameraOffset | float | Push sprite toward camera (prevents z-fighting) |
| ribbonUVMode | string | stretch / tile_distance / tile_lifetime / distribute |
| ribbonTessellation | int | Custom tessellation factor (0=auto) |
| ribbonWidthScaleStart | float | Ribbon width at start (default 1.0) |
| ribbonWidthScaleEnd | float | Ribbon width at end (default 1.0) |
| meshPath | string | Static mesh asset path (e.g. /Engine/BasicShapes/Cube.Cube) |
| meshOrientation | string | default / velocity / camera |
| lightExponent | float | Light falloff exponent (default 1.0) |
| bLightVolumetricScattering | bool | Affects volumetric fog |

### Multi-point Curves (update extensions)
| Field | Type | Description |
|-------|------|-------------|
| colorCurve | array | [{time:0-1, color:{r,g,b,a}}, ...] Multi-point color transition |
| sizeCurve | array | [{time:0-1, scale:float}, ...] Multi-point size animation |
| cameraDistanceFadeNear | float | Start fade-out distance from camera |
| cameraDistanceFadeFar | float | Fully invisible distance from camera |

### Multi-wave Burst (spawn extensions)
| Field | Type | Description |
|-------|------|-------------|
| burstWaves | array | [{count:int, delay:float}, ...] Multiple timed bursts |

## Settings

Project Settings > Plugins > Hkt VFX Generator:
- **Emitter Templates**: per renderer type (sprite, ribbon, light, mesh)
- **Fallback Template**: when no specific template found
- **Materials**: Additive/Translucent master materials (optional)
- **Default Output Directory**: where generated assets are saved
