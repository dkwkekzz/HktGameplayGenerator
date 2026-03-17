# GameStory → Visual 자동화 파이프라인 (C++ 아키텍처)

> **대상**: C++ 개발자, 시스템 아키텍트
> **에이전트 작업 절차는 [AIAgentPipelineGuide.md](./AIAgentPipelineGuide.md) 참고**
>
> Story에서 정의한 Tag에 대해 데이터 작업 없이 AI Agent 중심으로 Visual 요소와 연결하는 시스템.

## 1. 문제와 목표

### 현재 (수동)
```
Story에서 Tag 정의 → [수동] DataAsset 생성 → [수동] Asset 연결 → Presentation 렌더링
```

### 목표 (자동)
```
Story에서 Tag 정의 → [자동] Convention/Generator가 해결 → Presentation 렌더링
```

**핵심 원칙**: Story의 Tag 정의 자체가 Generation Intent가 되어, 수동 DataAsset 작업 없이 AI Agent가 Visual 에셋을 자동 생성/연결.

---

## 2. 아키텍처 개요

```
                    Story Tag 정의
                         │
                    ┌────┴────┐
                    │ Tag Miss │ (AssetSubsystem에 없는 Tag)
                    └────┬────┘
                         │
              ┌──────────┴──────────┐
              │ UHktAssetSubsystem   │  해결 순서:
              │ 1. TagToPathMap      │  ← DataAsset 기반 (기존)
              │ 2. Convention Path   │  ← 경로 규칙 (신규)
              │ 3. OnTagMiss 콜백    │  ← Generator 자동 생성 (신규)
              └──────────┬──────────┘
                         │
              ┌──────────┴──────────┐
              │ UHktGeneratorRouter  │  Tag prefix로 라우팅
              │                      │
              │ "VFX.*"    → VFX     │
              │ "Entity.*" → Char    │
              │ "Anim.*"   → Anim    │
              │ "Entity.Item.*"→Item │
              └──────────┬──────────┘
                         │
         ┌───────┬───────┼───────┬────────┐
         ▼       ▼       ▼       ▼        ▼
     VFXGen  MeshGen  AnimGen  ItemGen  TextureGen
         │       │       │       │        │
         ▼       ▼       ▼       ▼        ▼
   Niagara  SkMesh   AnimSeq  SMesh   UTexture2D
         │       │       │    +Icon      │
         └───────┴───────┴───────┘       │
                    │                    │
         Convention Path에 저장 ◄────────┘
         AssetSubsystem TagMap 갱신
         → Presentation 자동 연결 완료

                    ▲
                    │ (의존 분석)
         ┌──────────┴──────────┐
         │ HktStoryGenerator   │  JSON → FHktStoryBuilder
         │                      │  → Bytecode 컴파일 + 등록
         │ McpAnalyzeDeps()    │  → 누락 Tag → Generator별 분류
         └─────────────────────┘
```

---

## 3. 모듈 구조

### HktGameplayGenerator Plugin

