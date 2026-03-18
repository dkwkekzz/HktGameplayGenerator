"""
Python Script Executor Tool

Execute arbitrary Python code in the Unreal Editor's embedded Python environment.
Provides LLM with direct access to the full `unreal` module API.
"""

import json
import logging
from typing import Any

from ..bridge.editor_bridge import EditorBridge

logger = logging.getLogger("hkt_mcp.python_tools")

# Patterns blocked by default for safety (can be overridden with skip_safety_check)
BLOCKED_PATTERNS = [
    "os.remove(", "os.unlink(", "os.rmdir(", "shutil.rmtree(",
    "os.system(", "subprocess.", "__import__(", "importlib.",
    "os.rename(", "os.replace(",
    "pathlib.Path.unlink", "pathlib.Path.rmdir",
]


async def execute_python_script(
    bridge: EditorBridge,
    script_code: str,
    timeout: float = 30.0,
    skip_safety_check: bool = False,
) -> str:
    """
    Execute Python script in the Unreal Editor.

    The script runs in UE5's embedded Python environment with full access
    to the `unreal` module. Use print() to return data — stdout is captured.

    Args:
        bridge: EditorBridge instance
        script_code: Python code to execute
        timeout: Execution timeout in seconds
        skip_safety_check: If True, skip blocked pattern validation

    Returns:
        JSON string with {success, output, error}
    """
    logger.info(f"Executing Python script ({len(script_code)} chars)")

    # Safety check for dangerous patterns
    if not skip_safety_check:
        for pattern in BLOCKED_PATTERNS:
            if pattern in script_code:
                result = {
                    "success": False,
                    "output": "",
                    "error": (
                        f"Safety check blocked: script contains '{pattern}'. "
                        f"Set skip_safety_check=true to override if intentional."
                    ),
                }
                return json.dumps(result, indent=2)

    # Send to Unreal via bridge
    data = await bridge.call_method(
        "McpExecutePythonScript",
        ScriptCode=script_code,
        TimeoutSeconds=timeout,
    )

    if data is None:
        return json.dumps({
            "success": False,
            "output": "",
            "error": "Bridge call failed — is the Unreal Editor running?",
        }, indent=2)

    # data should already be a parsed dict from the C++ JSON response
    if isinstance(data, dict):
        return json.dumps(data, indent=2)

    # Fallback: wrap raw string
    return json.dumps({
        "success": True,
        "output": str(data),
        "error": "",
    }, indent=2)
