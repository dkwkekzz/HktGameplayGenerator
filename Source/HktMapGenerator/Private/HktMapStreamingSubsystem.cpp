// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktMapStreamingSubsystem.h"
#include "HktTerrainRecipeBuilder.h"
#include "HktMapRegionVolume.h"
#include "HktSpawnerActor.h"
#include "HktAssetSubsystem.h"

#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "TimerManager.h"
#include "Landscape.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktStreaming, Log, All);

// ── Lifecycle ───────────────────────────────────────────────────────

void UHktMapStreamingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StoryRegistry = NewObject<UHktMapStoryRegistry>(this);
	UE_LOG(LogHktStreaming, Log, TEXT("HktMapStreaming Subsystem initialized"));
}

void UHktMapStreamingSubsystem::Deinitialize()
{
	UnloadMap();
	Super::Deinitialize();
}

// ── Map Load / Unload ───────────────────────────────────────────────

bool UHktMapStreamingSubsystem::LoadMap(const FString& MapFilePath)
{
	FString JsonStr;
	if (!FFileHelper::LoadFileToString(JsonStr, *MapFilePath))
	{
		UE_LOG(LogHktStreaming, Error, TEXT("Failed to load map file: %s"), *MapFilePath);
		return false;
	}

	FHktMapData MapData;
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogHktStreaming, Error, TEXT("Failed to parse map JSON from %s"), *MapFilePath);
		return false;
	}

	// TODO: Use shared parsing logic (currently in editor subsystem)
	// For now, store the raw data path and expect LoadMapFromData to be called
	UE_LOG(LogHktStreaming, Log, TEXT("Map file loaded, use LoadMapFromData for full initialization"));
	return false;
}

bool UHktMapStreamingSubsystem::LoadMapFromData(const FHktMapData& MapData)
{
	UnloadMap();
	CurrentMapData = MapData;

	// Initialize region states (all inactive)
	for (const auto& Region : MapData.Regions)
	{
		RegionActiveState.Add(Region.Name, false);
	}

	// Spawn global content (always active)
	SpawnGlobalContent(MapData);

	// Register global stories
	StoryRegistry->RegisterGlobalStories(MapData.GlobalStories);

	// Start auto-streaming timer
	if (bAutoStreamingEnabled)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			World->GetTimerManager().SetTimer(
				StreamingTimerHandle,
				this, &UHktMapStreamingSubsystem::OnStreamingUpdate,
				StreamingUpdateInterval, true);
		}
	}

	UE_LOG(LogHktStreaming, Log, TEXT("Map '%s' loaded — %d regions (all inactive), auto-streaming=%s"),
		*MapData.MapId, MapData.Regions.Num(), bAutoStreamingEnabled ? TEXT("ON") : TEXT("OFF"));
	return true;
}

void UHktMapStreamingSubsystem::UnloadMap()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(StreamingTimerHandle);
	}

	// Deactivate all regions
	for (auto& Pair : RegionActiveState)
	{
		if (Pair.Value)
		{
			DestroyRegionContent(Pair.Key);
		}
	}
	RegionActiveState.Empty();
	RegionActorMap.Empty();

	// Destroy global actors
	for (auto& WeakActor : GlobalActors)
	{
		if (AActor* Actor = WeakActor.Get())
		{
			Actor->Destroy();
		}
	}
	GlobalActors.Empty();

	// Clear stories
	if (StoryRegistry)
	{
		StoryRegistry->Clear();
	}

	CurrentMapData = FHktMapData();
	UE_LOG(LogHktStreaming, Log, TEXT("Map unloaded"));
}

// ── Region Control ──────────────────────────────────────────────────

