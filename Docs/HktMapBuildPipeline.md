# HktMap 빌드 파이프라인 (C++ 구현 기준)

> **대상**: C++ 개발자, 시스템 아키텍트
> **에이전트 작업 절차는 [AIAgentPipelineGuide.md](./AIAgentPipelineGuide.md) 참고**
>
> HktMap JSON이 UE5 월드에 반영되기까지의 전체 흐름.
> 에디터 빌드(BuildMap)와 런타임 스트리밍(LoadMapFromData) 양쪽 경로를 모두 다룬다.

---

## 1. 전체 흐름 개요

```
사용자 컨셉
    │
    ▼
concept_design (terrain_spec + stories)
    │
    ├───────────────────────────┐
    ▼                           ▼
map_generation              story_generation
    │                           │
    ▼                           ▼
HktMap JSON                 Story JSON
    │                           │
    ▼                           ▼
ParseMapFromJson()          McpBuildStory()
    │                           │
    ▼                           ▼
FHktMapData                 Story VM 등록
    │
    ├─[에디터]──▶ BuildMap()
    │              ├─ ALandscape::Import()      ← 절차적 지형
    │              ├─ AHktSpawnerActor           ← 엔티티 배치
    │              ├─ StaticMeshActor             ← 프롭 배치
    │              ├─ DirectionalLight/Fog/Wind   ← 환경
    │              └─ StoryRegistry               ← VM 연동
    │
    └─[런타임]──▶ LoadMapFromData()
                   ├─ GlobalContent 즉시 스폰
                   └─ OnStreamingUpdate()
                        ├─ ActivateRegion()  → 지형+엔티티+스토리 로드
                        └─ DeactivateRegion() → 전부 해제
```

---

## 2. Phase 1 — JSON 입력 → 파싱

**진입 함수**: `UHktMapGeneratorSubsystem::BuildMapFromJson(JsonStr)` 또는 MCP의 `McpBuildMap(JsonStr)`

파싱 로직은 `FHktMapJsonParser` (HktMapJsonParser.h/cpp)에 분리되어 있어,
에디터(`UHktMapGeneratorSubsystem`)와 런타임(`UHktMapStreamingSubsystem`) 양쪽에서 공용 사용한다.

```
JSON 문자열
    │
    ▼
FHktMapJsonParser::Parse()            ← HktMapJsonParser.cpp (에디터/런타임 공용)
    │  ● map_id, map_name, description
    │  ● regions[]
    │  │   └─ 각 Region → Landscape / Spawner / Story / Prop
    │  ● global_entities[]            → WorldBoss / NPC / NPCSpawner
    │  ● environment                  → 날씨 / 시간 / 포그 / 바람 / 조명
    │  ● global_stories[]             → 전역 스토리
    ▼
FHktMapData 구조체 완성
```

### 주요 데이터 구조체 (HktMapData.h)

| 구조체 | 역할 |
|---|---|
| `FHktMapData` | 최상위. Regions + GlobalEntities + Environment + GlobalStories |
| `FHktMapRegion` | 독립 스트리밍 단위. 자체 Landscape/Spawner/Story/Prop 소유 |
| `FHktMapLandscape` | Region별 지형 설정. HeightmapPath 또는 TerrainRecipe |
| `FHktTerrainRecipe` | 절차적 지형 파라미터. 노이즈 + Feature + Erosion |
| `FHktMapSpawner` | EntityTag + Position + SpawnRule |
| `FHktMapProp` | MeshTag + Transform |
| `FHktMapEnvironment` | Weather/TimeOfDay/Fog/Wind/Lighting/AmbientVFX |
| `FHktMapGlobalEntity` | WorldBoss/NPC/NPCSpawner + Properties |

---

## 3. Phase 2 — 에디터 빌드 (BuildMap)

**서브시스템**: `UHktMapGeneratorSubsystem` (EditorSubsystem)
**소스**: `HktMapGeneratorSubsystem.cpp`

### 3.1 BuildMap 전체 순서

