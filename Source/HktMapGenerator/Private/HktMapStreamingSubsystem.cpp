// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktMapStreamingSubsystem.h"
#include "HktTerrainRecipeBuilder.h"
#include "HktMapRegionVolume.h"
#include "HktSpawnerActor.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "TimerManager.h"
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

	// 1. Region Volume (for visualization/overlap)
	AHktMapRegionVolume* Volume = World->SpawnActor<AHktMapRegionVolume>();
	if (Volume)
	{
		Volume->InitFromRegionData(Region.Name, Region.Center, Region.Extent, Region.Properties);
		RA.Volume = Volume;
		RA.AllActors.Add(Volume);
	}

	// 2. Landscape generation
	// TODO: Create ALandscape from TerrainRecipe or heightmap file
	// This is the same logic as BuildRegionLandscape in the editor subsystem
	// For runtime, we generate the heightmap and create the landscape actor
	if (Region.Landscape.HeightmapPath.IsEmpty())
	{
		TArray<uint16> HeightData = FHktTerrainRecipeBuilder::GenerateHeightmap(
			Region.Landscape.TerrainRecipe,
			Region.Landscape.SizeX, Region.Landscape.SizeY,
			Region.Landscape.HeightMin, Region.Landscape.HeightMax);

		UE_LOG(LogHktStreaming, Log, TEXT("Region '%s': Generated %dx%d heightmap"),
			*Region.Name, Region.Landscape.SizeX, Region.Landscape.SizeY);

		// TODO: Create ALandscape from HeightData at Region.Center
		// ALandscape creation at runtime requires careful handling
	}

	// 3. Spawners
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

	// 4. Props
	// TODO: Resolve MeshTag → StaticMesh and spawn

	UE_LOG(LogHktStreaming, Log, TEXT("Region '%s': Spawned %d spawners"),
		*Region.Name, RA.Spawners.Num());
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
