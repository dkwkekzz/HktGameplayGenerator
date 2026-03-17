"""
Step data models for the modular generation pipeline.

Each step is independent with defined input/output schemas.
Steps can be run by different agents and communicate via JSON files.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from datetime import datetime, timezone
from enum import Enum
from typing import Any


class StepType(str, Enum):
    """Independent generation steps with clear input/output boundaries."""

    CONCEPT_DESIGN = "concept_design"
    """Input: user concept text → Output: terrain spec + story list"""

    MAP_GENERATION = "map_generation"
    """Input: concept_design output → Output: HktMap JSON"""

    STORY_GENERATION = "story_generation"
    """Input: concept_design output → Output: Story JSONs for HktCore"""

    ASSET_DISCOVERY = "asset_discovery"
    """Input: story JSONs → Output: asset specifications (character/item/vfx)"""

    CHARACTER_GENERATION = "character_generation"
    """Input: character specs from asset_discovery → Output: uasset paths"""

    ITEM_GENERATION = "item_generation"
    """Input: item specs from asset_discovery → Output: uasset paths"""

    VFX_GENERATION = "vfx_generation"
    """Input: vfx specs from asset_discovery → Output: uasset paths"""


class StepStatus(str, Enum):
    NOT_STARTED = "not_started"
    IN_PROGRESS = "in_progress"
    COMPLETED = "completed"
    FAILED = "failed"


def _now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


# ── Input/Output Schema Definitions ──────────────────────────────────

STEP_SCHEMAS: dict[str, dict[str, Any]] = {
    StepType.CONCEPT_DESIGN: {
        "input": {
            "type": "object",
            "required": ["concept"],
            "properties": {
                "concept": {
                    "type": "string",
                    "description": "User's initial concept text describing the desired gameplay scenario",
                },
                "existing_maps": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "List of existing HktMap IDs that can be reused",
                },
            },
        },
        "output": {
            "type": "object",
            "required": ["terrain_spec", "stories"],
            "properties": {
                "terrain_spec": {
                    "type": "object",
                    "description": "Terrain specification for map generation",
                    "properties": {
                        "reuse_map_id": {
                            "type": "string",
                            "description": "Existing map to reuse (null if new map needed)",
                        },
                        "landscape": {
                            "type": "object",
                            "description": "Landscape configuration (heightmap, size, material, biome)",
                        },
                        "spawners": {
                            "type": "array",
                            "items": {"type": "object"},
                            "description": "Spawner placement data (position, entity tag, spawn rules)",
                        },
                        "regions": {
                            "type": "array",
                            "items": {"type": "object"},
                            "description": "Named regions with bounds and properties",
                        },
                    },
                },
                "stories": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["title", "description", "story_tag"],
                        "properties": {
                            "title": {"type": "string"},
                            "description": {"type": "string"},
                            "story_tag": {
                                "type": "string",
                                "description": "GameplayTag for the story (e.g. Story.Quest.DragonSlayer)",
                            },
                            "region": {
                                "type": "string",
                                "description": "Region name where this story takes place",
                            },
                        },
                    },
                },
            },
        },
    },
    StepType.MAP_GENERATION: {
        "input": {
            "type": "object",
            "required": ["terrain_spec", "stories"],
            "description": "Output from concept_design step",
        },
        "output": {
            "type": "object",
            "required": ["map_id", "map_path"],
            "properties": {
                "map_id": {
                    "type": "string",
                    "description": "Unique identifier for the generated HktMap",
                },
                "map_path": {
                    "type": "string",
                    "description": "File path to the generated HktMap JSON",
                },
                "hkt_map": {
                    "type": "object",
                    "description": "The full HktMap data (landscape, spawners, story refs, regions)",
                },
            },
        },
    },
    StepType.STORY_GENERATION: {
        "input": {
            "type": "object",
            "required": ["stories"],
            "properties": {
                "stories": {
                    "type": "array",
                    "description": "Story list from concept_design step",
                },
                "map_id": {
                    "type": "string",
                    "description": "HktMap ID for story context (optional)",
                },
            },
        },
        "output": {
            "type": "object",
            "required": ["story_files"],
            "properties": {
                "story_files": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["story_tag", "json_path"],
                        "properties": {
                            "story_tag": {"type": "string"},
                            "json_path": {"type": "string"},
                            "built": {"type": "boolean"},
                            "build_errors": {
                                "type": "array",
                                "items": {"type": "string"},
                            },
                        },
                    },
                },
            },
        },
    },
    StepType.ASSET_DISCOVERY: {
        "input": {
            "type": "object",
            "required": ["story_files"],
            "description": "Output from story_generation step",
        },
        "output": {
            "type": "object",
            "required": ["characters", "items", "vfx"],
            "properties": {
                "characters": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["tag", "description"],
                        "properties": {
                            "tag": {"type": "string"},
                            "description": {"type": "string"},
                            "skeleton_type": {"type": "string"},
                            "required_animations": {
                                "type": "array",
                                "items": {"type": "string"},
                            },
                        },
                    },
                },
                "items": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["tag", "description"],
                        "properties": {
                            "tag": {"type": "string"},
                            "description": {"type": "string"},
                            "category": {"type": "string"},
                            "sub_type": {"type": "string"},
                            "element": {"type": "string"},
                        },
                    },
                },
                "vfx": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["tag", "description"],
                        "properties": {
                            "tag": {"type": "string"},
                            "description": {"type": "string"},
                            "event_type": {"type": "string"},
                            "element": {"type": "string"},
                        },
                    },
                },
            },
        },
    },
    StepType.CHARACTER_GENERATION: {
        "input": {
            "type": "object",
            "required": ["characters"],
            "description": "Character specs from asset_discovery output",
        },
        "output": {
            "type": "object",
            "required": ["generated_assets"],
            "properties": {
                "generated_assets": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["tag", "asset_path"],
                        "properties": {
                            "tag": {"type": "string"},
                            "asset_path": {"type": "string"},
                            "mesh_path": {"type": "string"},
                            "animations": {
                                "type": "array",
                                "items": {
                                    "type": "object",
                                    "properties": {
                                        "tag": {"type": "string"},
                                        "path": {"type": "string"},
                                    },
                                },
                            },
                        },
                    },
                },
            },
        },
    },
    StepType.ITEM_GENERATION: {
        "input": {
            "type": "object",
            "required": ["items"],
            "description": "Item specs from asset_discovery output",
        },
        "output": {
            "type": "object",
            "required": ["generated_assets"],
            "properties": {
                "generated_assets": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["tag", "asset_path"],
                        "properties": {
                            "tag": {"type": "string"},
                            "asset_path": {"type": "string"},
                            "mesh_path": {"type": "string"},
                            "icon_path": {"type": "string"},
                            "material_path": {"type": "string"},
                        },
                    },
                },
            },
        },
    },
    StepType.VFX_GENERATION: {
        "input": {
            "type": "object",
            "required": ["vfx"],
            "description": "VFX specs from asset_discovery output",
        },
        "output": {
            "type": "object",
            "required": ["generated_assets"],
            "properties": {
                "generated_assets": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["tag", "asset_path"],
                        "properties": {
                            "tag": {"type": "string"},
                            "asset_path": {"type": "string"},
                            "niagara_system_path": {"type": "string"},
                        },
                    },
                },
            },
        },
    },
}


@dataclass
class StepResult:
    """Result of a single step execution."""

    step_type: str
    status: str = StepStatus.NOT_STARTED
    input_data: dict[str, Any] = field(default_factory=dict)
    output_data: dict[str, Any] = field(default_factory=dict)
    error: str = ""
    started_at: str = ""
    completed_at: str = ""
    agent_id: str = ""

    def to_dict(self) -> dict[str, Any]:
        return {
            "step_type": self.step_type,
            "status": self.status,
            "input_data": self.input_data,
            "output_data": self.output_data,
            "error": self.error,
            "started_at": self.started_at,
            "completed_at": self.completed_at,
            "agent_id": self.agent_id,
        }

    @classmethod
    def from_dict(cls, d: dict[str, Any]) -> StepResult:
        return cls(
            step_type=d["step_type"],
            status=d.get("status", StepStatus.NOT_STARTED),
            input_data=d.get("input_data", {}),
            output_data=d.get("output_data", {}),
            error=d.get("error", ""),
            started_at=d.get("started_at", ""),
            completed_at=d.get("completed_at", ""),
            agent_id=d.get("agent_id", ""),
        )


@dataclass
class StepManifest:
    """
    Manifest tracking all steps for a generation project.
    Each project has one manifest with independent step results.
    """

    project_id: str
    project_name: str = ""
    concept: str = ""
    created_at: str = field(default_factory=_now_iso)
    updated_at: str = field(default_factory=_now_iso)
    steps: dict[str, StepResult] = field(default_factory=dict)
    config: dict[str, Any] = field(default_factory=dict)

    def __post_init__(self):
        # Ensure all step types have entries
        for st in StepType:
            if st.value not in self.steps:
                self.steps[st.value] = StepResult(step_type=st.value)

    def to_dict(self) -> dict[str, Any]:
        return {
            "project_id": self.project_id,
            "project_name": self.project_name,
            "concept": self.concept,
            "created_at": self.created_at,
            "updated_at": self.updated_at,
            "steps": {k: v.to_dict() for k, v in self.steps.items()},
            "config": self.config,
        }

    @classmethod
    def from_dict(cls, d: dict[str, Any]) -> StepManifest:
        manifest = cls(
            project_id=d["project_id"],
            project_name=d.get("project_name", ""),
            concept=d.get("concept", ""),
            created_at=d.get("created_at", ""),
            updated_at=d.get("updated_at", ""),
            config=d.get("config", {}),
        )
        steps_raw = d.get("steps", {})
        for k, v in steps_raw.items():
            manifest.steps[k] = StepResult.from_dict(v)
        return manifest
