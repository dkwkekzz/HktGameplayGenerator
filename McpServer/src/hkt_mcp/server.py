"""
HKT MCP Server - Main Entry Point

This server provides MCP (Model Context Protocol) tools for LLM clients
(Claude Desktop, Cursor, etc.) to interact with Unreal Engine 5.

Architecture:
- Editor Mode: Uses unreal module (Python API) directly
- Runtime Mode: Connects via WebSocket to UE5 game instance
"""

import asyncio
import logging
import os
import sys
from typing import Any

from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp.types import Tool, TextContent, Resource, Prompt

# Import tool modules
from .tools import asset_tools, level_tools, query_tools, runtime_tools
from .tools import vfx_tools, story_tools, texture_tools, anim_tools, mesh_tools, item_tools
from .tools import step_tools, map_tools, python_tools
from .bridge import editor_bridge, runtime_bridge
from .bridge.monolith_client import MonolithClient

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger("hkt_mcp")

# Create MCP server instance
server = Server("hkt-unreal")

# Global bridge instances
_editor_bridge: editor_bridge.EditorBridge | None = None
_runtime_bridge: runtime_bridge.RuntimeBridge | None = None
_monolith_client: MonolithClient | None = None


def get_editor_bridge() -> editor_bridge.EditorBridge:
    """Get or create the editor bridge instance"""
    global _editor_bridge
    if _editor_bridge is None:
        _editor_bridge = editor_bridge.EditorBridge()
    return _editor_bridge


def get_runtime_bridge() -> runtime_bridge.RuntimeBridge:
    """Get or create the runtime bridge instance"""
    global _runtime_bridge
    if _runtime_bridge is None:
        _runtime_bridge = runtime_bridge.RuntimeBridge()
    return _runtime_bridge


def _get_monolith_client() -> MonolithClient:
    """Get or create the monolith proxy client"""
    global _monolith_client
    if _monolith_client is None:
        from .config import get_config
        config = get_config()
        _monolith_client = MonolithClient(mcp_url=config.monolith_url)
    return _monolith_client


# ==================== Tool Definitions ====================

