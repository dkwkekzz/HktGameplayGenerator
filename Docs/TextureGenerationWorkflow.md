# 텍스처 생성 워크플로우

## 개요

SD WebUI(Stable Diffusion)를 활용한 텍스처 생성 시스템.
캐시 우선 확인 → 미생성분만 SD WebUI로 생성 → UE5 임포트 순서로 동작한다.

---

## 1. 사전 준비

### 1-1. SD WebUI 서버 설정

**Project Settings > HktGameplay > HktTextureGenerator** 에서 설정:

| 항목 | 설명 | 기본값 |
|---|---|---|
| `SDWebUIServerURL` | SD WebUI 주소 | `http://127.0.0.1:7860` |
| `SDWebUIBatchFilePath` | 서버 자동 시작용 bat 파일 경로 | (비어있음) |
| `DefaultResolution` | 기본 해상도 | 256 |
| `DefaultNegativePrompt` | 공통 네거티브 프롬프트 | text, watermark, frame 등 |
| `DefaultOutputDirectory` | UE5 에셋 출력 루트 | `/Game/Generated/Textures` |

### 1-2. 서버 상태 확인

에디터에서 확인하는 3가지 방법:

1. **프롬프트 패널 상태바** — 하단 "SD WebUI:" 표시 (Connected / Launching / Not connected)
2. **Texture 탭** — "Check Connection" 버튼
3. **MCP 도구** — `check_sd_server_status` 호출

서버가 꺼져 있으면:
- bat 파일이 설정되어 있으면 → `launch_sd_server` 또는 "Launch Server" 버튼으로 자동 시작
- bat 파일 미설정 시 → 수동으로 SD WebUI 실행 필요

---

## 2. 단독 텍스처 생성 (texture-gen 스텝)

### 워크플로우

```
[사용자 또는 파이프라인]
       │
       ▼
┌─────────────────────────────┐
│ Phase 0: SD 서버 점검        │
│  check_sd_server_status     │
│  → alive: false → launch    │
└──────────┬──────────────────┘
           ▼
┌─────────────────────────────┐
│ Phase 1: Intent 수집         │
│  텍스처별 JSON Intent 정의    │
│  (usage, prompt, resolution) │
└──────────┬──────────────────┘
           ▼
┌─────────────────────────────┐
│ Phase 2: 캐시 필터링         │
│  get_pending_texture_requests│
│  → 이미 생성된 텍스처 제외    │
└──────────┬──────────────────┘
           ▼
┌─────────────────────────────┐
│ Phase 3: SD WebUI 생성       │
│  pending 항목만 generate_    │
│  texture 호출                │
│  → SD txt2img → PNG 저장     │
│  → import_texture로 UE5 임포트│
└──────────┬──────────────────┘
           ▼
┌─────────────────────────────┐
│ Phase 4: 검증 & 출력 저장    │
│  list_generated_textures    │
│  step_save_output           │
└─────────────────────────────┘
```

### Intent JSON 구조

```json
{
  "usage": "ParticleSprite",
  "prompt": "fire flame sprite, bright orange, emissive",
  "negative_prompt": "",
  "resolution": 512,
  "alpha_channel": true,
  "tileable": false,
  "style_keywords": ["game vfx", "stylized"]
}
```

### Usage 종류와 용도

| Usage | 용도 | 기본 해상도 | 특성 |
|---|---|---|---|
| `ParticleSprite` | VFX 단일 파티클 스프라이트 | 256 | 알파 채널, 검은 배경 |
| `Flipbook4x4` | VFX 16프레임 SubUV 시퀀스 | 1024 | 4x4 그리드 |
| `Flipbook8x8` | VFX 64프레임 SubUV 시퀀스 | 2048 | 8x8 그리드 |
| `ItemIcon` | 아이템 UI 아이콘 | 256 | 알파 채널, 밉맵 없음 |
| `MaterialBase` | PBR 알베도 텍스처 | 1024 | sRGB, 타일링 가능 |
| `MaterialNormal` | 노멀맵 | 1024 | Linear, Normalmap 압축 |
| `MaterialMask` | 러프니스/메탈릭 마스크 | 1024 | Linear, Grayscale |
| `Noise` | 타일링 노이즈 | 256 | Linear, 시임리스 |
| `Gradient` | 그래디언트 램프 | 256 | Linear |

### 캐싱 메커니즘

- **AssetKey**: `T_{Usage}_{Hash}` — Prompt + Usage + Resolution의 조합으로 결정
- 동일한 Intent = 동일한 AssetKey = 캐시 히트 → SD WebUI 호출 스킵
- 캐시 저장 위치: `/Game/Generated/Textures/{Usage}/{AssetKey}`
- 이미지 파일 임시 위치: `Saved/TextureGenerator/{AssetKey}.png`

---

## 3. 다른 Generator에서의 텍스처 사용

### 3-1. VFX Generator (vfx-gen)

VFX 생성 중 커스텀 텍스처가 필요한 경우 Phase 1에서 처리한다.

