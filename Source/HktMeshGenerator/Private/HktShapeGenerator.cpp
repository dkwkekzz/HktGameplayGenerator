// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktShapeGenerator.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Dom/JsonObject.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktShapeGen, Log, All);

// ============================================================================
// FShapeMeshBuilder
// ============================================================================

void FShapeMeshBuilder::Commit(FMeshDescription& OutMesh) const
{
	FStaticMeshAttributes Attrs(OutMesh);
	Attrs.Register();

	FPolygonGroupID PolyGroup = OutMesh.CreatePolygonGroup();

	// 정점 생성 — 1 VertexID per Vert (position만)
	TArray<FVertexID> VertexIDs;
	VertexIDs.SetNum(Verts.Num());
	for (int32 i = 0; i < Verts.Num(); ++i)
	{
		VertexIDs[i] = OutMesh.CreateVertex();
		Attrs.GetVertexPositions()[VertexIDs[i]] = Verts[i].Pos;
	}

	// 삼각형마다 VertexInstance 생성
	for (int32 i = 0; i < Tris.Num(); i += 3)
	{
		TArray<FVertexInstanceID> TriVIs;
		TriVIs.SetNum(3);
		for (int32 j = 0; j < 3; ++j)
		{
			const uint32 Idx = Tris[i + j];
			FVertexInstanceID VI = OutMesh.CreateVertexInstance(VertexIDs[Idx]);
			Attrs.GetVertexInstanceNormals()[VI] = Verts[Idx].Normal;
			Attrs.GetVertexInstanceUVs().Set(VI, 0, Verts[Idx].UV);
			// 탄젠트: 노멀에서 단순 계산
			FVector3f T = FVector3f::CrossProduct(Verts[Idx].Normal, FVector3f(0, 0, 1)).GetSafeNormal();
			if (T.IsNearlyZero()) T = FVector3f::CrossProduct(Verts[Idx].Normal, FVector3f(0, 1, 0)).GetSafeNormal();
			Attrs.GetVertexInstanceTangents()[VI] = T;
			Attrs.GetVertexInstanceBinormalSigns()[VI] = 1.f;
			TriVIs[j] = VI;
		}
		OutMesh.CreateTriangle(PolyGroup, TriVIs);
	}
}

void FShapeMeshBuilder::ApplyPivot(EHktShapePivot Pivot)
{
	if (Pivot != EHktShapePivot::Bottom || Verts.Num() == 0) return;

	float MinZ = Verts[0].Pos.Z;
	for (const auto& V_ : Verts) MinZ = FMath::Min(MinZ, V_.Pos.Z);
	for (auto& V_ : Verts) V_.Pos.Z -= MinZ;
}

// ============================================================================
// 개별 Shape 빌더
// ============================================================================

