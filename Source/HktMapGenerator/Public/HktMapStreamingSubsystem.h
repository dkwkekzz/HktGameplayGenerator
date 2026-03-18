// Copyright Hkt Studios, Inc. All Rights Reserved.
// Runtime Region 스트리밍 — 플레이어 위치 기반 Region별 동적 로드/언로드

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HktMapData.h"
#include "HktMapStoryRegistry.h"
#include "HktMapStreamingSubsystem.generated.h"

class AHktMapRegionVolume;
class AHktSpawnerActor;

/**
 * UHktMapStreamingSubsystem
 *
 * Runtime WorldSubsystem으로, HktMap의 Region을 플레이어 위치에 따라
 * 동적으로 활성화/비활성화한다. Landscape는 Region별 독립 ALandscape이므로
 * Region 전체(지형 + Spawner + Story + Prop)를 하나의 단위로 관리.
 *
 * GlobalEntity와 GlobalStory는 맵 로드 시 항상 활성.
 */
UCLASS()
class HKTMAPGENERATOR_API UHktMapStreamingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Map Load / Unload ───────────────────────────────────────

	/** HktMap JSON 파일 경로로 맵 로드 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapStreaming")
	bool LoadMap(const FString& MapFilePath);

	/** HktMapData로 직접 맵 로드 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapStreaming")
	bool LoadMapFromData(const FHktMapData& MapData);

	/** 현재 맵 언로드 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapStreaming")
	void UnloadMap();

	// ── Region Control ──────────────────────────────────────────

	/** Region 수동 활성화 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapStreaming")
	void ActivateRegion(const FString& RegionName);

	/** Region 수동 비활성화 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapStreaming")
	void DeactivateRegion(const FString& RegionName);

	/** Region 활성 상태 조회 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapStreaming")
	bool IsRegionActive(const FString& RegionName) const;

	/** 현재 맵 ID */
	UFUNCTION(BlueprintCallable, Category = "HKT|MapStreaming")
	FString GetCurrentMapId() const { return CurrentMapData.MapId; }

	/** 자동 스트리밍 활성화/비활성화 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|MapStreaming")
	bool bAutoStreamingEnabled = true;

	/** 자동 스트리밍 갱신 간격 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|MapStreaming")
	float StreamingUpdateInterval = 0.5f;

private:
	/** 현재 로드된 맵 데이터 */
	FHktMapData CurrentMapData;

	/** Region별 활성 상태 */
	TMap<FString, bool> RegionActiveState;

	/** Region별 소속 액터 (Landscape, Spawners, Props) */
	struct FRegionActors
	{
		TArray<TWeakObjectPtr<AActor>> AllActors;
		TArray<TWeakObjectPtr<AHktSpawnerActor>> Spawners;
		TWeakObjectPtr<AHktMapRegionVolume> Volume;
	};
	TMap<FString, FRegionActors> RegionActorMap;

	/** 글로벌 액터 (Region에 속하지 않음) */
	TArray<TWeakObjectPtr<AActor>> GlobalActors;

	/** Story 관리 */
	UPROPERTY()
	TObjectPtr<UHktMapStoryRegistry> StoryRegistry;

	/** 스트리밍 타이머 핸들 */
	FTimerHandle StreamingTimerHandle;

	/** 자동 스트리밍 Tick */
	void OnStreamingUpdate();

	/** 플레이어의 현재 위치 가져오기 */
	FVector GetPlayerLocation() const;

	/** Region에 대한 Landscape + 콘텐츠 생성 */
	void SpawnRegionContent(const FHktMapRegion& Region);

	/** Region 콘텐츠 파괴 */
	void DestroyRegionContent(const FString& RegionName);

	/** GlobalEntity 스폰 */
	void SpawnGlobalContent(const FHktMapData& MapData);

	/** Environment 적용 (라이팅, 포그, 바람) */
	void ApplyEnvironment(const FHktMapEnvironment& Env);
};
