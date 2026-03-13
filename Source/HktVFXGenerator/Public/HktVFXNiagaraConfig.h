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
	 * "spline"        — UNiagaraDataInterfaceSpline (스플라인 경로를 따라 스폰)
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
// Shape Location 설정 (파티클 방출 형태)
// InitializeParticle의 ShapeLocation 모듈로 주입하여
// 파티클 스폰 위치를 구/박스/콘/링/토러스 등의 형태로 분포.
// ============================================================================

USTRUCT(BlueprintType)
struct HKTVFXGENERATOR_API FHktVFXShapeLocationConfig
{
	GENERATED_BODY()

	/**
	 * 방출 형태:
	 * "sphere"   — 구형 방출 (폭발, 마법 오라)
	 * "box"      — 박스형 방출 (환경 파티클, 비)
	 * "cylinder" — 원기둥형 (기둥 이펙트, 포탈)
	 * "cone"     — 콘형 (머즐 플래시, 분수)
	 * "ring"     — 링형 (충격파, 소용돌이)
	 * "torus"    — 토러스형 (도넛 형태)
	 * "plane"    — 평면형 (바닥 이펙트)
	 * 비어있으면 ShapeLocation 미사용 (포인트 스폰)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString Shape;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float SphereRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FVector BoxSize = FVector(100.f, 100.f, 100.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float CylinderHeight = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float CylinderRadius = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float ConeAngle = 45.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float ConeLength = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float RingRadius = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float RingWidth = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float TorusRadius = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float TorusSectionRadius = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FVector Offset = FVector::ZeroVector;

	/** true면 표면에서만 스폰, false면 볼륨 내부에서도 스폰 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	bool bSurfaceOnly = false;

	bool IsEnabled() const { return !Shape.IsEmpty(); }
};

// ============================================================================
// Collision 설정 (바닥/벽 충돌)
// ============================================================================

USTRUCT(BlueprintType)
struct HKTVFXGENERATOR_API FHktVFXCollisionConfig
{
	GENERATED_BODY()

	/**
	 * 충돌 활성화 여부. true면 Collision 모듈을 주입.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	bool bEnabled = false;

	/**
	 * 충돌 반응:
	 * "bounce" — 표면에서 바운스 (반사)
	 * "kill"   — 충돌 시 파티클 사망
	 * "stick"  — 충돌 지점에 멈춤
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString Response = TEXT("bounce");

	/** 반발 계수 (0 = 완전 비탄성, 1 = 완전 탄성) bounce 모드에서만 유효 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float Restitution = 0.5f;

	/** 마찰 계수 (0 = 마찰 없음, 1 = 최대 마찰) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float Friction = 0.2f;

	/** GPU Ray Trace 거리 (충돌 감지 깊이). 0이면 기본값 사용 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float TraceDistance = 0.f;
};

// ============================================================================
// Event-based 2차 스폰 설정
// 파티클 death/collision 이벤트 → 2차 파티클 생성
// ============================================================================

USTRUCT(BlueprintType)
struct HKTVFXGENERATOR_API FHktVFXEventSpawnConfig
{
	GENERATED_BODY()

	/**
	 * 이벤트 트리거 조건:
	 * "death"     — 파티클 사망 시
	 * "collision" — 충돌 시 (Collision 모듈 필요)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString TriggerEvent = TEXT("death");

	/** 이벤트당 생성할 2차 파티클 수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	int32 SpawnCount = 3;

	/**
	 * 2차 파티클이 참조할 에미터 이름.
	 * 같은 시스템 내의 다른 에미터 이름을 지정.
	 * 비어있으면 동일 에미터에서 자체 스폰 (GenerateLocationEvent 방식).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString TargetEmitterName;

	/** 2차 파티클 속도 스케일 (원본 파티클 속도 기준) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float VelocityScale = 0.5f;

	bool IsEnabled() const { return !TriggerEvent.IsEmpty() && SpawnCount > 0; }
};

// ============================================================================
// Spawn Per Unit 설정 (이동 거리 기반 스폰)
// ============================================================================

USTRUCT(BlueprintType)
struct HKTVFXGENERATOR_API FHktVFXSpawnPerUnitConfig
{
	GENERATED_BODY()

	/** 활성화 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	bool bEnabled = false;

	/** 거리 단위당 스폰할 파티클 수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float SpawnPerUnit = 5.f;

	/** 최대 프레임당 스폰 수 (성능 보호) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float MaxFrameSpawn = 100.f;

	/** 이동 허용 역치 (이 거리 미만의 이동은 무시) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float MovementTolerance = 0.1f;
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

	// 다중 웨이브 burst (비어있으면 단일 burst 사용)
	// 각 웨이브: {Count, Delay}
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	TArray<int32> BurstWaveCounts;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	TArray<float> BurstWaveDelays;
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

	/** 파티클 방출 형태 (Shape Location 모듈) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FHktVFXShapeLocationConfig ShapeLocation;
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

	// Color Over Life 커브 키프레임 (비어있으면 기존 2점 보간 사용)
	// 각 키프레임: {Time(0-1), Color}
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	TArray<float> ColorCurveTimes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	TArray<FLinearColor> ColorCurveValues;

	// Size Over Life 커브 키프레임 (비어있으면 기존 start→end 사용)
	// 각 키프레임: {Time(0-1), Scale}
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	TArray<float> SizeCurveTimes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	TArray<float> SizeCurveValues;

	// Camera Distance Fade (0이면 미사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float CameraDistanceFadeNear = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float CameraDistanceFadeFar = 0.f;
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

	/**
	 * 스프라이트 페이싱 모드:
	 * "default"          — 카메라를 향함 (기본)
	 * "velocity"         — 속도 방향으로 향함
	 * "camera_position"  — 카메라 위치를 향함 (원근 보정)
	 * "camera_plane"     — 카메라 평면에 평행 (UI용)
	 * "custom_axis"      — 커스텀 축 정렬
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString FacingMode = TEXT("default");

	// Light renderer
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float LightRadiusScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float LightIntensity = 1.f;

	// =========================================================================
	// Ribbon renderer
	// =========================================================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float RibbonWidth = 10.f;

	/**
	 * 리본 UV 모드:
	 * "stretch"          — 전체 리본에 텍스처 스트레치 (기본)
	 * "tile_distance"    — 이동 거리 기반 타일링
	 * "tile_lifetime"    — 수명 기반 타일링
	 * "distribute"       — 균등 분배
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString RibbonUVMode;

	/** 리본 테셀레이션 수 (세밀한 곡선, 0=기본값) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	int32 RibbonTessellation = 0;

	/** 리본 너비 스케일 커브 — 시작/끝 (0=시작, 1=끝) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float RibbonWidthScaleStart = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float RibbonWidthScaleEnd = 1.f;

	// =========================================================================
	// Mesh renderer
	// =========================================================================

	/**
	 * 메시 렌더러에 사용할 스태틱 메시 에셋 경로.
	 * 예: "/Engine/BasicShapes/Cube.Cube", "/Game/Meshes/SM_Debris_01.SM_Debris_01"
	 * 비어있으면 템플릿 기본 메시 사용.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString MeshPath;

	/**
	 * 메시 방향 모드:
	 * "velocity"  — 속도 방향으로 정렬
	 * "camera"    — 카메라를 향함
	 * "default"   — 기본 방향 (회전 없음)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FString MeshOrientation;

	// =========================================================================
	// Light renderer (확장)
	// =========================================================================

	/** 라이트 감쇠 반경 배율 (기본 1, 높을수록 넓은 범위) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float LightExponent = 1.f;

	/** 라이트가 그림자를 생성하는지 여부 (비용 높음) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	bool bLightVolumetricScattering = false;

	// =========================================================================
	// SubUV 플립북
	// =========================================================================

	/** SubUV 그리드 크기 (0이면 사용 안함) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	int32 SubImageRows = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	int32 SubImageColumns = 0;

	/** SubUV 재생 속도 (1=수명 동안 1회 재생, 2=2배속, 0=정지) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float SubUVPlayRate = 1.f;

	/** SubUV 랜덤 시작 프레임 활성화 (폭발 등에서 동일한 시작 방지) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	bool bSubUVRandomStartFrame = false;

	// =========================================================================
	// Soft Particle / Depth Fade
	// =========================================================================

	/**
	 * 소프트 파티클 활성화 — 지오메트리와의 교차면에서 부드러운 페이드.
	 * 연기, 안개 등 반투명 파티클이 바닥을 뚫고 보이는 것을 방지.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	bool bSoftParticle = false;

	/** 소프트 파티클 페이드 거리 (월드 유닛, 높을수록 부드러움) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float SoftParticleFadeDistance = 100.f;

	/**
	 * 카메라 오프셋 거리 — 파티클을 카메라 방향으로 오프셋.
	 * 양수=카메라 쪽으로, 음수=멀리. 겹침 방지에 유용.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	float CameraOffset = 0.f;

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

	/** 충돌 설정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FHktVFXCollisionConfig Collision;

	/** 이벤트 기반 2차 스폰 설정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FHktVFXEventSpawnConfig EventSpawn;

	/** 이동 거리 기반 스폰 (Spawn Per Unit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	FHktVFXSpawnPerUnitConfig SpawnPerUnit;

	/** GPU 시뮬레이션 모드 (대규모 파티클 성능 향상) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HKT|VFX")
	bool bGPUSim = false;
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
				// Multi-wave burst
				const TArray<TSharedPtr<FJsonValue>>* WavesArray;
				if ((*SpawnObj)->TryGetArrayField(TEXT("burstWaves"), WavesArray))
				{
					for (const auto& WaveVal : *WavesArray)
					{
						const TSharedPtr<FJsonObject>& WaveObj = WaveVal->AsObject();
						if (WaveObj)
						{
							int32 WC = 10; float WD = 0.f;
							WaveObj->TryGetNumberField(TEXT("count"), WC);
							WaveObj->TryGetNumberField(TEXT("delay"), WD);
							Emitter.Spawn.BurstWaveCounts.Add(WC);
							Emitter.Spawn.BurstWaveDelays.Add(WD);
						}
					}
				}
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

			// ShapeLocation (init 하위)
			if (const TSharedPtr<FJsonObject>* ShapeObj; EmObj->TryGetObjectField(TEXT("shapeLocation"), ShapeObj))
			{
				(*ShapeObj)->TryGetStringField(TEXT("shape"), Emitter.Init.ShapeLocation.Shape);
				(*ShapeObj)->TryGetNumberField(TEXT("sphereRadius"), Emitter.Init.ShapeLocation.SphereRadius);
				if (const TSharedPtr<FJsonObject>* BS; (*ShapeObj)->TryGetObjectField(TEXT("boxSize"), BS))
					Emitter.Init.ShapeLocation.BoxSize = ParseJsonVector(*BS);
				(*ShapeObj)->TryGetNumberField(TEXT("cylinderHeight"), Emitter.Init.ShapeLocation.CylinderHeight);
				(*ShapeObj)->TryGetNumberField(TEXT("cylinderRadius"), Emitter.Init.ShapeLocation.CylinderRadius);
				(*ShapeObj)->TryGetNumberField(TEXT("coneAngle"), Emitter.Init.ShapeLocation.ConeAngle);
				(*ShapeObj)->TryGetNumberField(TEXT("coneLength"), Emitter.Init.ShapeLocation.ConeLength);
				(*ShapeObj)->TryGetNumberField(TEXT("ringRadius"), Emitter.Init.ShapeLocation.RingRadius);
				(*ShapeObj)->TryGetNumberField(TEXT("ringWidth"), Emitter.Init.ShapeLocation.RingWidth);
				(*ShapeObj)->TryGetNumberField(TEXT("torusRadius"), Emitter.Init.ShapeLocation.TorusRadius);
				(*ShapeObj)->TryGetNumberField(TEXT("torusSectionRadius"), Emitter.Init.ShapeLocation.TorusSectionRadius);
				if (const TSharedPtr<FJsonObject>* Off; (*ShapeObj)->TryGetObjectField(TEXT("offset"), Off))
					Emitter.Init.ShapeLocation.Offset = ParseJsonVector(*Off);
				(*ShapeObj)->TryGetBoolField(TEXT("surfaceOnly"), Emitter.Init.ShapeLocation.bSurfaceOnly);
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
				// Color Over Life curve
				const TArray<TSharedPtr<FJsonValue>>* ColorCurveArray;
				if ((*UpdObj)->TryGetArrayField(TEXT("colorCurve"), ColorCurveArray))
				{
					for (const auto& KeyVal : *ColorCurveArray)
					{
						const TSharedPtr<FJsonObject>& KeyObj = KeyVal->AsObject();
						if (KeyObj)
						{
							float T = 0.f;
							KeyObj->TryGetNumberField(TEXT("time"), T);
							Emitter.Update.ColorCurveTimes.Add(T);
							FLinearColor C = FLinearColor::White;
							if (const TSharedPtr<FJsonObject>* CV; KeyObj->TryGetObjectField(TEXT("color"), CV))
								C = ParseJsonColor(*CV);
							Emitter.Update.ColorCurveValues.Add(C);
						}
					}
				}
				// Size Over Life curve
				const TArray<TSharedPtr<FJsonValue>>* SizeCurveArray;
				if ((*UpdObj)->TryGetArrayField(TEXT("sizeCurve"), SizeCurveArray))
				{
					for (const auto& KeyVal : *SizeCurveArray)
					{
						const TSharedPtr<FJsonObject>& KeyObj = KeyVal->AsObject();
						if (KeyObj)
						{
							float T = 0.f, S = 1.f;
							KeyObj->TryGetNumberField(TEXT("time"), T);
							KeyObj->TryGetNumberField(TEXT("scale"), S);
							Emitter.Update.SizeCurveTimes.Add(T);
							Emitter.Update.SizeCurveValues.Add(S);
						}
					}
				}
				// Camera Distance Fade
				(*UpdObj)->TryGetNumberField(TEXT("cameraDistanceFadeNear"), Emitter.Update.CameraDistanceFadeNear);
				(*UpdObj)->TryGetNumberField(TEXT("cameraDistanceFadeFar"), Emitter.Update.CameraDistanceFadeFar);
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
				(*RenObj)->TryGetStringField(TEXT("facingMode"), Emitter.Render.FacingMode);
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
				// Phase 3: Ribbon
				(*RenObj)->TryGetStringField(TEXT("ribbonUVMode"), Emitter.Render.RibbonUVMode);
				int32 RibTess = 0;
				if ((*RenObj)->TryGetNumberField(TEXT("ribbonTessellation"), RibTess))
					Emitter.Render.RibbonTessellation = RibTess;
				(*RenObj)->TryGetNumberField(TEXT("ribbonWidthScaleStart"), Emitter.Render.RibbonWidthScaleStart);
				(*RenObj)->TryGetNumberField(TEXT("ribbonWidthScaleEnd"), Emitter.Render.RibbonWidthScaleEnd);
				// Phase 3: Mesh
				(*RenObj)->TryGetStringField(TEXT("meshPath"), Emitter.Render.MeshPath);
				(*RenObj)->TryGetStringField(TEXT("meshOrientation"), Emitter.Render.MeshOrientation);
				// Phase 3: Light extended
				(*RenObj)->TryGetNumberField(TEXT("lightExponent"), Emitter.Render.LightExponent);
				(*RenObj)->TryGetBoolField(TEXT("lightVolumetricScattering"), Emitter.Render.bLightVolumetricScattering);
				// Phase 3: SubUV extended
				(*RenObj)->TryGetNumberField(TEXT("subUVPlayRate"), Emitter.Render.SubUVPlayRate);
				(*RenObj)->TryGetBoolField(TEXT("subUVRandomStartFrame"), Emitter.Render.bSubUVRandomStartFrame);
				// Phase 3: Soft Particle / Depth Fade
				(*RenObj)->TryGetBoolField(TEXT("softParticle"), Emitter.Render.bSoftParticle);
				(*RenObj)->TryGetNumberField(TEXT("softParticleFadeDistance"), Emitter.Render.SoftParticleFadeDistance);
				(*RenObj)->TryGetNumberField(TEXT("cameraOffset"), Emitter.Render.CameraOffset);
			}

			// Collision
			if (const TSharedPtr<FJsonObject>* ColObj; EmObj->TryGetObjectField(TEXT("collision"), ColObj))
			{
				(*ColObj)->TryGetBoolField(TEXT("enabled"), Emitter.Collision.bEnabled);
				(*ColObj)->TryGetStringField(TEXT("response"), Emitter.Collision.Response);
				(*ColObj)->TryGetNumberField(TEXT("restitution"), Emitter.Collision.Restitution);
				(*ColObj)->TryGetNumberField(TEXT("friction"), Emitter.Collision.Friction);
				(*ColObj)->TryGetNumberField(TEXT("traceDistance"), Emitter.Collision.TraceDistance);
			}

			// Event Spawn
			if (const TSharedPtr<FJsonObject>* EvtObj; EmObj->TryGetObjectField(TEXT("eventSpawn"), EvtObj))
			{
				(*EvtObj)->TryGetStringField(TEXT("triggerEvent"), Emitter.EventSpawn.TriggerEvent);
				int32 SC = 0;
				if ((*EvtObj)->TryGetNumberField(TEXT("spawnCount"), SC))
					Emitter.EventSpawn.SpawnCount = SC;
				(*EvtObj)->TryGetStringField(TEXT("targetEmitter"), Emitter.EventSpawn.TargetEmitterName);
				(*EvtObj)->TryGetNumberField(TEXT("velocityScale"), Emitter.EventSpawn.VelocityScale);
			}

			// Spawn Per Unit
			if (const TSharedPtr<FJsonObject>* SpuObj; EmObj->TryGetObjectField(TEXT("spawnPerUnit"), SpuObj))
			{
				(*SpuObj)->TryGetBoolField(TEXT("enabled"), Emitter.SpawnPerUnit.bEnabled);
				(*SpuObj)->TryGetNumberField(TEXT("spawnPerUnit"), Emitter.SpawnPerUnit.SpawnPerUnit);
				(*SpuObj)->TryGetNumberField(TEXT("maxFrameSpawn"), Emitter.SpawnPerUnit.MaxFrameSpawn);
				(*SpuObj)->TryGetNumberField(TEXT("movementTolerance"), Emitter.SpawnPerUnit.MovementTolerance);
			}

			// GPU Sim
			EmObj->TryGetBoolField(TEXT("gpuSim"), Emitter.bGPUSim);

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
			if (E.Spawn.BurstWaveCounts.Num() > 0)
			{
				W->WriteArrayStart(TEXT("burstWaves"));
				for (int32 i = 0; i < E.Spawn.BurstWaveCounts.Num(); ++i)
				{
					W->WriteObjectStart();
					W->WriteValue(TEXT("count"), E.Spawn.BurstWaveCounts[i]);
					W->WriteValue(TEXT("delay"), i < E.Spawn.BurstWaveDelays.Num() ? E.Spawn.BurstWaveDelays[i] : 0.f);
					W->WriteObjectEnd();
				}
				W->WriteArrayEnd();
			}
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

			// ShapeLocation
			if (E.Init.ShapeLocation.IsEnabled())
			{
				W->WriteObjectStart(TEXT("shapeLocation"));
				W->WriteValue(TEXT("shape"), E.Init.ShapeLocation.Shape);
				if (E.Init.ShapeLocation.Shape == TEXT("sphere"))
					W->WriteValue(TEXT("sphereRadius"), E.Init.ShapeLocation.SphereRadius);
				if (E.Init.ShapeLocation.Shape == TEXT("box"))
				{
					W->WriteObjectStart(TEXT("boxSize"));
					W->WriteValue(TEXT("x"), E.Init.ShapeLocation.BoxSize.X);
					W->WriteValue(TEXT("y"), E.Init.ShapeLocation.BoxSize.Y);
					W->WriteValue(TEXT("z"), E.Init.ShapeLocation.BoxSize.Z);
					W->WriteObjectEnd();
				}
				if (E.Init.ShapeLocation.Shape == TEXT("cylinder"))
				{
					W->WriteValue(TEXT("cylinderHeight"), E.Init.ShapeLocation.CylinderHeight);
					W->WriteValue(TEXT("cylinderRadius"), E.Init.ShapeLocation.CylinderRadius);
				}
				if (E.Init.ShapeLocation.Shape == TEXT("cone"))
				{
					W->WriteValue(TEXT("coneAngle"), E.Init.ShapeLocation.ConeAngle);
					W->WriteValue(TEXT("coneLength"), E.Init.ShapeLocation.ConeLength);
				}
				if (E.Init.ShapeLocation.Shape == TEXT("ring"))
				{
					W->WriteValue(TEXT("ringRadius"), E.Init.ShapeLocation.RingRadius);
					W->WriteValue(TEXT("ringWidth"), E.Init.ShapeLocation.RingWidth);
				}
				if (E.Init.ShapeLocation.Shape == TEXT("torus"))
				{
					W->WriteValue(TEXT("torusRadius"), E.Init.ShapeLocation.TorusRadius);
					W->WriteValue(TEXT("torusSectionRadius"), E.Init.ShapeLocation.TorusSectionRadius);
				}
				if (!E.Init.ShapeLocation.Offset.IsNearlyZero(1.f))
				{
					W->WriteObjectStart(TEXT("offset"));
					W->WriteValue(TEXT("x"), E.Init.ShapeLocation.Offset.X);
					W->WriteValue(TEXT("y"), E.Init.ShapeLocation.Offset.Y);
					W->WriteValue(TEXT("z"), E.Init.ShapeLocation.Offset.Z);
					W->WriteObjectEnd();
				}
				W->WriteValue(TEXT("surfaceOnly"), E.Init.ShapeLocation.bSurfaceOnly);
				W->WriteObjectEnd();
			}

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
			// Color Over Life curve
			if (E.Update.ColorCurveTimes.Num() > 0)
			{
				W->WriteArrayStart(TEXT("colorCurve"));
				for (int32 i = 0; i < E.Update.ColorCurveTimes.Num(); ++i)
				{
					W->WriteObjectStart();
					W->WriteValue(TEXT("time"), E.Update.ColorCurveTimes[i]);
					W->WriteObjectStart(TEXT("color"));
					const FLinearColor& C = i < E.Update.ColorCurveValues.Num() ? E.Update.ColorCurveValues[i] : FLinearColor::White;
					W->WriteValue(TEXT("r"), C.R);
					W->WriteValue(TEXT("g"), C.G);
					W->WriteValue(TEXT("b"), C.B);
					W->WriteValue(TEXT("a"), C.A);
					W->WriteObjectEnd();
					W->WriteObjectEnd();
				}
				W->WriteArrayEnd();
			}
			// Size Over Life curve
			if (E.Update.SizeCurveTimes.Num() > 0)
			{
				W->WriteArrayStart(TEXT("sizeCurve"));
				for (int32 i = 0; i < E.Update.SizeCurveTimes.Num(); ++i)
				{
					W->WriteObjectStart();
					W->WriteValue(TEXT("time"), E.Update.SizeCurveTimes[i]);
					W->WriteValue(TEXT("scale"), i < E.Update.SizeCurveValues.Num() ? E.Update.SizeCurveValues[i] : 1.f);
					W->WriteObjectEnd();
				}
				W->WriteArrayEnd();
			}
			// Camera Distance Fade
			if (E.Update.CameraDistanceFadeNear > 0.f || E.Update.CameraDistanceFadeFar > 0.f)
			{
				W->WriteValue(TEXT("cameraDistanceFadeNear"), E.Update.CameraDistanceFadeNear);
				W->WriteValue(TEXT("cameraDistanceFadeFar"), E.Update.CameraDistanceFadeFar);
			}
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
			if (E.Render.FacingMode != TEXT("default") && !E.Render.FacingMode.IsEmpty())
				W->WriteValue(TEXT("facingMode"), E.Render.FacingMode);
			W->WriteValue(TEXT("lightRadiusScale"), E.Render.LightRadiusScale);
			W->WriteValue(TEXT("lightIntensity"), E.Render.LightIntensity);
			W->WriteValue(TEXT("ribbonWidth"), E.Render.RibbonWidth);
			if (!E.Render.RibbonUVMode.IsEmpty())
				W->WriteValue(TEXT("ribbonUVMode"), E.Render.RibbonUVMode);
			if (E.Render.RibbonTessellation > 0)
				W->WriteValue(TEXT("ribbonTessellation"), E.Render.RibbonTessellation);
			if (E.Render.RibbonWidthScaleStart != 1.f || E.Render.RibbonWidthScaleEnd != 1.f)
			{
				W->WriteValue(TEXT("ribbonWidthScaleStart"), E.Render.RibbonWidthScaleStart);
				W->WriteValue(TEXT("ribbonWidthScaleEnd"), E.Render.RibbonWidthScaleEnd);
			}
			if (!E.Render.MeshPath.IsEmpty())
				W->WriteValue(TEXT("meshPath"), E.Render.MeshPath);
			if (!E.Render.MeshOrientation.IsEmpty())
				W->WriteValue(TEXT("meshOrientation"), E.Render.MeshOrientation);
			if (E.Render.LightExponent != 1.f)
				W->WriteValue(TEXT("lightExponent"), E.Render.LightExponent);
			if (E.Render.bLightVolumetricScattering)
				W->WriteValue(TEXT("lightVolumetricScattering"), E.Render.bLightVolumetricScattering);
			if (E.Render.SubImageRows > 0)
				W->WriteValue(TEXT("subImageRows"), E.Render.SubImageRows);
			if (E.Render.SubImageColumns > 0)
				W->WriteValue(TEXT("subImageColumns"), E.Render.SubImageColumns);
			if (E.Render.SubUVPlayRate != 1.f)
				W->WriteValue(TEXT("subUVPlayRate"), E.Render.SubUVPlayRate);
			if (E.Render.bSubUVRandomStartFrame)
				W->WriteValue(TEXT("subUVRandomStartFrame"), E.Render.bSubUVRandomStartFrame);
			if (E.Render.bSoftParticle)
			{
				W->WriteValue(TEXT("softParticle"), E.Render.bSoftParticle);
				W->WriteValue(TEXT("softParticleFadeDistance"), E.Render.SoftParticleFadeDistance);
			}
			if (E.Render.CameraOffset != 0.f)
				W->WriteValue(TEXT("cameraOffset"), E.Render.CameraOffset);
			if (!E.Render.TexturePrompt.IsEmpty())
			{
				W->WriteValue(TEXT("texturePrompt"), E.Render.TexturePrompt);
				if (!E.Render.TextureNegativePrompt.IsEmpty())
					W->WriteValue(TEXT("textureNegativePrompt"), E.Render.TextureNegativePrompt);
				W->WriteValue(TEXT("textureType"), E.Render.TextureType);
				W->WriteValue(TEXT("textureResolution"), E.Render.TextureResolution);
			}
			W->WriteObjectEnd();

			// Collision
			if (E.Collision.bEnabled)
			{
				W->WriteObjectStart(TEXT("collision"));
				W->WriteValue(TEXT("enabled"), E.Collision.bEnabled);
				W->WriteValue(TEXT("response"), E.Collision.Response);
				W->WriteValue(TEXT("restitution"), E.Collision.Restitution);
				W->WriteValue(TEXT("friction"), E.Collision.Friction);
				if (E.Collision.TraceDistance > 0.f)
					W->WriteValue(TEXT("traceDistance"), E.Collision.TraceDistance);
				W->WriteObjectEnd();
			}

			// Event Spawn
			if (E.EventSpawn.IsEnabled())
			{
				W->WriteObjectStart(TEXT("eventSpawn"));
				W->WriteValue(TEXT("triggerEvent"), E.EventSpawn.TriggerEvent);
				W->WriteValue(TEXT("spawnCount"), E.EventSpawn.SpawnCount);
				if (!E.EventSpawn.TargetEmitterName.IsEmpty())
					W->WriteValue(TEXT("targetEmitter"), E.EventSpawn.TargetEmitterName);
				W->WriteValue(TEXT("velocityScale"), E.EventSpawn.VelocityScale);
				W->WriteObjectEnd();
			}

			// Spawn Per Unit
			if (E.SpawnPerUnit.bEnabled)
			{
				W->WriteObjectStart(TEXT("spawnPerUnit"));
				W->WriteValue(TEXT("enabled"), E.SpawnPerUnit.bEnabled);
				W->WriteValue(TEXT("spawnPerUnit"), E.SpawnPerUnit.SpawnPerUnit);
				W->WriteValue(TEXT("maxFrameSpawn"), E.SpawnPerUnit.MaxFrameSpawn);
				W->WriteValue(TEXT("movementTolerance"), E.SpawnPerUnit.MovementTolerance);
				W->WriteObjectEnd();
			}

			// GPU Sim
			if (E.bGPUSim)
			{
				W->WriteValue(TEXT("gpuSim"), E.bGPUSim);
			}

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
		S += TEXT("        \"burstDelay\": \"float (delay seconds)\",\n");
		S += TEXT("        \"burstWaves\": [{\"count\":\"int\",\"delay\":\"float\"}, ...] (multi-wave burst, optional)\n");
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
		S += TEXT("      \"shapeLocation\": {\n");
		S += TEXT("        \"shape\": \"sphere | box | cylinder | cone | ring | torus | plane (empty=point spawn)\",\n");
		S += TEXT("        \"sphereRadius\": \"float (sphere only)\",\n");
		S += TEXT("        \"boxSize\": {\"x\":100,\"y\":100,\"z\":100} (box only),\n");
		S += TEXT("        \"cylinderHeight\": \"float\", \"cylinderRadius\": \"float\",\n");
		S += TEXT("        \"coneAngle\": \"float (degrees)\", \"coneLength\": \"float\",\n");
		S += TEXT("        \"ringRadius\": \"float\", \"ringWidth\": \"float\",\n");
		S += TEXT("        \"torusRadius\": \"float\", \"torusSectionRadius\": \"float\",\n");
		S += TEXT("        \"offset\": {\"x\":0,\"y\":0,\"z\":0},\n");
		S += TEXT("        \"surfaceOnly\": \"bool (true=surface only, false=volume fill)\"\n");
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
		S += TEXT("        \"speedLimit\": \"float (0=no limit)\",\n");
		S += TEXT("        \"colorCurve\": [{\"time\":0,\"color\":{\"r\":1,\"g\":0.5,\"b\":0}},{\"time\":1,\"color\":{\"r\":0.2,\"g\":0,\"b\":0}}] (multi-point, optional),\n");
		S += TEXT("        \"sizeCurve\": [{\"time\":0,\"scale\":0.5},{\"time\":0.5,\"scale\":2.0},{\"time\":1,\"scale\":0.1}] (multi-point, optional),\n");
		S += TEXT("        \"cameraDistanceFadeNear\": \"float (start fade-out distance)\",\n");
		S += TEXT("        \"cameraDistanceFadeFar\": \"float (fully faded distance)\"\n");
		S += TEXT("      },\n");
		S += TEXT("      \"render\": {\n");
		S += TEXT("        \"rendererType\": \"sprite | ribbon | light | mesh\",\n");
		S += TEXT("        \"emitterTemplate\": \"spark | smoke | explosion | debris | impact | flame | flare | arc | dust | muzzle_flash (optional, overrides rendererType template)\",\n");
		S += TEXT("        \"materialPath\": \"/Game/NiagaraExamples/Materials/MI_xxx (optional, overrides default material)\",\n");
		S += TEXT("        \"blendMode\": \"additive | translucent\",\n");
		S += TEXT("        \"sortOrder\": \"int\",\n");
		S += TEXT("        \"alignment\": \"unaligned | velocity_aligned\",\n");
		S += TEXT("        \"facingMode\": \"default | velocity | camera_position | camera_plane | custom_axis\",\n");
		S += TEXT("        \"lightRadiusScale\": \"float (light only)\",\n");
		S += TEXT("        \"lightIntensity\": \"float (light only)\",\n");
		S += TEXT("        \"lightExponent\": \"float (attenuation falloff, default 1)\",\n");
		S += TEXT("        \"lightVolumetricScattering\": \"bool (volumetric fog interaction, expensive)\",\n");
		S += TEXT("        \"ribbonWidth\": \"float (ribbon only)\",\n");
		S += TEXT("        \"ribbonUVMode\": \"stretch | tile_distance | tile_lifetime | distribute\",\n");
		S += TEXT("        \"ribbonTessellation\": \"int (curve smoothness, 0=default)\",\n");
		S += TEXT("        \"ribbonWidthScaleStart\": \"float (width at ribbon start)\",\n");
		S += TEXT("        \"ribbonWidthScaleEnd\": \"float (width at ribbon end, <1=taper)\",\n");
		S += TEXT("        \"meshPath\": \"/Game/... or /Engine/BasicShapes/... (mesh renderer asset)\",\n");
		S += TEXT("        \"meshOrientation\": \"velocity | camera | default\",\n");
		S += TEXT("        \"subImageRows\": \"int (flipbook rows, 0=none)\",\n");
		S += TEXT("        \"subImageColumns\": \"int (flipbook cols, 0=none)\",\n");
		S += TEXT("        \"subUVPlayRate\": \"float (1=lifetime, 2=2x speed, 0=static)\",\n");
		S += TEXT("        \"subUVRandomStartFrame\": \"bool (randomize start frame)\",\n");
		S += TEXT("        \"softParticle\": \"bool (depth fade at geometry intersection)\",\n");
		S += TEXT("        \"softParticleFadeDistance\": \"float (fade distance in world units)\",\n");
		S += TEXT("        \"cameraOffset\": \"float (offset toward camera, prevents z-fighting)\",\n");
		S += TEXT("        \"texturePrompt\": \"string (SD prompt for custom texture, optional)\",\n");
		S += TEXT("        \"textureNegativePrompt\": \"string (SD negative prompt, optional)\",\n");
		S += TEXT("        \"textureType\": \"particle_sprite | flipbook_4x4 | flipbook_8x8 | noise | gradient\",\n");
		S += TEXT("        \"textureResolution\": \"int (0=none, 128/256/512/1024)\"\n");
		S += TEXT("      },\n");
		S += TEXT("      \"collision\": {\n");
		S += TEXT("        \"enabled\": \"bool (activate collision module)\",\n");
		S += TEXT("        \"response\": \"bounce | kill | stick\",\n");
		S += TEXT("        \"restitution\": \"float (0-1, bounciness, bounce mode only)\",\n");
		S += TEXT("        \"friction\": \"float (0-1)\",\n");
		S += TEXT("        \"traceDistance\": \"float (GPU ray trace distance, 0=default)\"\n");
		S += TEXT("      },\n");
		S += TEXT("      \"eventSpawn\": {\n");
		S += TEXT("        \"triggerEvent\": \"death | collision\",\n");
		S += TEXT("        \"spawnCount\": \"int (particles per event)\",\n");
		S += TEXT("        \"targetEmitter\": \"string (target emitter name in same system, optional)\",\n");
		S += TEXT("        \"velocityScale\": \"float (inherited velocity multiplier)\"\n");
		S += TEXT("      },\n");
		S += TEXT("      \"spawnPerUnit\": {\n");
		S += TEXT("        \"enabled\": \"bool\",\n");
		S += TEXT("        \"spawnPerUnit\": \"float (particles per distance unit)\",\n");
		S += TEXT("        \"maxFrameSpawn\": \"float (max particles per frame)\",\n");
		S += TEXT("        \"movementTolerance\": \"float (min movement threshold)\"\n");
		S += TEXT("      },\n");
		S += TEXT("      \"gpuSim\": \"bool (GPU simulation for large particle counts)\",\n");
		S += TEXT("      \"dataInterfaces\": [\n");
		S += TEXT("        {\n");
		S += TEXT("          \"type\": \"skeletal_mesh | spline\",\n");
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