```
HktGameplayGenerator/
├── Source/
│   ├── HktTextureGenerator/    ← 공유 텍스처 생성 (완료)
│   │   ├── FHktTextureIntent          (Usage + Prompt)
│   │   ├── UHktTextureGeneratorSubsystem  (Generate, Import, Cache)
│   │   ├── UHktTextureGeneratorSettings   (프롬프트 접미사, 기본값)
│   │   └── UHktTextureGeneratorFunctionLibrary (MCP API)
│   │
│   ├── HktGeneratorCore/       ← 라우터 + Intent 타입 + VFX AutoResolver
│   │   ├── UHktGeneratorRouter        (Tag miss → Generator 라우팅)
│   │   ├── IHktGeneratorHandler       (Generator 인터페이스)
│   │   ├── FHktVFXAutoResolver        (VFX Tag → FHktVFXIntent 파싱)
│   │   ├── FHktAnimIntent             (Anim Tag → Layer/Type/Name 파싱)
│   │   ├── FHktCharacterIntent        (Entity Tag → Name/Skeleton 파싱)
│   │   └── FHktItemIntent             (Entity.Item Tag → Category/SubType 파싱)
│   │
│   ├── HktVFXGenerator/        ← VFX 생성 (완료)
│   │   ├── FHktVFXNiagaraBuilder      (Config → Niagara System)
│   │   ├── UHktVFXGeneratorSubsystem  (빌드 API)
│   │   └── UHktVFXGeneratorHandler    (IHktGeneratorHandler — "VFX.*")
│   │
│   ├── HktMeshGenerator/       ← 캐릭터/엔티티 메시 생성 (완료)
│   │   ├── UHktMeshGeneratorSubsystem (MCP 메시 생성 API)
│   │   ├── UHktMeshGeneratorHandler   (IHktGeneratorHandler — "Entity.*")
│   │   ├── UHktMeshGeneratorSettings  (스켈레톤 풀, 출력 경로)
│   │   └── UHktMeshGeneratorFunctionLibrary (MCP API)
│   │
│   ├── HktAnimGenerator/       ← 애니메이션 생성 (완료)
│   │   ├── UHktAnimGeneratorSubsystem (MCP 애니메이션 생성 API)
│   │   ├── UHktAnimGeneratorHandler   (IHktGeneratorHandler — "Anim.*")
│   │   ├── UHktAnimGeneratorSettings  (타겟 스켈레톤, BlendSpace 옵션)
│   │   └── UHktAnimGeneratorFunctionLibrary (MCP API)
│   │
│   ├── HktItemGenerator/       ← 아이템 생성 (완료)
│   │   ├── UHktItemGeneratorSubsystem (MCP 아이템 생성 API)
│   │   ├── UHktItemGeneratorHandler   (IHktGeneratorHandler — "Entity.Item.*")
│   │   ├── UHktItemGeneratorSettings  (소켓 매핑, 머티리얼 맵)
│   │   └── UHktItemGeneratorFunctionLibrary (MCP API)
│   │
│   ├── HktStoryGenerator/      ← Story JSON 컴파일 (완료)
│   │   ├── FHktStoryJsonCompiler        (JSON → FHktStoryBuilder 컴파일)
│   │   ├── UHktStoryGeneratorSubsystem  (MCP Story 빌드/검증/분석 API)
│   │   └── UHktStoryGeneratorFunctionLibrary (MCP API)
│   │
│   └── HktMapGenerator/         ← HktMap JSON 기반 맵 생성 (신규)
│       ├── FHktMapData                  (맵 데이터 구조체 — Landscape/Spawner/Region/Story)
│       ├── UHktMapGeneratorSubsystem    (JSON 파싱, 맵 빌드, MCP API)
│       └── UHktMapGeneratorSettings     (출력 경로, 기본 Landscape 설정)
│
└── Docs/
    └── GameStoryVisualPipeline.md  ← 이 문서
```

### 모듈 의존 그래프

```
HktTextureGenerator (독립, 최하위)
        ↑
HktGeneratorCore (HktTextureGenerator, HktAsset, HktVFX)
        ↑
├── HktVFXGenerator    (HktGeneratorCore, HktTextureGenerator, HktVFX)
├── HktMeshGenerator   (HktGeneratorCore, HktTextureGenerator)
├── HktAnimGenerator   (HktGeneratorCore)
├── HktItemGenerator   (HktGeneratorCore, HktTextureGenerator)
├── HktStoryGenerator  (HktCore, HktAsset, HktGeneratorCore)
└── HktMapGenerator    (HktGeneratorCore, Landscape)  ← 신규: HktMap JSON → 월드 빌드
```

### Handler 자동 등록

각 Generator Subsystem이 `Initialize()` 에서 자신의 Handler를 `UHktGeneratorRouter`에 등록:

