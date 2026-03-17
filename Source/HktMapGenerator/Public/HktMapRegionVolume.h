// Copyright Hkt Studios, Inc. All Rights Reserved.
// Region 영역 표시용 Box Volume 액터

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "HktMapRegionVolume.generated.h"

/**
 * AHktMapRegionVolume
 *
 * HktMap Region의 영역을 나타내는 경량 액터.
 * 에디터에서는 시각화, 런타임에서는 플레이어 진입/이탈 감지에 사용.
 * 자체적으로 Overlap 이벤트를 처리하여 Region 스트리밍을 트리거한다.
 */
UCLASS()
class HKTMAPGENERATOR_API AHktMapRegionVolume : public AActor
{
	GENERATED_BODY()

public:
	AHktMapRegionVolume();

	/** Region 이름 (FHktMapRegion::Name과 일치) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|Region")
	FString RegionName;

	/** Custom properties */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|Region")
	TMap<FString, FString> Properties;

	/** Region 초기화 */
	void InitFromRegionData(const FString& Name, FVector Center, FVector Extent, const TMap<FString, FString>& Props);

	/** 해당 위치가 이 Region 내부인지 판정 */
	UFUNCTION(BlueprintCallable, Category = "HKT|Region")
	bool ContainsPoint(FVector WorldPoint) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HKT|Region")
	TObjectPtr<UBoxComponent> BoxComponent;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
