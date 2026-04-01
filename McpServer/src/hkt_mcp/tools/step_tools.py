"""
MCP tool functions for the modular step system.

Provides read/write access to step inputs/outputs, allowing agents
to run steps independently and pass data between them via JSON files.
"""

from __future__ import annotations

import json
import logging
import os
import re
from datetime import datetime, timezone
from typing import Any

from ..steps.models import STEP_SCHEMAS, StepManifest, StepStatus, StepType
from ..steps.store import StepStore

logger = logging.getLogger(__name__)

_store: StepStore | None = None


def _get_store() -> StepStore:
    global _store
    if _store is None:
        base = os.environ.get("HKT_STEPS_DIR", "")
        if base:
            from pathlib import Path
            _store = StepStore(Path(base))
        else:
            _store = StepStore()
    return _store


def _make_project_id(name: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")[:40]
    date = datetime.now(timezone.utc).strftime("%Y%m%d")
    return f"{slug}-{date}"


# ── Project Management ───────────────────────────────────────────────


async def step_create_project(
    name: str,
    concept: str,
    config_json: str = "{}",
) -> str:
    """Create a new generation project with step tracking."""
    store = _get_store()
    config = json.loads(config_json) if config_json else {}
    project_id = _make_project_id(name)
    manifest = store.create_project(
        project_id=project_id,
        project_name=name,
        concept=concept,
        config=config,
    )
    return json.dumps({
        "success": True,
        "project_id": manifest.project_id,
        "steps_dir": str(store.base_path / manifest.project_id),
        "steps": list(manifest.steps.keys()),
    }, ensure_ascii=False)


async def step_list_projects() -> str:
    """List all generation projects."""
    store = _get_store()
    projects = store.list_projects()
    return json.dumps(projects, ensure_ascii=False)


async def step_get_status(project_id: str) -> str:
    """Get full status of a project including all step states."""
    store = _get_store()
    manifest = store.load_manifest(project_id)
    result = manifest.to_dict()
    # Add file paths for convenience
    result["steps_dir"] = str(store.base_path / project_id)
    for step_type, step_data in result["steps"].items():
        step_dir = store.base_path / project_id / step_type
        step_data["output_file"] = str(step_dir / "output.json")
        step_data["input_file"] = str(step_dir / "input.json")
    return json.dumps(result, ensure_ascii=False, indent=2)


async def step_delete_project(project_id: str) -> str:
    """Delete a generation project and all its data."""
    store = _get_store()
    store.delete_project(project_id)
    return json.dumps({"success": True, "deleted": project_id})


# ── Step I/O ─────────────────────────────────────────────────────────


async def step_save_output(
    project_id: str,
    step_type: str,
    output_json: str,
    agent_id: str = "",
) -> str:
    """
    Save step output data. This marks the step as completed.
    The output is saved both in the manifest and as a standalone JSON file
    at .hkt_steps/{project_id}/{step_type}/output.json for direct access.
    """
    store = _get_store()
    output_data = json.loads(output_json)
    step = store.save_step_output(
        project_id=project_id,
        step_type=step_type,
        output_data=output_data,
        agent_id=agent_id,
    )
    output_path = store.base_path / project_id / step_type / "output.json"
    return json.dumps({
        "success": True,
        "step_type": step_type,
        "status": step.status,
        "output_file": str(output_path),
    }, ensure_ascii=False)


async def step_load_input(
    project_id: str,
    step_type: str,
) -> str:
    """
    Load input for a step. Automatically resolves from the appropriate
    upstream step's output based on the step dependency graph.
    """
    store = _get_store()

    # Auto-resolve input from upstream steps
    upstream = _get_upstream_step(step_type)
    if upstream:
        upstream_output = store.load_step_output(project_id, upstream)
        if upstream_output:
            return json.dumps({
                "step_type": step_type,
                "source_step": upstream,
                "input_data": upstream_output,
            }, ensure_ascii=False, indent=2)

    # Fallback: check explicit input file
    input_data = store.load_step_input(project_id, step_type)
    return json.dumps({
        "step_type": step_type,
        "source_step": None,
        "input_data": input_data,
    }, ensure_ascii=False, indent=2)


async def step_begin(
    project_id: str,
    step_type: str,
    input_json: str = "",
    agent_id: str = "",
) -> str:
    """
    Mark a step as in_progress and optionally set its input.
    If input_json is empty, auto-resolves from upstream step output.
    """
    store = _get_store()

    if input_json:
        input_data = json.loads(input_json)
    else:
        # Auto-resolve from upstream
        upstream = _get_upstream_step(step_type)
        if upstream:
            input_data = store.load_step_output(project_id, upstream)
        else:
            input_data = {}

    step = store.save_step_input(
        project_id=project_id,
        step_type=step_type,
        input_data=input_data,
        agent_id=agent_id,
    )
    return json.dumps({
        "success": True,
        "step_type": step_type,
        "status": step.status,
        "input_data": input_data,
    }, ensure_ascii=False, indent=2)


async def step_fail(
    project_id: str,
    step_type: str,
    error: str,
) -> str:
    """Mark a step as failed with an error message."""
    store = _get_store()
    step = store.mark_step_failed(project_id, step_type, error)
    return json.dumps({
        "success": True,
        "step_type": step_type,
        "status": step.status,
        "error": error,
    }, ensure_ascii=False)


async def step_get_schema(step_type: str) -> str:
    """Get the input/output JSON schema for a step type."""
    schema = STEP_SCHEMAS.get(step_type)
    if schema is None:
        return json.dumps({
            "error": f"Unknown step type: {step_type}",
            "valid_types": [s.value for s in StepType],
        })
    return json.dumps({
        "step_type": step_type,
        "input_schema": schema["input"],
        "output_schema": schema["output"],
    }, ensure_ascii=False, indent=2)


async def step_list_types() -> str:
    """List all available step types with their descriptions."""
    types = []
    for st in StepType:
        types.append({
            "type": st.value,
            "description": st.__doc__ or st.value,
            "has_schema": st.value in STEP_SCHEMAS,
        })
    return json.dumps(types, ensure_ascii=False, indent=2)


# ── Dependency Graph ─────────────────────────────────────────────────

# Upstream step mapping: which step's output feeds into which step's input
_UPSTREAM_MAP: dict[str, str | None] = {
    StepType.CONCEPT_DESIGN: None,
    StepType.FEATURE_DESIGN: StepType.CONCEPT_DESIGN,
    StepType.MAP_GENERATION: StepType.CONCEPT_DESIGN,
    StepType.STORY_GENERATION: StepType.FEATURE_DESIGN,
    StepType.ASSET_DISCOVERY: StepType.STORY_GENERATION,
    StepType.CHARACTER_GENERATION: StepType.ASSET_DISCOVERY,
    StepType.ITEM_GENERATION: StepType.ASSET_DISCOVERY,
    StepType.VFX_GENERATION: StepType.ASSET_DISCOVERY,
}


def _get_upstream_step(step_type: str) -> str | None:
    return _UPSTREAM_MAP.get(step_type)


# ── Feature Management ──────────────────────────────────────────────


async def step_add_feature(
    project_id: str,
    feature_id: str,
    name: str = "",
    source: str = "pipeline",
) -> str:
    """Register a feature in the project manifest."""
    store = _get_store()
    fs = store.add_feature(project_id, feature_id, name, source)
    return json.dumps({
        "success": True,
        "feature_id": fs.feature_id,
        "source": fs.source,
    }, ensure_ascii=False)


async def step_list_features(project_id: str) -> str:
    """List all features for a project with their statuses."""
    store = _get_store()
    features = store.list_features(project_id)
    return json.dumps(features, ensure_ascii=False, indent=2)


async def step_update_feature(
    project_id: str,
    feature_id: str,
    status: str = "",
    stories_completed: int = -1,
    assets_completed: int = -1,
    agent_id: str = "",
) -> str:
    """Update a feature's status and progress counters."""
    store = _get_store()
    kwargs: dict[str, Any] = {}
    if status:
        kwargs["status"] = status
    if stories_completed >= 0:
        kwargs["stories_completed"] = stories_completed
    if assets_completed >= 0:
        kwargs["assets_completed"] = assets_completed
    if agent_id:
        kwargs["agent_id"] = agent_id
    fs = store.update_feature(project_id, feature_id, **kwargs)
    return json.dumps(fs.to_dict(), ensure_ascii=False, indent=2)


async def feature_save_work(
    project_id: str,
    feature_id: str,
    work_json: str,
) -> str:
    """
    Save per-feature work.json. Used by Worker Agents to store their
    results without conflicting with other workers.
    Writes to .hkt_steps/{project_id}/features/{feature_id}/work.json
    """
    store = _get_store()
    work_data = json.loads(work_json)
    work_path = store.save_feature_work(project_id, feature_id, work_data)
    return json.dumps({
        "success": True,
        "feature_id": feature_id,
        "work_file": str(work_path),
    }, ensure_ascii=False)


async def feature_load_work(
    project_id: str,
    feature_id: str,
) -> str:
    """Load per-feature work.json."""
    store = _get_store()
    work = store.load_feature_work(project_id, feature_id)
    return json.dumps({
        "feature_id": feature_id,
        "work_data": work,
    }, ensure_ascii=False, indent=2)


async def feature_aggregate(project_id: str) -> str:
    """
    Aggregate all feature work.json files into unified step outputs.
    Called by the Orchestrator after all Worker Agents complete.
    Produces story_generation/output.json, asset_discovery/output.json,
    and per-type generation outputs.
    """
    store = _get_store()
    result = store.aggregate_features(project_id)
    return json.dumps({
        "success": True,
        **result,
    }, ensure_ascii=False, indent=2)
