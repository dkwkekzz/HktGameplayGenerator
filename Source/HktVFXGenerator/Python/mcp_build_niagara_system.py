"""MCP Build Niagara System — 바로 실행 가능한 VFX 빌드 스크립트

UE5 에디터 Python 환경에서 직접 실행하여 Niagara VFX 시스템을 빌드합니다.

사용법:
    === UE5 에디터 Python 콘솔에서 직접 실행 ===

    # 방법 1: JSON 파일에서 빌드
    import mcp_build_niagara_system as mcp
    result = mcp.build_from_file("/path/to/config.json")

    # 방법 2: JSON 문자열로 빌드
    result = mcp.build(json_string)

    # 방법 3: Python dict로 빌드
    result = mcp.build_from_dict(config_dict)

    # 방법 4: 예제 Config로 빌드 (테스트용)
    result = mcp.build_example("aaa_explosion")

    # 방법 5: 프리셋 폭발 이펙트 빌드 (빠른 테스트)
    result = mcp.build_preset_explosion("TestExplosion", r=1.0, g=0.5, b=0.1)

    # 유틸리티
    guide = mcp.get_prompt_guide()        # LLM용 디자인 가이드
    examples = mcp.get_example_configs()   # 예제 Config 목록
    schema = mcp.get_config_schema()       # JSON 스키마
    assets = mcp.list_generated_vfx()      # 생성된 에셋 목록
    mcp.dump_template_params("sprite")     # 템플릿 파라미터 덤프

    === 커맨드라인 실행 (UE5 -ExecutePythonScript) ===

    # JSON 파일에서 빌드
    UnrealEditor-Cmd.exe project.uproject -ExecutePythonScript="mcp_build_niagara_system.py --file config.json"

    # 예제 빌드
    UnrealEditor-Cmd.exe project.uproject -ExecutePythonScript="mcp_build_niagara_system.py --example aaa_explosion"

    # 프리셋 폭발
    UnrealEditor-Cmd.exe project.uproject -ExecutePythonScript="mcp_build_niagara_system.py --preset TestBoom --color 1,0.5,0.1"

    # 전체 예제 빌드 (배치)
    UnrealEditor-Cmd.exe project.uproject -ExecutePythonScript="mcp_build_niagara_system.py --build-all"

    # stdin에서 JSON 읽기
    echo '{"systemName":"Test",...}' | UnrealEditor-Cmd.exe project.uproject -ExecutePythonScript="mcp_build_niagara_system.py --stdin"
"""

import json
import sys
from pathlib import Path
from typing import Optional

# ---------------------------------------------------------------------------
# Unreal Engine import (에디터 환경에서만 사용 가능)
# ---------------------------------------------------------------------------
try:
    import unreal

    _HAS_UNREAL = True
except ImportError:
    _HAS_UNREAL = False

# ---------------------------------------------------------------------------
# 로컬 예제 로더
# ---------------------------------------------------------------------------
_SCRIPT_DIR = Path(__file__).parent
sys.path.insert(0, str(_SCRIPT_DIR))
from vfx_production_examples import (
    get_config,
    get_config_json,
    get_all_configs,
    list_examples,
    load_examples,
)


# ===========================================================================
# 내부 헬퍼
# ===========================================================================

def _ensure_unreal():
    """UE5 Python 환경인지 확인."""
    if not _HAS_UNREAL:
        raise RuntimeError(
            "이 스크립트는 UE5 에디터 Python 환경에서만 실행할 수 있습니다.\n"
            "UE5 에디터의 Python 콘솔 또는 -ExecutePythonScript 로 실행하세요."
        )


def _call_build(json_config: str, output_dir: str = "") -> dict:
    """C++ McpBuildNiagaraSystem 호출 후 결과 dict 반환."""
    _ensure_unreal()
    result_json = unreal.HktVFXGeneratorFunctionLibrary.mcp_build_niagara_system(
        json_config, output_dir
    )
    return json.loads(result_json)


def _log(msg: str):
    """UE5 로그 또는 stdout 출력."""
    if _HAS_UNREAL:
        unreal.log(msg)
    print(msg)


# ===========================================================================
# 공개 API — 빌드
# ===========================================================================

def build(json_config: str, output_dir: str = "") -> dict:
    """JSON 문자열로 Niagara 시스템 빌드.

    Args:
        json_config: VFX Config JSON 문자열
        output_dir: 출력 디렉토리 (빈 문자열이면 기본값 /Game/GeneratedVFX)

    Returns:
        {"success": True, "assetPath": "..."} 또는 {"success": False, "error": "..."}
    """
    result = _call_build(json_config, output_dir)
    if result.get("success"):
        _log(f"[MCP] 빌드 성공: {result['assetPath']}")
    else:
        _log(f"[MCP] 빌드 실패: {result.get('error', 'unknown')}")
    return result


