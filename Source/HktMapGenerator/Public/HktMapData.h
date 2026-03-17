// Copyright Hkt Studios, Inc. All Rights Reserved.
// HktMap JSON 기반 맵 데이터 구조체 — 동적 로드/언로드를 위한 경량 맵 정의

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktMapData.generated.h"

// ── Landscape Layer ─────────────────────────────────────────────────

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

// ── Landscape Config ────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct HKTMAPGENERATOR_API FHktMapLandscape
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SizeX = 8129;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SizeY = 8129;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FHktMapLandscapeLayer> Layers;
};

// ── Region ──────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct HKTMAPGENERATOR_API FHktMapRegion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Center = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Extent = FVector(1000.f);

	/** Custom properties (difficulty, theme, etc.) - JSON string */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FString, FString> Properties;
};

// ── Spawner ─────────────────────────────────────────────────────────

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
	FString Region;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RespawnSeconds = 0.f;
};

// ── Story Reference ─────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct HKTMAPGENERATOR_API FHktMapStoryRef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag StoryTag;

	/** Story loads automatically when map loads */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAutoLoad = true;

	/** Region that triggers story loading (empty = global) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString TriggerRegion;
};

// ── Prop Placement ──────────────────────────────────────────────────

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

// ── HktMap (Top-Level) ──────────────────────────────────────────────

/**
 * FHktMapData — JSON 기반 맵 정의.
 *
 * UMap이 아닌 경량 데이터로, 런타임에 동적 로드/언로드 가능.
 * Landscape, Spawner, Region, Story 참조, Prop 배치를 포함.
 * MCP에서 JSON으로 생성하고, UE5에서 파싱하여 월드에 반영.
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FHktMapLandscape Landscape;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FHktMapRegion> Regions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FHktMapSpawner> Spawners;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FHktMapStoryRef> Stories;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FHktMapProp> Props;
};