def _get_hkt_tools() -> list[Tool]:
    """Return hkt-unreal native tools"""
    tools = []
    
    # Asset Tools
    tools.extend([
        Tool(
            name="list_assets",
            description="List assets in a specified path. Returns asset names, classes, and paths.",
            inputSchema={
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "Asset path to search (e.g., '/Game/Blueprints')"
                    },
                    "class_filter": {
                        "type": "string",
                        "description": "Optional class name to filter (e.g., 'Blueprint', 'StaticMesh')"
                    }
                },
                "required": ["path"]
            }
        ),
        Tool(
            name="get_asset_info",
            description="Get detailed information about a specific asset",
            inputSchema={
                "type": "object",
                "properties": {
                    "asset_path": {
                        "type": "string",
                        "description": "Full asset path (e.g., '/Game/Blueprints/BP_Player')"
                    }
                },
                "required": ["asset_path"]
            }
        ),
        Tool(
            name="search_assets",
            description="Search for assets by name.",
            inputSchema={
                "type": "object",
                "properties": {
                    "query": {
                        "type": "string",
                        "description": "Search query string"
                    },
                    "class_filter": {
                        "type": "string",
                        "description": "Optional class name to filter results"
                    }
                },
                "required": ["query"]
            }
        ),
        Tool(
            name="modify_asset",
            description="Modify a property of an asset",
            inputSchema={
                "type": "object",
                "properties": {
                    "asset_path": {
                        "type": "string",
                        "description": "Full asset path"
                    },
                    "property_name": {
                        "type": "string",
                        "description": "Property name to modify"
                    },
                    "new_value": {
                        "type": "string",
                        "description": "New value for the property"
                    }
                },
                "required": ["asset_path", "property_name", "new_value"]
            }
        ),
        Tool(
            name="create_data_asset_with_properties",
            description="Create any UDataAsset subclass and set properties in one call. "
                        "Supports FGameplayTag ('Entity.Character.Goblin'), FSoftObjectPath, TSoftObjectPtr, "
                        "hard UObject* refs, FGameplayTagContainer ('Tag.A, Tag.B'), int/float/bool/string/enum.",
            inputSchema={
                "type": "object",
                "properties": {
                    "asset_path": {
                        "type": "string",
                        "description": "UE5 asset path (e.g. /Game/Generated/DA_MyAsset)"
                    },
                    "parent_class": {
                        "type": "string",
                        "description": "Full class path (e.g. /Script/HktAsset.HktVFXVisualDataAsset)"
                    },
                    "properties": {
                        "type": "object",
                        "description": "Property name-value pairs to set (e.g. {\"IdentifierTag\": \"VFX.Fire\", \"NiagaraSystem\": \"/Game/VFX/NS_Fire.NS_Fire\"})",
                        "additionalProperties": {"type": "string"}
                    }
                },
                "required": ["asset_path", "parent_class"]
            }
        ),
    ])

    # Level Tools
    tools.extend([
        Tool(
            name="list_actors",
            description="List all actors in the current level",
            inputSchema={
                "type": "object",
                "properties": {
                    "class_filter": {
                        "type": "string",
                        "description": "Optional class name to filter actors"
                    }
                }
            }
        ),
        Tool(
            name="spawn_actor",
            description="Spawn a new actor in the level from a Blueprint",
            inputSchema={
                "type": "object",
                "properties": {
                    "blueprint_path": {
                        "type": "string",
                        "description": "Path to the Blueprint asset"
                    },
                    "location": {
                        "type": "object",
                        "properties": {
                            "x": {"type": "number"},
                            "y": {"type": "number"},
                            "z": {"type": "number"}
                        },
                        "required": ["x", "y", "z"],
                        "description": "Spawn location"
                    },
                    "rotation": {
                        "type": "object",
                        "properties": {
                            "pitch": {"type": "number"},
                            "yaw": {"type": "number"},
                            "roll": {"type": "number"}
                        },
                        "description": "Spawn rotation (optional)"
                    },
                    "label": {
                        "type": "string",
                        "description": "Actor label (optional)"
                    }
                },
                "required": ["blueprint_path", "location"]
            }
        ),
        Tool(
            name="modify_actor",
            description="Modify an actor's transform or properties",
            inputSchema={
                "type": "object",
                "properties": {
                    "actor_name": {
                        "type": "string",
                        "description": "Name or label of the actor"
                    },
                    "location": {
                        "type": "object",
                        "properties": {
                            "x": {"type": "number"},
                            "y": {"type": "number"},
                            "z": {"type": "number"}
                        },
                        "description": "New location (optional)"
                    },
                    "rotation": {
                        "type": "object",
                        "properties": {
                            "pitch": {"type": "number"},
                            "yaw": {"type": "number"},
                            "roll": {"type": "number"}
                        },
                        "description": "New rotation (optional)"
                    },
                    "scale": {
                        "type": "object",
                        "properties": {
                            "x": {"type": "number"},
                            "y": {"type": "number"},
                            "z": {"type": "number"}
                        },
                        "description": "New scale (optional)"
                    }
                },
                "required": ["actor_name"]
            }
        ),
        Tool(
            name="delete_actor",
            description="Delete an actor from the level",
            inputSchema={
                "type": "object",
                "properties": {
                    "actor_name": {
                        "type": "string",
                        "description": "Name or label of the actor to delete"
                    }
                },
                "required": ["actor_name"]
            }
        ),
        Tool(
            name="select_actor",
            description="Select an actor in the editor viewport",
            inputSchema={
                "type": "object",
                "properties": {
                    "actor_name": {
                        "type": "string",
                        "description": "Name or label of the actor to select"
                    }
                },
                "required": ["actor_name"]
            }
        ),
    ])
    
    # Query Tools
    tools.extend([
        Tool(
            name="search_classes",
            description="Search for classes by name.",
            inputSchema={
                "type": "object",
                "properties": {
                    "query": {
                        "type": "string",
                        "description": "Search query"
                    },
                    "blueprint_only": {
                        "type": "boolean",
                        "description": "Search only Blueprint classes"
                    }
                },
                "required": ["query"]
            }
        ),
        Tool(
            name="get_class_properties",
            description="Get all properties of a class.",
            inputSchema={
                "type": "object",
                "properties": {
                    "class_name": {
                        "type": "string",
                        "description": "Name of the class"
                    }
                },
                "required": ["class_name"]
            }
        ),
        Tool(
            name="get_project_structure",
            description="Get the folder structure of the project.",
            inputSchema={
                "type": "object",
                "properties": {
                    "root_path": {
                        "type": "string",
                        "description": "Root path to start from (default: '/Game')"
                    }
                }
            }
        ),
        Tool(
            name="get_level_info",
            description="Get information about the current level",
            inputSchema={
                "type": "object",
                "properties": {}
            }
        ),
    ])
    
    # Runtime Tools
    tools.extend([
        Tool(
            name="start_pie",
            description="Start Play In Editor (PIE) session",
            inputSchema={
                "type": "object",
                "properties": {}
            }
        ),
        Tool(
            name="stop_pie",
            description="Stop the current PIE session",
            inputSchema={
                "type": "object",
                "properties": {}
            }
        ),
        Tool(
            name="execute_console_command",
            description="Execute a console command in the editor",
            inputSchema={
                "type": "object",
                "properties": {
                    "command": {
                        "type": "string",
                        "description": "Console command to execute"
                    }
                },
                "required": ["command"]
            }
        ),
        Tool(
            name="get_game_state",
            description="Get current game state (requires running PIE or connected runtime)",
            inputSchema={
                "type": "object",
                "properties": {}
            }
        ),
    ])
    
    # Editor Utility Tools
    tools.extend([
        Tool(
            name="get_viewport_camera",
            description="Get the current viewport camera position and rotation",
            inputSchema={
                "type": "object",
                "properties": {}
            }
        ),
        Tool(
            name="set_viewport_camera",
            description="Set the viewport camera position and rotation",
            inputSchema={
                "type": "object",
                "properties": {
                    "location": {
                        "type": "object",
                        "properties": {
                            "x": {"type": "number"},
                            "y": {"type": "number"},
                            "z": {"type": "number"}
                        },
                        "required": ["x", "y", "z"]
                    },
                    "rotation": {
                        "type": "object",
                        "properties": {
                            "pitch": {"type": "number"},
                            "yaw": {"type": "number"},
                            "roll": {"type": "number"}
                        },
                        "required": ["pitch", "yaw", "roll"]
                    }
                },
                "required": ["location", "rotation"]
            }
        ),
        Tool(
            name="show_notification",
            description="Show a notification in the editor",
            inputSchema={
                "type": "object",
                "properties": {
                    "message": {
                        "type": "string",
                        "description": "Message to display"
                    },
                    "duration": {
                        "type": "number",
                        "description": "Duration in seconds (default: 3.0)"
                    }
                },
                "required": ["message"]
            }
        ),
    ])

    # ==================== VFX Generator Tools ====================
    tools.extend([
        Tool(
            name="build_vfx_system",
            description="Build a complete Niagara VFX system from JSON config via HktVFXGenerator. Creates full system from scratch using FHktVFXNiagaraConfig. Call get_vfx_prompt_guide first.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {
                        "type": "string",
                        "description": "Full VFX Niagara config as JSON string (FHktVFXNiagaraConfig format)"
                    },
                    "output_dir": {
                        "type": "string",
                        "description": "Output directory for the asset (default: auto)"
                    }
                },
                "required": ["json_config"]
            }
        ),
        Tool(
            name="build_preset_explosion",
            description="Build a preset explosion VFX (quick test). For custom VFX use build_vfx_system instead.",
            inputSchema={
                "type": "object",
                "properties": {
                    "name": {"type": "string", "description": "Explosion name"},
                    "r": {"type": "number", "description": "Red (0-1, default 1.0)"},
                    "g": {"type": "number", "description": "Green (0-1, default 0.5)"},
                    "b": {"type": "number", "description": "Blue (0-1, default 0.1)"},
                    "intensity": {"type": "number", "description": "Effect intensity (0-1, default 0.5)"},
                    "output_dir": {"type": "string", "description": "Output directory"}
                },
                "required": ["name"]
            }
        ),
        Tool(
            name="get_vfx_prompt_guide",
            description="Get comprehensive VFX design guide: JSON schema + emitter layer patterns + value ranges + tips. Read this BEFORE creating VFX configs.",
            inputSchema={"type": "object", "properties": {}}
        ),
        Tool(
            name="get_vfx_examples",
            description="Get example VFX config JSONs (Explosion, Fire, Magic patterns) for learning.",
            inputSchema={"type": "object", "properties": {}}
        ),
        Tool(
            name="get_vfx_config_schema",
            description="Get the JSON schema for FHktVFXNiagaraConfig structure.",
            inputSchema={"type": "object", "properties": {}}
        ),
        Tool(
            name="list_generated_vfx",
            description="List generated VFX assets in a directory.",
            inputSchema={
                "type": "object",
                "properties": {
                    "directory": {"type": "string", "description": "Directory to search (default: auto)"}
                }
            }
        ),
        Tool(
            name="dump_vfx_template_parameters",
            description="Dump template emitter parameters for debugging.",
            inputSchema={
                "type": "object",
                "properties": {
                    "renderer_type": {
                        "type": "string",
                        "description": "Renderer type: sprite, ribbon, light, mesh (default: sprite)"
                    }
                }
            }
        ),
        # --- Phase 2: Material Prep ---
        Tool(
            name="create_particle_material",
            description="Create a particle MaterialInstance from master material with texture binding and emissive control. Use in Phase 2 (Material Prep) before building Niagara system.",
            inputSchema={
                "type": "object",
                "properties": {
                    "material_name": {
                        "type": "string",
                        "description": "Name for the new MaterialInstance (will be prefixed with MI_)"
                    },
                    "texture_path": {
                        "type": "string",
                        "description": "UE asset path to texture (e.g. /Game/Generated/Textures/T_Fire). Empty = no texture."
                    },
                    "blend_mode": {
                        "type": "string",
                        "enum": ["additive", "translucent"],
                        "description": "Blend mode: additive (default, glow/fire) or translucent (smoke/dust)"
                    },
                    "emissive_intensity": {
                        "type": "number",
                        "description": "Emissive multiplier (default 1.0, use >1 for HDR glow)"
                    },
                    "output_dir": {
                        "type": "string",
                        "description": "Output directory (default: auto)"
                    }
                },
                "required": ["material_name"]
            }
        ),
        Tool(
            name="assign_vfx_material",
            description="Assign a material to a specific emitter in an existing Niagara system. Use after Phase 3 build or to swap materials on existing VFX.",
            inputSchema={
                "type": "object",
                "properties": {
                    "niagara_system_path": {
                        "type": "string",
                        "description": "UE asset path to the Niagara system"
                    },
                    "emitter_name": {
                        "type": "string",
                        "description": "Name of the emitter to update"
                    },
                    "material_path": {
                        "type": "string",
                        "description": "UE asset path to the material to assign"
                    }
                },
                "required": ["niagara_system_path", "emitter_name", "material_path"]
            }
        ),
        # --- Phase 4: Preview & Refine ---
        Tool(
            name="preview_vfx",
            description="Spawn VFX in editor viewport, capture screenshot for visual verification. Returns screenshot file path. Use in Phase 4 to validate VFX quality.",
            inputSchema={
                "type": "object",
                "properties": {
                    "niagara_system_path": {
                        "type": "string",
                        "description": "UE asset path to the Niagara system to preview"
                    },
                    "duration": {
                        "type": "number",
                        "description": "How long to keep VFX alive in seconds (default 2.0)"
                    },
                    "screenshot_path": {
                        "type": "string",
                        "description": "Custom screenshot save path (default: auto-generated in Saved/VFXPreviews/)"
                    }
                },
                "required": ["niagara_system_path"]
            }
        ),
        Tool(
            name="update_vfx_emitter",
            description="Update specific emitter parameters in an existing Niagara system without full rebuild. Supports spawn/init/update section overrides. Use in Phase 4 refinement loop.",
            inputSchema={
                "type": "object",
                "properties": {
                    "niagara_system_path": {
                        "type": "string",
                        "description": "UE asset path to the Niagara system"
                    },
                    "emitter_name": {
                        "type": "string",
                        "description": "Name of the emitter to update"
                    },
                    "json_overrides": {
                        "type": "string",
                        "description": "JSON with section overrides: {\"spawn\":{...}, \"init\":{...}, \"update\":{...}}"
                    }
                },
                "required": ["niagara_system_path", "emitter_name", "json_overrides"]
            }
        ),
    ])

    # ==================== Story Generator Tools ====================
    tools.extend([
        Tool(
            name="get_story_schema",
            description="Get Story JSON schema with all operations, registers, and entity properties. Read this BEFORE writing stories.",
            inputSchema={"type": "object", "properties": {}}
        ),
        Tool(
            name="get_story_examples",
            description="Get story pattern examples (BasicAttack, Fireball, Spawn, Wave) for learning.",
            inputSchema={"type": "object", "properties": {}}
        ),
        Tool(
            name="validate_story",
            description="Validate Story JSON syntax without registering to VM.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_story": {"type": "string", "description": "Story JSON to validate"}
                },
                "required": ["json_story"]
            }
        ),
        Tool(
            name="analyze_story_dependencies",
            description="Analyze story dependencies: identify required asset tags and trigger appropriate generators.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_story": {"type": "string", "description": "Story JSON to analyze"}
                },
                "required": ["json_story"]
            }
        ),
        Tool(
            name="build_story",
            description="Compile Story JSON and register to the bytecode VM.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_story": {"type": "string", "description": "Story JSON to compile and register"}
                },
                "required": ["json_story"]
            }
        ),
        Tool(
            name="list_stories",
            description="List all dynamically registered stories.",
            inputSchema={"type": "object", "properties": {}}
        ),
    ])

    # ==================== Texture Generator Tools ====================
    tools.extend([
        Tool(
            name="check_sd_server_status",
            description="Check SD WebUI server connection status. Returns alive state, server URL, batch file path, and configuration. Call this before generating textures to verify the server is ready.",
            inputSchema={
                "type": "object",
                "properties": {}
            }
        ),
        Tool(
            name="launch_sd_server",
            description="Launch the SD WebUI server using the configured batch file. Blocks until the server is ready or times out. Use check_sd_server_status first to see if the server is already running.",
            inputSchema={
                "type": "object",
                "properties": {}
            }
        ),
        Tool(
            name="generate_texture",
            description="Generate or lookup texture. Cache hit returns asset path; cache miss auto-generates via local SD WebUI (if running) or returns pending + prompt for external generation.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_intent": {
                        "type": "string",
                        "description": "Texture intent JSON (FHktTextureIntent format: usage, prompt, negativePrompt, resolution, etc.)"
                    },
                    "output_dir": {"type": "string", "description": "Output directory (default: auto)"}
                },
                "required": ["json_intent"]
            }
        ),
        Tool(
            name="import_texture",
            description="Import an externally generated image file as UTexture2D with auto texture settings based on usage.",
            inputSchema={
                "type": "object",
                "properties": {
                    "image_file_path": {"type": "string", "description": "Path to the image file on disk"},
                    "json_intent": {"type": "string", "description": "Texture intent JSON"},
                    "output_dir": {"type": "string", "description": "Output directory (default: auto)"}
                },
                "required": ["image_file_path", "json_intent"]
            }
        ),
        Tool(
            name="get_pending_texture_requests",
            description="Filter batch texture requests to return only non-cached textures needing generation.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_requests": {"type": "string", "description": "JSON array of texture request intents"}
                },
                "required": ["json_requests"]
            }
        ),
        Tool(
            name="list_generated_textures",
            description="List generated textures in a directory.",
            inputSchema={
                "type": "object",
                "properties": {
                    "directory": {"type": "string", "description": "Directory to search (default: auto)"}
                }
            }
        ),
    ])

    # ==================== Animation Generator Tools ====================
    tools.extend([
        Tool(
            name="request_animation",
            description="Request animation generation via HktAnimGenerator. Returns convention path + prompt for external tools.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_intent": {"type": "string", "description": "Animation intent JSON (tag, layer, type, skeletonType, etc.)"}
                },
                "required": ["json_intent"]
            }
        ),
        Tool(
            name="import_animation",
            description="Import an externally generated animation file into UE5.",
            inputSchema={
                "type": "object",
                "properties": {
                    "file_path": {"type": "string", "description": "Path to the animation file (FBX, etc.)"},
                    "json_intent": {"type": "string", "description": "Animation intent JSON"}
                },
                "required": ["file_path", "json_intent"]
            }
        ),
        Tool(
            name="get_pending_anim_requests",
            description="List pending animation generation requests.",
            inputSchema={"type": "object", "properties": {}}
        ),
        Tool(
            name="list_generated_animations",
            description="List generated animations in a directory.",
            inputSchema={
                "type": "object",
                "properties": {
                    "directory": {"type": "string", "description": "Directory to search (default: auto)"}
                }
            }
        ),
    ])

    # ==================== Animation Blueprint & Advanced Tools ====================
    tools.extend([
        # --- ABP ---
        Tool(
            name="get_anim_api_guide",
            description="Get the Animation API usage guide. Call this first to understand available tools, workflows, and JSON schemas for ABP/StateMachine/Montage/BlendSpace/Skeleton.",
            inputSchema={"type": "object", "properties": {}}
        ),
        Tool(
            name="create_anim_blueprint",
            description="Create an Animation Blueprint with a specified skeleton.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {"type": "string", "description": "JSON: {name, packagePath?, skeletonPath}"}
                },
                "required": ["json_config"]
            }
        ),
        Tool(
            name="get_anim_blueprint_info",
            description="Get AnimBlueprint structure: graphs, nodes, state machines, pins, parameters.",
            inputSchema={
                "type": "object",
                "properties": {
                    "asset_path": {"type": "string", "description": "Asset path of the AnimBlueprint"}
                },
                "required": ["asset_path"]
            }
        ),
        Tool(
            name="compile_anim_blueprint",
            description="Compile an Animation Blueprint. Returns compilation status and errors.",
            inputSchema={
                "type": "object",
                "properties": {
                    "asset_path": {"type": "string", "description": "Asset path of the AnimBlueprint"}
                },
                "required": ["asset_path"]
            }
        ),
        # --- State Machine ---
        Tool(
            name="add_state_machine",
            description="Add a State Machine node to an AnimBlueprint's AnimGraph.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {"type": "string", "description": "JSON: {abpPath, machineName, posX?, posY?}"}
                },
                "required": ["json_config"]
            }
        ),
        Tool(
            name="add_state",
            description="Add a State to a State Machine. Optionally set animation asset. First state auto-connects to Entry.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {"type": "string", "description": "JSON: {abpPath, machineName, stateName, animAssetPath?, posX?, posY?}"}
                },
                "required": ["json_config"]
            }
        ),
        Tool(
            name="add_transition",
            description="Add a Transition between two States with optional transition rule.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {"type": "string", "description": "JSON: {abpPath, machineName, fromState, toState, transitionRule?: {type: automatic|timeRemaining|boolParam, crossfadeDuration?, paramName?}}"}
                },
                "required": ["json_config"]
            }
        ),
        Tool(
            name="connect_state_machine_to_output",
            description="Connect a State Machine's output to the AnimGraph OutputPose (Result node).",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {"type": "string", "description": "JSON: {abpPath, machineName}"}
                },
                "required": ["json_config"]
            }
        ),
        Tool(
            name="set_state_animation",
            description="Set or change the animation asset for a State in a State Machine.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {"type": "string", "description": "JSON: {abpPath, machineName, stateName, animAssetPath}"}
                },
                "required": ["json_config"]
            }
        ),
        # --- AnimGraph Nodes ---
        Tool(
            name="add_anim_graph_node",
            description="Add a node to the AnimGraph. Types: SequencePlayer, BlendListByBool, TwoWayBlend, LayeredBoneBlend, SaveCachedPose, UseCachedPose.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {"type": "string", "description": "JSON: {abpPath, nodeType, posX?, posY?, animAssetPath?(SequencePlayer), cacheName?(SaveCachedPose)}"}
                },
                "required": ["json_config"]
            }
        ),
        Tool(
            name="connect_anim_nodes",
            description="Connect two AnimGraph nodes via their pins (use nodeId from get_anim_blueprint_info).",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {"type": "string", "description": "JSON: {abpPath, sourceNodeId, sourcePinName, targetNodeId, targetPinName}"}
                },
                "required": ["json_config"]
            }
        ),
        Tool(
            name="add_anim_parameter",
            description="Add a Bool/Float/Int parameter (variable) to an AnimBlueprint.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {"type": "string", "description": "JSON: {abpPath, paramName, paramType: bool|float|int, defaultValue?}"}
                },
                "required": ["json_config"]
            }
        ),
        # --- Montage ---
        Tool(
            name="create_montage",
            description="Create an AnimMontage. Supports single anim (animSequencePath) or multi-anim with auto-sections (animSequences array).",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {"type": "string", "description": "JSON: {name, packagePath?, slotName?, animSequencePath? (single), animSequences?: [{name, path}] (multi — each becomes a Section), sections?: [{name, startTime}]}"}
                },
                "required": ["json_config"]
            }
        ),
        Tool(
            name="add_montage_section",
            description="Add a section to a Montage at a specific time.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {"type": "string", "description": "JSON: {montagePath, sectionName, startTime}"}
                },
                "required": ["json_config"]
            }
        ),
        Tool(
            name="set_montage_slot",
            description="Set the slot name for a Montage.",
            inputSchema={
                "type": "object",
                "properties": {
                    "asset_path": {"type": "string", "description": "Montage asset path"},
                    "slot_name": {"type": "string", "description": "Slot name (e.g. DefaultSlot, UpperBody)"}
                },
                "required": ["asset_path", "slot_name"]
            }
        ),
        Tool(
            name="link_montage_sections",
            description="Link two Montage sections (set next section after fromSection).",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {"type": "string", "description": "JSON: {montagePath, fromSection, toSection}"}
                },
                "required": ["json_config"]
            }
        ),
        # --- BlendSpace ---
        Tool(
            name="create_blend_space",
            description="Create a BlendSpace (1D if only axisX, 2D if axisX+axisY). Optionally add initial samples.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {"type": "string", "description": "JSON: {name, packagePath?, skeletonPath, axisX: {name, min, max}, axisY?: {name, min, max}, samples?: [{animPath, x, y?}]}"}
                },
                "required": ["json_config"]
            }
        ),
        Tool(
            name="add_blend_space_sample",
            description="Add a sample point (animation at coordinates) to a BlendSpace.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {"type": "string", "description": "JSON: {blendSpacePath, animPath, x, y?}"}
                },
                "required": ["json_config"]
            }
        ),
        Tool(
            name="set_blend_space_axis",
            description="Set axis parameters (name, min, max) for a BlendSpace.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {"type": "string", "description": "JSON: {blendSpacePath, axis: X|Y, name, min, max}"}
                },
                "required": ["json_config"]
            }
        ),
        # --- Skeleton ---
        Tool(
            name="get_skeleton_info",
            description="Get skeleton bone hierarchy, sockets, and virtual bones.",
            inputSchema={
                "type": "object",
                "properties": {
                    "skeleton_path": {"type": "string", "description": "Skeleton asset path"}
                },
                "required": ["skeleton_path"]
            }
        ),
        Tool(
            name="add_socket",
            description="Add a socket to a skeleton bone.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {"type": "string", "description": "JSON: {skeletonPath, boneName, socketName, relativeLocation?: {x,y,z}, relativeRotation?: {pitch,yaw,roll}, relativeScale?: {x,y,z}}"}
                },
                "required": ["json_config"]
            }
        ),
        Tool(
            name="add_virtual_bone",
            description="Add a virtual bone between two bones in a skeleton.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_config": {"type": "string", "description": "JSON: {skeletonPath, sourceBone, targetBone, virtualBoneName}"}
                },
                "required": ["json_config"]
            }
        ),
    ])

    # ==================== Mesh Generator Tools ====================
    tools.extend([
        Tool(
            name="request_character_mesh",
            description="Request character mesh generation. Returns convention path + generation prompt for external tools (Meshy, Rodin).",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_intent": {"type": "string", "description": "Character mesh intent JSON (entityTag, name, skeletonType, styleKeywords)"}
                },
                "required": ["json_intent"]
            }
        ),
        Tool(
            name="import_mesh",
            description="Import an externally generated mesh file into UE5.",
            inputSchema={
                "type": "object",
                "properties": {
                    "file_path": {"type": "string", "description": "Path to the mesh file (FBX, OBJ, etc.)"},
                    "json_intent": {"type": "string", "description": "Character mesh intent JSON"}
                },
                "required": ["file_path", "json_intent"]
            }
        ),
        Tool(
            name="get_pending_mesh_requests",
            description="List pending mesh generation requests.",
            inputSchema={"type": "object", "properties": {}}
        ),
        Tool(
            name="list_generated_meshes",
            description="List generated meshes in a directory.",
            inputSchema={
                "type": "object",
                "properties": {
                    "directory": {"type": "string", "description": "Directory to search (default: auto)"}
                }
            }
        ),
        Tool(
            name="get_skeleton_pool",
            description="Get available base skeleton information for mesh generation.",
            inputSchema={"type": "object", "properties": {}}
        ),
        # --- Shape Generator ---
        Tool(
            name="create_shape",
            description=(
                "Create a procedural shape StaticMesh asset for Niagara Mesh Renderer particles. "
                "Supports: Star, Ring, Disc, Sphere, Hemisphere, Petal, Diamond, Beam, ShockwaveRing, Spike, Cross. "
                "Hash-based caching — same params reuse existing asset. "
                "json_params example: {\"shapeType\":\"Star\",\"name\":\"Star_5pt\",\"points\":5,\"outerRadius\":100,\"innerRadius\":40,\"thickness\":5}"
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "json_params": {
                        "type": "string",
                        "description": (
                            "Shape params JSON. Required: shapeType, name. "
                            "Optional: scale(default 1), pivot('center'|'bottom'). "
                            "Shape-specific params vary by type (see description)."
                        )
                    },
                    "output_dir": {
                        "type": "string",
                        "description": "Output directory (default: /Game/Generated/VFX/Shapes)"
                    }
                },
                "required": ["json_params"]
            }
        ),
        Tool(
            name="list_shapes",
            description="List all available shape StaticMesh assets (catalog + on-disk). Use before creating to avoid duplicates.",
            inputSchema={"type": "object", "properties": {}}
        ),
        Tool(
            name="find_shape",
            description="Find a shape asset by name. Returns asset path if found.",
            inputSchema={
                "type": "object",
                "properties": {
                    "shape_name": {"type": "string", "description": "Shape name to search for"}
                },
                "required": ["shape_name"]
            }
        ),
    ])

    # ==================== Item Generator Tools ====================
    tools.extend([
        Tool(
            name="request_item",
            description="Request item (Entity.Item.*) generation. Returns convention path + mesh/icon generation prompts.",
            inputSchema={
                "type": "object",
                "properties": {
                    "json_intent": {"type": "string", "description": "Item intent JSON (itemTag, category, subType, element, rarity, styleKeywords)"}
                },
                "required": ["json_intent"]
            }
        ),
        Tool(
            name="import_item_mesh",
            description="Import an externally generated item mesh into UE5.",
            inputSchema={
                "type": "object",
                "properties": {
                    "file_path": {"type": "string", "description": "Path to the item mesh file"},
                    "json_intent": {"type": "string", "description": "Item intent JSON"}
                },
                "required": ["file_path", "json_intent"]
            }
        ),
        Tool(
            name="get_pending_item_requests",
            description="List pending item generation requests.",
            inputSchema={"type": "object", "properties": {}}
        ),
        Tool(
            name="list_generated_items",
            description="List generated items in a directory.",
            inputSchema={
                "type": "object",
                "properties": {
                    "directory": {"type": "string", "description": "Directory to search (default: auto)"}
                }
            }
        ),
        Tool(
            name="get_socket_mappings",
            description="Get item attachment socket mappings for character skeleton.",
            inputSchema={"type": "object", "properties": {}}
        ),
    ])

    # ==================== Step Management Tools ====================
    tools.extend([
        Tool(
            name="step_create_project",
            description="Create a new generation project with modular step tracking. Each step (concept_design, map_generation, story_generation, asset_discovery, character_generation, item_generation, vfx_generation) runs independently.",
            inputSchema={
                "type": "object",
                "properties": {
                    "name": {"type": "string", "description": "Project name (e.g., 'Goblin Dungeon')"},
                    "concept": {"type": "string", "description": "Initial concept text describing the desired gameplay scenario"},
                    "config_json": {"type": "string", "description": "Optional JSON config (output paths, generation preferences)"}
                },
                "required": ["name", "concept"]
            }
        ),
        Tool(
            name="step_list_projects",
            description="List all generation projects with step progress.",
            inputSchema={"type": "object", "properties": {}}
        ),
        Tool(
            name="step_get_status",
            description="Get full project status including all step states, input/output file paths, and completion progress.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project_id": {"type": "string", "description": "Project ID"}
                },
                "required": ["project_id"]
            }
        ),
        Tool(
            name="step_delete_project",
            description="Delete a project and all its step data.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project_id": {"type": "string", "description": "Project ID to delete"}
                },
                "required": ["project_id"]
            }
        ),
        Tool(
            name="step_begin",
            description="Start working on a step. Marks it as in_progress and auto-resolves input from upstream step output. Can be called by any agent.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project_id": {"type": "string", "description": "Project ID"},
                    "step_type": {"type": "string", "description": "Step type: concept_design|map_generation|story_generation|asset_discovery|character_generation|item_generation|vfx_generation"},
                    "input_json": {"type": "string", "description": "Optional explicit input JSON (overrides auto-resolve from upstream)"},
                    "agent_id": {"type": "string", "description": "Optional agent identifier for tracking"}
                },
                "required": ["project_id", "step_type"]
            }
        ),
        Tool(
            name="step_save_output",
            description="Save step output data, marking the step as completed. Output is saved both in manifest and as a standalone JSON file for direct access.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project_id": {"type": "string", "description": "Project ID"},
                    "step_type": {"type": "string", "description": "Step type"},
                    "output_json": {"type": "string", "description": "JSON output data from the step"},
                    "agent_id": {"type": "string", "description": "Optional agent identifier"}
                },
                "required": ["project_id", "step_type", "output_json"]
            }
        ),
        Tool(
            name="step_load_input",
            description="Load input for a step. Auto-resolves from upstream step's output based on the dependency graph.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project_id": {"type": "string", "description": "Project ID"},
                    "step_type": {"type": "string", "description": "Step type"}
                },
                "required": ["project_id", "step_type"]
            }
        ),
        Tool(
            name="step_fail",
            description="Mark a step as failed with an error message.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project_id": {"type": "string", "description": "Project ID"},
                    "step_type": {"type": "string", "description": "Step type"},
                    "error": {"type": "string", "description": "Error description"}
                },
                "required": ["project_id", "step_type", "error"]
            }
        ),
        Tool(
            name="step_get_schema",
            description="Get the input/output JSON schema for a step type. Useful for understanding what data a step expects and produces.",
            inputSchema={
                "type": "object",
                "properties": {
                    "step_type": {"type": "string", "description": "Step type"}
                },
                "required": ["step_type"]
            }
        ),
        Tool(
            name="step_list_types",
            description="List all available step types with descriptions and dependency relationships.",
            inputSchema={"type": "object", "properties": {}}
        ),
        Tool(
            name="step_add_feature",
            description="Register a feature in the project manifest. Used by pipeline for tracked features or by individual tabs for ad-hoc features.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project_id": {"type": "string", "description": "Project ID"},
                    "feature_id": {"type": "string", "description": "Unique feature identifier (e.g. fire-magic, manual-vfx-143025)"},
                    "name": {"type": "string", "description": "Human-readable feature name"},
                    "source": {"type": "string", "description": "Feature source: 'pipeline' or 'manual'", "default": "pipeline"},
                },
                "required": ["project_id", "feature_id"]
            }
        ),
        Tool(
            name="step_list_features",
            description="List all features for a project with their statuses and progress.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project_id": {"type": "string", "description": "Project ID"},
                },
                "required": ["project_id"]
            }
        ),
        Tool(
            name="step_update_feature",
            description="Update a feature's status and progress counters in the manifest.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project_id": {"type": "string", "description": "Project ID"},
                    "feature_id": {"type": "string", "description": "Feature ID to update"},
                    "status": {"type": "string", "description": "New status: not_started, designing, generating, completed, failed"},
                    "stories_completed": {"type": "integer", "description": "Number of stories completed"},
                    "assets_completed": {"type": "integer", "description": "Number of assets completed"},
                    "agent_id": {"type": "string", "description": "Agent currently working on this feature"},
                },
                "required": ["project_id", "feature_id"]
            }
        ),
        Tool(
            name="feature_save_work",
            description="Save per-feature work.json. Worker Agents use this to store results without conflicting with other workers. Each feature has its own isolated file.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project_id": {"type": "string", "description": "Project ID"},
                    "feature_id": {"type": "string", "description": "Feature ID"},
                    "work_json": {"type": "string", "description": "JSON string with story_files, asset_discovery, generated_assets, errors"},
                },
                "required": ["project_id", "feature_id", "work_json"]
            }
        ),
        Tool(
            name="feature_load_work",
            description="Load per-feature work.json to inspect a worker's results.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project_id": {"type": "string", "description": "Project ID"},
                    "feature_id": {"type": "string", "description": "Feature ID"},
                },
                "required": ["project_id", "feature_id"]
            }
        ),
        Tool(
            name="feature_aggregate",
            description="Aggregate all feature work.json files into unified step outputs. Called by Orchestrator after all Worker Agents complete. Produces story_generation, asset_discovery, and per-type generation output files.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project_id": {"type": "string", "description": "Project ID"},
                },
                "required": ["project_id"]
            }
        ),
    ])

    # ==================== Map Generation Tools ====================
    tools.extend([
        Tool(
            name="get_map_schema",
            description="Get the HktMap JSON schema. HktMap defines landscape, spawners, regions, and linked stories for dynamic loading.",
            inputSchema={"type": "object", "properties": {}}
        ),
        Tool(
            name="validate_map",
            description="Validate an HktMap JSON against the schema. Checks required fields, cross-references regions with spawners and stories.",
            inputSchema={
                "type": "object",
                "properties": {
                    "map_json": {"type": "string", "description": "HktMap JSON string to validate"}
                },
                "required": ["map_json"]
            }
        ),
        Tool(
            name="save_map",
            description="Save an HktMap JSON to the maps directory.",
            inputSchema={
                "type": "object",
                "properties": {
                    "map_json": {"type": "string", "description": "HktMap JSON string to save"}
                },
                "required": ["map_json"]
            }
        ),
        Tool(
            name="load_map",
            description="Load a saved HktMap JSON by its map_id.",
            inputSchema={
                "type": "object",
                "properties": {
                    "map_id": {"type": "string", "description": "Map ID to load"}
                },
                "required": ["map_id"]
            }
        ),
        Tool(
            name="list_maps",
            description="List all saved HktMaps with summary info.",
            inputSchema={"type": "object", "properties": {}}
        ),
        Tool(
            name="delete_map",
            description="Delete a saved HktMap by ID.",
            inputSchema={
                "type": "object",
                "properties": {
                    "map_id": {"type": "string", "description": "Map ID to delete"}
                },
                "required": ["map_id"]
            }
        ),
        Tool(
            name="build_map",
            description="Build an HktMap in UE5 - instantiate per-region landscapes, spawners, and load linked stories. Requires connected UE5 editor.",
            inputSchema={
                "type": "object",
                "properties": {
                    "map_json": {"type": "string", "description": "HktMap JSON to build in UE5"}
                },
                "required": ["map_json"]
            }
        ),
        Tool(
            name="generate_terrain_preview",
            description="Generate an ASCII heightmap preview from a terrain recipe JSON. Useful for visually verifying terrain layout before building.",
            inputSchema={
                "type": "object",
                "properties": {
                    "terrain_recipe_json": {"type": "string", "description": "Terrain recipe JSON (base_noise_type, octaves, frequency, features, etc.)"},
                    "width": {"type": "integer", "description": "ASCII preview width (default 60)", "default": 60},
                    "height": {"type": "integer", "description": "ASCII preview height (default 30)", "default": 30},
                },
                "required": ["terrain_recipe_json"]
            }
        ),
    ])

    # Python Script Executor
    tools.append(
        Tool(
            name="execute_python_script",
            description=(
                "Execute Python code directly in the Unreal Editor's embedded Python environment. "
                "The script has full access to the 'unreal' module and all editor APIs. Use this for:\n"
                "- Custom editor operations not covered by other tools\n"
                "- Batch operations on multiple actors/assets\n"
                "- Complex queries combining multiple API calls\n"
                "- Rapid prototyping of editor workflows\n\n"
                "Use print() to return data — stdout is captured and returned.\n"
                "Example: print(unreal.EditorAssetLibrary.list_assets('/Game/Meshes'))\n\n"
                "SAFETY: Scripts containing os.remove, subprocess, etc. are blocked by default. "
                "Set skip_safety_check=true to override."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "script_code": {
                        "type": "string",
                        "description": "Python code to execute in the UE5 editor Python environment"
                    },
                    "timeout": {
                        "type": "number",
                        "description": "Execution timeout in seconds (default: 30)",
                        "default": 30
                    },
                    "skip_safety_check": {
                        "type": "boolean",
                        "description": "Skip blocked-pattern safety check (default: false)",
                        "default": False
                    }
                },
                "required": ["script_code"]
            }
        )
    )

    return tools


