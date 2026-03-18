// Copyright Hkt Studios, Inc. All Rights Reserved.
// Map-Story 바인딩 관리 — Region 활성화 시 스토리 로드/언로드

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktMapData.h"
#include "HktMapStoryRegistry.generated.h"

/**
 * UHktMapStoryRegistry
 *
 * HktMap의 Story 참조를 관리하는 컴포넌트.
 * Global Story는 맵 로드 시 즉시 등록.
 * Region Story는 Region 활성화/비활성화에 연동.
 */
UCLASS()
class HKTMAPGENERATOR_API UHktMapStoryRegistry : public UObject
{
	GENERATED_BODY()

public:
	/** 전역 스토리 등록 (맵 로드 시) */
	void RegisterGlobalStories(const TArray<FHktMapStoryRef>& Stories);

	/** Region 활성화 시 해당 Region의 스토리 로드 */
	void OnRegionActivated(const FString& RegionName, const TArray<FHktMapStoryRef>& Stories);

	/** Region 비활성화 시 해당 Region의 스토리 언로드 */
	void OnRegionDeactivated(const FString& RegionName);

	/** 전체 초기화 (맵 언로드 시) */
	void Clear();

	/** 현재 로드된 스토리 태그 목록 */
	TArray<FGameplayTag> GetLoadedStoryTags() const;

private:
	/** 글로벌 스토리 (항상 활성) */
	TArray<FGameplayTag> GlobalStoryTags;

	/** Region별 활성 스토리 */
	TMap<FString, TArray<FGameplayTag>> RegionStoryTags;

	/** 현재 VM에 로드된 스토리 태그 (중복 로드 방지) */
	TSet<FGameplayTag> LoadedStorySet;

	/** 스토리 태그를 HktStoryGenerator에 등록 */
	void LoadStory(const FGameplayTag& StoryTag);

	/** 스토리 태그를 HktStoryGenerator에서 해제 */
	void UnloadStory(const FGameplayTag& StoryTag);
};
