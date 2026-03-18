// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktSpawnerActor.h"
#include "HktAssetSubsystem.h"
#include "HktAnimGeneratorTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
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

	// ── Resolve EntityTag → Blueprint class ─────────────────────
	UClass* SpawnClass = nullptr;

	if (EntityTag.IsValid())
	{
		// 1. Try ConventionPath resolution (settings-based)
		FSoftObjectPath ConventionPath = UHktAssetSubsystem::ResolveConventionPath(EntityTag);
		if (ConventionPath.IsValid())
		{
			UObject* LoadedObj = ConventionPath.TryLoad();
			if (UBlueprint* BP = Cast<UBlueprint>(LoadedObj))
			{
				SpawnClass = BP->GeneratedClass;
			}
			else if (UClass* AsClass = Cast<UClass>(LoadedObj))
			{
				SpawnClass = AsClass;
			}
		}

		// 2. Fallback: tag-based naming convention
		if (!SpawnClass)
		{
			FString TagStr = EntityTag.ToString();
			FString FallbackBPPath;

			if (TagStr.StartsWith(TEXT("Entity.Character.")))
			{
				FHktCharacterIntent Intent;
				if (FHktCharacterIntent::FromTag(EntityTag, Intent))
				{
					FallbackBPPath = FString::Printf(
						TEXT("/Game/Generated/Characters/%s/BP_%s"), *Intent.Name, *Intent.Name);
				}
			}
			else if (TagStr.StartsWith(TEXT("Entity.Item.")))
			{
				FHktItemIntent Intent;
				if (FHktItemIntent::FromTag(EntityTag, Intent))
				{
					FallbackBPPath = FString::Printf(
						TEXT("/Game/Generated/Items/%s/SM_%s"), *Intent.Category, *Intent.SubType);
				}
			}

			if (!FallbackBPPath.IsEmpty())
			{
				UObject* FallbackObj = FSoftObjectPath(FallbackBPPath).TryLoad();
				if (UBlueprint* BP = Cast<UBlueprint>(FallbackObj))
				{
					SpawnClass = BP->GeneratedClass;
				}
			}
		}

		if (!SpawnClass)
		{
			UE_LOG(LogHktSpawner, Warning, TEXT("Entity '%s': Could not resolve to a spawnable class"),
				*EntityTag.ToString());
		}
	}

	// ── Perform actual spawning ─────────────────────────────────
	for (int32 i = 0; i < NeededCount; ++i)
	{
		FVector SpawnLocation = GetActorLocation();
		if (SpawnCount > 1)
		{
			float Angle = (2.f * PI * i) / SpawnCount;
			float SpawnRadius = 200.f;
			SpawnLocation += FVector(FMath::Cos(Angle) * SpawnRadius, FMath::Sin(Angle) * SpawnRadius, 0.f);
		}

		if (SpawnClass)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			AActor* SpawnedActor = World->SpawnActor<AActor>(SpawnClass, &SpawnLocation, &GetActorRotation(), Params);
			if (SpawnedActor)
			{
				SpawnedEntities.Add(SpawnedActor);
				UE_LOG(LogHktSpawner, Log, TEXT("Spawned '%s' (%s) at (%.0f, %.0f, %.0f)"),
					*EntityTag.ToString(), *SpawnClass->GetName(),
					SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);
			}
		}
		else
		{
			UE_LOG(LogHktSpawner, Log, TEXT("Spawn deferred for '%s' at (%.0f, %.0f, %.0f) — class not yet available"),
				*EntityTag.ToString(), SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);
		}
	}
}

void AHktSpawnerActor::OnRespawnTimer()
{
	if (!bActive) return;
	DoSpawn();
}
