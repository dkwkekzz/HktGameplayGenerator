"""
MCP tool functions for HktMap generation.

HktMap is a JSON-based map definition that can be dynamically loaded/unloaded.
Each Region owns an independent Landscape, Spawners, Stories, and Props,
enabling per-region streaming. GlobalEntities and Environment apply map-wide.
"""

from __future__ import annotations

import json
import logging
import math
import os
from pathlib import Path
from typing import Any

logger = logging.getLogger(__name__)


def _get_maps_dir() -> Path:
    base = os.environ.get("HKT_MAPS_DIR", "")
    if base:
        return Path(base)
    return Path.cwd() / ".hkt_maps"


# ── HktMap Schema ────────────────────────────────────────────────────

_TERRAIN_FEATURE_SCHEMA: dict[str, Any] = {
    "type": "object",
    "required": ["type", "position", "radius", "intensity"],
    "properties": {
        "type": {
            "type": "string",
            "enum": ["mountain", "ridge", "valley", "plateau", "crater", "river_bed"],
            "description": "Terrain feature type",
        },
        "position": {
            "type": "array",
            "items": {"type": "number"},
            "minItems": 2,
            "maxItems": 2,
            "description": "Normalized [0,1] position on landscape [x, y]",
        },
        "radius": {
            "type": "number",
            "minimum": 0.01,
            "maximum": 1.0,
            "description": "Normalized radius [0,1]",
        },
        "intensity": {
            "type": "number",
            "minimum": -1.0,
            "maximum": 1.0,
            "description": "Height influence. Positive=elevation, negative=depression",
        },
        "falloff": {
            "type": "string",
            "enum": ["linear", "smooth", "sharp"],
            "default": "smooth",
        },
    },
}

_TERRAIN_RECIPE_SCHEMA: dict[str, Any] = {
    "type": "object",
    "description": "Procedural heightmap generation parameters (used when heightmap_path is empty)",
    "properties": {
        "base_noise_type": {
            "type": "string",
            "enum": ["perlin", "simplex", "ridged", "billow"],
            "default": "perlin",
        },
        "octaves": {"type": "integer", "minimum": 1, "maximum": 8, "default": 4},
        "frequency": {"type": "number", "default": 0.002},
        "lacunarity": {"type": "number", "default": 2.0},
        "persistence": {"type": "number", "default": 0.5},
        "seed": {"type": "integer", "default": 0},
        "features": {
            "type": "array",
            "items": _TERRAIN_FEATURE_SCHEMA,
            "description": "Terrain features (mountains, valleys, etc.) placed by LLM",
        },
        "erosion_passes": {"type": "integer", "minimum": 0, "default": 0},
    },
}

_LANDSCAPE_SCHEMA: dict[str, Any] = {
    "type": "object",
    "description": "Per-region landscape configuration",
    "properties": {
        "size_x": {"type": "integer", "description": "Landscape size X (UE5 valid: 2017, 4033, 8129)"},
        "size_y": {"type": "integer", "description": "Landscape size Y"},
        "heightmap_path": {"type": "string", "description": "Direct heightmap file (.r16/.png). If empty, terrain_recipe is used."},
        "material_tag": {"type": "string", "description": "GameplayTag for landscape material"},
        "biome": {"type": "string", "description": "Biome type (forest, desert, snow, volcanic, dark_forest, etc.)"},
        "height_range": {
            "type": "object",
            "properties": {"min": {"type": "number"}, "max": {"type": "number"}},
        },
        "terrain_recipe": _TERRAIN_RECIPE_SCHEMA,
        "layers": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "name": {"type": "string"},
                    "material_tag": {"type": "string"},
                    "weight_map": {"type": "string"},
                },
            },
            "description": "Landscape paint layers",
        },
    },
}

