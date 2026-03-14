# HKT MCP Server for Unreal Engine 5

MCP (Model Context Protocol) 서버로, LLM 클라이언트(Claude Desktop, Cursor 등)가 Unreal Engine 5 에디터와 직접 대화하며 콘텐츠를 생성할 수 있게 합니다.

---

## 🔥 핵심 개념: MCP란?

**MCP(Model Context Protocol)**는 AI(Claude, GPT 등)가 외부 도구를 사용할 수 있게 해주는 프로토콜입니다.

### ❓ 어디에 명령을 입력하나요?

**Unreal Editor 안에 입력하는 게 아닙니다!**

명령은 **Claude Desktop 앱** 또는 **Cursor의 AI 채팅창**에 자연어로 입력합니다.

```
┌─────────────────────────────────────────────────────────────┐
│  Claude Desktop 또는 Cursor AI Chat                         │
│  ┌─────────────────────────────────────────────────────────┐│
│  │  💬 "현재 레벨에 있는 모든 액터 목록 보여줘"              ││
│  │                                                         ││
│  │  🤖 Claude가 list_actors 도구를 호출합니다...            ││
│  │     결과: StaticMeshActor (5개), PointLight (3개)...    ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

---

## 📊 데이터 흐름 상세 설명

### 전체 아키텍처

```
┌──────────────────┐
│   👤 사용자       │  "BP_Player를 (0,0,100)에 스폰해줘"
└────────┬─────────┘
         │ 자연어 입력
         ▼
┌──────────────────────────────────────────┐
│   🤖 LLM Client (Claude Desktop/Cursor)   │
│                                          │
│   Claude가 요청을 분석하고 적절한 도구 선택  │
│   → spawn_actor 도구 호출 결정            │
└────────┬─────────────────────────────────┘
         │ JSON-RPC (stdio)
         ▼
┌──────────────────────────────────────────┐
│   🐍 MCP Server (Python)                  │
│   (hkt_mcp.server)                       │
│                                          │
│   Tool 요청 수신 → spawn_actor 실행       │
└────────┬─────────────────────────────────┘
         │ unreal Python API
         ▼
┌──────────────────────────────────────────┐
│   🎮 Unreal Engine 5 Editor               │
│   (HktMcpEditorSubsystem)                │
│                                          │
│   실제로 액터 스폰 실행                    │
└────────┬─────────────────────────────────┘
         │ 결과 반환
         ▼
┌──────────────────────────────────────────┐
│   👤 사용자에게 결과 표시                  │
│   "BP_Player_C_0 액터가 스폰되었습니다"    │
└──────────────────────────────────────────┘
```

### 단계별 상세 설명

#### 1️⃣ 사용자 입력 (Claude Desktop / Cursor)

**입력 위치:**
- **Claude Desktop**: 앱 실행 후 채팅창에 자연어로 입력
- **Cursor**: Cmd+L (또는 Ctrl+L) → AI 채팅창에 자연어로 입력

**예시 입력:**
```
현재 열려있는 레벨의 모든 StaticMeshActor 위치를 알려줘
```

#### 2️⃣ LLM 분석 및 도구 선택

Claude가 사용자 요청을 분석하고, 사용 가능한 MCP 도구 중 적절한 것을 선택합니다.

**Claude의 내부 판단:**
```
사용자가 StaticMeshActor 목록을 원함
→ list_actors 도구 사용
→ class_filter: "StaticMeshActor"
```

#### 3️⃣ MCP 서버로 도구 호출 (JSON-RPC)

Claude가 MCP 서버로 보내는 실제 메시지:

```json
{
  "jsonrpc": "2.0",
  "id": "req-001",
  "method": "tools/call",
  "params": {
    "name": "list_actors",
    "arguments": {
      "class_filter": "StaticMeshActor"
    }
  }
}
```

#### 4️⃣ MCP 서버 처리

`server.py`가 요청을 받아 처리:

```python
# server.py의 dispatch_tool 함수
async def dispatch_tool(name: str, arguments: dict):
    if name == "list_actors":
        return await level_tools.list_actors(
            bridge,
            arguments.get("class_filter", "")
        )
