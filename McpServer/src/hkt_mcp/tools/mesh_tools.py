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


async def create_actor_data_asset(
    bridge: EditorBridge,
    tag_string: str,
    actor_class_path: str,
    output_dir: str = "",
) -> str:
    """Create ActorVisualDataAsset linking a GameplayTag to an imported mesh/BP.

    Args:
        tag_string: GameplayTag (e.g. "Entity.Character.Goblin")
        actor_class_path: Path to the actor class or BP (e.g. "/Game/Generated/Characters/Goblin/BP_Goblin.BP_Goblin_C")
        output_dir: Output directory (optional, defaults to /Game/Generated/Characters)
    """
    logger.info(f"Creating ActorVisualDataAsset: tag={tag_string}, class={actor_class_path}")
    data = await bridge.call_method(
        "McpCreateActorDataAsset",
        object_path=OBJECT_PATH,
        TagString=tag_string,
        ActorClassPath=actor_class_path,
        OutputDir=output_dir,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)
