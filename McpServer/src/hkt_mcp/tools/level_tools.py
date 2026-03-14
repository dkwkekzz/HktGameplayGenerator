"""
Level Tools - MCP tools for level editing in Unreal Engine 5

Provides tools for:
- Listing actors in the current level
- Spawning new actors
- Modifying actor transforms and properties
- Deleting actors
- Viewport camera control
"""

import json
import logging
from typing import Any, Optional

from ..bridge.editor_bridge import EditorBridge

logger = logging.getLogger("hkt_mcp.tools.level")


async def list_actors(bridge: EditorBridge, class_filter: str = "") -> str:
    """
    List all actors in the current level
    """
    logger.info(f"Listing actors (filter: {class_filter})")
    
    data = await bridge.call_method("McpListActors", ClassFilter=class_filter)
    
    items = data.get("items", []) if data else []
    
    result = {
        "class_filter": class_filter,
        "count": len(items),
        "actors": items
    }
    
    return json.dumps(result, indent=2)


async def spawn_actor(
    bridge: EditorBridge,
    blueprint_path: str,
    location: tuple[float, float, float],
    rotation: tuple[float, float, float] = (0, 0, 0),
    label: str = ""
) -> str:
    """
    Spawn a new actor from a Blueprint
    """
    logger.info(f"Spawning actor from {blueprint_path} at {location}")
    
    actor_name = await bridge.call_method(
        "McpSpawnActor",
        BlueprintPath=blueprint_path,
        Location={"X": location[0], "Y": location[1], "Z": location[2]},
        Rotation={"Pitch": rotation[0], "Yaw": rotation[1], "Roll": rotation[2]},
        ActorLabel=label
    )
    
    result = {
        "blueprint_path": blueprint_path,
        "location": {"x": location[0], "y": location[1], "z": location[2]},
        "rotation": {"pitch": rotation[0], "yaw": rotation[1], "roll": rotation[2]},
        "label": label,
        "success": bool(actor_name),
        "actor_name": actor_name if actor_name else "",
        "message": f"Spawned actor: {actor_name}" if actor_name else "Failed to spawn actor"
    }
    
    return json.dumps(result, indent=2)


async def spawn_actor_by_class(
    bridge: EditorBridge,
    class_name: str,
    location: tuple[float, float, float],
    rotation: tuple[float, float, float] = (0, 0, 0)
) -> str:
    """
    Spawn a new actor by native class name
    """
    logger.info(f"Spawning {class_name} at {location}")
    
    actor_name = await bridge.call_method(
        "McpSpawnActorByClass",
        ClassName=class_name,
        Location={"X": location[0], "Y": location[1], "Z": location[2]},
        Rotation={"Pitch": rotation[0], "Yaw": rotation[1], "Roll": rotation[2]}
    )
    
    result = {
        "class_name": class_name,
        "location": {"x": location[0], "y": location[1], "z": location[2]},
        "rotation": {"pitch": rotation[0], "yaw": rotation[1], "roll": rotation[2]},
        "success": bool(actor_name),
        "actor_name": actor_name if actor_name else "",
        "message": f"Spawned actor: {actor_name}" if actor_name else "Failed to spawn actor"
    }
    
    return json.dumps(result, indent=2)


async def modify_actor(
    bridge: EditorBridge,
    actor_name: str,
    location: Optional[dict] = None,
    rotation: Optional[dict] = None,
    scale: Optional[dict] = None
) -> str:
    """
    Modify an actor's transform
    """
    logger.info(f"Modifying actor {actor_name}")
    
    # We need to handle partial updates by getting current transform first if needed
    # Or simply pass what we have.
    # To be robust with the generic generic generic C++ API that requires all 3 (Location, Rotation, Scale),
    # we should fetch the actor first.
    
    # Check if we need to fetch defaults
    if location is None or rotation is None or scale is None:
        # Fetch actor info
        # Optimization: We could have a specific McpGetActorTransform, but we'll use ListActors or select logic
        # For now, let's use a simpler approach: if we don't provide it, we default to 0/1, which is bad.
        # So we MUST fetch.
        
        # We'll use McpListActors with a filter on the name? No, filter is class.
        # We can use McpGetSelectedActors if we select it first.
        
        await bridge.call_method("McpSelectActor", ActorName=actor_name)
        selected_data = await bridge.call_method("McpGetSelectedActors")
        items = selected_data.get("items", []) if selected_data else []
        
        current_loc = {"x": 0, "y": 0, "z": 0}
        current_rot = {"pitch": 0, "yaw": 0, "roll": 0}
        current_scale = {"x": 1, "y": 1, "z": 1}
        
        found = False
        if items:
            # Assume the first one is our actor
            a = items[0]
            if a.get("actor_name") == actor_name or a.get("actor_label") == actor_name:
                l = a.get("location", {})
                r = a.get("rotation", {})
                s = a.get("scale", {})
                current_loc = {"x": l.get("x", 0), "y": l.get("y", 0), "z": l.get("z", 0)}
                current_rot = {"pitch": r.get("pitch", 0), "yaw": r.get("yaw", 0), "roll": r.get("roll", 0)}
                current_scale = {"x": s.get("x", 1), "y": s.get("y", 1), "z": s.get("z", 1)}
                found = True
        
        if not found:
            return json.dumps({"success": False, "message": f"Actor {actor_name} not found"}, indent=2)

        if location is None:
            location = current_loc
        if rotation is None:
            rotation = current_rot
        if scale is None:
            scale = current_scale

    # Prepare params
    loc_param = {"X": location["x"], "Y": location["y"], "Z": location["z"]}
    rot_param = {"Pitch": rotation["pitch"], "Yaw": rotation["yaw"], "Roll": rotation["roll"]}
    scale_param = {"X": scale["x"], "Y": scale["y"], "Z": scale["z"]}
    
    success = await bridge.call_method(
        "McpModifyActorTransform",
        ActorName=actor_name,
        NewLocation=loc_param,
        NewRotation=rot_param,
        NewScale=scale_param
    )
    
    result = {
        "actor_name": actor_name,
        "location": location,
        "rotation": rotation,
        "scale": scale,
        "success": success is True,
        "message": "Actor modified successfully" if success else "Failed to modify actor"
    }
    
    return json.dumps(result, indent=2)