void FHktShapeGenerator::BuildStar(const FHktStarParams& P, float S, EHktShapePivot Pivot, FMeshDescription& Out)
{
	FShapeMeshBuilder B;
	const int32 N = P.Points * 2;
	const float HalfT = P.Thickness * S * 0.5f;
	const FVector3f Up(0, 0, 1);
	const FVector3f Down(0, 0, -1);

	// 중심 정점 (상/하)
	int32 CenterTop = B.V(0, 0, HalfT, 0, 0, 1, 0.5f, 0.5f);
	int32 CenterBot = (HalfT > 0.f) ? B.V(0, 0, -HalfT, 0, 0, -1, 0.5f, 0.5f) : -1;

	// 외곽 정점
	TArray<int32> TopRing, BotRing;
	for (int32 i = 0; i < N; ++i)
	{
		float Angle = PI * 2.f * i / N - PI * 0.5f;
		float R = (i % 2 == 0) ? P.OuterRadius * S : P.InnerRadius * S;
		float X = FMath::Cos(Angle) * R;
		float Y = FMath::Sin(Angle) * R;
		float U = FMath::Cos(Angle) * 0.5f + 0.5f;
		float V_ = FMath::Sin(Angle) * 0.5f + 0.5f;
		TopRing.Add(B.V(X, Y, HalfT, 0, 0, 1, U, V_));
		if (HalfT > 0.f)
			BotRing.Add(B.V(X, Y, -HalfT, 0, 0, -1, U, V_));
	}

	// 상면 fan
	for (int32 i = 0; i < N; ++i)
		B.Tri(CenterTop, TopRing[i], TopRing[(i + 1) % N]);

	// 하면 fan (두께 > 0)
	if (HalfT > 0.f)
	{
		for (int32 i = 0; i < N; ++i)
			B.Tri(CenterBot, BotRing[(i + 1) % N], BotRing[i]);

		// 측면 quad strip
		for (int32 i = 0; i < N; ++i)
		{
			int32 Next = (i + 1) % N;
			FVector3f Edge = FVector3f(B.Verts[TopRing[Next]].Pos - B.Verts[TopRing[i]].Pos);
			FVector3f SideN = FVector3f::CrossProduct(Edge, FVector3f(0, 0, 1)).GetSafeNormal();
			float U0 = (float)i / N, U1 = (float)(i + 1) / N;
			int32 A = B.V(B.Verts[TopRing[i]].Pos, SideN, FVector2f(U0, 0));
			int32 Bv = B.V(B.Verts[TopRing[Next]].Pos, SideN, FVector2f(U1, 0));
			int32 C = B.V(B.Verts[BotRing[Next]].Pos, SideN, FVector2f(U1, 1));
			int32 D = B.V(B.Verts[BotRing[i]].Pos, SideN, FVector2f(U0, 1));
			B.Quad(A, Bv, C, D);
		}
	}

	B.ApplyPivot(Pivot);
	B.Commit(Out);
}

void FHktShapeGenerator::BuildRing(const FHktRingParams& P, float S, EHktShapePivot Pivot, FMeshDescription& Out)
{
	FShapeMeshBuilder B;
	const float MajR = P.Radius * S;
	const float MinR = P.TubeRadius * S;

	for (int32 i = 0; i <= P.RadialSegments; ++i)
	{
		float Phi = 2.f * PI * i / P.RadialSegments;
		FVector3f Center(FMath::Cos(Phi) * MajR, FMath::Sin(Phi) * MajR, 0);
		FVector3f RadDir(FMath::Cos(Phi), FMath::Sin(Phi), 0);

		for (int32 j = 0; j <= P.TubeSegments; ++j)
		{
			float Theta = 2.f * PI * j / P.TubeSegments;
			FVector3f Pos = Center + (RadDir * FMath::Cos(Theta) + FVector3f(0, 0, 1) * FMath::Sin(Theta)) * MinR;
			FVector3f Norm = (Pos - Center).GetSafeNormal();
			FVector2f UV((float)i / P.RadialSegments, (float)j / P.TubeSegments);
			B.V(Pos, Norm, UV);
		}
	}

	const int32 Stride = P.TubeSegments + 1;
	for (int32 i = 0; i < P.RadialSegments; ++i)
	{
		for (int32 j = 0; j < P.TubeSegments; ++j)
		{
			int32 A = i * Stride + j;
			int32 Bv = A + Stride;
			int32 C = Bv + 1;
			int32 D = A + 1;
			B.Quad(A, Bv, C, D);
		}
	}

	B.ApplyPivot(Pivot);
	B.Commit(Out);
}

void FHktShapeGenerator::BuildDisc(const FHktDiscParams& P, float S, EHktShapePivot Pivot, FMeshDescription& Out)
{
	FShapeMeshBuilder B;
	const float R = P.Radius * S;
	const FVector3f N(0, 0, 1);

	int32 Center = B.V(0, 0, 0, 0, 0, 1, 0.5f, 0.5f);
	TArray<int32> Ring;
	for (int32 i = 0; i < P.Segments; ++i)
	{
		float A = 2.f * PI * i / P.Segments;
		Ring.Add(B.V(FMath::Cos(A) * R, FMath::Sin(A) * R, 0,
			0, 0, 1,
			FMath::Cos(A) * 0.5f + 0.5f, FMath::Sin(A) * 0.5f + 0.5f));
	}
	for (int32 i = 0; i < P.Segments; ++i)
		B.Tri(Center, Ring[i], Ring[(i + 1) % P.Segments]);

	B.ApplyPivot(Pivot);
	B.Commit(Out);
}