@server.list_tools()
async def list_tools() -> list[Tool]:
    """Return all available MCP tools (hkt + monolith merged)"""
    tools = _get_hkt_tools()
    monolith = _get_monolith_client()
    # 헬스 폴링 결과와 무관하게 매번 직접 확인 (초기 tools/list가 폴링보다 먼저 올 수 있음)
    if not monolith.is_available:
        monolith.is_available = await monolith._health_check()
    if monolith.is_available:
        monolith_tools = await monolith.fetch_tools()
        tools.extend(monolith_tools)
    return tools


@server.call_tool()
async def call_tool(name: str, arguments: dict[str, Any]) -> list[TextContent]:
    """Handle tool execution requests (routes to monolith or hkt)"""
    logger.info(f"Calling tool: {name} with arguments: {arguments}")

    # monolith 도구면 프록시
    monolith = _get_monolith_client()
    if name in monolith.cached_tool_names and monolith.is_available:
        try:
            content = await monolith.call_tool(name, arguments)
            return [TextContent(type="text", text=item.get("text", "")) if isinstance(item, dict) else item for item in content]
        except Exception as e:
            logger.error(f"Monolith tool '{name}' failed: {e}")
            return [TextContent(type="text", text=f"Monolith error: {str(e)}")]

    # hkt 도구
    try:
        result = await dispatch_tool(name, arguments)
        return [TextContent(type="text", text=str(result))]
    except Exception as e:
        logger.error(f"Tool execution failed: {e}")
        return [TextContent(type="text", text=f"Error: {str(e)}")]


