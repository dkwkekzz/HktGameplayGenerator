// Copyright Hkt Studios, Inc. All Rights Reserved.
// Layer 2 — Shape Asset Factory (해시 캐싱 + 카탈로그)

#pragma once

#include "CoreMinimal.h"
#include "HktShapeTypes.h"
#include "HktShapeFactory.generated.h"

class UStaticMesh;

/**
 * Shape 카탈로그 엔트리 — 생성된 Shape의 메타데이터
 */
USTRUCT(BlueprintType)
struct HKTMESHGENERATOR_API FHktShapeCatalogEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	EHktShapeType ShapeType = EHktShapeType::Disc;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	FString AssetPath;

	/** 파라미터 해시 (중복 방지) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	FString ParamsHash;

	/** 원본 JSON 파라미터 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	FString ParamsJson;
};

/**
 * UHktShapeFactory
 *
 * Shape StaticMesh 에셋 생성 + 해시 기반 캐싱.
 * 같은 파라미터로 이미 만든 에셋이 있으면 재사용.
 */
UCLASS()
class HKTMESHGENERATOR_API UHktShapeFactory : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * JSON 파라미터로 Shape StaticMesh 에셋 생성/조회.
	 * @param JsonParams  shape 파라미터 JSON 문자열
	 * @param OutputDir   출력 디렉토리 (빈 문자열이면 기본 경로)
	 * @return 생성된(또는 캐싱된) StaticMesh 에셋 경로. 실패 시 빈 문자열.
	 */
	FString CreateShapeAsset(const FString& JsonParams, const FString& OutputDir = TEXT(""));

	/** 카탈로그 목록 반환 (JSON) */
	FString GetCatalogJson() const;

	/** 카탈로그에서 이름으로 검색 */
	FString FindShapeByName(const FString& ShapeName) const;

	/** 기본 출력 경로 */
	static FString GetDefaultOutputDir();

private:
	/** 파라미터 → 해시 */
	static FString ComputeParamsHash(const FString& JsonParams);

	/** MeshDescription → UStaticMesh 에셋으로 저장 */
	UStaticMesh* SaveMeshAsset(struct FMeshDescription& MeshDesc, const FString& PackagePath, const FString& AssetName);

	/** 메모리 카탈로그 (런타임 세션 동안 유지) */
	UPROPERTY()
	TArray<FHktShapeCatalogEntry> Catalog;
};