void FHktShapeGenerator::BuildSphere(const FHktSphereParams& P, bool bHemi, float S, EHktShapePivot Pivot, FMeshDescription& Out)
{
	FShapeMeshBuilder B;
	const float R = P.Radius * S;
	const int32 LatMax = bHemi ? P.LatSegments / 2 : P.LatSegments;

	for (int32 lat = 0; lat <= LatMax; ++lat)
	{
		float V_ = (float)lat / P.LatSegments;
		float Theta = PI * V_;
		for (int32 lon = 0; lon <= P.LonSegments; ++lon)
		{
			float U = (float)lon / P.LonSegments;
			float Phi = 2.f * PI * U;
			FVector3f Norm(
				FMath::Sin(Theta) * FMath::Cos(Phi),
				FMath::Sin(Theta) * FMath::Sin(Phi),
				FMath::Cos(Theta));
			B.V(Norm * R, Norm, FVector2f(U, V_));
		}
	}

	const int32 Stride = P.LonSegments + 1;
	for (int32 lat = 0; lat < LatMax; ++lat)
	{
		for (int32 lon = 0; lon < P.LonSegments; ++lon)
		{
			int32 A = lat * Stride + lon;
			int32 Bv = A + Stride;
			B.Quad(A, Bv, Bv + 1, A + 1);
		}
	}

	// Hemisphere cap
	if (bHemi)
	{
		int32 CenterIdx = B.V(0, 0, 0, 0, 0, -1, 0.5f, 0.5f);
		int32 BaseRow = LatMax * Stride;
		for (int32 lon = 0; lon < P.LonSegments; ++lon)
			B.Tri(CenterIdx, BaseRow + lon + 1, BaseRow + lon);
	}

	B.ApplyPivot(Pivot);
	B.Commit(Out);
}

void FHktShapeGenerator::BuildPetal(const FHktPetalParams& P, float S, EHktShapePivot Pivot, FMeshDescription& Out)
{
	FShapeMeshBuilder B;
	const float L = P.Length * S;
	const float W = P.Width * S;

	for (int32 i = 0; i <= P.LengthSegments; ++i)
	{
		float T = (float)i / P.LengthSegments;
		float X = T * L;
		// 잎 폭 프로파일: sin 기반
		float WidthScale = FMath::Sin(T * PI);
		float ZCurve = P.Curvature * FMath::Sin(T * PI) * L * 0.2f;

		for (int32 j = 0; j <= P.WidthSegments; ++j)
		{
			float S2 = (float)j / P.WidthSegments;
			float Y = (S2 - 0.5f) * W * WidthScale;
			float Z = ZCurve;
			B.V(X, Y, Z, 0, 0, 1, T, S2);
		}
	}

	const int32 Stride = P.WidthSegments + 1;
	for (int32 i = 0; i < P.LengthSegments; ++i)
	{
		for (int32 j = 0; j < P.WidthSegments; ++j)
		{
			int32 A = i * Stride + j;
			int32 Bv = A + Stride;
			B.Quad(A, Bv, Bv + 1, A + 1);
		}
	}

	B.ApplyPivot(Pivot);
	B.Commit(Out);
}

