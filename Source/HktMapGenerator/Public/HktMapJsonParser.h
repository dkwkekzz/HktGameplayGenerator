// Copyright Hkt Studios, Inc. All Rights Reserved.
// HktMap JSON 파싱 유틸리티 — 에디터/런타임 공용

#pragma once

#include "CoreMinimal.h"
#include "HktMapData.h"

/**
 * FHktMapJsonParser
 *
 * FHktMapData ↔ JSON 변환을 담당하는 정적 유틸리티.
 * EditorSubsystem과 WorldSubsystem 양쪽에서 공용으로 사용.
 */
struct HKTMAPGENERATOR_API FHktMapJsonParser
{
	/** JSON 문자열 → FHktMapData 파싱 */
	static bool Parse(const FString& JsonStr, FHktMapData& OutMapData);

	/** FHktMapData → JSON 문자열 직렬화 */
	static FString Serialize(const FHktMapData& MapData);
};