```
[asset_discovery 출력]
  visual_design.emitter_layers[].needs_custom_texture = true
       │
       ▼
┌─────────────────────────────────┐
│ VFX Phase 1: 머티리얼 준비       │
│                                 │
│ 1. SD 서버 사전 점검             │
│    check_sd_server_status       │
│    → 실패 시 템플릿 머티리얼 폴백 │
│                                 │
│ 2. generate_texture 호출         │
│    usage: ParticleSprite /       │
│           Flipbook4x4 / Noise   │
│                                 │
│ 3. create_particle_material      │
│    생성된 텍스처 → MI 적용        │
└──────────┬──────────────────────┘
           ▼
  Phase 2: Niagara 빌드 (materialPath에 MI 설정)
           ▼
  Phase 3: 프리뷰 & 튜닝
```

**폴백**: SD 서버 불가 시 `needs_custom_texture` 레이어는 템플릿 기본 머티리얼로 대체.

### 3-2. Item Generator (item-gen)

아이콘 생성 시 C++ 레벨에서 직접 TextureGeneratorSubsystem을 호출한다.

```
[Item 생성 요청]
       │
       ▼
┌────────────────────────────────────┐
│ UHktItemGeneratorSubsystem         │
│   ::GenerateIcon(ItemIntent)       │
│                                    │
│   FHktTextureIntent TexIntent;     │
│   TexIntent.Usage = ItemIcon;      │
│   TexIntent.Prompt = BuildIconPrompt();│
│   TexIntent.Resolution = 256;      │
│   TexIntent.bAlphaChannel = true;  │
│                                    │
│   → TexSub->GenerateTexture(...)   │
└──────────┬─────────────────────────┘
           ▼
  TextureGeneratorSubsystem이
  캐시 확인 → SD 생성 → 임포트 처리
```

### 3-3. Character Generator (char-gen)

직접적인 텍스처 생성 없음. 메시와 애니메이션에 집중.

### 요약: Generator별 텍스처 사용

| Generator | 텍스처 사용 | Usage 타입 | 호출 방식 |
|---|---|---|---|
| **texture-gen** | 직접 생성 (독립 스텝) | 전체 | MCP `generate_texture` |
| **vfx-gen** | 파티클 스프라이트/노이즈 | ParticleSprite, Flipbook, Noise, Gradient | MCP `generate_texture` |
| **item-gen** | 아이템 아이콘 | ItemIcon | C++ `GenerateTexture()` 직접 호출 |
| **char-gen** | 사용 안 함 | — | — |

---

## 4. 전체 파이프라인에서의 텍스처 흐름

```
full-pipeline
  │
  ├─ concept_design
  │   └─ feature_design
  │       └─ [feature별 Worker Agent 병렬]
  │             │
  │             ├─ story_generation
  │             ├─ asset_discovery ─────────────────┐
  │             │                                   │
  │             ├─ vfx_generation ◄─ VFX 명세 ──────┤
  │             │    └─ Phase 1: generate_texture    │
  │             │       (ParticleSprite 등)          │
  │             │                                   │
  │             ├─ item_generation ◄─ Item 명세 ────┘
  │             │    └─ GenerateIcon()
  │             │       (ItemIcon)
  │             │
  │             └─ texture_generation ◄─ 추가 텍스처 명세
  │                  (MaterialBase, Noise 등 일괄)
  │
  └─ map_generation (텍스처 불필요)
```

**핵심**: 텍스처는 여러 Generator에서 필요하지만, 캐싱 덕분에 동일 Intent가 중복 생성되지 않는다.

---

## 5. 트러블슈팅

| 증상 | 원인 | 해결 |
|---|---|---|
| "Not connected" 표시 | SD WebUI 미실행 | bat 파일 설정 후 Launch Server, 또는 수동 실행 |
| launch 후에도 연결 안 됨 | bat 경로 오류 / 포트 충돌 | Project Settings에서 경로·URL 확인 |
| 생성 요청이 pending만 반환 | 캐시 미스 + SD 미연결 | SD 서버 연결 후 재시도 |
| 텍스처가 검게 나옴 | 프롬프트 품질 문제 | negative prompt 보강, alpha_channel 확인 |
| VFX에 텍스처 안 보임 | materialPath 미설정 | Phase 1 → Phase 2로 경로 전달 확인 |
| 같은 텍스처 반복 생성 | AssetKey 불일치 | 동일 prompt+usage+resolution 사용 확인 |

---

## 6. 프롬프트 작성 팁

Settings에서 Usage별 suffix가 자동 추가되므로 **핵심 내용만** 작성:

```
# 좋은 예
"fire flame, bright orange, emissive glow"

# 나쁜 예 (suffix와 중복)
"fire flame, bright orange, emissive glow, centered, black background, VFX sprite, game asset"
```

| 용도 | 프롬프트 예시 |
|---|---|
| 파티클 스프라이트 | `"smoke puff, volumetric, soft edges"` |
| Flipbook | `"explosion sequence, 4 stages, frame by frame"` |
| 아이템 아이콘 | `"iron sword, fantasy RPG style"` |
| PBR 알베도 | `"mossy stone wall, weathered"` |
| 노이즈 | `"perlin noise, organic cloud pattern"` |
