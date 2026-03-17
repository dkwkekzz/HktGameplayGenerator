// Copyright Hkt Studios, Inc. All Rights Reserved.
// 텍스처 생성 설정 — Project Settings > HktGameplay > HktTextureGenerator

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HktTextureGeneratorSettings.generated.h"

/**
 * UHktTextureGeneratorSettings
 *
 * Project Settings > HktGameplay > HktTextureGenerator 에서 설정 가능.
 */
UCLASS(config = Game, DefaultConfig, DisplayName = "Hkt Texture Generator")
class HKTTEXTUREGENERATOR_API UHktTextureGeneratorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UHktTextureGeneratorSettings();

	// =========================================================================
	// 출력
	// =========================================================================

	/** 생성된 텍스처 에셋의 기본 출력 디렉토리 */
	UPROPERTY(Config, EditAnywhere, Category = "Output")
	FString DefaultOutputDirectory = TEXT("/Game/Generated/Textures");

	// =========================================================================
	// SD WebUI (로컬 Stable Diffusion 서버)
	// =========================================================================

	/** SD WebUI 실행 파일 경로 (예: E:/AI/webui_forge/run.bat). 비어있으면 자동 실행 안함. */
	UPROPERTY(Config, EditAnywhere, Category = "SD WebUI", meta = (FilePathFilter = "bat"))
	FString SDWebUIBatchFilePath;

	/** SD WebUI 서버 URL (기본: http://127.0.0.1:7860) */
	UPROPERTY(Config, EditAnywhere, Category = "SD WebUI")
	FString SDWebUIServerURL = TEXT("http://127.0.0.1:7860");

	// =========================================================================
	// 생성 기본값
	// =========================================================================

	/** 기본 해상도 (Intent에서 0일 때 적용) */
	UPROPERTY(Config, EditAnywhere, Category = "Defaults", meta = (ClampMin = "64", ClampMax = "2048"))
	int32 DefaultResolution = 256;

	/** 기본 네거티브 프롬프트 (항상 추가) */
	UPROPERTY(Config, EditAnywhere, Category = "Defaults")
	FString DefaultNegativePrompt = TEXT("text, watermark, frame, border, realistic photo, face, blurry");

	// =========================================================================
	// Usage별 프롬프트 접미사 (자동 추가)
	// =========================================================================

	/** ParticleSprite용 프롬프트 접미사 */
	UPROPERTY(Config, EditAnywhere, Category = "PromptSuffix")
	FString ParticleSpritePromptSuffix = TEXT(", centered, soft alpha gradient edge, black background, VFX sprite, game asset");

	/** Flipbook용 프롬프트 접미사 */
	UPROPERTY(Config, EditAnywhere, Category = "PromptSuffix")
	FString FlipbookPromptSuffix = TEXT(", sprite sheet, black background, game VFX animation sequence");

	/** ItemIcon용 프롬프트 접미사 */
	UPROPERTY(Config, EditAnywhere, Category = "PromptSuffix")
	FString ItemIconPromptSuffix = TEXT(", game icon, isometric view, clean background, stylized");

	/** Noise용 프롬프트 접미사 */
	UPROPERTY(Config, EditAnywhere, Category = "PromptSuffix")
	FString NoisePromptSuffix = TEXT(", tileable, seamless, grayscale");

	/** Material 텍스처용 프롬프트 접미사 */
	UPROPERTY(Config, EditAnywhere, Category = "PromptSuffix")
	FString MaterialPromptSuffix = TEXT(", PBR texture, seamless, game asset");

	// =========================================================================
	// UDeveloperSettings
	// =========================================================================

	virtual FName GetContainerName() const override { return FName("Project"); }
	virtual FName GetCategoryName() const override { return FName("HktGameplay"); }
	virtual FName GetSectionName() const override { return FName("HktTextureGenerator"); }

	static const UHktTextureGeneratorSettings* Get()
	{
		return GetDefault<UHktTextureGeneratorSettings>();
	}
};
