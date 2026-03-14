"""
Texture Generator Tools - MCP tools for texture generation and import

Calls UHktTextureGeneratorFunctionLibrary via Remote Control API.

Workflow:
1. generate_texture    -> Cache hit: asset path / Cache miss: pending + prompt
2. Agent generates image externally (SD/DALL-E/ComfyUI)
3. import_texture      -> Import generated image as UTexture2D
4. list_generated_textures -> Verify
"""

import json
import logging
from typing import Any

from ..bridge.editor_bridge import EditorBridge

logger = logging.getLogger("hkt_mcp.tools.texture")

OBJECT_PATH = "/Script/HktTextureGenerator.Default__HktTextureGeneratorFunctionLibrary"


async def generate_texture(bridge: EditorBridge, json_intent: str, output_dir: str = "") -> str:
    """Generate or lookup texture from intent JSON. Returns asset path (cache hit) or pending + prompt (cache miss)."""
    logger.info("Generating texture from intent")
    data = await bridge.call_method(
        "McpGenerateTexture", object_path=OBJECT_PATH,
        JsonIntent=json_intent, OutputDir=output_dir,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def import_texture(bridge: EditorBridge, image_file_path: str, json_intent: str, output_dir: str = "") -> str:
    """Import an externally generated image file as UTexture2D with auto texture settings"""
    logger.info(f"Importing texture from: {image_file_path}")
    data = await bridge.call_method(
        "McpImportTexture", object_path=OBJECT_PATH,
        ImageFilePath=image_file_path, JsonIntent=json_intent, OutputDir=output_dir,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def get_pending_texture_requests(bridge: EditorBridge, json_requests: str) -> str:
    """Filter batch requests to return only non-cached textures needing generation"""
    logger.info("Getting pending texture requests")
    data = await bridge.call_method(
        "McpGetPendingRequests", object_path=OBJECT_PATH, JsonRequests=json_requests,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def list_generated_textures(bridge: EditorBridge, directory: str = "") -> str:
    """List generated textures"""
    logger.info(f"Listing generated textures in: {directory or '(default)'}")
    data = await bridge.call_method(
        "McpListGeneratedTextures", object_path=OBJECT_PATH, Directory=directory,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)
