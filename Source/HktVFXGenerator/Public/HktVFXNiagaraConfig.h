// Copyright Hkt Studios, Inc. All Rights Reserved.
// Config->Niagara 빌드용 설정 구조체

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HktVFXNiagaraConfig.generated.h"

// ============================================================================
// 데이터 인터페이스 바인딩 설정
// Niagara User Parameter로 노출되어 런타임에 외부 객체(스켈레톤 등)를 주입.
// ============================================================================

USTRUCT(BlueprintType)
struct HKTVFXGENERATOR_API FHktVFXDataInterfaceBinding
{
	GENERATED_BODY()

	/**
	 * 데이터 인터페이스 타입:
	 * "skeletal_mesh" — UNiagaraDataInterfaceSkeletalMesh (캐릭터 메시 표면 스폰)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString Type;

	/** User Parameter 이름 (에미터/모듈에서 참조하는 이름) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString ParameterName;

	/**
	 * 스폰 소스 모드 (skeletal_mesh 전용):
	 * "surface"  — 메시 표면에서 파티클 스폰
	 * "vertex"   — 버텍스 위치에서 스폰
	 * "bone"     — 본 위치에서 스폰
	 * "socket"   — 소켓 위치에서 스폰
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString SpawnSource = TEXT("surface");

	/** 특정 본/소켓 필터 (비어있으면 전체) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	TArray<FString> FilterNames;
};

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

	// burst 발생 지연 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float BurstDelay = 0.f;
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

	// 초기 스프라이트 회전 (도 단위)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float SpriteRotationMin = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float SpriteRotationMax = 0.f;

	// 초기 질량 (Force 계산용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float MassMin = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float MassMax = 1.f;
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

	// 회전 속도 (도/초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float RotationRateMin = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float RotationRateMax = 0.f;

	// 수명 기반 크기 스케일 (1.0 = 변화없음)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float SizeScaleStart = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float SizeScaleEnd = 1.f;

	// 수명 기반 투명도 (1.0 = 불투명)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float OpacityStart = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float OpacityEnd = 0.f;

	// 수명 기반 컬러 보간 (bUseColorOverLife가 true일 때만)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	bool bUseColorOverLife = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FLinearColor ColorEnd = FLinearColor::Black;

	// Curl Noise 터뷸런스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float NoiseStrength = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float NoiseFrequency = 1.f;

	// Point Attractor (0이면 미사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float AttractionStrength = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float AttractionRadius = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FVector AttractionPosition = FVector::ZeroVector;

	// Vortex (0이면 미사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float VortexStrength = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float VortexRadius = 100.f;

	// 방향성 바람 (0이면 미사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FVector WindForce = FVector::ZeroVector;

	// 일정 가속도 (Gravity와 독립, 0이면 미사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FVector AccelerationForce = FVector::ZeroVector;

	// Vortex 축 벡터 (기본 Z축, 0이면 미사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FVector VortexAxis = FVector(0.f, 0.f, 1.f);

	// 속도 제한 (0이면 미사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float SpeedLimit = 0.f;
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

	/**
	 * 에미터 템플릿 키 (Settings의 EmitterTemplates 맵에서 조회).
	 * 예: "spark", "smoke", "explosion", "debris", "impact", "flame", "flare", "arc"
	 * 비어있으면 RendererType으로 폴백.
	 * NiagaraExamples의 NE_ 에미터들이 이미 적절한 모듈(Gravity, Drag, Noise 등)과
	 * 머티리얼/텍스처를 갖고 있으므로 이걸 쓰면 훨씬 풍부한 결과를 얻음.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString EmitterTemplate;

	/**
	 * 머티리얼 에셋 경로 오버라이드.
	 * 예: "/Game/NiagaraExamples/Materials/MI_Sparks"
	 * 비어있으면 템플릿의 기본 머티리얼 사용.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString MaterialPath;

	// "additive", "translucent"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString BlendMode = TEXT("additive");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	int32 SortOrder = 0;

	// "unaligned", "velocity_aligned" (sprite only)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString Alignment = TEXT("unaligned");

	// Light renderer
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float LightRadiusScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float LightIntensity = 1.f;

	// Ribbon renderer
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float RibbonWidth = 10.f;

	// SubUV 플립북 (0이면 사용 안함)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	int32 SubImageRows = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	int32 SubImageColumns = 0;

	// =========================================================================
	// 텍스처 생성 요청 (외부 SD/이미지 생성 도구용)
	// LLM이 커스텀 텍스처가 필요하다고 판단하면 SD 프롬프트를 포함.
	// Builder는 이 정보를 응답에 포함시키고, 외부 도구가 실제 생성 수행.
	// =========================================================================

	/** Stable Diffusion 텍스처 생성 프롬프트 (비어있으면 생성 안함) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString TexturePrompt;

	/** SD 네거티브 프롬프트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString TextureNegativePrompt;

	/**
	 * 텍스처 타입:
	 * "particle_sprite" — 단일 파티클 스프라이트 (원형, 별, 불꽃 등)
	 * "flipbook_4x4"   — 4x4 SubUV 시퀀스 (폭발, 연기 애니메이션 등)
	 * "flipbook_8x8"   — 8x8 SubUV 시퀀스
	 * "noise"          — 타일 가능한 노이즈 텍스처
	 * "gradient"       — 그라디언트 램프 텍스처
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString TextureType;

	/** 텍스처 해상도 (0=생성 안함, 128/256/512/1024) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	int32 TextureResolution = 0;
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

	/** 이 에미터가 사용하는 데이터 인터페이스 바인딩 목록 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	TArray<FHktVFXDataInterfaceBinding> DataInterfaces;
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

	// 시스템 프리웜 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float WarmupTime = 0.f;

	// 시스템 루프 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	bool bLooping = false;

	bool IsValid() const { return !SystemName.IsEmpty() && Emitters.Num() > 0; }

	// ============================================================================
	// JSON 직렬화
	// ============================================================================

	static FVector ParseJsonVector(const TSharedPtr<FJsonObject>& Obj)
	{
		return FVector(
			Obj->GetNumberField(TEXT("x")),
			Obj->GetNumberField(TEXT("y")),
			Obj->GetNumberField(TEXT("z")));
	}

	static FLinearColor ParseJsonColor(const TSharedPtr<FJsonObject>& Obj)
	{
		double A = 1.0;
		Obj->TryGetNumberField(TEXT("a"), A);
		return FLinearColor(
			Obj->GetNumberField(TEXT("r")),
			Obj->GetNumberField(TEXT("g")),
			Obj->GetNumberField(TEXT("b")),
			A);
	}

	static bool FromJson(const FString& JsonString, FHktVFXNiagaraConfig& OutConfig)
	{
		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}

		OutConfig.SystemName = Root->GetStringField(TEXT("systemName"));
		Root->TryGetNumberField(TEXT("warmupTime"), OutConfig.WarmupTime);
		Root->TryGetBoolField(TEXT("looping"), OutConfig.bLooping);

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
				(*SpawnObj)->TryGetStringField(TEXT("mode"), Emitter.Spawn.Mode);
				(*SpawnObj)->TryGetNumberField(TEXT("rate"), Emitter.Spawn.Rate);
				int32 BC = 0;
				if ((*SpawnObj)->TryGetNumberField(TEXT("burstCount"), BC))
					Emitter.Spawn.BurstCount = BC;
				(*SpawnObj)->TryGetNumberField(TEXT("burstDelay"), Emitter.Spawn.BurstDelay);
			}

			// Init
			if (const TSharedPtr<FJsonObject>* InitObj; EmObj->TryGetObjectField(TEXT("init"), InitObj))
			{
				(*InitObj)->TryGetNumberField(TEXT("lifetimeMin"), Emitter.Init.LifetimeMin);
				(*InitObj)->TryGetNumberField(TEXT("lifetimeMax"), Emitter.Init.LifetimeMax);
				(*InitObj)->TryGetNumberField(TEXT("sizeMin"), Emitter.Init.SizeMin);
				(*InitObj)->TryGetNumberField(TEXT("sizeMax"), Emitter.Init.SizeMax);
				(*InitObj)->TryGetNumberField(TEXT("spriteRotationMin"), Emitter.Init.SpriteRotationMin);
				(*InitObj)->TryGetNumberField(TEXT("spriteRotationMax"), Emitter.Init.SpriteRotationMax);
				(*InitObj)->TryGetNumberField(TEXT("massMin"), Emitter.Init.MassMin);
				(*InitObj)->TryGetNumberField(TEXT("massMax"), Emitter.Init.MassMax);

				if (const TSharedPtr<FJsonObject>* V; (*InitObj)->TryGetObjectField(TEXT("velocityMin"), V))
					Emitter.Init.VelocityMin = ParseJsonVector(*V);
				if (const TSharedPtr<FJsonObject>* V; (*InitObj)->TryGetObjectField(TEXT("velocityMax"), V))
					Emitter.Init.VelocityMax = ParseJsonVector(*V);
				if (const TSharedPtr<FJsonObject>* C; (*InitObj)->TryGetObjectField(TEXT("color"), C))
					Emitter.Init.Color = ParseJsonColor(*C);
			}

			// Update
			if (const TSharedPtr<FJsonObject>* UpdObj; EmObj->TryGetObjectField(TEXT("update"), UpdObj))
			{
				if (const TSharedPtr<FJsonObject>* G; (*UpdObj)->TryGetObjectField(TEXT("gravity"), G))
					Emitter.Update.Gravity = ParseJsonVector(*G);
				(*UpdObj)->TryGetNumberField(TEXT("drag"), Emitter.Update.Drag);
				(*UpdObj)->TryGetNumberField(TEXT("rotationRateMin"), Emitter.Update.RotationRateMin);
				(*UpdObj)->TryGetNumberField(TEXT("rotationRateMax"), Emitter.Update.RotationRateMax);
				(*UpdObj)->TryGetNumberField(TEXT("sizeScaleStart"), Emitter.Update.SizeScaleStart);
				(*UpdObj)->TryGetNumberField(TEXT("sizeScaleEnd"), Emitter.Update.SizeScaleEnd);
				(*UpdObj)->TryGetNumberField(TEXT("opacityStart"), Emitter.Update.OpacityStart);
				(*UpdObj)->TryGetNumberField(TEXT("opacityEnd"), Emitter.Update.OpacityEnd);
				(*UpdObj)->TryGetBoolField(TEXT("useColorOverLife"), Emitter.Update.bUseColorOverLife);
				if (const TSharedPtr<FJsonObject>* C; (*UpdObj)->TryGetObjectField(TEXT("colorEnd"), C))
					Emitter.Update.ColorEnd = ParseJsonColor(*C);
				(*UpdObj)->TryGetNumberField(TEXT("noiseStrength"), Emitter.Update.NoiseStrength);
				(*UpdObj)->TryGetNumberField(TEXT("noiseFrequency"), Emitter.Update.NoiseFrequency);
				(*UpdObj)->TryGetNumberField(TEXT("attractionStrength"), Emitter.Update.AttractionStrength);
				(*UpdObj)->TryGetNumberField(TEXT("attractionRadius"), Emitter.Update.AttractionRadius);
				if (const TSharedPtr<FJsonObject>* P; (*UpdObj)->TryGetObjectField(TEXT("attractionPosition"), P))
					Emitter.Update.AttractionPosition = ParseJsonVector(*P);
				(*UpdObj)->TryGetNumberField(TEXT("vortexStrength"), Emitter.Update.VortexStrength);
				(*UpdObj)->TryGetNumberField(TEXT("vortexRadius"), Emitter.Update.VortexRadius);
				if (const TSharedPtr<FJsonObject>* W; (*UpdObj)->TryGetObjectField(TEXT("windForce"), W))
					Emitter.Update.WindForce = ParseJsonVector(*W);
				if (const TSharedPtr<FJsonObject>* A; (*UpdObj)->TryGetObjectField(TEXT("accelerationForce"), A))
					Emitter.Update.AccelerationForce = ParseJsonVector(*A);
				if (const TSharedPtr<FJsonObject>* VA; (*UpdObj)->TryGetObjectField(TEXT("vortexAxis"), VA))
					Emitter.Update.VortexAxis = ParseJsonVector(*VA);
				(*UpdObj)->TryGetNumberField(TEXT("speedLimit"), Emitter.Update.SpeedLimit);
			}

			// Render
			if (const TSharedPtr<FJsonObject>* RenObj; EmObj->TryGetObjectField(TEXT("render"), RenObj))
			{
				(*RenObj)->TryGetStringField(TEXT("rendererType"), Emitter.Render.RendererType);
				(*RenObj)->TryGetStringField(TEXT("emitterTemplate"), Emitter.Render.EmitterTemplate);
				(*RenObj)->TryGetStringField(TEXT("materialPath"), Emitter.Render.MaterialPath);
				(*RenObj)->TryGetStringField(TEXT("blendMode"), Emitter.Render.BlendMode);
				int32 SO = 0;
				if ((*RenObj)->TryGetNumberField(TEXT("sortOrder"), SO))
					Emitter.Render.SortOrder = SO;
				(*RenObj)->TryGetStringField(TEXT("alignment"), Emitter.Render.Alignment);
				(*RenObj)->TryGetNumberField(TEXT("lightRadiusScale"), Emitter.Render.LightRadiusScale);
				(*RenObj)->TryGetNumberField(TEXT("lightIntensity"), Emitter.Render.LightIntensity);
				(*RenObj)->TryGetNumberField(TEXT("ribbonWidth"), Emitter.Render.RibbonWidth);
				int32 SubRows = 0, SubCols = 0;
				if ((*RenObj)->TryGetNumberField(TEXT("subImageRows"), SubRows))
					Emitter.Render.SubImageRows = SubRows;
				if ((*RenObj)->TryGetNumberField(TEXT("subImageColumns"), SubCols))
					Emitter.Render.SubImageColumns = SubCols;
				// Texture generation
				(*RenObj)->TryGetStringField(TEXT("texturePrompt"), Emitter.Render.TexturePrompt);
				(*RenObj)->TryGetStringField(TEXT("textureNegativePrompt"), Emitter.Render.TextureNegativePrompt);
				(*RenObj)->TryGetStringField(TEXT("textureType"), Emitter.Render.TextureType);
				int32 TexRes = 0;
				if ((*RenObj)->TryGetNumberField(TEXT("textureResolution"), TexRes))
					Emitter.Render.TextureResolution = TexRes;
			}

			// DataInterfaces
			const TArray<TSharedPtr<FJsonValue>>* DIArray;
			if (EmObj->TryGetArrayField(TEXT("dataInterfaces"), DIArray))
			{
				for (const auto& DIVal : *DIArray)
				{
					const TSharedPtr<FJsonObject>& DIObj = DIVal->AsObject();
					if (!DIObj.IsValid()) continue;

					FHktVFXDataInterfaceBinding DI;
					DIObj->TryGetStringField(TEXT("type"), DI.Type);
					DIObj->TryGetStringField(TEXT("parameterName"), DI.ParameterName);
					DIObj->TryGetStringField(TEXT("spawnSource"), DI.SpawnSource);

					const TArray<TSharedPtr<FJsonValue>>* FilterArray;
					if (DIObj->TryGetArrayField(TEXT("filterNames"), FilterArray))
					{
						for (const auto& F : *FilterArray)
						{
							DI.FilterNames.Add(F->AsString());
						}
					}

					Emitter.DataInterfaces.Add(MoveTemp(DI));
				}
			}

			OutConfig.Emitters.Add(MoveTemp(Emitter));
		}

		return OutConfig.IsValid();
	}

	FString ToJson() const
	{
		FString Output;
		TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Output);
		W->WriteObjectStart();
		W->WriteValue(TEXT("systemName"), SystemName);
		W->WriteValue(TEXT("warmupTime"), WarmupTime);
		W->WriteValue(TEXT("looping"), bLooping);

		W->WriteArrayStart(TEXT("emitters"));
		for (const auto& E : Emitters)
		{
			W->WriteObjectStart();
			W->WriteValue(TEXT("name"), E.Name);

			// Spawn
			W->WriteObjectStart(TEXT("spawn"));
			W->WriteValue(TEXT("mode"), E.Spawn.Mode);
			W->WriteValue(TEXT("rate"), E.Spawn.Rate);
			W->WriteValue(TEXT("burstCount"), E.Spawn.BurstCount);
			W->WriteValue(TEXT("burstDelay"), E.Spawn.BurstDelay);
			W->WriteObjectEnd();

			// Init
			W->WriteObjectStart(TEXT("init"));
			W->WriteValue(TEXT("lifetimeMin"), E.Init.LifetimeMin);
			W->WriteValue(TEXT("lifetimeMax"), E.Init.LifetimeMax);
			W->WriteValue(TEXT("sizeMin"), E.Init.SizeMin);
			W->WriteValue(TEXT("sizeMax"), E.Init.SizeMax);
			W->WriteValue(TEXT("spriteRotationMin"), E.Init.SpriteRotationMin);
			W->WriteValue(TEXT("spriteRotationMax"), E.Init.SpriteRotationMax);
			W->WriteValue(TEXT("massMin"), E.Init.MassMin);
			W->WriteValue(TEXT("massMax"), E.Init.MassMax);

			W->WriteObjectStart(TEXT("velocityMin"));
			W->WriteValue(TEXT("x"), E.Init.VelocityMin.X);
			W->WriteValue(TEXT("y"), E.Init.VelocityMin.Y);
			W->WriteValue(TEXT("z"), E.Init.VelocityMin.Z);
			W->WriteObjectEnd();
			W->WriteObjectStart(TEXT("velocityMax"));
			W->WriteValue(TEXT("x"), E.Init.VelocityMax.X);
			W->WriteValue(TEXT("y"), E.Init.VelocityMax.Y);
			W->WriteValue(TEXT("z"), E.Init.VelocityMax.Z);
			W->WriteObjectEnd();

			W->WriteObjectStart(TEXT("color"));
			W->WriteValue(TEXT("r"), E.Init.Color.R);
			W->WriteValue(TEXT("g"), E.Init.Color.G);
			W->WriteValue(TEXT("b"), E.Init.Color.B);
			W->WriteValue(TEXT("a"), E.Init.Color.A);
			W->WriteObjectEnd();
			W->WriteObjectEnd(); // init

			// Update
			W->WriteObjectStart(TEXT("update"));
			W->WriteObjectStart(TEXT("gravity"));
			W->WriteValue(TEXT("x"), E.Update.Gravity.X);
			W->WriteValue(TEXT("y"), E.Update.Gravity.Y);
			W->WriteValue(TEXT("z"), E.Update.Gravity.Z);
			W->WriteObjectEnd();
			W->WriteValue(TEXT("drag"), E.Update.Drag);
			W->WriteValue(TEXT("rotationRateMin"), E.Update.RotationRateMin);
			W->WriteValue(TEXT("rotationRateMax"), E.Update.RotationRateMax);
			W->WriteValue(TEXT("sizeScaleStart"), E.Update.SizeScaleStart);
			W->WriteValue(TEXT("sizeScaleEnd"), E.Update.SizeScaleEnd);
			W->WriteValue(TEXT("opacityStart"), E.Update.OpacityStart);
			W->WriteValue(TEXT("opacityEnd"), E.Update.OpacityEnd);
			W->WriteValue(TEXT("useColorOverLife"), E.Update.bUseColorOverLife);
			W->WriteObjectStart(TEXT("colorEnd"));
			W->WriteValue(TEXT("r"), E.Update.ColorEnd.R);
			W->WriteValue(TEXT("g"), E.Update.ColorEnd.G);
			W->WriteValue(TEXT("b"), E.Update.ColorEnd.B);
			W->WriteValue(TEXT("a"), E.Update.ColorEnd.A);
			W->WriteObjectEnd();
			W->WriteValue(TEXT("noiseStrength"), E.Update.NoiseStrength);
			W->WriteValue(TEXT("noiseFrequency"), E.Update.NoiseFrequency);
			W->WriteValue(TEXT("attractionStrength"), E.Update.AttractionStrength);
			W->WriteValue(TEXT("attractionRadius"), E.Update.AttractionRadius);
			W->WriteObjectStart(TEXT("attractionPosition"));
			W->WriteValue(TEXT("x"), E.Update.AttractionPosition.X);
			W->WriteValue(TEXT("y"), E.Update.AttractionPosition.Y);
			W->WriteValue(TEXT("z"), E.Update.AttractionPosition.Z);
			W->WriteObjectEnd();
			W->WriteObjectStart(TEXT("windForce"));
			W->WriteValue(TEXT("x"), E.Update.WindForce.X);
			W->WriteValue(TEXT("y"), E.Update.WindForce.Y);
			W->WriteValue(TEXT("z"), E.Update.WindForce.Z);
			W->WriteObjectEnd();
			if (!E.Update.AccelerationForce.IsNearlyZero(1.f))
			{
				W->WriteObjectStart(TEXT("accelerationForce"));
				W->WriteValue(TEXT("x"), E.Update.AccelerationForce.X);
				W->WriteValue(TEXT("y"), E.Update.AccelerationForce.Y);
				W->WriteValue(TEXT("z"), E.Update.AccelerationForce.Z);
				W->WriteObjectEnd();
			}
			W->WriteValue(TEXT("vortexStrength"), E.Update.VortexStrength);
			W->WriteValue(TEXT("vortexRadius"), E.Update.VortexRadius);
			if (!(E.Update.VortexAxis - FVector(0,0,1)).IsNearlyZero(0.01f))
			{
				W->WriteObjectStart(TEXT("vortexAxis"));
				W->WriteValue(TEXT("x"), E.Update.VortexAxis.X);
				W->WriteValue(TEXT("y"), E.Update.VortexAxis.Y);
				W->WriteValue(TEXT("z"), E.Update.VortexAxis.Z);
				W->WriteObjectEnd();
			}
			W->WriteValue(TEXT("speedLimit"), E.Update.SpeedLimit);
			W->WriteObjectEnd(); // update

			// Render
			W->WriteObjectStart(TEXT("render"));
			W->WriteValue(TEXT("rendererType"), E.Render.RendererType);
			if (!E.Render.EmitterTemplate.IsEmpty())
				W->WriteValue(TEXT("emitterTemplate"), E.Render.EmitterTemplate);
			if (!E.Render.MaterialPath.IsEmpty())
				W->WriteValue(TEXT("materialPath"), E.Render.MaterialPath);
			W->WriteValue(TEXT("blendMode"), E.Render.BlendMode);
			W->WriteValue(TEXT("sortOrder"), E.Render.SortOrder);
			W->WriteValue(TEXT("alignment"), E.Render.Alignment);
			W->WriteValue(TEXT("lightRadiusScale"), E.Render.LightRadiusScale);
			W->WriteValue(TEXT("lightIntensity"), E.Render.LightIntensity);
			W->WriteValue(TEXT("ribbonWidth"), E.Render.RibbonWidth);
			if (E.Render.SubImageRows > 0)
				W->WriteValue(TEXT("subImageRows"), E.Render.SubImageRows);
			if (E.Render.SubImageColumns > 0)
				W->WriteValue(TEXT("subImageColumns"), E.Render.SubImageColumns);
			if (!E.Render.TexturePrompt.IsEmpty())
			{
				W->WriteValue(TEXT("texturePrompt"), E.Render.TexturePrompt);
				if (!E.Render.TextureNegativePrompt.IsEmpty())
					W->WriteValue(TEXT("textureNegativePrompt"), E.Render.TextureNegativePrompt);
				W->WriteValue(TEXT("textureType"), E.Render.TextureType);
				W->WriteValue(TEXT("textureResolution"), E.Render.TextureResolution);
			}
			W->WriteObjectEnd();

			// DataInterfaces
			if (E.DataInterfaces.Num() > 0)
			{
				W->WriteArrayStart(TEXT("dataInterfaces"));
				for (const auto& DI : E.DataInterfaces)
				{
					W->WriteObjectStart();
					W->WriteValue(TEXT("type"), DI.Type);
					W->WriteValue(TEXT("parameterName"), DI.ParameterName);
					W->WriteValue(TEXT("spawnSource"), DI.SpawnSource);
					if (DI.FilterNames.Num() > 0)
					{
						W->WriteArrayStart(TEXT("filterNames"));
						for (const auto& F : DI.FilterNames)
						{
							W->WriteValue(F);
						}
						W->WriteArrayEnd();
					}
					W->WriteObjectEnd();
				}
				W->WriteArrayEnd();
			}

			W->WriteObjectEnd(); // emitter
		}
		W->WriteArrayEnd();

		W->WriteObjectEnd();
		W->Close();
		return Output;
	}

	static FString GetSchemaJson()
	{
		FString S;
		S += TEXT("{\n");
		S += TEXT("  \"systemName\": \"string\",\n");
		S += TEXT("  \"warmupTime\": \"float (pre-warm seconds, 0=none)\",\n");
		S += TEXT("  \"looping\": \"bool (default false)\",\n");
		S += TEXT("  \"emitters\": [\n");
		S += TEXT("    {\n");
		S += TEXT("      \"name\": \"string\",\n");
		S += TEXT("      \"spawn\": {\n");
		S += TEXT("        \"mode\": \"burst | rate\",\n");
		S += TEXT("        \"rate\": \"float (particles/sec)\",\n");
		S += TEXT("        \"burstCount\": \"int\",\n");
		S += TEXT("        \"burstDelay\": \"float (delay seconds)\"\n");
		S += TEXT("      },\n");
		S += TEXT("      \"init\": {\n");
		S += TEXT("        \"lifetimeMin\": \"float\", \"lifetimeMax\": \"float\",\n");
		S += TEXT("        \"sizeMin\": \"float\", \"sizeMax\": \"float\",\n");
		S += TEXT("        \"spriteRotationMin\": \"float (degrees)\", \"spriteRotationMax\": \"float\",\n");
		S += TEXT("        \"massMin\": \"float\", \"massMax\": \"float\",\n");
		S += TEXT("        \"velocityMin\": {\"x\":0,\"y\":0,\"z\":0},\n");
		S += TEXT("        \"velocityMax\": {\"x\":0,\"y\":0,\"z\":0},\n");
		S += TEXT("        \"color\": {\"r\":1,\"g\":1,\"b\":1,\"a\":1}\n");
		S += TEXT("      },\n");
		S += TEXT("      \"update\": {\n");
		S += TEXT("        \"gravity\": {\"x\":0,\"y\":0,\"z\":-980},\n");
		S += TEXT("        \"drag\": \"float\",\n");
		S += TEXT("        \"rotationRateMin\": \"float (deg/sec)\", \"rotationRateMax\": \"float\",\n");
		S += TEXT("        \"sizeScaleStart\": \"float (1.0=no change)\", \"sizeScaleEnd\": \"float\",\n");
		S += TEXT("        \"opacityStart\": \"float (0-1)\", \"opacityEnd\": \"float\",\n");
		S += TEXT("        \"useColorOverLife\": \"bool\",\n");
		S += TEXT("        \"colorEnd\": {\"r\":0,\"g\":0,\"b\":0,\"a\":1},\n");
		S += TEXT("        \"noiseStrength\": \"float (0=none)\", \"noiseFrequency\": \"float\",\n");
		S += TEXT("        \"attractionStrength\": \"float (0=none)\", \"attractionRadius\": \"float\",\n");
		S += TEXT("        \"attractionPosition\": {\"x\":0,\"y\":0,\"z\":0},\n");
		S += TEXT("        \"vortexStrength\": \"float (0=none)\", \"vortexRadius\": \"float\",\n");
		S += TEXT("        \"vortexAxis\": {\"x\":0,\"y\":0,\"z\":1} (rotation axis, default Z-up),\n");
		S += TEXT("        \"windForce\": {\"x\":0,\"y\":0,\"z\":0},\n");
		S += TEXT("        \"accelerationForce\": {\"x\":0,\"y\":0,\"z\":0} (constant accel, independent from gravity),\n");
		S += TEXT("        \"speedLimit\": \"float (0=no limit)\"\n");
		S += TEXT("      },\n");
		S += TEXT("      \"render\": {\n");
		S += TEXT("        \"rendererType\": \"sprite | ribbon | light | mesh\",\n");
		S += TEXT("        \"emitterTemplate\": \"spark | smoke | explosion | debris | impact | flame | flare | arc | dust | muzzle_flash (optional, overrides rendererType template)\",\n");
		S += TEXT("        \"materialPath\": \"/Game/NiagaraExamples/Materials/MI_xxx (optional, overrides default material)\",\n");
		S += TEXT("        \"blendMode\": \"additive | translucent\",\n");
		S += TEXT("        \"sortOrder\": \"int\",\n");
		S += TEXT("        \"alignment\": \"unaligned | velocity_aligned\",\n");
		S += TEXT("        \"lightRadiusScale\": \"float (light only)\",\n");
		S += TEXT("        \"lightIntensity\": \"float (light only)\",\n");
		S += TEXT("        \"ribbonWidth\": \"float (ribbon only)\",\n");
		S += TEXT("        \"subImageRows\": \"int (flipbook rows, 0=none)\",\n");
		S += TEXT("        \"subImageColumns\": \"int (flipbook cols, 0=none)\",\n");
		S += TEXT("        \"texturePrompt\": \"string (SD prompt for custom texture, optional)\",\n");
		S += TEXT("        \"textureNegativePrompt\": \"string (SD negative prompt, optional)\",\n");
		S += TEXT("        \"textureType\": \"particle_sprite | flipbook_4x4 | flipbook_8x8 | noise | gradient\",\n");
		S += TEXT("        \"textureResolution\": \"int (0=none, 128/256/512/1024)\"\n");
		S += TEXT("      },\n");
		S += TEXT("      \"dataInterfaces\": [\n");
		S += TEXT("        {\n");
		S += TEXT("          \"type\": \"skeletal_mesh\",\n");
		S += TEXT("          \"parameterName\": \"string (User Parameter name for runtime binding)\",\n");
		S += TEXT("          \"spawnSource\": \"surface | vertex | bone | socket (skeletal_mesh only)\",\n");
		S += TEXT("          \"filterNames\": [\"bone1\", \"bone2\"] (optional bone/socket filter)\n");
		S += TEXT("        }\n");
		S += TEXT("      ]\n");
		S += TEXT("    }\n");
		S += TEXT("  ]\n");
		S += TEXT("}");
		return S;
	}
};
