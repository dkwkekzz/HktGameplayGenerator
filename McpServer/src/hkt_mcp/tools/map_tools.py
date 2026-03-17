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


def _validate_vec3(arr: Any, path: str, errors: list[str]) -> None:
    """Validate a 3-element numeric array (position/rotation/extent)."""
    if not isinstance(arr, list):
        errors.append(f"{path}: expected array, got {type(arr).__name__}")
        return
    if len(arr) != 3:
        errors.append(f"{path}: expected 3 elements [X,Y,Z], got {len(arr)}")
        return
    for k, v in enumerate(arr):
        if not isinstance(v, (int, float)):
            errors.append(f"{path}[{k}]: expected number, got {type(v).__name__}")
        elif math.isnan(v) or math.isinf(v):
            errors.append(f"{path}[{k}]: NaN/Inf not allowed")


def _validate_vec_n(arr: Any, expected: int, path: str, errors: list[str]) -> None:
    """Validate an N-element numeric array."""
    if not isinstance(arr, list):
        errors.append(f"{path}: expected array, got {type(arr).__name__}")
        return
    if len(arr) != expected:
        errors.append(f"{path}: expected {expected} elements, got {len(arr)}")
        return
    for k, v in enumerate(arr):
        if not isinstance(v, (int, float)):
            errors.append(f"{path}[{k}]: expected number, got {type(v).__name__}")
        elif math.isnan(v) or math.isinf(v):
            errors.append(f"{path}[{k}]: NaN/Inf not allowed")