_SPAWNER_SCHEMA: dict[str, Any] = {
    "type": "object",
    "required": ["entity_tag", "position"],
    "properties": {
        "entity_tag": {"type": "string", "description": "GameplayTag of entity to spawn"},
        "position": {"type": "array", "items": {"type": "number"}, "description": "[X, Y, Z]"},
        "rotation": {"type": "array", "items": {"type": "number"}, "description": "[Pitch, Yaw, Roll]"},
        "spawn_rule": {"type": "string", "enum": ["always", "on_story_start", "on_trigger", "timed"]},
        "count": {"type": "integer", "default": 1},
        "respawn_seconds": {"type": "number", "default": 0},
    },
}

_STORY_REF_SCHEMA: dict[str, Any] = {
    "type": "object",
    "required": ["story_tag"],
    "properties": {
        "story_tag": {"type": "string", "description": "GameplayTag of linked story"},
        "auto_load": {"type": "boolean", "default": True, "description": "Load story when region activates"},
    },
}

_PROP_SCHEMA: dict[str, Any] = {
    "type": "object",
    "properties": {
        "mesh_tag": {"type": "string"},
        "position": {"type": "array", "items": {"type": "number"}},
        "rotation": {"type": "array", "items": {"type": "number"}},
        "scale": {"type": "array", "items": {"type": "number"}},
    },
}

_REGION_SCHEMA: dict[str, Any] = {
    "type": "object",
    "required": ["name", "bounds", "landscape"],
    "properties": {
        "name": {"type": "string", "description": "Unique region name"},
        "bounds": {
            "type": "object",
            "required": ["center", "extent"],
            "properties": {
                "center": {"type": "array", "items": {"type": "number"}, "description": "[X, Y, Z]"},
                "extent": {"type": "array", "items": {"type": "number"}, "description": "[X, Y, Z] half-extent"},
            },
        },
        "properties": {"type": "object", "description": "Custom properties (difficulty, theme, etc.)"},
        "landscape": _LANDSCAPE_SCHEMA,
        "spawners": {"type": "array", "items": _SPAWNER_SCHEMA},
        "stories": {"type": "array", "items": _STORY_REF_SCHEMA},
        "props": {"type": "array", "items": _PROP_SCHEMA},
    },
}

_GLOBAL_ENTITY_SCHEMA: dict[str, Any] = {
    "type": "object",
    "required": ["entity_tag", "entity_type", "position"],
    "properties": {
        "entity_tag": {"type": "string", "description": "GameplayTag of the entity"},
        "entity_type": {
            "type": "string",
            "enum": ["world_boss", "npc", "npc_spawner"],
            "description": "world_boss: region-independent boss, npc: always-visible NPC, npc_spawner: spawns multiple NPCs",
        },
        "position": {"type": "array", "items": {"type": "number"}, "description": "[X, Y, Z]"},
        "rotation": {"type": "array", "items": {"type": "number"}, "description": "[Pitch, Yaw, Roll]"},
        "count": {"type": "integer", "default": 1, "description": "Spawn count (for npc_spawner)"},
        "properties": {"type": "object", "description": "Custom props (dialogue_set, patrol_radius, level, etc.)"},
    },
}

_ENVIRONMENT_SCHEMA: dict[str, Any] = {
    "type": "object",
    "description": "Map-wide environment settings",
    "properties": {
        "weather": {"type": "string", "enum": ["clear", "rain", "snow", "fog", "storm"], "default": "clear"},
        "time_of_day": {"type": "string", "enum": ["dawn", "morning", "noon", "afternoon", "dusk", "night"], "default": "noon"},
        "fog_density": {"type": "number", "minimum": 0, "maximum": 1, "default": 0.02},
        "wind_direction": {"type": "array", "items": {"type": "number"}, "description": "[X, Y, Z] normalized"},
        "wind_strength": {"type": "number", "minimum": 0, "maximum": 1, "default": 0.5},
        "ambient_color": {"type": "array", "items": {"type": "number"}, "description": "[R, G, B, A]"},
        "sun_color": {"type": "array", "items": {"type": "number"}, "description": "[R, G, B, A]"},
        "ambient_vfx_tags": {"type": "array", "items": {"type": "string"}, "description": "VFX GameplayTags for ambient effects"},
    },
}

