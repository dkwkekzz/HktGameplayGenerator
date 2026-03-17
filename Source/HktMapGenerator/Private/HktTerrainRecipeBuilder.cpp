// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktTerrainRecipeBuilder.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktTerrain, Log, All);

// ── Noise Functions ─────────────────────────────────────────────────

namespace
{
	/** Hash-based gradient table for Perlin noise */
	FORCEINLINE float GradientDot(int32 Hash, float X, float Y)
	{
		switch (Hash & 3)
		{
		case 0: return  X + Y;
		case 1: return -X + Y;
		case 2: return  X - Y;
		case 3: return -X - Y;
		default: return 0.f;
		}
	}

	FORCEINLINE float Fade(float T)
	{
		return T * T * T * (T * (T * 6.f - 15.f) + 10.f);
	}

	FORCEINLINE int32 HashCoord(int32 X, int32 Y, int32 Seed)
	{
		int32 H = X * 73856093 ^ Y * 19349663 ^ Seed * 83492791;
		H = (H ^ (H >> 13)) * 1274126177;
		return H;
	}
} // anonymous namespace

float FHktTerrainRecipeBuilder::PerlinNoise2D(float X, float Y, int32 Seed)
{
	int32 X0 = FMath::FloorToInt(X);
	int32 Y0 = FMath::FloorToInt(Y);
	int32 X1 = X0 + 1;
	int32 Y1 = Y0 + 1;

	float FracX = X - X0;
	float FracY = Y - Y0;

	float U = Fade(FracX);
	float V = Fade(FracY);

	float N00 = GradientDot(HashCoord(X0, Y0, Seed), FracX,       FracY);
	float N10 = GradientDot(HashCoord(X1, Y0, Seed), FracX - 1.f, FracY);
	float N01 = GradientDot(HashCoord(X0, Y1, Seed), FracX,       FracY - 1.f);
	float N11 = GradientDot(HashCoord(X1, Y1, Seed), FracX - 1.f, FracY - 1.f);

	float Nx0 = FMath::Lerp(N00, N10, U);
	float Nx1 = FMath::Lerp(N01, N11, U);
	return FMath::Lerp(Nx0, Nx1, V);
}

float FHktTerrainRecipeBuilder::RidgedNoise2D(float X, float Y, int32 Seed)
{
	return 1.0f - FMath::Abs(PerlinNoise2D(X, Y, Seed));
}

float FHktTerrainRecipeBuilder::BillowNoise2D(float X, float Y, int32 Seed)
{
	return FMath::Abs(PerlinNoise2D(X, Y, Seed));
}

float FHktTerrainRecipeBuilder::FBM(float X, float Y, const FHktTerrainRecipe& Recipe)
{
	float Value = 0.f;
	float Amplitude = 1.f;
	float Freq = Recipe.Frequency;
	float MaxAmp = 0.f;

	for (int32 i = 0; i < Recipe.Octaves; ++i)
	{
		float SampleX = X * Freq;
		float SampleY = Y * Freq;

		float NoiseVal;
		if (Recipe.BaseNoiseType == TEXT("ridged"))
		{
			NoiseVal = RidgedNoise2D(SampleX, SampleY, Recipe.Seed + i);
		}
		else if (Recipe.BaseNoiseType == TEXT("billow"))
		{
			NoiseVal = BillowNoise2D(SampleX, SampleY, Recipe.Seed + i);
		}
		else // perlin, simplex (using perlin as fallback)
		{
			NoiseVal = PerlinNoise2D(SampleX, SampleY, Recipe.Seed + i);
		}

		Value += NoiseVal * Amplitude;
		MaxAmp += Amplitude;
		Amplitude *= Recipe.Persistence;
		Freq *= Recipe.Lacunarity;
	}

	return (MaxAmp > 0.f) ? Value / MaxAmp : 0.f;
}

// ── Feature Application ─────────────────────────────────────────────

