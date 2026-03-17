// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktMapGeneratorSubsystem.h"
#include "HktMapGeneratorSettings.h"
#include "HktTerrainRecipeBuilder.h"
#include "HktMapRegionVolume.h"
#include "HktSpawnerActor.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
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

	FVector2D ParseJsonVector2D(const TSharedPtr<FJsonObject>& Obj, const FString& Key, FVector2D Default = FVector2D::ZeroVector)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Obj->TryGetArrayField(Key, Arr) && Arr->Num() >= 2)
		{
			return FVector2D((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber());
		}
		return Default;
	}

	FLinearColor ParseJsonColor(const TSharedPtr<FJsonObject>& Obj, const FString& Key, FLinearColor Default = FLinearColor::White)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Obj->TryGetArrayField(Key, Arr) && Arr->Num() >= 3)
		{
			float A = (Arr->Num() >= 4) ? (*Arr)[3]->AsNumber() : 1.0f;
			return FLinearColor((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber(), A);
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

	TSharedPtr<FJsonValue> Vector2DToJsonArray(const FVector2D& V)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		Arr.Add(MakeShareable(new FJsonValueNumber(V.X)));
		Arr.Add(MakeShareable(new FJsonValueNumber(V.Y)));
		return MakeShareable(new FJsonValueArray(Arr));
	}

	TSharedPtr<FJsonValue> ColorToJsonArray(const FLinearColor& C)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		Arr.Add(MakeShareable(new FJsonValueNumber(C.R)));
		Arr.Add(MakeShareable(new FJsonValueNumber(C.G)));
		Arr.Add(MakeShareable(new FJsonValueNumber(C.B)));
		Arr.Add(MakeShareable(new FJsonValueNumber(C.A)));
		return MakeShareable(new FJsonValueArray(Arr));
	}

	TSharedPtr<FJsonValue> RotatorToJsonArray(const FRotator& R)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		Arr.Add(MakeShareable(new FJsonValueNumber(R.Pitch)));
		Arr.Add(MakeShareable(new FJsonValueNumber(R.Yaw)));
		Arr.Add(MakeShareable(new FJsonValueNumber(R.Roll)));
		return MakeShareable(new FJsonValueArray(Arr));
	}

	FRotator ParseJsonRotator(const TSharedPtr<FJsonObject>& Obj, const FString& Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Obj->TryGetArrayField(Key, Arr) && Arr->Num() >= 3)
		{
			return FRotator((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
		}
		return FRotator::ZeroRotator;
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

	EHktGlobalEntityType ParseGlobalEntityType(const FString& Str)
	{
		if (Str == TEXT("world_boss")) return EHktGlobalEntityType::WorldBoss;
		if (Str == TEXT("npc_spawner")) return EHktGlobalEntityType::NPCSpawner;
		return EHktGlobalEntityType::NPC;
	}

	FString GlobalEntityTypeToString(EHktGlobalEntityType Type)
	{
		switch (Type)
		{
		case EHktGlobalEntityType::WorldBoss: return TEXT("world_boss");
		case EHktGlobalEntityType::NPCSpawner: return TEXT("npc_spawner");
		default: return TEXT("npc");
		}
	}

	// ── Sub-struct Parsing ──────────────────────────────────────────

	FHktTerrainFeature ParseTerrainFeature(const TSharedPtr<FJsonObject>& Obj)
	{
		FHktTerrainFeature F;
		Obj->TryGetStringField(TEXT("type"), F.Type);
		F.Position = ParseJsonVector2D(Obj, TEXT("position"), FVector2D(0.5f, 0.5f));
		Obj->TryGetNumberField(TEXT("radius"), F.Radius);
		Obj->TryGetNumberField(TEXT("intensity"), F.Intensity);
		Obj->TryGetStringField(TEXT("falloff"), F.Falloff);
		return F;
	}

	FHktTerrainRecipe ParseTerrainRecipe(const TSharedPtr<FJsonObject>& Obj)
	{
		FHktTerrainRecipe R;
		Obj->TryGetStringField(TEXT("base_noise_type"), R.BaseNoiseType);
		Obj->TryGetNumberField(TEXT("octaves"), R.Octaves);
		Obj->TryGetNumberField(TEXT("frequency"), R.Frequency);
		Obj->TryGetNumberField(TEXT("lacunarity"), R.Lacunarity);
		Obj->TryGetNumberField(TEXT("persistence"), R.Persistence);
		Obj->TryGetNumberField(TEXT("seed"), R.Seed);
		Obj->TryGetNumberField(TEXT("erosion_passes"), R.ErosionPasses);

		const TArray<TSharedPtr<FJsonValue>>* FeaturesArr;
		if (Obj->TryGetArrayField(TEXT("features"), FeaturesArr))
		{
			for (auto& Val : *FeaturesArr)
			{
				R.Features.Add(ParseTerrainFeature(Val->AsObject()));
			}
		}
		return R;
	}

	FHktMapLandscape ParseLandscape(const TSharedPtr<FJsonObject>& Obj)
	{
		FHktMapLandscape L;
		Obj->TryGetNumberField(TEXT("size_x"), L.SizeX);
		Obj->TryGetNumberField(TEXT("size_y"), L.SizeY);
		Obj->TryGetStringField(TEXT("heightmap_path"), L.HeightmapPath);

		FString MaterialTagStr;
		if (Obj->TryGetStringField(TEXT("material_tag"), MaterialTagStr))
		{
			L.MaterialTag = FGameplayTag::RequestGameplayTag(FName(*MaterialTagStr), false);
		}
		Obj->TryGetStringField(TEXT("biome"), L.Biome);

		const TSharedPtr<FJsonObject>* HeightRange;
		if (Obj->TryGetObjectField(TEXT("height_range"), HeightRange))
		{
			(*HeightRange)->TryGetNumberField(TEXT("min"), L.HeightMin);
			(*HeightRange)->TryGetNumberField(TEXT("max"), L.HeightMax);
		}

		const TSharedPtr<FJsonObject>* RecipeObj;
		if (Obj->TryGetObjectField(TEXT("terrain_recipe"), RecipeObj))
		{
			L.TerrainRecipe = ParseTerrainRecipe(*RecipeObj);
		}

		const TArray<TSharedPtr<FJsonValue>>* LayersArr;
		if (Obj->TryGetArrayField(TEXT("layers"), LayersArr))
		{
			for (auto& Val : *LayersArr)
			{
				auto LayerObj = Val->AsObject();
				FHktMapLandscapeLayer Layer;
				LayerObj->TryGetStringField(TEXT("name"), Layer.Name);
				FString MatTag;
				if (LayerObj->TryGetStringField(TEXT("material_tag"), MatTag))
				{
					Layer.MaterialTag = FGameplayTag::RequestGameplayTag(FName(*MatTag), false);
				}
				LayerObj->TryGetStringField(TEXT("weight_map"), Layer.WeightMapPath);
				L.Layers.Add(Layer);
			}
		}
		return L;
	}

	FHktMapSpawner ParseSpawner(const TSharedPtr<FJsonObject>& Obj)
	{
		FHktMapSpawner S;
		FString EntityTagStr;
		if (Obj->TryGetStringField(TEXT("entity_tag"), EntityTagStr))
		{
			S.EntityTag = FGameplayTag::RequestGameplayTag(FName(*EntityTagStr), false);
		}
		S.Position = ParseJsonVector(Obj, TEXT("position"));
		S.Rotation = ParseJsonRotator(Obj, TEXT("rotation"));

		FString RuleStr;
		if (Obj->TryGetStringField(TEXT("spawn_rule"), RuleStr))
		{
			S.SpawnRule = ParseSpawnRule(RuleStr);
		}

		Obj->TryGetNumberField(TEXT("count"), S.Count);
		if (S.Count < 1) S.Count = 1;
		Obj->TryGetNumberField(TEXT("respawn_seconds"), S.RespawnSeconds);
		return S;
	}

	FHktMapStoryRef ParseStoryRef(const TSharedPtr<FJsonObject>& Obj)
	{
		FHktMapStoryRef St;
		FString StoryTagStr;
		if (Obj->TryGetStringField(TEXT("story_tag"), StoryTagStr))
		{
			St.StoryTag = FGameplayTag::RequestGameplayTag(FName(*StoryTagStr), false);
		}
		Obj->TryGetBoolField(TEXT("auto_load"), St.bAutoLoad);
		return St;
	}

	FHktMapProp ParseProp(const TSharedPtr<FJsonObject>& Obj)
	{
		FHktMapProp P;
		FString MeshTagStr;
		if (Obj->TryGetStringField(TEXT("mesh_tag"), MeshTagStr))
		{
			P.MeshTag = FGameplayTag::RequestGameplayTag(FName(*MeshTagStr), false);
		}
		P.Position = ParseJsonVector(Obj, TEXT("position"));
		P.Rotation = ParseJsonRotator(Obj, TEXT("rotation"));
		P.Scale = ParseJsonVector(Obj, TEXT("scale"), FVector::OneVector);
		return P;
	}

	FHktMapGlobalEntity ParseGlobalEntity(const TSharedPtr<FJsonObject>& Obj)
	{
		FHktMapGlobalEntity E;
		FString TagStr;
		if (Obj->TryGetStringField(TEXT("entity_tag"), TagStr))
		{
			E.EntityTag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
		}
		FString TypeStr;
		if (Obj->TryGetStringField(TEXT("entity_type"), TypeStr))
		{
			E.EntityType = ParseGlobalEntityType(TypeStr);
		}
		E.Position = ParseJsonVector(Obj, TEXT("position"));
		E.Rotation = ParseJsonRotator(Obj, TEXT("rotation"));
		Obj->TryGetNumberField(TEXT("count"), E.Count);
		if (E.Count < 1) E.Count = 1;

		const TSharedPtr<FJsonObject>* PropsObj;
		if (Obj->TryGetObjectField(TEXT("properties"), PropsObj))
		{
			for (auto& Pair : (*PropsObj)->Values)
			{
				E.Properties.Add(Pair.Key, Pair.Value->AsString());
			}
		}
		return E;
	}

	FHktMapEnvironment ParseEnvironment(const TSharedPtr<FJsonObject>& Obj)
	{
		FHktMapEnvironment Env;
		Obj->TryGetStringField(TEXT("weather"), Env.Weather);
		Obj->TryGetStringField(TEXT("time_of_day"), Env.TimeOfDay);
		Obj->TryGetNumberField(TEXT("fog_density"), Env.FogDensity);
		Env.WindDirection = ParseJsonVector(Obj, TEXT("wind_direction"), FVector(1.f, 0.f, 0.f));
		Obj->TryGetNumberField(TEXT("wind_strength"), Env.WindStrength);
		Env.AmbientColor = ParseJsonColor(Obj, TEXT("ambient_color"), FLinearColor(0.5f, 0.5f, 0.6f));
		Env.SunColor = ParseJsonColor(Obj, TEXT("sun_color"), FLinearColor(1.f, 0.95f, 0.8f));

		const TArray<TSharedPtr<FJsonValue>>* VFXArr;
		if (Obj->TryGetArrayField(TEXT("ambient_vfx_tags"), VFXArr))
		{
			for (auto& Val : *VFXArr)
			{
				FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Val->AsString()), false);
				if (Tag.IsValid())
				{
					Env.AmbientVFXTags.Add(Tag);
				}
			}
		}
		return Env;
	}

	FHktMapRegion ParseRegion(const TSharedPtr<FJsonObject>& Obj)
	{
		FHktMapRegion Region;
		Obj->TryGetStringField(TEXT("name"), Region.Name);

		const TSharedPtr<FJsonObject>* BoundsObj;
		if (Obj->TryGetObjectField(TEXT("bounds"), BoundsObj))
		{
			Region.Center = ParseJsonVector(*BoundsObj, TEXT("center"));
			Region.Extent = ParseJsonVector(*BoundsObj, TEXT("extent"), FVector(1000.f));
		}

		const TSharedPtr<FJsonObject>* PropsObj;
		if (Obj->TryGetObjectField(TEXT("properties"), PropsObj))
		{
			for (auto& Pair : (*PropsObj)->Values)
			{
				Region.Properties.Add(Pair.Key, Pair.Value->AsString());
			}
		}

		const TSharedPtr<FJsonObject>* LandscapeObj;
		if (Obj->TryGetObjectField(TEXT("landscape"), LandscapeObj))
		{
			Region.Landscape = ParseLandscape(*LandscapeObj);
		}

		const TArray<TSharedPtr<FJsonValue>>* SpawnersArr;
		if (Obj->TryGetArrayField(TEXT("spawners"), SpawnersArr))
		{
			for (auto& Val : *SpawnersArr)
			{
				Region.Spawners.Add(ParseSpawner(Val->AsObject()));
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* StoriesArr;
		if (Obj->TryGetArrayField(TEXT("stories"), StoriesArr))
		{
			for (auto& Val : *StoriesArr)
			{
				Region.Stories.Add(ParseStoryRef(Val->AsObject()));
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* PropsArr;
		if (Obj->TryGetArrayField(TEXT("props"), PropsArr))
		{
			for (auto& Val : *PropsArr)
			{
				Region.Props.Add(ParseProp(Val->AsObject()));
			}
		}

		return Region;
	}

	// ── Sub-struct Serialization ────────────────────────────────────

	TSharedPtr<FJsonObject> SerializeTerrainFeature(const FHktTerrainFeature& F)
	{
		auto Obj = MakeShareable(new FJsonObject());
		Obj->SetStringField(TEXT("type"), F.Type);
		Obj->SetField(TEXT("position"), Vector2DToJsonArray(F.Position));
		Obj->SetNumberField(TEXT("radius"), F.Radius);
		Obj->SetNumberField(TEXT("intensity"), F.Intensity);
		Obj->SetStringField(TEXT("falloff"), F.Falloff);
		return Obj;
	}

	TSharedPtr<FJsonObject> SerializeTerrainRecipe(const FHktTerrainRecipe& R)
	{
		auto Obj = MakeShareable(new FJsonObject());
		Obj->SetStringField(TEXT("base_noise_type"), R.BaseNoiseType);
		Obj->SetNumberField(TEXT("octaves"), R.Octaves);
		Obj->SetNumberField(TEXT("frequency"), R.Frequency);
		Obj->SetNumberField(TEXT("lacunarity"), R.Lacunarity);
		Obj->SetNumberField(TEXT("persistence"), R.Persistence);
		Obj->SetNumberField(TEXT("seed"), R.Seed);
		Obj->SetNumberField(TEXT("erosion_passes"), R.ErosionPasses);

		TArray<TSharedPtr<FJsonValue>> FeatArr;
		for (auto& F : R.Features)
		{
			FeatArr.Add(MakeShareable(new FJsonValueObject(SerializeTerrainFeature(F))));
		}
		Obj->SetArrayField(TEXT("features"), FeatArr);
		return Obj;
	}

	TSharedPtr<FJsonObject> SerializeLandscape(const FHktMapLandscape& L)
	{
		auto Obj = MakeShareable(new FJsonObject());
		Obj->SetNumberField(TEXT("size_x"), L.SizeX);
		Obj->SetNumberField(TEXT("size_y"), L.SizeY);
		Obj->SetStringField(TEXT("heightmap_path"), L.HeightmapPath);
		Obj->SetStringField(TEXT("material_tag"), L.MaterialTag.ToString());
		Obj->SetStringField(TEXT("biome"), L.Biome);

		auto HR = MakeShareable(new FJsonObject());
		HR->SetNumberField(TEXT("min"), L.HeightMin);
		HR->SetNumberField(TEXT("max"), L.HeightMax);
		Obj->SetObjectField(TEXT("height_range"), HR);

		Obj->SetObjectField(TEXT("terrain_recipe"), SerializeTerrainRecipe(L.TerrainRecipe));

		TArray<TSharedPtr<FJsonValue>> LayerArr;
		for (auto& Layer : L.Layers)
		{
			auto LObj = MakeShareable(new FJsonObject());
			LObj->SetStringField(TEXT("name"), Layer.Name);
			LObj->SetStringField(TEXT("material_tag"), Layer.MaterialTag.ToString());
			LObj->SetStringField(TEXT("weight_map"), Layer.WeightMapPath);
			LayerArr.Add(MakeShareable(new FJsonValueObject(LObj)));
		}
		Obj->SetArrayField(TEXT("layers"), LayerArr);
		return Obj;
	}

	TSharedPtr<FJsonObject> SerializeSpawner(const FHktMapSpawner& S)
	{
		auto Obj = MakeShareable(new FJsonObject());
		Obj->SetStringField(TEXT("entity_tag"), S.EntityTag.ToString());
		Obj->SetField(TEXT("position"), VectorToJsonArray(S.Position));
		Obj->SetField(TEXT("rotation"), RotatorToJsonArray(S.Rotation));
		Obj->SetStringField(TEXT("spawn_rule"), SpawnRuleToString(S.SpawnRule));
		Obj->SetNumberField(TEXT("count"), S.Count);
		Obj->SetNumberField(TEXT("respawn_seconds"), S.RespawnSeconds);
		return Obj;
	}

	TSharedPtr<FJsonObject> SerializeStoryRef(const FHktMapStoryRef& St)
	{
		auto Obj = MakeShareable(new FJsonObject());
		Obj->SetStringField(TEXT("story_tag"), St.StoryTag.ToString());
		Obj->SetBoolField(TEXT("auto_load"), St.bAutoLoad);
		return Obj;
	}

	TSharedPtr<FJsonObject> SerializeProp(const FHktMapProp& P)
	{
		auto Obj = MakeShareable(new FJsonObject());
		Obj->SetStringField(TEXT("mesh_tag"), P.MeshTag.ToString());
		Obj->SetField(TEXT("position"), VectorToJsonArray(P.Position));
		Obj->SetField(TEXT("rotation"), RotatorToJsonArray(P.Rotation));
		Obj->SetField(TEXT("scale"), VectorToJsonArray(P.Scale));
		return Obj;
	}

	TSharedPtr<FJsonObject> SerializeGlobalEntity(const FHktMapGlobalEntity& E)
	{
		auto Obj = MakeShareable(new FJsonObject());
		Obj->SetStringField(TEXT("entity_tag"), E.EntityTag.ToString());
		Obj->SetStringField(TEXT("entity_type"), GlobalEntityTypeToString(E.EntityType));
		Obj->SetField(TEXT("position"), VectorToJsonArray(E.Position));
		Obj->SetField(TEXT("rotation"), RotatorToJsonArray(E.Rotation));
		Obj->SetNumberField(TEXT("count"), E.Count);

		auto PropsObj = MakeShareable(new FJsonObject());
		for (auto& Pair : E.Properties)
		{
			PropsObj->SetStringField(Pair.Key, Pair.Value);
		}
		Obj->SetObjectField(TEXT("properties"), PropsObj);
		return Obj;
	}

	TSharedPtr<FJsonObject> SerializeEnvironment(const FHktMapEnvironment& Env)
	{
		auto Obj = MakeShareable(new FJsonObject());
		Obj->SetStringField(TEXT("weather"), Env.Weather);
		Obj->SetStringField(TEXT("time_of_day"), Env.TimeOfDay);
		Obj->SetNumberField(TEXT("fog_density"), Env.FogDensity);
		Obj->SetField(TEXT("wind_direction"), VectorToJsonArray(Env.WindDirection));
		Obj->SetNumberField(TEXT("wind_strength"), Env.WindStrength);
		Obj->SetField(TEXT("ambient_color"), ColorToJsonArray(Env.AmbientColor));
		Obj->SetField(TEXT("sun_color"), ColorToJsonArray(Env.SunColor));

		TArray<TSharedPtr<FJsonValue>> VFXArr;
		for (auto& Tag : Env.AmbientVFXTags)
		{
			VFXArr.Add(MakeShareable(new FJsonValueString(Tag.ToString())));
		}
		Obj->SetArrayField(TEXT("ambient_vfx_tags"), VFXArr);
		return Obj;
	}

	TSharedPtr<FJsonObject> SerializeRegion(const FHktMapRegion& R)
	{
		auto Obj = MakeShareable(new FJsonObject());
		Obj->SetStringField(TEXT("name"), R.Name);

		auto BoundsObj = MakeShareable(new FJsonObject());
		BoundsObj->SetField(TEXT("center"), VectorToJsonArray(R.Center));
		BoundsObj->SetField(TEXT("extent"), VectorToJsonArray(R.Extent));
		Obj->SetObjectField(TEXT("bounds"), BoundsObj);

		auto PropsMapObj = MakeShareable(new FJsonObject());
		for (auto& Pair : R.Properties)
		{
			PropsMapObj->SetStringField(Pair.Key, Pair.Value);
		}
		Obj->SetObjectField(TEXT("properties"), PropsMapObj);

		Obj->SetObjectField(TEXT("landscape"), SerializeLandscape(R.Landscape));

		TArray<TSharedPtr<FJsonValue>> SpawnArr;
		for (auto& S : R.Spawners) SpawnArr.Add(MakeShareable(new FJsonValueObject(SerializeSpawner(S))));
		Obj->SetArrayField(TEXT("spawners"), SpawnArr);

		TArray<TSharedPtr<FJsonValue>> StoryArr;
		for (auto& St : R.Stories) StoryArr.Add(MakeShareable(new FJsonValueObject(SerializeStoryRef(St))));
		Obj->SetArrayField(TEXT("stories"), StoryArr);

		TArray<TSharedPtr<FJsonValue>> PropArr;
		for (auto& P : R.Props) PropArr.Add(MakeShareable(new FJsonValueObject(SerializeProp(P))));
		Obj->SetArrayField(TEXT("props"), PropArr);

		return Obj;
	}
} // anonymous namespace

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

	Root->TryGetStringField(TEXT("map_id"), OutMapData.MapId);
	Root->TryGetStringField(TEXT("map_name"), OutMapData.MapName);
	Root->TryGetStringField(TEXT("description"), OutMapData.Description);

	// Regions (each with own landscape)
	const TArray<TSharedPtr<FJsonValue>>* RegionsArr;
	if (Root->TryGetArrayField(TEXT("regions"), RegionsArr))
	{
		for (auto& Val : *RegionsArr)
		{
			OutMapData.Regions.Add(ParseRegion(Val->AsObject()));
		}
	}

	// Global Entities
	const TArray<TSharedPtr<FJsonValue>>* GlobalEntArr;
	if (Root->TryGetArrayField(TEXT("global_entities"), GlobalEntArr))
	{
		for (auto& Val : *GlobalEntArr)
		{
			OutMapData.GlobalEntities.Add(ParseGlobalEntity(Val->AsObject()));
		}
	}

	// Environment
	const TSharedPtr<FJsonObject>* EnvObj;
	if (Root->TryGetObjectField(TEXT("environment"), EnvObj))
	{
		OutMapData.Environment = ParseEnvironment(*EnvObj);
	}

	// Global Stories
	const TArray<TSharedPtr<FJsonValue>>* GStoriesArr;
	if (Root->TryGetArrayField(TEXT("global_stories"), GStoriesArr))
	{
		for (auto& Val : *GStoriesArr)
		{
			OutMapData.GlobalStories.Add(ParseStoryRef(Val->AsObject()));
		}
	}

	int32 TotalSpawners = 0, TotalStories = 0;
	for (auto& R : OutMapData.Regions)
	{
		TotalSpawners += R.Spawners.Num();
		TotalStories += R.Stories.Num();
	}
	TotalStories += OutMapData.GlobalStories.Num();

	UE_LOG(LogHktMapGenerator, Log, TEXT("Parsed HktMap '%s' — %d regions, %d spawners, %d stories, %d global entities"),
		*OutMapData.MapId, OutMapData.Regions.Num(), TotalSpawners, TotalStories, OutMapData.GlobalEntities.Num());
	return true;
}

FString UHktMapGeneratorSubsystem::SerializeMapToJson(const FHktMapData& MapData)
{
	TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject());
	Root->SetStringField(TEXT("map_id"), MapData.MapId);
	Root->SetStringField(TEXT("map_name"), MapData.MapName);
	Root->SetStringField(TEXT("description"), MapData.Description);

	// Regions
	TArray<TSharedPtr<FJsonValue>> RegArr;
	for (auto& R : MapData.Regions)
	{
		RegArr.Add(MakeShareable(new FJsonValueObject(SerializeRegion(R))));
	}
	Root->SetArrayField(TEXT("regions"), RegArr);

	// Global Entities
	TArray<TSharedPtr<FJsonValue>> GEArr;
	for (auto& E : MapData.GlobalEntities)
	{
		GEArr.Add(MakeShareable(new FJsonValueObject(SerializeGlobalEntity(E))));
	}
	Root->SetArrayField(TEXT("global_entities"), GEArr);

	// Environment
	Root->SetObjectField(TEXT("environment"), SerializeEnvironment(MapData.Environment));

	// Global Stories
	TArray<TSharedPtr<FJsonValue>> GSArr;
	for (auto& St : MapData.GlobalStories)
	{
		GSArr.Add(MakeShareable(new FJsonValueObject(SerializeStoryRef(St))));
	}
	Root->SetArrayField(TEXT("global_stories"), GSArr);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Output;
}

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
	const auto& Landscape = Region.Landscape;

	// Generate or load heightmap data
	TArray<uint16> HeightData;

	if (!Landscape.HeightmapPath.IsEmpty())
	{
		// Load from file
		TArray<uint8> RawData;
		if (FFileHelper::LoadFileToArray(RawData, *Landscape.HeightmapPath))
		{
			HeightData.SetNumUninitialized(RawData.Num() / 2);
			FMemory::Memcpy(HeightData.GetData(), RawData.GetData(), RawData.Num());
			UE_LOG(LogHktMapGenerator, Log, TEXT("Region '%s': Loaded heightmap from %s (%d samples)"),
				*Region.Name, *Landscape.HeightmapPath, HeightData.Num());
		}
		else
		{
			UE_LOG(LogHktMapGenerator, Warning, TEXT("Region '%s': Failed to load heightmap '%s', using recipe"),
				*Region.Name, *Landscape.HeightmapPath);
		}
	}

	if (HeightData.Num() == 0)
	{
		// Generate from terrain recipe
		HeightData = FHktTerrainRecipeBuilder::GenerateHeightmap(
			Landscape.TerrainRecipe,
			Landscape.SizeX, Landscape.SizeY,
			Landscape.HeightMin, Landscape.HeightMax);
	}

	if (HeightData.Num() != Landscape.SizeX * Landscape.SizeY)
	{
		UE_LOG(LogHktMapGenerator, Error, TEXT("Region '%s': Heightmap size mismatch (%d vs %dx%d)"),
			*Region.Name, HeightData.Num(), Landscape.SizeX, Landscape.SizeY);
		return false;
	}

	// TODO: Create ALandscape from HeightData via LandscapeEditorUtils
	// This requires LandscapeEditor module. The actual creation uses:
	//   ALandscape* NewLandscape = World->SpawnActor<ALandscape>();
	//   NewLandscape->Import(...)
	// The landscape should be positioned at Region.Center
	//
	// For now, log success and track the intent
	UE_LOG(LogHktMapGenerator, Log, TEXT("Region '%s': Landscape %dx%d generated (biome=%s, %d layers)"),
		*Region.Name, Landscape.SizeX, Landscape.SizeY, *Landscape.Biome, Landscape.Layers.Num());

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
		// TODO: Resolve MeshTag → StaticMesh via ConventionPath/GeneratorRouter
		// For now, create a placeholder StaticMeshActor at the right transform
		FTransform Transform;
		Transform.SetLocation(Prop.Position);
		Transform.SetRotation(FQuat(Prop.Rotation));
		Transform.SetScale3D(Prop.Scale);

		AStaticMeshActor* PropActor = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), &Transform);

		if (PropActor)
		{
			PropActor->SetFolderPath(FName(TEXT("HktMap/Props")));
			SpawnedActors.Add(PropActor);
		}
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
	UE_LOG(LogHktMapGenerator, Log, TEXT("Applying environment — weather=%s, time=%s, fog=%.3f"),
		*Env.Weather, *Env.TimeOfDay, Env.FogDensity);

	// Find or create directional light
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		if (UDirectionalLightComponent* LightComp = It->GetComponent())
		{
			LightComp->SetLightColor(Env.SunColor);
		}
		break;  // Use first found
	}

	// TODO: Apply fog, wind, ambient VFX
	// These require finding or creating ExponentialHeightFog, WindDirectionalSource, etc.
	// Ambient VFX would be spawned via HktVFXGenerator using the AmbientVFXTags
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