void UHktMapStreamingSubsystem::ActivateRegion(const FString& RegionName)
{
	if (bool* Active = RegionActiveState.Find(RegionName))
	{
		if (*Active) return;  // Already active
		*Active = true;
	}
	else
	{
		return;  // Unknown region
	}

	// Find region data
	const FHktMapRegion* RegionData = nullptr;
	for (const auto& R : CurrentMapData.Regions)
	{
		if (R.Name == RegionName)
		{
			RegionData = &R;
			break;
		}
	}
	if (!RegionData) return;

	// Spawn content for this region
	SpawnRegionContent(*RegionData);

	// Register region stories
	StoryRegistry->OnRegionActivated(RegionName, RegionData->Stories);

	// Activate spawners
	if (FRegionActors* RA = RegionActorMap.Find(RegionName))
	{
		for (auto& WeakSpawner : RA->Spawners)
		{
			if (AHktSpawnerActor* Spawner = WeakSpawner.Get())
			{
				Spawner->Activate();
			}
		}
	}

	UE_LOG(LogHktStreaming, Log, TEXT("Region '%s' activated"), *RegionName);
}

void UHktMapStreamingSubsystem::DeactivateRegion(const FString& RegionName)
{
	if (bool* Active = RegionActiveState.Find(RegionName))
	{
		if (!*Active) return;  // Already inactive
		*Active = false;
	}
	else
	{
		return;
	}

	// Deactivate spawners first
	if (FRegionActors* RA = RegionActorMap.Find(RegionName))
	{
		for (auto& WeakSpawner : RA->Spawners)
		{
			if (AHktSpawnerActor* Spawner = WeakSpawner.Get())
			{
				Spawner->Deactivate();
			}
		}
	}

	// Unregister region stories
	StoryRegistry->OnRegionDeactivated(RegionName);

	// Destroy region content
	DestroyRegionContent(RegionName);

	UE_LOG(LogHktStreaming, Log, TEXT("Region '%s' deactivated"), *RegionName);
}

bool UHktMapStreamingSubsystem::IsRegionActive(const FString& RegionName) const
{
	const bool* Active = RegionActiveState.Find(RegionName);
	return Active && *Active;
}

// ── Auto-Streaming ──────────────────────────────────────────────────

FVector UHktMapStreamingSubsystem::GetPlayerLocation() const
{
	UWorld* World = GetWorld();
	if (!World) return FVector::ZeroVector;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return FVector::ZeroVector;

	APawn* Pawn = PC->GetPawn();
	return Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
}

void UHktMapStreamingSubsystem::OnStreamingUpdate()
{
	FVector PlayerLoc = GetPlayerLocation();
	if (PlayerLoc.IsNearlyZero()) return;  // No valid player location

	for (const auto& Region : CurrentMapData.Regions)
	{
		// Check if player is within region bounds (AABB test)
		FVector Delta = PlayerLoc - Region.Center;
		bool bInRegion =
			FMath::Abs(Delta.X) <= Region.Extent.X &&
			FMath::Abs(Delta.Y) <= Region.Extent.Y &&
			FMath::Abs(Delta.Z) <= Region.Extent.Z;

		bool bWasActive = IsRegionActive(Region.Name);

		if (bInRegion && !bWasActive)
		{
			ActivateRegion(Region.Name);
		}
		else if (!bInRegion && bWasActive)
		{
			DeactivateRegion(Region.Name);
		}
	}
}

// ── Content Spawning ────────────────────────────────────────────────

