// Copyright Hkt Studios, Inc. All Rights Reserved.
// Config→Niagara 빌드용 설정 구조체 (Phase 0 단순화 버전)

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HktVFXNiagaraConfig.generated.h"

// ============================================================================
// 에미터 Spawn 설정
// ============================================================================

USTRUCT(BlueprintType)
struct HKTVFXGENERATOR_API FHktVFXEmitterSpawnConfig
{
	GENERATED_BODY()

	// "burst" 또는 "rate"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString Mode = TEXT("burst");

	// rate 모드일 때 초당 파티클 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float Rate = 0.f;

	// burst 모드일 때 파티클 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	int32 BurstCount = 0;
};

// ============================================================================
// 에미터 Init 설정
// ============================================================================

USTRUCT(BlueprintType)
struct HKTVFXGENERATOR_API FHktVFXEmitterInitConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float LifetimeMin = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float LifetimeMax = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float SizeMin = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float SizeMax = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FVector VelocityMin = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FVector VelocityMax = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FLinearColor Color = FLinearColor::White;
};

// ============================================================================
// 에미터 Update 설정
// ============================================================================

USTRUCT(BlueprintType)
struct HKTVFXGENERATOR_API FHktVFXEmitterUpdateConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FVector Gravity = FVector(0.f, 0.f, -980.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float Drag = 0.f;
};

// ============================================================================
// 에미터 Render 설정
// ============================================================================

USTRUCT(BlueprintType)
struct HKTVFXGENERATOR_API FHktVFXEmitterRenderConfig
{
	GENERATED_BODY()

	// "sprite", "ribbon", "light", "mesh"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString RendererType = TEXT("sprite");

	// "additive", "translucent"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString BlendMode = TEXT("additive");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	int32 SortOrder = 0;
};

// ============================================================================
// 단일 에미터 설정
// ============================================================================

USTRUCT(BlueprintType)
struct HKTVFXGENERATOR_API FHktVFXEmitterConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FHktVFXEmitterSpawnConfig Spawn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FHktVFXEmitterInitConfig Init;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FHktVFXEmitterUpdateConfig Update;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FHktVFXEmitterRenderConfig Render;
};

// ============================================================================
// Niagara 시스템 전체 설정
// ============================================================================