void FHktShapeGenerator::BuildDiamond(const FHktDiamondParams& P, float S, EHktShapePivot Pivot, FMeshDescription& Out)
{
	FShapeMeshBuilder B;
	const float R = P.Radius * S;
	const float H = P.Height * S;
	const float MidZ = H * P.MidpointRatio - H * 0.5f;

	for (int32 i = 0; i < P.Sides; ++i)
	{
		float A0 = 2.f * PI * i / P.Sides;
		float A1 = 2.f * PI * ((i + 1) % P.Sides) / P.Sides;
		FVector3f P0(FMath::Cos(A0) * R, FMath::Sin(A0) * R, MidZ);
		FVector3f P1(FMath::Cos(A1) * R, FMath::Sin(A1) * R, MidZ);
		FVector3f Top(0, 0, H * 0.5f);
		FVector3f Bot(0, 0, -H * 0.5f);

		// 상단 삼각형
		FVector3f NTop = FVector3f::CrossProduct(P1 - Top, P0 - Top).GetSafeNormal();
		float U0 = (float)i / P.Sides, U1 = (float)(i + 1) / P.Sides;
		int32 T0 = B.V(Top, NTop, FVector2f((U0 + U1) * 0.5f, 0));
		int32 T1 = B.V(P0, NTop, FVector2f(U0, 0.5f));
		int32 T2 = B.V(P1, NTop, FVector2f(U1, 0.5f));
		B.Tri(T0, T1, T2);

		// 하단 삼각형
		FVector3f NBot = FVector3f::CrossProduct(P0 - Bot, P1 - Bot).GetSafeNormal();
		int32 B0 = B.V(Bot, NBot, FVector2f((U0 + U1) * 0.5f, 1));
		int32 B1 = B.V(P1, NBot, FVector2f(U1, 0.5f));
		int32 B2 = B.V(P0, NBot, FVector2f(U0, 0.5f));
		B.Tri(B0, B1, B2);
	}

	B.ApplyPivot(Pivot);
	B.Commit(Out);
}

void FHktShapeGenerator::BuildBeam(const FHktBeamParams& P, float S, EHktShapePivot Pivot, FMeshDescription& Out)
{
	FShapeMeshBuilder B;
	const float L = P.Length * S;
	const float SR = P.StartRadius * S;
	const float ER = P.EndRadius * S;

	// 상하 링
	TArray<int32> BotRing, TopRing;
	for (int32 i = 0; i <= P.Segments; ++i)
	{
		float A = 2.f * PI * i / P.Segments;
		float C = FMath::Cos(A), Sn = FMath::Sin(A);
		FVector3f NDir(C, Sn, 0);
		float U = (float)i / P.Segments;
		BotRing.Add(B.V(C * SR, Sn * SR, 0, C, Sn, 0, U, 0));
		TopRing.Add(B.V(C * ER, Sn * ER, L, C, Sn, 0, U, 1));
	}

	for (int32 i = 0; i < P.Segments; ++i)
		B.Quad(BotRing[i], TopRing[i], TopRing[i + 1], BotRing[i + 1]);

	B.ApplyPivot(Pivot);
	B.Commit(Out);
}

