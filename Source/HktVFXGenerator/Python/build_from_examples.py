# build_from_examples.py
# Unreal Editor > Python Script (Tools > Execute Python Script) 로 실행

import json
import sys
from pathlib import Path

import unreal

# vfx_production_examples.py 경로 추가
sys.path.insert(0, str(Path(__file__).parent))
from vfx_production_examples import load_examples

# ── 옵션 ──────────────────────────────────────────────────────────────────────
FILTER_CATEGORY = ""    # "explosion" / "weapon" / "magic" / "" (전체)
FILTER_QUALITY  = ""    # "production" / "basic" / "" (전체)
FILTER_IDS      = []    # ["explosion_fire", "campfire_deluxe"] / [] (전체)
OUTPUT_DIR      = ""    # "/Game/VFX/Test/" / "" (기본 경로)
# ─────────────────────────────────────────────────────────────────────────────

def collect_targets():
    examples = load_examples()
    if FILTER_IDS:
        return [(e["_id"], e["config"]) for e in examples if e["_id"] in FILTER_IDS]
    if FILTER_CATEGORY:
        return [(e["_id"], e["config"]) for e in examples if e["_category"] == FILTER_CATEGORY]
    if FILTER_QUALITY:
        return [(e["_id"], e["config"]) for e in examples if e.get("_quality") == FILTER_QUALITY]
    return [(e["_id"], e["config"]) for e in examples]


def main():
    targets = collect_targets()
    unreal.log(f"[VFXBuild] 총 {len(targets)}개 빌드 시작")

    success, failed = [], []

    with unreal.ScopedSlowTask(len(targets), "Niagara 시스템 빌드 중...") as task:
        task.make_dialog(True)

        for eid, config in targets:
            system_name = config.get("systemName", eid)
            task.enter_progress_frame(1, f"빌드: {system_name}")

            if task.should_cancel():
                unreal.log_warning("[VFXBuild] 사용자가 취소했습니다.")
                break

            json_str = json.dumps(config, ensure_ascii=False)
            result_str = unreal.HktVFXGeneratorFunctionLibrary.mcp_build_niagara_system(
                json_str, OUTPUT_DIR
            )

            try:
                result = json.loads(result_str)
            except json.JSONDecodeError:
                result = {"success": False, "error": result_str}

            if result.get("success"):
                asset_path = result.get("assetPath", "")
                unreal.log(f"[VFXBuild] OK  {system_name}  →  {asset_path}")
                success.append(eid)

                # textureRequests가 있으면 별도 처리 필요
                if result.get("textureRequests"):
                    unreal.log_warning(
                        f"[VFXBuild] '{system_name}' 에 textureRequests 있음 — 수동 처리 필요"
                    )
            else:
                err = result.get("error", "unknown")
                unreal.log_error(f"[VFXBuild] FAIL  {system_name}  ({err})")
                failed.append((eid, err))

    unreal.log(f"[VFXBuild] 완료: {len(success)}개 성공 / {len(failed)}개 실패")
    for eid, err in failed:
        unreal.log_error(f"  - {eid}: {err}")


main()