def build_from_dict(config: dict, output_dir: str = "") -> dict:
    """Python dict로 Niagara 시스템 빌드.

    Args:
        config: VFX Config dict (systemName, emitters 필수)
        output_dir: 출력 디렉토리

    Returns:
        빌드 결과 dict
    """
    json_str = json.dumps(config, ensure_ascii=False)
    return build(json_str, output_dir)


def build_from_file(file_path: str, output_dir: str = "") -> dict:
    """JSON 파일에서 Config 읽어 Niagara 시스템 빌드.

    Args:
        file_path: JSON 파일 경로 (절대 또는 상대)
        output_dir: 출력 디렉토리

    Returns:
        빌드 결과 dict
    """
    path = Path(file_path)
    if not path.exists():
        raise FileNotFoundError(f"Config 파일을 찾을 수 없습니다: {file_path}")

    json_str = path.read_text(encoding="utf-8")

    # 파일이 examples 형식이면 config 추출
    data = json.loads(json_str)
    if "config" in data and "systemName" not in data:
        json_str = json.dumps(data["config"], ensure_ascii=False)

    _log(f"[MCP] 파일에서 빌드: {file_path}")
    return build(json_str, output_dir)


def build_example(example_id: str, output_dir: str = "") -> dict:
    """예제 Config ID로 빌드 (테스트/검증용).

    사용 가능한 예제 ID:
        explosion_fire, water_fountain, confetti, dust_storm, firefly,
        gun_impact, campfire_basic, magic_portal, tornado, energy_implosion,
        magic_rune, aaa_explosion, muzzle_flash, sword_slash, bullet_impact,
        electric_arc, healing_aura, campfire_deluxe, magic_impact

    Args:
        example_id: 예제 ID
        output_dir: 출력 디렉토리

    Returns:
        빌드 결과 dict
    """
    config_json = get_config_json(example_id)
    if config_json is None:
        available = [ex["id"] for ex in list_examples()]
        raise ValueError(
            f"예제 '{example_id}'를 찾을 수 없습니다.\n"
            f"사용 가능: {', '.join(available)}"
        )

    _log(f"[MCP] 예제 빌드: {example_id}")
    return build(config_json, output_dir)


def build_preset_explosion(
    name: str,
    r: float = 1.0,
    g: float = 0.5,
    b: float = 0.1,
    intensity: float = 0.5,
    output_dir: str = "",
) -> dict:
    """프리셋 폭발 이펙트 빌드 (빠른 테스트용).

    Args:
        name: 에셋 이름
        r, g, b: 색상 (0~10+)
        intensity: 강도 (0~1)
        output_dir: 출력 디렉토리

    Returns:
        빌드 결과 dict
    """
    _ensure_unreal()
    result_json = unreal.HktVFXGeneratorFunctionLibrary.mcp_build_preset_explosion(
        name, r, g, b, intensity, output_dir
    )
    result = json.loads(result_json)
    if result.get("success"):
        _log(f"[MCP] 프리셋 폭발 빌드 성공: {result['assetPath']}")
    else:
        _log(f"[MCP] 프리셋 폭발 빌드 실패: {result.get('error', 'unknown')}")
    return result


def build_all_examples(output_dir: str = "") -> list[dict]:
    """모든 예제를 일괄 빌드 (검증/데모용).

    Returns:
        각 예제의 빌드 결과 리스트
    """
    results = []
    for ex_info in list_examples():
        _log(f"[MCP] 배치 빌드 [{ex_info['id']}] {ex_info['description']}...")
        try:
            result = build_example(ex_info["id"], output_dir)
            result["_example_id"] = ex_info["id"]
            results.append(result)
        except Exception as e:
            results.append({
                "success": False,
                "_example_id": ex_info["id"],
                "error": str(e),
            })

    success_count = sum(1 for r in results if r.get("success"))
    _log(f"[MCP] 배치 빌드 완료: {success_count}/{len(results)} 성공")
    return results


# ===========================================================================
# 공개 API — 조회/유틸리티
# ===========================================================================

def get_prompt_guide() -> str:
    """LLM용 VFX Config 디자인 가이드 반환."""
    _ensure_unreal()
    return unreal.HktVFXGeneratorFunctionLibrary.mcp_get_vfx_prompt_guide()


def get_example_configs() -> str:
    """예제 Config JSON 목록 반환."""
    _ensure_unreal()
    return unreal.HktVFXGeneratorFunctionLibrary.mcp_get_vfx_example_configs()