```

#### 5️⃣ Unreal Engine 연동

`editor_bridge.py`가 실제 UE5 API 호출:

```python
# 에디터 내에서 실행될 때 (unreal 모듈 사용)
actors = unreal.EditorLevelLibrary.get_all_level_actors()
for actor in actors:
    if "StaticMeshActor" in actor.get_class().get_name():
        # 액터 정보 수집
```

#### 6️⃣ 응답 반환

MCP 서버가 Claude에게 보내는 응답:

```json
{
  "jsonrpc": "2.0",
  "id": "req-001",
  "result": {
    "content": [
      {
        "type": "text",
        "text": "{\"count\": 5, \"actors\": [{\"name\": \"SM_Floor\", \"location\": {\"x\": 0, \"y\": 0, \"z\": 0}}, ...]}"
      }
    ]
  }
}
```

#### 7️⃣ 사용자에게 결과 표시

Claude가 결과를 자연어로 변환하여 표시:

```
현재 레벨에서 5개의 StaticMeshActor를 찾았습니다:

1. SM_Floor - 위치: (0, 0, 0)
2. SM_Wall_01 - 위치: (100, 0, 0)
3. SM_Wall_02 - 위치: (-100, 0, 0)
...
```

---

## 🎯 실제 사용 시나리오

### 시나리오 1: 에셋 탐색

**Cursor AI 채팅에서:**

```
👤 나: /Game/Blueprints 폴더에 어떤 블루프린트들이 있어?

🤖 Claude: MCP 서버의 list_assets 도구를 사용하여 확인하겠습니다.

[도구 호출: list_assets]
  - path: "/Game/Blueprints"
  - class_filter: "Blueprint"

📋 결과:
/Game/Blueprints 경로에서 3개의 블루프린트를 찾았습니다:
  • BP_Player (Blueprint) - /Game/Blueprints/BP_Player
  • BP_Enemy (Blueprint) - /Game/Blueprints/BP_Enemy
  • BP_Projectile (Blueprint) - /Game/Blueprints/BP_Projectile
```

### 시나리오 2: 액터 스폰

**Claude Desktop에서:**

```
👤 나: BP_Enemy를 플레이어 근처 (500, 200, 0) 위치에 3개 스폰해줘

🤖 Claude: spawn_actor 도구를 사용하여 3개의 적을 스폰하겠습니다.

[도구 호출: spawn_actor] x 3
  - blueprint_path: "/Game/Blueprints/BP_Enemy"
  - location: {"x": 500, "y": 200, "z": 0}
  - location: {"x": 600, "y": 200, "z": 0}
  - location: {"x": 700, "y": 200, "z": 0}

✅ 3개의 BP_Enemy 액터가 성공적으로 스폰되었습니다:
  • BP_Enemy_C_0 at (500, 200, 0)
  • BP_Enemy_C_1 at (600, 200, 0)
  • BP_Enemy_C_2 at (700, 200, 0)
```

### 시나리오 3: 레벨 수정

```
👤 나: 현재 선택된 액터를 100 유닛 위로 올려줘

🤖 Claude: 먼저 선택된 액터를 확인하고 위치를 수정하겠습니다.

[도구 호출: get_selected_actors]
현재 선택: SM_Cube_01 at (0, 0, 50)

[도구 호출: modify_actor]
  - actor_name: "SM_Cube_01"
  - location: {"x": 0, "y": 0, "z": 150}

✅ SM_Cube_01이 (0, 0, 150)으로 이동되었습니다.
```

### 시나리오 4: PIE 제어

```
👤 나: 게임 플레이 시작하고 5초 후에 중지해줘

🤖 Claude: PIE를 시작하고 5초 후 중지하겠습니다.

[도구 호출: start_pie]
✅ PIE 세션이 시작되었습니다.

(5초 대기)

