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
    
    # Pipeline monitor
    pipeline_data_path: str = ".pipeline_data"

    # Timeouts
    rpc_timeout: float = 30.0
    connection_timeout: float = 10.0

    # Monolith MCP proxy
    monolith_url: str = "http://localhost:9316/mcp"

    # Local SD WebUI settings (A1111/Forge)
    sd_url: str = "http://127.0.0.1:7860"
    sd_timeout: float = 120.0
    sd_steps: int = 20
    sd_cfg_scale: float = 7.0
    sd_sampler: str = "Euler a"
    sd_auto_generate: bool = True
    
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

        # Local SD WebUI
        sd_url = os.environ.get("SD_WEBUI_URL")
        if sd_url:
            config.sd_url = sd_url

        sd_timeout = os.environ.get("SD_WEBUI_TIMEOUT")
        if sd_timeout:
            config.sd_timeout = float(sd_timeout)

        sd_steps = os.environ.get("SD_WEBUI_STEPS")
        if sd_steps:
            config.sd_steps = int(sd_steps)

        sd_cfg = os.environ.get("SD_WEBUI_CFG_SCALE")
        if sd_cfg:
            config.sd_cfg_scale = float(sd_cfg)

        sd_sampler = os.environ.get("SD_WEBUI_SAMPLER")
        if sd_sampler:
            config.sd_sampler = sd_sampler

        sd_auto = os.environ.get("SD_AUTO_GENERATE")
        if sd_auto is not None:
            config.sd_auto_generate = sd_auto.lower() != "false"

        # Monolith MCP
        monolith_url = os.environ.get("MONOLITH_URL")
        if monolith_url:
            config.monolith_url = monolith_url

        return config
    
    @property
    def sd_enabled(self) -> bool:
        """Whether local SD WebUI auto-generation is enabled."""
        return self.sd_auto_generate

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

