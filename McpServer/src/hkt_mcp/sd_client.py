"""
Local Stable Diffusion WebUI (A1111/Forge) API Client for texture generation.

Connects to a locally running Stable Diffusion WebUI server via its REST API.
If the server is not running, auto-launches it using the batch file path
configured in UHktTextureGeneratorSettings (Project Settings).

Default endpoint: http://127.0.0.1:7860/sdapi/v1/txt2img
"""

import asyncio
import base64
import logging
import subprocess
from pathlib import Path
from typing import Optional

logger = logging.getLogger("hkt_mcp.sd_client")

# Singleton process reference to avoid launching multiple instances
_sd_process: Optional[subprocess.Popen] = None


class SDWebUIClient:
    """Async client for local Stable Diffusion WebUI (A1111/Forge) API."""

    def __init__(
        self,
        url: str = "http://127.0.0.1:7860",
        timeout: float = 120.0,
        default_steps: int = 20,
        default_cfg_scale: float = 7.0,
        default_sampler: str = "Euler a",
    ):
        self._url = url.rstrip("/")
        self._timeout = timeout
        self._default_steps = default_steps
        self._default_cfg_scale = default_cfg_scale
        self._default_sampler = default_sampler

    async def is_alive(self) -> bool:
        """Check if the SD WebUI server is responding."""
        import aiohttp

        try:
            timeout = aiohttp.ClientTimeout(total=5.0)
            async with aiohttp.ClientSession(timeout=timeout) as session:
                async with session.get(f"{self._url}/sdapi/v1/sd-models") as resp:
                    return resp.status == 200
        except Exception:
            return False

    async def ensure_running(self, batch_file_path: str, poll_interval: float = 3.0, max_wait: float = 180.0) -> bool:
        """Ensure the SD WebUI server is running. Launch it if not.

        Args:
            batch_file_path: Path to the .bat file that starts SD WebUI.
            poll_interval: Seconds between health check polls.
            max_wait: Maximum seconds to wait for the server to become ready.

        Returns:
            True if the server is alive, False if timed out.
        """
        global _sd_process

        # Already running?
        if await self.is_alive():
            logger.info("SD WebUI already running at %s", self._url)
            return True

        # Launch the batch file
        bat = Path(batch_file_path)
        if not bat.exists():
            logger.error("SD WebUI batch file not found: %s", batch_file_path)
            return False

        logger.info("Launching SD WebUI: %s", batch_file_path)
        _sd_process = subprocess.Popen(
            [str(bat)],
            cwd=str(bat.parent),
            shell=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            creationflags=getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0),
        )

        # Poll until the server responds
        elapsed = 0.0
        while elapsed < max_wait:
            await asyncio.sleep(poll_interval)
            elapsed += poll_interval

            if await self.is_alive():
                logger.info("SD WebUI is ready (waited %.0fs)", elapsed)
                return True

            # Check if the process died
            if _sd_process.poll() is not None:
                logger.error("SD WebUI process exited with code %d", _sd_process.returncode)
                return False

            logger.debug("Waiting for SD WebUI... (%.0f/%.0fs)", elapsed, max_wait)

        logger.error("SD WebUI did not become ready within %.0fs", max_wait)
        return False

    async def generate_image(
        self,
        prompt: str,
        negative_prompt: str = "",
        output_path: str | Path = "output.png",
        width: int = 512,
        height: int = 512,
    ) -> Path:
        """Generate an image via local SD WebUI and save to output_path.

        Returns the Path on success, raises RuntimeError on failure.
        """
        import aiohttp

        output_path = Path(output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)

        payload = {
            "prompt": prompt,
            "negative_prompt": negative_prompt,
            "width": width,
            "height": height,
            "steps": self._default_steps,
            "cfg_scale": self._default_cfg_scale,
            "sampler_name": self._default_sampler,
            "batch_size": 1,
            "n_iter": 1,
        }

        endpoint = f"{self._url}/sdapi/v1/txt2img"
        timeout = aiohttp.ClientTimeout(total=self._timeout)

        async with aiohttp.ClientSession(timeout=timeout) as session:
            async with session.post(endpoint, json=payload) as resp:
                if resp.status == 200:
                    result = await resp.json()
                    images = result.get("images", [])
                    if not images:
                        raise RuntimeError("SD WebUI returned empty images array")

                    image_bytes = base64.b64decode(images[0])
                    output_path.write_bytes(image_bytes)
                    logger.info("Image saved to %s (%d bytes)", output_path, len(image_bytes))
                    return output_path
                else:
                    body = await resp.text()
                    raise RuntimeError(
                        f"SD WebUI API error {resp.status}: {body}"
                    )
