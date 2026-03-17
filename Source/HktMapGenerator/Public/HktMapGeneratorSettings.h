// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HktMapGeneratorSettings.generated.h"

/**
 * Project Settings > Plugins > HKT Map Generator
 *
 * HktMap 생성 관련 설정. 출력 경로, 기본 Landscape 설정 등.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "HKT Map Generator"))
class HKTMAPGENERATOR_API UHktMapGeneratorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UHktMapGeneratorSettings()
	{
		CategoryName = TEXT("HktGameplay");
		SectionName = TEXT("HktMapGenerator");
	}

	/** HktMap JSON 파일 출력 디렉터리 (프로젝트 기준) */
	UPROPERTY(config, EditAnywhere, Category = "Output",
		meta = (DisplayName = "Map Output Directory"))
	FString MapOutputDirectory = TEXT("Content/Generated/Maps");

	/** 기본 Landscape 머티리얼 경로 */
	UPROPERTY(config, EditAnywhere, Category = "Landscape",
		meta = (DisplayName = "Default Landscape Material"))
	FSoftObjectPath DefaultLandscapeMaterial;

	/** 기본 Landscape 크기 */
	UPROPERTY(config, EditAnywhere, Category = "Landscape",
		meta = (DisplayName = "Default Landscape Size"))
	int32 DefaultLandscapeSize = 8129;

	/** Spawner 기본 클래스 */
	UPROPERTY(config, EditAnywhere, Category = "Spawner",
		meta = (DisplayName = "Default Spawner Class"))
	FSoftClassPath DefaultSpawnerClass;

	static const UHktMapGeneratorSettings* Get()
	{
		return GetDefault<UHktMapGeneratorSettings>();
	}
};
