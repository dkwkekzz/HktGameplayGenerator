// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktMapGeneratorSubsystem.h"
#include "HktMapGeneratorSettings.h"
#include "HktMapJsonParser.h"
#include "HktTerrainRecipeBuilder.h"
#include "HktMapRegionVolume.h"
#include "HktSpawnerActor.h"
#include "HktAssetSubsystem.h"
#include "HktAnimGeneratorTypes.h"
#include "HktStoryGeneratorSubsystem.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/WindDirectionalSource.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/WindDirectionalSourceComponent.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeImportHelper.h"
#include "EngineUtils.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktMapGenerator, Log, All);

// ── Lifecycle ───────────────────────────────────────────────────────

void UHktMapGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogHktMapGenerator, Log, TEXT("HktMapGenerator Subsystem initialized"));
}

void UHktMapGeneratorSubsystem::Deinitialize()
{
	UnloadCurrentMap();
	Super::Deinitialize();
}

// ── JSON I/O (delegates to FHktMapJsonParser) ──────────────────────

bool UHktMapGeneratorSubsystem::ParseMapFromJson(const FString& JsonStr, FHktMapData& OutMapData)
{
	return FHktMapJsonParser::Parse(JsonStr, OutMapData);
}

FString UHktMapGeneratorSubsystem::SerializeMapToJson(const FHktMapData& MapData)
{
	return FHktMapJsonParser::Serialize(MapData);
}

// ── GlobalEntityType string conversion (used by BuildGlobalEntities log) ──

namespace
{
	FString GlobalEntityTypeToString(EHktGlobalEntityType Type)
	{
		switch (Type)
		{
		case EHktGlobalEntityType::WorldBoss: return TEXT("world_boss");
		case EHktGlobalEntityType::NPCSpawner: return TEXT("npc_spawner");
		default: return TEXT("npc");
		}
	}
} // anonymous namespace

// ── Build ───────────────────────────────────────────────────────────

UHktMapStoryRegistry* UHktMapGeneratorSubsystem::GetOrCreateStoryRegistry()
{
	if (!StoryRegistry)
	{
		StoryRegistry = NewObject<UHktMapStoryRegistry>(this);
	}
	return StoryRegistry;
}

bool UHktMapGeneratorSubsystem::BuildMap(const FHktMapData& MapData)
{
	UE_LOG(LogHktMapGenerator, Log, TEXT("BuildMap: '%s' — %d regions, %d global entities"),
		*MapData.MapId, MapData.Regions.Num(), MapData.GlobalEntities.Num());

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		UE_LOG(LogHktMapGenerator, Error, TEXT("BuildMap: No editor world available"));
		return false;
	}

	// Clear previous map
	UnloadCurrentMap();

	// 1. Per-Region: Landscape + Volume + Spawners + Stories + Props
	for (const auto& Region : MapData.Regions)
	{
		// Create Landscape for this region
		BuildRegionLandscape(Region, World);

		// Create Region Volume (for streaming detection)
		AHktMapRegionVolume* Volume = BuildRegionVolume(Region, World);
		if (Volume)
		{
			SpawnedActors.Add(Volume);
		}

		// Spawners
		BuildSpawners(Region.Spawners, World);

		// Props
		BuildProps(Region.Props, World);

		// Register region stories
		GetOrCreateStoryRegistry()->OnRegionActivated(Region.Name, Region.Stories);
	}

	// 2. Global Entities
	BuildGlobalEntities(MapData.GlobalEntities, World);

	// 3. Environment
	ApplyEnvironment(MapData.Environment, World);

	// 4. Global Stories
	GetOrCreateStoryRegistry()->RegisterGlobalStories(MapData.GlobalStories);

	CurrentMapId = MapData.MapId;
	UE_LOG(LogHktMapGenerator, Log, TEXT("BuildMap complete: '%s'"), *MapData.MapId);
	return true;
}