[도구 호출: stop_pie]
✅ PIE 세션이 중지되었습니다.
```

---

## ⚙️ 설치 및 설정

### 1. Python 의존성 설치

```bash
cd Plugins/HktGameplayGenerator/McpServer
pip install -e .
```

### 2. Unreal Engine 플러그인 활성화

1. Unreal Editor 실행
2. Edit → Plugins
3. "HktGameplayGenerator" 검색 후 활성화
4. 에디터 재시작

### 3. Claude Desktop 설정

**Windows:** `%APPDATA%\Claude\claude_desktop_config.json`
**macOS:** `~/Library/Application Support/Claude/claude_desktop_config.json`

```json
{
  "mcpServers": {
    "hkt-unreal": {
      "command": "python",
      "args": ["-m", "hkt_mcp.server"],
      "env": {
        "UE_PROJECT_PATH": "E:/WS/UE5/HktProto",
        "PYTHONPATH": "E:/WS/UE5/HktProto/Plugins/HktGameplayGenerator/McpServer/src"
      }
    }
  }
}
```

### 4. Cursor 설정

프로젝트 루트에 `.cursor/mcp.json` 생성:

```json
{
  "mcpServers": {
    "hkt-unreal": {
      "command": "python",
      "args": ["-m", "hkt_mcp.server"],
      "cwd": "E:/WS/UE5/HktProto/Plugins/HktGameplayGenerator/McpServer",
      "env": {
        "UE_PROJECT_PATH": "E:/WS/UE5/HktProto",
        "PYTHONPATH": "E:/WS/UE5/HktProto/Plugins/HktGameplayGenerator/McpServer/src"
      }
    }
  }
}
```

---

## 📋 사용 가능한 MCP Tools

### Asset Tools
| Tool | 설명 | 파라미터 |
|------|------|----------|
| `list_assets` | 경로의 에셋 목록 | `path`, `class_filter` |
| `get_asset_info` | 에셋 상세 정보 | `asset_path` |
| `search_assets` | 이름으로 검색 | `query`, `class_filter` |
| `modify_asset` | 속성 수정 | `asset_path`, `property_name`, `new_value` |

### Level Tools
| Tool | 설명 | 파라미터 |
|------|------|----------|
| `list_actors` | 레벨 액터 목록 | `class_filter` |
| `spawn_actor` | 블루프린트 스폰 | `blueprint_path`, `location`, `rotation`, `label` |
| `modify_actor` | Transform 수정 | `actor_name`, `location`, `rotation`, `scale` |
| `delete_actor` | 액터 삭제 | `actor_name` |
| `select_actor` | 액터 선택 | `actor_name` |

### Query Tools
| Tool | 설명 | 파라미터 |
|------|------|----------|
| `search_classes` | 클래스 검색 | `query`, `blueprint_only` |
| `get_class_properties` | 속성 조회 | `class_name` |
| `get_project_structure` | 폴더 구조 | `root_path` |
| `get_level_info` | 레벨 정보 | - |

### Runtime Tools
| Tool | 설명 | 파라미터 |
|------|------|----------|
| `start_pie` | PIE 시작 | - |
| `stop_pie` | PIE 중지 | - |
| `execute_console_command` | 콘솔 커맨드 | `command` |
| `get_game_state` | 게임 상태 | - |

### Utility Tools
| Tool | 설명 | 파라미터 |
|------|------|----------|
| `get_viewport_camera` | 카메라 위치 | - |
| `set_viewport_camera` | 카메라 이동 | `location`, `rotation` |
| `show_notification` | 알림 표시 | `message`, `duration` |

---

## 🔧 트러블슈팅

### MCP 서버가 연결되지 않음

1. Claude Desktop/Cursor 재시작
2. Python 경로 확인 (`where python` 또는 `which python`)
3. 설정 파일 JSON 문법 오류 확인

### "unreal 모듈을 찾을 수 없음"

MCP 서버는 UE 에디터 외부에서 실행됩니다. 일부 기능은 에디터 내 Python 환경에서만 동작합니다.

### 도구가 작동하지 않음

1. Unreal Editor가 실행 중인지 확인
2. HktGameplayGenerator 플러그인이 활성화되어 있는지 확인

---

## 라이선스

MIT License
