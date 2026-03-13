"""HKT VFX Production Examples Loader

프로덕션 퀄리티 VFX Config 예제를 로드하고 조회하는 유틸리티.

Usage:
    from vfx_production_examples import load_examples, get_config, list_examples

    # 전체 예제 로드
    examples = load_examples()

    # ID로 config 가져오기 (McpBuildNiagaraSystem에 바로 전달 가능한 dict)
    config = get_config("aaa_explosion")

    # JSON 문자열로 변환
    json_str = get_config_json("campfire_deluxe")

    # 카테고리별 필터
    weapon_configs = get_configs_by_category("weapon")

    # 태그로 검색
    ribbon_configs = get_configs_by_tag("ribbon")

    # 퀄리티별 필터
    production_configs = get_configs_by_quality("production")
    basic_configs = get_configs_by_quality("basic")
"""

import json
from pathlib import Path
from typing import Optional


_EXAMPLES_PATH = Path(__file__).parent / "VFXProductionExamples.json"
_cache: Optional[dict] = None


def _get_data() -> dict:
    """JSON 파일을 로드하고 캐싱."""
    global _cache
    if _cache is None:
        with open(_EXAMPLES_PATH, "r", encoding="utf-8") as f:
            _cache = json.load(f)
    return _cache


def load_examples() -> list[dict]:
    """전체 예제 목록 반환. 각 항목은 _id, _description, _category, _tags, config 포함."""
    return _get_data()["examples"]


def list_examples() -> list[dict]:
    """예제 요약 목록 반환 (config 제외, 메타데이터만)."""
    return [
        {
            "id": ex["_id"],
            "description": ex["_description"],
            "category": ex["_category"],
            "quality": ex.get("_quality", "unknown"),
            "tags": ex["_tags"],
            "emitter_count": len(ex["config"]["emitters"]),
        }
        for ex in load_examples()
    ]


def get_config(example_id: str) -> Optional[dict]:
    """ID로 VFX config dict 반환. McpBuildNiagaraSystem에 바로 전달 가능.

    Args:
        example_id: 예제 ID (e.g., "aaa_explosion", "campfire_deluxe")

    Returns:
        config dict 또는 None (ID 미발견 시)
    """
    for ex in load_examples():
        if ex["_id"] == example_id:
            return ex["config"]
    return None


def get_config_json(example_id: str, indent: int = 2) -> Optional[str]:
    """ID로 VFX config를 JSON 문자열로 반환.

    Args:
        example_id: 예제 ID
        indent: JSON 들여쓰기 (기본 2)

    Returns:
        JSON 문자열 또는 None
    """
    config = get_config(example_id)
    if config is None:
        return None
    return json.dumps(config, indent=indent, ensure_ascii=False)


def get_configs_by_category(category: str) -> list[dict]:
    """카테고리로 config 목록 반환.

    Categories: explosion, weapon, impact, magic, environment
    """
    return [ex["config"] for ex in load_examples() if ex["_category"] == category]


def get_configs_by_tag(tag: str) -> list[dict]:
    """태그를 포함하는 config 목록 반환.

    Tags: burst, rate, looping, ribbon, distortion, vortex, arc,
          rich_template, mesh_debris, ground_ring, ground_glow,
          muzzle_flash, directional, melee, shockwave, etc.
    """
    return [ex["config"] for ex in load_examples() if tag in ex["_tags"]]


def get_configs_by_quality(quality: str) -> list[dict]:
    """퀄리티 등급으로 config 목록 반환.

    Quality levels: "basic" (기존 레거시 예제), "production" (프로덕션 퀄리티)
    """
    return [ex["config"] for ex in load_examples() if ex.get("_quality") == quality]


def get_all_configs() -> list[dict]:
    """모든 config를 리스트로 반환."""
    return [ex["config"] for ex in load_examples()]


def get_all_configs_json(indent: int = 2) -> str:
    """모든 config를 JSON 배열 문자열로 반환. McpGetVFXExampleConfigs 대체용."""
    return json.dumps(get_all_configs(), indent=indent, ensure_ascii=False)


def get_meta() -> dict:
    """메타 정보 (버전, 빌더 제약 사항 등) 반환."""
    return _get_data()["_meta"]


if __name__ == "__main__":
    print("=== HKT VFX Production Examples ===\n")
    for info in list_examples():
        print(f"  [{info['id']}] {info['description']}")
        print(f"    category: {info['category']}, quality: {info['quality']}, emitters: {info['emitter_count']}, tags: {info['tags']}")
        print()
