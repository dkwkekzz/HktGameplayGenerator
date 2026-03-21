#!/usr/bin/env python3
"""
hkt_story_inject.py — 실시간 Story JSON 주입 CLI

UE5 에디터가 실행 중일 때 Story JSON을 실시간으로 컴파일 + VM 등록.

사용법:
  # 파일에서 주입
  python hkt_story_inject.py inject story.json

  # stdin에서 직접 주입
  echo '{"storyTag":"Test.Hello","steps":[{"op":"Log","message":"hello"},{"op":"Halt"}]}' | python hkt_story_inject.py inject -

  # 디렉토리 감시 (파일 추가/변경 시 자동 주입)
  python hkt_story_inject.py watch ./stories/

  # 검증만 (등록 안 함)
  python hkt_story_inject.py validate story.json

  # 등록된 Story 목록
  python hkt_story_inject.py list

  # 스키마 확인
  python hkt_story_inject.py schema
"""

import argparse
import json
import sys
import time
import os
import urllib.request
import urllib.error

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 30010  # UE5 Remote Control API default port
OBJECT_PATH = "/Script/HktStoryGenerator.Default__HktStoryGeneratorFunctionLibrary"


def call_ue5(method: str, host: str, port: int, **kwargs) -> dict:
    """UE5 Remote Control API 호출"""
    url = f"http://{host}:{port}/remote/object/call"
    payload = {
        "objectPath": OBJECT_PATH,
        "functionName": method,
    }
    if kwargs:
        payload["parameters"] = kwargs

    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        headers={"Content-Type": "application/json"},
        method="PUT",
    )

    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.URLError as e:
        return {"error": f"UE5 연결 실패: {e}"}
    except Exception as e:
        return {"error": str(e)}


def inject_story(json_str: str, host: str, port: int) -> dict:
    """Story JSON을 컴파일하여 VM에 등록"""
    return call_ue5("McpBuildStory", host, port, JsonStory=json_str)


def validate_story(json_str: str, host: str, port: int) -> dict:
    """Story JSON 문법 검증"""
    return call_ue5("McpValidateStory", host, port, JsonStory=json_str)


def list_stories(host: str, port: int) -> dict:
    """등록된 Story 목록"""
    return call_ue5("McpListGeneratedStories", host, port)


def get_schema(host: str, port: int) -> dict:
    """Story JSON 스키마"""
    return call_ue5("McpGetStorySchema", host, port)


def load_json_file(path: str) -> str:
    """JSON 파일 로드 (stdin은 '-')"""
    if path == "-":
        return sys.stdin.read()
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def cmd_inject(args):
    json_str = load_json_file(args.file)
    result = inject_story(json_str, args.host, args.port)
    print(json.dumps(result, indent=2, ensure_ascii=False))
    if "error" in result:
        sys.exit(1)


def cmd_validate(args):
    json_str = load_json_file(args.file)
    result = validate_story(json_str, args.host, args.port)
    print(json.dumps(result, indent=2, ensure_ascii=False))


def cmd_list(args):
    result = list_stories(args.host, args.port)
    print(json.dumps(result, indent=2, ensure_ascii=False))


def cmd_schema(args):
    result = get_schema(args.host, args.port)
    print(json.dumps(result, indent=2, ensure_ascii=False))


def cmd_watch(args):
    """디렉토리 감시 — .json 파일 추가/변경 시 자동 주입"""
    watch_dir = args.directory
    if not os.path.isdir(watch_dir):
        print(f"Error: '{watch_dir}' is not a directory")
        sys.exit(1)

    print(f"Watching {watch_dir} for .json changes... (Ctrl+C to stop)")
    processed: dict[str, float] = {}

    # 초기 파일 목록 기록 (이미 있는 파일은 처리하지 않음)
    for name in os.listdir(watch_dir):
        if name.endswith(".json"):
            path = os.path.join(watch_dir, name)
            processed[path] = os.path.getmtime(path)

    try:
        while True:
            for name in os.listdir(watch_dir):
                if not name.endswith(".json"):
                    continue
                path = os.path.join(watch_dir, name)
                mtime = os.path.getmtime(path)

                if path not in processed or processed[path] < mtime:
                    processed[path] = mtime
                    print(f"\n--- Injecting: {name} ---")
                    try:
                        json_str = load_json_file(path)
                        result = inject_story(json_str, args.host, args.port)
                        # 결과에서 성공 여부 추출
                        result_data = result.get("ReturnValue", result)
                        if isinstance(result_data, str):
                            try:
                                result_data = json.loads(result_data)
                            except json.JSONDecodeError:
                                pass
                        if isinstance(result_data, dict) and result_data.get("success"):
                            print(f"  OK: {result_data.get('storyTag', name)}")
                        else:
                            print(f"  FAIL: {json.dumps(result_data, indent=2, ensure_ascii=False)}")
                    except Exception as e:
                        print(f"  ERROR: {e}")

            time.sleep(1)
    except KeyboardInterrupt:
        print("\nStopped.")


def main():
    parser = argparse.ArgumentParser(
        description="HktStory 실시간 주입 CLI",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--host", default=DEFAULT_HOST, help=f"UE5 호스트 (기본: {DEFAULT_HOST})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"UE5 포트 (기본: {DEFAULT_PORT})")

    sub = parser.add_subparsers(dest="command", required=True)

    p_inject = sub.add_parser("inject", help="Story JSON 주입 (컴파일 + VM 등록)")
    p_inject.add_argument("file", help="JSON 파일 경로 (또는 '-'로 stdin)")
    p_inject.set_defaults(func=cmd_inject)

    p_validate = sub.add_parser("validate", help="Story JSON 문법 검증")
    p_validate.add_argument("file", help="JSON 파일 경로")
    p_validate.set_defaults(func=cmd_validate)

    p_list = sub.add_parser("list", help="등록된 Story 목록")
    p_list.set_defaults(func=cmd_list)

    p_schema = sub.add_parser("schema", help="Story JSON 스키마 확인")
    p_schema.set_defaults(func=cmd_schema)

    p_watch = sub.add_parser("watch", help="디렉토리 감시 (자동 주입)")
    p_watch.add_argument("directory", help="감시할 디렉토리")
    p_watch.set_defaults(func=cmd_watch)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
