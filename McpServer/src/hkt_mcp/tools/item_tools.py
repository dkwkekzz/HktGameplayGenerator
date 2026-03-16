"""
Item Generator Tools - MCP tools for item (Entity.Item.*) generation and import

Calls UHktItemGeneratorFunctionLibrary via Remote Control API.

Workflow:
1. request_item       -> Get convention path + mesh/icon prompts
2. Agent generates 3D mesh + icon image externally
3. import_item_mesh   -> Import mesh into UE5
4. (icon via texture_tools.import_texture)
5. get_socket_mappings -> Item attachment socket info
"""

import json
import logging
from typing import Any

from ..bridge.editor_bridge import EditorBridge

logger = logging.getLogger("hkt_mcp.tools.item")

OBJECT_PATH = "/Script/HktItemGenerator.Default__HktItemGeneratorFunctionLibrary"


async def request_item(bridge: EditorBridge, json_intent: str) -> str:
    """Request item generation. Returns convention path + mesh/icon prompts."""
    logger.info("Requesting item generation")
    data = await bridge.call_method(
        "McpRequestItem", object_path=OBJECT_PATH, JsonIntent=json_intent,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def import_item_mesh(bridge: EditorBridge, file_path: str, json_intent: str) -> str:
    """Import an externally generated item mesh into UE5"""
    logger.info(f"Importing item mesh from: {file_path}")
    data = await bridge.call_method(
        "McpImportItemMesh", object_path=OBJECT_PATH,
        FilePath=file_path, JsonIntent=json_intent,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def get_pending_item_requests(bridge: EditorBridge) -> str:
    """List pending item requests"""
    logger.info("Getting pending item requests")
    data = await bridge.call_method("McpGetPendingItemRequests", object_path=OBJECT_PATH)
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def list_generated_items(bridge: EditorBridge, directory: str = "") -> str:
    """List generated items"""
    logger.info(f"Listing generated items in: {directory or '(default)'}")
    data = await bridge.call_method(
        "McpListGeneratedItems", object_path=OBJECT_PATH, Directory=directory,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def get_socket_mappings(bridge: EditorBridge) -> str:
    """Get item attachment socket mappings"""
    logger.info("Getting socket mappings")
    data = await bridge.call_method("McpGetSocketMappings", object_path=OBJECT_PATH)
    return json.dumps({"success": data is not None, "data": data}, indent=2)