bool UHktMapGeneratorSubsystem::BuildRegionLandscape(const FHktMapRegion& Region, UWorld* World)
{
	const auto& LandscapeConfig = Region.Landscape;

	// ── 1. Generate or load heightmap data ──────────────────────
	TArray<uint16> HeightData;

	if (!LandscapeConfig.HeightmapPath.IsEmpty())
	{
		TArray<uint8> RawData;
		if (FFileHelper::LoadFileToArray(RawData, *LandscapeConfig.HeightmapPath))
		{
			HeightData.SetNumUninitialized(RawData.Num() / 2);
			FMemory::Memcpy(HeightData.GetData(), RawData.GetData(), RawData.Num());
			UE_LOG(LogHktMapGenerator, Log, TEXT("Region '%s': Loaded heightmap from %s (%d samples)"),
				*Region.Name, *LandscapeConfig.HeightmapPath, HeightData.Num());
		}
		else
		{
			UE_LOG(LogHktMapGenerator, Warning, TEXT("Region '%s': Failed to load heightmap '%s', using recipe"),
				*Region.Name, *LandscapeConfig.HeightmapPath);
		}
	}

	if (HeightData.Num() == 0)
	{
		HeightData = FHktTerrainRecipeBuilder::GenerateHeightmap(
			LandscapeConfig.TerrainRecipe,
			LandscapeConfig.SizeX, LandscapeConfig.SizeY,
			LandscapeConfig.HeightMin, LandscapeConfig.HeightMax);
	}

	if (HeightData.Num() != LandscapeConfig.SizeX * LandscapeConfig.SizeY)
	{
		UE_LOG(LogHktMapGenerator, Error, TEXT("Region '%s': Heightmap size mismatch (%d vs %dx%d)"),
			*Region.Name, HeightData.Num(), LandscapeConfig.SizeX, LandscapeConfig.SizeY);
		return false;
	}

	// ── 2. Determine component geometry ─────────────────────────
	// UE5 Landscape valid component sizes: 7, 15, 31, 63, 127, 255
	// Valid section sizes: 7, 15, 31, 63, 127
	// Total size = NumComponents * SectionsPerComponent * QuadsPerSection + 1
	const int32 QuadsPerSection = 63;
	const int32 SectionsPerComponent = 1;
	const int32 QuadsPerComponent = QuadsPerSection * SectionsPerComponent;

	const int32 NumComponentsX = FMath::Max(1, (LandscapeConfig.SizeX - 1) / QuadsPerComponent);
	const int32 NumComponentsY = FMath::Max(1, (LandscapeConfig.SizeY - 1) / QuadsPerComponent);

	// ── 3. Prepare landscape material ───────────────────────────
	const UHktMapGeneratorSettings* Settings = UHktMapGeneratorSettings::Get();
	UMaterialInterface* LandscapeMaterial = nullptr;

	if (LandscapeConfig.MaterialTag.IsValid())
	{
		FSoftObjectPath MatPath = UHktAssetSubsystem::ResolveConventionPath(LandscapeConfig.MaterialTag);
		if (MatPath.IsValid())
		{
			LandscapeMaterial = Cast<UMaterialInterface>(MatPath.TryLoad());
		}
	}

	if (!LandscapeMaterial && Settings->DefaultLandscapeMaterial.IsValid())
	{
		LandscapeMaterial = Cast<UMaterialInterface>(Settings->DefaultLandscapeMaterial.TryLoad());
	}

	// ── 4. Prepare layer infos ──────────────────────────────────
	TArray<FLandscapeImportLayerInfo> ImportLayers;

	// Generate weight maps from heightmap for automatic layer distribution
	const int32 LayerCount = FMath::Max(LandscapeConfig.Layers.Num(), 1);
	TArray<TArray<uint8>> WeightMaps = FHktTerrainRecipeBuilder::GenerateWeightMaps(
		HeightData, LandscapeConfig.SizeX, LandscapeConfig.SizeY, LayerCount);

	for (int32 i = 0; i < LandscapeConfig.Layers.Num(); ++i)
	{
		const auto& LayerDef = LandscapeConfig.Layers[i];
		FLandscapeImportLayerInfo LayerInfo;
		LayerInfo.LayerName = FName(*LayerDef.Name);

		// Use weight map from file if provided, otherwise use generated
		if (!LayerDef.WeightMapPath.IsEmpty())
		{
			TArray<uint8> FileWeightData;
			if (FFileHelper::LoadFileToArray(FileWeightData, *LayerDef.WeightMapPath)
				&& FileWeightData.Num() == LandscapeConfig.SizeX * LandscapeConfig.SizeY)
			{
				LayerInfo.LayerData = FileWeightData;
			}
			else if (i < WeightMaps.Num())
			{
				LayerInfo.LayerData = WeightMaps[i];
			}
		}
		else if (i < WeightMaps.Num())
		{
			LayerInfo.LayerData = WeightMaps[i];
		}

		ImportLayers.Add(LayerInfo);
	}

	// ── 5. Spawn ALandscape actor ───────────────────────────────
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = FName(*FString::Printf(TEXT("HktLandscape_%s"), *Region.Name));

	// Offset so that Region.Center is the center of the landscape
	float ScaleXY = Region.Extent.X * 2.f / FMath::Max(LandscapeConfig.SizeX - 1, 1);
	float ScaleZ = (LandscapeConfig.HeightMax - LandscapeConfig.HeightMin) / 512.f; // UE5 Landscape: 512 UU per unit at scale 1
	FVector LandscapeScale(ScaleXY, ScaleXY, FMath::Max(ScaleZ, 0.01f));

	// Position at the corner so that center of landscape matches Region.Center
	FVector LandscapeOrigin = Region.Center - FVector(Region.Extent.X, Region.Extent.Y, 0.f);
	LandscapeOrigin.Z = LandscapeConfig.HeightMin;

	ALandscape* NewLandscape = World->SpawnActor<ALandscape>(
		ALandscape::StaticClass(), &LandscapeOrigin, nullptr, SpawnParams);

	if (!NewLandscape)
	{
		UE_LOG(LogHktMapGenerator, Error, TEXT("Region '%s': Failed to spawn ALandscape actor"), *Region.Name);
		return false;
	}

	NewLandscape->SetActorScale3D(LandscapeScale);

	// ── 6. Import heightmap data into landscape ─────────────────
	TMap<FGuid, TArray<uint16>> HeightDataPerLayer;
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayer;

	FGuid LandscapeGuid = FGuid::NewGuid();
	HeightDataPerLayer.Add(LandscapeGuid, HeightData);
	MaterialLayerDataPerLayer.Add(LandscapeGuid, ImportLayers);

	NewLandscape->Import(
		LandscapeGuid,
		NumComponentsX, NumComponentsY,
		QuadsPerSection, SectionsPerComponent,
		HeightDataPerLayer, TEXT(""),
		MaterialLayerDataPerLayer,
		ELandscapeImportAlphamapType::Additive);

	// ── 7. Apply landscape material ─────────────────────────────
	if (LandscapeMaterial)
	{
		NewLandscape->LandscapeMaterial = LandscapeMaterial;
	}

	NewLandscape->SetFolderPath(FName(TEXT("HktMap/Landscapes")));
	NewLandscape->MarkPackageDirty();
	SpawnedActors.Add(NewLandscape);

	UE_LOG(LogHktMapGenerator, Log, TEXT("Region '%s': ALandscape created — %dx%d, %d components (%dx%d), biome=%s, %d layers, scale=(%.2f, %.2f, %.2f)"),
		*Region.Name, LandscapeConfig.SizeX, LandscapeConfig.SizeY,
		NumComponentsX * NumComponentsY, NumComponentsX, NumComponentsY,
		*LandscapeConfig.Biome, ImportLayers.Num(),
		LandscapeScale.X, LandscapeScale.Y, LandscapeScale.Z);

	return true;
}