```cpp
bool BuildMap(const FHktMapData& MapData)
{
    UnloadCurrentMap();                          // 기존 맵 정리

    for (Region : MapData.Regions)
    {
        BuildRegionLandscape(Region, World);     // ① 지형
        BuildRegionVolume(Region, World);         // ② 영역 볼륨
        BuildSpawners(Region.Spawners, World);    // ③ 스포너
        BuildProps(Region.Props, World);           // ④ 프롭
        StoryRegistry->OnRegionActivated(...);     // ⑤ 스토리
    }

    BuildGlobalEntities(GlobalEntities, World);   // ⑥ 글로벌 엔티티
    ApplyEnvironment(Environment, World);          // ⑦ 환경
    StoryRegistry->RegisterGlobalStories(...);     // ⑧ 글로벌 스토리
}
```

### 3.2 BuildRegionLandscape — ALandscape 생성

Region별 독립 ALandscape를 생성하는 핵심 함수. 7단계로 구성.

```
Region.Landscape
    │
    ├─[1] 하이트맵 소스 결정
    │      HeightmapPath 있으면 → 파일 로드 (uint16 raw)
    │      없으면 → FHktTerrainRecipeBuilder::GenerateHeightmap()
    │           ├─ FBM 노이즈 (perlin / ridged / billow)
    │           ├─ Feature 적용 (mountain / valley / ridge / plateau / crater / river_bed)
    │           └─ Hydraulic Erosion (선택적, 0~20 패스)
    │
    ├─[2] 컴포넌트 지오메트리 계산
    │      QuadsPerSection = 63, SectionsPerComponent = 1
    │      NumComponentsX = (SizeX - 1) / 63
    │      NumComponentsY = (SizeY - 1) / 63
    │
    ├─[3] 머티리얼 해석
    │      MaterialTag → UHktAssetSubsystem::ResolveConventionPath()
    │      실패 시 → Settings->DefaultLandscapeMaterial
    │
    ├─[4] WeightMap 준비
    │      FHktTerrainRecipeBuilder::GenerateWeightMaps()
    │      (높이/경사도 기반 자동 레이어 분배)
    │      파일 WeightMap이 제공되면 그것 우선 사용
    │
    ├─[5] ALandscape 스폰
    │      위치 = Region.Center - (Extent.X, Extent.Y, 0)  ← 코너 기준
    │      Z = HeightMin
    │      스케일 = (Extent*2 / (Size-1), Extent*2 / (Size-1), HeightRange/512)
    │
    ├─[6] ALandscape::Import()
    │      HeightDataPerLayer + MaterialLayerDataPerLayer
    │      → 실제 지형 컴포넌트 생성
    │
    └─[7] 머티리얼 적용 + 폴더 정리
           LandscapeMaterial 설정
           폴더: HktMap/Landscapes
```

**스케일 계산 공식**:
- `ScaleXY = Region.Extent.X * 2 / (SizeX - 1)` — 하이트맵 해상도를 Region 물리 크기에 매핑
- `ScaleZ = (HeightMax - HeightMin) / 512` — UE5 Landscape는 스케일 1에서 512 UU 높이 범위

### 3.3 BuildProps — MeshTag → StaticMesh 해석

```
MeshTag
    │
    ├─[1순위] UHktAssetSubsystem::ResolveConventionPath(MeshTag)
    │          → FSoftObjectPath → TryLoad() → UStaticMesh
    │
    ├─[2순위] 태그 기반 폴백 경로
    │          "Entity.Item.Weapon.Sword"
    │          → /Game/Generated/Props/SM_Entity_Item_Weapon_Sword
    │
    └─ 해석 성공 시 StaticMeshComponent에 적용
       실패 시 빈 StaticMeshActor (placeholder)로 배치
```

### 3.4 ApplyEnvironment — 라이팅/포그/바람

