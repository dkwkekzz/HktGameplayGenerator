// Copyright Hkt Studios, Inc. All Rights Reserved.
// TerrainRecipe → Heightmap 절차적 생성

#pragma once

#include "CoreMinimal.h"
#include "HktMapData.h"

/**
 * FHktTerrainRecipeBuilder
 *
 * FHktTerrainRecipe로부터 heightmap (TArray<uint16>)을 절차적으로 생성한다.
 * Base noise (Perlin/Simplex/Ridged/Billow) + Feature overlay + Erosion.
 * 생성된 heightmap은 ALandscape::Import()에 직접 전달 가능.
 */
struct HKTMAPGENERATOR_API FHktTerrainRecipeBuilder
{
	/**
	 * TerrainRecipe에서 heightmap 데이터를 생성한다.
	 *
	 * @param Recipe    지형 레시피 (노이즈 파라미터 + Feature 배치)
	 * @param SizeX     Heightmap 가로 크기 (pixels)
	 * @param SizeY     Heightmap 세로 크기 (pixels)
	 * @param HeightMin 최소 높이 (UE5 units)
	 * @param HeightMax 최대 높이 (UE5 units)
	 * @return          uint16 heightmap 데이터 (SizeX * SizeY)
	 */
	static TArray<uint16> GenerateHeightmap(
		const FHktTerrainRecipe& Recipe,
		int32 SizeX, int32 SizeY,
		float HeightMin = 0.f, float HeightMax = 1000.f);

	/**
	 * Heightmap + Landscape 설정에서 레이어별 WeightMap을 자동 생성한다.
	 * 높이/경사도 기반으로 레이어를 분배한다.
	 *
	 * @param Heightmap  GenerateHeightmap()의 출력
	 * @param SizeX      가로 크기
	 * @param SizeY      세로 크기
	 * @param LayerCount 레이어 수
	 * @return           레이어별 uint8 WeightMap 배열
	 */
	static TArray<TArray<uint8>> GenerateWeightMaps(
		const TArray<uint16>& Heightmap,
		int32 SizeX, int32 SizeY,
		int32 LayerCount);

private:
	// ── Noise Functions ─────────────────────────────────────────

	/** Perlin noise 2D */
	static float PerlinNoise2D(float X, float Y, int32 Seed);

	/** Ridged noise: 1.0 - |noise| */
	static float RidgedNoise2D(float X, float Y, int32 Seed);

	/** Billow noise: |noise| */
	static float BillowNoise2D(float X, float Y, int32 Seed);

	/** fBm (fractal Brownian motion) using specified noise type */
	static float FBM(float X, float Y, const FHktTerrainRecipe& Recipe);

	// ── Feature Application ─────────────────────────────────────

	/** Apply a single terrain feature to a height value */
	static float ApplyFeature(float Height, float NormX, float NormY, const FHktTerrainFeature& Feature);

	/** Compute falloff weight */
	static float ComputeFalloff(float T, const FString& FalloffType);

	// ── Erosion ─────────────────────────────────────────────────

	/** Simple hydraulic erosion simulation */
	static void ApplyErosion(TArray<float>& HeightmapF, int32 SizeX, int32 SizeY, int32 Passes);
};
