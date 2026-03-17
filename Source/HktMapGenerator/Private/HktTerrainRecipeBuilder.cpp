// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktTerrainRecipeBuilder.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktTerrain, Log, All);

// ── Constants ───────────────────────────────────────────────────────

static constexpr int32 MAX_LANDSCAPE_SIZE = 8129;
static constexpr int32 MAX_EROSION_PASSES = 20;
static constexpr int32 MAX_OCTAVES = 8;
static constexpr int32 MAX_FEATURES = 256;

// ── Noise Functions ─────────────────────────────────────────────────

namespace
{
	/**
	 * Gradient table for Perlin noise — 8 unit-length gradient directions.
	 * Using 8 gradients instead of 4 reduces directional bias.
	 */
	FORCEINLINE float GradientDot(int32 Hash, float X, float Y)
	{
		// Normalize by 1/sqrt(2) ≈ 0.7071 for unit-length diagonals
		constexpr float InvSqrt2 = 0.70710678f;
		switch (Hash & 7)
		{
		case 0: return  X;
		case 1: return  Y;
		case 2: return -X;
		case 3: return -Y;
		case 4: return (X + Y) * InvSqrt2;
		case 5: return (X - Y) * InvSqrt2;
		case 6: return (-X + Y) * InvSqrt2;
		case 7: return (-X - Y) * InvSqrt2;
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

	// Result is in [-1, 1] due to normalized gradients
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

	int32 ClampedOctaves = FMath::Clamp(Recipe.Octaves, 1, MAX_OCTAVES);

	for (int32 i = 0; i < ClampedOctaves; ++i)
	{
		float SampleX = X * Freq;
		float SampleY = Y * Freq;

		float NoiseVal;
		if (Recipe.BaseNoiseType.Equals(TEXT("ridged"), ESearchCase::IgnoreCase))
		{
			NoiseVal = RidgedNoise2D(SampleX, SampleY, Recipe.Seed + i);
		}
		else if (Recipe.BaseNoiseType.Equals(TEXT("billow"), ESearchCase::IgnoreCase))
		{
			NoiseVal = BillowNoise2D(SampleX, SampleY, Recipe.Seed + i);
		}
		else // perlin (default)
		{
			NoiseVal = PerlinNoise2D(SampleX, SampleY, Recipe.Seed + i);
		}

		Value += NoiseVal * Amplitude;
		MaxAmp += Amplitude;
		Amplitude *= FMath::Clamp(Recipe.Persistence, 0.01f, 1.0f);
		Freq *= FMath::Max(Recipe.Lacunarity, 1.0f);
	}

	return (MaxAmp > 0.f) ? Value / MaxAmp : 0.f;
}

// ── Feature Application ─────────────────────────────────────────────

float FHktTerrainRecipeBuilder::ComputeFalloff(float T, const FString& FalloffType)
{
	if (FalloffType.Equals(TEXT("linear"), ESearchCase::IgnoreCase))
	{
		return 1.0f - T;
	}
	if (FalloffType.Equals(TEXT("sharp"), ESearchCase::IgnoreCase))
	{
		return 1.0f - T * T * T;
	}
	// smooth (smoothstep) — default
	return 1.0f - (3.f * T * T - 2.f * T * T * T);
}

float FHktTerrainRecipeBuilder::ApplyFeature(float Height, float NormX, float NormY, const FHktTerrainFeature& Feature)
{
	float FX = FMath::Clamp(Feature.Position.X, 0.f, 1.f);
	float FY = FMath::Clamp(Feature.Position.Y, 0.f, 1.f);
	float Radius = FMath::Clamp(Feature.Radius, 0.01f, 1.0f);
	float Intensity = FMath::Clamp(Feature.Intensity, -1.0f, 1.0f);

	float Dist = FMath::Sqrt((NormX - FX) * (NormX - FX) + (NormY - FY) * (NormY - FY));
	if (Dist >= Radius)
	{
		return Height;
	}

	float T = Dist / Radius;
	float Weight = ComputeFalloff(T, Feature.Falloff);
	FString Type = Feature.Type.ToLower();

	// ── Feature-type-specific behavior ──

	if (Type == TEXT("mountain"))
	{
		// Gaussian-like bump. Intensity controls peak height.
		return Height + FMath::Abs(Intensity) * Weight;
	}
	if (Type == TEXT("valley"))
	{
		// Depression. Always subtracts.
		return Height - FMath::Abs(Intensity) * Weight;
	}
	if (Type == TEXT("ridge"))
	{
		// Elongated bump along a direction. Uses sharper falloff laterally.
		// Approximate by using weight^0.5 for a wider, flatter profile
		float RidgeWeight = FMath::Sqrt(Weight);
		return Height + FMath::Abs(Intensity) * RidgeWeight;
	}
	if (Type == TEXT("plateau"))
	{
		// Flat-top elevation. Weight is clamped to create a flat region in center.
		float PlateauWeight = (T < 0.5f) ? 1.0f : ComputeFalloff((T - 0.5f) * 2.0f, Feature.Falloff);
		return Height + FMath::Abs(Intensity) * PlateauWeight;
	}
	if (Type == TEXT("crater"))
	{
		// Ring shape: depression in center, rim at edge.
		float RimT = FMath::Abs(T - 0.7f) / 0.3f;
		float CraterWeight = (T < 0.7f)
			? -FMath::Abs(Intensity) * (1.0f - T / 0.7f)    // Depression inside
			: FMath::Abs(Intensity) * 0.3f * (1.0f - FMath::Clamp(RimT, 0.f, 1.f));  // Rim
		return Height + CraterWeight;
	}
	if (Type == TEXT("river_bed"))
	{
		// Narrow depression channel. Uses sharper/narrower falloff.
		float ChannelWeight = Weight * Weight;  // Squared for narrow channel
		return Height - FMath::Abs(Intensity) * ChannelWeight;
	}

	// Unknown type: use raw intensity (positive = up, negative = down)
	return Height + Intensity * Weight;
}

// ── Erosion ─────────────────────────────────────────────────────────

void FHktTerrainRecipeBuilder::ApplyErosion(TArray<float>& HeightmapF, int32 SizeX, int32 SizeY, int32 Passes)
{
	int32 ClampedPasses = FMath::Clamp(Passes, 0, MAX_EROSION_PASSES);
	if (ClampedPasses <= 0) return;

	const float Threshold = 0.02f;
	const float TransferRate = 0.3f;

	TArray<float> Temp;
	Temp.SetNumUninitialized(SizeX * SizeY);

	static const int32 DX[] = { -1, 1, 0, 0 };
	static const int32 DY[] = { 0, 0, -1, 1 };

	for (int32 Pass = 0; Pass < ClampedPasses; ++Pass)
	{
		FMemory::Memcpy(Temp.GetData(), HeightmapF.GetData(), SizeX * SizeY * sizeof(float));

		// Process interior pixels
		for (int32 Y = 1; Y < SizeY - 1; ++Y)
		{
			for (int32 X = 1; X < SizeX - 1; ++X)
			{
				float Center = Temp[Y * SizeX + X];
				float TotalDiff = 0.f;

				// Sum height differences to lower neighbors
				for (int32 D = 0; D < 4; ++D)
				{
					float Neighbor = Temp[(Y + DY[D]) * SizeX + (X + DX[D])];
					float Diff = Center - Neighbor;
					if (Diff > Threshold)
					{
						TotalDiff += Diff;
					}
				}

				if (TotalDiff <= 0.f) continue;

				// Transfer material proportionally to each lower neighbor (mass conserving)
				float TotalTransfer = TotalDiff * TransferRate;
				HeightmapF[Y * SizeX + X] -= TotalTransfer;

				for (int32 D = 0; D < 4; ++D)
				{
					float Neighbor = Temp[(Y + DY[D]) * SizeX + (X + DX[D])];
					float Diff = Center - Neighbor;
					if (Diff > Threshold)
					{
						// Each neighbor receives proportional share
						float Share = (Diff / TotalDiff) * TotalTransfer;
						HeightmapF[(Y + DY[D]) * SizeX + (X + DX[D])] += Share;
					}
				}
			}
		}

		// Boundary smoothing: blend edges with nearest interior
		for (int32 X = 0; X < SizeX; ++X)
		{
			// Top edge
			HeightmapF[X] = FMath::Lerp(HeightmapF[X], HeightmapF[SizeX + X], 0.5f);
			// Bottom edge
			int32 Bot = (SizeY - 1) * SizeX + X;
			HeightmapF[Bot] = FMath::Lerp(HeightmapF[Bot], HeightmapF[(SizeY - 2) * SizeX + X], 0.5f);
		}
		for (int32 Y = 1; Y < SizeY - 1; ++Y)
		{
			// Left edge
			HeightmapF[Y * SizeX] = FMath::Lerp(HeightmapF[Y * SizeX], HeightmapF[Y * SizeX + 1], 0.5f);
			// Right edge
			int32 Right = Y * SizeX + SizeX - 1;
			HeightmapF[Right] = FMath::Lerp(HeightmapF[Right], HeightmapF[Y * SizeX + SizeX - 2], 0.5f);
		}
	}
}

// ── Main Generation ─────────────────────────────────────────────────

TArray<uint16> FHktTerrainRecipeBuilder::GenerateHeightmap(
	const FHktTerrainRecipe& Recipe,
	int32 SizeX, int32 SizeY,
	float HeightMin, float HeightMax)
{
	// ── Input Validation ────────────────────────────────────────
	SizeX = FMath::Clamp(SizeX, 2, MAX_LANDSCAPE_SIZE);
	SizeY = FMath::Clamp(SizeY, 2, MAX_LANDSCAPE_SIZE);

	if (HeightMax <= HeightMin)
	{
		HeightMax = HeightMin + 1000.f;
	}

	int32 FeatureCount = FMath::Min(Recipe.Features.Num(), MAX_FEATURES);

	UE_LOG(LogHktTerrain, Log, TEXT("Generating heightmap %dx%d — noise=%s octaves=%d features=%d erosion=%d height=[%.0f,%.0f]"),
		SizeX, SizeY, *Recipe.BaseNoiseType,
		FMath::Clamp(Recipe.Octaves, 1, MAX_OCTAVES),
		FeatureCount,
		FMath::Clamp(Recipe.ErosionPasses, 0, MAX_EROSION_PASSES),
		HeightMin, HeightMax);

	// ── Phase 1: Generate float heightmap with noise + features ──
	TArray<float> HeightmapF;
	HeightmapF.SetNumUninitialized(SizeX * SizeY);

	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		float NormY = static_cast<float>(Y) / SizeY;
		for (int32 X = 0; X < SizeX; ++X)
		{
			float NormX = static_cast<float>(X) / SizeX;

			// Base noise — scale to meaningful coordinate space
			float H = FBM(NormX * 1000.f, NormY * 1000.f, Recipe);

			// Apply features (capped)
			for (int32 F = 0; F < FeatureCount; ++F)
			{
				H = ApplyFeature(H, NormX, NormY, Recipe.Features[F]);
			}

			HeightmapF[Y * SizeX + X] = H;
		}
	}

	// ── Phase 2: Erosion ──
	ApplyErosion(HeightmapF, SizeX, SizeY, Recipe.ErosionPasses);

	// ── Phase 3: Map to [HeightMin, HeightMax] then quantize to uint16 ──
	float MinVal = HeightmapF[0], MaxVal = HeightmapF[0];
	for (float V : HeightmapF)
	{
		MinVal = FMath::Min(MinVal, V);
		MaxVal = FMath::Max(MaxVal, V);
	}
	float Range = MaxVal - MinVal;
	if (Range < KINDA_SMALL_NUMBER) Range = 1.f;

	// HeightMin/HeightMax define the UE5 Z range.
	// UE5 Landscape: uint16 0 = HeightMin, 65535 = HeightMax.
	// We normalize the procedural values to fill this range.
	float HeightRange = HeightMax - HeightMin;

	TArray<uint16> Heightmap;
	Heightmap.SetNumUninitialized(SizeX * SizeY);

	for (int32 i = 0; i < SizeX * SizeY; ++i)
	{
		float Normalized = (HeightmapF[i] - MinVal) / Range; // [0, 1]
		float WorldHeight = HeightMin + Normalized * HeightRange;
		// Map world height to uint16 assuming UE5 landscape range maps linearly
		float T = (WorldHeight - HeightMin) / HeightRange; // [0, 1] again, but now semantically correct
		Heightmap[i] = static_cast<uint16>(FMath::Clamp(T * 65535.f, 0.f, 65535.f));
	}

	UE_LOG(LogHktTerrain, Log, TEXT("Heightmap generated — raw [%.4f, %.4f] → world [%.0f, %.0f]"),
		MinVal, MaxVal, HeightMin, HeightMax);
	return Heightmap;
}

TArray<TArray<uint8>> FHktTerrainRecipeBuilder::GenerateWeightMaps(
	const TArray<uint16>& Heightmap,
	int32 SizeX, int32 SizeY,
	int32 LayerCount)
{
	TArray<TArray<uint8>> WeightMaps;
	if (LayerCount <= 0) return WeightMaps;
	LayerCount = FMath::Min(LayerCount, 16); // Cap layers

	WeightMaps.SetNum(LayerCount);
	for (auto& WM : WeightMaps)
	{
		WM.SetNumZeroed(SizeX * SizeY);
	}

	// Layer distribution strategy:
	// - Height-based bands with overlapping transitions
	// - Steep slopes boost a "cliff/rock" layer (last layer before the highest)
	// - This gives natural transitions: low=grass, mid=dirt, high=snow, slopes=rock

	int32 CliffLayer = FMath::Max(0, LayerCount - 2); // Second-to-last layer is "cliff"

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

		// Compute height-based weights
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
		}

		// Steep slopes boost cliff layer
		if (Slope > 0.03f && LayerCount >= 2)
		{
			float SlopeInfluence = FMath::Clamp((Slope - 0.03f) * 10.f, 0.f, 1.f);
			Weights[CliffLayer] += SlopeInfluence * 2.0f;
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
