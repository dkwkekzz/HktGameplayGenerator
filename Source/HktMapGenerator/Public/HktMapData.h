// Copyright Hkt Studios, Inc. All Rights Reserved.
// HktMap JSON 기반 맵 데이터 구조체 — Region별 독립 Landscape로 동적 로드/언로드 지원

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktMapData.generated.h"

// ── Terrain Feature ────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct HKTMAPGENERATOR_API FHktTerrainFeature
{
	GENERATED_BODY()

	/** Feature type: mountain, ridge, valley, plateau, crater, river_bed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Type;

	/** Normalized position [0,1] on the landscape */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D Position = FVector2D(0.5f, 0.5f);

	/** Radius in normalized [0,1] range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Radius = 0.1f;

	/** Height influence [-1, 1]. Positive = elevation, negative = depression */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Intensity = 0.5f;

	/** Falloff curve: linear, smooth, sharp */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Falloff = TEXT("smooth");
};

// ── Terrain Recipe ─────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct HKTMAPGENERATOR_API FHktTerrainRecipe
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

	/** Terrain features positioned by LLM */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FHktTerrainFeature> Features;

	/** Hydraulic erosion passes (0 = none) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ErosionPasses = 0;
};

// ── Landscape Layer ────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct HKTMAPGENERATOR_API FHktMapLandscapeLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag MaterialTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString WeightMapPath;
};

// ── Landscape Config ───────────────────────────────────────────────

USTRUCT(BlueprintType)
struct HKTMAPGENERATOR_API FHktMapLandscape
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SizeX = 2017;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SizeY = 2017;

	/** Direct heightmap file path. If empty, TerrainRecipe is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString HeightmapPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag MaterialTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Biome;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HeightMin = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HeightMax = 1000.f;

	/** Procedural terrain recipe (used when HeightmapPath is empty) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FHktTerrainRecipe TerrainRecipe;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FHktMapLandscapeLayer> Layers;
};

// ── Spawner ────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EHktSpawnRule : uint8
{
	Always        UMETA(DisplayName = "Always"),
	OnStoryStart  UMETA(DisplayName = "On Story Start"),
	OnTrigger     UMETA(DisplayName = "On Trigger"),
	Timed         UMETA(DisplayName = "Timed"),
};

USTRUCT(BlueprintType)
struct HKTMAPGENERATOR_API FHktMapSpawner
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag EntityTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EHktSpawnRule SpawnRule = EHktSpawnRule::Always;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RespawnSeconds = 0.f;
};

// ── Story Reference ────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct HKTMAPGENERATOR_API FHktMapStoryRef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag StoryTag;

	/** Story loads automatically when region loads */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAutoLoad = true;
};

// ── Prop Placement ─────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct HKTMAPGENERATOR_API FHktMapProp
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag MeshTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Scale = FVector::OneVector;
};

// ── Global Entity ──────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EHktGlobalEntityType : uint8
{
	WorldBoss    UMETA(DisplayName = "World Boss"),
	NPC          UMETA(DisplayName = "NPC"),
	NPCSpawner   UMETA(DisplayName = "NPC Spawner"),
};

USTRUCT(BlueprintType)
struct HKTMAPGENERATOR_API FHktMapGlobalEntity
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

	/** Custom properties (dialogue_set, patrol_radius, level, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FString, FString> Properties;
};

// ── Environment ────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct HKTMAPGENERATOR_API FHktMapEnvironment
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector WindDirection = FVector(1.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WindStrength = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor AmbientColor = FLinearColor(0.5f, 0.5f, 0.6f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor SunColor = FLinearColor(1.f, 0.95f, 0.8f);

	/** Ambient VFX tags (rain particles, fog volumes, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FGameplayTag> AmbientVFXTags;
};

// ── Region (with own Landscape) ────────────────────────────────────

/**
 * FHktMapRegion — Region별 독립 Landscape를 가진 스트리밍 단위.
 *
 * 각 Region은 자체 Landscape, Spawner, Story, Prop을 소유하며,
 * 플레이어 위치에 따라 독립적으로 로드/언로드된다.
 */
USTRUCT(BlueprintType)
struct HKTMAPGENERATOR_API FHktMapRegion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	/** World-space center of this region */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Center = FVector::ZeroVector;

	/** World-space half-extent of this region */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Extent = FVector(1000.f);

	/** Custom properties (difficulty, theme, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FString, FString> Properties;

	/** Region의 독립 Landscape 설정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FHktMapLandscape Landscape;

	/** Region 소속 Spawner 목록 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FHktMapSpawner> Spawners;

	/** Region 소속 Story 참조 목록 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FHktMapStoryRef> Stories;

	/** Region 소속 Prop 배치 목록 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FHktMapProp> Props;
};

// ── HktMap (Top-Level) ─────────────────────────────────────────────

/**
 * FHktMapData — JSON 기반 맵 정의.
 *
 * UMap이 아닌 경량 데이터로, Region별 독립 Landscape를 통해
 * 런타임에 동적 로드/언로드 가능. 각 Region은 자체 Landscape,
 * Spawner, Story, Prop을 소유하여 독립 스트리밍 단위로 동작.
 * GlobalEntity와 Environment는 맵 전체에 적용.
 */
USTRUCT(BlueprintType)
struct HKTMAPGENERATOR_API FHktMapData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString MapId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString MapName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Description;

	/** Region 목록 — 각 Region이 독립 Landscape + 콘텐츠를 소유 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FHktMapRegion> Regions;

	/** 전역 엔티티 — Region과 무관하게 맵 로드 시 항상 스폰 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FHktMapGlobalEntity> GlobalEntities;

	/** 환경 설정 — 맵 전체에 적용되는 날씨, 시간, 안개, 바람 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FHktMapEnvironment Environment;

	/** 맵 전역 스토리 (특정 Region이 아닌 맵 로드 시 즉시 로드) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FHktMapStoryRef> GlobalStories;
};
