"""
Local Stable Diffusion WebUI (A1111/Forge) API Client for texture generation.

Connects to a locally running Stable Diffusion WebUI server via its REST API.
Start the server with: python launch.py --api

Default endpoint: http://127.0.0.1:7860/sdapi/v1/txt2img
"""

import base64
import logging
from pathlib import Path

logger = logging.getLogger("hkt_mcp.sd_client")


# Common resolution presets for game textures (square)
RESOLUTION_MAP = {
    64: 64,
    128: 128,
    256: 256,
    512: 512,
    1024: 1024,
    2048: 2048,
}


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

    async def health_check(self) -> bool:
        """Check if the SD WebUI server is running."""
        import aiohttp

        try:
            timeout = aiohttp.ClientTimeout(total=5.0)
            async with aiohttp.ClientSession(timeout=timeout) as session:
                async with session.get(f"{self._url}/sdapi/v1/sd-models") as resp:
                    return resp.status == 200
        except Exception:
            return False
