"""
Runtime Tools - MCP tools for runtime control in Unreal Engine 5

Provides tools for:
- Starting/Stopping PIE (Play In Editor)
- Executing console commands
- Getting game state from running instances
"""

import json
import logging
from typing import Any

from ..bridge.editor_bridge import EditorBridge
from ..bridge.runtime_bridge import RuntimeBridge

logger = logging.getLogger("hkt_mcp.tools.runtime")


async def start_pie(bridge: EditorBridge) -> str:
    """
    Start Play In Editor session
    """
    logger.info("Starting PIE session")
    
    success = await bridge.call_method("McpStartPIE")
    
    result = {
        "success": success is True,
        "message": "PIE session started" if success else "Failed to start PIE"
    }
    
    return json.dumps(result, indent=2)


async def stop_pie(bridge: EditorBridge) -> str:
    """
    Stop the current PIE session
    """
    logger.info("Stopping PIE session")
    
    success = await bridge.call_method("McpStopPIE")
    
    result = {
        "success": success is True,
        "message": "PIE session stopped" if success else "Failed to stop PIE"
    }
    
    return json.dumps(result, indent=2)


async def is_pie_running(bridge: EditorBridge) -> str:
    """
    Check if PIE is currently running
    """
    logger.info("Checking PIE status")
    
    running = await bridge.call_method("McpIsPIERunning")
    
    result = {
        "is_running": running is True,
        "message": "PIE is running" if running else "PIE is not running"
    }
    
    return json.dumps(result, indent=2)


async def execute_console_command(bridge: EditorBridge, command: str) -> str:
    """
    Execute a console command in the editor
    """
    logger.info(f"Executing console command: {command}")
    
    await bridge.call_method("McpExecuteCommand", Command=command)
    
    result = {
        "command": command,
        "success": True,
        "message": f"Executed command: {command}"
    }
    
    return json.dumps(result, indent=2)


async def get_game_state(runtime_bridge: RuntimeBridge) -> str:
    """
    Get current game state from runtime
    """
    logger.info("Getting game state from runtime")
    
    state = await runtime_bridge.get_game_state()
    
    result = {
        "connected": runtime_bridge.is_connected,
        "state": state
    }
    
    return json.dumps(result, indent=2)


async def get_runtime_actors(runtime_bridge: RuntimeBridge, class_filter: str = "") -> str:
    """
    Get actors from running game
    """
    logger.info(f"Getting runtime actors (filter: {class_filter})")
    
    actors = await runtime_bridge.get_actor_list(class_filter)
    
    result = {
        "connected": runtime_bridge.is_connected,
        "class_filter": class_filter,
        "count": len(actors),
        "actors": actors
    }
    
    return json.dumps(result, indent=2)


async def get_player_info(runtime_bridge: RuntimeBridge) -> str:
    """
    Get player information from running game
    """
    logger.info("Getting player info from runtime")
    
    player_info = await runtime_bridge.get_player_info()
    
    result = {
        "connected": runtime_bridge.is_connected,
        "player": player_info
    }
    
    return json.dumps(result, indent=2)


async def teleport_player(
    runtime_bridge: RuntimeBridge,
    x: float,
    y: float,
    z: float
) -> str:
    """
    Teleport player to location in running game
    """
    logger.info(f"Teleporting player to ({x}, {y}, {z})")
    
    success = await runtime_bridge.teleport_player(x, y, z)
    
    result = {
        "location": {"x": x, "y": y, "z": z},
        "success": success,
        "message": "Player teleported" if success else "Failed to teleport player"
    }
    
    return json.dumps(result, indent=2)


async def runtime_command(runtime_bridge: RuntimeBridge, command: str) -> str:
    """
    Execute console command in running game
    """
    logger.info(f"Executing runtime command: {command}")
    
    result = await runtime_bridge.execute_console_command(command)
    
    return json.dumps({
        "command": command,
        "result": result,
        "connected": runtime_bridge.is_connected
    }, indent=2)


async def connect_runtime(runtime_bridge: RuntimeBridge) -> str:
    """
    Connect to running game instance
    """
    logger.info(f"Connecting to runtime at {runtime_bridge.url}")
    
    success = await runtime_bridge.connect()
    
    result = {
        "url": runtime_bridge.url,
        "connected": success,
        "message": "Connected to runtime" if success else "Failed to connect"
    }
    
    return json.dumps(result, indent=2)


async def disconnect_runtime(runtime_bridge: RuntimeBridge) -> str:
    """
    Disconnect from running game instance
    """
    logger.info("Disconnecting from runtime")
    
    await runtime_bridge.disconnect()
    
    result = {
        "connected": False,
        "message": "Disconnected from runtime"
    }
    
    return json.dumps(result, indent=2)
