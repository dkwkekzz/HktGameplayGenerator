"""
Stability AI REST API Client for texture generation.

Uses the Stability AI image generation API to create textures on cache miss.
Requires STABILITY_AI_API_KEY environment variable to be set.
"""

import logging
from pathlib import Path
from typing import Optional

logger = logging.getLogger("hkt_mcp.stability_client")

# Endpoint map by model name
ENDPOINTS = {
    "sd3.5-large": "https://api.stability.ai/v2beta/stable-image/generate/sd3",
    "sd3.5-medium": "https://api.stability.ai/v2beta/stable-image/generate/sd3",
    "core": "https://api.stability.ai/v2beta/stable-image/generate/core",
    "ultra": "https://api.stability.ai/v2beta/stable-image/generate/ultra",
}


def resolution_to_aspect_ratio(resolution: int) -> str:
    """Map pixel resolution to aspect ratio string. Game textures are typically square."""
    return "1:1"


class StabilityClient:
    """Async client for Stability AI image generation API."""

    def __init__(self, api_key: str, model: str = "sd3.5-large", timeout: float = 60.0):
        self._api_key = api_key
        self._model = model
        self._timeout = timeout
        self._endpoint = ENDPOINTS.get(model, ENDPOINTS["sd3.5-large"])

    async def generate_image(
        self,
        prompt: str,
        negative_prompt: str = "",
        output_path: str | Path = "output.png",
        aspect_ratio: str = "1:1",
    ) -> Path:
        """Generate an image via Stability AI and save to output_path.

        Returns the Path on success, raises RuntimeError on failure.
        """
        import aiohttp

        output_path = Path(output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)

        headers = {
            "Authorization": f"Bearer {self._api_key}",
            "Accept": "image/png",
        }

        form = aiohttp.FormData()
        form.add_field("prompt", prompt)
        if negative_prompt:
            form.add_field("negative_prompt", negative_prompt)
        form.add_field("aspect_ratio", aspect_ratio)
        form.add_field("output_format", "png")
        # For sd3.5 endpoints, specify model variant
        if "sd3" in self._endpoint:
            form.add_field("model", self._model.replace(".", "-"))

        timeout = aiohttp.ClientTimeout(total=self._timeout)
        async with aiohttp.ClientSession(timeout=timeout) as session:
            async with session.post(self._endpoint, headers=headers, data=form) as resp:
                if resp.status == 200:
                    image_bytes = await resp.read()
                    output_path.write_bytes(image_bytes)
                    logger.info("Image saved to %s (%d bytes)", output_path, len(image_bytes))
                    return output_path
                else:
                    body = await resp.text()
                    raise RuntimeError(
                        f"Stability AI API error {resp.status}: {body}"
                    )
