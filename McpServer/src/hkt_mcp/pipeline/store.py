"""
Pipeline persistence - JSON file storage for pipeline state
"""

from __future__ import annotations

import json
import logging
import shutil
import tempfile
from pathlib import Path
from typing import Any

from .models import Pipeline

logger = logging.getLogger("hkt_mcp.pipeline.store")


class PipelineStore:
    def __init__(self, base_path: Path):
        self._base = base_path
        self._pipelines_dir = base_path / "pipelines"
        self._pipelines_dir.mkdir(parents=True, exist_ok=True)

    def save(self, pipeline: Pipeline) -> None:
        """Save pipeline to JSON file with atomic write and backup."""
        filepath = self._pipelines_dir / f"{pipeline.id}.json"

        # Create backup if file already exists
        if filepath.exists():
            backup = filepath.with_suffix(".json.bak")
            shutil.copy2(filepath, backup)

        data = json.dumps(pipeline.to_dict(), indent=2, ensure_ascii=False)

        # Atomic write: write to temp then rename
        fd, tmp_path = tempfile.mkstemp(
            dir=self._pipelines_dir, suffix=".tmp"
        )
        try:
            with open(fd, "w", encoding="utf-8") as f:
                f.write(data)
            Path(tmp_path).replace(filepath)
        except Exception:
            Path(tmp_path).unlink(missing_ok=True)
            raise

        self._update_index()
        logger.info("Saved pipeline %s", pipeline.id)

    def load(self, pipeline_id: str) -> Pipeline:
        """Load pipeline from JSON file."""
        filepath = self._pipelines_dir / f"{pipeline_id}.json"
        if not filepath.exists():
            raise FileNotFoundError(f"Pipeline '{pipeline_id}' not found")

        with open(filepath, "r", encoding="utf-8") as f:
            data = json.load(f)
        return Pipeline.from_dict(data)

    def list_all(self) -> list[dict[str, Any]]:
        """Return lightweight index of all pipelines."""
        index_path = self._base / "index.json"
        if index_path.exists():
            with open(index_path, "r", encoding="utf-8") as f:
                return json.load(f)
        # Rebuild index from files
        self._update_index()
        if index_path.exists():
            with open(index_path, "r", encoding="utf-8") as f:
                return json.load(f)
        return []

    def delete(self, pipeline_id: str) -> None:
        """Delete a pipeline and its backup."""
        filepath = self._pipelines_dir / f"{pipeline_id}.json"
        if not filepath.exists():
            raise FileNotFoundError(f"Pipeline '{pipeline_id}' not found")
        filepath.unlink()
        filepath.with_suffix(".json.bak").unlink(missing_ok=True)
        self._update_index()
        logger.info("Deleted pipeline %s", pipeline_id)

    def _update_index(self) -> None:
        """Rebuild the lightweight index from pipeline files."""
        entries: list[dict[str, Any]] = []
        for filepath in sorted(self._pipelines_dir.glob("*.json")):
            try:
                with open(filepath, "r", encoding="utf-8") as f:
                    data = json.load(f)
                entries.append({
                    "id": data["id"],
                    "name": data["name"],
                    "current_phase": data.get("current_phase", ""),
                    "updated_at": data.get("updated_at", ""),
                })
            except (json.JSONDecodeError, KeyError) as e:
                logger.warning("Skipping corrupt pipeline file %s: %s", filepath, e)

        index_path = self._base / "index.json"
        with open(index_path, "w", encoding="utf-8") as f:
            json.dump(entries, f, indent=2, ensure_ascii=False)