AHktMapRegionVolume* UHktMapGeneratorSubsystem::BuildRegionVolume(const FHktMapRegion& Region, UWorld* World)
{
	FActorSpawnParameters Params;
	Params.Name = FName(*FString::Printf(TEXT("HktRegion_%s"), *Region.Name));

	AHktMapRegionVolume* Volume = World->SpawnActor<AHktMapRegionVolume>(
		AHktMapRegionVolume::StaticClass(), &Region.Center, nullptr, Params);

	if (Volume)
	{
		Volume->InitFromRegionData(Region.Name, Region.Center, Region.Extent, Region.Properties);
		Volume->SetFolderPath(FName(TEXT("HktMap/Regions")));
		UE_LOG(LogHktMapGenerator, Log, TEXT("Region volume '%s' created at (%.0f, %.0f, %.0f)"),
			*Region.Name, Region.Center.X, Region.Center.Y, Region.Center.Z);
	}
	return Volume;
}

void UHktMapGeneratorSubsystem::BuildSpawners(const TArray<FHktMapSpawner>& Spawners, UWorld* World)
{
	for (const auto& SpawnerData : Spawners)
	{
		AHktSpawnerActor* Spawner = World->SpawnActor<AHktSpawnerActor>(
			AHktSpawnerActor::StaticClass());

		if (Spawner)
		{
			Spawner->InitFromSpawnerData(SpawnerData);
			Spawner->SetFolderPath(FName(TEXT("HktMap/Spawners")));
			SpawnedActors.Add(Spawner);
		}
	}
}