void FHktShapeGenerator::BuildShockwaveRing(const FHktShockwaveRingParams& P, float S, EHktShapePivot Pivot, FMeshDescription& Out)
{
	FShapeMeshBuilder B;
	const float IR = P.InnerRadius * S;
	const float OR = P.OuterRadius * S;
	const float HH = P.Height * S * 0.5f;

	// 4개 링: InnerBot, InnerTop, OuterBot, OuterTop
	TArray<int32> IB, IT, OB, OT;
	for (int32 i = 0; i <= P.Segments; ++i)
	{
		float A = 2.f * PI * i / P.Segments;
		float C = FMath::Cos(A), Sn = FMath::Sin(A);
		float U = (float)i / P.Segments;
		IB.Add(B.V(C * IR, Sn * IR, -HH, -C, -Sn, 0, U, 0));
		IT.Add(B.V(C * IR, Sn * IR, HH, -C, -Sn, 0, U, 1));
		OB.Add(B.V(C * OR, Sn * OR, -HH, C, Sn, 0, U, 0));
		OT.Add(B.V(C * OR, Sn * OR, HH, C, Sn, 0, U, 1));
	}

	for (int32 i = 0; i < P.Segments; ++i)
	{
		// 외벽
		B.Quad(OB[i], OT[i], OT[i + 1], OB[i + 1]);
		// 내벽
		B.Quad(IT[i], IB[i], IB[i + 1], IT[i + 1]);
		// 상면
		int32 TIB = B.V(B.Verts[IT[i]].Pos, FVector3f(0,0,1), FVector2f((float)i/P.Segments, 0));
		int32 TIB2 = B.V(B.Verts[IT[i+1]].Pos, FVector3f(0,0,1), FVector2f((float)(i+1)/P.Segments, 0));
		int32 TOB = B.V(B.Verts[OT[i]].Pos, FVector3f(0,0,1), FVector2f((float)i/P.Segments, 1));
		int32 TOB2 = B.V(B.Verts[OT[i+1]].Pos, FVector3f(0,0,1), FVector2f((float)(i+1)/P.Segments, 1));
		B.Quad(TIB, TOB, TOB2, TIB2);
		// 하면
		int32 BIB = B.V(B.Verts[IB[i]].Pos, FVector3f(0,0,-1), FVector2f((float)i/P.Segments, 0));
		int32 BIB2 = B.V(B.Verts[IB[i+1]].Pos, FVector3f(0,0,-1), FVector2f((float)(i+1)/P.Segments, 0));
		int32 BOB = B.V(B.Verts[OB[i]].Pos, FVector3f(0,0,-1), FVector2f((float)i/P.Segments, 1));
		int32 BOB2 = B.V(B.Verts[OB[i+1]].Pos, FVector3f(0,0,-1), FVector2f((float)(i+1)/P.Segments, 1));
		B.Quad(BOB, BIB, BIB2, BOB2);
	}

	B.ApplyPivot(Pivot);
	B.Commit(Out);
}

void FHktShapeGenerator::BuildSpike(const FHktSpikeParams& P, float S, EHktShapePivot Pivot, FMeshDescription& Out)
{
	FShapeMeshBuilder B;
	const float H = P.Height * S;
	const float R = P.BaseRadius * S;

	// Apex + base ring
	for (int32 i = 0; i < P.Segments; ++i)
	{
		float A0 = 2.f * PI * i / P.Segments;
		float A1 = 2.f * PI * ((i + 1) % P.Segments) / P.Segments;
		FVector3f Apex(0, 0, H);
		FVector3f V0(FMath::Cos(A0) * R, FMath::Sin(A0) * R, 0);
		FVector3f V1(FMath::Cos(A1) * R, FMath::Sin(A1) * R, 0);
		FVector3f FN = FVector3f::CrossProduct(V1 - Apex, V0 - Apex).GetSafeNormal();
		float U0 = (float)i / P.Segments, U1 = (float)(i + 1) / P.Segments;
		int32 A_ = B.V(Apex, FN, FVector2f((U0 + U1) * 0.5f, 0));
		int32 B0 = B.V(V0, FN, FVector2f(U0, 1));
		int32 B1 = B.V(V1, FN, FVector2f(U1, 1));
		B.Tri(A_, B0, B1);
	}

	// 바닥 캡
	if (P.bCap)
	{
		int32 CenterIdx = B.V(0, 0, 0, 0, 0, -1, 0.5f, 0.5f);
		for (int32 i = 0; i < P.Segments; ++i)
		{
			float A0 = 2.f * PI * i / P.Segments;
			float A1 = 2.f * PI * ((i + 1) % P.Segments) / P.Segments;
			int32 V0 = B.V(FMath::Cos(A1) * R, FMath::Sin(A1) * R, 0, 0, 0, -1,
				FMath::Cos(A1) * 0.5f + 0.5f, FMath::Sin(A1) * 0.5f + 0.5f);
			int32 V1 = B.V(FMath::Cos(A0) * R, FMath::Sin(A0) * R, 0, 0, 0, -1,
				FMath::Cos(A0) * 0.5f + 0.5f, FMath::Sin(A0) * 0.5f + 0.5f);
			B.Tri(CenterIdx, V0, V1);
		}
	}

	B.ApplyPivot(Pivot);
	B.Commit(Out);
}