```
FHktMapEnvironment
    │
    ├─[1] DirectionalLight (태양)
    │      TimeOfDay → SunPitch / SunIntensity 매핑:
    │      ┌───────────┬──────────┬────────────┐
    │      │ TimeOfDay │ Pitch    │ Intensity  │
    │      ├───────────┼──────────┼────────────┤
    │      │ dawn      │ -10°     │ 3.0        │
    │      │ morning   │ -30°     │ 7.0        │
    │      │ noon      │ -70°     │ 10.0       │
    │      │ afternoon │ -50°     │ 8.0        │
    │      │ dusk      │ -15°     │ 3.0        │
    │      │ night     │ +10°     │ 0.1        │
    │      └───────────┴──────────┴────────────┘
    │      SunColor 적용, 없으면 새로 생성
    │
    ├─[2] ExponentialHeightFog
    │      Weather별 최소 밀도:
    │      clear=기본값, fog≥0.15, rain≥0.05, snow≥0.08, storm≥0.12
    │      AmbientColor → FogInscatteringColor
    │
    └─[3] WindDirectionalSource
           WindDirection → 액터 회전
           WindStrength → Strength + Speed(×200)
           Weather 보정: storm≥0.8, rain≥0.4
```

---

## 4. Phase 3 — 런타임 스트리밍

**서브시스템**: `UHktMapStreamingSubsystem` (WorldSubsystem)
**소스**: `HktMapStreamingSubsystem.cpp`

### 4.1 맵 로드

`LoadMap(FilePath)` → JSON 파일 읽기 → `FHktMapJsonParser::Parse()` → `LoadMapFromData()` 호출.
`LoadMapFromData(MapData)` 로 직접 호출도 가능.

```
LoadMap(FilePath) 또는 LoadMapFromData(MapData)
    ├─ 전 Region → inactive 초기화
    ├─ SpawnGlobalContent()
    │    └─ GlobalEntities → AHktSpawnerActor (즉시 Activate)
    │       EntityType별 SpawnRule:
    │         WorldBoss → OnTrigger
    │         NPC/NPCSpawner → Always
    │
    ├─ ApplyEnvironment()
    │    └─ DirectionalLight / Fog / Wind 설정 (에디터와 동일)
    │
    ├─ RegisterGlobalStories()
    │    └─ autoLoad=true → McpBuildStory() → VM 등록
    │
    └─ OnStreamingUpdate() 타이머 시작 (기본 0.5초 간격)
```

### 4.2 자동 스트리밍 Tick

```
OnStreamingUpdate()  ← 매 StreamingUpdateInterval(0.5s) 호출
    │
    ├─ PlayerLoc = GetPlayerLocation()
    │    └─ FirstPlayerController → Pawn → GetActorLocation()
    │
    └─ [각 Region에 대해]
         AABB 테스트:
           |PlayerLoc.X - Region.Center.X| <= Region.Extent.X
           |PlayerLoc.Y - Region.Center.Y| <= Region.Extent.Y
           |PlayerLoc.Z - Region.Center.Z| <= Region.Extent.Z

         ┌─ 진입 (wasInactive → isInside) ─────────────────┐
         │  ActivateRegion(Name)                             │
         │    ├─ SpawnRegionContent()                        │
         │    │    ├─ AHktMapRegionVolume                   │
         │    │    ├─ ALandscape (Import 포함)               │
         │    │    ├─ AHktSpawnerActor × N                  │
         │    │    └─ StaticMeshActor (MeshTag 해석)         │
         │    ├─ StoryRegistry->OnRegionActivated()          │
         │    └─ Spawner->Activate() → DoSpawn()             │
         └───────────────────────────────────────────────────┘

         ┌─ 이탈 (wasActive → isOutside) ──────────────────┐
         │  DeactivateRegion(Name)                           │
         │    ├─ Spawner->Deactivate()                       │
         │    │    └─ SpawnedEntities 전부 Destroy            │
         │    ├─ StoryRegistry->OnRegionDeactivated()        │
         │    │    └─ 전역 스토리가 아닌 것만 언로드           │
         │    └─ DestroyRegionContent()                      │
         │         └─ Region 전체 액터 Destroy                │
         └───────────────────────────────────────────────────┘
```