async def dispatch_tool(name: str, arguments: dict[str, Any]) -> Any:
    """Dispatch tool calls to appropriate handlers"""
    bridge = get_editor_bridge()
    
    # Asset Tools
    if name == "list_assets":
        return await asset_tools.list_assets(
            bridge,
            arguments["path"],
            arguments.get("class_filter", "")
        )
    elif name == "get_asset_info":
        return await asset_tools.get_asset_info(bridge, arguments["asset_path"])
    elif name == "search_assets":
        return await asset_tools.search_assets(
            bridge,
            arguments["query"],
            arguments.get("class_filter", "")
        )
    elif name == "modify_asset":
        return await asset_tools.modify_asset(
            bridge,
            arguments["asset_path"],
            arguments["property_name"],
            arguments["new_value"]
        )
    elif name == "create_data_asset_with_properties":
        return await asset_tools.create_data_asset_with_properties(
            bridge,
            arguments["asset_path"],
            arguments["parent_class"],
            arguments.get("properties"),
        )

    # Level Tools
    elif name == "list_actors":
        return await level_tools.list_actors(
            bridge,
            arguments.get("class_filter", "")
        )
    elif name == "spawn_actor":
        location = arguments["location"]
        rotation = arguments.get("rotation", {"pitch": 0, "yaw": 0, "roll": 0})
        return await level_tools.spawn_actor(
            bridge,
            arguments["blueprint_path"],
            (location["x"], location["y"], location["z"]),
            (rotation["pitch"], rotation["yaw"], rotation["roll"]),
            arguments.get("label", "")
        )
    elif name == "modify_actor":
        return await level_tools.modify_actor(
            bridge,
            arguments["actor_name"],
            arguments.get("location"),
            arguments.get("rotation"),
            arguments.get("scale")
        )
    elif name == "delete_actor":
        return await level_tools.delete_actor(bridge, arguments["actor_name"])
    elif name == "select_actor":
        return await level_tools.select_actor(bridge, arguments["actor_name"])
    
    # Query Tools
    elif name == "search_classes":
        return await query_tools.search_classes(
            bridge,
            arguments["query"],
            arguments.get("blueprint_only", False)
        )
    elif name == "get_class_properties":
        return await query_tools.get_class_properties(bridge, arguments["class_name"])
    elif name == "get_project_structure":
        return await query_tools.get_project_structure(
            bridge,
            arguments.get("root_path", "/Game")
        )
    elif name == "get_level_info":
        return await query_tools.get_level_info(bridge)
    
    # Runtime Tools
    elif name == "start_pie":
        return await runtime_tools.start_pie(bridge)
    elif name == "stop_pie":
        return await runtime_tools.stop_pie(bridge)
    elif name == "execute_console_command":
        return await runtime_tools.execute_console_command(bridge, arguments["command"])
    elif name == "get_game_state":
        runtime = get_runtime_bridge()
        return await runtime_tools.get_game_state(runtime)
    
    # Editor Utility Tools
    elif name == "get_viewport_camera":
        return await query_tools.get_viewport_camera(bridge)
    elif name == "set_viewport_camera":
        loc = arguments["location"]
        rot = arguments["rotation"]
        return await level_tools.set_viewport_camera(
            bridge,
            (loc["x"], loc["y"], loc["z"]),
            (rot["pitch"], rot["yaw"], rot["roll"])
        )
    elif name == "show_notification":
        return await query_tools.show_notification(
            bridge,
            arguments["message"],
            arguments.get("duration", 3.0)
        )
    
    # VFX Generator Tools
    elif name == "build_vfx_system":
        return await vfx_tools.build_vfx_system(
            bridge, arguments["json_config"], arguments.get("output_dir", "")
        )
    elif name == "build_preset_explosion":
        return await vfx_tools.build_preset_explosion(
            bridge, arguments["name"],
            arguments.get("r", 1.0), arguments.get("g", 0.5), arguments.get("b", 0.1),
            arguments.get("intensity", 0.5), arguments.get("output_dir", ""),
        )
    elif name == "get_vfx_prompt_guide":
        return await vfx_tools.get_vfx_prompt_guide(bridge)
    elif name == "get_vfx_examples":
        return await vfx_tools.get_vfx_examples(bridge)
    elif name == "get_vfx_config_schema":
        return await vfx_tools.get_vfx_config_schema(bridge)
    elif name == "list_generated_vfx":
        return await vfx_tools.list_generated_vfx(bridge, arguments.get("directory", ""))
    elif name == "dump_vfx_template_parameters":
        return await vfx_tools.dump_template_parameters(bridge, arguments.get("renderer_type", "sprite"))
    # Phase 2: Material Prep
    elif name == "create_particle_material":
        return await vfx_tools.create_particle_material(
            bridge, arguments["material_name"],
            arguments.get("texture_path", ""),
            arguments.get("blend_mode", "additive"),
            arguments.get("emissive_intensity", 1.0),
            arguments.get("output_dir", ""),
        )
    elif name == "assign_vfx_material":
        return await vfx_tools.assign_vfx_material(
            bridge, arguments["niagara_system_path"],
            arguments["emitter_name"], arguments["material_path"],
        )
    # Phase 4: Preview & Refine
    elif name == "preview_vfx":
        return await vfx_tools.preview_vfx(
            bridge, arguments["niagara_system_path"],
            arguments.get("duration", 2.0),
            arguments.get("screenshot_path", ""),
        )
    elif name == "update_vfx_emitter":
        return await vfx_tools.update_vfx_emitter(
            bridge, arguments["niagara_system_path"],
            arguments["emitter_name"], arguments["json_overrides"],
        )

    # Story Generator Tools
    elif name == "get_story_schema":
        return await story_tools.get_story_schema(bridge)
    elif name == "get_story_examples":
        return await story_tools.get_story_examples(bridge)
    elif name == "validate_story":
        return await story_tools.validate_story(bridge, arguments["json_story"])
    elif name == "analyze_story_dependencies":
        return await story_tools.analyze_dependencies(bridge, arguments["json_story"])
    elif name == "build_story":
        return await story_tools.build_story(bridge, arguments["json_story"])
    elif name == "list_stories":
        return await story_tools.list_stories(bridge)

    # Texture Generator Tools
    elif name == "check_sd_server_status":
        return await texture_tools.check_sd_server_status(bridge)
    elif name == "launch_sd_server":
        return await texture_tools.launch_sd_server(bridge)
    elif name == "generate_texture":
        return await texture_tools.generate_texture(
            bridge, arguments["json_intent"], arguments.get("output_dir", "")
        )
    elif name == "import_texture":
        return await texture_tools.import_texture(
            bridge, arguments["image_file_path"], arguments["json_intent"],
            arguments.get("output_dir", ""),
        )
    elif name == "get_pending_texture_requests":
        return await texture_tools.get_pending_texture_requests(bridge, arguments["json_requests"])
    elif name == "list_generated_textures":
        return await texture_tools.list_generated_textures(bridge, arguments.get("directory", ""))

    # Animation Generator Tools
    elif name == "request_animation":
        return await anim_tools.request_animation(bridge, arguments["json_intent"])
    elif name == "import_animation":
        return await anim_tools.import_animation(bridge, arguments["file_path"], arguments["json_intent"])
    elif name == "get_pending_anim_requests":
        return await anim_tools.get_pending_anim_requests(bridge)
    elif name == "list_generated_animations":
        return await anim_tools.list_generated_animations(bridge, arguments.get("directory", ""))

    # Animation Blueprint & Advanced Tools
    elif name == "get_anim_api_guide":
        return await anim_tools.get_anim_api_guide(bridge)
    elif name == "create_anim_blueprint":
        return await anim_tools.create_anim_blueprint(bridge, arguments["json_config"])
    elif name == "get_anim_blueprint_info":
        return await anim_tools.get_anim_blueprint_info(bridge, arguments["asset_path"])
    elif name == "compile_anim_blueprint":
        return await anim_tools.compile_anim_blueprint(bridge, arguments["asset_path"])
    elif name == "add_state_machine":
        return await anim_tools.add_state_machine(bridge, arguments["json_config"])
    elif name == "add_state":
        return await anim_tools.add_state(bridge, arguments["json_config"])
    elif name == "add_transition":
        return await anim_tools.add_transition(bridge, arguments["json_config"])
    elif name == "connect_state_machine_to_output":
        return await anim_tools.connect_state_machine_to_output(bridge, arguments["json_config"])
    elif name == "set_state_animation":
        return await anim_tools.set_state_animation(bridge, arguments["json_config"])
    elif name == "add_anim_graph_node":
        return await anim_tools.add_anim_graph_node(bridge, arguments["json_config"])
    elif name == "connect_anim_nodes":
        return await anim_tools.connect_anim_nodes(bridge, arguments["json_config"])
    elif name == "add_anim_parameter":
        return await anim_tools.add_anim_parameter(bridge, arguments["json_config"])
    elif name == "create_montage":
        return await anim_tools.create_montage(bridge, arguments["json_config"])
    elif name == "add_montage_section":
        return await anim_tools.add_montage_section(bridge, arguments["json_config"])
    elif name == "set_montage_slot":
        return await anim_tools.set_montage_slot(bridge, arguments["asset_path"], arguments["slot_name"])
    elif name == "link_montage_sections":
        return await anim_tools.link_montage_sections(bridge, arguments["json_config"])
    elif name == "create_blend_space":
        return await anim_tools.create_blend_space(bridge, arguments["json_config"])
    elif name == "add_blend_space_sample":
        return await anim_tools.add_blend_space_sample(bridge, arguments["json_config"])
    elif name == "set_blend_space_axis":
        return await anim_tools.set_blend_space_axis(bridge, arguments["json_config"])
    elif name == "get_skeleton_info":
        return await anim_tools.get_skeleton_info(bridge, arguments["skeleton_path"])
    elif name == "add_socket":
        return await anim_tools.add_socket(bridge, arguments["json_config"])
    elif name == "add_virtual_bone":
        return await anim_tools.add_virtual_bone(bridge, arguments["json_config"])

    # Mesh Generator Tools
    elif name == "request_character_mesh":
        return await mesh_tools.request_character_mesh(bridge, arguments["json_intent"])
    elif name == "import_mesh":
        return await mesh_tools.import_mesh(bridge, arguments["file_path"], arguments["json_intent"])
    elif name == "get_pending_mesh_requests":
        return await mesh_tools.get_pending_mesh_requests(bridge)
    elif name == "list_generated_meshes":
        return await mesh_tools.list_generated_meshes(bridge, arguments.get("directory", ""))
    elif name == "get_skeleton_pool":
        return await mesh_tools.get_skeleton_pool(bridge)
    # Shape Generator
    elif name == "create_shape":
        return await mesh_tools.create_shape(
            bridge, arguments["json_params"], arguments.get("output_dir", "")
        )
    elif name == "list_shapes":
        return await mesh_tools.list_shapes(bridge)
    elif name == "find_shape":
        return await mesh_tools.find_shape(bridge, arguments["shape_name"])

    # Item Generator Tools
    elif name == "request_item":
        return await item_tools.request_item(bridge, arguments["json_intent"])
    elif name == "import_item_mesh":
        return await item_tools.import_item_mesh(bridge, arguments["file_path"], arguments["json_intent"])
    elif name == "get_pending_item_requests":
        return await item_tools.get_pending_item_requests(bridge)
    elif name == "list_generated_items":
        return await item_tools.list_generated_items(bridge, arguments.get("directory", ""))
    elif name == "get_socket_mappings":
        return await item_tools.get_socket_mappings(bridge)

    # Step Management Tools (no bridge needed — pure Python state management)
    elif name == "step_create_project":
        return await step_tools.step_create_project(
            arguments["name"], arguments["concept"],
            arguments.get("config_json", "{}")
        )
    elif name == "step_list_projects":
        return await step_tools.step_list_projects()
    elif name == "step_get_status":
        return await step_tools.step_get_status(arguments["project_id"])
    elif name == "step_delete_project":
        return await step_tools.step_delete_project(arguments["project_id"])
    elif name == "step_begin":
        return await step_tools.step_begin(
            arguments["project_id"], arguments["step_type"],
            input_json=arguments.get("input_json", ""),
            agent_id=arguments.get("agent_id", ""),
        )
    elif name == "step_save_output":
        return await step_tools.step_save_output(
            arguments["project_id"], arguments["step_type"],
            arguments["output_json"],
            agent_id=arguments.get("agent_id", ""),
        )
    elif name == "step_load_input":
        return await step_tools.step_load_input(
            arguments["project_id"], arguments["step_type"],
        )
    elif name == "step_fail":
        return await step_tools.step_fail(
            arguments["project_id"], arguments["step_type"],
            arguments["error"],
        )
    elif name == "step_get_schema":
        return await step_tools.step_get_schema(arguments["step_type"])
    elif name == "step_list_types":
        return await step_tools.step_list_types()
    elif name == "step_add_feature":
        return await step_tools.step_add_feature(
            arguments["project_id"], arguments["feature_id"],
            name=arguments.get("name", ""),
            source=arguments.get("source", "pipeline"),
        )
    elif name == "step_list_features":
        return await step_tools.step_list_features(arguments["project_id"])
    elif name == "step_update_feature":
        return await step_tools.step_update_feature(
            arguments["project_id"], arguments["feature_id"],
            status=arguments.get("status", ""),
            stories_completed=arguments.get("stories_completed", -1),
            assets_completed=arguments.get("assets_completed", -1),
            agent_id=arguments.get("agent_id", ""),
        )
    elif name == "feature_save_work":
        return await step_tools.feature_save_work(
            arguments["project_id"], arguments["feature_id"],
            arguments["work_json"],
        )
    elif name == "feature_load_work":
        return await step_tools.feature_load_work(
            arguments["project_id"], arguments["feature_id"],
        )
    elif name == "feature_aggregate":
        return await step_tools.feature_aggregate(arguments["project_id"])

    # Map Generation Tools (no bridge needed for save/load/validate)
    elif name == "get_map_schema":
        return await map_tools.get_map_schema()
    elif name == "validate_map":
        return await map_tools.validate_map(arguments["map_json"])
    elif name == "save_map":
        return await map_tools.save_map(arguments["map_json"])
    elif name == "load_map":
        return await map_tools.load_map(arguments["map_id"])
    elif name == "list_maps":
        return await map_tools.list_maps()
    elif name == "delete_map":
        return await map_tools.delete_map(arguments["map_id"])
    elif name == "build_map":
        return await map_tools.build_map(arguments["map_json"], bridge)
    elif name == "generate_terrain_preview":
        return await map_tools.generate_terrain_preview(
            arguments["terrain_recipe_json"],
            arguments.get("width", 60),
            arguments.get("height", 30),
        )

    # Python Script Executor
    elif name == "execute_python_script":
        return await python_tools.execute_python_script(
            bridge,
            arguments["script_code"],
            timeout=arguments.get("timeout", 30.0),
            skip_safety_check=arguments.get("skip_safety_check", False),
        )

    else:
        raise ValueError(f"Unknown tool: {name}")


