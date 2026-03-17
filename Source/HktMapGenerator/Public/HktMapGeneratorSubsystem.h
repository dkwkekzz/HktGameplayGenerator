// Copyright Hkt Studios, Inc. All Rights Reserved.
// HktMap Generator Editor Subsystem — JSON → World 빌드 API

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "HktMapData.h"
#include "HktMapGeneratorSubsystem.generated.h"

/**
 * UHktMapGeneratorSubsystem
 *
 * 에디터 서브시스템으로 HktMap JSON → UE5 월드 반영 기능 제공.
 * MCP를 통해 호출되며, JSON 파싱 → Landscape 생성 → Spawner 배치 → Story 연결.
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

	/** HktMapData로 현재 레벨에 맵 빌드 (Landscape + Spawner + Story 등록) */
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

	/** MCP: JSON으로 맵 빌드 (bridge 호출용) */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapGenerator|MCP")
	FString McpBuildMap(const FString& JsonStr);

	/** MCP: 맵 유효성 검사 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapGenerator|MCP")
	FString McpValidateMap(const FString& JsonStr);

	/** MCP: 맵 스키마 반환 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapGenerator|MCP")
	FString McpGetMapSchema();

private:
	FString CurrentMapId;

	/** 빌드 시 생성된 액터 추적 (언로드용) */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SpawnedActors;
};