def get_config_schema() -> str:
    """VFX Config JSON 스키마 반환."""
    _ensure_unreal()
    return unreal.HktVFXGeneratorFunctionLibrary.mcp_get_vfx_config_schema()


def list_generated_vfx(directory: str = "") -> dict:
    """생성된 VFX 에셋 목록 반환."""
    _ensure_unreal()
    result_json = unreal.HktVFXGeneratorFunctionLibrary.mcp_list_generated_vfx(directory)
    return json.loads(result_json)


def dump_template_params(renderer_type: str = "sprite") -> str:
    """템플릿 에미터의 RapidIterationParameter 덤프."""
    _ensure_unreal()
    return unreal.HktVFXGeneratorFunctionLibrary.mcp_dump_template_parameters(renderer_type)


def dump_all_template_params() -> str:
    """모든 템플릿 에미터의 파라미터 덤프."""
    _ensure_unreal()
    return unreal.HktVFXGeneratorFunctionLibrary.mcp_dump_all_template_parameters()


# ===========================================================================
# 공개 API — 오프라인 (UE5 없이 사용 가능)
# ===========================================================================

def validate_config(config: dict) -> list[str]:
    """Config JSON의 유효성을 오프라인으로 검증.

    Args:
        config: VFX Config dict

    Returns:
        에러 메시지 리스트 (빈 리스트면 유효)
    """
    errors = []

    if not isinstance(config, dict):
        return ["config는 dict여야 합니다"]

    if "systemName" not in config:
        errors.append("systemName 필드가 필요합니다")
    elif not isinstance(config["systemName"], str) or not config["systemName"]:
        errors.append("systemName은 비어있지 않은 문자열이어야 합니다")

    if "emitters" not in config:
        errors.append("emitters 배열이 필요합니다")
        return errors

    if not isinstance(config["emitters"], list) or len(config["emitters"]) == 0:
        errors.append("emitters는 1개 이상의 에미터가 필요합니다")
        return errors

    valid_modes = {"burst", "rate"}
    valid_renderer_types = {"sprite", "ribbon", "light", "mesh"}
    valid_blend_modes = {"additive", "translucent"}
    valid_alignments = {"unaligned", "velocity_aligned"}
    valid_templates = {
        "simple_sprite_burst", "omnidirectional_burst", "directional_burst",
        "confetti_burst", "fountain", "blowing_particles", "hanging_particulates",
        "upward_mesh_burst", "single_looping_particle", "ribbon",
        "dynamic_beam", "static_beam", "minimal", "recycle_particles",
        "spark", "spark_secondary", "spark_debris", "smoke", "explosion",
        "core", "debris", "dust", "ground_dust", "impact", "impact_mesh",
        "muzzle_flash", "arc", "flame", "flare",
    }

    emitter_names = set()
    for i, emitter in enumerate(config["emitters"]):
        prefix = f"emitters[{i}]"

        if "name" not in emitter:
            errors.append(f"{prefix}: name 필드가 필요합니다")
        else:
            name = emitter["name"]
            if name in emitter_names:
                errors.append(f"{prefix}: 중복된 이름 '{name}'")
            emitter_names.add(name)

        # spawn 검증
        spawn = emitter.get("spawn", {})
        mode = spawn.get("mode", "burst")
        if mode not in valid_modes:
            errors.append(f"{prefix}.spawn.mode: '{mode}'은(는) 유효하지 않음 ({valid_modes})")
        if mode == "rate" and spawn.get("rate", 0) <= 0:
            errors.append(f"{prefix}.spawn.rate: rate 모드에서는 0보다 큰 값 필요")
        if mode == "burst" and spawn.get("burstCount", 0) <= 0:
            errors.append(f"{prefix}.spawn.burstCount: burst 모드에서는 0보다 큰 값 필요")

        # render 검증
        render = emitter.get("render", {})
        rt = render.get("rendererType", "sprite")
        if rt not in valid_renderer_types:
            errors.append(f"{prefix}.render.rendererType: '{rt}' 유효하지 않음")

        template = render.get("emitterTemplate", "")
        if template and template not in valid_templates:
            errors.append(f"{prefix}.render.emitterTemplate: '{template}' 알 수 없는 템플릿")

        blend = render.get("blendMode", "additive")
        if blend not in valid_blend_modes:
            errors.append(f"{prefix}.render.blendMode: '{blend}' 유효하지 않음")

        align = render.get("alignment", "unaligned")
        if align not in valid_alignments:
            errors.append(f"{prefix}.render.alignment: '{align}' 유효하지 않음")

    return errors