```cpp
// 예: VFXGeneratorSubsystem::Initialize()
if (UHktGeneratorRouter* Router = GEditor->GetEditorSubsystem<UHktGeneratorRouter>())
{
    VFXHandler = NewObject<UHktVFXGeneratorHandler>(this);
    Router->RegisterHandler(TScriptInterface<IHktGeneratorHandler>(VFXHandler));
}
```

---

## 4. Convention Path Resolution

DataAsset 없이 Tag만으로 에셋을 찾는 경로 규칙.
**모든 경로 설정은 Project Settings > HktGameplay > HktAsset 에서 변경 가능.**

### UHktAssetSettings (DeveloperSettings)

```
Project Settings > HktGameplay > HktAsset
├── ConventionRootDirectory: "/Game/Generated"   ← 루트 경로 ({Root})
└── ConventionRules: [                            ← 규칙 목록 (순서대로 매칭)
      { TagPrefix: "Entity.Character.", PathPattern: "{Root}/Characters/{Leaf}/BP_{Leaf}" },
      { TagPrefix: "Entity.",           PathPattern: "{Root}/Entities/{Leaf}/BP_{Leaf}" },
      { TagPrefix: "VFX.",              PathPattern: "{Root}/VFX/{TagPath}" },
      { TagPrefix: "Entity.Item.",       PathPattern: "{Root}/Items/{Category}/SM_{Leaf}" },
      { TagPrefix: "Anim.",             PathPattern: "{Root}/Animations/{TagPath}" },
    ]
```

### PathPattern 치환 변수

| 변수 | 설명 | 예시 (Tag: `Entity.Character.Goblin`) |
|------|------|-------|
| `{Root}` | ConventionRootDirectory | `/Game/Generated` |
| `{Leaf}` | 태그의 마지막 세그먼트 | `Goblin` |
| `{Category}` | 태그의 두 번째 세그먼트 | `Character` |
| `{TagPath}` | 태그를 `_`로 연결 | `Entity_Character_Goblin` |

### 기본 규칙 테이블

| Tag 패턴 | PathPattern | 결과 예시 |
|-----------|-------------|-----------|
| `Entity.Character.{Name}` | `{Root}/Characters/{Leaf}/BP_{Leaf}` | `/Game/Generated/Characters/Goblin/BP_Goblin` |
| `Entity.{Type}.{Name}` | `{Root}/Entities/{Leaf}/BP_{Leaf}` | `/Game/Generated/Entities/Tower/BP_Tower` |
| `VFX.{...}` | `{Root}/VFX/{TagPath}` | `/Game/Generated/VFX/VFX_Explosion_Fire` |
| `Entity.Item.{Cat}.{Sub}` | `{Root}/Items/{Category}/SM_{Leaf}` | `/Game/Generated/Items/Weapon/SM_Sword` |
| `Anim.{...}` | `{Root}/Animations/{TagPath}` | `/Game/Generated/Animations/Anim_FullBody_Run` |

### 해결 순서 (UHktAssetSubsystem::ResolvePath)

```
1. TagToPathMap 조회       → DataAsset이 있으면 즉시 반환
2. Convention Path 조회    → Settings 규칙으로 경로 생성 → 에셋 존재 시 반환 + 캐시
3. OnTagMiss 콜백          → Generator가 생성 후 경로 반환
```

### 코드 위치
- `HktAsset/Public/HktAssetSettings.h` — `UHktAssetSettings`, `FHktConventionRule`
- `HktAsset/Private/HktAssetSettings.cpp` — 기본 규칙 + `ResolveConventionPath()` 패턴 치환
- `HktAsset/Public/HktAssetSubsystem.h` — `ResolvePath()`, `ResolveConventionPath()`, `OnTagMiss`
- `HktAsset/Private/HktAssetSubsystem.cpp` — Settings 기반 구현

---

## 5. 각 Visual 요소별 파이프라인

### 5.1 VFX: Tag → NiagaraSystem

