---
name: texture-gen
description: SD WebUI를 통한 텍스처 일괄 생성 스텝. 캐시 우선 확인 후, 미생성 텍스처만 SD WebUI로 생성하여 UE5에 임포트한다.
allowed-tools: Bash, Read, Write, Grep, Glob
argument-hint: <project_id | intent_json | "batch" intent_list_file>
---

# texture_generation 스텝 실행

텍스처 Intent를 받아 **캐시 확인 → SD WebUI 생성 → UE5 임포트**를 수행한다.
단독 실행 또는 다른 생성 스텝(char-gen, item-gen, vfx-gen)에서 호출된다.

## 인자

- `$1` — 다음 중 하나:
  - `project_id` — 프로젝트의 asset_discovery 결과에서 텍스처 목록 추출
  - 직접 intent JSON 문자열
  - `batch <파일경로>` — intent 목록이 담긴 JSON 파일

## 선행 조건

- UE5 에디터가 실행 중이어야 한다 (Remote Control API 활성)
- SD WebUI 서버가 실행 중이어야 한다 — **반드시 `check_sd_server_status`로 확인**

## 핵심 원칙: 서버 확인 → 배치 우선, 캐시 우선

0. **반드시 `check_sd_server_status`를 먼저 호출**하여 SD 서버 상태를 확인한다
   - `alive: true` → 정상, 생성 진행
   - `alive: false` → `launch_sd_server`로 자동 시작 시도 (batch file 설정 시)
   - batch file 미설정 → 사용자에게 Project Settings 설정 안내
1. **항상 `get_pending_texture_requests`를 먼저 호출**하여 캐시 히트를 걸러낸다
2. pending 목록만 SD WebUI로 생성한다 — 불필요한 생성을 방지
3. 다수 텍스처는 순차 호출하되, 각 생성 후 즉시 임포트한다

## 실행 절차

### 0. SD WebUI 서버 상태 확인

`check_sd_server_status` MCP 도구 호출:

```json
{
  "alive": true,
  "launching": false,
  "message": "Connected (http://127.0.0.1:7860)",
  "serverURL": "http://127.0.0.1:7860",
  "batchFilePath": "E:/AI/webui_forge/run.bat",
  "batchFileExists": true
}
```

- `alive: false`이면 `launch_sd_server`를 호출하여 자동 시작
- `batchFileExists: false`이면 사용자에게 안내:
  "SD WebUI 배치 파일 경로가 설정되지 않았습니다. Project Settings > HktGameplay > HktTextureGenerator에서 SDWebUIBatchFilePath를 설정하세요."
- 서버 시작 후 다시 `check_sd_server_status`로 확인

### 1. 텍스처 Intent 목록 수집

**project_id 모드:**
1. `.hkt_steps/{project_id}/asset_discovery/output.json` 읽기
2. characters/items/vfx 명세에서 필요한 텍스처 Intent 추출
3. 각 어셋 타입별 적절한 Usage 매핑:
   - VFX 스프라이트 → `ParticleSprite`
   - VFX 플립북 → `Flipbook4x4` 또는 `Flipbook8x8`
   - 아이템 아이콘 → `ItemIcon`
   - 머티리얼 베이스 → `MaterialBase`
   - 노멀맵 → `MaterialNormal`
   - 마스크 → `MaterialMask`
   - 노이즈/그라디언트 → `Noise` / `Gradient`

**직접 intent 모드:**
단일 intent JSON을 그대로 사용

**batch 모드:**
파일에서 intent 배열을 로드

### 2. 캐시 필터링 (배치)

`get_pending_texture_requests` MCP 도구 호출:
- `json_requests`: Intent 배열 JSON 문자열

```json
[
  {"name": "fire_sprite", "intent": {"usage": "ParticleSprite", "prompt": "fire particle sprite, vfx, game asset", "resolution": 256}},
  {"name": "sword_icon", "intent": {"usage": "ItemIcon", "prompt": "medieval sword icon, game ui", "resolution": 256}}
]
```

