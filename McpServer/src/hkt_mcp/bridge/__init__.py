"""
Bridge modules for connecting MCP Server to Unreal Engine 5

- editor_bridge: Uses unreal Python module for editor operations
- runtime_bridge: Uses WebSocket for runtime game state operations
"""

from .editor_bridge import EditorBridge
from .runtime_bridge import RuntimeBridge

__all__ = ["EditorBridge", "RuntimeBridge"]

