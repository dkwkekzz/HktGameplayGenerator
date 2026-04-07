"""
Monolith MCP 서버 HTTP 프록시 클라이언트

monolith(localhost:9316)의 MCP 도구를 hkt-unreal 게이트웨이를 통해 노출한다.
헬스 폴링으로 에디터 상태를 추적하고, 도구 목록을 동적으로 병합한다.

참고: Plugins/monolith/Scripts/monolith_proxy.py
"""

import asyncio
import json
import logging
from typing import Any, Callable, Coroutine, Optional

from mcp.types import Tool

logger = logging.getLogger("hkt_mcp.monolith_client")

try:
    import aiohttp
    HAS_AIOHTTP = True
except ImportError:
    HAS_AIOHTTP = False
    logger.warning("aiohttp not available, monolith proxy disabled")

import urllib.request
import urllib.error


class MonolithClient:
    """monolith MCP 서버와 통신하는 HTTP 프록시 클라이언트"""

    DEFAULT_URL = "http://localhost:9316/mcp"
    HEALTH_TIMEOUT = 3.0
    RPC_TIMEOUT = 30.0
    POLL_INTERVAL = 5.0
    POLL_START_DELAY = 3.0

    def __init__(self, mcp_url: str = DEFAULT_URL, poll_interval: float = POLL_INTERVAL):
        self.mcp_url = mcp_url
        self.health_url = mcp_url.replace("/mcp", "/health")
        self.poll_interval = poll_interval

        self.is_available: bool = False
        self.cached_tools: list[Tool] = []
        self.cached_tool_names: set[str] = set()

        self._rpc_id: int = 0
        self._session: Optional["aiohttp.ClientSession"] = None
        self._on_state_change: Optional[Callable[[], Coroutine]] = None

    # ── Health Polling ──

    async def start_health_poll(
        self,
        on_state_change: Optional[Callable[[], Coroutine]] = None,
    ) -> None:
        """헬스 폴링 루프. asyncio.create_task()로 실행."""
        self._on_state_change = on_state_change
        await asyncio.sleep(self.POLL_START_DELAY)
        logger.info(f"Monolith health poll started (interval={self.poll_interval}s, url={self.health_url})")

        while True:
            try:
                was_available = self.is_available
                self.is_available = await self._health_check()

                if was_available != self.is_available:
                    direction = "online" if self.is_available else "offline"
                    logger.info(f"Monolith went {direction}")

                    if self.is_available:
                        await self.fetch_tools()
                    else:
                        self.cached_tools = []
                        self.cached_tool_names = set()

                    if self._on_state_change:
                        await self._on_state_change()
            except asyncio.CancelledError:
                raise
            except Exception as e:
                logger.debug(f"Health poll error: {e}")

            await asyncio.sleep(self.poll_interval)

    async def _health_check(self) -> bool:
        """GET /health → bool"""
        if HAS_AIOHTTP:
            try:
                session = await self._get_session()
                async with session.get(
                    self.health_url,
                    timeout=aiohttp.ClientTimeout(total=self.HEALTH_TIMEOUT),
                ) as resp:
                    return resp.status == 200
            except Exception:
                return False
        else:
            try:
                req = urllib.request.Request(self.health_url, method="GET")
                with urllib.request.urlopen(req, timeout=self.HEALTH_TIMEOUT) as resp:
                    return resp.status == 200
            except Exception:
                return False

    # ── Tool Discovery ──

    async def fetch_tools(self) -> list[Tool]:
        """POST tools/list → Tool 객체 리스트. 캐시도 갱신."""
        resp = await self._post_jsonrpc("tools/list")
        if resp is None:
            self.cached_tools = []
            self.cached_tool_names = set()
            return []

        raw_tools = resp.get("result", {}).get("tools", [])
        tools = []
        for t in raw_tools:
            tools.append(Tool(
                name=t["name"],
                description=t.get("description", ""),
                inputSchema=t.get("inputSchema", {"type": "object", "properties": {}}),
            ))

        self.cached_tools = tools
        self.cached_tool_names = {t.name for t in tools}
        logger.info(f"Fetched {len(tools)} monolith tools")
        return tools

    # ── Tool Execution ──

    async def call_tool(self, name: str, arguments: dict[str, Any]) -> list[dict]:
        """POST tools/call → MCP content 리스트 (패스스루)."""
        resp = await self._post_jsonrpc("tools/call", {
            "name": name,
            "arguments": arguments,
        })

        if resp is None:
            return [{"type": "text", "text": (
                f"Monolith MCP is not available (Unreal Editor not running). "
                f"Tool '{name}' cannot execute. Start the editor and try again."
            )}]

        # JSON-RPC error
        if "error" in resp:
            error = resp["error"]
            msg = error.get("message", str(error))
            return [{"type": "text", "text": f"Monolith error: {msg}"}]

        # 정상 result — content 리스트를 그대로 반환
        result = resp.get("result", {})
        content = result.get("content", [])
        if content:
            return content

        # content가 없으면 result 전체를 텍스트로
        return [{"type": "text", "text": json.dumps(result, ensure_ascii=False)}]

    # ── JSON-RPC Transport ──

    async def _post_jsonrpc(self, method: str, params: Optional[dict] = None) -> Optional[dict]:
        """JSON-RPC POST to monolith. Returns parsed response or None."""
        self._rpc_id += 1
        body = json.dumps({
            "jsonrpc": "2.0",
            "id": self._rpc_id,
            "method": method,
            **({"params": params} if params else {}),
        })

        if HAS_AIOHTTP:
            try:
                session = await self._get_session()
                async with session.post(
                    self.mcp_url,
                    data=body,
                    headers={"Content-Type": "application/json"},
                    timeout=aiohttp.ClientTimeout(total=self.RPC_TIMEOUT),
                ) as resp:
                    if resp.status == 200:
                        return await resp.json()
                    text = await resp.text()
                    logger.warning(f"Monolith POST {method} failed ({resp.status}): {text}")
                    return None
            except Exception as e:
                logger.debug(f"Monolith POST {method} error: {e}")
                return None
        else:
            try:
                req = urllib.request.Request(
                    self.mcp_url,
                    data=body.encode("utf-8"),
                    headers={"Content-Type": "application/json"},
                    method="POST",
                )
                with urllib.request.urlopen(req, timeout=self.RPC_TIMEOUT) as resp:
                    return json.loads(resp.read().decode("utf-8"))
            except Exception as e:
                logger.debug(f"Monolith POST {method} error: {e}")
                return None

    async def _get_session(self) -> "aiohttp.ClientSession":
        if self._session is None or self._session.closed:
            self._session = aiohttp.ClientSession()
        return self._session

    async def close(self) -> None:
        if self._session and not self._session.closed:
            await self._session.close()
            self._session = None