float FHktTerrainRecipeBuilder::ComputeFalloff(float T, const FString& FalloffType)
{
	if (FalloffType == TEXT("linear"))
	{
		return 1.0f - T;
	}
	if (FalloffType == TEXT("sharp"))
	{
		return 1.0f - T * T * T;
	}
	// smooth (smoothstep)
	return 1.0f - (3.f * T * T - 2.f * T * T * T);
}

float FHktTerrainRecipeBuilder::ApplyFeature(float Height, float NormX, float NormY, const FHktTerrainFeature& Feature)
{
	float FX = Feature.Position.X;
	float FY = Feature.Position.Y;
	float Dist = FMath::Sqrt((NormX - FX) * (NormX - FX) + (NormY - FY) * (NormY - FY));

	if (Dist >= Feature.Radius)
	{
		return Height;
	}

	float T = Dist / Feature.Radius;
	float Weight = ComputeFalloff(T, Feature.Falloff);

	return Height + Feature.Intensity * Weight;
}

// ── Erosion ─────────────────────────────────────────────────────────

void FHktTerrainRecipeBuilder::ApplyErosion(TArray<float>& HeightmapF, int32 SizeX, int32 SizeY, int32 Passes)
{
	if (Passes <= 0) return;

	// Simple thermal erosion: smooth height differences that exceed a threshold
	const float Threshold = 0.02f;
	const float TransferRate = 0.3f;

	TArray<float> Temp;
	Temp.SetNumUninitialized(SizeX * SizeY);

	for (int32 Pass = 0; Pass < Passes; ++Pass)
	{
		FMemory::Memcpy(Temp.GetData(), HeightmapF.GetData(), SizeX * SizeY * sizeof(float));

		for (int32 Y = 1; Y < SizeY - 1; ++Y)
		{
			for (int32 X = 1; X < SizeX - 1; ++X)
			{
				float Center = Temp[Y * SizeX + X];
				float TotalDiff = 0.f;
				int32 LowerCount = 0;

				// Check 4 neighbors
				static const int32 DX[] = { -1, 1, 0, 0 };
				static const int32 DY[] = { 0, 0, -1, 1 };

				for (int32 D = 0; D < 4; ++D)
				{
					float Neighbor = Temp[(Y + DY[D]) * SizeX + (X + DX[D])];
					float Diff = Center - Neighbor;
					if (Diff > Threshold)
					{
						TotalDiff += Diff;
						++LowerCount;
					}
				}

				if (LowerCount > 0)
				{
					float Transfer = TotalDiff * TransferRate / LowerCount;
					HeightmapF[Y * SizeX + X] -= Transfer * LowerCount;

					for (int32 D = 0; D < 4; ++D)
					{
						float Neighbor = Temp[(Y + DY[D]) * SizeX + (X + DX[D])];
						float Diff = Center - Neighbor;
						if (Diff > Threshold)
						{
							HeightmapF[(Y + DY[D]) * SizeX + (X + DX[D])] += Transfer;
						}
					}
				}
			}
		}
	}
}

// ── Main Generation ─────────────────────────────────────────────────