async def modify_actor_property(
    bridge: EditorBridge,
    actor_name: str,
    property_name: str,
    new_value: str
) -> str:
    """
    Modify an actor's property
    """
    logger.info(f"Modifying actor {actor_name} property {property_name}")
    
    # Not implemented in C++ yet? Wait, let's check. 
    # Subsystem has ModifyActorProperty, Library doesn't seem to have exposed it in the file provided earlier?
    # Ah, I should check HktMcpFunctionLibrary.h again.
    # It was not in the list I wrote.
    # The user said "If possible, just HktMcpFunctionLibrary.h".
    # I didn't add McpModifyActorProperty to the library in the previous step.
    # So I cannot call it yet.
    
    result = {
        "actor_name": actor_name,
        "property_name": property_name,
        "new_value": new_value,
        "success": False,
        "message": "McpModifyActorProperty not exposed in FunctionLibrary yet"
    }
    
    return json.dumps(result, indent=2)


async def delete_actor(bridge: EditorBridge, actor_name: str) -> str:
    """
    Delete an actor from the level
    """
    logger.info(f"Deleting actor: {actor_name}")
    
    success = await bridge.call_method("McpDeleteActor", ActorName=actor_name)
    
    result = {
        "actor_name": actor_name,
        "success": success is True,
        "message": f"Deleted actor: {actor_name}" if success else "Failed to delete actor"
    }
    
    return json.dumps(result, indent=2)


async def select_actor(bridge: EditorBridge, actor_name: str) -> str:
    """
    Select an actor in the editor
    """
    logger.info(f"Selecting actor: {actor_name}")
    
    success = await bridge.call_method("McpSelectActor", ActorName=actor_name)
    
    result = {
        "actor_name": actor_name,
        "success": success is True,
        "message": f"Selected actor: {actor_name}" if success else "Failed to select actor"
    }
    
    return json.dumps(result, indent=2)


async def get_selected_actors(bridge: EditorBridge) -> str:
    """
    Get currently selected actors
    """
    data = await bridge.call_method("McpGetSelectedActors")
    
    items = data.get("items", []) if data else []
    
    result = {
        "count": len(items),
        "actors": items
    }
    
    return json.dumps(result, indent=2)


async def set_viewport_camera(
    bridge: EditorBridge,
    location: tuple[float, float, float],
    rotation: tuple[float, float, float]
) -> str:
    """
    Set viewport camera transform
    """
    logger.info(f"Setting viewport camera to {location}")
    
    success = await bridge.call_method(
        "McpSetViewportCamera",
        Location={"X": location[0], "Y": location[1], "Z": location[2]},
        Rotation={"Pitch": rotation[0], "Yaw": rotation[1], "Roll": rotation[2]}
    )
    
    result = {
        "location": {"x": location[0], "y": location[1], "z": location[2]},
        "rotation": {"pitch": rotation[0], "yaw": rotation[1], "roll": rotation[2]},
        "success": success is True,
        "message": "Camera moved successfully" if success else "Failed to move camera"
    }
    
    return json.dumps(result, indent=2)


async def focus_on_actor(bridge: EditorBridge, actor_name: str) -> str:
    """
    Focus viewport camera on an actor
    """
    logger.info(f"Focusing on actor: {actor_name}")
    
    # First select the actor
    await bridge.call_method("McpSelectActor", ActorName=actor_name)
    
    # Get actor location
    selected_data = await bridge.call_method("McpGetSelectedActors")
    items = selected_data.get("items", []) if selected_data else []
    
    actor = next((a for a in items if a.get("actor_name") == actor_name or a.get("actor_label") == actor_name), None)
    
    if actor:
        loc = actor.get("location", {})
        x, y, z = loc.get("x", 0), loc.get("y", 0), loc.get("z", 0)
        
        # Position camera slightly offset from actor
        camera_loc = {"X": x - 500, "Y": y - 500, "Z": z + 200}
        camera_rot = {"Pitch": -20, "Yaw": 45, "Roll": 0}
        
        await bridge.call_method("McpSetViewportCamera", Location=camera_loc, Rotation=camera_rot)
    
    result = {
        "actor_name": actor_name,
        "success": actor is not None,
        "message": f"Focused on actor: {actor_name}" if actor else "Actor not found"
    }
    
    return json.dumps(result, indent=2)