async def validate_map(map_json: str) -> str:
    """Validate an HktMap JSON against the schema with comprehensive bounds checking."""
    try:
        data = json.loads(map_json)
    except json.JSONDecodeError as e:
        return json.dumps({"valid": False, "error": f"Invalid JSON: {e}"})

    errors: list[str] = []
    warnings: list[str] = []

    for field in ["map_id", "map_name", "regions"]:
        if field not in data:
            errors.append(f"Missing required field: {field}")

    regions = data.get("regions", [])
    if not regions:
        errors.append("No regions defined")
    elif len(regions) > _MAX_REGIONS:
        errors.append(f"Too many regions: {len(regions)} (max {_MAX_REGIONS})")

    region_names: set[str] = set()
    total_spawners = 0
    total_stories = 0
    total_props = 0

    for i, region in enumerate(regions):
        prefix = f"regions[{i}]"
        name = region.get("name")
        if not name:
            errors.append(f"{prefix}: missing name")
        elif name in region_names:
            errors.append(f"{prefix}: duplicate name '{name}'")
        else:
            region_names.add(name)

        # Bounds validation
        bounds = region.get("bounds")
        if not bounds:
            errors.append(f"{prefix}: missing bounds")
        else:
            if "center" in bounds:
                _validate_vec3(bounds["center"], f"{prefix}.bounds.center", errors)
            else:
                errors.append(f"{prefix}.bounds: missing center")
            if "extent" in bounds:
                _validate_vec3(bounds["extent"], f"{prefix}.bounds.extent", errors)
                ext = bounds["extent"]
                if isinstance(ext, list) and len(ext) == 3:
                    for k, v in enumerate(ext):
                        if isinstance(v, (int, float)) and v <= 0:
                            errors.append(f"{prefix}.bounds.extent[{k}]: must be positive, got {v}")
            else:
                errors.append(f"{prefix}.bounds: missing extent")

        # Landscape validation
        landscape = region.get("landscape")
        if not landscape:
            errors.append(f"{prefix}: missing landscape")
        else:
            ls_prefix = f"{prefix}.landscape"
            for dim in ("size_x", "size_y"):
                sz = landscape.get(dim)
                if sz is not None:
                    if not isinstance(sz, int) or sz <= 0:
                        errors.append(f"{ls_prefix}.{dim}: must be positive integer")
                    elif sz > _MAX_LANDSCAPE_SIZE:
                        errors.append(f"{ls_prefix}.{dim}: {sz} exceeds max {_MAX_LANDSCAPE_SIZE}")
                    elif sz not in _VALID_LANDSCAPE_SIZES:
                        warnings.append(
                            f"{ls_prefix}.{dim}: {sz} is not a standard UE5 landscape size "
                            f"(valid: {sorted(_VALID_LANDSCAPE_SIZES)})"
                        )

            # Terrain recipe validation
            recipe = landscape.get("terrain_recipe")
            if recipe:
                r_prefix = f"{ls_prefix}.terrain_recipe"
                octaves = recipe.get("octaves")
                if octaves is not None and (not isinstance(octaves, int) or octaves < 1 or octaves > _MAX_OCTAVES):
                    errors.append(f"{r_prefix}.octaves: must be 1-{_MAX_OCTAVES}, got {octaves}")

                erosion = recipe.get("erosion_passes")
                if erosion is not None and (not isinstance(erosion, int) or erosion < 0 or erosion > _MAX_EROSION_PASSES):
                    errors.append(f"{r_prefix}.erosion_passes: must be 0-{_MAX_EROSION_PASSES}, got {erosion}")

                freq = recipe.get("frequency")
                if freq is not None:
                    if not isinstance(freq, (int, float)):
                        errors.append(f"{r_prefix}.frequency: must be a number")
                    elif math.isnan(freq) or math.isinf(freq) or freq <= 0:
                        errors.append(f"{r_prefix}.frequency: must be a positive finite number")

                persistence = recipe.get("persistence")
                if persistence is not None:
                    if not isinstance(persistence, (int, float)) or persistence <= 0 or persistence > 1:
                        errors.append(f"{r_prefix}.persistence: must be (0, 1], got {persistence}")

                noise_type = recipe.get("base_noise_type", "perlin")
                valid_noise = {"perlin", "simplex", "ridged", "billow"}
                if isinstance(noise_type, str) and noise_type.lower() not in valid_noise:
                    errors.append(f"{r_prefix}.base_noise_type: '{noise_type}' not in {valid_noise}")

                # Features validation
                features = recipe.get("features", [])
                if len(features) > _MAX_FEATURES:
                    errors.append(f"{r_prefix}.features: {len(features)} exceeds max {_MAX_FEATURES}")
                valid_ftypes = {"mountain", "ridge", "valley", "plateau", "crater", "river_bed"}
                for fi, feat in enumerate(features[:_MAX_FEATURES]):
                    fp = f"{r_prefix}.features[{fi}]"
                    ftype = feat.get("type")
                    if not ftype:
                        errors.append(f"{fp}: missing type")
                    elif ftype.lower() not in valid_ftypes:
                        errors.append(f"{fp}.type: '{ftype}' not in {valid_ftypes}")

                    fpos = feat.get("position")
                    if fpos is None:
                        errors.append(f"{fp}: missing position")
                    else:
                        _validate_vec_n(fpos, 2, f"{fp}.position", errors)

                    radius = feat.get("radius")
                    if radius is not None:
                        if not isinstance(radius, (int, float)) or radius < 0.01 or radius > 1.0:
                            errors.append(f"{fp}.radius: must be [0.01, 1.0], got {radius}")

                    intensity = feat.get("intensity")
                    if intensity is not None:
                        if not isinstance(intensity, (int, float)) or abs(intensity) > 1.0:
                            errors.append(f"{fp}.intensity: must be [-1.0, 1.0], got {intensity}")

            # Height range validation
            hr = landscape.get("height_range")
            if hr:
                hmin = hr.get("min")
                hmax = hr.get("max")
                if hmin is not None and hmax is not None:
                    if isinstance(hmin, (int, float)) and isinstance(hmax, (int, float)):
                        if hmin >= hmax:
                            errors.append(f"{ls_prefix}.height_range: min ({hmin}) must be < max ({hmax})")

        # Spawner validation
        spawners = region.get("spawners", [])
        if len(spawners) > _MAX_SPAWNERS_PER_REGION:
            errors.append(f"{prefix}.spawners: {len(spawners)} exceeds max {_MAX_SPAWNERS_PER_REGION}")
        total_spawners += len(spawners)
        valid_spawn_rules = {"always", "on_story_start", "on_trigger", "timed"}
        for j, spawner in enumerate(spawners[:_MAX_SPAWNERS_PER_REGION]):
            sp = f"{prefix}.spawners[{j}]"
            if "entity_tag" not in spawner:
                errors.append(f"{sp}: missing entity_tag")
            pos = spawner.get("position")
            if pos is None:
                errors.append(f"{sp}: missing position")
            else:
                _validate_vec3(pos, f"{sp}.position", errors)
            rot = spawner.get("rotation")
            if rot is not None:
                _validate_vec3(rot, f"{sp}.rotation", errors)
            rule = spawner.get("spawn_rule")
            if rule is not None and rule.lower() not in valid_spawn_rules:
                errors.append(f"{sp}.spawn_rule: '{rule}' not in {valid_spawn_rules}")
            count = spawner.get("count")
            if count is not None and (not isinstance(count, int) or count < 1):
                errors.append(f"{sp}.count: must be positive integer, got {count}")

        # Story validation
        stories = region.get("stories", [])
        total_stories += len(stories)
        for j, story in enumerate(stories):
            if "story_tag" not in story:
                errors.append(f"{prefix}.stories[{j}]: missing story_tag")

        # Props validation
        props = region.get("props", [])
        total_props += len(props)
        for j, prop in enumerate(props):
            pp = f"{prefix}.props[{j}]"
            pos = prop.get("position")
            if pos is not None:
                _validate_vec3(pos, f"{pp}.position", errors)
            rot = prop.get("rotation")
            if rot is not None:
                _validate_vec3(rot, f"{pp}.rotation", errors)
            scale = prop.get("scale")
            if scale is not None:
                _validate_vec3(scale, f"{pp}.scale", errors)

    # Validate global entities
    global_entities = data.get("global_entities", [])
    if len(global_entities) > _MAX_GLOBAL_ENTITIES:
        errors.append(f"global_entities: {len(global_entities)} exceeds max {_MAX_GLOBAL_ENTITIES}")
    valid_entity_types = {"world_boss", "npc", "npc_spawner"}
    for i, ge in enumerate(global_entities[:_MAX_GLOBAL_ENTITIES]):
        gp = f"global_entities[{i}]"
        if "entity_tag" not in ge:
            errors.append(f"{gp}: missing entity_tag")
        etype = ge.get("entity_type")
        if etype is not None and etype.lower() not in valid_entity_types:
            errors.append(f"{gp}.entity_type: '{etype}' not in {valid_entity_types}")
        pos = ge.get("position")
        if pos is None:
            errors.append(f"{gp}: missing position")
        else:
            _validate_vec3(pos, f"{gp}.position", errors)
        rot = ge.get("rotation")
        if rot is not None:
            _validate_vec3(rot, f"{gp}.rotation", errors)

    # Global stories
    global_stories = data.get("global_stories", [])
    total_stories += len(global_stories)
    for i, story in enumerate(global_stories):
        if "story_tag" not in story:
            errors.append(f"global_stories[{i}]: missing story_tag")

    # Environment validation
    env = data.get("environment")
    if env:
        wind_dir = env.get("wind_direction")
        if wind_dir is not None:
            _validate_vec3(wind_dir, "environment.wind_direction", errors)
        ambient = env.get("ambient_color")
        if ambient is not None:
            _validate_vec_n(ambient, 4, "environment.ambient_color", errors)
        sun = env.get("sun_color")
        if sun is not None:
            _validate_vec_n(sun, 4, "environment.sun_color", errors)
        fog = env.get("fog_density")
        if fog is not None:
            if not isinstance(fog, (int, float)) or fog < 0 or fog > 1:
                errors.append(f"environment.fog_density: must be [0, 1], got {fog}")
        wind_str = env.get("wind_strength")
        if wind_str is not None:
            if not isinstance(wind_str, (int, float)) or wind_str < 0 or wind_str > 1:
                errors.append(f"environment.wind_strength: must be [0, 1], got {wind_str}")

    result: dict[str, Any] = {
        "valid": len(errors) == 0,
        "errors": errors,
        "region_count": len(regions),
        "spawner_count": total_spawners,
        "story_count": total_stories,
        "prop_count": total_props,
        "global_entity_count": len(global_entities),
    }
    if warnings:
        result["warnings"] = warnings

    return json.dumps(result, ensure_ascii=False, indent=2)


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

