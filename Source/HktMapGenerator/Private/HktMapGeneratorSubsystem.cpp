// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktMapGeneratorSubsystem.h"
#include "HktMapGeneratorSettings.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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

// ── JSON Parsing Helpers ────────────────────────────────────────────

namespace
{
	FVector ParseJsonVector(const TSharedPtr<FJsonObject>& Obj, const FString& Key, FVector Default = FVector::ZeroVector)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Obj->TryGetArrayField(Key, Arr) && Arr->Num() >= 3)
		{
			return FVector(
				(*Arr)[0]->AsNumber(),
				(*Arr)[1]->AsNumber(),
				(*Arr)[2]->AsNumber()
			);
		}
		return Default;
	}

	TSharedPtr<FJsonValue> VectorToJsonArray(const FVector& V)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		Arr.Add(MakeShareable(new FJsonValueNumber(V.X)));
		Arr.Add(MakeShareable(new FJsonValueNumber(V.Y)));
		Arr.Add(MakeShareable(new FJsonValueNumber(V.Z)));
		return MakeShareable(new FJsonValueArray(Arr));
	}

	EHktSpawnRule ParseSpawnRule(const FString& Str)
	{
		if (Str == TEXT("on_story_start")) return EHktSpawnRule::OnStoryStart;
		if (Str == TEXT("on_trigger")) return EHktSpawnRule::OnTrigger;
		if (Str == TEXT("timed")) return EHktSpawnRule::Timed;
		return EHktSpawnRule::Always;
	}

	FString SpawnRuleToString(EHktSpawnRule Rule)
	{
		switch (Rule)
		{
		case EHktSpawnRule::OnStoryStart: return TEXT("on_story_start");
		case EHktSpawnRule::OnTrigger: return TEXT("on_trigger");
		case EHktSpawnRule::Timed: return TEXT("timed");
		default: return TEXT("always");
		}
	}
}

// ── JSON I/O ────────────────────────────────────────────────────────

