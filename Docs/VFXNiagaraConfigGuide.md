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
| lifetimeMin/Max | float | Particle lifetime (seconds) |
| sizeMin/Max | float | Particle size (UE units) |
| spriteRotationMin/Max | float | Initial rotation (degrees) |
| massMin/Max | float | Mass (force calculation) |
| velocityMin/Max | {x,y,z} | Initial velocity range |
| color | {r,g,b,a} | Particle color (HDR: r/g/b > 1.0 for glow) |

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
| lightRadiusScale | float | Light renderer radius |
| lightIntensity | float | Light renderer intensity |
| ribbonWidth | float | Ribbon renderer width |

## Settings

Project Settings > Plugins > Hkt VFX Generator:
- **Emitter Templates**: per renderer type (sprite, ribbon, light, mesh)
- **Fallback Template**: when no specific template found
- **Materials**: Additive/Translucent master materials (optional)
- **Default Output Directory**: where generated assets are saved
