"""
Configuration for HKT MCP Server
"""

import os
from dataclasses import dataclass, field
from typing import Optional
from pathlib import Path


@dataclass
class McpConfig:
    """MCP Server configuration"""
    
    # Project settings
    project_path: Optional[Path] = None
    project_name: str = "HktProto"
    
    # WebSocket settings for runtime connection
    websocket_host: str = "127.0.0.1"
    websocket_port: int = 9876
    websocket_reconnect: bool = True
    websocket_reconnect_delay: float = 5.0
    
    # Logging
    log_level: str = "INFO"
    log_format: str = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
    
    # Timeouts
    rpc_timeout: float = 30.0
    connection_timeout: float = 10.0

    # Stability AI settings
    stability_ai_api_key: Optional[str] = None
    stability_ai_model: str = "sd3.5-large"
    stability_ai_timeout: float = 60.0
    stability_ai_auto_generate: bool = True
    
    @classmethod
    def from_environment(cls) -> "McpConfig":
        """Create config from environment variables"""
        config = cls()
        
        # Project path
        project_path = os.environ.get("UE_PROJECT_PATH")
        if project_path:
            config.project_path = Path(project_path)
        
        # Project name
        project_name = os.environ.get("UE_PROJECT_NAME")
        if project_name:
            config.project_name = project_name
        
        # WebSocket settings
        ws_host = os.environ.get("HKT_MCP_WS_HOST")
        if ws_host:
            config.websocket_host = ws_host
        
        ws_port = os.environ.get("HKT_MCP_WS_PORT")
        if ws_port:
            config.websocket_port = int(ws_port)
        
        # Logging
        log_level = os.environ.get("HKT_MCP_LOG_LEVEL")
        if log_level:
            config.log_level = log_level.upper()

        # Stability AI
        stability_key = os.environ.get("STABILITY_AI_API_KEY")
        if stability_key:
            config.stability_ai_api_key = stability_key

        stability_model = os.environ.get("STABILITY_AI_MODEL")
        if stability_model:
            config.stability_ai_model = stability_model

        stability_timeout = os.environ.get("STABILITY_AI_TIMEOUT")
        if stability_timeout:
            config.stability_ai_timeout = float(stability_timeout)

        stability_auto = os.environ.get("STABILITY_AI_AUTO_GENERATE")
        if stability_auto is not None:
            config.stability_ai_auto_generate = stability_auto.lower() != "false"

        return config
    
    @property
    def stability_ai_enabled(self) -> bool:
        """Whether Stability AI auto-generation is enabled."""
        return self.stability_ai_api_key is not None and self.stability_ai_auto_generate

    @property
    def websocket_url(self) -> str:
        """Get full WebSocket URL"""
        return f"ws://{self.websocket_host}:{self.websocket_port}"


# Global config instance
_config: Optional[McpConfig] = None


def get_config() -> McpConfig:
    """Get or create the global config instance"""
    global _config
    if _config is None:
        _config = McpConfig.from_environment()
    return _config


def set_config(config: McpConfig) -> None:
    """Set the global config instance"""
    global _config
    _config = config

