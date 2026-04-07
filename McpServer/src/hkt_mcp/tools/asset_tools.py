"""
Asset Tools - MCP tools for asset management in Unreal Engine 5

Provides tools for:
- Listing assets by path and class
- Searching assets by name
- Getting asset details
- Modifying asset properties
"""

import json
import logging
from typing import Any

from ..bridge.editor_bridge import EditorBridge

logger = logging.getLogger("hkt_mcp.tools.asset")


async def list_assets(bridge: EditorBridge, path: str, class_filter: str = "") -> str:
    """
    List assets in a specified path
    """
    logger.info(f"Listing assets in {path} (filter: {class_filter})")
    
    # Call Generic Bridge
    data = await bridge.call_method("McpListAssets", Path=path, ClassFilter=class_filter)
    
    items = data.get("items", []) if data else []
    
    result = {
        "path": path,
        "class_filter": class_filter,
        "count": len(items),
        "assets": items
    }
    
    return json.dumps(result, indent=2)


async def get_asset_info(bridge: EditorBridge, asset_path: str) -> str:
    """
    Get detailed information about an asset
    """
    logger.info(f"Getting asset info: {asset_path}")
    
    # Call Generic Bridge - McpGetAssetDetails returns a JSON string already in C++ (or parsed object if remote_execution parsed it)
    # The remote_execution parses JSON strings. So 'data' is a dict.
    data = await bridge.call_method("McpGetAssetDetails", AssetPath=asset_path)
    
    if data:
        return json.dumps(data, indent=2)
    return "{}"


async def search_assets(bridge: EditorBridge, query: str, class_filter: str = "") -> str:
    """
    Search for assets by name
    """
    logger.info(f"Searching assets: {query} (filter: {class_filter})")
    
    data = await bridge.call_method("McpSearchAssets", SearchQuery=query, ClassFilter=class_filter)
    
    items = data.get("items", []) if data else []
    
    result = {
        "query": query,
        "class_filter": class_filter,
        "count": len(items),
        "assets": items
    }
    
    return json.dumps(result, indent=2)


async def modify_asset(
    bridge: EditorBridge,
    asset_path: str,
    property_name: str,
    new_value: str
) -> str:
    """
    Modify an asset property
    """
    logger.info(f"Modifying asset {asset_path}: {property_name} = {new_value}")
    
    success = await bridge.call_method("McpModifyAssetProperty", AssetPath=asset_path, PropertyName=property_name, NewValue=new_value)
    
    result = {
        "asset_path": asset_path,
        "property_name": property_name,
        "new_value": new_value,
        "success": success is True,
        "message": "Property modified successfully" if success else "Failed to modify property"
    }
    
    return json.dumps(result, indent=2)


async def delete_asset(bridge: EditorBridge, asset_path: str) -> str:
    """
    Delete an asset
    """
    logger.info(f"Deleting asset: {asset_path}")
    
    # Not implemented in C++ lib yet
    result = {
        "asset_path": asset_path,
        "success": False,
        "message": "Delete asset not implemented yet"
    }
    
    return json.dumps(result, indent=2)


async def duplicate_asset(
    bridge: EditorBridge,
    source_path: str,
    destination_path: str
) -> str:
    """
    Duplicate an asset
    """
    logger.info(f"Duplicating asset: {source_path} -> {destination_path}")
    
    # Not implemented in C++ lib yet
    result = {
        "source_path": source_path,
        "destination_path": destination_path,
        "success": False,
        "message": "Duplicate asset not implemented yet"
    }
    
    return json.dumps(result, indent=2)


async def create_data_asset(
    bridge: EditorBridge,
    asset_path: str,
    parent_class: str
) -> str:
    """Create a new DataAsset (without setting properties)."""
    logger.info(f"Creating DataAsset: {asset_path} ({parent_class})")

    data = await bridge.call_method(
        "McpCreateDataAsset",
        AssetPath=asset_path,
        ParentClassName=parent_class,
    )

    if isinstance(data, dict):
        return json.dumps(data, indent=2)

    return json.dumps({"success": False, "error": "Unexpected response"}, indent=2)


async def create_data_asset_with_properties(
    bridge: EditorBridge,
    asset_path: str,
    parent_class: str,
    properties: dict | None = None,
) -> str:
    """Create a DataAsset and set properties in one call.

    Works with ANY UDataAsset subclass. Properties use the same formats as modify_asset:
      - FGameplayTag: "Entity.Character.Goblin"
      - FSoftObjectPath: "/Game/Path/Asset.Asset"
      - TSoftObjectPtr: "/Game/Path/Asset.Asset"
      - UObject* hard ref: "/Game/Path/Asset"
      - FGameplayTagContainer: "Tag.A, Tag.B, Tag.C"
      - int/float/bool/string/enum: "42", "true", "Hello"

    Args:
        asset_path: UE5 asset path (e.g. /Game/Generated/VFX/DA_VFX_Fire)
        parent_class: Full class path (e.g. /Script/HktAsset.HktVFXVisualDataAsset)
        properties: Dict of {PropertyName: Value} to set after creation
    """
    logger.info(f"Creating DataAsset with properties: {asset_path} ({parent_class})")

    properties_json = json.dumps(properties) if properties else ""

    data = await bridge.call_method(
        "McpCreateDataAssetWithProperties",
        AssetPath=asset_path,
        ParentClassName=parent_class,
        PropertiesJson=properties_json,
    )

    if isinstance(data, dict):
        return json.dumps(data, indent=2)
    if isinstance(data, str):
        return data

    return json.dumps({"success": False, "error": "Unexpected response"}, indent=2)
