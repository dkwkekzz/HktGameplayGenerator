// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HktMeshGeneratorSettings.generated.h"

/**
 * UHktMeshGeneratorSettings
 *
 * Project Settings > HktGameplay > HktMeshGenerator
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "HktMeshGenerator"))
class HKTMESHGENERATOR_API UHktMeshGeneratorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UHktMeshGeneratorSettings();

	virtual FName GetCategoryName() const override { return TEXT("HktGameplay"); }
	virtual FName GetSectionName() const override { return TEXT("HktMeshGenerator"); }

	static const UHktMeshGeneratorSettings* Get()
	{
		return GetDefault<UHktMeshGeneratorSettings>();
	}

	/** 생성된 메시 에셋 기본 출력 경로 */
	UPROPERTY(config, EditAnywhere, Category = "Output", meta = (DisplayName = "Default Output Directory"))
	FString DefaultOutputDirectory;

	/** 기본 Humanoid 스켈레톤 경로 */
	UPROPERTY(config, EditAnywhere, Category = "Skeleton Pool", meta = (DisplayName = "Humanoid Base Skeleton"))
	FSoftObjectPath HumanoidBaseSkeleton;

	/** 기본 Quadruped 스켈레톤 경로 */
	UPROPERTY(config, EditAnywhere, Category = "Skeleton Pool", meta = (DisplayName = "Quadruped Base Skeleton"))
	FSoftObjectPath QuadrupedBaseSkeleton;

	/** 캐릭터 Blueprint 기본 부모 클래스 */
	UPROPERTY(config, EditAnywhere, Category = "Blueprint", meta = (DisplayName = "Default Character Blueprint Parent"))
	FSoftClassPath DefaultCharacterParentClass;

	/** MCP 프롬프트용 — 캐릭터 메시 생성 스타일 가이드 */
	UPROPERTY(config, EditAnywhere, Category = "Prompt", meta = (DisplayName = "Character Style Prompt Suffix", MultiLine = true))
	FString CharacterStylePromptSuffix;

	/** MCP 프롬프트용 — 건물/구조물 생성 스타일 가이드 */
	UPROPERTY(config, EditAnywhere, Category = "Prompt", meta = (DisplayName = "Structure Style Prompt Suffix", MultiLine = true))
	FString StructureStylePromptSuffix;
};
