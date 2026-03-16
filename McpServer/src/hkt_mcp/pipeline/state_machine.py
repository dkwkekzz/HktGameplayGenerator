"""
Pipeline state machine - Phase transition logic and validation
"""

from __future__ import annotations

import logging
from typing import Any

from .models import (
    Pipeline, PhaseState, Checkpoint, Task,
    PipelinePhase, TaskStatus, CheckpointAction, _now_iso,
)

logger = logging.getLogger("hkt_mcp.pipeline.state_machine")

# Ordered phase list for linear progression
PHASE_ORDER = [
    PipelinePhase.DESIGN,
    PipelinePhase.TASK_PLANNING,
    PipelinePhase.STORY_BUILDING,
    PipelinePhase.ASSET_DISCOVERY,
    PipelinePhase.VERIFICATION,
]

TERMINAL_STATUSES = {TaskStatus.COMPLETED.value, TaskStatus.FAILED.value}
BLOCKING_STATUSES = {TaskStatus.IN_PROGRESS.value, TaskStatus.BLOCKED.value}


class PipelineStateMachine:

    def can_advance(self, pipeline: Pipeline) -> tuple[bool, str]:
        """Check if the current phase can advance to the next."""
        phase = pipeline.current_phase
        tasks = pipeline.tasks_for_phase(phase)

        # Check for unresolved checkpoints
        for cp in pipeline.checkpoints:
            if cp.phase == phase and cp.action is None:
                return False, f"Unresolved checkpoint '{cp.id}' must be resolved first"

        if not tasks:
            return True, "No tasks in phase (empty phase)"

        blocking = [t for t in tasks if t.status in BLOCKING_STATUSES]
        if blocking:
            names = ", ".join(t.title for t in blocking[:3])
            return False, f"Tasks still active or blocked: {names}"

        pending = [t for t in tasks if t.status == TaskStatus.PENDING.value]
        if pending:
            names = ", ".join(t.title for t in pending[:3])
            return False, f"Tasks still pending: {names}"

        # All tasks must be completed or failed
        review = [t for t in tasks if t.status == TaskStatus.REVIEW.value]
        if review:
            names = ", ".join(t.title for t in review[:3])
            return False, f"Tasks awaiting review: {names}"

        return True, "All tasks resolved"

    def get_next_phase(self, phase: str) -> str | None:
        """Get the next phase in the linear progression."""
        try:
            idx = PHASE_ORDER.index(PipelinePhase(phase))
        except (ValueError, IndexError):
            return None
        if idx + 1 < len(PHASE_ORDER):
            return PHASE_ORDER[idx + 1].value
        return None

    def create_checkpoint(self, pipeline: Pipeline, report: dict[str, Any]) -> Checkpoint:
        """Create a checkpoint for the current phase."""
        phase = pipeline.current_phase
        cp_count = sum(1 for c in pipeline.checkpoints if c.phase == phase)
        cp_id = f"cp_{phase}_{cp_count + 1:03d}"

        next_phase = self.get_next_phase(phase)
        checkpoint = Checkpoint(
            id=cp_id,
            phase=phase,
            title=f"Phase '{phase}' review (→ {next_phase or 'complete'})",
            report=report,
        )
        pipeline.checkpoints.append(checkpoint)
        pipeline.updated_at = _now_iso()
        return checkpoint

    def resolve_checkpoint(
        self,
        pipeline: Pipeline,
        checkpoint_id: str,
        action: str,
        feedback: str = "",
        target_phase: str = "",
    ) -> dict[str, Any]:
        """Resolve a checkpoint and transition the pipeline accordingly."""
        checkpoint = None
        for cp in pipeline.checkpoints:
            if cp.id == checkpoint_id:
                checkpoint = cp
                break
        if checkpoint is None:
            raise ValueError(f"Checkpoint '{checkpoint_id}' not found")
        if checkpoint.action is not None:
            raise ValueError(f"Checkpoint '{checkpoint_id}' already resolved")

        now = _now_iso()
        checkpoint.action = action
        checkpoint.user_feedback = feedback or None
        checkpoint.resolved_at = now

        result: dict[str, Any] = {
            "checkpoint_id": checkpoint_id,
            "action": action,
        }

        if action == CheckpointAction.APPROVE.value:
            next_phase = self.get_next_phase(pipeline.current_phase)
            if next_phase is None:
                # Pipeline complete
                phase_state = pipeline.phases[pipeline.current_phase]
                phase_state.status = TaskStatus.COMPLETED.value
                phase_state.completed_at = now
                result["message"] = "Pipeline completed — all phases done"
                result["next_phase"] = None
            else:
                # Advance
                old_state = pipeline.phases[pipeline.current_phase]
                old_state.status = TaskStatus.COMPLETED.value
                old_state.completed_at = now

                pipeline.current_phase = next_phase
                new_state = pipeline.phases[next_phase]
                new_state.status = TaskStatus.IN_PROGRESS.value
                new_state.entered_at = now

                result["message"] = f"Advanced to phase '{next_phase}'"
                result["next_phase"] = next_phase

        elif action == CheckpointAction.REVISE.value:
            # Stay in current phase, reset failed tasks to pending
            for task in pipeline.tasks_for_phase(pipeline.current_phase):
                if task.status == TaskStatus.FAILED.value:
                    task.status = TaskStatus.PENDING.value
                    task.updated_at = now
            result["message"] = f"Revising phase '{pipeline.current_phase}'"
            result["next_phase"] = pipeline.current_phase

        elif action == CheckpointAction.REJECT.value:
            if not target_phase:
                raise ValueError("target_phase required for reject action")
            if target_phase not in [p.value for p in PipelinePhase]:
                raise ValueError(f"Invalid target_phase: {target_phase}")

            # Mark current phase as pending
            old_state = pipeline.phases[pipeline.current_phase]
            old_state.status = TaskStatus.PENDING.value
            old_state.completed_at = None

            # Move to target phase
            pipeline.current_phase = target_phase
            target_state = pipeline.phases[target_phase]
            target_state.status = TaskStatus.IN_PROGRESS.value
            target_state.entered_at = now
            target_state.completed_at = None

            result["message"] = f"Rejected — returning to phase '{target_phase}'"
            result["next_phase"] = target_phase
        else:
            raise ValueError(f"Invalid action: {action}")

        pipeline.updated_at = now
        return result
