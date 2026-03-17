// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktSpawnerActor.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktSpawner, Log, All);

AHktSpawnerActor::AHktSpawnerActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AHktSpawnerActor::InitFromSpawnerData(const FHktMapSpawner& Data)
{
	EntityTag = Data.EntityTag;
	SpawnRule = Data.SpawnRule;
	SpawnCount = Data.Count;
	RespawnSeconds = Data.RespawnSeconds;
	SetActorLocationAndRotation(Data.Position, Data.Rotation);
}

void AHktSpawnerActor::Activate()
{
	if (bActive) return;
	bActive = true;

	if (SpawnRule == EHktSpawnRule::Always)
	{
		DoSpawn();
	}

	// Timed spawning
	if (SpawnRule == EHktSpawnRule::Timed && RespawnSeconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			RespawnTimerHandle,
			this, &AHktSpawnerActor::OnRespawnTimer,
			RespawnSeconds, true);
	}

	UE_LOG(LogHktSpawner, Log, TEXT("Spawner activated: %s (rule=%d, count=%d)"),
		*EntityTag.ToString(), static_cast<int32>(SpawnRule), SpawnCount);
}

void AHktSpawnerActor::Deactivate()
{
	if (!bActive) return;
	bActive = false;

	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);

	// Destroy all spawned entities
	for (auto& WeakEntity : SpawnedEntities)
	{
		if (AActor* Entity = WeakEntity.Get())
		{
			Entity->Destroy();
		}
	}
	SpawnedEntities.Empty();

	UE_LOG(LogHktSpawner, Log, TEXT("Spawner deactivated: %s"), *EntityTag.ToString());
}

void AHktSpawnerActor::BeginPlay()
{
	Super::BeginPlay();

	// Always-rule spawners start immediately if activated
	// Other rules wait for external trigger
}

void AHktSpawnerActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Deactivate();
	Super::EndPlay(EndPlayReason);
}

void AHktSpawnerActor::DoSpawn()
{
	// Clean up dead references
	SpawnedEntities.RemoveAll([](const TWeakObjectPtr<AActor>& Ptr) { return !Ptr.IsValid(); });

	int32 NeededCount = SpawnCount - SpawnedEntities.Num();
	if (NeededCount <= 0) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// TODO: Resolve EntityTag → Blueprint class via HktGeneratorRouter/ConventionPath
	// For now, log the intent and create placeholder actors
	for (int32 i = 0; i < NeededCount; ++i)
	{
		FVector SpawnLocation = GetActorLocation();
		// Spread spawns in a small radius
		if (SpawnCount > 1)
		{
			float Angle = (2.f * PI * i) / SpawnCount;
			float Radius = 200.f;
			SpawnLocation += FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
		}

		UE_LOG(LogHktSpawner, Log, TEXT("Spawn entity '%s' at (%.0f, %.0f, %.0f)"),
			*EntityTag.ToString(), SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);

		// Actual spawning will be implemented when GeneratorRouter integration is complete
		// SpawnedEntities.Add(SpawnedActor);
	}
}

void AHktSpawnerActor::OnRespawnTimer()
{
	if (!bActive) return;
	DoSpawn();
}
