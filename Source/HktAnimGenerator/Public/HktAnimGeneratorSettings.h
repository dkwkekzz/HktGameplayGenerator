// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HktAnimGeneratorSettings.generated.h"

/**
 * UHktAnimGeneratorSettings
 *
 * Project Settings > HktGameplay > HktAnimGenerator
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "HktAnimGenerator"))
class HKTANIMGENERATOR_API UHktAnimGeneratorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UHktAnimGeneratorSettings();

	virtual FName GetCategoryName() const override { return TEXT("HktGameplay"); }
	virtual FName GetSectionName() const override { return TEXT("HktAnimGenerator"); }

	static const UHktAnimGeneratorSettings* Get()
	{
		return GetDefault<UHktAnimGeneratorSettings>();
	}

	/** 생성된 애니메이션 기본 출력 경로 */
	UPROPERTY(config, EditAnywhere, Category = "Output", meta = (DisplayName = "Default Output Directory"))
	FString DefaultOutputDirectory;

	/** 기본 Humanoid 스켈레톤 (애니메이션 타겟) */
	UPROPERTY(config, EditAnywhere, Category = "Skeleton", meta = (DisplayName = "Default Target Skeleton"))
	FSoftObjectPath DefaultTargetSkeleton;

	/** MCP 프롬프트용 — 애니메이션 스타일 접미사 */
	UPROPERTY(config, EditAnywhere, Category = "Prompt", meta = (DisplayName = "Animation Style Prompt Suffix", MultiLine = true))
	FString AnimStylePromptSuffix;

	/** Locomotion 타입은 BlendSpace 생성 여부 */
	UPROPERTY(config, EditAnywhere, Category = "Generation", meta = (DisplayName = "Generate BlendSpace for Locomotion"))
	bool bGenerateBlendSpaceForLocomotion;
};
