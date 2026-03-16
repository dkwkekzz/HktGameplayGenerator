"""
Pipeline data models - Enums, dataclasses, and serialization
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from datetime import datetime, timezone
from enum import Enum
from typing import Any


class PipelinePhase(str, Enum):
    DESIGN = "design"
    TASK_PLANNING = "task_planning"
    STORY_BUILDING = "story_building"
    ASSET_DISCOVERY = "asset_discovery"
    VERIFICATION = "verification"


class TaskStatus(str, Enum):
    PENDING = "pending"
    IN_PROGRESS = "in_progress"
    COMPLETED = "completed"
    FAILED = "failed"
    BLOCKED = "blocked"
    REVIEW = "review"


class TaskCategory(str, Enum):
    STORY = "story"
    MESH = "mesh"
    ANIMATION = "animation"
    VFX = "vfx"
    TEXTURE = "texture"
    ITEM = "item"
    STORY_BUILDER = "story_builder"
    DESIGN = "design"
    VERIFICATION = "verification"


class CheckpointAction(str, Enum):
    APPROVE = "approve"
    REVISE = "revise"
    REJECT = "reject"


def _now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


@dataclass
class Task:
    id: str
    category: str
    title: str
    description: str
    status: str = TaskStatus.PENDING.value
    phase: str = ""
    parent_id: str | None = None
    tags: list[str] = field(default_factory=list)
    created_at: str = field(default_factory=_now_iso)
    updated_at: str = field(default_factory=_now_iso)
    result: dict[str, Any] | None = None
    error: str | None = None
    notes: list[str] = field(default_factory=list)
    mcp_tool_hint: str | None = None

    def to_dict(self) -> dict[str, Any]:
        return {
            "id": self.id,
            "category": self.category,
            "title": self.title,
            "description": self.description,
            "status": self.status,
            "phase": self.phase,
            "parent_id": self.parent_id,
            "tags": self.tags,
            "created_at": self.created_at,
            "updated_at": self.updated_at,
            "result": self.result,
            "error": self.error,
            "notes": self.notes,
            "mcp_tool_hint": self.mcp_tool_hint,
        }

    @classmethod
    def from_dict(cls, d: dict[str, Any]) -> Task:
        return cls(
            id=d["id"],
            category=d["category"],
            title=d["title"],
            description=d["description"],
            status=d.get("status", TaskStatus.PENDING.value),
            phase=d.get("phase", ""),
            parent_id=d.get("parent_id"),
            tags=d.get("tags", []),
            created_at=d.get("created_at", _now_iso()),
            updated_at=d.get("updated_at", _now_iso()),
            result=d.get("result"),
            error=d.get("error"),
            notes=d.get("notes", []),
            mcp_tool_hint=d.get("mcp_tool_hint"),
        )


@dataclass
class Checkpoint:
    id: str
    phase: str
    title: str
    created_at: str = field(default_factory=_now_iso)
    report: dict[str, Any] = field(default_factory=dict)
    action: str | None = None
    user_feedback: str | None = None
    resolved_at: str | None = None

    def to_dict(self) -> dict[str, Any]:
        return {
            "id": self.id,
            "phase": self.phase,
            "title": self.title,
            "created_at": self.created_at,
            "report": self.report,
            "action": self.action,
            "user_feedback": self.user_feedback,
            "resolved_at": self.resolved_at,
        }

    @classmethod
    def from_dict(cls, d: dict[str, Any]) -> Checkpoint:
        return cls(
            id=d["id"],
            phase=d["phase"],
            title=d["title"],
            created_at=d.get("created_at", _now_iso()),
            report=d.get("report", {}),
            action=d.get("action"),
            user_feedback=d.get("user_feedback"),
            resolved_at=d.get("resolved_at"),
        )


@dataclass
class PhaseState:
    phase: str
    status: str = TaskStatus.PENDING.value
    entered_at: str | None = None
    completed_at: str | None = None

    def to_dict(self) -> dict[str, Any]:
        return {
            "phase": self.phase,
            "status": self.status,
            "entered_at": self.entered_at,
            "completed_at": self.completed_at,
        }

    @classmethod
    def from_dict(cls, d: dict[str, Any]) -> PhaseState:
        return cls(
            phase=d["phase"],
            status=d.get("status", TaskStatus.PENDING.value),
            entered_at=d.get("entered_at"),
            completed_at=d.get("completed_at"),
        )


@dataclass
class Pipeline:
    id: str
    name: str
    description: str
    current_phase: str = PipelinePhase.DESIGN.value
    phases: dict[str, PhaseState] = field(default_factory=dict)
    tasks: list[Task] = field(default_factory=list)
    checkpoints: list[Checkpoint] = field(default_factory=list)
    created_at: str = field(default_factory=_now_iso)
    updated_at: str = field(default_factory=_now_iso)
    metadata: dict[str, Any] = field(default_factory=dict)

    def __post_init__(self):
        if not self.phases:
            now = _now_iso()
            for p in PipelinePhase:
                state = PhaseState(phase=p.value)
                if p == PipelinePhase.DESIGN:
                    state.status = TaskStatus.IN_PROGRESS.value
                    state.entered_at = now
                self.phases[p.value] = state

    def tasks_for_phase(self, phase: str) -> list[Task]:
        return [t for t in self.tasks if t.phase == phase]

    def next_task_id(self) -> str:
        return f"task_{len(self.tasks) + 1:03d}"

    def to_dict(self) -> dict[str, Any]:
        return {
            "id": self.id,
            "name": self.name,
            "description": self.description,
            "current_phase": self.current_phase,
            "phases": {k: v.to_dict() for k, v in self.phases.items()},
            "tasks": [t.to_dict() for t in self.tasks],
            "checkpoints": [c.to_dict() for c in self.checkpoints],
            "created_at": self.created_at,
            "updated_at": self.updated_at,
            "metadata": self.metadata,
        }

    @classmethod
    def from_dict(cls, d: dict[str, Any]) -> Pipeline:
        pipeline = cls(
            id=d["id"],
            name=d["name"],
            description=d["description"],
            current_phase=d.get("current_phase", PipelinePhase.DESIGN.value),
            created_at=d.get("created_at", _now_iso()),
            updated_at=d.get("updated_at", _now_iso()),
            metadata=d.get("metadata", {}),
        )
        pipeline.phases = {
            k: PhaseState.from_dict(v) for k, v in d.get("phases", {}).items()
        }
        pipeline.tasks = [Task.from_dict(t) for t in d.get("tasks", [])]
        pipeline.checkpoints = [Checkpoint.from_dict(c) for c in d.get("checkpoints", [])]
        return pipeline