void UHktMapStreamingSubsystem::SpawnRegionContent(const FHktMapRegion& Region)
{
	UWorld* World = GetWorld();
	if (!World) return;

	FRegionActors& RA = RegionActorMap.FindOrAdd(Region.Name);

	// ── 1. Region Volume (for visualization/overlap) ────────────
	AHktMapRegionVolume* Volume = World->SpawnActor<AHktMapRegionVolume>();
	if (Volume)
	{
		Volume->InitFromRegionData(Region.Name, Region.Center, Region.Extent, Region.Properties);
		RA.Volume = Volume;
		RA.AllActors.Add(Volume);
	}

	// ── 2. Landscape generation ─────────────────────────────────
	const auto& LandscapeConfig = Region.Landscape;
	TArray<uint16> HeightData;

	if (!LandscapeConfig.HeightmapPath.IsEmpty())
	{
		TArray<uint8> RawData;
		if (FFileHelper::LoadFileToArray(RawData, *LandscapeConfig.HeightmapPath))
		{
			HeightData.SetNumUninitialized(RawData.Num() / 2);
			FMemory::Memcpy(HeightData.GetData(), RawData.GetData(), RawData.Num());
		}
	}

	if (HeightData.Num() == 0)
	{
		HeightData = FHktTerrainRecipeBuilder::GenerateHeightmap(
			LandscapeConfig.TerrainRecipe,
			LandscapeConfig.SizeX, LandscapeConfig.SizeY,
			LandscapeConfig.HeightMin, LandscapeConfig.HeightMax);
	}

	if (HeightData.Num() == LandscapeConfig.SizeX * LandscapeConfig.SizeY)
	{
		// Compute landscape placement and scale
		const int32 QuadsPerSection = 63;
		const int32 SectionsPerComponent = 1;
		const int32 QuadsPerComponent = QuadsPerSection * SectionsPerComponent;
		const int32 NumComponentsX = FMath::Max(1, (LandscapeConfig.SizeX - 1) / QuadsPerComponent);
		const int32 NumComponentsY = FMath::Max(1, (LandscapeConfig.SizeY - 1) / QuadsPerComponent);

		float ScaleXY = Region.Extent.X * 2.f / FMath::Max(LandscapeConfig.SizeX - 1, 1);
		float ScaleZ = (LandscapeConfig.HeightMax - LandscapeConfig.HeightMin) / 512.f;
		FVector LandscapeScale(ScaleXY, ScaleXY, FMath::Max(ScaleZ, 0.01f));
		FVector LandscapeOrigin = Region.Center - FVector(Region.Extent.X, Region.Extent.Y, 0.f);
		LandscapeOrigin.Z = LandscapeConfig.HeightMin;

		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = FName(*FString::Printf(TEXT("HktLandscape_%s"), *Region.Name));
		ALandscape* NewLandscape = World->SpawnActor<ALandscape>(
			ALandscape::StaticClass(), &LandscapeOrigin, nullptr, SpawnParams);

		if (NewLandscape)
		{
			NewLandscape->SetActorScale3D(LandscapeScale);

			// Generate weight maps
			const int32 LayerCount = FMath::Max(LandscapeConfig.Layers.Num(), 1);
			TArray<TArray<uint8>> WeightMaps = FHktTerrainRecipeBuilder::GenerateWeightMaps(
				HeightData, LandscapeConfig.SizeX, LandscapeConfig.SizeY, LayerCount);

			TArray<FLandscapeImportLayerInfo> ImportLayers;
			for (int32 i = 0; i < LandscapeConfig.Layers.Num(); ++i)
			{
				FLandscapeImportLayerInfo LayerInfo;
				LayerInfo.LayerName = FName(*LandscapeConfig.Layers[i].Name);
				if (i < WeightMaps.Num())
				{
					LayerInfo.LayerData = WeightMaps[i];
				}
				ImportLayers.Add(LayerInfo);
			}

			FGuid LandscapeGuid = FGuid::NewGuid();
			TMap<FGuid, TArray<uint16>> HeightDataPerLayer;
			TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayer;
			HeightDataPerLayer.Add(LandscapeGuid, HeightData);
			MaterialLayerDataPerLayer.Add(LandscapeGuid, ImportLayers);

			NewLandscape->Import(
				LandscapeGuid,
				NumComponentsX, NumComponentsY,
				QuadsPerSection, SectionsPerComponent,
				HeightDataPerLayer, TEXT(""),
				MaterialLayerDataPerLayer,
				ELandscapeImportAlphamapType::Additive);

			// Apply material if tag is set
			if (LandscapeConfig.MaterialTag.IsValid())
			{
				FSoftObjectPath MatPath = UHktAssetSubsystem::ResolveConventionPath(LandscapeConfig.MaterialTag);
				if (MatPath.IsValid())
				{
					UMaterialInterface* Mat = Cast<UMaterialInterface>(MatPath.TryLoad());
					if (Mat) NewLandscape->LandscapeMaterial = Mat;
				}
			}

			RA.AllActors.Add(NewLandscape);
			UE_LOG(LogHktStreaming, Log, TEXT("Region '%s': ALandscape created %dx%d at runtime"),
				*Region.Name, LandscapeConfig.SizeX, LandscapeConfig.SizeY);
		}
	}
	else
	{
		UE_LOG(LogHktStreaming, Warning, TEXT("Region '%s': Heightmap size mismatch, skipping landscape"),
			*Region.Name);
	}

	// ── 3. Spawners ─────────────────────────────────────────────
	for (const auto& SpawnerData : Region.Spawners)
	{
		AHktSpawnerActor* Spawner = World->SpawnActor<AHktSpawnerActor>();
		if (Spawner)
		{
			Spawner->InitFromSpawnerData(SpawnerData);
			RA.Spawners.Add(Spawner);
			RA.AllActors.Add(Spawner);
		}
	}

	// ── 4. Props (resolve MeshTag → StaticMesh) ─────────────────
	for (const auto& Prop : Region.Props)
	{
		FTransform Transform;
		Transform.SetLocation(Prop.Position);
		Transform.SetRotation(FQuat(Prop.Rotation));
		Transform.SetScale3D(Prop.Scale);

		AStaticMeshActor* PropActor = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), &Transform);

		if (PropActor && Prop.MeshTag.IsValid())
		{
			FSoftObjectPath MeshPath = UHktAssetSubsystem::ResolveConventionPath(Prop.MeshTag);
			if (MeshPath.IsValid())
			{
				UStaticMesh* Mesh = Cast<UStaticMesh>(MeshPath.TryLoad());
				if (Mesh)
				{
					PropActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
				}
			}
			RA.AllActors.Add(PropActor);
		}
	}

	UE_LOG(LogHktStreaming, Log, TEXT("Region '%s': Spawned landscape + %d spawners + %d props"),
		*Region.Name, RA.Spawners.Num(), Region.Props.Num());
}

