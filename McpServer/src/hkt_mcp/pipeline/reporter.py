"""
Pipeline reporter - Generates structured reports for status and checkpoints
"""

from __future__ import annotations

from typing import Any

from .models import Pipeline, Task, PipelinePhase, TaskStatus


class PipelineReporter:

    def summary(self, pipeline: Pipeline) -> dict[str, Any]:
        """Generate a summary report of the pipeline."""
        phase_summary: dict[str, Any] = {}
        for p in PipelinePhase:
            tasks = pipeline.tasks_for_phase(p.value)
            completed = sum(1 for t in tasks if t.status == TaskStatus.COMPLETED.value)
            total = len(tasks)
            state = pipeline.phases.get(p.value)
            phase_summary[p.value] = {
                "status": state.status if state else "unknown",
                "tasks": f"{completed}/{total}",
            }

        # Current phase detail
        current_tasks = pipeline.tasks_for_phase(pipeline.current_phase)
        detail = self._group_tasks_by_status(current_tasks)

        # Pending checkpoints
        pending_cps = [
            {"id": cp.id, "phase": cp.phase, "title": cp.title}
            for cp in pipeline.checkpoints if cp.action is None
        ]

        # Next action hint
        next_action = self._suggest_next_action(pipeline, current_tasks)

        return {
            "pipeline": pipeline.name,
            "pipeline_id": pipeline.id,
            "current_phase": pipeline.current_phase,
            "overall_progress": self._calculate_progress(pipeline),
            "phase_summary": phase_summary,
            "current_phase_detail": detail,
            "pending_checkpoints": pending_cps,
            "next_action": next_action,
        }

    def checkpoint_report(self, pipeline: Pipeline) -> dict[str, Any]:
        """Generate a report for phase advancement checkpoint."""
        phase = pipeline.current_phase
        tasks = pipeline.tasks_for_phase(phase)

        completed = [t for t in tasks if t.status == TaskStatus.COMPLETED.value]
        failed = [t for t in tasks if t.status == TaskStatus.FAILED.value]

        key_outputs = []
        for t in completed:
            entry: dict[str, Any] = {"task": t.title, "category": t.category}
            if t.result:
                entry["result"] = t.result
            key_outputs.append(entry)

        issues = []
        for t in failed:
            issues.append({"task": t.title, "error": t.error or "Unknown error"})

        # Determine next phase
        from .state_machine import PipelineStateMachine
        sm = PipelineStateMachine()
        next_phase = sm.get_next_phase(phase)

        recommendations = []
        if not failed:
            recommendations.append(f"All {len(completed)} tasks completed successfully")
        else:
            recommendations.append(f"{len(failed)} task(s) failed — review before advancing")
        if next_phase:
            recommendations.append(f"Next phase: {next_phase}")

        return {
            "phase_completed": phase,
            "next_phase": next_phase,
            "tasks_completed": len(completed),
            "tasks_failed": len(failed),
            "tasks_total": len(tasks),
            "key_outputs": key_outputs,
            "issues": issues,
            "recommendations": recommendations,
            "awaiting_user_action": "approve / revise / reject",
        }

    def phase_report(self, pipeline: Pipeline, phase: str) -> dict[str, Any]:
        """Detailed report for a specific phase."""
        tasks = pipeline.tasks_for_phase(phase)
        state = pipeline.phases.get(phase)
        return {
            "phase": phase,
            "status": state.status if state else "unknown",
            "entered_at": state.entered_at if state else None,
            "completed_at": state.completed_at if state else None,
            "tasks": [t.to_dict() for t in tasks],
            "task_counts": self._count_by_status(tasks),
        }

    def _calculate_progress(self, pipeline: Pipeline) -> str:
        """Calculate overall progress as percentage string."""
        total = len(pipeline.tasks)
        if total == 0:
            return "0%"
        completed = sum(
            1 for t in pipeline.tasks
            if t.status == TaskStatus.COMPLETED.value
        )
        pct = int((completed / total) * 100)
        return f"{pct}%"

    def _group_tasks_by_status(self, tasks: list[Task]) -> dict[str, list[str]]:
        groups: dict[str, list[str]] = {}
        for status in TaskStatus:
            matching = [t.title for t in tasks if t.status == status.value]
            if matching:
                groups[status.value] = matching
        return groups

    def _count_by_status(self, tasks: list[Task]) -> dict[str, int]:
        counts: dict[str, int] = {}
        for status in TaskStatus:
            c = sum(1 for t in tasks if t.status == status.value)
            if c > 0:
                counts[status.value] = c
        return counts

    def _suggest_next_action(self, pipeline: Pipeline, current_tasks: list[Task]) -> str:
        # Check pending checkpoints first
        pending_cps = [cp for cp in pipeline.checkpoints if cp.action is None]
        if pending_cps:
            return f"Resolve pending checkpoint '{pending_cps[0].id}'"

        in_progress = [t for t in current_tasks if t.status == TaskStatus.IN_PROGRESS.value]
        if in_progress:
            t = in_progress[0]
            hint = f" via {t.mcp_tool_hint}" if t.mcp_tool_hint else ""
            return f"Continue task '{t.title}'{hint}"

        pending = [t for t in current_tasks if t.status == TaskStatus.PENDING.value]
        if pending:
            t = pending[0]
            hint = f" via {t.mcp_tool_hint}" if t.mcp_tool_hint else ""
            return f"Start task '{t.title}'{hint}"

        blocked = [t for t in current_tasks if t.status == TaskStatus.BLOCKED.value]
        if blocked:
            return f"Unblock task '{blocked[0].title}'"

        # All done — suggest advance
        return "All tasks resolved — call pipeline_request_advance to proceed"
