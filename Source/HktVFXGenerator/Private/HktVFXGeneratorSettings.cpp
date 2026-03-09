// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVFXGeneratorSettings.h"

UHktVFXGeneratorSettings::UHktVFXGeneratorSettings()
{
	// === 렌더러 타입 기본 템플릿 (폴백용) ===
	EmitterTemplates.Add(TEXT("sprite"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst")));
	EmitterTemplates.Add(TEXT("ribbon"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/LocationBasedRibbon.LocationBasedRibbon")));
	EmitterTemplates.Add(TEXT("light"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal")));
	EmitterTemplates.Add(TEXT("mesh"),
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst.UpwardMeshBurst")));

	// === NiagaraExamples 기반 행동별 템플릿 ===
	// 이 에미터들은 적절한 모듈(Gravity, Drag, Noise, SubUV 등)과
	// 머티리얼/텍스처가 이미 설정되어 있어 풍부한 결과를 즉시 얻을 수 있음.

	// 스파크/불꽃 — 빠르고 작은 파티클, Gravity+Drag 포함
	EmitterTemplates.Add(TEXT("spark"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Sparks/Emitters/NE_Sparks.NE_Sparks")));

	// 보조 스파크 — 더 작고 느린 2차 스파크
	EmitterTemplates.Add(TEXT("spark_secondary"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Sparks/Emitters/NE_SecondarySparks.NE_SecondarySparks")));

	// 연기 — 부드럽게 확장하는 연기 파티클
	EmitterTemplates.Add(TEXT("smoke"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Smoke/Emitters/NE_Smoke.NE_Smoke")));

	// 폭발 코어 — 중심부 강렬한 플래시
	EmitterTemplates.Add(TEXT("explosion"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Explosions/Emitters/NE_Explosion.NE_Explosion")));

	// 폭발 코어 (발광) — 밝은 중심
	EmitterTemplates.Add(TEXT("core"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Explosions/Emitters/NE_Core.NE_Core")));

	// 파편 — 중력 영향 받는 조각
	EmitterTemplates.Add(TEXT("debris"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Explosions/Emitters/NE_Debris.NE_Debris")));

	// 먼지 폭발 — 지면 먼지
	EmitterTemplates.Add(TEXT("dust"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Explosions/Emitters/NE_DustExplosion.NE_DustExplosion")));

	// 지면 먼지 — 바닥 먼지
	EmitterTemplates.Add(TEXT("ground_dust"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Explosions/Emitters/NE_GroundDust.NE_GroundDust")));

	// 스파크 파편 — 불꽃 파편
	EmitterTemplates.Add(TEXT("spark_debris"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Explosions/Emitters/NE_SparkDebris.NE_SparkDebris")));

	// 임팩트 — 충격 이펙트
	EmitterTemplates.Add(TEXT("impact"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Weapons/Impacts/Emitters/NE_Impact_Sprite.NE_Impact_Sprite")));

	// 임팩트 메시 — 메시 기반 충격
	EmitterTemplates.Add(TEXT("impact_mesh"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Weapons/Impacts/Emitters/NE_Impact_Mesh.NE_Impact_Mesh")));

	// 머즐 플래시 — 총구 화염
	EmitterTemplates.Add(TEXT("muzzle_flash"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Weapons/MuzzleFlashes/Emitters/NE_MuzzleFlash_Base.NE_MuzzleFlash_Base")));

	// 불꽃/화염 — SubUV 애니메이션 화염
	EmitterTemplates.Add(TEXT("flame"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Explosions/Emitters/NE_Explosion.NE_Explosion")));

	// 아크/전기 리본
	EmitterTemplates.Add(TEXT("arc"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Ribbons/Emitters/NE_Arc.NE_Arc")));

	// 플레어 — 렌즈 플레어 효과
	EmitterTemplates.Add(TEXT("flare"),
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/FX_Explosions/Emitters/NE_Core.NE_Core")));

	FallbackEmitterTemplate =
		FSoftObjectPath(TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst"));

	// === 기본 머티리얼 (NiagaraExamples) ===
	AdditiveMaterial =
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/Materials/MasterMaterials/M_SimpleAdditive.M_SimpleAdditive"));
	TranslucentMaterial =
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/Materials/MI_BasicSprite_Translucent.MI_BasicSprite_Translucent"));
}
