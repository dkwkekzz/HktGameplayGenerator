"""
Editor Bridge - Interfaces with Unreal Engine 5 Editor

This bridge uses the Remote Control API to communicate with the editor.
It serves as a thin wrapper around the generic Remote Control client.
"""

import json
import logging
from typing import Any, Optional, Dict, List

from .remote_execution import get_remote_execution, UnrealRemoteExecution

logger = logging.getLogger("hkt_mcp.editor_bridge")


class EditorBridge:
    """
    Bridge for Unreal Engine 5 Editor operations
    
    Exposes a generic interface to call MCP functions in Unreal.
    """
    
    def __init__(self):
        self._remote = get_remote_execution()
        logger.info("EditorBridge initialized - using generic Remote Control API")

    def set_unreal_module(self, unreal_module):
        """Not used in Remote Control mode"""
        pass

    async def call_method(self, method_name: str, **kwargs) -> Any:
        """
        Call a method on the generic remote execution client.
        
        Args:
            method_name: Name of the Blueprint function (e.g., "McpListAssets")
            **kwargs: Arguments to pass to the function (must match C++ parameter names)
            
        Returns:
            The 'data' portion of the response if successful, or raises Exception/returns None.
        """
        result = await self._remote.call_function(method_name, **kwargs)
        
        if result.get("success"):
            return result.get("data")
        else:
            error_msg = result.get("error", "Unknown error")
            logger.error(f"Error calling {method_name}: {error_msg}")
            # Depending on error handling strategy, we might return None or raise
            # For now, let's return None so the tool can handle it
            return None

    # Helper for project info which is used by server resource
    async def get_project_info(self) -> str:
        """Get project info"""
        # This one doesn't necessarily map to a specific BP function yet, or we could add one.
        # For now, return static info as placeholder or implement McpGetProjectInfo in C++
        return json.dumps({"name": "Unreal Project", "type": "Remote Control"})