**흐름:**
```
Story: .PlayVFX(Self, "VFX.Explosion.Fire")
    ↓
FHktVFXAutoResolver::ParseTagToIntent("VFX.Explosion.Fire")
    → EventType=Explosion, Element=Fire, Intensity=0.5
    ↓
HktVFXGenerator.BuildNiagaraFromConfig(Intent → Config)
    ↓
NiagaraSystem 생성 → /Game/Generated/VFX/VFX_Explosion_Fire
    ↓
텍스처 필요 시 → HktTextureGenerator.GenerateBatch()
    ↓
AssetSubsystem TagMap 갱신
```

**핵심 파일:**
- `HktGeneratorCore/Public/HktVFXAutoResolver.h` — Tag → Intent 파싱
- `HktVFXGenerator/` — Config → Niagara System 빌드
- `HktTextureGenerator/` — 텍스처 생성/임포트

**Tag 규칙:**
```
VFX.{EventType}.{Element}            → 기본 (Intensity=0.5)
VFX.{EventType}.{Element}.{Surface}  → 표면 지정
VFX.Custom.{Description}             → 커스텀 자연어
```

### 5.2 Character: Tag → Actor Blueprint

**흐름:**
```
Story: .SpawnEntity("Entity.Character.Goblin")
    ↓
FHktActorRenderer::SpawnActor()
    ↓
Path 1: UHktAssetSubsystem → DataAsset → ActorClass (기존)
Path 2: Convention Path → /Game/Generated/Characters/Goblin/BP_Goblin (신규)
Path 3: OnTagMiss → MeshGenerator → Blueprint 자동 생성 (미래)
    ↓
Actor 스폰 + CapsuleComponent 오프셋
```

**핵심 파일:**
- `HktPresentation/Private/Renderers/HktActorRenderer.cpp` — Convention fallback 추가됨
- `HktGeneratorCore/Public/HktAnimGeneratorTypes.h` — FHktCharacterIntent

**Skeleton Pool 전략:**
```
모든 캐릭터가 소수의 Base Skeleton 공유:
├── SK_Humanoid_Base   (인간형)
├── SK_Quadruped_Base  (사족보행)
└── SK_Custom_{N}      (특수)

CharacterIntent.SkeletonType → Skeleton 자동 선택
IKRig Retarget으로 체형 차이 보정
```

### 5.3 Mesh: CharacterIntent → SkeletalMesh

**단계별 접근:**

| Phase | 방식 | 설명 |
|-------|------|------|
| Phase 1 (현재) | Template Pool | 미리 만들어둔 base mesh + material swap |
| Phase 2 | AI 3D API | Meshy/Rodin 등 외부 API → FBX → UE5 Import |
| Phase 3 | 실시간 | 프로시저럴 메시 생성 |

**Convention Path:** `/Game/Generated/Characters/{Name}/SK_{Name}`

### 5.4 Animation: Tag → AnimSequence/Montage

**흐름:**
```
Story: .AddTag(Self, "Anim.FullBody.Action.Spawn")
    ↓
FHktAnimIntent::FromTag()
    → Layer=FullBody, Type=Action, Name=Spawn
    ↓
UHktAnimInstance::SyncFromTagContainer()
    → 태그 변화 감지 → ApplyAnimTag()
    ↓
FindMapping(AnimTag)
    → AnimMappings에서 매칭 에셋 찾아 재생
```

**동적 매핑 등록 (Generator 연동):**
```cpp
// Generator가 AnimSequence 생성 후 AnimInstance에 등록
AnimInstance->RegisterAnimMapping(
    FGameplayTag::RequestGameplayTag("Anim.FullBody.Action.Spawn"),
    nullptr,           // Montage
    GeneratedSequence, // Sequence
    nullptr            // BlendSpace
);
// AnimBP 리컴파일 불필요!
```

**핵심 파일:**
- `HktPresentation/Public/HktAnimInstance.h` — RegisterAnimMapping() 추가됨
- `HktPresentation/Private/HktAnimInstance.cpp` — 동적 등록 구현
- `HktGeneratorCore/Public/HktAnimGeneratorTypes.h` — FHktAnimIntent

