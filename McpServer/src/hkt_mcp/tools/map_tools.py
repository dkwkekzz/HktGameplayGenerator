"""
MCP tool functions for HktMap generation.

HktMap is a JSON-based map definition that can be dynamically loaded/unloaded.
It contains landscape configuration, spawner placements, region definitions,
and story references. It is NOT a UMap - it's a lightweight data format that
the runtime uses to procedurally set up a game area.
"""

from __future__ import annotations

import json
import logging
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

HKTMAP_SCHEMA: dict[str, Any] = {
    "type": "object",
    "required": ["map_id", "map_name", "landscape", "stories"],
    "properties": {
        "map_id": {
            "type": "string",
            "description": "Unique identifier for this map",
        },
        "map_name": {
            "type": "string",
            "description": "Human-readable name",
        },
        "description": {
            "type": "string",
        },
        "landscape": {
            "type": "object",
            "description": "Landscape/terrain configuration",
            "properties": {
                "size_x": {"type": "integer", "description": "Landscape size X in units"},
                "size_y": {"type": "integer", "description": "Landscape size Y in units"},
                "heightmap_path": {
                    "type": "string",
                    "description": "Path to heightmap asset or generation params",
                },
                "material_tag": {
                    "type": "string",
                    "description": "GameplayTag for landscape material",
                },
                "biome": {
                    "type": "string",
                    "description": "Biome type (forest, desert, snow, volcanic, etc.)",
                },
                "height_range": {
                    "type": "object",
                    "properties": {
                        "min": {"type": "number"},
                        "max": {"type": "number"},
                    },
                },
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
        },
        "regions": {
            "type": "array",
            "items": {
                "type": "object",
                "required": ["name", "bounds"],
                "properties": {
                    "name": {"type": "string"},
                    "bounds": {
                        "type": "object",
                        "properties": {
                            "center": {"type": "array", "items": {"type": "number"}},
                            "extent": {"type": "array", "items": {"type": "number"}},
                        },
                    },
                    "properties": {
                        "type": "object",
                        "description": "Custom region properties (difficulty, theme, etc.)",
                    },
                },
            },
            "description": "Named regions within the map",
        },
        "spawners": {
            "type": "array",
            "items": {
                "type": "object",
                "required": ["entity_tag", "position"],
                "properties": {
                    "entity_tag": {
                        "type": "string",
                        "description": "GameplayTag of entity to spawn",
                    },
                    "position": {
                        "type": "array",
                        "items": {"type": "number"},
                        "description": "[X, Y, Z] world position",
                    },
                    "rotation": {
                        "type": "array",
                        "items": {"type": "number"},
                        "description": "[Pitch, Yaw, Roll]",
                    },
                    "spawn_rule": {
                        "type": "string",
                        "description": "Spawn condition (always, on_story_start, on_trigger, timed)",
                    },
                    "region": {
                        "type": "string",
                        "description": "Region this spawner belongs to",
                    },
                    "count": {
                        "type": "integer",
                        "description": "Number of entities to spawn",
                        "default": 1,
                    },
                    "respawn_seconds": {
                        "type": "number",
                        "description": "Respawn interval (0 = no respawn)",
                        "default": 0,
                    },
                },
            },
        },
        "stories": {
            "type": "array",
            "items": {
                "type": "object",
                "required": ["story_tag"],
                "properties": {
                    "story_tag": {
                        "type": "string",
                        "description": "GameplayTag of linked story",
                    },
                    "auto_load": {
                        "type": "boolean",
                        "description": "Load story when map loads",
                        "default": True,
                    },
                    "trigger_region": {
                        "type": "string",
                        "description": "Region that triggers story loading",
                    },
                },
            },
            "description": "Stories linked to this map",
        },
        "props": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "mesh_tag": {"type": "string"},
                    "position": {"type": "array", "items": {"type": "number"}},
                    "rotation": {"type": "array", "items": {"type": "number"}},
                    "scale": {"type": "array", "items": {"type": "number"}},
                },
            },
            "description": "Static prop placements",
        },
    },
}


# ── MCP Tool Functions ───────────────────────────────────────────────


async def get_map_schema() -> str:
    """Get the HktMap JSON schema for map generation."""
    return json.dumps({
        "schema": HKTMAP_SCHEMA,
        "description": (
            "HktMap is a JSON-based map definition for dynamic loading. "
            "It defines landscape, spawners, regions, and linked stories. "
            "When an HktMap is loaded at runtime, associated stories are "
            "also loaded and spawners are activated."
        ),
    }, ensure_ascii=False, indent=2)


async def validate_map(map_json: str) -> str:
    """Validate an HktMap JSON against the schema."""
    try:
        data = json.loads(map_json)
    except json.JSONDecodeError as e:
        return json.dumps({"valid": False, "error": f"Invalid JSON: {e}"})

    errors = []
    for field in ["map_id", "map_name", "landscape", "stories"]:
        if field not in data:
            errors.append(f"Missing required field: {field}")

    # Validate story references
    stories = data.get("stories", [])
    for i, story in enumerate(stories):
        if "story_tag" not in story:
            errors.append(f"stories[{i}]: missing story_tag")

    # Validate spawners
    spawners = data.get("spawners", [])
    for i, spawner in enumerate(spawners):
        if "entity_tag" not in spawner:
            errors.append(f"spawners[{i}]: missing entity_tag")
        if "position" not in spawner:
            errors.append(f"spawners[{i}]: missing position")

    # Validate regions
    regions = data.get("regions", [])
    region_names = set()
    for i, region in enumerate(regions):
        if "name" not in region:
            errors.append(f"regions[{i}]: missing name")
        else:
            region_names.add(region["name"])

    # Cross-reference: spawner regions must exist
    for i, spawner in enumerate(spawners):
        r = spawner.get("region")
        if r and r not in region_names:
            errors.append(f"spawners[{i}]: unknown region '{r}'")

    # Cross-reference: story trigger regions must exist
    for i, story in enumerate(stories):
        r = story.get("trigger_region")
        if r and r not in region_names:
            errors.append(f"stories[{i}]: unknown trigger_region '{r}'")

    return json.dumps({
        "valid": len(errors) == 0,
        "errors": errors,
        "story_count": len(stories),
        "spawner_count": len(spawners),
        "region_count": len(regions),
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
            maps.append({
                "map_id": data.get("map_id", f.stem),
                "map_name": data.get("map_name", ""),
                "story_count": len(data.get("stories", [])),
                "spawner_count": len(data.get("spawners", [])),
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
    Build an HktMap in UE5 - instantiate landscape, spawners, and load stories.
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

    # Delegate to bridge for actual UE5 construction
    result = await bridge.call(
        "HktMapGenerator", "BuildMap", data
    )
    return json.dumps(result, ensure_ascii=False, indent=2)
