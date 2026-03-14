"""
Unreal Engine Remote Control API Client

Connects to Unreal Editor via Remote Control API (port 30010)
to call BlueprintFunctionLibrary functions.

This client is designed to be generic. It does not define specific methods for each
available function. Instead, use call_function() to invoke any exposed function
in UHktMcpFunctionLibrary.
"""

import asyncio
import json
import logging
from typing import Any, Optional, Dict

logger = logging.getLogger("hkt_mcp.remote_execution")

try:
    import aiohttp
    HAS_AIOHTTP = True
except ImportError:
    HAS_AIOHTTP = False
    logger.warning("aiohttp not available, using synchronous HTTP")

import urllib.request
import urllib.error


class UnrealRemoteExecution:
    """
    Remote Control API Client for Unreal Engine
    
    Generic client to call functions in UHktMcpFunctionLibrary via Remote Control API.
    """
    
    # Remote Control API default port
    DEFAULT_PORT = 30010
    
    # Object path for the function library CDO
    FUNCTION_LIBRARY_PATH = "/Script/HktMcpBridgeEditor.Default__HktMcpFunctionLibrary"
    
    def __init__(self, host: str = "127.0.0.1", port: int = DEFAULT_PORT):
        self._host = host
        self._port = port
        self._base_url = f"http://{host}:{port}"
        self._session: Optional["aiohttp.ClientSession"] = None
        self._connected = False
    
    @property
    def is_connected(self) -> bool:
        return self._connected
    
    async def connect(self) -> bool:
        """Test connection to Remote Control API"""
        try:
            # We use McpPing to test connection
            result = await self.call_function("McpPing")
            if result.get("success"):
                self._connected = True
                logger.info(f"Connected to Remote Control API at {self._base_url}")
                return True
        except Exception as e:
            logger.debug(f"Connection failed: {e}")
        
        self._connected = False
        return False
    
    async def disconnect(self) -> None:
        """Close session"""
        if self._session:
            await self._session.close()
            self._session = None
        self._connected = False
    
    async def call_function(self, function_name: str, **parameters) -> dict:
        """
        Call a function on UHktMcpFunctionLibrary
        
        Args:
            function_name: Name of the function (e.g., "McpListAssets")
            **parameters: Function parameters as keyword arguments matching C++ argument names
            
        Returns:
            Dict with result: {"success": bool, "data": Any, "error": str}
        """
        url = f"{self._base_url}/remote/object/call"
        
        payload = {
            "objectPath": self.FUNCTION_LIBRARY_PATH,
            "functionName": function_name,
            "parameters": parameters,
            "generateTransaction": True
        }
        
        try:
            response = await self._post(url, payload)
            
            if response is None:
                return {"success": False, "error": "No response from server"}
            
            # Check for Remote Control API error
            if "errorMessage" in response:
                return {"success": False, "error": response["errorMessage"]}
            
            # Get return value
            return_value = response.get("ReturnValue", response)
            
            # If return value is a JSON string (which most of our C++ functions return), parse it
            if isinstance(return_value, str):
                try:
                    # Try to parse as JSON
                    parsed = json.loads(return_value)
                    return {"success": True, "data": parsed}
                except json.JSONDecodeError:
                    # If not valid JSON, return as string
                    return {"success": True, "data": return_value}
            
            return {"success": True, "data": return_value}
            
        except Exception as e:
            logger.error(f"Function call failed: {e}")
            return {"success": False, "error": str(e)}
    
    async def _post(self, url: str, data: dict) -> Optional[dict]:
        """Make POST request"""
        if HAS_AIOHTTP:
            if not self._session:
                timeout = aiohttp.ClientTimeout(total=30)
                self._session = aiohttp.ClientSession(timeout=timeout)
            
            try:
                async with self._session.put(  # Remote Control uses PUT
                    url,
                    json=data,
                    headers={"Content-Type": "application/json"}
                ) as resp:
                    if resp.status == 200:
                        return await resp.json()
                    else:
                        text = await resp.text()
                        logger.warning(f"Request failed ({resp.status}): {text}")
                        return {"errorMessage": text}
            except aiohttp.ClientError as e:
                logger.debug(f"Request error: {e}")
                return None
        else:
            # Synchronous fallback
            try:
                body = json.dumps(data).encode()
                req = urllib.request.Request(
                    url,
                    data=body,
                    headers={"Content-Type": "application/json"},
                    method="PUT"  # Remote Control uses PUT
                )
                with urllib.request.urlopen(req, timeout=30) as resp:
                    return json.loads(resp.read().decode())
            except Exception as e:
                logger.debug(f"Request error: {e}")
                return None


# Global instance
_remote_execution: Optional[UnrealRemoteExecution] = None


def get_remote_execution() -> UnrealRemoteExecution:
    """Get or create global remote execution instance"""
    global _remote_execution
    if _remote_execution is None:
        _remote_execution = UnrealRemoteExecution()
    return _remote_execution
