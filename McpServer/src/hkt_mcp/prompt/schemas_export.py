"""
Export STEP_SCHEMAS to a JSON file for the UE5 Generator Editor to consume.

Usage:
    python -m hkt_mcp.prompt.schemas_export [output_path]

The exported JSON maps step_type → {input: schema, output: schema}.
The editor uses this to validate intents and dynamically build input forms.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

from hkt_mcp.steps.models import STEP_SCHEMAS


def export_schemas(output_path: str | Path = ".hkt_steps/schemas.json") -> Path:
    """Export STEP_SCHEMAS to a JSON file."""
    output = Path(output_path)
    output.parent.mkdir(parents=True, exist_ok=True)

    # Convert enum keys to string values
    serializable = {}
    for key, value in STEP_SCHEMAS.items():
        str_key = key.value if hasattr(key, "value") else str(key)
        serializable[str_key] = value

    with open(output, "w", encoding="utf-8") as f:
        json.dump(serializable, f, indent=2, ensure_ascii=False, default=str)

    return output.resolve()


def main() -> None:
    output_path = sys.argv[1] if len(sys.argv) > 1 else ".hkt_steps/schemas.json"
    resolved = export_schemas(output_path)
    print(f"Schemas exported to: {resolved}")


if __name__ == "__main__":
    main()
