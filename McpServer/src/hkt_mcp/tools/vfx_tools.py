"""
VFX Generator Tools - MCP tools for Niagara VFX generation

Calls UHktVFXGeneratorFunctionLibrary via Remote Control API.

Workflow:
1. get_vfx_prompt_guide  -> Learn schema + emitter patterns + value ranges
2. get_vfx_examples      -> Study example configs
3. build_vfx_system      -> Build Niagara system from JSON config
4. list_generated_vfx    -> Verify generated assets
"""

import json
import logging
from typing import Any

from ..bridge.editor_bridge import EditorBridge

logger = logging.getLogger("hkt_mcp.tools.vfx")

OBJECT_PATH = "/Script/HktVFXGenerator.Default__HktVFXGeneratorFunctionLibrary"


async def build_vfx_system(bridge: EditorBridge, json_config: str, output_dir: str = "") -> str:
    """Build a Niagara system from JSON config"""
    logger.info("Building VFX Niagara system")
    data = await bridge.call_method(
        "McpBuildNiagaraSystem",
        object_path=OBJECT_PATH,
        JsonConfig=json_config,
        OutputDir=output_dir,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def build_preset_explosion(
    bridge: EditorBridge,
    name: str,
    r: float = 1.0, g: float = 0.5, b: float = 0.1,
    intensity: float = 0.5,
    output_dir: str = "",
) -> str:
    """Build a preset explosion VFX (for testing)"""
    logger.info(f"Building preset explosion: {name}")
    data = await bridge.call_method(
        "McpBuildPresetExplosion",
        object_path=OBJECT_PATH,
        Name=name, R=r, G=g, B=b, Intensity=intensity, OutputDir=output_dir,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def get_vfx_config_schema(bridge: EditorBridge) -> str:
    """Get VFX Config JSON schema"""
    logger.info("Getting VFX config schema")
    data = await bridge.call_method("McpGetVFXConfigSchema", object_path=OBJECT_PATH)
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def get_vfx_prompt_guide(bridge: EditorBridge) -> str:
    """Get comprehensive VFX prompt guide (schema + emitter patterns + value ranges + tips)"""
    logger.info("Getting VFX prompt guide")
    data = await bridge.call_method("McpGetVFXPromptGuide", object_path=OBJECT_PATH)
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def get_vfx_examples(bridge: EditorBridge) -> str:
    """Get example VFX configs (Explosion, Fire, Magic patterns)"""
    logger.info("Getting VFX example configs")
    data = await bridge.call_method("McpGetVFXExampleConfigs", object_path=OBJECT_PATH)
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def list_generated_vfx(bridge: EditorBridge, directory: str = "") -> str:
    """List generated VFX assets"""
    logger.info(f"Listing generated VFX in: {directory or '(default)'}")
    data = await bridge.call_method(
        "McpListGeneratedVFX", object_path=OBJECT_PATH, Directory=directory,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def dump_template_parameters(bridge: EditorBridge, renderer_type: str = "sprite") -> str:
    """Dump template emitter parameters for debugging"""
    logger.info(f"Dumping template parameters for: {renderer_type}")
    data = await bridge.call_method(
        "McpDumpTemplateParameters", object_path=OBJECT_PATH, RendererType=renderer_type,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)
