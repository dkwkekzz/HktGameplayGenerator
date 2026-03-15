"""FireTornado VFX 빌드 스크립트

monolith niagara_query 과정을 HktVFXGenerator JSON config로 재현.
Unreal Editor Python 콘솔 또는 CLI에서 실행 가능.

Usage (Unreal Editor Python):
    import build_fire_tornado
    build_fire_tornado.build()

Usage (standalone — JSON 출력만):
    python build_fire_tornado.py [--output-dir /Game/HktGameplay/Generated]
"""

import json
import sys
from pathlib import Path


_CONFIG_PATH = Path(__file__).parent / "FireTornadoConfig.json"
_DEFAULT_OUTPUT_DIR = "/Game/HktGameplay/Generated"


def load_config() -> dict:
    """FireTornado config를 로드하여 반환."""
    with open(_CONFIG_PATH, "r", encoding="utf-8") as f:
        data = json.load(f)
    return data["examples"][0]["config"]


def get_config_json(indent: int = 2) -> str:
    """FireTornado config를 JSON 문자열로 반환."""
    return json.dumps(load_config(), indent=indent, ensure_ascii=False)


def build(output_dir: str = _DEFAULT_OUTPUT_DIR) -> str:
    """Unreal Editor에서 McpBuildNiagaraSystem을 호출하여 FireTornado 생성.

    Args:
        output_dir: 출력 디렉토리 (기본: /Game/HktGameplay/Generated)

    Returns:
        빌드 결과 문자열
    """
    try:
        import unreal
    except ImportError:
        print("ERROR: unreal 모듈을 찾을 수 없습니다. Unreal Editor Python 환경에서 실행하세요.")
        return ""

    config = load_config()
    json_str = json.dumps(config)
    result = unreal.HktVFXGeneratorFunctionLibrary.mcp_build_niagara_system(
        json_str, output_dir
    )
    print(f"FireTornado 빌드 결과: {result}")
    return result


def print_summary():
    """FireTornado config 요약을 출력."""
    config = load_config()
    print(f"=== {config['systemName']} ===")
    print(f"  looping: {config.get('looping', False)}")
    print(f"  warmupTime: {config.get('warmupTime', 0)}")
    print(f"  emitters: {len(config['emitters'])}")
    print()

    for em in config["emitters"]:
        name = em["name"]
        spawn = em.get("spawn", {})
        init = em.get("init", {})
        update = em.get("update", {})
        render = em.get("render", {})

        print(f"  [{name}]")
        print(f"    template: {render.get('emitterTemplate', 'N/A')}")
        print(f"    material: {render.get('materialPath', '(default)')}")
        print(f"    spawn: mode={spawn.get('mode')}, rate={spawn.get('rate', 'N/A')}")
        print(f"    lifetime: {init.get('lifetimeMin')}~{init.get('lifetimeMax')}")
        print(f"    size: {init.get('sizeMin')}~{init.get('sizeMax')}")

        color = init.get("color", {})
        print(f"    color: ({color.get('r',0)}, {color.get('g',0)}, {color.get('b',0)}, {color.get('a',1)})")

        if "vortexStrength" in update:
            print(f"    vortex: strength={update['vortexStrength']}, attraction={update.get('attractionStrength', 0)}")
        if "accelerationForce" in update:
            af = update["accelerationForce"]
            print(f"    accelerationForce: ({af['x']}, {af['y']}, {af['z']})")

        shape = em.get("shapeLocation")
        if shape:
            print(f"    shapeLocation: {shape['shape']}, radius={shape.get('cylinderRadius', 'N/A')}, height={shape.get('cylinderHeight', 'N/A')}")

        print()


def _monolith_comparison():
    """monolith 원본과 HktVFXGenerator 매핑 비교표 출력."""
    print("=== monolith → HktVFXGenerator 매핑 비교 ===\n")
    mapping = [
        ("create_system(NS_FireTornado)", 'systemName: "FireTornado"'),
        ("add_emitter(NE_Smoke, FireCore)", 'emitterTemplate: "smoke"'),
        ("add_emitter(NE_Smoke, Smoke)", 'emitterTemplate: "smoke"'),
        ("add_emitter(NE_Sparks, Embers)", 'emitterTemplate: "spark"'),
        ("add_emitter(NE_Core, InnerGlow)", 'emitterTemplate: "core"'),
        ("set_renderer_material(MI_Flames)", "render.materialPath"),
        ("set_renderer_material(MI_Emissive)", "render.materialPath"),
        ("add_module(VortexForce)", "update.vortexStrength + vortexAxis"),
        ("VortexForce Amount", "update.vortexStrength"),
        ("VortexForce Origin Pull", "update.attractionStrength"),
        ("VortexForce Falloff Radius", "update.vortexRadius"),
        ("LinearForce (0,0,Z)", "update.accelerationForce"),
        ("ShapeLocation Cylinder", 'shapeLocation.shape: "cylinder"'),
        ("SpawnBurst→SpawnRate 전환", 'spawn.mode: "rate"'),
        ("set_module_enabled", "(implicit — mode로 자동 처리)"),
        ("request_compile", "(McpBuildNiagaraSystem에서 자동 처리)"),
    ]

    for monolith_action, vfx_gen in mapping:
        print(f"  {monolith_action:<42} → {vfx_gen}")
    print()


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="FireTornado VFX Config Generator")
    parser.add_argument(
        "--output-dir",
        default=_DEFAULT_OUTPUT_DIR,
        help=f"Unreal output directory (default: {_DEFAULT_OUTPUT_DIR})",
    )
    parser.add_argument(
        "--json", action="store_true", help="JSON config만 stdout으로 출력"
    )
    parser.add_argument(
        "--summary", action="store_true", help="config 요약 출력"
    )
    parser.add_argument(
        "--compare", action="store_true", help="monolith 매핑 비교표 출력"
    )

    args = parser.parse_args()

    if args.json:
        print(get_config_json())
    elif args.summary:
        print_summary()
    elif args.compare:
        _monolith_comparison()
    else:
        print_summary()
        print("---")
        _monolith_comparison()
        print("--- JSON Config ---")
        print(get_config_json())

    build()