HKTMAP_SCHEMA: dict[str, Any] = {
    "type": "object",
    "required": ["map_id", "map_name", "regions"],
    "properties": {
        "map_id": {"type": "string", "description": "Unique identifier for this map"},
        "map_name": {"type": "string", "description": "Human-readable name"},
        "description": {"type": "string"},
        "regions": {
            "type": "array",
            "items": _REGION_SCHEMA,
            "description": "Regions — each with independent Landscape, Spawners, Stories, Props. Streamed per-region.",
        },
        "global_entities": {
            "type": "array",
            "items": _GLOBAL_ENTITY_SCHEMA,
            "description": "WorldBoss/NPC/NPCSpawner — always active regardless of region streaming",
        },
        "environment": _ENVIRONMENT_SCHEMA,
        "global_stories": {
            "type": "array",
            "items": _STORY_REF_SCHEMA,
            "description": "Stories loaded when map loads (not region-specific)",
        },
    },
}


# ── MCP Tool Functions ───────────────────────────────────────────────


async def get_map_schema() -> str:
    """Get the HktMap JSON schema for map generation."""
    return json.dumps({
        "schema": HKTMAP_SCHEMA,
        "description": (
            "HktMap is a JSON-based map definition with per-region streaming. "
            "Each Region owns an independent Landscape (created from terrain_recipe "
            "or heightmap), Spawners, Stories, and Props. Regions are loaded/unloaded "
            "independently based on player proximity. GlobalEntities (WorldBoss, NPC, "
            "NPCSpawner) and Environment settings apply map-wide."
        ),
        "architecture": (
            "Region-based streaming: each Region has its own ALandscape actor in UE5. "
            "When a player enters a region's bounds, its Landscape + content is loaded. "
            "When the player leaves, it is unloaded. GlobalEntities and GlobalStories "
            "are always active."
        ),
    }, ensure_ascii=False, indent=2)


async def validate_map(map_json: str) -> str:
    """Validate an HktMap JSON against the schema."""
    try:
        data = json.loads(map_json)
    except json.JSONDecodeError as e:
        return json.dumps({"valid": False, "error": f"Invalid JSON: {e}"})

    errors: list[str] = []
    for field in ["map_id", "map_name", "regions"]:
        if field not in data:
            errors.append(f"Missing required field: {field}")

    regions = data.get("regions", [])
    if not regions:
        errors.append("No regions defined")

    region_names: set[str] = set()
    total_spawners = 0
    total_stories = 0
    total_props = 0

    for i, region in enumerate(regions):
        name = region.get("name")
        if not name:
            errors.append(f"regions[{i}]: missing name")
        elif name in region_names:
            errors.append(f"regions[{i}]: duplicate name '{name}'")
        else:
            region_names.add(name)

        if "bounds" not in region:
            errors.append(f"regions[{i}]: missing bounds")

        if "landscape" not in region:
            errors.append(f"regions[{i}]: missing landscape")

        # Validate spawners
        spawners = region.get("spawners", [])
        total_spawners += len(spawners)
        for j, spawner in enumerate(spawners):
            if "entity_tag" not in spawner:
                errors.append(f"regions[{i}].spawners[{j}]: missing entity_tag")
            if "position" not in spawner:
                errors.append(f"regions[{i}].spawners[{j}]: missing position")

        # Validate stories
        stories = region.get("stories", [])
        total_stories += len(stories)
        for j, story in enumerate(stories):
            if "story_tag" not in story:
                errors.append(f"regions[{i}].stories[{j}]: missing story_tag")

        total_props += len(region.get("props", []))

    # Validate global entities
    global_entities = data.get("global_entities", [])
    for i, ge in enumerate(global_entities):
        if "entity_tag" not in ge:
            errors.append(f"global_entities[{i}]: missing entity_tag")
        if "position" not in ge:
            errors.append(f"global_entities[{i}]: missing position")

    # Global stories
    global_stories = data.get("global_stories", [])
    total_stories += len(global_stories)
    for i, story in enumerate(global_stories):
        if "story_tag" not in story:
            errors.append(f"global_stories[{i}]: missing story_tag")

    return json.dumps({
        "valid": len(errors) == 0,
        "errors": errors,
        "region_count": len(regions),
        "spawner_count": total_spawners,
        "story_count": total_stories,
        "prop_count": total_props,
        "global_entity_count": len(global_entities),
    }, ensure_ascii=False, indent=2)