void UHktMapGeneratorSubsystem::BuildProps(const TArray<FHktMapProp>& Props, UWorld* World)
{
	for (const auto& Prop : Props)
	{
		FTransform Transform;
		Transform.SetLocation(Prop.Position);
		Transform.SetRotation(FQuat(Prop.Rotation));
		Transform.SetScale3D(Prop.Scale);

		AStaticMeshActor* PropActor = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), &Transform);

		if (!PropActor) continue;

		// Resolve MeshTag → StaticMesh via ConventionPath
		if (Prop.MeshTag.IsValid())
		{
			FSoftObjectPath MeshPath = UHktAssetSubsystem::ResolveConventionPath(Prop.MeshTag);
			UStaticMesh* Mesh = nullptr;

			if (MeshPath.IsValid())
			{
				Mesh = Cast<UStaticMesh>(MeshPath.TryLoad());
			}

			if (!Mesh)
			{
				// Fallback: try standard naming convention
				// Tag format: Entity.Item.{Cat}.{Sub} → {Root}/Items/{Cat}/SM_{Sub}
				FString TagStr = Prop.MeshTag.ToString();
				FString FallbackPath = FString::Printf(TEXT("/Game/Generated/Props/SM_%s"),
					*TagStr.Replace(TEXT("."), TEXT("_")));
				Mesh = Cast<UStaticMesh>(FSoftObjectPath(FallbackPath).TryLoad());
			}

			if (Mesh)
			{
				PropActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
				UE_LOG(LogHktMapGenerator, Log, TEXT("Prop '%s': Mesh resolved and applied"),
					*Prop.MeshTag.ToString());
			}
			else
			{
				UE_LOG(LogHktMapGenerator, Warning, TEXT("Prop '%s': Could not resolve mesh, placed as placeholder"),
					*Prop.MeshTag.ToString());
			}
		}

		PropActor->SetFolderPath(FName(TEXT("HktMap/Props")));
		SpawnedActors.Add(PropActor);
	}
}