**Tag 구조:**
```
Anim.FullBody.Locomotion.Run    → Layer=FullBody,  BlendSpace
Anim.FullBody.Action.Spawn      → Layer=FullBody,  Montage/Sequence
Anim.UpperBody.Combat.Attack    → Layer=UpperBody, Montage (DefaultSlot)
Anim.Montage.Intro              → Layer=Montage,   One-shot Montage
```

### 5.5 Item: Tag → StaticMesh + Icon

**흐름:**
```
SpawnEntity("Entity.Item.Weapon.Sword.Fire")
+ SaveEntityProperty(Spawned, "Owner", Self)
+ SaveEntityProperty(Spawned, "Slot", 0)
    ↓
FHktItemIntent::FromTag()
    → Category=Weapon, SubType=Sword, Element=Fire
    ↓
ItemGenerator:
├── StaticMesh: HktMeshGenerator 또는 외부 API
├── Icon: HktTextureGenerator (Usage=ItemIcon)
├── Material: Element 기반 자동 MI 생성
└── Convention Path: /Game/Generated/Items/Weapon/SM_Sword
```

**장착 시각화:**
```
Weapon     → Socket Attach (hand_r)
Armor      → Material Overlay 또는 Modular Mesh
Accessory  → Socket Attach (소형) 또는 VFX (버프)
```

**Convention Path:** `/Game/Generated/Items/{Category}/SM_{SubType}`

---

## 6. HktTextureGenerator (공유 모듈)

모든 Generator가 텍스처 생성 시 이 모듈을 통해 수행. 중복 제거.

### 지원 Usage

| Usage | 용도 | Compression | SRGB | LODGroup |
|-------|------|-------------|------|----------|
| ParticleSprite | VFX 파티클 | EditorIcon | true | Effects |
| Flipbook4x4/8x8 | SubUV 시퀀스 | EditorIcon | true | Effects |
| Noise | 노이즈 텍스처 | Grayscale | false | Effects |
| Gradient | 그라디언트 | Grayscale | false | Effects |
| ItemIcon | 아이템 아이콘 | EditorIcon | true | UI |
| MaterialBase | Albedo | Default | true | World |
| MaterialNormal | Normal Map | Normalmap | false | WorldNormalMap |
| MaterialMask | R/M/AO Packed | Masks | false | World |

