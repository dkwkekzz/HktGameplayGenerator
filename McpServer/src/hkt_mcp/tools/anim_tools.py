"""
Animation Generator Tools - MCP tools for animation generation and import

Calls UHktAnimGeneratorFunctionLibrary via Remote Control API.

Workflow:
1. request_animation   -> Get convention path + prompt + expected type
2. Agent generates animation externally (Mixamo, Motion Diffusion, etc.)
3. import_animation    -> Import into UE5
4. list_generated_animations -> Verify
"""

import json
import logging
from typing import Any

from ..bridge.editor_bridge import EditorBridge

logger = logging.getLogger("hkt_mcp.tools.anim")

OBJECT_PATH = "/Script/HktAnimGenerator.Default__HktAnimGeneratorFunctionLibrary"


async def request_animation(bridge: EditorBridge, json_intent: str) -> str:
    """Request animation generation. Returns convention path + prompt + expected type."""
    logger.info("Requesting animation generation")
    data = await bridge.call_method(
        "McpRequestAnimation", object_path=OBJECT_PATH, JsonIntent=json_intent,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def import_animation(bridge: EditorBridge, file_path: str, json_intent: str) -> str:
    """Import an externally generated animation file into UE5"""
    logger.info(f"Importing animation from: {file_path}")
    data = await bridge.call_method(
        "McpImportAnimation", object_path=OBJECT_PATH,
        FilePath=file_path, JsonIntent=json_intent,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def get_pending_anim_requests(bridge: EditorBridge) -> str:
    """List pending animation requests"""
    logger.info("Getting pending animation requests")
    data = await bridge.call_method("McpGetPendingAnimRequests", object_path=OBJECT_PATH)
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def list_generated_animations(bridge: EditorBridge, directory: str = "") -> str:
    """List generated animations"""
    logger.info(f"Listing generated animations in: {directory or '(default)'}")
    data = await bridge.call_method(
        "McpListGeneratedAnimations", object_path=OBJECT_PATH, Directory=directory,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)