USTRUCT(BlueprintType)
struct HKTVFXGENERATOR_API FHktVFXNiagaraConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString SystemName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	TArray<FHktVFXEmitterConfig> Emitters;

	bool IsValid() const { return !SystemName.IsEmpty() && Emitters.Num() > 0; }

	// ============================================================================
	// JSON 직렬화
	// ============================================================================

	static bool FromJson(const FString& JsonString, FHktVFXNiagaraConfig& OutConfig)
	{
		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}

		OutConfig.SystemName = Root->GetStringField(TEXT("systemName"));

		const TArray<TSharedPtr<FJsonValue>>* EmittersArray;
		if (!Root->TryGetArrayField(TEXT("emitters"), EmittersArray))
		{
			return false;
		}

		for (const auto& EmitterVal : *EmittersArray)
		{
			const TSharedPtr<FJsonObject>& EmObj = EmitterVal->AsObject();
			if (!EmObj.IsValid()) continue;

			FHktVFXEmitterConfig Emitter;
			Emitter.Name = EmObj->GetStringField(TEXT("name"));

			// Spawn
			if (const TSharedPtr<FJsonObject>* SpawnObj; EmObj->TryGetObjectField(TEXT("spawn"), SpawnObj))
			{
				Emitter.Spawn.Mode = (*SpawnObj)->GetStringField(TEXT("mode"));
				(*SpawnObj)->TryGetNumberField(TEXT("rate"), Emitter.Spawn.Rate);
				int32 BurstCount = 0;
				if ((*SpawnObj)->TryGetNumberField(TEXT("burstCount"), BurstCount))
				{
					Emitter.Spawn.BurstCount = BurstCount;
				}
			}

			// Init
			if (const TSharedPtr<FJsonObject>* InitObj; EmObj->TryGetObjectField(TEXT("init"), InitObj))
			{
				(*InitObj)->TryGetNumberField(TEXT("lifetimeMin"), Emitter.Init.LifetimeMin);
				(*InitObj)->TryGetNumberField(TEXT("lifetimeMax"), Emitter.Init.LifetimeMax);
				(*InitObj)->TryGetNumberField(TEXT("sizeMin"), Emitter.Init.SizeMin);
				(*InitObj)->TryGetNumberField(TEXT("sizeMax"), Emitter.Init.SizeMax);

				if (const TSharedPtr<FJsonObject>* VelMinObj; (*InitObj)->TryGetObjectField(TEXT("velocityMin"), VelMinObj))
				{
					Emitter.Init.VelocityMin.X = (*VelMinObj)->GetNumberField(TEXT("x"));
					Emitter.Init.VelocityMin.Y = (*VelMinObj)->GetNumberField(TEXT("y"));
					Emitter.Init.VelocityMin.Z = (*VelMinObj)->GetNumberField(TEXT("z"));
				}
				if (const TSharedPtr<FJsonObject>* VelMaxObj; (*InitObj)->TryGetObjectField(TEXT("velocityMax"), VelMaxObj))
				{
					Emitter.Init.VelocityMax.X = (*VelMaxObj)->GetNumberField(TEXT("x"));
					Emitter.Init.VelocityMax.Y = (*VelMaxObj)->GetNumberField(TEXT("y"));
					Emitter.Init.VelocityMax.Z = (*VelMaxObj)->GetNumberField(TEXT("z"));
				}

				if (const TSharedPtr<FJsonObject>* ColorObj; (*InitObj)->TryGetObjectField(TEXT("color"), ColorObj))
				{
					Emitter.Init.Color.R = (*ColorObj)->GetNumberField(TEXT("r"));
					Emitter.Init.Color.G = (*ColorObj)->GetNumberField(TEXT("g"));
					Emitter.Init.Color.B = (*ColorObj)->GetNumberField(TEXT("b"));
					double A = 1.0;
					(*ColorObj)->TryGetNumberField(TEXT("a"), A);
					Emitter.Init.Color.A = A;
				}
			}

			// Update
			if (const TSharedPtr<FJsonObject>* UpdateObj; EmObj->TryGetObjectField(TEXT("update"), UpdateObj))
			{
				if (const TSharedPtr<FJsonObject>* GravObj; (*UpdateObj)->TryGetObjectField(TEXT("gravity"), GravObj))
				{
					Emitter.Update.Gravity.X = (*GravObj)->GetNumberField(TEXT("x"));
					Emitter.Update.Gravity.Y = (*GravObj)->GetNumberField(TEXT("y"));
					Emitter.Update.Gravity.Z = (*GravObj)->GetNumberField(TEXT("z"));
				}
				(*UpdateObj)->TryGetNumberField(TEXT("drag"), Emitter.Update.Drag);
			}

			// Render
			if (const TSharedPtr<FJsonObject>* RenderObj; EmObj->TryGetObjectField(TEXT("render"), RenderObj))
			{
				FString RendererType;
				if ((*RenderObj)->TryGetStringField(TEXT("rendererType"), RendererType))
				{
					Emitter.Render.RendererType = RendererType;
				}
				FString BlendMode;
				if ((*RenderObj)->TryGetStringField(TEXT("blendMode"), BlendMode))
				{
					Emitter.Render.BlendMode = BlendMode;
				}
				int32 SortOrder = 0;
				if ((*RenderObj)->TryGetNumberField(TEXT("sortOrder"), SortOrder))
				{
					Emitter.Render.SortOrder = SortOrder;
				}
			}

			OutConfig.Emitters.Add(MoveTemp(Emitter));
		}

		return OutConfig.IsValid();
	}

	FString ToJson() const
	{
		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("systemName"), SystemName);

		Writer->WriteArrayStart(TEXT("emitters"));
		for (const auto& Emitter : Emitters)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("name"), Emitter.Name);

			// Spawn
			Writer->WriteObjectStart(TEXT("spawn"));
			Writer->WriteValue(TEXT("mode"), Emitter.Spawn.Mode);
			Writer->WriteValue(TEXT("rate"), Emitter.Spawn.Rate);
			Writer->WriteValue(TEXT("burstCount"), Emitter.Spawn.BurstCount);
			Writer->WriteObjectEnd();

			// Init
			Writer->WriteObjectStart(TEXT("init"));
			Writer->WriteValue(TEXT("lifetimeMin"), Emitter.Init.LifetimeMin);
			Writer->WriteValue(TEXT("lifetimeMax"), Emitter.Init.LifetimeMax);
			Writer->WriteValue(TEXT("sizeMin"), Emitter.Init.SizeMin);
			Writer->WriteValue(TEXT("sizeMax"), Emitter.Init.SizeMax);

			Writer->WriteObjectStart(TEXT("velocityMin"));
			Writer->WriteValue(TEXT("x"), Emitter.Init.VelocityMin.X);
			Writer->WriteValue(TEXT("y"), Emitter.Init.VelocityMin.Y);
			Writer->WriteValue(TEXT("z"), Emitter.Init.VelocityMin.Z);
			Writer->WriteObjectEnd();

			Writer->WriteObjectStart(TEXT("velocityMax"));
			Writer->WriteValue(TEXT("x"), Emitter.Init.VelocityMax.X);
			Writer->WriteValue(TEXT("y"), Emitter.Init.VelocityMax.Y);
			Writer->WriteValue(TEXT("z"), Emitter.Init.VelocityMax.Z);
			Writer->WriteObjectEnd();

			Writer->WriteObjectStart(TEXT("color"));
			Writer->WriteValue(TEXT("r"), Emitter.Init.Color.R);
			Writer->WriteValue(TEXT("g"), Emitter.Init.Color.G);
			Writer->WriteValue(TEXT("b"), Emitter.Init.Color.B);
			Writer->WriteValue(TEXT("a"), Emitter.Init.Color.A);
			Writer->WriteObjectEnd();

			Writer->WriteObjectEnd(); // init

			// Update
			Writer->WriteObjectStart(TEXT("update"));
			Writer->WriteObjectStart(TEXT("gravity"));
			Writer->WriteValue(TEXT("x"), Emitter.Update.Gravity.X);
			Writer->WriteValue(TEXT("y"), Emitter.Update.Gravity.Y);
			Writer->WriteValue(TEXT("z"), Emitter.Update.Gravity.Z);
			Writer->WriteObjectEnd();
			Writer->WriteValue(TEXT("drag"), Emitter.Update.Drag);
			Writer->WriteObjectEnd();

			// Render
			Writer->WriteObjectStart(TEXT("render"));
			Writer->WriteValue(TEXT("rendererType"), Emitter.Render.RendererType);
			Writer->WriteValue(TEXT("blendMode"), Emitter.Render.BlendMode);
			Writer->WriteValue(TEXT("sortOrder"), Emitter.Render.SortOrder);
			Writer->WriteObjectEnd();

			Writer->WriteObjectEnd(); // emitter
		}
		Writer->WriteArrayEnd();

		Writer->WriteObjectEnd();
		Writer->Close();
		return Output;
	}

	static FString GetSchemaJson()
	{
		FString S;
		S += TEXT("{\n");
		S += TEXT("  \"systemName\": \"string (asset name, NS_ prefix auto-added)\",\n");
		S += TEXT("  \"emitters\": [\n");
		S += TEXT("    {\n");
		S += TEXT("      \"name\": \"string (emitter name)\",\n");
		S += TEXT("      \"spawn\": {\n");
		S += TEXT("        \"mode\": \"burst | rate\",\n");
		S += TEXT("        \"rate\": \"float (rate mode: particles per second)\",\n");
		S += TEXT("        \"burstCount\": \"int (burst mode: particles per burst)\"\n");
		S += TEXT("      },\n");
		S += TEXT("      \"init\": {\n");
		S += TEXT("        \"lifetimeMin\": \"float (min lifetime, seconds)\",\n");
		S += TEXT("        \"lifetimeMax\": \"float (max lifetime, seconds)\",\n");
		S += TEXT("        \"sizeMin\": \"float (min size, UE units)\",\n");
		S += TEXT("        \"sizeMax\": \"float (max size, UE units)\",\n");
		S += TEXT("        \"velocityMin\": { \"x\": \"float\", \"y\": \"float\", \"z\": \"float\" },\n");
		S += TEXT("        \"velocityMax\": { \"x\": \"float\", \"y\": \"float\", \"z\": \"float\" },\n");
		S += TEXT("        \"color\": { \"r\": \"float 0-1\", \"g\": \"float 0-1\", \"b\": \"float 0-1\", \"a\": \"float 0-1\" }\n");
		S += TEXT("      },\n");
		S += TEXT("      \"update\": {\n");
		S += TEXT("        \"gravity\": { \"x\": \"float\", \"y\": \"float\", \"z\": \"float (default -980)\" },\n");
		S += TEXT("        \"drag\": \"float (0 = none)\"\n");
		S += TEXT("      },\n");
		S += TEXT("      \"render\": {\n");
		S += TEXT("        \"rendererType\": \"sprite | ribbon | light | mesh\",\n");
		S += TEXT("        \"blendMode\": \"additive | translucent\",\n");
		S += TEXT("        \"sortOrder\": \"int\"\n");
		S += TEXT("      }\n");
		S += TEXT("    }\n");
		S += TEXT("  ]\n");
		S += TEXT("}");
		return S;
	}
};