bool UHktMapGeneratorSubsystem::ParseMapFromJson(const FString& JsonStr, FHktMapData& OutMapData)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogHktMapGenerator, Error, TEXT("Failed to parse HktMap JSON"));
		return false;
	}

	OutMapData.MapId = Root->GetStringField(TEXT("map_id"));
	OutMapData.MapName = Root->GetStringField(TEXT("map_name"));
	Root->TryGetStringField(TEXT("description"), OutMapData.Description);

	// Landscape
	const TSharedPtr<FJsonObject>* LandscapeObj;
	if (Root->TryGetObjectField(TEXT("landscape"), LandscapeObj))
	{
		auto& L = OutMapData.Landscape;
		L.SizeX = (*LandscapeObj)->GetIntegerField(TEXT("size_x"));
		L.SizeY = (*LandscapeObj)->GetIntegerField(TEXT("size_y"));
		(*LandscapeObj)->TryGetStringField(TEXT("heightmap_path"), L.HeightmapPath);
		FString MaterialTagStr;
		if ((*LandscapeObj)->TryGetStringField(TEXT("material_tag"), MaterialTagStr))
		{
			L.MaterialTag = FGameplayTag::RequestGameplayTag(FName(*MaterialTagStr), false);
		}
		(*LandscapeObj)->TryGetStringField(TEXT("biome"), L.Biome);

		const TSharedPtr<FJsonObject>* HeightRange;
		if ((*LandscapeObj)->TryGetObjectField(TEXT("height_range"), HeightRange))
		{
			L.HeightMin = (*HeightRange)->GetNumberField(TEXT("min"));
			L.HeightMax = (*HeightRange)->GetNumberField(TEXT("max"));
		}

		const TArray<TSharedPtr<FJsonValue>>* LayersArr;
		if ((*LandscapeObj)->TryGetArrayField(TEXT("layers"), LayersArr))
		{
			for (auto& Val : *LayersArr)
			{
				auto LayerObj = Val->AsObject();
				FHktMapLandscapeLayer Layer;
				Layer.Name = LayerObj->GetStringField(TEXT("name"));
				FString MatTag;
				if (LayerObj->TryGetStringField(TEXT("material_tag"), MatTag))
				{
					Layer.MaterialTag = FGameplayTag::RequestGameplayTag(FName(*MatTag), false);
				}
				LayerObj->TryGetStringField(TEXT("weight_map"), Layer.WeightMapPath);
				OutMapData.Landscape.Layers.Add(Layer);
			}
		}
	}

	// Regions
	const TArray<TSharedPtr<FJsonValue>>* RegionsArr;
	if (Root->TryGetArrayField(TEXT("regions"), RegionsArr))
	{
		for (auto& Val : *RegionsArr)
		{
			auto RegObj = Val->AsObject();
			FHktMapRegion Region;
			Region.Name = RegObj->GetStringField(TEXT("name"));

			const TSharedPtr<FJsonObject>* BoundsObj;
			if (RegObj->TryGetObjectField(TEXT("bounds"), BoundsObj))
			{
				Region.Center = ParseJsonVector(*BoundsObj, TEXT("center"));
				Region.Extent = ParseJsonVector(*BoundsObj, TEXT("extent"), FVector(1000.f));
			}

			const TSharedPtr<FJsonObject>* PropsObj;
			if (RegObj->TryGetObjectField(TEXT("properties"), PropsObj))
			{
				for (auto& Pair : (*PropsObj)->Values)
				{
					Region.Properties.Add(Pair.Key, Pair.Value->AsString());
				}
			}
			OutMapData.Regions.Add(Region);
		}
	}

	// Spawners
	const TArray<TSharedPtr<FJsonValue>>* SpawnersArr;
	if (Root->TryGetArrayField(TEXT("spawners"), SpawnersArr))
	{
		for (auto& Val : *SpawnersArr)
		{
			auto SpObj = Val->AsObject();
			FHktMapSpawner Spawner;
			FString EntityTagStr = SpObj->GetStringField(TEXT("entity_tag"));
			Spawner.EntityTag = FGameplayTag::RequestGameplayTag(FName(*EntityTagStr), false);
			Spawner.Position = ParseJsonVector(SpObj, TEXT("position"));

			const TArray<TSharedPtr<FJsonValue>>* RotArr;
			if (SpObj->TryGetArrayField(TEXT("rotation"), RotArr) && RotArr->Num() >= 3)
			{
				Spawner.Rotation = FRotator(
					(*RotArr)[0]->AsNumber(),
					(*RotArr)[1]->AsNumber(),
					(*RotArr)[2]->AsNumber()
				);
			}

			FString RuleStr;
			if (SpObj->TryGetStringField(TEXT("spawn_rule"), RuleStr))
			{
				Spawner.SpawnRule = ParseSpawnRule(RuleStr);
			}
			SpObj->TryGetStringField(TEXT("region"), Spawner.Region);
			Spawner.Count = SpObj->GetIntegerField(TEXT("count"));
			if (Spawner.Count < 1) Spawner.Count = 1;
			SpObj->TryGetNumberField(TEXT("respawn_seconds"), Spawner.RespawnSeconds);

			OutMapData.Spawners.Add(Spawner);
		}
	}

	// Stories
	const TArray<TSharedPtr<FJsonValue>>* StoriesArr;
	if (Root->TryGetArrayField(TEXT("stories"), StoriesArr))
	{
		for (auto& Val : *StoriesArr)
		{
			auto StObj = Val->AsObject();
			FHktMapStoryRef Story;
			FString StoryTagStr = StObj->GetStringField(TEXT("story_tag"));
			Story.StoryTag = FGameplayTag::RequestGameplayTag(FName(*StoryTagStr), false);
			StObj->TryGetBoolField(TEXT("auto_load"), Story.bAutoLoad);
			StObj->TryGetStringField(TEXT("trigger_region"), Story.TriggerRegion);
			OutMapData.Stories.Add(Story);
		}
	}

	// Props
	const TArray<TSharedPtr<FJsonValue>>* PropsArr;
	if (Root->TryGetArrayField(TEXT("props"), PropsArr))
	{
		for (auto& Val : *PropsArr)
		{
			auto PrObj = Val->AsObject();
			FHktMapProp Prop;
			FString MeshTagStr = PrObj->GetStringField(TEXT("mesh_tag"));
			Prop.MeshTag = FGameplayTag::RequestGameplayTag(FName(*MeshTagStr), false);
			Prop.Position = ParseJsonVector(PrObj, TEXT("position"));

			const TArray<TSharedPtr<FJsonValue>>* RotArr;
			if (PrObj->TryGetArrayField(TEXT("rotation"), RotArr) && RotArr->Num() >= 3)
			{
				Prop.Rotation = FRotator(
					(*RotArr)[0]->AsNumber(),
					(*RotArr)[1]->AsNumber(),
					(*RotArr)[2]->AsNumber()
				);
			}
			Prop.Scale = ParseJsonVector(PrObj, TEXT("scale"), FVector::OneVector);
			OutMapData.Props.Add(Prop);
		}
	}

	UE_LOG(LogHktMapGenerator, Log, TEXT("Parsed HktMap '%s' — %d regions, %d spawners, %d stories"),
		*OutMapData.MapId, OutMapData.Regions.Num(), OutMapData.Spawners.Num(), OutMapData.Stories.Num());
	return true;
}

