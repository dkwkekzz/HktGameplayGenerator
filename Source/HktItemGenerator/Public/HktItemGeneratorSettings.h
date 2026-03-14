// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HktItemGeneratorSettings.generated.h"

/**
 * 아이템 카테고리별 소켓 매핑
 */
USTRUCT(BlueprintType)
struct HKTITEMGENERATOR_API FHktItemSocketMapping
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Socket")
	FString Category;

	UPROPERTY(EditAnywhere, Category = "Socket")
	FName SocketName;

	UPROPERTY(EditAnywhere, Category = "Socket")
	FString AttachMethod;  // "socket", "overlay", "vfx"
};

/**
 * UHktItemGeneratorSettings
 *
 * Project Settings > HktGameplay > HktItemGenerator
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "HktItemGenerator"))
class HKTITEMGENERATOR_API UHktItemGeneratorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UHktItemGeneratorSettings();

	virtual FName GetCategoryName() const override { return TEXT("HktGameplay"); }
	virtual FName GetSectionName() const override { return TEXT("HktItemGenerator"); }

	static const UHktItemGeneratorSettings* Get()
	{
		return GetDefault<UHktItemGeneratorSettings>();
	}

	/** 생성된 아이템 에셋 기본 출력 경로 */
	UPROPERTY(config, EditAnywhere, Category = "Output", meta = (DisplayName = "Default Output Directory"))
	FString DefaultOutputDirectory;

	/** 아이콘 출력 경로 */
	UPROPERTY(config, EditAnywhere, Category = "Output", meta = (DisplayName = "Icon Output Directory"))
	FString IconOutputDirectory;

	/** 카테고리별 소켓 매핑 (장착 위치) */
	UPROPERTY(config, EditAnywhere, Category = "Socket", meta = (DisplayName = "Socket Mappings"))
	TArray<FHktItemSocketMapping> SocketMappings;

	/** MCP 프롬프트용 — 아이템 메시 스타일 접미사 */
	UPROPERTY(config, EditAnywhere, Category = "Prompt", meta = (DisplayName = "Item Mesh Style Suffix", MultiLine = true))
	FString ItemMeshPromptSuffix;

	/** MCP 프롬프트용 — 아이템 아이콘 스타일 접미사 */
	UPROPERTY(config, EditAnywhere, Category = "Prompt", meta = (DisplayName = "Item Icon Style Suffix", MultiLine = true))
	FString ItemIconPromptSuffix;

	/** Element별 기본 머티리얼 경로 매핑 */
	UPROPERTY(config, EditAnywhere, Category = "Material", meta = (DisplayName = "Element Material Map"))
	TMap<FString, FSoftObjectPath> ElementMaterialMap;
};
