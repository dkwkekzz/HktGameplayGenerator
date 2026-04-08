// Copyright Hkt Studios, Inc. All Rights Reserved.
// Layer 1 — 프로시저럴 Shape 메시 생성

#pragma once

#include "CoreMinimal.h"
#include "HktShapeTypes.h"

struct FMeshDescription;

/**
 * FHktShapeGenerator
 *
 * Shape별 프로시저럴 메시를 FMeshDescription으로 빌드.
 * Niagara Mesh Renderer용 low-poly StaticMesh 전용.
 * Collision 없음, Emissive/Unlit 기본.
 */
class HKTMESHGENERATOR_API FHktShapeGenerator
{
public:
	// =========================================================================
	// Shape별 빌드
	// =========================================================================

	static void BuildStar(const FHktStarParams& Params, float Scale, EHktShapePivot Pivot, FMeshDescription& OutMesh);
	static void BuildRing(const FHktRingParams& Params, float Scale, EHktShapePivot Pivot, FMeshDescription& OutMesh);
	static void BuildDisc(const FHktDiscParams& Params, float Scale, EHktShapePivot Pivot, FMeshDescription& OutMesh);
	static void BuildSphere(const FHktSphereParams& Params, bool bHemisphere, float Scale, EHktShapePivot Pivot, FMeshDescription& OutMesh);
	static void BuildPetal(const FHktPetalParams& Params, float Scale, EHktShapePivot Pivot, FMeshDescription& OutMesh);
	static void BuildDiamond(const FHktDiamondParams& Params, float Scale, EHktShapePivot Pivot, FMeshDescription& OutMesh);
	static void BuildBeam(const FHktBeamParams& Params, float Scale, EHktShapePivot Pivot, FMeshDescription& OutMesh);
	static void BuildShockwaveRing(const FHktShockwaveRingParams& Params, float Scale, EHktShapePivot Pivot, FMeshDescription& OutMesh);
	static void BuildSpike(const FHktSpikeParams& Params, float Scale, EHktShapePivot Pivot, FMeshDescription& OutMesh);
	static void BuildCross(const FHktCrossParams& Params, float Scale, EHktShapePivot Pivot, FMeshDescription& OutMesh);

	/** 문자열 → EHktShapeType 변환 (매칭 실패 시 Disc fallback) */
	static EHktShapeType ParseShapeType(const FString& TypeString);

	/** JSON에서 타입 + 파라미터 파싱 후 적절한 Build 호출 */
	static bool BuildFromJson(const TSharedPtr<class FJsonObject>& JsonParams, FMeshDescription& OutMesh);
};

// ============================================================================
// 내부 빌더 헬퍼 (cpp에서만 사용하지만 테스트 접근 위해 Public)
// ============================================================================

struct HKTMESHGENERATOR_API FShapeMeshBuilder
{
	struct FVert
	{
		FVector3f Pos;
		FVector3f Normal;
		FVector2f UV;
	};

	TArray<FVert> Verts;
	TArray<uint32> Tris; // 3 per triangle

	int32 V(const FVector3f& P, const FVector3f& N, const FVector2f& UV)
	{
		return Verts.Add({P, N, UV});
	}

	int32 V(float X, float Y, float Z, float NX, float NY, float NZ, float U, float V_)
	{
		return Verts.Add({FVector3f(X,Y,Z), FVector3f(NX,NY,NZ), FVector2f(U,V_)});
	}

	void Tri(int32 A, int32 B, int32 C)
	{
		Tris.Add(A); Tris.Add(B); Tris.Add(C);
	}

	void Quad(int32 A, int32 B, int32 C, int32 D)
	{
		Tri(A, B, C);
		Tri(A, C, D);
	}

	/** FMeshDescription에 커밋 */
	void Commit(FMeshDescription& OutMesh) const;

	/** 모든 정점에 Pivot offset 적용 */
	void ApplyPivot(EHktShapePivot Pivot);
};