# ==================== Resource Definitions ====================

@server.list_resources()
async def list_resources() -> list[Resource]:
    """Return available resources"""
    return [
        Resource(
            uri="unreal://project/info",
            name="Project Information",
            description="Current Unreal Engine project information",
            mimeType="application/json"
        ),
        Resource(
            uri="unreal://level/current",
            name="Current Level",
            description="Information about the currently open level",
            mimeType="application/json"
        ),
    ]


@server.read_resource()
async def read_resource(uri: str) -> str:
    """Read resource content"""
    bridge = get_editor_bridge()
    
    if uri == "unreal://project/info":
        return await bridge.get_project_info()
    elif uri == "unreal://level/current":
        return await bridge.get_current_level_info()
    else:
        raise ValueError(f"Unknown resource: {uri}")


# ==================== Prompt Definitions ====================

@server.list_prompts()
async def list_prompts() -> list[Prompt]:
    """Return available prompts"""
    return [
        Prompt(
            name="level_setup",
            description="Generate a prompt for setting up a new level with specific requirements",
            arguments=[
                {
                    "name": "theme",
                    "description": "Level theme (e.g., 'sci-fi', 'fantasy', 'urban')",
                    "required": True
                },
                {
                    "name": "size",
                    "description": "Level size (small, medium, large)",
                    "required": False
                }
            ]
        ),
        Prompt(
            name="actor_placement",
            description="Generate a prompt for placing actors in a level",
            arguments=[
                {
                    "name": "actor_type",
                    "description": "Type of actors to place",
                    "required": True
                },
                {
                    "name": "count",
                    "description": "Number of actors to place",
                    "required": False
                }
            ]
        ),
    ]


