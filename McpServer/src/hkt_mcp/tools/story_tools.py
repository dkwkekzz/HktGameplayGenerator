"""
Story Generator Tools - MCP tools for Story/Ability script compilation

Calls UHktStoryGeneratorFunctionLibrary via Remote Control API.

Workflow:
1. get_story_schema    -> Learn Story JSON format + operations
2. get_story_examples  -> Study pattern examples
3. validate_story      -> Syntax validation
4. analyze_dependencies -> Identify required assets + trigger generators
5. build_story         -> Compile + register to VM
6. list_stories        -> Verify registered stories
"""

import json
import logging
from typing import Any

from ..bridge.editor_bridge import EditorBridge

logger = logging.getLogger("hkt_mcp.tools.story")

OBJECT_PATH = "/Script/HktStoryGenerator.Default__HktStoryGeneratorFunctionLibrary"


async def build_story(bridge: EditorBridge, json_story: str) -> str:
    """Compile JSON Story and register to VM"""
    logger.info("Building story")
    data = await bridge.call_method(
        "McpBuildStory", object_path=OBJECT_PATH, JsonStory=json_story,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def validate_story(bridge: EditorBridge, json_story: str) -> str:
    """Validate Story JSON syntax without registration"""
    logger.info("Validating story")
    data = await bridge.call_method(
        "McpValidateStory", object_path=OBJECT_PATH, JsonStory=json_story,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def analyze_dependencies(bridge: EditorBridge, json_story: str) -> str:
    """Analyze story dependencies and identify required assets"""
    logger.info("Analyzing story dependencies")
    data = await bridge.call_method(
        "McpAnalyzeDependencies", object_path=OBJECT_PATH, JsonStory=json_story,
    )
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def get_story_schema(bridge: EditorBridge) -> str:
    """Get Story JSON schema with all operations and registers"""
    logger.info("Getting story schema")
    data = await bridge.call_method("McpGetStorySchema", object_path=OBJECT_PATH)
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def get_story_examples(bridge: EditorBridge) -> str:
    """Get story pattern examples (BasicAttack, Fireball, Spawn, Wave)"""
    logger.info("Getting story examples")
    data = await bridge.call_method("McpGetStoryExamples", object_path=OBJECT_PATH)
    return json.dumps({"success": data is not None, "data": data}, indent=2)


async def list_stories(bridge: EditorBridge) -> str:
    """List dynamically registered stories"""
    logger.info("Listing registered stories")
    data = await bridge.call_method("McpListGeneratedStories", object_path=OBJECT_PATH)
    return json.dumps({"success": data is not None, "data": data}, indent=2)
