"""
Animation Generator Tools - MCP tools for animation generation, ABP, StateMachine, Montage, BlendSpace, Skeleton

Calls UHktAnimGeneratorFunctionLibrary via Remote Control API.

=== Tool Categories ===
1. Generation: request/import/list animation assets
2. ABP: create/inspect/compile Animation Blueprints
3. StateMachine: add state machines, states, transitions
4. AnimGraph: add nodes, connect pins, add parameters
5. Montage: create montages, add sections, set slots
6. BlendSpace: create blend spaces, add samples, set axes
7. Skeleton: inspect bones, add sockets, add virtual bones
8. Guide: AI agent API usage guide
"""

import json
import logging
from typing import Any

from ..bridge.editor_bridge import EditorBridge

logger = logging.getLogger("hkt_mcp.tools.anim")

OBJECT_PATH = "/Script/HktAnimGenerator.Default__HktAnimGeneratorFunctionLibrary"


# ============================================================================
# 기존 Generation API
# ============================================================================

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


# ============================================================================
# Animation Blueprint API
# ============================================================================

async def create_anim_blueprint(bridge: EditorBridge, json_config: str) -> str:
    """Create an Animation Blueprint with specified skeleton."""
    logger.info("Creating AnimBlueprint")
    data = await bridge.call_method(
        "McpCreateAnimBlueprint", object_path=OBJECT_PATH, JsonConfig=json_config,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def get_anim_blueprint_info(bridge: EditorBridge, asset_path: str) -> str:
    """Get AnimBlueprint info — graphs, nodes, state machines, parameters."""
    logger.info(f"Getting ABP info: {asset_path}")
    data = await bridge.call_method(
        "McpGetAnimBlueprintInfo", object_path=OBJECT_PATH, AssetPath=asset_path,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def compile_anim_blueprint(bridge: EditorBridge, asset_path: str) -> str:
    """Compile an Animation Blueprint."""
    logger.info(f"Compiling ABP: {asset_path}")
    data = await bridge.call_method(
        "McpCompileAnimBlueprint", object_path=OBJECT_PATH, AssetPath=asset_path,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


# ============================================================================
# State Machine API
# ============================================================================

async def add_state_machine(bridge: EditorBridge, json_config: str) -> str:
    """Add a State Machine to an AnimBlueprint's AnimGraph."""
    logger.info("Adding State Machine")
    data = await bridge.call_method(
        "McpAddStateMachine", object_path=OBJECT_PATH, JsonConfig=json_config,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def add_state(bridge: EditorBridge, json_config: str) -> str:
    """Add a State to a State Machine."""
    logger.info("Adding State")
    data = await bridge.call_method(
        "McpAddState", object_path=OBJECT_PATH, JsonConfig=json_config,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def add_transition(bridge: EditorBridge, json_config: str) -> str:
    """Add a Transition between States."""
    logger.info("Adding Transition")
    data = await bridge.call_method(
        "McpAddTransition", object_path=OBJECT_PATH, JsonConfig=json_config,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def connect_state_machine_to_output(bridge: EditorBridge, json_config: str) -> str:
    """Connect a State Machine to the AnimGraph OutputPose."""
    logger.info("Connecting StateMachine to OutputPose")
    data = await bridge.call_method(
        "McpConnectStateMachineToOutput", object_path=OBJECT_PATH, JsonConfig=json_config,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def set_state_animation(bridge: EditorBridge, json_config: str) -> str:
    """Set animation asset for a State."""
    logger.info("Setting State animation")
    data = await bridge.call_method(
        "McpSetStateAnimation", object_path=OBJECT_PATH, JsonConfig=json_config,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


# ============================================================================
# AnimGraph Node API
# ============================================================================

async def add_anim_graph_node(bridge: EditorBridge, json_config: str) -> str:
    """Add a node to the AnimGraph (SequencePlayer, BlendListByBool, etc.)."""
    logger.info("Adding AnimGraph node")
    data = await bridge.call_method(
        "McpAddAnimGraphNode", object_path=OBJECT_PATH, JsonConfig=json_config,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def connect_anim_nodes(bridge: EditorBridge, json_config: str) -> str:
    """Connect two AnimGraph nodes via pins."""
    logger.info("Connecting AnimGraph nodes")
    data = await bridge.call_method(
        "McpConnectAnimNodes", object_path=OBJECT_PATH, JsonConfig=json_config,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def add_anim_parameter(bridge: EditorBridge, json_config: str) -> str:
    """Add a Bool/Float/Int parameter to an AnimBlueprint."""
    logger.info("Adding AnimBlueprint parameter")
    data = await bridge.call_method(
        "McpAddAnimParameter", object_path=OBJECT_PATH, JsonConfig=json_config,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


# ============================================================================
# Montage API
# ============================================================================

async def create_montage(bridge: EditorBridge, json_config: str) -> str:
    """Create an AnimMontage from an AnimSequence."""
    logger.info("Creating Montage")
    data = await bridge.call_method(
        "McpCreateMontage", object_path=OBJECT_PATH, JsonConfig=json_config,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def add_montage_section(bridge: EditorBridge, json_config: str) -> str:
    """Add a section to a Montage."""
    logger.info("Adding Montage section")
    data = await bridge.call_method(
        "McpAddMontageSection", object_path=OBJECT_PATH, JsonConfig=json_config,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def set_montage_slot(bridge: EditorBridge, asset_path: str, slot_name: str) -> str:
    """Set the slot name for a Montage."""
    logger.info(f"Setting Montage slot: {slot_name}")
    data = await bridge.call_method(
        "McpSetMontageSlot", object_path=OBJECT_PATH,
        AssetPath=asset_path, SlotName=slot_name,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def link_montage_sections(bridge: EditorBridge, json_config: str) -> str:
    """Link two Montage sections (set next section)."""
    logger.info("Linking Montage sections")
    data = await bridge.call_method(
        "McpLinkMontageSections", object_path=OBJECT_PATH, JsonConfig=json_config,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


# ============================================================================
# BlendSpace API
# ============================================================================

async def create_blend_space(bridge: EditorBridge, json_config: str) -> str:
    """Create a BlendSpace (1D or 2D)."""
    logger.info("Creating BlendSpace")
    data = await bridge.call_method(
        "McpCreateBlendSpace", object_path=OBJECT_PATH, JsonConfig=json_config,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def add_blend_space_sample(bridge: EditorBridge, json_config: str) -> str:
    """Add a sample point to a BlendSpace."""
    logger.info("Adding BlendSpace sample")
    data = await bridge.call_method(
        "McpAddBlendSpaceSample", object_path=OBJECT_PATH, JsonConfig=json_config,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def set_blend_space_axis(bridge: EditorBridge, json_config: str) -> str:
    """Set axis parameters for a BlendSpace."""
    logger.info("Setting BlendSpace axis")
    data = await bridge.call_method(
        "McpSetBlendSpaceAxis", object_path=OBJECT_PATH, JsonConfig=json_config,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


# ============================================================================
# Skeleton API
# ============================================================================

async def get_skeleton_info(bridge: EditorBridge, skeleton_path: str) -> str:
    """Get skeleton info (bones, sockets, virtual bones)."""
    logger.info(f"Getting skeleton info: {skeleton_path}")
    data = await bridge.call_method(
        "McpGetSkeletonInfo", object_path=OBJECT_PATH, SkeletonPath=skeleton_path,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def add_socket(bridge: EditorBridge, json_config: str) -> str:
    """Add a socket to a skeleton."""
    logger.info("Adding socket")
    data = await bridge.call_method(
        "McpAddSocket", object_path=OBJECT_PATH, JsonConfig=json_config,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def add_virtual_bone(bridge: EditorBridge, json_config: str) -> str:
    """Add a virtual bone to a skeleton."""
    logger.info("Adding virtual bone")
    data = await bridge.call_method(
        "McpAddVirtualBone", object_path=OBJECT_PATH, JsonConfig=json_config,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


# ============================================================================
# Guide API
# ============================================================================

async def get_anim_api_guide(bridge: EditorBridge) -> str:
    """Get the Animation API usage guide for AI agents."""
    logger.info("Getting Animation API guide")
    data = await bridge.call_method("McpGetAnimApiGuide", object_path=OBJECT_PATH)
    return json.dumps({"success": data is not None, "data": data}, indent=2)