# Limits matching C++ constants
_MAX_LANDSCAPE_SIZE = 8129
_MAX_EROSION_PASSES = 20
_MAX_OCTAVES = 8
_MAX_FEATURES = 256
_MAX_REGIONS = 64
_MAX_SPAWNERS_PER_REGION = 500
_MAX_GLOBAL_ENTITIES = 100
_VALID_LANDSCAPE_SIZES = {127, 253, 505, 1009, 2017, 4033, 8129}


def _perlin_noise_2d(x: float, y: float, seed: int = 0) -> float:
    """
    Perlin-like noise matching C++ implementation.
    Uses same hash and gradient approach for consistent results.
    """
    import math as _m

    x0 = _m.floor(x)
    y0 = _m.floor(y)
    fx = x - x0
    fy = y - y0

    # Fade (Hermite smoothing) — same as C++
    u = fx * fx * fx * (fx * (fx * 6.0 - 15.0) + 10.0)
    v = fy * fy * fy * (fy * (fy * 6.0 - 15.0) + 10.0)

    def _hash(ix: int, iy: int) -> int:
        h = (ix * 73856093) ^ (iy * 19349663) ^ (seed * 83492791)
        h = (h ^ (h >> 13)) * 1274126177
        return h

    inv_sqrt2 = 0.70710678

    def _grad(h: int, dx: float, dy: float) -> float:
        g = h & 7
        if g == 0: return dx
        if g == 1: return dy
        if g == 2: return -dx
        if g == 3: return -dy
        if g == 4: return (dx + dy) * inv_sqrt2
        if g == 5: return (dx - dy) * inv_sqrt2
        if g == 6: return (-dx + dy) * inv_sqrt2
        return (-dx - dy) * inv_sqrt2

    ix0, iy0 = int(x0), int(y0)
    n00 = _grad(_hash(ix0, iy0), fx, fy)
    n10 = _grad(_hash(ix0 + 1, iy0), fx - 1, fy)
    n01 = _grad(_hash(ix0, iy0 + 1), fx, fy - 1)
    n11 = _grad(_hash(ix0 + 1, iy0 + 1), fx - 1, fy - 1)

    nx0 = n00 + u * (n10 - n00)
    nx1 = n01 + u * (n11 - n01)
    return nx0 + v * (nx1 - nx0)