**MCP 도구 사용법**: [AIAgentPipelineGuide.md - Section 8](./AIAgentPipelineGuide.md#8-texture-generator--공유-모듈) 참고

### Convention Path
```
/Game/Generated/Textures/{UsageSubDir}/{AssetKey}
    VFX/           ← ParticleSprite, Flipbook
    Icons/         ← ItemIcon
    Materials/     ← MaterialBase, Normal, Mask
    Noise/         ← Noise
    Gradient/      ← Gradient
```

---

## 7. GeneratorRouter 시스템

### 바인딩 구조

```
UHktGeneratorRouter (EditorSubsystem)
    │
    ├── AssetSubsystem.OnTagMiss에 바인딩
    │
    └── Handlers[] (IHktGeneratorHandler 구현체)
        ├── VFXGeneratorHandler     — "VFX.*" 처리
        ├── MeshGeneratorHandler    — "Entity.*" 처리
        ├── AnimGeneratorHandler    — "Anim.*" 처리
        ├── ItemGeneratorHandler    — "Entity.Item.*" 처리
        │
        (StoryGenerator는 Handler 아님 — Tag 생산자 역할)
        └── StoryGeneratorSubsystem — JSON→Story 컴파일 + 의존 분석
```

### IHktGeneratorHandler 인터페이스

```cpp
class IHktGeneratorHandler
{
    // 이 Generator가 해당 Tag를 처리할 수 있는지
    virtual bool CanHandle(const FGameplayTag& Tag) const = 0;

    // Tag에 대한 에셋 생성 후 경로 반환
    virtual FSoftObjectPath HandleTagMiss(const FGameplayTag& Tag) = 0;
};
```

---

## 8. 파일 변경 목록

### 신규 생성

| 모듈 | 파일 | 역할 |
|------|------|------|
| **HktTextureGenerator** | `Build.cs` | 모듈 빌드 |
| | `Public/IHktTextureGeneratorModule.h` | 모듈 인터페이스 |
| | `Public/HktTextureIntent.h` | Intent, Request, Result 타입 |
| | `Public/HktTextureGeneratorSubsystem.h` | EditorSubsystem |
| | `Public/HktTextureGeneratorSettings.h` | DeveloperSettings |
| | `Public/HktTextureGeneratorFunctionLibrary.h` | MCP API |
| | `Private/*.cpp` | 구현 (5개) |
| **HktGeneratorCore** | `Build.cs` | 모듈 빌드 |
| | `Public/IHktGeneratorCoreModule.h` | 모듈 인터페이스 |
| | `Public/HktGeneratorRouter.h` | Tag miss 라우터 + IHktGeneratorHandler 인터페이스 |
| | `Public/HktVFXAutoResolver.h` | VFX Tag → Intent 파서 |
| | `Public/HktAnimGeneratorTypes.h` | Anim/Character/Item Intent |
| | `Private/*.cpp` | 구현 (4개) |
| **HktVFXGenerator** | `Public/HktVFXGeneratorHandler.h` | IHktGeneratorHandler — "VFX.*" |
| | `Private/HktVFXGeneratorHandler.cpp` | Intent→Config 변환 + Niagara 빌드 |
| **HktMeshGenerator** | `Build.cs` | 모듈 빌드 |
| | `Public/IHktMeshGeneratorModule.h` | 모듈 인터페이스 |
| | `Public/HktMeshGeneratorSubsystem.h` | EditorSubsystem + FHktMeshGenerationRequest |
| | `Public/HktMeshGeneratorHandler.h` | IHktGeneratorHandler — "Entity.*" |
| | `Public/HktMeshGeneratorSettings.h` | DeveloperSettings (스켈레톤 풀) |
| | `Public/HktMeshGeneratorFunctionLibrary.h` | MCP API |
| | `Private/*.cpp` | 구현 (5개) |
| **HktAnimGenerator** | `Build.cs` | 모듈 빌드 |
| | `Public/IHktAnimGeneratorModule.h` | 모듈 인터페이스 |
| | `Public/HktAnimGeneratorSubsystem.h` | EditorSubsystem + FHktAnimGenerationRequest |
| | `Public/HktAnimGeneratorHandler.h` | IHktGeneratorHandler — "Anim.*" |
| | `Public/HktAnimGeneratorSettings.h` | DeveloperSettings (BlendSpace 옵션) |
| | `Public/HktAnimGeneratorFunctionLibrary.h` | MCP API |
| | `Private/*.cpp` | 구현 (5개) |
| **HktItemGenerator** | `Build.cs` | 모듈 빌드 |
| | `Public/IHktItemGeneratorModule.h` | 모듈 인터페이스 |
| | `Public/HktItemGeneratorSubsystem.h` | EditorSubsystem + FHktItemGenerationRequest |
| | `Public/HktItemGeneratorHandler.h` | IHktGeneratorHandler — "Entity.Item.*" |
| | `Public/HktItemGeneratorSettings.h` | DeveloperSettings (소켓 매핑) |
| | `Public/HktItemGeneratorFunctionLibrary.h` | MCP API |
| | `Private/*.cpp` | 구현 (5개) |
| **HktStoryGenerator** | `Build.cs` | 모듈 빌드 |
| | `Public/HktStoryJsonCompiler.h` | JSON → FHktStoryBuilder 컴파일러 |
| | `Public/HktStoryGeneratorSubsystem.h` | EditorSubsystem (MCP API) |
| | `Public/HktStoryGeneratorFunctionLibrary.h` | MCP API |
| | `Private/*.cpp` | 구현 (4개) |

### 기존 파일 변경

| 파일 | 변경 내용 |
|------|-----------|
| `HktGameplayGenerator.uplugin` | 7개 모듈 등록 (TextureGen, Core, VFX, Mesh, Anim, Item, Story) |
| `HktVFXGenerator.Build.cs` | HktTextureGenerator, HktGeneratorCore 의존성 추가 |
| `HktVFXGeneratorSubsystem.cpp` | Handler 자동 등록 + 텍스처 위임 |
| `HktAssetSubsystem.h` | Convention Path, OnTagMiss, ResolvePath 추가 |
| `HktAssetSubsystem.cpp` | ResolvePath, ResolveConventionPath, RegisterTagPath 구현 |
| `HktAssetSettings.h/cpp` | UHktAssetSettings (Convention 규칙 DeveloperSettings) |
| `HktActorRenderer.cpp` | SpawnActor에 Convention Path fallback 추가 |
| `HktAnimInstance.h/cpp` | RegisterAnimMapping, 동적 매핑 등록 |

---

## 9. Skeleton & AnimInstance 공정

### Skeleton Pool 전략

```
소수의 Base Skeleton을 준비하고 모든 캐릭터가 공유:

SK_Humanoid_Base
├── 60+ bones (UE5 Mannequin 호환)
├── IKRig: IKR_Humanoid_Base
├── 기본 소켓: hand_r, hand_l, head, spine_03
└── 모든 인간형 캐릭터가 이 Skeleton 사용

SK_Quadruped_Base
├── 사족보행 표준 뼈대
├── IKRig: IKR_Quadruped_Base
└── 동물/마운트 계열

SK_Custom_{N}
└── 특수 형태 (슬라임, 드래곤 등)
```

### Retarget 워크플로우

```
1. 새 캐릭터 메시 생성 (외부 API or Template)
2. Base Skeleton으로 리깅 (동일 Skeleton 공유)
3. 이미 존재하는 모든 AnimSequence/Montage가 즉시 사용 가능
4. 체형 차이는 IKRig Retarget으로 자동 보정
```

### AnimInstance 연동

```
AnimBP 수정 불필요:
1. AnimMappings 테이블이 태그 기반 동적 매핑
2. RegisterAnimMapping()으로 런타임 등록 가능
3. SyncFromTagContainer()가 태그 변화 자동 감지
4. Story에서 AddTag/RemoveTag만 하면 애니메이션 자동 재생/중지
```

---

## 10. AI Agent 통합

에이전트 MCP 작업 절차 및 시나리오 예제는 [AIAgentPipelineGuide.md](./AIAgentPipelineGuide.md) 참고.

### 런타임 해결 흐름 (VFX 예시)

```
1. Story: .PlayVFX(Self, "VFX.Explosion.Fire")

2. 첫 실행 시 (Convention miss):
   → FHktVFXAutoResolver → EventType=Explosion, Element=Fire
   → HktVFXGenerator → NiagaraSystem 생성
   → 텍스처 필요 시 HktTextureGenerator 자동 위임
   → /Game/Generated/VFX/VFX_Explosion_Fire 저장

3. 이후 실행:
   → Convention Path hit → 즉시 재생
```

---

## 11. 구현 우선순위

| 순서 | 항목 | 상태 | 설명 |
|------|------|------|------|
| 1 | HktTextureGenerator | ✅ 완료 | 공유 텍스처 생성 모듈 |
| 2 | Convention Path Resolution | ✅ 완료 | AssetSubsystem에 Convention + OnMiss |
| 3 | GeneratorRouter + Intent 타입 | ✅ 완료 | Tag miss 라우팅 + VFX/Anim/Char/Item Intent |
| 4 | VFX AutoResolver | ✅ 완료 | Tag → Intent 자동 파싱 |
| 5 | ActorRenderer Convention | ✅ 완료 | DataAsset 없이 Blueprint 직접 로드 |
| 6 | AnimInstance 동적 매핑 | ✅ 완료 | RegisterAnimMapping() API |
| 7 | VFX Generator Handler | ✅ 완료 | Intent→Config→Niagara 자동 빌드 |
| 8 | Mesh Generator Handler | ✅ 완료 | MCP API + 펜딩 요청 + 스켈레톤 풀 |
| 9 | Anim Generator Handler | ✅ 완료 | MCP API + 에셋 타입 추론 (Seq/Montage/BS) |
| 10 | Item Generator Handler | ✅ 완료 | MCP API + 아이콘 TextureGen 위임 + 소켓 매핑 |
| 11 | Story Generator | ✅ 완료 | JSON→Bytecode 컴파일 + 의존 분석 + Generator 연계 |

---

## 12. Story Generator: JSON → Bytecode 파이프라인

### 개요

`FHktStoryJsonCompiler`가 JSON → `FHktStoryBuilder` 메서드 호출로 변환 → 바이트코드 컴파일 → `FHktStoryRegistry` 등록.

**MCP 도구 사용법 및 Op 목록**: [AIAgentPipelineGuide.md - Section 3](./AIAgentPipelineGuide.md#3-step-by-step-작업-절차-스토리-생성--빌드) 참고

### 컴파일 흐름 (C++)

```
FHktStoryJsonCompiler::Compile(JsonStr)
    ├── JSON 파싱 → storyTag, tags{}, steps[]
    ├── tags{} → FGameplayTag alias 등록 (UGameplayTagsManager::AddNativeGameplayTag)
    ├── steps[] → FHktStoryBuilder 메서드 호출 (45+ ops)
    ├── FHktStoryBuilder → 바이트코드 생성
    └── FHktStoryRegistry::Register(storyTag, bytecode)
```

### 의존 분석 (C++)

```
FHktStoryJsonCompiler::AnalyzeDependencies(JsonStr)
    → steps에서 참조되는 모든 Tag 추출
    → prefix별 분류:
       VFX.*       → HktVFXGenerator
       Entity.*    → HktMeshGenerator
       Anim.*      → HktAnimGenerator
       Entity.Item.* → HktItemGenerator
```

### 핵심 파일

- `HktStoryGenerator/Public/HktStoryJsonCompiler.h` — 컴파일러 + FHktStoryCompileResult
- `HktStoryGenerator/Private/HktStoryJsonCompiler.cpp` — op→Builder 디스패치, 스키마/예제 생성
- `HktStoryGenerator/Public/HktStoryGeneratorSubsystem.h` — MCP API (6개 엔드포인트)
- `HktStoryGenerator/Public/HktStoryGeneratorFunctionLibrary.h` — 정적 MCP 진입점

---

## 13. Convention Path 디렉토리 구조

```
/Game/Generated/
├── Characters/
│   ├── Goblin/
│   │   ├── BP_Goblin              (Actor Blueprint)
│   │   ├── SK_Goblin              (SkeletalMesh)
│   │   └── Materials/
│   │       ├── MI_Goblin_Base     (MaterialInstance)
│   │       └── (textures via HktTextureGenerator)
│   └── Knight/
│       └── ...
├── VFX/
│   ├── VFX_Explosion_Fire         (NiagaraSystem)
│   ├── VFX_Heal_Holy              (NiagaraSystem)
│   └── ...
├── Items/
│   ├── Weapon/
│   │   ├── SM_Sword               (StaticMesh)
│   │   └── SM_Bow                 (StaticMesh)
│   └── Armor/
│       └── SM_ChestPlate          (StaticMesh)
├── Textures/
│   ├── VFX/                       (ParticleSprite, Flipbook)
│   ├── Icons/                     (ItemIcon)
│   ├── Materials/                 (Base, Normal, Mask)
│   ├── Noise/                     (Noise)
│   └── Gradient/                  (Gradient)
└── Animations/
    ├── Humanoid/
    │   ├── AM_Run                 (AnimMontage)
    │   ├── AS_Idle                (AnimSequence)
    │   └── BS_Locomotion          (BlendSpace)
    └── Quadruped/
        └── ...
```