@server.get_prompt()
async def get_prompt(name: str, arguments: dict[str, str] | None = None) -> str:
    """Get prompt content"""
    if name == "level_setup":
        theme = arguments.get("theme", "default") if arguments else "default"
        size = arguments.get("size", "medium") if arguments else "medium"
        return f"""You are helping to set up a new Unreal Engine 5 level with the following specifications:
        
Theme: {theme}
Size: {size}

Please analyze the current level state and suggest:
1. What actors should be added
2. Recommended lighting setup
3. Suggested blueprint actors to spawn
4. Environment considerations

Use the available MCP tools to:
- List current actors with list_actors
- Check available assets with list_assets
- Spawn new actors with spawn_actor
- Modify existing actors with modify_actor"""
    
    elif name == "actor_placement":
        actor_type = arguments.get("actor_type", "StaticMesh") if arguments else "StaticMesh"
        count = arguments.get("count", "5") if arguments else "5"
        return f"""You are placing {count} actors of type {actor_type} in the current level.

Use the following workflow:
1. Get current level info with get_level_info
2. Search for available {actor_type} assets with search_assets
3. Use spawn_actor to place each actor at appropriate positions
4. Use modify_actor to adjust transforms as needed

Consider spacing, visual composition, and gameplay requirements."""
    
    else:
        raise ValueError(f"Unknown prompt: {name}")


