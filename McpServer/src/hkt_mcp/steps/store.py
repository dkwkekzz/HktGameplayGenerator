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

from .models import (
    FeatureStatus,
    FeatureStatusValue,
    StepManifest,
    StepResult,
    StepStatus,
    StepType,
    _now_iso,
)

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

        # Create features directory
        (project_dir / "features").mkdir(exist_ok=True)

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

    # ── Feature Operations ───────────────────────────────────────────

    def add_feature(
        self,
        project_id: str,
        feature_id: str,
        name: str = "",
        source: str = "pipeline",
    ) -> FeatureStatus:
        """Register a feature in the project manifest."""
        manifest = self.load_manifest(project_id)
        fs = FeatureStatus(
            feature_id=feature_id,
            name=name,
            source=source,
        )
        manifest.features[feature_id] = fs

        # Create per-feature directory
        feat_dir = self._base / project_id / "features" / feature_id
        feat_dir.mkdir(parents=True, exist_ok=True)

        manifest.updated_at = _now_iso()
        self._save_manifest(manifest)
        return fs

    def update_feature(
        self,
        project_id: str,
        feature_id: str,
        *,
        status: str | None = None,
        stories_total: int | None = None,
        stories_completed: int | None = None,
        assets_total: int | None = None,
        assets_completed: int | None = None,
        agent_id: str | None = None,
    ) -> FeatureStatus:
        """Update feature status fields in the manifest."""
        manifest = self.load_manifest(project_id)
        fs = manifest.features.get(feature_id)
        if fs is None:
            raise KeyError(f"Feature not found: {feature_id}")
        if status is not None:
            fs.status = status
        if stories_total is not None:
            fs.stories_total = stories_total
        if stories_completed is not None:
            fs.stories_completed = stories_completed
        if assets_total is not None:
            fs.assets_total = assets_total
        if assets_completed is not None:
            fs.assets_completed = assets_completed
        if agent_id is not None:
            fs.agent_id = agent_id
        manifest.updated_at = _now_iso()
        self._save_manifest(manifest)
        return fs

    def list_features(self, project_id: str) -> list[dict[str, Any]]:
        """List all features for a project."""
        manifest = self.load_manifest(project_id)
        return [fs.to_dict() for fs in manifest.features.values()]

    def save_feature_work(
        self,
        project_id: str,
        feature_id: str,
        work_data: dict[str, Any],
    ) -> Path:
        """Save per-feature work.json (Worker Agent writes here)."""
        feat_dir = self._base / project_id / "features" / feature_id
        feat_dir.mkdir(parents=True, exist_ok=True)
        work_path = feat_dir / "work.json"
        self._atomic_write(work_path, work_data)
        return work_path

    def load_feature_work(
        self,
        project_id: str,
        feature_id: str,
    ) -> dict[str, Any]:
        """Load per-feature work.json."""
        work_path = self._base / project_id / "features" / feature_id / "work.json"
        if not work_path.exists():
            return {}
        return json.loads(work_path.read_text(encoding="utf-8"))

    def aggregate_features(self, project_id: str) -> dict[str, Any]:
        """
        Aggregate all feature work.json files into unified step outputs.
        Produces: story_generation/output.json, asset_discovery/output.json,
        and per-type generation outputs.
        """
        features_dir = self._base / project_id / "features"
        if not features_dir.exists():
            return {"error": "No features directory"}

        all_story_files: list[dict] = []
        all_characters: list[dict] = []
        all_items: list[dict] = []
        all_vfx: list[dict] = []
        all_generated: list[dict] = []
        errors: list[dict] = []

        for feat_dir in sorted(features_dir.iterdir()):
            work_path = feat_dir / "work.json"
            if not feat_dir.is_dir() or not work_path.exists():
                continue
            try:
                work = json.loads(work_path.read_text(encoding="utf-8"))
            except (json.JSONDecodeError, OSError):
                errors.append({"feature_id": feat_dir.name, "error": "corrupt work.json"})
                continue

            all_story_files.extend(work.get("story_files", []))

            ad = work.get("asset_discovery", {})
            all_characters.extend(ad.get("characters", []))
            all_items.extend(ad.get("items", []))
            all_vfx.extend(ad.get("vfx", []))

            all_generated.extend(work.get("generated_assets", []))
            errors.extend(
                {"feature_id": feat_dir.name, "error": e}
                for e in work.get("errors", [])
            )

        # Write aggregated step outputs
        story_out = {"story_files": all_story_files}
        self._atomic_write(
            self._base / project_id / "story_generation" / "output.json",
            story_out,
        )

        asset_out = {
            "characters": all_characters,
            "items": all_items,
            "vfx": all_vfx,
        }
        self._atomic_write(
            self._base / project_id / "asset_discovery" / "output.json",
            asset_out,
        )

        # Split generated assets by type
        char_assets = [a for a in all_generated if a.get("type") == "character"]
        item_assets = [a for a in all_generated if a.get("type") == "item"]
        vfx_assets = [a for a in all_generated if a.get("type") == "vfx"]

        if char_assets:
            self._atomic_write(
                self._base / project_id / "character_generation" / "output.json",
                {"generated_assets": char_assets},
            )
        if item_assets:
            self._atomic_write(
                self._base / project_id / "item_generation" / "output.json",
                {"generated_assets": item_assets},
            )
        if vfx_assets:
            self._atomic_write(
                self._base / project_id / "vfx_generation" / "output.json",
                {"generated_assets": vfx_assets},
            )

        return {
            "stories": len(all_story_files),
            "characters": len(all_characters),
            "items": len(all_items),
            "vfx": len(all_vfx),
            "generated_assets": len(all_generated),
            "errors": errors,
        }

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
                    features = data.get("features", {})
                    feat_done = sum(
                        1 for f in features.values()
                        if f.get("status") == FeatureStatusValue.COMPLETED
                    )
                    entries.append({
                        "project_id": data.get("project_id", d.name),
                        "project_name": data.get("project_name", ""),
                        "created_at": data.get("created_at", ""),
                        "updated_at": data.get("updated_at", ""),
                        "progress": f"{completed}/{total}",
                        "features": f"{feat_done}/{len(features)}" if features else "0/0",
                    })
                except (json.JSONDecodeError, OSError):
                    logger.warning("Corrupt manifest in %s", d)
        index_path = self._base / "index.json"
        self._atomic_write(index_path, entries)