def validate_json(json_string: str) -> list[str]:
    """JSON 문자열의 유효성을 검증.

    Returns:
        에러 메시지 리스트 (빈 리스트면 유효)
    """
    try:
        config = json.loads(json_string)
    except json.JSONDecodeError as e:
        return [f"JSON 파싱 실패: {e}"]
    return validate_config(config)


def list_available_examples() -> None:
    """사용 가능한 예제 목록 출력."""
    print("\n=== HKT VFX Production Examples ===\n")
    for info in list_examples():
        print(f"  [{info['id']}] {info['description']}")
        print(f"    category={info['category']}, quality={info['quality']}, "
              f"emitters={info['emitter_count']}, tags={info['tags']}")
    print()


# ===========================================================================
# CLI 엔트리 포인트
# ===========================================================================

def _parse_args(argv: list[str]) -> dict:
    """간단한 인자 파싱 (argparse 없이 UE5 호환)."""
    args = {
        "command": None,
        "file": None,
        "example": None,
        "preset_name": None,
        "color": (1.0, 0.5, 0.1),
        "intensity": 0.5,
        "output_dir": "",
        "validate_only": False,
        "list": False,
    }

    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == "--file" and i + 1 < len(argv):
            args["command"] = "file"
            args["file"] = argv[i + 1]
            i += 2
        elif arg == "--example" and i + 1 < len(argv):
            args["command"] = "example"
            args["example"] = argv[i + 1]
            i += 2
        elif arg == "--preset" and i + 1 < len(argv):
            args["command"] = "preset"
            args["preset_name"] = argv[i + 1]
            i += 2
        elif arg == "--color" and i + 1 < len(argv):
            parts = argv[i + 1].split(",")
            if len(parts) == 3:
                args["color"] = tuple(float(p) for p in parts)
            i += 2
        elif arg == "--intensity" and i + 1 < len(argv):
            args["intensity"] = float(argv[i + 1])
            i += 2
        elif arg == "--output-dir" and i + 1 < len(argv):
            args["output_dir"] = argv[i + 1]
            i += 2
        elif arg == "--stdin":
            args["command"] = "stdin"
            i += 1
        elif arg == "--build-all":
            args["command"] = "build_all"
            i += 1
        elif arg == "--validate" and i + 1 < len(argv):
            args["command"] = "validate"
            args["file"] = argv[i + 1]
            i += 2
        elif arg == "--list":
            args["command"] = "list"
            i += 1
        elif arg in ("--help", "-h"):
            args["command"] = "help"
            i += 1
        else:
            i += 1

    return args


def _print_help():
    print("""
mcp_build_niagara_system.py — HKT VFX Niagara 빌드 스크립트

사용법:
  --file <path>           JSON 파일에서 Config 읽어 빌드
  --example <id>          예제 Config으로 빌드 (aaa_explosion, campfire_deluxe, ...)
  --preset <name>         프리셋 폭발 이펙트 빌드
    --color R,G,B           색상 (기본: 1,0.5,0.1)
    --intensity <float>     강도 (기본: 0.5)
  --stdin                 stdin에서 JSON Config 읽어 빌드
  --build-all             모든 예제를 일괄 빌드
  --validate <path>       JSON 파일 유효성 검증 (빌드하지 않음)
  --list                  사용 가능한 예제 목록 출력
  --output-dir <path>     출력 디렉토리 (기본: /Game/GeneratedVFX)
  --help                  도움말 출력
""")


def main():
    """CLI 엔트리 포인트."""
    args = _parse_args(sys.argv[1:])
    command = args["command"]

    if command is None or command == "help":
        _print_help()
        return

    if command == "list":
        list_available_examples()
        return

    if command == "validate":
        path = Path(args["file"])
        if not path.exists():
            print(f"ERROR: 파일을 찾을 수 없습니다: {args['file']}")
            sys.exit(1)
        data = json.loads(path.read_text(encoding="utf-8"))
        config = data.get("config", data)
        errors = validate_config(config)
        if errors:
            print("검증 실패:")
            for err in errors:
                print(f"  - {err}")
            sys.exit(1)
        else:
            print("검증 통과! Config가 유효합니다.")
        return

    if command == "file":
        result = build_from_file(args["file"], args["output_dir"])

    elif command == "example":
        result = build_example(args["example"], args["output_dir"])

    elif command == "preset":
        r, g, b = args["color"]
        result = build_preset_explosion(
            args["preset_name"], r, g, b, args["intensity"], args["output_dir"]
        )

    elif command == "stdin":
        json_str = sys.stdin.read()
        result = build(json_str, args["output_dir"])

    elif command == "build_all":
        results = build_all_examples(args["output_dir"])
        print(json.dumps(results, indent=2, ensure_ascii=False))
        return

    else:
        _print_help()
        return

    print(json.dumps(result, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
