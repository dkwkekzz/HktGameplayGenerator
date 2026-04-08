"""
Mesh Generator Tools - MCP tools for character mesh generation and import

Calls UHktMeshGeneratorFunctionLibrary via Remote Control API.

Workflow:
1. request_character_mesh -> Get convention path + generation prompt
2. Agent generates 3D mesh externally (Meshy, Rodin, etc.)
3. import_mesh            -> Import into UE5
4. get_skeleton_pool      -> Query available base skeletons
"""

import json
import logging
from typing import Any

from ..bridge.editor_bridge import EditorBridge

logger = logging.getLogger("hkt_mcp.tools.mesh")

OBJECT_PATH = "/Script/HktMeshGenerator.Default__HktMeshGeneratorFunctionLibrary"


async def request_character_mesh(bridge: EditorBridge, json_intent: str) -> str:
    """Request character mesh generation. Returns convention path + generation prompt."""
    logger.info("Requesting character mesh generation")
    data = await bridge.call_method(
        "McpRequestCharacterMesh", object_path=OBJECT_PATH, JsonIntent=json_intent,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def import_mesh(bridge: EditorBridge, file_path: str, json_intent: str) -> str:
    """Import an externally generated mesh file into UE5"""
    logger.info(f"Importing mesh from: {file_path}")
    data = await bridge.call_method(
        "McpImportMesh", object_path=OBJECT_PATH,
        FilePath=file_path, JsonIntent=json_intent,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def get_pending_mesh_requests(bridge: EditorBridge) -> str:
    """List pending mesh requests"""
    logger.info("Getting pending mesh requests")
    data = await bridge.call_method("McpGetPendingMeshRequests", object_path=OBJECT_PATH)
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def list_generated_meshes(bridge: EditorBridge, directory: str = "") -> str:
    """List generated meshes"""
    logger.info(f"Listing generated meshes in: {directory or '(default)'}")
    data = await bridge.call_method(
        "McpListGeneratedMeshes", object_path=OBJECT_PATH, Directory=directory,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def get_skeleton_pool(bridge: EditorBridge) -> str:
    """Get available base skeleton information"""
    logger.info("Getting skeleton pool")
    data = await bridge.call_method("McpGetSkeletonPool", object_path=OBJECT_PATH)
    return json.dumps({"success": data is not None, "data": data}, indent=2)


# ============================================================================
# Shape Generator Tools (Layer 3)
# ============================================================================

async def create_shape(bridge: EditorBridge, json_params: str, output_dir: str = "") -> str:
    """Create a procedural shape StaticMesh asset for Niagara particles"""
    logger.info("Creating shape asset")
    data = await bridge.call_method(
        "McpCreateShape", object_path=OBJECT_PATH,
        JsonParams=json_params, OutputDir=output_dir,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def list_shapes(bridge: EditorBridge) -> str:
    """List all available shape assets (catalog + existing on disk)"""
    logger.info("Listing shape catalog")
    data = await bridge.call_method("McpListShapes", object_path=OBJECT_PATH)
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def find_shape(bridge: EditorBridge, shape_name: str) -> str:
    """Find a shape asset by name"""
    logger.info(f"Finding shape: {shape_name}")
    data = await bridge.call_method(
        "McpFindShape", object_path=OBJECT_PATH, ShapeName=shape_name,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)
