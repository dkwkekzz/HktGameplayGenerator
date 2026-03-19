// Copyright Hkt Studios, Inc. All Rights Reserved.
// VFX Tag → FHktVFXIntent 자동 변환 + 생성 요청

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktVFXIntent.h"

/**
 * FHktVFXAutoResolver
 *
 * VFX Tag 문자열을 FHktVFXIntent로 자동 파싱.
 * Story에서 정의한 "VFX.Explosion.Fire" 같은 태그를
 * EventType=Explosion, Element=Fire로 변환하여 Generator에 전달.
 *
 * Tag 규칙:
 *   VFX.Niagara.{Name}                   → Niagara 직접 참조 (Convention Path)
 *   VFX.{EventType}.{Element}            → 기본 (Intensity=0.5)
 *   VFX.{EventType}.{Element}.{Surface}  → 표면 지정
 *   VFX.Custom.{Description}             → 커스텀 자연어
 */
struct HKTGENERATORCORE_API FHktVFXAutoResolver
{
	/**
	 * VFX 태그에서 Intent 추론.
	 * @param VFXTag "VFX.Explosion.Fire" 형태의 태그
	 * @param OutIntent 추론된 Intent
	 * @return 파싱 성공 여부
	 */
	static bool ParseTagToIntent(const FGameplayTag& VFXTag, FHktVFXIntent& OutIntent);

	/**
	 * VFX 태그에서 에셋 키 생성.
	 * Intent의 GetAssetKey()와 동일한 결과를 Tag만으로 생성.
	 */
	static FString GetAssetKeyFromTag(const FGameplayTag& VFXTag);

private:
	static EHktVFXEventType ParseEventType(const FString& Str);
	static EHktVFXElement ParseElement(const FString& Str);
	static EHktVFXSurfaceType ParseSurfaceType(const FString& Str);
};
