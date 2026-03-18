// Copyright Hkt Studios, Inc. All Rights Reserved.
// HktMap Generator Editor Subsystem — JSON → World 빌드 API

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "HktMapData.h"
#include "HktMapJsonParser.h"
#include "HktMapStoryRegistry.h"
#include "HktMapGeneratorSubsystem.generated.h"

class AHktMapRegionVolume;
class AHktSpawnerActor;

/**
 * UHktMapGeneratorSubsystem
 *
 * 에디터 서브시스템으로 HktMap JSON → UE5 월드 반영 기능 제공.
 * MCP를 통해 호출되며, JSON 파싱 → Region별 Landscape 생성 →
 * Spawner 배치 → Story 연결 → GlobalEntity 스폰.
 */
UCLASS()
class HKTMAPGENERATOR_API UHktMapGeneratorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── JSON I/O ────────────────────────────────────────────────

	/** JSON 문자열로부터 HktMapData 파싱 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapGenerator")
	bool ParseMapFromJson(const FString& JsonStr, FHktMapData& OutMapData);

	/** HktMapData → JSON 문자열 직렬화 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapGenerator")
	FString SerializeMapToJson(const FHktMapData& MapData);

	// ── Build ───────────────────────────────────────────────────

	/** HktMapData로 현재 레벨에 맵 빌드 (Region별 Landscape + Spawner + Story) */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapGenerator")
	bool BuildMap(const FHktMapData& MapData);

	/** JSON 문자열로 맵 빌드 (ParseMap + BuildMap 통합) */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapGenerator")
	bool BuildMapFromJson(const FString& JsonStr);

	// ── Load / Unload ───────────────────────────────────────────

	/** 저장된 HktMap JSON 파일 로드 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapGenerator")
	bool LoadMapFromFile(const FString& FilePath, FHktMapData& OutMapData);

	/** HktMap JSON 파일로 저장 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapGenerator")
	bool SaveMapToFile(const FHktMapData& MapData, const FString& FilePath);

	/** 현재 빌드된 맵 언로드 (월드에서 관련 액터 제거) */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapGenerator")
	void UnloadCurrentMap();

	// ── Query ───────────────────────────────────────────────────

	/** 저장된 맵 목록 반환 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapGenerator")
	TArray<FString> ListSavedMaps();

	/** 현재 로드된 맵 ID */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapGenerator")
	FString GetCurrentMapId() const { return CurrentMapId; }

	// ── MCP Endpoints ───────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "HKT|MapGenerator|MCP")
	FString McpBuildMap(const FString& JsonStr);

	UFUNCTION(BlueprintCallable, Category = "HKT|MapGenerator|MCP")
	FString McpValidateMap(const FString& JsonStr);

	UFUNCTION(BlueprintCallable, Category = "HKT|MapGenerator|MCP")
	FString McpGetMapSchema();

private:
	FString CurrentMapId;

	/** 빌드 시 생성된 전체 액터 추적 (언로드용) */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SpawnedActors;

	/** Story 관리 */
	UPROPERTY()
	TObjectPtr<UHktMapStoryRegistry> StoryRegistry;

	UHktMapStoryRegistry* GetOrCreateStoryRegistry();

	// ── Build Helpers ───────────────────────────────────────────

	/** Region별 Landscape 생성 */
	bool BuildRegionLandscape(const FHktMapRegion& Region, UWorld* World);

	/** Region Volume 생성 */
	AHktMapRegionVolume* BuildRegionVolume(const FHktMapRegion& Region, UWorld* World);

	/** Spawner 액터 배치 */
	void BuildSpawners(const TArray<FHktMapSpawner>& Spawners, UWorld* World);

	/** Prop 배치 */
	void BuildProps(const TArray<FHktMapProp>& Props, UWorld* World);

	/** GlobalEntity 스폰 */
	void BuildGlobalEntities(const TArray<FHktMapGlobalEntity>& Entities, UWorld* World);

	/** Environment 설정 적용 */
	void ApplyEnvironment(const FHktMapEnvironment& Env, UWorld* World);
};
