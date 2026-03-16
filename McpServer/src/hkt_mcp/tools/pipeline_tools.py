"""
Pipeline Monitor Tools - MCP tools for pipeline state tracking and checkpoints

These tools are pure Python state management — they do NOT use EditorBridge.
The LLM agent uses these to track progress while calling existing MCP tools
(build_story, request_character_mesh, etc.) for actual execution.
"""

import json
import logging
import re
from pathlib import Path
from typing import Any

from ..pipeline.models import (
    Pipeline, Task, PipelinePhase, TaskStatus, TaskCategory,
    CheckpointAction, _now_iso,
)
from ..pipeline.store import PipelineStore
from ..pipeline.state_machine import PipelineStateMachine
from ..pipeline.reporter import PipelineReporter

logger = logging.getLogger("hkt_mcp.tools.pipeline")

_store: PipelineStore | None = None
_machine = PipelineStateMachine()
_reporter = PipelineReporter()


def _get_store() -> PipelineStore:
    global _store
    if _store is None:
        from ..config import get_config
        config = get_config()
        base = Path(getattr(config, "pipeline_data_path", ".pipeline_data"))
        _store = PipelineStore(base)
    return _store


def _make_id(name: str) -> str:
    """Generate a pipeline ID from a name."""
    slug = re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")
    from datetime import datetime, timezone
    date = datetime.now(timezone.utc).strftime("%Y%m%d")
    return f"{slug}_{date}"


async def pipeline_create(name: str, description: str, metadata_json: str = "{}") -> str:
    """Create a new pipeline."""
    logger.info("Creating pipeline: %s", name)
    store = _get_store()

    try:
        metadata = json.loads(metadata_json) if metadata_json else {}
    except json.JSONDecodeError:
        return json.dumps({"success": False, "error": "Invalid metadata_json"})

    pipeline_id = _make_id(name)
    pipeline = Pipeline(
        id=pipeline_id,
        name=name,
        description=description,
        metadata=metadata,
    )
    store.save(pipeline)

    return json.dumps({
        "success": True,
        "pipeline_id": pipeline_id,
        "current_phase": pipeline.current_phase,
        "message": f"Pipeline '{name}' created. Current phase: {pipeline.current_phase}",
    }, indent=2)


async def pipeline_list() -> str:
    """List all pipelines with summary status."""
    logger.info("Listing pipelines")
    store = _get_store()
    entries = store.list_all()
    return json.dumps({"success": True, "pipelines": entries, "count": len(entries)}, indent=2)


async def pipeline_get_status(pipeline_id: str) -> str:
    """Get comprehensive pipeline status."""
    logger.info("Getting status for pipeline: %s", pipeline_id)
    store = _get_store()

    try:
        pipeline = store.load(pipeline_id)
    except FileNotFoundError as e:
        return json.dumps({"success": False, "error": str(e)})

    report = _reporter.summary(pipeline)
    return json.dumps({"success": True, **report}, indent=2)


async def pipeline_delete(pipeline_id: str) -> str:
    """Delete a pipeline."""
    logger.info("Deleting pipeline: %s", pipeline_id)
    store = _get_store()

    try:
        store.delete(pipeline_id)
    except FileNotFoundError as e:
        return json.dumps({"success": False, "error": str(e)})

    return json.dumps({"success": True, "message": f"Pipeline '{pipeline_id}' deleted"})


async def pipeline_add_tasks(pipeline_id: str, tasks_json: str) -> str:
    """Add tasks to the pipeline's current phase."""
    logger.info("Adding tasks to pipeline: %s", pipeline_id)
    store = _get_store()

    try:
        pipeline = store.load(pipeline_id)
    except FileNotFoundError as e:
        return json.dumps({"success": False, "error": str(e)})

    try:
        task_defs = json.loads(tasks_json)
    except json.JSONDecodeError:
        return json.dumps({"success": False, "error": "Invalid tasks_json"})

    if not isinstance(task_defs, list):
        task_defs = [task_defs]

    added: list[str] = []
    for td in task_defs:
        task_id = pipeline.next_task_id()
        task = Task(
            id=task_id,
            category=td.get("category", TaskCategory.DESIGN.value),
            title=td["title"],
            description=td.get("description", ""),
            phase=pipeline.current_phase,
            parent_id=td.get("parent_id"),
            tags=td.get("tags", []),
            mcp_tool_hint=td.get("mcp_tool_hint"),
        )
        pipeline.tasks.append(task)
        added.append(task_id)

    pipeline.updated_at = _now_iso()
    store.save(pipeline)

    return json.dumps({
        "success": True,
        "added_task_ids": added,
        "total_tasks": len(pipeline.tasks),
        "current_phase": pipeline.current_phase,
    }, indent=2)


