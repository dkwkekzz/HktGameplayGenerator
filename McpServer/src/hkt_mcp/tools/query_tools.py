"""
Query Tools - MCP tools for querying content in Unreal Engine 5

Provides tools for:
- Searching classes
- Getting class properties
- Project structure inspection
- Current level information
- Viewport camera queries
"""

import json
import logging
from typing import Any

from ..bridge.editor_bridge import EditorBridge

logger = logging.getLogger("hkt_mcp.tools.query")


async def search_classes(
    bridge: EditorBridge,
    query: str,
    blueprint_only: bool = False
) -> str:
    """
    Search for classes by name
    """
    logger.info(f"Searching classes: {query} (blueprint_only: {blueprint_only})")
    
    data = await bridge.call_method("McpSearchClasses", SearchQuery=query, bBlueprintOnly=blueprint_only)
    
    items = data.get("items", []) if data else []
    
    result = {
        "query": query,
        "blueprint_only": blueprint_only,
        "count": len(items),
        "classes": items
    }
    
    return json.dumps(result, indent=2)


async def get_class_properties(bridge: EditorBridge, class_name: str) -> str:
    """
    Get all properties of a class
    """
    logger.info(f"Getting properties of class: {class_name}")
    
    data = await bridge.call_method("McpGetClassProperties", ClassName=class_name)
    
    items = data.get("items", []) if data else []
    
    result = {
        "class_name": class_name,
        "count": len(items),
        "properties": items
    }
    
    return json.dumps(result, indent=2)


async def get_project_structure(bridge: EditorBridge, root_path: str = "/Game") -> str:
    """
    Get project folder structure
    """
    logger.info(f"Getting project structure from: {root_path}")
    
    data = await bridge.call_method("McpGetProjectStructure", RootPath=root_path)
    
    # Data is already the structure dict
    return json.dumps(data, indent=2) if data else "{}"


async def get_level_info(bridge: EditorBridge) -> str:
    """
    Get information about the current level
    """
    logger.info("Getting current level info")
    
    data = await bridge.call_method("McpGetCurrentLevelInfo")
    
    return json.dumps(data, indent=2) if data else "{}"


async def get_viewport_camera(bridge: EditorBridge) -> str:
    """
    Get current viewport camera position and rotation
    """
    logger.info("Getting viewport camera transform")
    
    data = await bridge.call_method("McpGetViewportCamera")
    
    if data and data.get("success"):
        result = {
            "location": data.get("location", {}),
            "rotation": data.get("rotation", {}),
            "success": True
        }
    else:
        result = {"error": "Failed to get camera", "success": False}
    
    return json.dumps(result, indent=2)


async def show_notification(bridge: EditorBridge, message: str, duration: float = 3.0) -> str:
    """
    Show a notification in the editor
    """
    logger.info(f"Showing notification: {message}")
    
    await bridge.call_method("McpShowNotification", Message=message, Duration=duration)
    
    result = {
        "message": message,
        "duration": duration,
        "success": True
    }
    
    return json.dumps(result, indent=2)