void UHktMapGeneratorSubsystem::BuildGlobalEntities(const TArray<FHktMapGlobalEntity>& Entities, UWorld* World)
{
	for (const auto& Entity : Entities)
	{
		// GlobalEntities are spawned as AHktSpawnerActor with appropriate rules
		FHktMapSpawner SpawnerData;
		SpawnerData.EntityTag = Entity.EntityTag;
		SpawnerData.Position = Entity.Position;
		SpawnerData.Rotation = Entity.Rotation;
		SpawnerData.Count = Entity.Count;

		switch (Entity.EntityType)
		{
		case EHktGlobalEntityType::WorldBoss:
			SpawnerData.SpawnRule = EHktSpawnRule::OnTrigger;
			break;
		case EHktGlobalEntityType::NPC:
			SpawnerData.SpawnRule = EHktSpawnRule::Always;
			SpawnerData.Count = 1;
			break;
		case EHktGlobalEntityType::NPCSpawner:
			SpawnerData.SpawnRule = EHktSpawnRule::Always;
			break;
		}

		AHktSpawnerActor* Spawner = World->SpawnActor<AHktSpawnerActor>(
			AHktSpawnerActor::StaticClass());

		if (Spawner)
		{
			Spawner->InitFromSpawnerData(SpawnerData);
			Spawner->SetFolderPath(FName(TEXT("HktMap/GlobalEntities")));
			SpawnedActors.Add(Spawner);
		}

		UE_LOG(LogHktMapGenerator, Log, TEXT("Global entity '%s' (%s) placed at (%.0f, %.0f, %.0f)"),
			*Entity.EntityTag.ToString(),
			*GlobalEntityTypeToString(Entity.EntityType),
			Entity.Position.X, Entity.Position.Y, Entity.Position.Z);
	}
}

void UHktMapGeneratorSubsystem::ApplyEnvironment(const FHktMapEnvironment& Env, UWorld* World)
{
	UE_LOG(LogHktMapGenerator, Log, TEXT("Applying environment — weather=%s, time=%s, fog=%.3f, wind=%.2f"),
		*Env.Weather, *Env.TimeOfDay, Env.FogDensity, Env.WindStrength);

	// ── 1. Sun / Directional Light ──────────────────────────────
	// Compute sun rotation from time of day
	float SunPitch = -45.f; // default noon
	float SunIntensity = 10.f;
	if (Env.TimeOfDay == TEXT("dawn"))        { SunPitch = -10.f; SunIntensity = 3.f; }
	else if (Env.TimeOfDay == TEXT("morning")) { SunPitch = -30.f; SunIntensity = 7.f; }
	else if (Env.TimeOfDay == TEXT("noon"))    { SunPitch = -70.f; SunIntensity = 10.f; }
	else if (Env.TimeOfDay == TEXT("afternoon")) { SunPitch = -50.f; SunIntensity = 8.f; }
	else if (Env.TimeOfDay == TEXT("dusk"))    { SunPitch = -15.f; SunIntensity = 3.f; }
	else if (Env.TimeOfDay == TEXT("night"))   { SunPitch = 10.f; SunIntensity = 0.1f; }

	ADirectionalLight* SunLight = nullptr;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		SunLight = *It;
		break;
	}

	if (!SunLight)
	{
		SunLight = World->SpawnActor<ADirectionalLight>();
		if (SunLight)
		{
			SunLight->SetFolderPath(FName(TEXT("HktMap/Environment")));
			SpawnedActors.Add(SunLight);
		}
	}

	if (SunLight)
	{
		SunLight->SetActorRotation(FRotator(SunPitch, -45.f, 0.f));
		if (UDirectionalLightComponent* LightComp = SunLight->GetComponent())
		{
			LightComp->SetLightColor(Env.SunColor);
			LightComp->SetIntensity(SunIntensity);
		}
	}

	// ── 2. Exponential Height Fog ───────────────────────────────
	AExponentialHeightFog* FogActor = nullptr;
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		FogActor = *It;
		break;
	}

	if (!FogActor)
	{
		FogActor = World->SpawnActor<AExponentialHeightFog>();
		if (FogActor)
		{
			FogActor->SetFolderPath(FName(TEXT("HktMap/Environment")));
			SpawnedActors.Add(FogActor);
		}
	}

	if (FogActor)
	{
		UExponentialHeightFogComponent* FogComp = FogActor->GetComponent();
		if (FogComp)
		{
			// Adjust fog based on weather preset
			float EffectiveDensity = Env.FogDensity;
			if (Env.Weather == TEXT("fog"))         EffectiveDensity = FMath::Max(EffectiveDensity, 0.15f);
			else if (Env.Weather == TEXT("rain"))   EffectiveDensity = FMath::Max(EffectiveDensity, 0.05f);
			else if (Env.Weather == TEXT("snow"))   EffectiveDensity = FMath::Max(EffectiveDensity, 0.08f);
			else if (Env.Weather == TEXT("storm"))  EffectiveDensity = FMath::Max(EffectiveDensity, 0.12f);

			FogComp->SetFogDensity(EffectiveDensity);
			FogComp->SetFogInscatteringColor(Env.AmbientColor);
		}
	}

	// ── 3. Wind Directional Source ──────────────────────────────
	AWindDirectionalSource* WindActor = nullptr;
	for (TActorIterator<AWindDirectionalSource> It(World); It; ++It)
	{
		WindActor = *It;
		break;
	}

	if (!WindActor && Env.WindStrength > KINDA_SMALL_NUMBER)
	{
		WindActor = World->SpawnActor<AWindDirectionalSource>();
		if (WindActor)
		{
			WindActor->SetFolderPath(FName(TEXT("HktMap/Environment")));
			SpawnedActors.Add(WindActor);
		}
	}

	if (WindActor)
	{
		// Orient wind actor to match wind direction
		FRotator WindRot = Env.WindDirection.GetSafeNormal().Rotation();
		WindActor->SetActorRotation(WindRot);

		if (UWindDirectionalSourceComponent* WindComp = WindActor->GetComponent())
		{
			// Weather modifiers for wind
			float EffectiveStrength = Env.WindStrength;
			if (Env.Weather == TEXT("storm"))     EffectiveStrength = FMath::Max(EffectiveStrength, 0.8f);
			else if (Env.Weather == TEXT("rain")) EffectiveStrength = FMath::Max(EffectiveStrength, 0.4f);

			WindComp->SetStrength(EffectiveStrength);
			WindComp->SetSpeed(EffectiveStrength * 200.f);
		}
	}

	UE_LOG(LogHktMapGenerator, Log, TEXT("Environment applied — sun pitch=%.1f intensity=%.1f, fog=%.3f, wind=%.2f"),
		SunPitch, SunIntensity,
		FogActor && FogActor->GetComponent() ? FogActor->GetComponent()->FogDensity : 0.f,
		Env.WindStrength);
}