FString UHktMapGeneratorSubsystem::SerializeMapToJson(const FHktMapData& MapData)
{
	TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject());
	Root->SetStringField(TEXT("map_id"), MapData.MapId);
	Root->SetStringField(TEXT("map_name"), MapData.MapName);
	Root->SetStringField(TEXT("description"), MapData.Description);

	// Landscape
	TSharedPtr<FJsonObject> LandObj = MakeShareable(new FJsonObject());
	LandObj->SetNumberField(TEXT("size_x"), MapData.Landscape.SizeX);
	LandObj->SetNumberField(TEXT("size_y"), MapData.Landscape.SizeY);
	LandObj->SetStringField(TEXT("heightmap_path"), MapData.Landscape.HeightmapPath);
	LandObj->SetStringField(TEXT("material_tag"), MapData.Landscape.MaterialTag.ToString());
	LandObj->SetStringField(TEXT("biome"), MapData.Landscape.Biome);
	{
		TSharedPtr<FJsonObject> HR = MakeShareable(new FJsonObject());
		HR->SetNumberField(TEXT("min"), MapData.Landscape.HeightMin);
		HR->SetNumberField(TEXT("max"), MapData.Landscape.HeightMax);
		LandObj->SetObjectField(TEXT("height_range"), HR);
	}
	Root->SetObjectField(TEXT("landscape"), LandObj);

	// Regions
	TArray<TSharedPtr<FJsonValue>> RegArr;
	for (auto& R : MapData.Regions)
	{
		TSharedPtr<FJsonObject> RObj = MakeShareable(new FJsonObject());
		RObj->SetStringField(TEXT("name"), R.Name);
		TSharedPtr<FJsonObject> BObj = MakeShareable(new FJsonObject());
		BObj->SetField(TEXT("center"), VectorToJsonArray(R.Center));
		BObj->SetField(TEXT("extent"), VectorToJsonArray(R.Extent));
		RObj->SetObjectField(TEXT("bounds"), BObj);
		RegArr.Add(MakeShareable(new FJsonValueObject(RObj)));
	}
	Root->SetArrayField(TEXT("regions"), RegArr);

	// Spawners
	TArray<TSharedPtr<FJsonValue>> SpArr;
	for (auto& S : MapData.Spawners)
	{
		TSharedPtr<FJsonObject> SObj = MakeShareable(new FJsonObject());
		SObj->SetStringField(TEXT("entity_tag"), S.EntityTag.ToString());
		SObj->SetField(TEXT("position"), VectorToJsonArray(S.Position));
		SObj->SetStringField(TEXT("spawn_rule"), SpawnRuleToString(S.SpawnRule));
		SObj->SetStringField(TEXT("region"), S.Region);
		SObj->SetNumberField(TEXT("count"), S.Count);
		SObj->SetNumberField(TEXT("respawn_seconds"), S.RespawnSeconds);
		SpArr.Add(MakeShareable(new FJsonValueObject(SObj)));
	}
	Root->SetArrayField(TEXT("spawners"), SpArr);

	// Stories
	TArray<TSharedPtr<FJsonValue>> StArr;
	for (auto& St : MapData.Stories)
	{
		TSharedPtr<FJsonObject> StObj = MakeShareable(new FJsonObject());
		StObj->SetStringField(TEXT("story_tag"), St.StoryTag.ToString());
		StObj->SetBoolField(TEXT("auto_load"), St.bAutoLoad);
		StObj->SetStringField(TEXT("trigger_region"), St.TriggerRegion);
		StArr.Add(MakeShareable(new FJsonValueObject(StObj)));
	}
	Root->SetArrayField(TEXT("stories"), StArr);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Output;
}

