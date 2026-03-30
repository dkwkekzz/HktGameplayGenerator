"""
VFX Generator Tools - MCP tools for Niagara VFX generation

Calls UHktVFXGeneratorFunctionLibrary via Remote Control API.

4-Phase Workflow:
Phase 1 — Design:
  get_vfx_prompt_guide -> get_vfx_examples -> 에미터 레이어 설계
Phase 2 — Material Prep:
  create_particle_material -> 텍스처/머티리얼 생성
Phase 3 — Build:
  build_vfx_system -> assign_vfx_material -> Niagara 빌드
Phase 4 — Preview & Refine:
  preview_vfx -> update_vfx_emitter -> 시각 검증 + 파라미터 튜닝
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


# ============================================================================
# Phase 2: Material Prep
# ============================================================================

async def create_particle_material(
    bridge: EditorBridge,
    material_name: str,
    texture_path: str = "",
    blend_mode: str = "additive",
    emissive_intensity: float = 1.0,
    output_dir: str = "",
) -> str:
    """Create a particle MaterialInstance with texture binding and emissive control"""
    logger.info(f"Creating particle material: {material_name}")
    data = await bridge.call_method(
        "McpCreateParticleMaterial",
        object_path=OBJECT_PATH,
        MaterialName=material_name,
        TexturePath=texture_path,
        BlendMode=blend_mode,
        EmissiveIntensity=emissive_intensity,
        OutputDir=output_dir,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def assign_vfx_material(
    bridge: EditorBridge,
    niagara_system_path: str,
    emitter_name: str,
    material_path: str,
) -> str:
    """Assign a material to a specific emitter in an existing Niagara system"""
    logger.info(f"Assigning material '{material_path}' to emitter '{emitter_name}'")
    data = await bridge.call_method(
        "McpAssignVFXMaterial",
        object_path=OBJECT_PATH,
        NiagaraSystemPath=niagara_system_path,
        EmitterName=emitter_name,
        MaterialPath=material_path,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


# ============================================================================
# Phase 4: Preview & Refine
# ============================================================================

async def preview_vfx(
    bridge: EditorBridge,
    niagara_system_path: str,
    duration: float = 2.0,
    screenshot_path: str = "",
) -> str:
    """Spawn VFX in viewport and capture screenshot for visual verification"""
    logger.info(f"Previewing VFX: {niagara_system_path}")
    data = await bridge.call_method(
        "McpPreviewVFX",
        object_path=OBJECT_PATH,
        NiagaraSystemPath=niagara_system_path,
        Duration=duration,
        ScreenshotPath=screenshot_path,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def update_vfx_emitter(
    bridge: EditorBridge,
    niagara_system_path: str,
    emitter_name: str,
    json_overrides: str,
) -> str:
    """Update specific emitter parameters without full rebuild (spawn/init/update sections)"""
    logger.info(f"Updating emitter '{emitter_name}' in '{niagara_system_path}'")
    data = await bridge.call_method(
        "McpUpdateVFXEmitter",
        object_path=OBJECT_PATH,
        NiagaraSystemPath=niagara_system_path,
        EmitterName=emitter_name,
        JsonOverrides=json_overrides,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)