void FHktShapeGenerator::BuildCross(const FHktCrossParams& P, float S, EHktShapePivot Pivot, FMeshDescription& Out)
{
	FShapeMeshBuilder B;
	const float L = P.ArmLength * S;
	const float W = P.ArmWidth * S * 0.5f;
	const float HT = P.Thickness * S * 0.5f;

	// Plus 프로파일: 12 꼭지점 (시계방향)
	// 가로 팔: (-L,-W) → (L,-W) → (L,W) → (-L,W)
	// 세로 팔: (-W,-L) → (W,-L) → (W,L) → (-W,L)
	const FVector2f Profile[12] = {
		{W, -L}, {W, -W}, {L, -W},
		{L, W}, {W, W}, {W, L},
		{-W, L}, {-W, W}, {-L, W},
		{-L, -W}, {-W, -W}, {-W, -L}
	};

	auto MakeFace = [&](float Z, const FVector3f& N, bool bFlip)
	{
		int32 CIdx = B.V(0, 0, Z, N.X, N.Y, N.Z, 0.5f, 0.5f);
		TArray<int32> Ring;
		for (int32 i = 0; i < 12; ++i)
		{
			Ring.Add(B.V(Profile[i].X, Profile[i].Y, Z, N.X, N.Y, N.Z,
				Profile[i].X / (L * 2) + 0.5f, Profile[i].Y / (L * 2) + 0.5f));
		}
		for (int32 i = 0; i < 12; ++i)
		{
			if (bFlip)
				B.Tri(CIdx, Ring[(i + 1) % 12], Ring[i]);
			else
				B.Tri(CIdx, Ring[i], Ring[(i + 1) % 12]);
		}
	};

	// 상면
	MakeFace(HT, FVector3f(0, 0, 1), false);

	if (HT > 0.f)
	{
		// 하면
		MakeFace(-HT, FVector3f(0, 0, -1), true);

		// 측면
		for (int32 i = 0; i < 12; ++i)
		{
			int32 Next = (i + 1) % 12;
			FVector3f Dir(Profile[Next].X - Profile[i].X, Profile[Next].Y - Profile[i].Y, 0);
			FVector3f SN = FVector3f::CrossProduct(Dir, FVector3f(0, 0, 1)).GetSafeNormal();
			int32 A_ = B.V(Profile[i].X, Profile[i].Y, HT, SN.X, SN.Y, SN.Z, (float)i / 12, 0);
			int32 Bv = B.V(Profile[Next].X, Profile[Next].Y, HT, SN.X, SN.Y, SN.Z, (float)(i + 1) / 12, 0);
			int32 C_ = B.V(Profile[Next].X, Profile[Next].Y, -HT, SN.X, SN.Y, SN.Z, (float)(i + 1) / 12, 1);
			int32 D_ = B.V(Profile[i].X, Profile[i].Y, -HT, SN.X, SN.Y, SN.Z, (float)i / 12, 1);
			B.Quad(A_, Bv, C_, D_);
		}
	}

	B.ApplyPivot(Pivot);
	B.Commit(Out);
}

// ============================================================================
// JSON 디스패처
// ============================================================================

EHktShapeType FHktShapeGenerator::ParseShapeType(const FString& Str)
{
	if (Str.Equals(TEXT("Star"), ESearchCase::IgnoreCase)) return EHktShapeType::Star;
	if (Str.Equals(TEXT("Ring"), ESearchCase::IgnoreCase)) return EHktShapeType::Ring;
	if (Str.Equals(TEXT("Disc"), ESearchCase::IgnoreCase)) return EHktShapeType::Disc;
	if (Str.Equals(TEXT("Sphere"), ESearchCase::IgnoreCase)) return EHktShapeType::Sphere;
	if (Str.Equals(TEXT("Hemisphere"), ESearchCase::IgnoreCase)) return EHktShapeType::Hemisphere;
	if (Str.Equals(TEXT("Petal"), ESearchCase::IgnoreCase)) return EHktShapeType::Petal;
	if (Str.Equals(TEXT("Diamond"), ESearchCase::IgnoreCase)) return EHktShapeType::Diamond;
	if (Str.Equals(TEXT("Beam"), ESearchCase::IgnoreCase)) return EHktShapeType::Beam;
	if (Str.Equals(TEXT("ShockwaveRing"), ESearchCase::IgnoreCase)) return EHktShapeType::ShockwaveRing;
	if (Str.Equals(TEXT("Spike"), ESearchCase::IgnoreCase)) return EHktShapeType::Spike;
	if (Str.Equals(TEXT("Cross"), ESearchCase::IgnoreCase)) return EHktShapeType::Cross;
	return EHktShapeType::Disc; // fallback
}