// ── Build ───────────────────────────────────────────────────────────

bool UHktMapGeneratorSubsystem::BuildMap(const FHktMapData& MapData)
{
	UE_LOG(LogHktMapGenerator, Log, TEXT("BuildMap: '%s' — will set up landscape, spawners, stories"), *MapData.MapId);

	// TODO: Implement actual UE5 world construction:
	// 1. Create/modify Landscape from heightmap + layers
	// 2. Spawn Spawner actors at designated positions
	// 3. Register stories for auto-load via HktStoryRegistry
	// 4. Place props at designated positions
	// This requires the UE5 editor context and will be implemented
	// when connected to the actual editor subsystem.

	CurrentMapId = MapData.MapId;
	return true;
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
	return FString::Printf(TEXT("{\"success\":%s,\"map_id\":\"%s\"}"),
		bSuccess ? TEXT("true") : TEXT("false"), *MapData.MapId);
}

FString UHktMapGeneratorSubsystem::McpValidateMap(const FString& JsonStr)
{
	FHktMapData MapData;
	bool bValid = ParseMapFromJson(JsonStr, MapData);
	if (!bValid)
	{
		return TEXT("{\"valid\":false,\"error\":\"Failed to parse JSON\"}");
	}

	// Basic validation
	TArray<FString> Errors;
	if (MapData.MapId.IsEmpty()) Errors.Add(TEXT("Missing map_id"));
	if (MapData.MapName.IsEmpty()) Errors.Add(TEXT("Missing map_name"));

	TSet<FString> RegionNames;
	for (auto& R : MapData.Regions)
	{
		RegionNames.Add(R.Name);
	}

	for (int32 i = 0; i < MapData.Spawners.Num(); ++i)
	{
		auto& S = MapData.Spawners[i];
		if (!S.Region.IsEmpty() && !RegionNames.Contains(S.Region))
		{
			Errors.Add(FString::Printf(TEXT("Spawner[%d]: unknown region '%s'"), i, *S.Region));
		}
	}

	return FString::Printf(TEXT("{\"valid\":%s,\"error_count\":%d}"),
		Errors.Num() == 0 ? TEXT("true") : TEXT("false"), Errors.Num());
}

FString UHktMapGeneratorSubsystem::McpGetMapSchema()
{
	// Return a simplified schema description
	return TEXT(R"({
		"description": "HktMap JSON schema for dynamic map generation",
		"required": ["map_id", "map_name", "landscape", "stories"],
		"sections": {
			"landscape": "Terrain config (size, heightmap, biome, layers)",
			"regions": "Named regions with bounds and properties",
			"spawners": "Entity spawn points with rules",
			"stories": "Linked stories that load with the map",
			"props": "Static prop placements"
		}
	})");
}