void UHktMapStreamingSubsystem::DestroyRegionContent(const FString& RegionName)
{
	if (FRegionActors* RA = RegionActorMap.Find(RegionName))
	{
		for (auto& WeakActor : RA->AllActors)
		{
			if (AActor* Actor = WeakActor.Get())
			{
				Actor->Destroy();
			}
		}
		RegionActorMap.Remove(RegionName);
	}
}

void UHktMapStreamingSubsystem::SpawnGlobalContent(const FHktMapData& MapData)
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (const auto& Entity : MapData.GlobalEntities)
	{
		FHktMapSpawner SpawnerData;
		SpawnerData.EntityTag = Entity.EntityTag;
		SpawnerData.Position = Entity.Position;
		SpawnerData.Rotation = Entity.Rotation;
		SpawnerData.Count = Entity.Count;
		SpawnerData.SpawnRule = (Entity.EntityType == EHktGlobalEntityType::WorldBoss)
			? EHktSpawnRule::OnTrigger : EHktSpawnRule::Always;

		AHktSpawnerActor* Spawner = World->SpawnActor<AHktSpawnerActor>();
		if (Spawner)
		{
			Spawner->InitFromSpawnerData(SpawnerData);
			Spawner->Activate();  // Global entities are always active
			GlobalActors.Add(Spawner);
		}
	}

	UE_LOG(LogHktStreaming, Log, TEXT("Spawned %d global entities"), MapData.GlobalEntities.Num());
}
