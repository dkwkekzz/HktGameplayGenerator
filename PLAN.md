# HktMapGenerator 구현 계획

## 개요

LLM 기반 UE5 맵 자동 생성 시스템. LLM이 컨셉 텍스트에서 "Terrain Recipe" JSON을 생성하면, C++에서 이를 해석하여 Landscape + Spawner + Story + 환경을 구축한다. Region 단위 스트리밍으로 부분 로드/언로드를 지원한다.

---

## 1단계: HktMap JSON 스키마 확장

### 변경 대상
- `Source/HktMapGenerator/Public/HktMapData.h`
- `McpServer/src/hkt_mcp/tools/map_tools.py` (HKTMAP_SCHEMA)

### 추가할 필드

#### 1-1. Terrain Recipe (landscape 확장)

`FHktMapLandscape`에 heightmap 직접 경로 대신 **절차적 생성 파라미터**를 추가한다.

```cpp
// HktMapData.h - FHktMapLandscape에 추가

USTRUCT(BlueprintType)
struct FHktTerrainFeature
{
    GENERATED_BODY()

    /** Feature type: mountain, ridge, valley, plateau, crater, river_bed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Type;

    /** Normalized position [0,1] range on the landscape */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector2D Position = FVector2D(0.5f, 0.5f);

    /** Radius in normalized [0,1] range */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Radius = 0.1f;

    /** Height influence [-1, 1] range. 1=max elevation, -1=max depression */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Intensity = 0.5f;

    /** Falloff curve: linear, smooth, sharp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Falloff = TEXT("smooth");
};

USTRUCT(BlueprintType)
struct FHktTerrainRecipe
{
    GENERATED_BODY()

    /** Base noise type: perlin, simplex, ridged, billow */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BaseNoiseType = TEXT("perlin");

    /** Number of noise octaves (1-8) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Octaves = 4;

    /** Base frequency */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Frequency = 0.002f;

    /** Lacunarity (frequency multiplier per octave) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Lacunarity = 2.0f;

    /** Persistence (amplitude multiplier per octave) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Persistence = 0.5f;

    /** Random seed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Seed = 0;

    /** Terrain features: mountains, valleys, etc. positioned by LLM */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FHktTerrainFeature> Features;

    /** Post-processing: erosion passes (0 = none) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ErosionPasses = 0;
};
```

`FHktMapLandscape`에 추가:
```cpp
/** Procedural terrain recipe (used when HeightmapPath is empty) */
UPROPERTY(EditAnywhere, BlueprintReadWrite)
FHktTerrainRecipe TerrainRecipe;
```

#### 1-2. Global Entities 섹션

```cpp
UENUM(BlueprintType)
enum class EHktGlobalEntityType : uint8
{
    WorldBoss    UMETA(DisplayName = "World Boss"),
    NPC          UMETA(DisplayName = "NPC"),
    NPCSpawner   UMETA(DisplayName = "NPC Spawner"),
};

USTRUCT(BlueprintType)
struct FHktMapGlobalEntity
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTag EntityTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EHktGlobalEntityType EntityType = EHktGlobalEntityType::NPC;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Position = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator Rotation = FRotator::ZeroRotator;

    /** Spawn count (for NPCSpawner type) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Count = 1;

    /** Custom properties */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FString> Properties;
};
```

#### 1-3. Environment 섹션

```cpp
USTRUCT(BlueprintType)
struct FHktMapEnvironment
{
    GENERATED_BODY()

    /** Weather preset: clear, rain, snow, fog, storm */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Weather = TEXT("clear");

    /** Time of day: dawn, morning, noon, afternoon, dusk, night */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TimeOfDay = TEXT("noon");

    /** Fog density [0, 1] */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FogDensity = 0.02f;

    /** Wind direction and strength */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector WindDirection = FVector(1.f, 0.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WindStrength = 0.5f;

    /** Ambient light color */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor AmbientColor = FLinearColor(0.5f, 0.5f, 0.6f);

    /** Directional light color */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor SunColor = FLinearColor(1.f, 0.95f, 0.8f);

    /** VFX tags for ambient effects (rain particles, fog volumes, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGameplayTag> AmbientVFXTags;
};
```

#### 1-4. FHktMapData 확장