응답의 `pending` 배열만 생성 대상이다. 이미 캐시에 있는 것은 건너뛴다.

### 3. 텍스처 생성 (pending 항목만)

각 pending 텍스처에 대해 `generate_texture` MCP 도구 호출:
- `json_intent`: FHktTextureIntent JSON 문자열

**Intent JSON 형식:**
```json
{
  "usage": "ParticleSprite",
  "prompt": "fire particle sprite, orange flame, vfx, game asset, black background",
  "negativePrompt": "text, watermark, realistic photo",
  "resolution": 256,
  "alphaChannel": true,
  "tileable": false,
  "styleKeywords": ["stylized", "hand-painted"]
}
```

**Usage별 프롬프트 가이드:**

| Usage | 프롬프트 포인트 | 해상도 |
|---|---|---|
| `ParticleSprite` | "black background", "vfx sprite", "single element centered" | 256 |
| `Flipbook4x4` | "4x4 sprite sheet", "16 frames", "animation sequence" | 512 |
| `Flipbook8x8` | "8x8 sprite sheet", "64 frames" | 1024 |
| `ItemIcon` | "game icon", "clean border", "centered item" | 256 |
| `MaterialBase` | "seamless texture", "PBR albedo", "tileable" | 512~1024 |
| `MaterialNormal` | "normal map", "seamless", "tileable" | 512~1024 |
| `MaterialMask` | "roughness metallic mask", "grayscale", "tileable" | 512~1024 |
| `Noise` | "tileable noise", "seamless", "grayscale" | 256 |
| `Gradient` | "gradient ramp", "horizontal" | 256 |

### 4. 결과 확인

각 `generate_texture` 호출 결과 확인:
- `success: true` → 캐시 히트 또는 SD WebUI 생성+임포트 완료
- `success: false, pending: true` → SD WebUI 연결 실패, 수동 처리 필요
- `success: false, sd_error` → SD WebUI 에러, 프롬프트 수정 후 재시도

실패한 항목은 프롬프트를 단순화하여 **1회 재시도**한다.

### 5. 생성 결과 검증

`list_generated_textures` MCP 도구로 최종 확인:
- 모든 예상 텍스처가 존재하는지 확인
- 누락된 항목 보고

### 6. 출력 저장 (project_id 모드)

MCP 도구 `step_save_output` 호출:
- `step_type`: "texture_generation"
- `output_json`:

```json
{
  "generated_textures": [
    {
      "name": "fire_sprite",
      "usage": "ParticleSprite",
      "asset_path": "/Game/Generated/Textures/ParticleSprite/T_Sprite_A1B2C3D4",
      "asset_key": "T_Sprite_A1B2C3D4"
    }
  ],
  "cached_textures": [
    {
      "name": "sword_icon",
      "asset_path": "/Game/Generated/Textures/ItemIcon/T_Icon_E5F6G7H8",
      "asset_key": "T_Icon_E5F6G7H8"
    }
  ],
  "failed": []
}
```

## 수동 임포트 (SD WebUI 없이)

SD WebUI 없이 외부에서 이미지를 생성한 경우:
1. 이미지를 `Saved/TextureGenerator/{AssetKey}.png`에 저장
2. `import_texture` MCP 도구 호출:
   - `image_file_path`: 이미지 파일 경로
   - `json_intent`: Intent JSON

## 디버깅

- SD WebUI 상태 확인: `check_sd_server_status` MCP 도구 (또는 에디터 Texture 탭의 "Check Connection" 버튼)
- 캐시 확인: `Saved/TextureGenerator/` 폴더의 PNG 파일
- UE5 어셋 확인: `list_generated_textures` 호출
- 로그: MCP 서버의 `hkt_mcp.tools.texture` 로거

## 실패 처리

- 개별 텍스처 생성 실패 시 해당 항목만 건너뛰고 계속 진행
- 프롬프트 단순화 후 1회 재시도
- SD WebUI 연결 실패 시 pending 목록을 반환하고 사용자에게 안내
- 모든 텍스처가 실패하면 `step_fail` 호출