# ==================== Main Entry Point ====================

async def run_server():
    """Run the MCP server"""
    logger.info("Starting HKT MCP Server for Unreal Engine 5")

    # Log SD WebUI configuration
    from .config import get_config
    config = get_config()
    if config.sd_enabled:
        logger.info("SD WebUI auto-generation enabled (url: %s). Auto-launch uses batch file from Project Settings.", config.sd_url)
    else:
        logger.info("SD WebUI auto-generation disabled (SD_AUTO_GENERATE=false)")

    # Check if running inside UE Python environment
    try:
        import unreal
        logger.info("Running inside Unreal Engine Python environment")
        get_editor_bridge().set_unreal_module(unreal)
    except ImportError:
        logger.info("Running outside Unreal Engine - will use subprocess/RPC")

    # Start monolith proxy health polling
    monolith = _get_monolith_client()
    logger.info(f"Monolith proxy target: {monolith.mcp_url}")

    async def _on_monolith_state_change():
        """monolith 상태 전환 시 tools/list_changed 알림 시도"""
        # request_context는 요청 처리 중에만 유효하므로 백그라운드에서는 실패할 수 있음
        # 실패해도 다음 tools/list 요청 시 최신 상태가 반영됨
        try:
            session = server.request_context.session
            await session.send_tool_list_changed()
            logger.info("Sent tools/list_changed notification")
        except Exception:
            logger.debug("tools/list_changed skipped (no active session context)")

    poll_task = asyncio.create_task(
        monolith.start_health_poll(on_state_change=_on_monolith_state_change)
    )

    try:
        async with stdio_server() as (read_stream, write_stream):
            await server.run(read_stream, write_stream, server.create_initialization_options())
    finally:
        poll_task.cancel()
        await monolith.close()


def main():
    """Main entry point"""
    asyncio.run(run_server())


if __name__ == "__main__":
    main()