bool FHktShapeGenerator::BuildFromJson(const TSharedPtr<FJsonObject>& J, FMeshDescription& Out)
{
	if (!J.IsValid()) return false;

	EHktShapeType Type = ParseShapeType(J->GetStringField(TEXT("shapeType")));
	float Scale = J->HasField(TEXT("scale")) ? J->GetNumberField(TEXT("scale")) : 1.f;
	EHktShapePivot Pivot = J->GetStringField(TEXT("pivot")).Equals(TEXT("bottom"), ESearchCase::IgnoreCase)
		? EHktShapePivot::Bottom : EHktShapePivot::Center;

	switch (Type)
	{
	case EHktShapeType::Star:
	{
		FHktStarParams P;
		P.Points = J->HasField(TEXT("points")) ? (int32)J->GetNumberField(TEXT("points")) : P.Points;
		P.OuterRadius = J->HasField(TEXT("outerRadius")) ? J->GetNumberField(TEXT("outerRadius")) : P.OuterRadius;
		P.InnerRadius = J->HasField(TEXT("innerRadius")) ? J->GetNumberField(TEXT("innerRadius")) : P.InnerRadius;
		P.Thickness = J->HasField(TEXT("thickness")) ? J->GetNumberField(TEXT("thickness")) : P.Thickness;
		BuildStar(P, Scale, Pivot, Out);
		return true;
	}
	case EHktShapeType::Ring:
	{
		FHktRingParams P;
		P.Radius = J->HasField(TEXT("radius")) ? J->GetNumberField(TEXT("radius")) : P.Radius;
		P.TubeRadius = J->HasField(TEXT("tubeRadius")) ? J->GetNumberField(TEXT("tubeRadius")) : P.TubeRadius;
		P.RadialSegments = J->HasField(TEXT("radialSegments")) ? (int32)J->GetNumberField(TEXT("radialSegments")) : P.RadialSegments;
		P.TubeSegments = J->HasField(TEXT("tubeSegments")) ? (int32)J->GetNumberField(TEXT("tubeSegments")) : P.TubeSegments;
		BuildRing(P, Scale, Pivot, Out);
		return true;
	}
	case EHktShapeType::Disc:
	{
		FHktDiscParams P;
		P.Radius = J->HasField(TEXT("radius")) ? J->GetNumberField(TEXT("radius")) : P.Radius;
		P.Segments = J->HasField(TEXT("segments")) ? (int32)J->GetNumberField(TEXT("segments")) : P.Segments;
		BuildDisc(P, Scale, Pivot, Out);
		return true;
	}
	case EHktShapeType::Sphere:
	case EHktShapeType::Hemisphere:
	{
		FHktSphereParams P;
		P.Radius = J->HasField(TEXT("radius")) ? J->GetNumberField(TEXT("radius")) : P.Radius;
		P.LatSegments = J->HasField(TEXT("latSegments")) ? (int32)J->GetNumberField(TEXT("latSegments")) : P.LatSegments;
		P.LonSegments = J->HasField(TEXT("lonSegments")) ? (int32)J->GetNumberField(TEXT("lonSegments")) : P.LonSegments;
		BuildSphere(P, Type == EHktShapeType::Hemisphere, Scale, Pivot, Out);
		return true;
	}
	case EHktShapeType::Petal:
	{
		FHktPetalParams P;
		P.Length = J->HasField(TEXT("length")) ? J->GetNumberField(TEXT("length")) : P.Length;
		P.Width = J->HasField(TEXT("width")) ? J->GetNumberField(TEXT("width")) : P.Width;
		P.Curvature = J->HasField(TEXT("curvature")) ? J->GetNumberField(TEXT("curvature")) : P.Curvature;
		P.LengthSegments = J->HasField(TEXT("lengthSegments")) ? (int32)J->GetNumberField(TEXT("lengthSegments")) : P.LengthSegments;
		P.WidthSegments = J->HasField(TEXT("widthSegments")) ? (int32)J->GetNumberField(TEXT("widthSegments")) : P.WidthSegments;
		BuildPetal(P, Scale, Pivot, Out);
		return true;
	}
	case EHktShapeType::Diamond:
	{
		FHktDiamondParams P;
		P.Radius = J->HasField(TEXT("radius")) ? J->GetNumberField(TEXT("radius")) : P.Radius;
		P.Height = J->HasField(TEXT("height")) ? J->GetNumberField(TEXT("height")) : P.Height;
		P.MidpointRatio = J->HasField(TEXT("midpointRatio")) ? J->GetNumberField(TEXT("midpointRatio")) : P.MidpointRatio;
		P.Sides = J->HasField(TEXT("sides")) ? (int32)J->GetNumberField(TEXT("sides")) : P.Sides;
		BuildDiamond(P, Scale, Pivot, Out);
		return true;
	}
	case EHktShapeType::Beam:
	{
		FHktBeamParams P;
		P.Length = J->HasField(TEXT("length")) ? J->GetNumberField(TEXT("length")) : P.Length;
		P.StartRadius = J->HasField(TEXT("startRadius")) ? J->GetNumberField(TEXT("startRadius")) : P.StartRadius;
		P.EndRadius = J->HasField(TEXT("endRadius")) ? J->GetNumberField(TEXT("endRadius")) : P.EndRadius;
		P.Segments = J->HasField(TEXT("segments")) ? (int32)J->GetNumberField(TEXT("segments")) : P.Segments;
		BuildBeam(P, Scale, Pivot, Out);
		return true;
	}
	case EHktShapeType::ShockwaveRing:
	{
		FHktShockwaveRingParams P;
		P.InnerRadius = J->HasField(TEXT("innerRadius")) ? J->GetNumberField(TEXT("innerRadius")) : P.InnerRadius;
		P.OuterRadius = J->HasField(TEXT("outerRadius")) ? J->GetNumberField(TEXT("outerRadius")) : P.OuterRadius;
		P.Height = J->HasField(TEXT("height")) ? J->GetNumberField(TEXT("height")) : P.Height;
		P.Segments = J->HasField(TEXT("segments")) ? (int32)J->GetNumberField(TEXT("segments")) : P.Segments;
		BuildShockwaveRing(P, Scale, Pivot, Out);
		return true;
	}
	case EHktShapeType::Spike:
	{
		FHktSpikeParams P;
		P.Height = J->HasField(TEXT("height")) ? J->GetNumberField(TEXT("height")) : P.Height;
		P.BaseRadius = J->HasField(TEXT("baseRadius")) ? J->GetNumberField(TEXT("baseRadius")) : P.BaseRadius;
		P.Segments = J->HasField(TEXT("segments")) ? (int32)J->GetNumberField(TEXT("segments")) : P.Segments;
		P.bCap = J->HasField(TEXT("cap")) ? J->GetBoolField(TEXT("cap")) : P.bCap;
		BuildSpike(P, Scale, Pivot, Out);
		return true;
	}
	case EHktShapeType::Cross:
	{
		FHktCrossParams P;
		P.ArmLength = J->HasField(TEXT("armLength")) ? J->GetNumberField(TEXT("armLength")) : P.ArmLength;
		P.ArmWidth = J->HasField(TEXT("armWidth")) ? J->GetNumberField(TEXT("armWidth")) : P.ArmWidth;
		P.Thickness = J->HasField(TEXT("thickness")) ? J->GetNumberField(TEXT("thickness")) : P.Thickness;
		BuildCross(P, Scale, Pivot, Out);
		return true;
	}
	}

	return false;
}