def _fbm(x: float, y: float, octaves: int, frequency: float,
         lacunarity: float, persistence: float, seed: int,
         noise_type: str = "perlin") -> float:
    """Fractional Brownian Motion — matches C++ FBM implementation."""
    value = 0.0
    amp = 1.0
    freq = frequency
    max_amp = 0.0
    clamped_octaves = min(max(octaves, 1), _MAX_OCTAVES)
    persistence = max(0.01, min(persistence, 1.0))
    lacunarity = max(1.0, lacunarity)

    for i in range(clamped_octaves):
        sx = x * freq
        sy = y * freq
        n = _perlin_noise_2d(sx, sy, seed + i)

        if noise_type == "ridged":
            n = 1.0 - abs(n)
        elif noise_type == "billow":
            n = abs(n)

        value += n * amp
        max_amp += amp
        amp *= persistence
        freq *= lacunarity

    return value / max_amp if max_amp > 0 else 0.0


def _compute_falloff(t: float, falloff_type: str) -> float:
    ft = falloff_type.lower()
    if ft == "linear":
        return 1.0 - t
    if ft == "sharp":
        return 1.0 - t * t * t
    # smooth (smoothstep)
    return 1.0 - (3 * t * t - 2 * t * t * t)


def _apply_feature(height: float, px: float, py: float, feature: dict) -> float:
    """Apply a terrain feature — matches C++ ApplyFeature type-specific logic."""
    pos = feature.get("position", [0.5, 0.5])
    fx = max(0.0, min(1.0, pos[0] if len(pos) >= 1 else 0.5))
    fy = max(0.0, min(1.0, pos[1] if len(pos) >= 2 else 0.5))
    radius = max(0.01, min(1.0, feature.get("radius", 0.1)))
    intensity = max(-1.0, min(1.0, feature.get("intensity", 0.5)))
    falloff = feature.get("falloff", "smooth")
    ftype = feature.get("type", "mountain").lower()

    dist = math.sqrt((px - fx) ** 2 + (py - fy) ** 2)
    if dist >= radius:
        return height

    t = dist / radius
    weight = _compute_falloff(t, falloff)
    ai = abs(intensity)

    if ftype == "mountain":
        return height + ai * weight
    elif ftype == "valley":
        return height - ai * weight
    elif ftype == "ridge":
        return height + ai * math.sqrt(weight)
    elif ftype == "plateau":
        pw = 1.0 if t < 0.5 else _compute_falloff((t - 0.5) * 2.0, falloff)
        return height + ai * pw
    elif ftype == "crater":
        if t < 0.7:
            return height - ai * (1.0 - t / 0.7)
        rim_t = abs(t - 0.7) / 0.3
        return height + ai * 0.3 * (1.0 - min(rim_t, 1.0))
    elif ftype == "river_bed":
        return height - ai * weight * weight
    else:
        return height + intensity * weight


async def generate_terrain_preview(terrain_recipe_json: str, width: int = 60, height: int = 30) -> str:
    """
    Generate an ASCII heightmap preview from a terrain recipe.
    Uses the same noise/feature algorithms as C++ for accurate preview.
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
    noise_type = recipe.get("base_noise_type", "perlin").lower()
    features = recipe.get("features", [])[:_MAX_FEATURES]

    width = max(10, min(width, 120))
    height = max(5, min(height, 60))

    # Generate heightmap — same scaling as C++ (NormX * 1000)
    heightmap: list[list[float]] = []
    for row in range(height):
        hrow: list[float] = []
        py = row / height
        for col in range(width):
            px = col / width
            h = _fbm(px * 1000, py * 1000, octaves, frequency,
                      lacunarity, persistence, seed, noise_type)
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
        pos = feat.get("position", [0.5, 0.5])
        fx = pos[0] if len(pos) >= 1 else 0.5
        fy = pos[1] if len(pos) >= 2 else 0.5
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