async def save_map(map_json: str) -> str:
    """Save an HktMap JSON to the maps directory."""
    try:
        data = json.loads(map_json)
    except json.JSONDecodeError as e:
        return json.dumps({"success": False, "error": f"Invalid JSON: {e}"})

    map_id = data.get("map_id")
    if not map_id:
        return json.dumps({"success": False, "error": "Missing map_id"})

    maps_dir = _get_maps_dir()
    maps_dir.mkdir(parents=True, exist_ok=True)
    map_path = maps_dir / f"{map_id}.json"
    map_path.write_text(
        json.dumps(data, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    return json.dumps({
        "success": True,
        "map_id": map_id,
        "map_path": str(map_path),
    }, ensure_ascii=False)


async def load_map(map_id: str) -> str:
    """Load an HktMap JSON by ID."""
    maps_dir = _get_maps_dir()
    map_path = maps_dir / f"{map_id}.json"
    if not map_path.exists():
        return json.dumps({"error": f"Map not found: {map_id}"})
    data = json.loads(map_path.read_text(encoding="utf-8"))
    return json.dumps(data, ensure_ascii=False, indent=2)


async def list_maps() -> str:
    """List all saved HktMaps."""
    maps_dir = _get_maps_dir()
    if not maps_dir.exists():
        return json.dumps([])
    maps = []
    for f in sorted(maps_dir.glob("*.json")):
        try:
            data = json.loads(f.read_text(encoding="utf-8"))
            regions = data.get("regions", [])
            total_spawners = sum(len(r.get("spawners", [])) for r in regions)
            total_stories = sum(len(r.get("stories", [])) for r in regions) + len(data.get("global_stories", []))
            maps.append({
                "map_id": data.get("map_id", f.stem),
                "map_name": data.get("map_name", ""),
                "region_count": len(regions),
                "spawner_count": total_spawners,
                "story_count": total_stories,
                "global_entity_count": len(data.get("global_entities", [])),
                "file_path": str(f),
            })
        except (json.JSONDecodeError, OSError):
            pass
    return json.dumps(maps, ensure_ascii=False, indent=2)


async def delete_map(map_id: str) -> str:
    """Delete an HktMap by ID."""
    maps_dir = _get_maps_dir()
    map_path = maps_dir / f"{map_id}.json"
    if not map_path.exists():
        return json.dumps({"error": f"Map not found: {map_id}"})
    map_path.unlink()
    return json.dumps({"success": True, "deleted": map_id})


async def build_map(map_json: str, bridge: Any = None) -> str:
    """
    Build an HktMap in UE5 - per-region Landscape + Spawners + Stories.
    Requires a connected UE5 editor bridge.
    """
    if bridge is None:
        return json.dumps({
            "error": "No UE5 editor bridge connected. Save the map with save_map and build later."
        })

    try:
        data = json.loads(map_json)
    except json.JSONDecodeError as e:
        return json.dumps({"success": False, "error": f"Invalid JSON: {e}"})

    result = await bridge.call(
        "HktMapGenerator", "BuildMap", data
    )
    return json.dumps(result, ensure_ascii=False, indent=2)


# ── Terrain Preview ──────────────────────────────────────────────────


def _simple_noise_2d(x: float, y: float, seed: int = 0) -> float:
    """Simple hash-based pseudo noise for preview purposes."""
    n = int(x * 73856093) ^ int(y * 19349663) ^ (seed * 83492791)
    n = (n << 13) ^ n
    return 1.0 - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7FFFFFFF) / 1073741824.0


def _fbm(x: float, y: float, octaves: int, frequency: float,
         lacunarity: float, persistence: float, seed: int) -> float:
    """Fractional Brownian Motion using simple noise."""
    value = 0.0
    amp = 1.0
    freq = frequency
    for _ in range(octaves):
        value += amp * _simple_noise_2d(x * freq, y * freq, seed)
        amp *= persistence
        freq *= lacunarity
    return value


def _apply_feature(height: float, px: float, py: float, feature: dict) -> float:
    """Apply a terrain feature to a height value."""
    fx, fy = feature.get("position", [0.5, 0.5])
    radius = feature.get("radius", 0.1)
    intensity = feature.get("intensity", 0.5)
    falloff = feature.get("falloff", "smooth")

    dist = math.sqrt((px - fx) ** 2 + (py - fy) ** 2)
    if dist >= radius:
        return height

    t = dist / radius
    if falloff == "linear":
        weight = 1.0 - t
    elif falloff == "sharp":
        weight = 1.0 - t * t * t
    else:  # smooth (smoothstep)
        weight = 1.0 - (3 * t * t - 2 * t * t * t)

    ftype = feature.get("type", "mountain")
    if ftype in ("valley", "river_bed", "crater"):
        return height + intensity * weight  # intensity should be negative
    else:
        return height + intensity * weight


async def generate_terrain_preview(terrain_recipe_json: str, width: int = 60, height: int = 30) -> str:
    """
    Generate an ASCII heightmap preview from a terrain recipe.
    Useful for LLM agents to visually verify their terrain recipe before building.
    """
    try:
        recipe = json.loads(terrain_recipe_json)
    except json.JSONDecodeError as e:
        return json.dumps({"error": f"Invalid JSON: {e}"})

    octaves = recipe.get("octaves", 4)
    frequency = recipe.get("frequency", 0.002)
    lacunarity = recipe.get("lacunarity", 2.0)
    persistence = recipe.get("persistence", 0.5)
    seed = recipe.get("seed", 0)
    features = recipe.get("features", [])

    # Generate heightmap
    heightmap: list[list[float]] = []
    for row in range(height):
        hrow: list[float] = []
        py = row / height
        for col in range(width):
            px = col / width
            h = _fbm(px, py, octaves, frequency * 500, lacunarity, persistence, seed)
            for feat in features:
                h = _apply_feature(h, px, py, feat)
            hrow.append(h)
        heightmap.append(hrow)

    # Normalize
    flat = [v for row in heightmap for v in row]
    hmin, hmax = min(flat), max(flat)
    rng = hmax - hmin if hmax > hmin else 1.0

    # Render ASCII
    chars = " .:-=+*#%@"
    lines: list[str] = []
    for row in heightmap:
        line = ""
        for v in row:
            norm = (v - hmin) / rng
            idx = min(int(norm * (len(chars) - 1)), len(chars) - 1)
            line += chars[idx]
        lines.append(line)

    ascii_map = "\n".join(lines)

    # Feature legend
    legend_lines: list[str] = []
    for i, feat in enumerate(features):
        fx, fy = feat.get("position", [0.5, 0.5])
        col = int(fx * width)
        row = int(fy * height)
        legend_lines.append(
            f"  [{i}] {feat.get('type', '?')} at ({fx:.2f},{fy:.2f}) "
            f"r={feat.get('radius', 0):.2f} i={feat.get('intensity', 0):.2f} "
            f"→ ASCII pos ({col},{row})"
        )

    return json.dumps({
        "preview": ascii_map,
        "width": width,
        "height": height,
        "legend": "\n".join(legend_lines) if legend_lines else "No features",
    }, ensure_ascii=False, indent=2)