```cpp
// FHktMapData에 추가
UPROPERTY(EditAnywhere, BlueprintReadWrite)
TArray<FHktMapGlobalEntity> GlobalEntities;

UPROPERTY(EditAnywhere, BlueprintReadWrite)
FHktMapEnvironment Environment;
```

### JSON 스키마 예시 (LLM이 생성할 형태)

```json
{
  "map_id": "dark_forest_001",
  "map_name": "어둠의 숲",
  "description": "안개 자욱한 고대 숲. 중앙에 세계수가 있고, 북쪽은 늪지대.",
  "landscape": {
    "size_x": 4033,
    "size_y": 4033,
    "biome": "dark_forest",
    "height_range": { "min": 0, "max": 800 },
    "material_tag": "Material.Landscape.DarkForest",
    "terrain_recipe": {
      "base_noise_type": "ridged",
      "octaves": 5,
      "frequency": 0.003,
      "lacunarity": 2.2,
      "persistence": 0.45,
      "seed": 42,
      "features": [
        {
          "type": "plateau",
          "position": [0.5, 0.5],
          "radius": 0.15,
          "intensity": 0.3,
          "falloff": "smooth"
        },
        {
          "type": "valley",
          "position": [0.3, 0.7],
          "radius": 0.2,
          "intensity": -0.4,
          "falloff": "smooth"
        },
        {
          "type": "ridge",
          "position": [0.8, 0.3],
          "radius": 0.1,
          "intensity": 0.7,
          "falloff": "sharp"
        }
      ],
      "erosion_passes": 3
    },
    "layers": [
      { "name": "Grass", "material_tag": "Material.Layer.DarkGrass" },
      { "name": "Dirt", "material_tag": "Material.Layer.WetDirt" },
      { "name": "Moss", "material_tag": "Material.Layer.Moss" }
    ]
  },
  "regions": [
    {
      "name": "WorldTree",
      "bounds": { "center": [0, 0, 200], "extent": [2000, 2000, 500] },
      "properties": { "difficulty": "3", "theme": "sacred", "lighting": "dim" }
    },
    {
      "name": "Swamp",
      "bounds": { "center": [3000, 0, 50], "extent": [2500, 2000, 300] },
      "properties": { "difficulty": "5", "theme": "cursed", "fog": "heavy" }
    }
  ],
  "spawners": [
    {
      "entity_tag": "Entity.Character.TreeGuardian",
      "position": [500, 200, 200],
      "spawn_rule": "on_trigger",
      "region": "WorldTree",
      "count": 1,
      "respawn_seconds": 0
    },
    {
      "entity_tag": "Entity.Character.SwampCreature",
      "position": [3000, -500, 50],
      "spawn_rule": "always",
      "region": "Swamp",
      "count": 5,
      "respawn_seconds": 120
    }
  ],
  "global_entities": [
    {
      "entity_tag": "Entity.Character.ForestElder",
      "entity_type": "npc",
      "position": [0, 0, 250],
      "properties": { "dialogue_set": "elder_intro", "shop": "true" }
    },
    {
      "entity_tag": "Entity.Character.AncientDragon",
      "entity_type": "world_boss",
      "position": [5000, 5000, 600],
      "properties": { "level": "50", "phase_count": "3" }
    },
    {
      "entity_tag": "Entity.Character.WanderingMerchant",
      "entity_type": "npc_spawner",
      "position": [1000, 1000, 150],
      "count": 3,
      "properties": { "patrol_radius": "500" }
    }
  ],
  "environment": {
    "weather": "fog",
    "time_of_day": "dusk",
    "fog_density": 0.08,
    "wind_direction": [0.7, 0.3, 0],
    "wind_strength": 0.3,
    "ambient_color": [0.3, 0.35, 0.4, 1.0],
    "sun_color": [0.8, 0.6, 0.4, 1.0],
    "ambient_vfx_tags": ["VFX.Ambient.Fireflies", "VFX.Ambient.FogVolume"]
  },
  "stories": [
    {
      "story_tag": "Story.Quest.WorldTreeBlessing",
      "auto_load": false,
      "trigger_region": "WorldTree"
    },
    {
      "story_tag": "Story.Quest.SwampPurification",
      "auto_load": false,
      "trigger_region": "Swamp"
    },
    {
      "story_tag": "Story.Main.ForestAwakening",
      "auto_load": true
    }
  ],
  "props": [
    {
      "mesh_tag": "Entity.Prop.AncientStone",
      "position": [200, 100, 210],
      "rotation": [0, 45, 0],
      "scale": [2, 2, 2]
    }
  ]
}
```

