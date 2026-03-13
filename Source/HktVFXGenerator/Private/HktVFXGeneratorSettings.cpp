// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVFXGeneratorSettings.h"

UHktVFXGeneratorSettings::UHktVFXGeneratorSettings()
{
	// =========================================================================
	// 14 built-in UE5 Niagara Template Emitters
	// 경로: /Niagara/DefaultAssets/Templates/Emitters/{Name}.{Name}
	//
	// 각 템플릿은 고유한 모듈 조합을 가지며, Config의 emitterTemplate 필드로 선택.
	// Builder가 모든 모듈 파라미터를 시도하고, 해당 모듈이 있는 템플릿에만 적용됨.
	// =========================================================================

	// --- Burst 계열 (일회성 폭발) ---

	// simple_sprite_burst — 가장 기본. InitializeParticle + ScaleColor + SolveForcesAndVelocity
	EmitterTemplates.Add(TEXT("simple_sprite_burst"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst")));

	// omnidirectional_burst — 전방향 폭발. GravityForce + Drag 포함
	EmitterTemplates.Add(TEXT("omnidirectional_burst"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst.OmnidirectionalBurst")));

	// directional_burst — 방향성 폭발 (원뿔형). GravityForce + Drag 포함
	EmitterTemplates.Add(TEXT("directional_burst"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/DirectionalBurst.DirectionalBurst")));

	// confetti_burst — 색종이 폭발. GravityForce + Drag + SpriteRotationRate 포함
	EmitterTemplates.Add(TEXT("confetti_burst"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/ConfettiBurst.ConfettiBurst")));

	// upward_mesh_burst — 상향 메시 폭발. GravityForce + ScaleMeshSize 포함
	EmitterTemplates.Add(TEXT("upward_mesh_burst"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst.UpwardMeshBurst")));

	// --- Rate 계열 (지속 방출) ---

	// fountain — 분수. SpawnRate + GravityForce + ScaleColor + SolveForcesAndVelocity
	EmitterTemplates.Add(TEXT("fountain"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/Fountain.Fountain")));

	// blowing_particles — 바람에 날리는 파티클. SpawnRate + CurlNoiseForce
	EmitterTemplates.Add(TEXT("blowing_particles"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/BlowingParticles.BlowingParticles")));

	// hanging_particulates — 떠다니는 미립자. SpawnRate + CurlNoiseForce
	EmitterTemplates.Add(TEXT("hanging_particulates"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/HangingParticulates.HangingParticulates")));

	// --- 특수 계열 ---

	// single_looping_particle — 단일 루핑 파티클. 하나만 계속 유지
	EmitterTemplates.Add(TEXT("single_looping_particle"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/SingleLoopingParticle.SingleLoopingParticle")));

	// recycle_particles — 카메라 뷰 기반 재활용 파티클
	EmitterTemplates.Add(TEXT("recycle_particles"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/RecycleParticlesInView.RecycleParticlesInView")));

	// minimal — 최소한의 에미터. SolveForcesAndVelocity만
	EmitterTemplates.Add(TEXT("minimal"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal")));

	// --- Ribbon/Beam 계열 ---

	// ribbon — 위치 기반 리본 트레일. SpawnRate + Ribbon 렌더러
	EmitterTemplates.Add(TEXT("ribbon"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/LocationBasedRibbon.LocationBasedRibbon")));

	// dynamic_beam — 동적 빔 (두 점 사이). Beam 모듈 + Ribbon 렌더러
	EmitterTemplates.Add(TEXT("dynamic_beam"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/DynamicBeam.DynamicBeam")));

	// static_beam — 정적 빔. Beam 모듈 + Ribbon 렌더러
	EmitterTemplates.Add(TEXT("static_beam"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/StaticBeam.StaticBeam")));

	// --- 렌더러 타입 폴백 (하위 호환) ---
	// rendererType으로 직접 조회할 때 사용
	EmitterTemplates.Add(TEXT("sprite"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst")));
	EmitterTemplates.Add(TEXT("light"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal")));
	EmitterTemplates.Add(TEXT("mesh"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst.UpwardMeshBurst")));

	// --- NiagaraExamples 기반 행동별 템플릿 ---
	// 프로젝트에 NiagaraExamples 콘텐츠가 있을 때만 작동.
	// 이 에미터들은 완성된 VFX 모듈+머티리얼+텍스처를 포함.
	EmitterTemplates.Add(TEXT("spark"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Sparks/Emitters/NE_Sparks.NE_Sparks")));
	EmitterTemplates.Add(TEXT("spark_secondary"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Sparks/Emitters/NE_SecondarySparks.NE_SecondarySparks")));
	EmitterTemplates.Add(TEXT("smoke"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Smoke/Emitters/NE_Smoke.NE_Smoke")));
	EmitterTemplates.Add(TEXT("explosion"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Explosions/Emitters/NE_Explosion.NE_Explosion")));
	EmitterTemplates.Add(TEXT("core"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Explosions/Emitters/NE_Core.NE_Core")));
	EmitterTemplates.Add(TEXT("debris"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Explosions/Emitters/NE_Debris.NE_Debris")));
	EmitterTemplates.Add(TEXT("dust"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Explosions/Emitters/NE_DustExplosion.NE_DustExplosion")));
	EmitterTemplates.Add(TEXT("ground_dust"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Explosions/Emitters/NE_GroundDust.NE_GroundDust")));
	EmitterTemplates.Add(TEXT("spark_debris"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Explosions/Emitters/NE_SparkDebris.NE_SparkDebris")));
	EmitterTemplates.Add(TEXT("impact"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Weapons/Impacts/Emitters/NE_Impact_Sprite.NE_Impact_Sprite")));
	EmitterTemplates.Add(TEXT("impact_mesh"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Weapons/Impacts/Emitters/NE_Impact_Mesh.NE_Impact_Mesh")));
	EmitterTemplates.Add(TEXT("muzzle_flash"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Weapons/MuzzleFlashes/Emitters/NE_MuzzleFlash_Base.NE_MuzzleFlash_Base")));
	EmitterTemplates.Add(TEXT("arc"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Ribbons/Emitters/NE_Arc.NE_Arc")));

	FallbackEmitterTemplate =
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst"));

	// =========================================================================
	// 동적 모듈 주입용 스크립트 경로
	// 템플릿에 해당 모듈이 없을 때, Config 요구에 따라 그래프에 동적 추가.
	// 경로: /Niagara/Modules/{ModuleName}.{ModuleName}
	// =========================================================================

	// 힘/물리 모듈
	ModuleScriptPaths.Add(TEXT("GravityForce"),
		FSoftObjectPath(TEXT("/Niagara/Modules/GravityForce.GravityForce")));
	ModuleScriptPaths.Add(TEXT("Drag"),
		FSoftObjectPath(TEXT("/Niagara/Modules/Drag.Drag")));
	ModuleScriptPaths.Add(TEXT("CurlNoiseForce"),
		FSoftObjectPath(TEXT("/Niagara/Modules/CurlNoiseForce.CurlNoiseForce")));
	ModuleScriptPaths.Add(TEXT("VortexVelocity"),
		FSoftObjectPath(TEXT("/Niagara/Modules/VortexVelocity.VortexVelocity")));
	ModuleScriptPaths.Add(TEXT("PointAttractionForce"),
		FSoftObjectPath(TEXT("/Niagara/Modules/PointAttractionForce.PointAttractionForce")));
	ModuleScriptPaths.Add(TEXT("WindForce"),
		FSoftObjectPath(TEXT("/Niagara/Modules/WindForce.WindForce")));
	ModuleScriptPaths.Add(TEXT("AccelerationForce"),
		FSoftObjectPath(TEXT("/Niagara/Modules/AccelerationForce.AccelerationForce")));

	// 크기/회전 모듈
	ModuleScriptPaths.Add(TEXT("ScaleSpriteSize"),
		FSoftObjectPath(TEXT("/Niagara/Modules/ScaleSpriteSize.ScaleSpriteSize")));
	ModuleScriptPaths.Add(TEXT("ScaleMeshSize"),
		FSoftObjectPath(TEXT("/Niagara/Modules/ScaleMeshSize.ScaleMeshSize")));
	ModuleScriptPaths.Add(TEXT("SpriteRotationRate"),
		FSoftObjectPath(TEXT("/Niagara/Modules/SpriteRotationRate.SpriteRotationRate")));
	ModuleScriptPaths.Add(TEXT("ScaleColor"),
		FSoftObjectPath(TEXT("/Niagara/Modules/ScaleColor.ScaleColor")));
	ModuleScriptPaths.Add(TEXT("SolveForcesAndVelocity"),
		FSoftObjectPath(TEXT("/Niagara/Modules/SolveForcesAndVelocity.SolveForcesAndVelocity")));

	// 데이터 인터페이스 관련 모듈 (스켈레톤 메시 표면 스폰 등)
	ModuleScriptPaths.Add(TEXT("InitializeMeshReproductionSprite"),
		FSoftObjectPath(TEXT("/Niagara/Modules/InitializeMeshReproductionSprite.InitializeMeshReproductionSprite")));
	ModuleScriptPaths.Add(TEXT("SampleSkeletalMesh"),
		FSoftObjectPath(TEXT("/Niagara/Modules/SampleSkeletalMesh.SampleSkeletalMesh")));
	ModuleScriptPaths.Add(TEXT("UpdateMeshReproductionSprite"),
		FSoftObjectPath(TEXT("/Niagara/Modules/UpdateMeshReproductionSprite.UpdateMeshReproductionSprite")));

	// 스플라인 관련 모듈
	ModuleScriptPaths.Add(TEXT("SampleSpline"),
		FSoftObjectPath(TEXT("/Niagara/Modules/SampleSpline.SampleSpline")));

	// Shape Location 모듈 (파티클 방출 형태: sphere, box, cone, ring, torus 등)
	ModuleScriptPaths.Add(TEXT("ShapeLocation"),
		FSoftObjectPath(TEXT("/Niagara/Modules/Location/ShapeLocation.ShapeLocation")));

	// Collision 모듈 (GPU Depth Buffer 충돌)
	ModuleScriptPaths.Add(TEXT("Collision"),
		FSoftObjectPath(TEXT("/Niagara/Modules/Collision.Collision")));

	// Event 모듈 (파티클 이벤트 기반 2차 스폰)
	ModuleScriptPaths.Add(TEXT("GenerateLocationEvent"),
		FSoftObjectPath(TEXT("/Niagara/Modules/GenerateLocationEvent.GenerateLocationEvent")));
	ModuleScriptPaths.Add(TEXT("ReceiveLocationEvent"),
		FSoftObjectPath(TEXT("/Niagara/Modules/ReceiveLocationEvent.ReceiveLocationEvent")));

	// Spawn Per Unit (이동 거리 기반 스폰)
	ModuleScriptPaths.Add(TEXT("SpawnPerUnit"),
		FSoftObjectPath(TEXT("/Niagara/Modules/SpawnPerUnit.SpawnPerUnit")));

	// === 기본 머티리얼 (NiagaraExamples) ===
	AdditiveMaterial =
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/Materials/MasterMaterials/M_SimpleAdditive.M_SimpleAdditive"));
	TranslucentMaterial =
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/Materials/MI_BasicSprite_Translucent.MI_BasicSprite_Translucent"));
}