TArray<uint16> FHktTerrainRecipeBuilder::GenerateHeightmap(
	const FHktTerrainRecipe& Recipe,
	int32 SizeX, int32 SizeY,
	float HeightMin, float HeightMax)
{
	UE_LOG(LogHktTerrain, Log, TEXT("Generating heightmap %dx%d — noise=%s octaves=%d features=%d erosion=%d"),
		SizeX, SizeY, *Recipe.BaseNoiseType, Recipe.Octaves, Recipe.Features.Num(), Recipe.ErosionPasses);

	// Phase 1: Generate float heightmap with noise + features
	TArray<float> HeightmapF;
	HeightmapF.SetNumUninitialized(SizeX * SizeY);

	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		float NormY = static_cast<float>(Y) / SizeY;
		for (int32 X = 0; X < SizeX; ++X)
		{
			float NormX = static_cast<float>(X) / SizeX;

			// Base noise
			float H = FBM(NormX * 1000.f, NormY * 1000.f, Recipe);

			// Apply features
			for (const auto& Feature : Recipe.Features)
			{
				H = ApplyFeature(H, NormX, NormY, Feature);
			}

			HeightmapF[Y * SizeX + X] = H;
		}
	}

	// Phase 2: Erosion
	ApplyErosion(HeightmapF, SizeX, SizeY, Recipe.ErosionPasses);

	// Phase 3: Normalize and convert to uint16
	float MinVal = HeightmapF[0], MaxVal = HeightmapF[0];
	for (float V : HeightmapF)
	{
		MinVal = FMath::Min(MinVal, V);
		MaxVal = FMath::Max(MaxVal, V);
	}
	float Range = MaxVal - MinVal;
	if (Range < KINDA_SMALL_NUMBER) Range = 1.f;

	TArray<uint16> Heightmap;
	Heightmap.SetNumUninitialized(SizeX * SizeY);

	for (int32 i = 0; i < SizeX * SizeY; ++i)
	{
		float Normalized = (HeightmapF[i] - MinVal) / Range;
		Heightmap[i] = static_cast<uint16>(FMath::Clamp(Normalized * 65535.f, 0.f, 65535.f));
	}

	UE_LOG(LogHktTerrain, Log, TEXT("Heightmap generated — range [%.2f, %.2f]"), MinVal, MaxVal);
	return Heightmap;
}

TArray<TArray<uint8>> FHktTerrainRecipeBuilder::GenerateWeightMaps(
	const TArray<uint16>& Heightmap,
	int32 SizeX, int32 SizeY,
	int32 LayerCount)
{
	TArray<TArray<uint8>> WeightMaps;
	if (LayerCount <= 0) return WeightMaps;

	WeightMaps.SetNum(LayerCount);
	for (auto& WM : WeightMaps)
	{
		WM.SetNumZeroed(SizeX * SizeY);
	}

	// Distribute layers by height bands:
	// Layer 0: low elevation (0.0 - 0.3)   e.g. grass/dirt
	// Layer 1: mid elevation (0.2 - 0.6)   e.g. rock
	// Layer 2: high elevation (0.5 - 1.0)  e.g. snow/stone
	// Additional layers subdivide evenly

	for (int32 i = 0; i < SizeX * SizeY; ++i)
	{
		float Normalized = Heightmap[i] / 65535.f;

		// Calculate slope from neighbors
		float Slope = 0.f;
		int32 X = i % SizeX;
		int32 Y = i / SizeX;
		if (X > 0 && X < SizeX - 1 && Y > 0 && Y < SizeY - 1)
		{
			float DX = (Heightmap[Y * SizeX + (X + 1)] - Heightmap[Y * SizeX + (X - 1)]) / 65535.f;
			float DY = (Heightmap[(Y + 1) * SizeX + X] - Heightmap[(Y - 1) * SizeX + X]) / 65535.f;
			Slope = FMath::Sqrt(DX * DX + DY * DY);
		}

		// Compute weights for each layer
		TArray<float> Weights;
		Weights.SetNumZeroed(LayerCount);

		float BandSize = 1.0f / LayerCount;
		float Overlap = BandSize * 0.5f;

		for (int32 L = 0; L < LayerCount; ++L)
		{
			float BandCenter = (L + 0.5f) * BandSize;
			float Dist = FMath::Abs(Normalized - BandCenter);
			float HalfWidth = BandSize * 0.5f + Overlap;

			if (Dist < HalfWidth)
			{
				Weights[L] = 1.0f - (Dist / HalfWidth);
			}

			// Steep slopes favor middle layers (rocky)
			if (Slope > 0.05f && L == LayerCount / 2)
			{
				Weights[L] += Slope * 3.0f;
			}
		}

		// Normalize and write
		float Total = 0.f;
		for (float W : Weights) Total += W;
		if (Total < KINDA_SMALL_NUMBER) Total = 1.f;

		for (int32 L = 0; L < LayerCount; ++L)
		{
			WeightMaps[L][i] = static_cast<uint8>(FMath::Clamp(Weights[L] / Total * 255.f, 0.f, 255.f));
		}
	}

	return WeightMaps;
}