---

## 2단계: Terrain Recipe Builder (C++)

### 새 파일
- `Source/HktMapGenerator/Public/HktTerrainRecipeBuilder.h`
- `Source/HktMapGenerator/Private/HktTerrainRecipeBuilder.cpp`

### 역할
`FHktTerrainRecipe` → `TArray<uint16>` heightmap 데이터 생성

### 구현 내용

```
FHktTerrainRecipeBuilder
├── GenerateHeightmap(Recipe, SizeX, SizeY) → TArray<uint16>
│   ├── 1. Base noise layer (Perlin/Simplex/Ridged/Billow)
│   │   └── FMath 기반 noise 구현 또는 FastNoise 라이브러리
│   ├── 2. Feature overlay (각 Feature를 heightmap 위에 합성)
│   │   ├── Mountain: Gaussian bump
│   │   ├── Valley: Inverted Gaussian
│   │   ├── Ridge: Directional elongated bump
│   │   ├── Plateau: Flat-top clamped bump
│   │   ├── Crater: Ring shape
│   │   └── RiverBed: Bezier path depression
│   ├── 3. Erosion (선택적, hydraulic erosion simulation)
│   └── 4. Normalize to uint16 range [0, 65535]
└── GenerateWeightMaps(Recipe, Heightmap, Layers) → TArray<TArray<uint8>>
    └── Height/slope 기반 자동 레이어 분배
```

### 노이즈 구현 방식
- UE5 `FMath::PerlinNoise2D`/`FMath::PerlinNoise3D` 사용 가능
- 더 다양한 노이즈가 필요하면 FastNoiseLite (헤더 온리, MIT 라이센스)를 ThirdParty로 추가
- **Ridged noise**: `1.0 - abs(noise)` 방식
- **Billow noise**: `abs(noise)` 방식
- 옥타브 합성은 fBm (fractal Brownian motion)으로 구현

### Feature 합성 공식
각 Feature는 독립적으로 heightmap에 합성:
```
for each pixel (x, y):
    dist = distance(pixel, feature.position * mapSize)
    if dist < feature.radius * mapSize:
        t = dist / (feature.radius * mapSize)
        weight = ApplyFalloff(t, feature.falloff)  // linear, smooth(smoothstep), sharp(1-t^3)
        height[x][y] += feature.intensity * weight * heightRange
```

---

## 3단계: BuildMap C++ 구현

### 변경 대상
- `Source/HktMapGenerator/Private/HktMapGeneratorSubsystem.cpp` — `BuildMap()` 구현

### 새 파일
| 파일 | 설명 |
|---|---|
| `Public/HktMapRegionVolume.h/.cpp` | Region 영역 액터 (BoxComponent 기반) |
| `Public/HktSpawnerActor.h/.cpp` | Spawner 액터 (SpawnRule 로직) |
| `Public/HktMapStoryRegistry.h/.cpp` | Map-Story 바인딩 관리 |
| `Public/HktTerrainRecipeBuilder.h/.cpp` | Terrain Recipe → Heightmap |

### BuildMap 순서

```
BuildMap(MapData)
│
├── 1. Landscape 생성
│   ├── HeightmapPath가 있으면 → 파일에서 로드
│   └── 없으면 → FHktTerrainRecipeBuilder로 절차 생성
│   ├── ALandscape::Import()로 Landscape 생성
│   ├── MaterialTag → ConventionPath로 머티리얼 적용
│   └── Layer별 WeightMap 생성/적용
│
├── 2. Region Volume 생성
│   └── 각 Region → AHktMapRegionVolume 스폰
│       ├── BoxComponent로 영역 정의
│       └── Properties 저장
│
├── 3. Environment 설정
│   ├── DirectionalLight 색상/강도 설정
│   ├── ExponentialHeightFog 밀도 설정
│   ├── Wind 설정
│   └── Ambient VFX 스폰 (HktVFXGenerator 연동)
│
├── 4. Global Entity 스폰
│   ├── WorldBoss → AHktSpawnerActor (SpawnRule=OnTrigger)
│   ├── NPC → 직접 스폰 (Always visible)
│   └── NPCSpawner → AHktSpawnerActor (SpawnRule=Always, Count 설정)
│
├── 5. Region별 Spawner 배치
│   └── 각 Spawner → AHktSpawnerActor 스폰
│       └── Region 참조 설정 (스트리밍 시 활성화/비활성화용)
│
├── 6. Story 등록
│   ├── auto_load=true → 즉시 HktStoryRegistry에 등록
│   └── trigger_region → Region과 바인딩
│
└── 7. Prop 배치
    └── 각 Prop → StaticMeshActor 스폰
```

