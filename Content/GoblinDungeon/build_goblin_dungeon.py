"""
Goblin Dungeon Content Builder
===============================
HktMcp.Pipeline을 사용하여 고블린 던전 컨텐츠를 자동 빌드하는 스크립트.

사용법:
    MCP Agent가 이 스크립트의 각 Phase를 순서대로 실행합니다.
    Phase 1~4는 병렬 실행 가능, Phase 5는 모든 리소스 완료 후 실행.

파이프라인 흐름:
    Phase 1: VFX 빌드 (GoblinDungeonVFXConfigs.json)
    Phase 2: Entity Mesh 요청 + 임포트
    Phase 3: Item 요청 + 임포트
    Phase 4: Animation 요청 + 임포트
    Phase 5: Story 컴파일 (GoblinDungeonStories.json) — 모든 리소스 완료 후
"""

import json
from pathlib import Path

CONTENT_DIR = Path(__file__).parent


def load_json(filename: str) -> dict:
    """Content 디렉토리에서 JSON 파일 로드."""
    filepath = CONTENT_DIR / filename
    with open(filepath, "r", encoding="utf-8") as f:
        return json.load(f)


# ──────────────────────────────────────────────────────
# Phase 1: VFX Build
# ──────────────────────────────────────────────────────

def get_vfx_build_commands() -> list[dict]:
    """VFX Config들을 MCP API 호출 형태로 반환.

    Returns:
        각 VFX에 대한 McpBuildNiagaraSystem 호출 파라미터 리스트.
    """
    data = load_json("GoblinDungeonVFXConfigs.json")
    commands = []
    for entry in data["configs"]:
        commands.append({
            "api": "McpBuildNiagaraSystem",
            "tag": entry["_tag"],
            "config": entry["config"],
        })
    return commands


# ──────────────────────────────────────────────────────
# Phase 2: Entity Mesh
# ──────────────────────────────────────────────────────

def get_mesh_requests() -> list[dict]:
    """Entity Mesh 생성 요청 목록 반환.

    Returns:
        각 엔티티에 대한 McpRequestCharacterMesh 호출 파라미터 리스트.
    """
    data = load_json("GoblinDungeonDependencies.json")
    return [
        {
            "api": "McpRequestCharacterMesh",
            "tag": entry["tag"],
            "intent": entry["intent"],
        }
        for entry in data["dependencies"]["Entity"]["tags"]
    ]


# ──────────────────────────────────────────────────────
# Phase 3: Item
# ──────────────────────────────────────────────────────

def get_item_requests() -> list[dict]:
    """Item 생성 요청 목록 반환.

    Returns:
        각 아이템에 대한 McpRequestItem 호출 파라미터 리스트.
    """
    data = load_json("GoblinDungeonDependencies.json")
    return [
        {
            "api": "McpRequestItem",
            "tag": entry["tag"],
            "intent": entry["intent"],
        }
        for entry in data["dependencies"]["Item"]["tags"]
    ]


# ──────────────────────────────────────────────────────
# Phase 4: Animation
# ──────────────────────────────────────────────────────

def get_anim_requests() -> list[dict]:
    """Animation 생성 요청 목록 반환.

    Returns:
        각 애니메이션에 대한 McpRequestAnimation 호출 파라미터 리스트.
    """
    data = load_json("GoblinDungeonDependencies.json")
    return [
        {
            "api": "McpRequestAnimation",
            "tag": entry["tag"],
            "intent": entry["intent"],
        }
        for entry in data["dependencies"]["Anim"]["tags"]
    ]


# ──────────────────────────────────────────────────────
# Phase 5: Story Build (모든 리소스 완료 후)
# ──────────────────────────────────────────────────────

def get_story_build_commands() -> list[dict]:
    """Story JSON들을 MCP API 호출 형태로 반환.

    Returns:
        각 Story에 대한 McpBuildStory 호출 파라미터 리스트.
    """
    data = load_json("GoblinDungeonStories.json")
    commands = []
    for entry in data["stories"]:
        commands.append({
            "api": "McpBuildStory",
            "storyTag": entry["story"]["storyTag"],
            "json": entry["story"],
        })
    return commands


# ──────────────────────────────────────────────────────
# Summary
# ──────────────────────────────────────────────────────

def print_build_summary():
    """빌드 요약 정보 출력."""
    vfx = get_vfx_build_commands()
    mesh = get_mesh_requests()
    items = get_item_requests()
    anims = get_anim_requests()
    stories = get_story_build_commands()

    print("=" * 60)
    print("  Goblin Dungeon — Build Summary")
    print("=" * 60)
    print(f"  Phase 1: VFX .............. {len(vfx)} systems")
    for cmd in vfx:
        print(f"           - {cmd['tag']}")
    print(f"  Phase 2: Entity Mesh ...... {len(mesh)} meshes")
    for cmd in mesh:
        print(f"           - {cmd['tag']}")
    print(f"  Phase 3: Items ............ {len(items)} items")
    for cmd in items:
        print(f"           - {cmd['tag']}")
    print(f"  Phase 4: Animations ....... {len(anims)} anims")
    for cmd in anims:
        print(f"           - {cmd['tag']}")
    print(f"  Phase 5: Stories .......... {len(stories)} stories")
    for cmd in stories:
        print(f"           - {cmd['storyTag']}")
    print("=" * 60)
    total = len(vfx) + len(mesh) + len(items) + len(anims) + len(stories)
    print(f"  Total assets to generate: {total}")
    print("=" * 60)


if __name__ == "__main__":
    print_build_summary()