### 4.3 SpawnRegionContent — 런타임 Region 생성

에디터의 BuildMap과 동일한 결과를 런타임에서 생성. 4단계.

```
SpawnRegionContent(Region)
    ├─[1] RegionVolume 생성 (Overlap 감지용)
    ├─[2] ALandscape 생성
    │      HeightmapPath 또는 TerrainRecipe → HeightData
    │      → ALandscape::Import() (에디터 BuildRegionLandscape와 동일)
    │      → MaterialTag 해석 및 적용
    ├─[3] Spawners 배치 (AHktSpawnerActor)
    └─[4] Props 배치 (MeshTag → StaticMesh 해석)
```

---

## 5. 엔티티 스폰 — Tag 해석 체인

**함수**: `AHktSpawnerActor::DoSpawn()`
**소스**: `HktSpawnerActor.cpp`

SpawnRule에 따라 호출 시점이 결정되고, EntityTag에서 스폰할 클래스를 해석한다.

### 5.1 SpawnRule별 동작

| Rule | 트리거 | 용도 |
|---|---|---|
| `Always` | `Activate()` 즉시 | 일반 몬스터, NPC |
| `OnStoryStart` | Story에서 트리거 | 스토리 연동 엔티티 |
| `OnTrigger` | 외부 트리거 | 월드 보스, 이벤트 |
| `Timed` | 타이머 (RespawnSeconds) | 리스폰 몬스터 |

### 5.2 EntityTag → UClass 해석

```
EntityTag (FGameplayTag)
    │
    ├─[1순위] UHktAssetSubsystem::ResolveConventionPath(EntityTag)
    │          → FSoftObjectPath → TryLoad()
    │          → UBlueprint → GeneratedClass
    │          → 또는 UClass 직접
    │
    ├─[2순위] Tag 패턴 기반 폴백
    │    ┌─ "Entity.Character.{Name}"
    │    │    → FHktCharacterIntent::FromTag()
    │    │    → /Game/Generated/Characters/{Name}/BP_{Name}
    │    │
    │    └─ "Entity.Item.{Cat}.{Sub}"
    │         → FHktItemIntent::FromTag()
    │         → /Game/Generated/Items/{Cat}/SM_{Sub}
    │
    └─ 해석 성공 시:
         World->SpawnActor<AActor>(SpawnClass, Location, Rotation)
         SpawnCount > 1이면 원형 배치 (반경 200 UU)
         SpawnedEntities에 WeakObjectPtr 추적

       해석 실패 시:
         Warning 로그, 스폰 보류 (deferred)
```

---

## 6. Story 연동

**클래스**: `UHktMapStoryRegistry`
**소스**: `HktMapStoryRegistry.cpp`

### 6.1 로드 흐름

```
StoryTag
    │
    ├─ LoadedStorySet에 이미 있으면 → skip (중복 방지)
    │
    ├─ GEditor->GetEditorSubsystem<UHktStoryGeneratorSubsystem>()
    │
    └─ McpBuildStory(StoryJson) 호출
         → Story 컴파일 + VM 등록
         → 성공 시 LoadedStorySet에 추가
```

### 6.2 언로드 규칙

```
OnRegionDeactivated(RegionName)
    │
    └─ Region의 StoryTag 순회
         ├─ GlobalStoryTags에 포함? → 유지 (언로드하지 않음)
         └─ 포함 안 됨? → UnloadStory() + LoadedStorySet에서 제거
```

### 6.3 생명주기

| 이벤트 | 동작 |
|---|---|
| 맵 로드 | `RegisterGlobalStories()` → 전역 스토리 즉시 로드 |
| Region 진입 | `OnRegionActivated()` → Region 스토리 로드 |
| Region 이탈 | `OnRegionDeactivated()` → 전역이 아닌 스토리 언로드 |
| 맵 언로드 | `Clear()` → 전체 언로드 + LoadedStorySet 초기화 |