async def pipeline_update_task(
    pipeline_id: str,
    task_id: str,
    status: str = "",
    result_json: str = "",
    error: str = "",
    note: str = "",
) -> str:
    """Update a task's status, result, error, or add a note."""
    logger.info("Updating task %s in pipeline %s", task_id, pipeline_id)
    store = _get_store()

    try:
        pipeline = store.load(pipeline_id)
    except FileNotFoundError as e:
        return json.dumps({"success": False, "error": str(e)})

    task = None
    for t in pipeline.tasks:
        if t.id == task_id:
            task = t
            break
    if task is None:
        return json.dumps({"success": False, "error": f"Task '{task_id}' not found"})

    now = _now_iso()
    if status:
        # Validate status value
        valid = {s.value for s in TaskStatus}
        if status not in valid:
            return json.dumps({"success": False, "error": f"Invalid status: {status}. Valid: {valid}"})
        task.status = status
    if result_json:
        try:
            task.result = json.loads(result_json)
        except json.JSONDecodeError:
            return json.dumps({"success": False, "error": "Invalid result_json"})
    if error:
        task.error = error
    if note:
        task.notes.append(note)

    task.updated_at = now
    pipeline.updated_at = now
    store.save(pipeline)

    return json.dumps({
        "success": True,
        "task_id": task_id,
        "status": task.status,
        "message": f"Task '{task.title}' updated",
    }, indent=2)


async def pipeline_get_tasks(
    pipeline_id: str,
    phase: str = "",
    status: str = "",
    category: str = "",
) -> str:
    """Get filtered task list."""
    logger.info("Getting tasks for pipeline: %s", pipeline_id)
    store = _get_store()

    try:
        pipeline = store.load(pipeline_id)
    except FileNotFoundError as e:
        return json.dumps({"success": False, "error": str(e)})

    tasks = pipeline.tasks
    if phase:
        tasks = [t for t in tasks if t.phase == phase]
    if status:
        tasks = [t for t in tasks if t.status == status]
    if category:
        tasks = [t for t in tasks if t.category == category]

    return json.dumps({
        "success": True,
        "tasks": [t.to_dict() for t in tasks],
        "count": len(tasks),
    }, indent=2)


async def pipeline_request_advance(pipeline_id: str) -> str:
    """Request advancing to the next phase. Creates a checkpoint with report."""
    logger.info("Requesting advance for pipeline: %s", pipeline_id)
    store = _get_store()

    try:
        pipeline = store.load(pipeline_id)
    except FileNotFoundError as e:
        return json.dumps({"success": False, "error": str(e)})

    can, reason = _machine.can_advance(pipeline)
    if not can:
        return json.dumps({
            "success": False,
            "error": f"Cannot advance: {reason}",
            "current_phase": pipeline.current_phase,
        }, indent=2)

    report = _reporter.checkpoint_report(pipeline)
    checkpoint = _machine.create_checkpoint(pipeline, report)
    store.save(pipeline)

    return json.dumps({
        "success": True,
        "checkpoint_id": checkpoint.id,
        "checkpoint_report": report,
        "message": "Checkpoint created — awaiting user review (approve/revise/reject)",
    }, indent=2)


async def pipeline_resolve_checkpoint(
    pipeline_id: str,
    checkpoint_id: str,
    action: str,
    feedback: str = "",
    target_phase: str = "",
) -> str:
    """Resolve a checkpoint: approve, revise, or reject."""
    logger.info("Resolving checkpoint %s with action %s", checkpoint_id, action)
    store = _get_store()

    try:
        pipeline = store.load(pipeline_id)
    except FileNotFoundError as e:
        return json.dumps({"success": False, "error": str(e)})

    try:
        result = _machine.resolve_checkpoint(
            pipeline, checkpoint_id, action, feedback, target_phase,
        )
    except ValueError as e:
        return json.dumps({"success": False, "error": str(e)})

    store.save(pipeline)

    return json.dumps({"success": True, **result}, indent=2)
