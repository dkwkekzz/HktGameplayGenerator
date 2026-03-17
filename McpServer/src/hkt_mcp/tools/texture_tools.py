"""
Texture Generator Tools - MCP tools for texture generation and import

Calls UHktTextureGeneratorFunctionLibrary via Remote Control API.

Workflow:
1. generate_texture    -> Cache hit: asset path / Cache miss: auto-generate via Stability AI (if configured) or return pending + prompt
2. Agent generates image externally if needed (SD/DALL-E/ComfyUI)
3. import_texture      -> Import generated image as UTexture2D
4. list_generated_textures -> Verify
"""

import json
import logging
from typing import Any

from ..bridge.editor_bridge import EditorBridge
from ..config import get_config

logger = logging.getLogger("hkt_mcp.tools.texture")

OBJECT_PATH = "/Script/HktTextureGenerator.Default__HktTextureGeneratorFunctionLibrary"

# Lazy-initialized Stability AI client
_stability_client = None


def _get_stability_client():
    """Get or create the Stability AI client (lazy singleton)."""
    global _stability_client
    if _stability_client is None:
        config = get_config()
        if config.stability_ai_enabled:
            from ..stability_client import StabilityClient
            _stability_client = StabilityClient(
                api_key=config.stability_ai_api_key,
                model=config.stability_ai_model,
                timeout=config.stability_ai_timeout,
            )
    return _stability_client


async def generate_texture(bridge: EditorBridge, json_intent: str, output_dir: str = "") -> str:
    """Generate or lookup texture. Auto-generates via Stability AI on cache miss if configured."""
    logger.info("Generating texture from intent")

    # Step 1: Check cache via C++
    data = await bridge.call_method(
        "McpGenerateTexture", object_path=OBJECT_PATH,
        JsonIntent=json_intent, OutputDir=output_dir,
    )

    if data is None:
        return json.dumps({"success": False, "error": "C++ call failed"}, indent=2)

    # Step 2: If cache hit, return immediately
    if data.get("success", False) or not data.get("pending", False):
        return json.dumps({"success": True, "data": data}, indent=2)

    # Step 3: Cache miss -- try Stability AI auto-generation
    client = _get_stability_client()
    if client is None:
        logger.info("Stability AI not configured, returning pending response")
        return json.dumps({"success": False, "data": data}, indent=2)

    prompt = data.get("prompt", "")
    negative_prompt = data.get("negativePrompt", "")
    image_path = data.get("imagePath", "")
    resolution = data.get("resolution", 512)

    if not prompt or not image_path:
        return json.dumps({"success": False, "data": data, "error": "Missing prompt or imagePath in pending response"}, indent=2)

    try:
        logger.info("Auto-generating texture via Stability AI: %s...", prompt[:80])
        from ..stability_client import resolution_to_aspect_ratio
        aspect_ratio = resolution_to_aspect_ratio(resolution)

        await client.generate_image(
            prompt=prompt,
            negative_prompt=negative_prompt,
            output_path=image_path,
            aspect_ratio=aspect_ratio,
        )

        # Step 4: Import the generated image into UE5
        logger.info("Importing generated texture: %s", image_path)
        import_data = await bridge.call_method(
            "McpImportTexture", object_path=OBJECT_PATH,
            ImageFilePath=image_path, JsonIntent=json_intent, OutputDir=output_dir,
        )

        if import_data is not None:
            return json.dumps({
                "success": True,
                "data": import_data,
                "generated_by": "stability_ai",
            }, indent=2)
        else:
            return json.dumps({
                "success": False,
                "error": "Image generated but UE5 import failed",
                "imagePath": image_path,
                "data": data,
            }, indent=2)

    except Exception as e:
        logger.warning("Stability AI generation failed: %s, falling back to pending", e)
        return json.dumps({
            "success": False,
            "data": data,
            "stability_error": str(e),
        }, indent=2)


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