bool UHktMapGeneratorSubsystem::BuildMapFromJson(const FString& JsonStr)
{
	FHktMapData MapData;
	if (!ParseMapFromJson(JsonStr, MapData))
	{
		return false;
	}
	return BuildMap(MapData);
}

// ── Load / Unload ───────────────────────────────────────────────────

bool UHktMapGeneratorSubsystem::LoadMapFromFile(const FString& FilePath, FHktMapData& OutMapData)
{
	FString JsonStr;
	if (!FFileHelper::LoadFileToString(JsonStr, *FilePath))
	{
		UE_LOG(LogHktMapGenerator, Error, TEXT("Failed to load map file: %s"), *FilePath);
		return false;
	}
	return ParseMapFromJson(JsonStr, OutMapData);
}

bool UHktMapGeneratorSubsystem::SaveMapToFile(const FHktMapData& MapData, const FString& FilePath)
{
	FString JsonStr = SerializeMapToJson(MapData);
	if (!FFileHelper::SaveStringToFile(JsonStr, *FilePath))
	{
		UE_LOG(LogHktMapGenerator, Error, TEXT("Failed to save map file: %s"), *FilePath);
		return false;
	}
	UE_LOG(LogHktMapGenerator, Log, TEXT("Saved HktMap to: %s"), *FilePath);
	return true;
}

void UHktMapGeneratorSubsystem::UnloadCurrentMap()
{
	for (auto& WeakActor : SpawnedActors)
	{
		if (AActor* Actor = WeakActor.Get())
		{
			Actor->Destroy();
		}
	}
	SpawnedActors.Empty();
	CurrentMapId.Empty();
	UE_LOG(LogHktMapGenerator, Log, TEXT("Unloaded current map"));
}

