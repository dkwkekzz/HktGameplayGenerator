"""
File-based step store for modular pipeline.

Stores step manifests and individual step results as JSON files.
Directory structure:
    .hkt_steps/
        {project_id}/
            manifest.json          # Project manifest with all step statuses
            concept_design/
                output.json        # Step output data
            map_generation/
                output.json
            story_generation/
                output.json
            asset_discovery/
                output.json
            character_generation/
                output.json
            item_generation/
                output.json
            vfx_generation/
                output.json
"""

from __future__ import annotations

import json
import logging
import shutil
import tempfile
from pathlib import Path
from typing import Any

from .models import StepManifest, StepResult, StepStatus, StepType, _now_iso

logger = logging.getLogger(__name__)


class StepStore:
    """Persistent JSON file store for step data."""

    def __init__(self, base_path: Path | None = None):
        if base_path is None:
            base_path = Path.cwd() / ".hkt_steps"
        self._base = Path(base_path)
        self._base.mkdir(parents=True, exist_ok=True)

    @property
    def base_path(self) -> Path:
        return self._base

    # ── Manifest Operations ──────────────────────────────────────────

    def create_project(
        self,
        project_id: str,
        project_name: str = "",
        concept: str = "",
        config: dict[str, Any] | None = None,
    ) -> StepManifest:
        project_dir = self._base / project_id
        project_dir.mkdir(parents=True, exist_ok=True)

        # Create step subdirectories
        for st in StepType:
            (project_dir / st.value).mkdir(exist_ok=True)

        manifest = StepManifest(
            project_id=project_id,
            project_name=project_name,
            concept=concept,
            config=config or {},
        )
        self._save_manifest(manifest)
        self._update_index()
        return manifest

    def load_manifest(self, project_id: str) -> StepManifest:
        path = self._base / project_id / "manifest.json"
        if not path.exists():
            raise FileNotFoundError(f"Project not found: {project_id}")
        data = json.loads(path.read_text(encoding="utf-8"))
        return StepManifest.from_dict(data)

    def list_projects(self) -> list[dict[str, Any]]:
        index_path = self._base / "index.json"
        if index_path.exists():
            try:
                return json.loads(index_path.read_text(encoding="utf-8"))
            except (json.JSONDecodeError, OSError):
                pass
        self._update_index()
        if index_path.exists():
            return json.loads(index_path.read_text(encoding="utf-8"))
        return []

    def delete_project(self, project_id: str) -> None:
        project_dir = self._base / project_id
        if project_dir.exists():
            shutil.rmtree(project_dir)
        self._update_index()

    # ── Step I/O Operations ──────────────────────────────────────────

    def save_step_output(
        self,
        project_id: str,
        step_type: str,
        output_data: dict[str, Any],
        agent_id: str = "",
    ) -> StepResult:
        manifest = self.load_manifest(project_id)
        step = manifest.steps[step_type]
        step.output_data = output_data
        step.status = StepStatus.COMPLETED
        step.completed_at = _now_iso()
        if agent_id:
            step.agent_id = agent_id

        # Write step output to its own file for direct file-based access
        output_path = self._base / project_id / step_type / "output.json"
        self._atomic_write(output_path, output_data)

        manifest.updated_at = _now_iso()
        self._save_manifest(manifest)
        self._update_index()
        return step

    def save_step_input(
        self,
        project_id: str,
        step_type: str,
        input_data: dict[str, Any],
        agent_id: str = "",
    ) -> StepResult:
        manifest = self.load_manifest(project_id)
        step = manifest.steps[step_type]
        step.input_data = input_data
        step.status = StepStatus.IN_PROGRESS
        step.started_at = _now_iso()
        if agent_id:
            step.agent_id = agent_id

        # Write step input to its own file
        input_path = self._base / project_id / step_type / "input.json"
        self._atomic_write(input_path, input_data)

        manifest.updated_at = _now_iso()
        self._save_manifest(manifest)
        return step

    def load_step_output(
        self, project_id: str, step_type: str
    ) -> dict[str, Any]:
        """Load step output - can be read by file or via this method."""
        output_path = self._base / project_id / step_type / "output.json"
        if not output_path.exists():
            # Fallback to manifest
            manifest = self.load_manifest(project_id)
            return manifest.steps.get(step_type, StepResult(step_type=step_type)).output_data
        return json.loads(output_path.read_text(encoding="utf-8"))

    def load_step_input(
        self, project_id: str, step_type: str
    ) -> dict[str, Any]:
        input_path = self._base / project_id / step_type / "input.json"
        if not input_path.exists():
            manifest = self.load_manifest(project_id)
            return manifest.steps.get(step_type, StepResult(step_type=step_type)).input_data
        return json.loads(input_path.read_text(encoding="utf-8"))

    def mark_step_failed(
        self, project_id: str, step_type: str, error: str
    ) -> StepResult:
        manifest = self.load_manifest(project_id)
        step = manifest.steps[step_type]
        step.status = StepStatus.FAILED
        step.error = error
        step.completed_at = _now_iso()

        manifest.updated_at = _now_iso()
        self._save_manifest(manifest)
        self._update_index()
        return step

    def get_step_status(
        self, project_id: str, step_type: str
    ) -> dict[str, Any]:
        manifest = self.load_manifest(project_id)
        step = manifest.steps.get(step_type)
        if step is None:
            return {"error": f"Unknown step: {step_type}"}
        return step.to_dict()

    # ── Internal Helpers ─────────────────────────────────────────────

    def _save_manifest(self, manifest: StepManifest) -> None:
        path = self._base / manifest.project_id / "manifest.json"
        self._atomic_write(path, manifest.to_dict())

    def _atomic_write(self, path: Path, data: Any) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        tmp_fd, tmp_path = tempfile.mkstemp(
            dir=path.parent, suffix=".tmp"
        )
        try:
            with open(tmp_fd, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
            Path(tmp_path).replace(path)
        except Exception:
            Path(tmp_path).unlink(missing_ok=True)
            raise

    def _update_index(self) -> None:
        entries = []
        for d in sorted(self._base.iterdir()):
            manifest_path = d / "manifest.json"
            if d.is_dir() and manifest_path.exists():
                try:
                    data = json.loads(manifest_path.read_text(encoding="utf-8"))
                    # Compute step summary
                    steps = data.get("steps", {})
                    completed = sum(
                        1 for s in steps.values()
                        if s.get("status") == StepStatus.COMPLETED
                    )
                    total = len(steps)
                    entries.append({
                        "project_id": data.get("project_id", d.name),
                        "project_name": data.get("project_name", ""),
                        "created_at": data.get("created_at", ""),
                        "updated_at": data.get("updated_at", ""),
                        "progress": f"{completed}/{total}",
                    })
                except (json.JSONDecodeError, OSError):
                    logger.warning("Corrupt manifest in %s", d)
        index_path = self._base / "index.json"
        self._atomic_write(index_path, entries)
