// Copyright Hkt Studios, Inc. All Rights Reserved.
// Shape 생성용 타입 정의 — Niagara Mesh Renderer용 프로시저럴 StaticMesh

#pragma once

#include "CoreMinimal.h"
#include "HktShapeTypes.generated.h"

UENUM(BlueprintType)
enum class EHktShapeType : uint8
{
	Star            UMETA(DisplayName = "Star"),
	Ring            UMETA(DisplayName = "Ring"),
	Disc            UMETA(DisplayName = "Disc"),
	Sphere          UMETA(DisplayName = "Sphere"),
	Hemisphere      UMETA(DisplayName = "Hemisphere"),
	Petal           UMETA(DisplayName = "Petal"),
	Diamond         UMETA(DisplayName = "Diamond"),
	Beam            UMETA(DisplayName = "Beam"),
	ShockwaveRing   UMETA(DisplayName = "Shockwave Ring"),
	Spike           UMETA(DisplayName = "Spike / Cone"),
	Cross           UMETA(DisplayName = "Cross / Plus"),
};

UENUM(BlueprintType)
enum class EHktShapePivot : uint8
{
	Center    UMETA(DisplayName = "Center"),
	Bottom    UMETA(DisplayName = "Bottom"),
};

// ============================================================================
// Shape별 파라미터
// ============================================================================

USTRUCT(BlueprintType)
struct HKTMESHGENERATOR_API FHktStarParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "3", ClampMax = "16"))
	int32 Points = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "1.0"))
	float OuterRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "1.0"))
	float InnerRadius = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star", meta = (ClampMin = "0.0"))
	float Thickness = 5.f;
};

USTRUCT(BlueprintType)
struct HKTMESHGENERATOR_API FHktRingParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring", meta = (ClampMin = "1.0"))
	float Radius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring", meta = (ClampMin = "0.5"))
	float TubeRadius = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring", meta = (ClampMin = "8", ClampMax = "64"))
	int32 RadialSegments = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring", meta = (ClampMin = "3", ClampMax = "16"))
	int32 TubeSegments = 8;
};

USTRUCT(BlueprintType)
struct HKTMESHGENERATOR_API FHktDiscParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Disc", meta = (ClampMin = "1.0"))
	float Radius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Disc", meta = (ClampMin = "6", ClampMax = "64"))
	int32 Segments = 24;
};

USTRUCT(BlueprintType)
struct HKTMESHGENERATOR_API FHktSphereParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sphere", meta = (ClampMin = "1.0"))
	float Radius = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sphere", meta = (ClampMin = "4", ClampMax = "32"))
	int32 LatSegments = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sphere", meta = (ClampMin = "6", ClampMax = "64"))
	int32 LonSegments = 12;
};

USTRUCT(BlueprintType)
struct HKTMESHGENERATOR_API FHktPetalParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal", meta = (ClampMin = "1.0"))
	float Length = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal", meta = (ClampMin = "1.0"))
	float Width = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Curvature = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal", meta = (ClampMin = "2", ClampMax = "16"))
	int32 LengthSegments = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Petal", meta = (ClampMin = "2", ClampMax = "16"))
	int32 WidthSegments = 4;
};

USTRUCT(BlueprintType)
struct HKTMESHGENERATOR_API FHktDiamondParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diamond", meta = (ClampMin = "1.0"))
	float Radius = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diamond", meta = (ClampMin = "1.0"))
	float Height = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diamond", meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float MidpointRatio = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diamond", meta = (ClampMin = "4", ClampMax = "16"))
	int32 Sides = 8;
};

USTRUCT(BlueprintType)
struct HKTMESHGENERATOR_API FHktBeamParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam", meta = (ClampMin = "1.0"))
	float Length = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam", meta = (ClampMin = "0.0"))
	float StartRadius = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam", meta = (ClampMin = "0.0"))
	float EndRadius = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam", meta = (ClampMin = "4", ClampMax = "24"))
	int32 Segments = 8;
};

USTRUCT(BlueprintType)
struct HKTMESHGENERATOR_API FHktShockwaveRingParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shockwave", meta = (ClampMin = "1.0"))
	float InnerRadius = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shockwave", meta = (ClampMin = "1.0"))
	float OuterRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shockwave", meta = (ClampMin = "0.0"))
	float Height = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shockwave", meta = (ClampMin = "8", ClampMax = "64"))
	int32 Segments = 24;
};

USTRUCT(BlueprintType)
struct HKTMESHGENERATOR_API FHktSpikeParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spike", meta = (ClampMin = "1.0"))
	float Height = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spike", meta = (ClampMin = "1.0"))
	float BaseRadius = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spike", meta = (ClampMin = "4", ClampMax = "24"))
	int32 Segments = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spike")
	bool bCap = true;
};

USTRUCT(BlueprintType)
struct HKTMESHGENERATOR_API FHktCrossParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross", meta = (ClampMin = "1.0"))
	float ArmLength = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross", meta = (ClampMin = "1.0"))
	float ArmWidth = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross", meta = (ClampMin = "0.0"))
	float Thickness = 5.f;
};