TArray<FString> UHktMapGeneratorSubsystem::ListSavedMaps()
{
	TArray<FString> Result;
	const UHktMapGeneratorSettings* Settings = UHktMapGeneratorSettings::Get();
	FString MapDir = FPaths::ProjectDir() / Settings->MapOutputDirectory;

	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, *(MapDir / TEXT("*.json")), true, false);
	for (auto& File : FoundFiles)
	{
		Result.Add(FPaths::GetBaseFilename(File));
	}
	return Result;
}

// ── MCP Endpoints ───────────────────────────────────────────────────

FString UHktMapGeneratorSubsystem::McpBuildMap(const FString& JsonStr)
{
	FHktMapData MapData;
	if (!ParseMapFromJson(JsonStr, MapData))
	{
		return TEXT("{\"success\":false,\"error\":\"Failed to parse map JSON\"}");
	}

	bool bSuccess = BuildMap(MapData);
	return FString::Printf(TEXT("{\"success\":%s,\"map_id\":\"%s\",\"region_count\":%d}"),
		bSuccess ? TEXT("true") : TEXT("false"), *MapData.MapId, MapData.Regions.Num());
}

FString UHktMapGeneratorSubsystem::McpValidateMap(const FString& JsonStr)
{
	FHktMapData MapData;
	bool bValid = ParseMapFromJson(JsonStr, MapData);
	if (!bValid)
	{
		return TEXT("{\"valid\":false,\"error\":\"Failed to parse JSON\"}");
	}

	TArray<FString> Errors;
	if (MapData.MapId.IsEmpty()) Errors.Add(TEXT("Missing map_id"));
	if (MapData.MapName.IsEmpty()) Errors.Add(TEXT("Missing map_name"));
	if (MapData.Regions.Num() == 0) Errors.Add(TEXT("No regions defined"));

	for (int32 i = 0; i < MapData.Regions.Num(); ++i)
	{
		auto& R = MapData.Regions[i];
		if (R.Name.IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("regions[%d]: missing name"), i));
		}
	}

	int32 TotalSpawners = 0, TotalStories = 0, TotalProps = 0;
	for (auto& R : MapData.Regions)
	{
		TotalSpawners += R.Spawners.Num();
		TotalStories += R.Stories.Num();
		TotalProps += R.Props.Num();
	}
	TotalStories += MapData.GlobalStories.Num();

	return FString::Printf(
		TEXT("{\"valid\":%s,\"error_count\":%d,\"region_count\":%d,\"spawner_count\":%d,\"story_count\":%d,\"prop_count\":%d,\"global_entity_count\":%d}"),
		Errors.Num() == 0 ? TEXT("true") : TEXT("false"),
		Errors.Num(), MapData.Regions.Num(), TotalSpawners, TotalStories, TotalProps, MapData.GlobalEntities.Num());
}

FString UHktMapGeneratorSubsystem::McpGetMapSchema()
{
	return TEXT(R"({
		"description": "HktMap JSON schema — Region-based dynamic map with per-region Landscape",
		"required": ["map_id", "map_name", "regions"],
		"sections": {
			"regions": "Array of regions, each with own landscape, spawners, stories, props",
			"regions[].landscape": "Per-region terrain (size, heightmap or terrain_recipe, biome, layers)",
			"regions[].landscape.terrain_recipe": "Procedural heightmap params (noise type, octaves, features)",
			"regions[].spawners": "Entity spawn points within this region",
			"regions[].stories": "Stories loaded when this region activates",
			"regions[].props": "Static prop placements within this region",
			"global_entities": "WorldBoss/NPC/NPCSpawner — always active regardless of region",
			"environment": "Weather, time of day, fog, wind, lighting, ambient VFX",
			"global_stories": "Stories loaded when map loads (not region-specific)"
		}
	})");
}