---

## 7. 핵심 모듈 의존 관계

```
HktMapGenerator
    ├─ HktGeneratorCore (Public)
    │    ├─ UHktAssetSubsystem     → Tag → AssetPath 해석
    │    ├─ FHktCharacterIntent    → Entity.Character 태그 파싱
    │    ├─ FHktItemIntent         → Entity.Item 태그 파싱
    │    └─ UHktGeneratorRouter    → PIE Tag miss 라우팅
    │
    ├─ HktStoryGenerator (Private)
    │    └─ UHktStoryGeneratorSubsystem → Story VM 등록/실행
    │
    ├─ Landscape / LandscapeEditor (Engine)
    │    └─ ALandscape::Import()    → 하이트맵 기반 지형 생성
    │
    └─ 내부 클래스
         ├─ FHktMapJsonParser        → JSON ↔ FHktMapData 변환 (에디터/런타임 공용)
         ├─ FHktTerrainRecipeBuilder → 절차적 하이트맵/웨이트맵 생성
         ├─ AHktSpawnerActor         → EntityTag 기반 엔티티 스폰
         ├─ AHktMapRegionVolume      → Region 영역 감지
         └─ UHktMapStoryRegistry     → Story 로드/언로드 관리
```

---

## 8. Tag 해석 우선순위 요약

모든 Tag 해석은 동일한 2단계 패턴을 따른다:

| 단계 | 방법 | 설명 |
|---|---|---|
| 1순위 | `UHktAssetSubsystem::ResolveConventionPath(Tag)` | Project Settings 기반 규칙 |
| 2순위 | Tag 패턴 → 기본 경로 | 태그 구조에서 경로 생성 |

**Tag 패턴별 기본 경로**:

| Tag 패턴 | 기본 경로 |
|---|---|
| `Entity.Character.{Name}` | `/Game/Generated/Characters/{Name}/BP_{Name}` |
| `Entity.Item.{Cat}.{Sub}` | `/Game/Generated/Items/{Cat}/SM_{Sub}` |
| `VFX.{Event}.{Element}` | `/Game/Generated/VFX/VFX_{Event}_{Element}` |
| Prop MeshTag (범용) | `/Game/Generated/Props/SM_{tag_underscored}` |

---

## 9. 에디터 Outliner 폴더 구조

BuildMap 완료 후 에디터의 World Outliner에 다음과 같은 폴더 구조가 생성된다:

```
HktMap/
  ├─ Landscapes/
  │    ├─ HktLandscape_DarkForest
  │    └─ HktLandscape_Village
  ├─ Regions/
  │    ├─ HktRegion_DarkForest
  │    └─ HktRegion_Village
  ├─ Spawners/
  │    ├─ HktSpawner (Entity.Character.Goblin)
  │    └─ HktSpawner (Entity.Character.Guard)
  ├─ Props/
  │    ├─ StaticMeshActor (rock_01)
  │    └─ StaticMeshActor (tree_02)
  ├─ GlobalEntities/
  │    └─ HktSpawner (Entity.Character.DragonBoss)
  └─ Environment/
       ├─ DirectionalLight
       ├─ ExponentialHeightFog
       └─ WindDirectionalSource
```

---

## 10. MCP 엔드포인트

| 함수 | 역할 | 반환 |
|---|---|---|
| `McpBuildMap(json)` | JSON → 파싱 + 빌드 | `{success, map_id, region_count}` |
| `McpValidateMap(json)` | JSON 문법/구조 검증 | `{valid, error_count, ...counts}` |
| `McpGetMapSchema()` | HktMap JSON 스키마 | 스키마 JSON |

### 일반적인 MCP 워크플로우

```
1. McpGetMapSchema()        → 스키마 확인
2. AI Agent가 HktMap JSON 작성
3. McpValidateMap(json)     → 유효성 검사
4. McpBuildMap(json)        → UE5 빌드
```