---

## 4단계: Region 스트리밍 시스템 (Runtime)

### 새 파일
- `Source/HktMapGenerator/Public/HktMapStreamingSubsystem.h`
- `Source/HktMapGenerator/Private/HktMapStreamingSubsystem.cpp`

### 설계

```cpp
UCLASS()
class UHktMapStreamingSubsystem : public UWorldSubsystem
{
    // 에디터가 아닌 런타임에서도 동작하는 WorldSubsystem

    /** 현재 로드된 맵 데이터 */
    FHktMapData CurrentMapData;

    /** Region별 활성 상태 */
    TMap<FString, bool> RegionActiveState;

    /** Region별 소속 액터 추적 */
    TMap<FString, TArray<TWeakObjectPtr<AActor>>> RegionActors;

    /** 글로벌 액터 (Region에 속하지 않음, 항상 활성) */
    TArray<TWeakObjectPtr<AActor>> GlobalActors;

public:
    /** HktMap JSON 파일 경로로 맵 로드 */
    void LoadMap(const FString& MapFilePath);

    /** 현재 맵 언로드 */
    void UnloadMap();

    /** Region 활성화 (해당 Region의 Spawner/Story/Prop 활성화) */
    void ActivateRegion(const FString& RegionName);

    /** Region 비활성화 */
    void DeactivateRegion(const FString& RegionName);

    /** 매 Tick: 플레이어 위치 기반 자동 Region 스트리밍 */
    virtual void Tick(float DeltaTime) override;

private:
    /** 플레이어가 Region 내부에 있는지 판정 */
    bool IsPlayerInRegion(const FHktMapRegion& Region) const;

    /** Region 진입 시 스토리 로드 트리거 */
    void OnRegionEntered(const FString& RegionName);

    /** Region 이탈 시 정리 */
    void OnRegionExited(const FString& RegionName);
};
```

### 스트리밍 로직

```
매 Tick:
  for each Region in CurrentMapData.Regions:
    playerInRegion = IsPlayerInRegion(Region)
    wasActive = RegionActiveState[Region.Name]

    if playerInRegion && !wasActive:
      ActivateRegion(Region.Name)
      → Region 소속 Spawner 활성화 (숨겨진 액터 Unhide + 스폰 시작)
      → Region에 바인딩된 Story 로드
      → Region 소속 Prop 표시

    if !playerInRegion && wasActive:
      DeactivateRegion(Region.Name)
      → Spawner 비활성화 (스폰된 엔티티 제거, 스포너는 유지)
      → trigger_region Story 언로드
      → Prop 숨김
```

### Landscape는 항상 로드
- Landscape는 전체 로드 상태 유지 (지형은 항상 보여야 함)
- 스트리밍 대상: Spawner에서 스폰된 엔티티, Story, Prop
- GlobalEntity는 Region과 무관하게 항상 활성

---

## 5단계: Python LLM 생성 파이프라인

### 변경 대상
- `McpServer/src/hkt_mcp/tools/map_tools.py` — 스키마 업데이트, generate 함수 추가

### 핵심 설계

LLM 에이전트 자체가 MCP 도구를 호출하는 구조이므로, Python 쪽에서 LLM을 호출하지 않는다.
대신, LLM 에이전트가 다음 MCP 도구 시퀀스를 실행한다:

