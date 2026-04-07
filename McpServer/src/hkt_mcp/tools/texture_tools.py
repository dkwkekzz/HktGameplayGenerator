"""
Texture Generator Tools - MCP tools for texture generation and import

Calls UHktTextureGeneratorFunctionLibrary via Remote Control API.

Workflow:
1. generate_texture    -> Cache hit: asset path / Cache miss: auto-generate via local SD WebUI (if running) or return pending + prompt
2. Agent generates image externally if needed
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

# Lazy-initialized SD WebUI client
_sd_client = None


def _get_sd_client(server_url: str = ""):
    """Get or create the SD WebUI client (lazy singleton).

    If server_url is provided (from C++ pending response), use it.
    Otherwise fall back to config.
    """
    global _sd_client
    if _sd_client is None:
        config = get_config()
        if config.sd_enabled:
            from ..sd_client import SDWebUIClient
            url = server_url or config.sd_url
            _sd_client = SDWebUIClient(
                url=url,
                timeout=config.sd_timeout,
                default_steps=config.sd_steps,
                default_cfg_scale=config.sd_cfg_scale,
                default_sampler=config.sd_sampler,
            )
    return _sd_client


async def check_sd_server_status(bridge: EditorBridge) -> str:
    """Check SD WebUI server connection status and configuration.

    Returns server alive status, URL, batch file path, and launch state.
    Also triggers an async health check on the C++ side.
    """
    logger.info("Checking SD WebUI server status")

    # Get status from C++ subsystem (includes settings info)
    data = await bridge.call_method(
        "McpCheckSDServerStatus", object_path=OBJECT_PATH,
    )

    if data is None:
        # Fall back to direct health check via SD client
        config = get_config()
        client = _get_sd_client(config.sd_url)
        if client is not None:
            try:
                alive = await client.is_alive()
                return json.dumps({
                    "success": True,
                    "alive": alive,
                    "serverURL": config.sd_url,
                    "source": "direct_check",
                }, indent=2)
            except Exception as e:
                return json.dumps({
                    "success": False,
                    "alive": False,
                    "error": str(e),
                    "serverURL": config.sd_url,
                }, indent=2)

        return json.dumps({
            "success": False,
            "error": "C++ call failed and SD client not configured",
        }, indent=2)

    return json.dumps({"success": True, "data": data}, indent=2)


async def launch_sd_server(bridge: EditorBridge) -> str:
    """Launch the SD WebUI server using the configured batch file.

    Triggers server launch on the C++ side which handles process management
    and health check polling. Monitor status via check_sd_server_status.
    """
    logger.info("Requesting SD WebUI server launch")

    # First check current status
    data = await bridge.call_method(
        "McpCheckSDServerStatus", object_path=OBJECT_PATH,
    )

    if data is None:
        return json.dumps({"success": False, "error": "C++ call failed"}, indent=2)

    if data.get("alive", False):
        return json.dumps({
            "success": True,
            "message": "Server is already running",
            "data": data,
        }, indent=2)

    if not data.get("batchFileExists", False):
        batch_path = data.get("batchFilePath", "")
        return json.dumps({
            "success": False,
            "error": f"Batch file not found or not configured: {batch_path}",
            "hint": "Configure SDWebUIBatchFilePath in Project Settings > HktGameplay > HktTextureGenerator",
            "data": data,
        }, indent=2)

    # Trigger launch via SD client (same mechanism as generate_texture)
    server_url = data.get("serverURL", "")
    batch_file = data.get("batchFilePath", "")
    client = _get_sd_client(server_url)

    if client is None:
        return json.dumps({
            "success": False,
            "error": "SD client not available (sd_auto_generate may be disabled)",
            "data": data,
        }, indent=2)

    try:
        ready = await client.ensure_running(batch_file)
        return json.dumps({
            "success": ready,
            "message": "Server is ready" if ready else "Server launch timed out",
            "alive": ready,
            "data": data,
        }, indent=2)
    except Exception as e:
        return json.dumps({
            "success": False,
            "error": f"Launch failed: {e}",
            "data": data,
        }, indent=2)


async def generate_texture(bridge: EditorBridge, json_intent: str, output_dir: str = "") -> str:
    """Generate or lookup texture. Auto-launches SD WebUI and generates on cache miss."""
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

    # Step 3: Cache miss -- try local SD WebUI auto-generation
    server_url = data.get("sdWebUIServerURL", "")
    client = _get_sd_client(server_url)
    if client is None:
        logger.info("SD WebUI not configured, returning pending response")
        return json.dumps({"success": False, "data": data}, indent=2)

    prompt = data.get("prompt", "")
    negative_prompt = data.get("negativePrompt", "")
    image_path = data.get("imagePath", "")
    resolution = data.get("resolution", 512)

    if not prompt or not image_path:
        return json.dumps({"success": False, "data": data, "error": "Missing prompt or imagePath in pending response"}, indent=2)

    try:
        # Step 3a: Ensure SD WebUI is running (auto-launch if batch file configured)
        batch_file = data.get("sdWebUIBatchFilePath", "")
        if batch_file:
            ready = await client.ensure_running(batch_file)
            if not ready:
                return json.dumps({
                    "success": False,
                    "data": data,
                    "sd_error": "Failed to launch SD WebUI server",
                }, indent=2)
        elif not await client.is_alive():
            logger.warning("SD WebUI not running and no batch file configured")
            return json.dumps({"success": False, "data": data}, indent=2)

        # Step 3b: Generate image
        logger.info("Auto-generating texture via SD WebUI: %s...", prompt[:80])

        await client.generate_image(
            prompt=prompt,
            negative_prompt=negative_prompt,
            output_path=image_path,
            width=resolution,
            height=resolution,
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
                "generated_by": "sd_webui",
            }, indent=2)
        else:
            return json.dumps({
                "success": False,
                "error": "Image generated but UE5 import failed",
                "imagePath": image_path,
                "data": data,
            }, indent=2)

    except Exception as e:
        logger.warning("SD WebUI generation failed: %s, falling back to pending", e)
        return json.dumps({
            "success": False,
            "data": data,
            "sd_error": str(e),
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
