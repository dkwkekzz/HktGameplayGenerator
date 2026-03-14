"""
Runtime Bridge - Connects to running UE5 game instance via WebSocket

This bridge is used when the MCP server needs to interact with a running game,
either PIE (Play In Editor) or standalone game.

The bridge acts as a WebSocket client connecting to the 
HktMcpBridgeSubsystem running inside UE5.
"""

import asyncio
import json
import logging
from typing import Any, Optional, Callable
from dataclasses import dataclass, field
import uuid

logger = logging.getLogger("hkt_mcp.runtime_bridge")

try:
    import websockets
    from websockets.client import WebSocketClientProtocol
    HAS_WEBSOCKETS = True
except ImportError:
    HAS_WEBSOCKETS = False
    logger.warning("websockets package not installed - runtime bridge disabled")


@dataclass
class PendingRequest:
    """Pending RPC request"""
    id: str
    method: str
    future: asyncio.Future
    timeout: float = 30.0


class RuntimeBridge:
    """
    Bridge for connecting to running UE5 game instance
    
    Uses WebSocket to communicate with HktMcpBridgeSubsystem.
    """
    
    def __init__(self, host: str = "127.0.0.1", port: int = 9876):
        self._host = host
        self._port = port
        self._ws: Optional["WebSocketClientProtocol"] = None
        self._connected = False
        self._pending_requests: dict[str, PendingRequest] = {}
        self._message_handler: Optional[Callable] = None
        self._receive_task: Optional[asyncio.Task] = None
        self._reconnect_task: Optional[asyncio.Task] = None
        self._auto_reconnect = True
    
    @property
    def is_connected(self) -> bool:
        """Check if connected to runtime"""
        return self._connected and self._ws is not None
    
    @property
    def url(self) -> str:
        """Get WebSocket URL"""
        return f"ws://{self._host}:{self._port}"
    
    async def connect(self) -> bool:
        """Connect to UE5 runtime"""
        if not HAS_WEBSOCKETS:
            logger.error("websockets package not available")
            return False
        
        if self._connected:
            logger.warning("Already connected")
            return True
        
        try:
            self._ws = await websockets.connect(self.url)
            self._connected = True
            logger.info(f"Connected to runtime at {self.url}")
            
            # Start receive loop
            self._receive_task = asyncio.create_task(self._receive_loop())
            
            return True
        except Exception as e:
            logger.error(f"Failed to connect to {self.url}: {e}")
            self._connected = False
            return False
    
    async def disconnect(self) -> None:
        """Disconnect from runtime"""
        self._auto_reconnect = False
        
        if self._receive_task:
            self._receive_task.cancel()
            try:
                await self._receive_task
            except asyncio.CancelledError:
                pass
            self._receive_task = None
        
        if self._ws:
            await self._ws.close()
            self._ws = None
        
        self._connected = False
        logger.info("Disconnected from runtime")
    
    async def _receive_loop(self) -> None:
        """Background task to receive messages"""
        if not self._ws:
            return
        
        try:
            async for message in self._ws:
                await self._handle_message(message)
        except websockets.ConnectionClosed:
            logger.warning("Connection closed")
            self._connected = False
            if self._auto_reconnect:
                asyncio.create_task(self._reconnect())
        except Exception as e:
            logger.error(f"Receive error: {e}")
            self._connected = False
    
    async def _reconnect(self, delay: float = 5.0) -> None:
        """Attempt to reconnect after delay"""
        logger.info(f"Attempting reconnect in {delay}s...")
        await asyncio.sleep(delay)
        
        if not self._auto_reconnect:
            return
        
        success = await self.connect()
        if not success and self._auto_reconnect:
            # Exponential backoff
            await self._reconnect(min(delay * 2, 60.0))
    
    async def _handle_message(self, raw_message: str) -> None:
        """Handle incoming message"""
        try:
            message = json.loads(raw_message)
            
            # Check if it's a response to a pending request
            msg_id = message.get("id")
            if msg_id and msg_id in self._pending_requests:
                request = self._pending_requests.pop(msg_id)
                
                if "error" in message:
                    request.future.set_exception(
                        RuntimeError(message["error"].get("message", "Unknown error"))
                    )
                else:
                    request.future.set_result(message.get("result"))
            
            # Check if it's a notification/event
            elif self._message_handler:
                await self._message_handler(message)
                
        except json.JSONDecodeError as e:
            logger.error(f"Invalid JSON message: {e}")
        except Exception as e:
            logger.error(f"Message handling error: {e}")
    
    def set_message_handler(self, handler: Callable) -> None:
        """Set handler for incoming notifications/events"""
        self._message_handler = handler
    
    async def call(self, method: str, params: Optional[dict] = None, timeout: float = 30.0) -> Any:
        """Make an RPC call to the runtime"""
        if not self.is_connected:
            # Try to connect
            if not await self.connect():
                raise RuntimeError("Not connected to runtime")
        
        request_id = str(uuid.uuid4())
        
        request = {
            "jsonrpc": "2.0",
            "id": request_id,
            "method": method,
            "params": params or {}
        }
        
        # Create pending request
        future: asyncio.Future = asyncio.get_event_loop().create_future()
        self._pending_requests[request_id] = PendingRequest(
            id=request_id,
            method=method,
            future=future,
            timeout=timeout
        )
        
        try:
            await self._ws.send(json.dumps(request))
            
            # Wait for response with timeout
            result = await asyncio.wait_for(future, timeout=timeout)
            return result
            
        except asyncio.TimeoutError:
            self._pending_requests.pop(request_id, None)
            raise RuntimeError(f"Request timeout: {method}")
        except Exception as e:
            self._pending_requests.pop(request_id, None)
            raise
    
    # ==================== Game State Operations ====================
    
    async def get_game_state(self) -> dict:
        """Get current game state from runtime"""
        try:
            result = await self.call("get_game_state")
            if isinstance(result, str):
                return json.loads(result)
            return result
        except Exception as e:
            logger.error(f"Failed to get game state: {e}")
            return {"error": str(e), "connected": self.is_connected}
    
    async def execute_console_command(self, command: str) -> dict:
        """Execute console command in runtime"""
        try:
            result = await self.call("execute_command", {"command": command})
            if isinstance(result, str):
                return json.loads(result)
            return result
        except Exception as e:
            logger.error(f"Failed to execute command: {e}")
            return {"error": str(e)}
    
    async def get_actor_list(self, class_filter: str = "") -> list[dict]:
        """Get actor list from runtime"""
        try:
            result = await self.call("get_actor_list", {"class_filter": class_filter})
            if isinstance(result, str):
                data = json.loads(result)
                return data.get("actors", [])
            return result.get("actors", []) if isinstance(result, dict) else []
        except Exception as e:
            logger.error(f"Failed to get actor list: {e}")
            return []
    
    async def get_player_info(self) -> dict:
        """Get player information from runtime"""
        state = await self.get_game_state()
        return {
            "location": state.get("player_location"),
            "world": state.get("world_name"),
            "is_playing": state.get("is_playing", False)
        }
    
    async def teleport_player(self, x: float, y: float, z: float) -> bool:
        """Teleport player to location"""
        try:
            result = await self.call(
                "execute_command",
                {"command": f"setpos {x} {y} {z}"}
            )
            return True
        except Exception as e:
            logger.error(f"Failed to teleport: {e}")
            return False
    
    # ==================== Connection Testing ====================
    
    async def ping(self) -> bool:
        """Test connection to runtime"""
        try:
            await self.call("get_game_state", timeout=5.0)
            return True
        except Exception:
            return False