```
1. step_begin(project_id, "map_generation")
   → concept_design 출력(terrain_spec + stories)을 자동으로 입력으로 받음

2. get_map_schema()
   → HktMap JSON 스키마 확인 (terrain_recipe 포함)

3. [LLM이 terrain_spec을 기반으로 HktMap JSON 생성]
   → biome, regions, terrain_recipe, spawners, global_entities, environment, stories 포함

4. validate_map(map_json)
   → 유효성 검사

5. save_map(map_json)
   → .hkt_maps/{map_id}.json 저장

6. step_save_output(project_id, "map_generation", {map_id, map_path, hkt_map})
   → 스텝 완료 처리

7. (선택) build_map(map_json)
   → UE5 에디터에 즉시 빌드
```

### Python 쪽 추가 도구

```python
async def generate_terrain_preview(terrain_recipe: str) -> str:
    """
    Terrain recipe의 ASCII 프리뷰 생성.
    LLM이 생성한 terrain recipe를 시각적으로 확인할 수 있도록
    간단한 ASCII heightmap을 반환한다.
    """
```

이 도구는 LLM이 자신이 생성한 terrain recipe를 검증하는 데 사용한다.
Python에서 noise 라이브러리(`noise` 또는 `opensimplex`)로 경량 heightmap을 생성하고,
ASCII 문자로 변환하여 반환한다.

### HKTMAP_SCHEMA 업데이트

`map_tools.py`의 `HKTMAP_SCHEMA`에 다음 추가:
- `landscape.terrain_recipe` 섹션 (base_noise_type, octaves, frequency, features 등)
- `global_entities` 배열
- `environment` 객체

---

## 6단계: Build.cs 및 모듈 의존성

### HktMapGenerator.Build.cs 변경

```csharp
// 추가 의존성
PrivateDependencyModuleNames.AddRange(new string[] {
    "Landscape",         // ALandscape 생성
    "LandscapeEditor",   // Heightmap import utilities (에디터 빌드용)
    "HktStoryGenerator", // Story 등록
    "HktVFXGenerator",   // Ambient VFX 연동
    "HktGeneratorCore",  // Tag → Path 해석
});
```

---

## 구현 순서 (권장)

| 순서 | 작업 | 파일 수 | 의존성 |
|---|---|---|---|
| 1 | HktMapData.h 스키마 확장 (TerrainRecipe, GlobalEntity, Environment) | 1 | 없음 |
| 2 | JSON 파싱/직렬화 업데이트 (HktMapGeneratorSubsystem.cpp) | 1 | 1 |
| 3 | Python 스키마 업데이트 (map_tools.py) | 1 | 없음 |
| 4 | TerrainRecipeBuilder 구현 | 2 | 1 |
| 5 | HktMapRegionVolume, HktSpawnerActor 구현 | 4 | 없음 |
| 6 | HktMapStoryRegistry 구현 | 2 | HktStoryGenerator |
| 7 | BuildMap() 구현 | 1 | 4, 5, 6 |
| 8 | HktMapStreamingSubsystem 구현 | 2 | 5, 6 |
| 9 | Python generate_terrain_preview 추가 | 1 | 3 |
| 10 | Build.cs 업데이트 | 1 | 전체 |

---

## 기술 결정 요약

| 항목 | 결정 | 이유 |
|---|---|---|
| 맵 포맷 | JSON (HktMap) | 이미 구현된 인프라 활용, LLM이 직접 생성 가능, 사람이 읽기 쉬움 |
| Heightmap 생성 | Terrain Recipe → C++ 절차적 생성 | LLM이 파라미터만 지정, 나머지는 노이즈 알고리즘이 처리 |
| 노이즈 라이브러리 | FastNoiseLite (헤더 온리) | 다양한 노이즈 타입 지원, MIT 라이센스, 의존성 최소 |
| 동적 로드 | Region 단위 스트리밍 (UWorldSubsystem) | 플레이어 위치 기반 자동 활성화, Landscape는 항상 로드 |
| 전역 엔티티 | GlobalEntity 배열 (Boss+NPC+NPCSpawner) | Region과 독립, 맵 로드 시 항상 스폰 |
| 환경 시스템 | Environment 섹션 (날씨, 시간, 안개, 바람) | 맵 분위기 설정, VFX 태그 연동 |
| Story 연동 | StoryRef + TriggerRegion | Region 진입 시 자동 스토리 로드, auto_load는 즉시 |
