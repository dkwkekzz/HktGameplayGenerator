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
from .bridge import editor_bridge, runtime_bridge

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


# ==================== Tool Definitions ====================

@server.list_tools()
async def list_tools() -> list[Tool]:
    """Return all available MCP tools"""
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
            description="Search for assets by name across the project",
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
            description="Search for classes by name",
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
            description="Get all properties of a class",
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
            description="Get the folder structure of the project",
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
            description="Build a Niagara VFX system from JSON config. Call get_vfx_prompt_guide first to learn the schema.",
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
            description="Request animation generation. Returns convention path + generation prompt + expected type for external tools (Mixamo, Motion Diffusion).",
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

    return tools


@server.call_tool()
async def call_tool(name: str, arguments: dict[str, Any]) -> list[TextContent]:
    """Handle tool execution requests"""
    logger.info(f"Calling tool: {name} with arguments: {arguments}")
    
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
        logger.info("SD WebUI auto-generation enabled (url: %s)", config.sd_url)
    else:
        logger.info("SD WebUI auto-generation disabled")

    # Check if running inside UE Python environment
    try:
        import unreal
        logger.info("Running inside Unreal Engine Python environment")
        get_editor_bridge().set_unreal_module(unreal)
    except ImportError:
        logger.info("Running outside Unreal Engine - will use subprocess/RPC")
    
    async with stdio_server() as (read_stream, write_stream):
        await server.run(read_stream, write_stream, server.create_initialization_options())


def main():
    """Main entry point"""
    asyncio.run(run_server())


if __name__ == "__main__":
    main()

