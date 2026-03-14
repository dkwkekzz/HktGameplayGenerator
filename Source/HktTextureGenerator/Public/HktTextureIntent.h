// Copyright Hkt Studios, Inc. All Rights Reserved.

// 텍스처 생성 의도(Intent) — "무엇이 필요한지"만 기술
// 실제 생성은 UHktTextureGeneratorSubsystem이 수행

#pragma once

#include "CoreMinimal.h"
#include "HktTextureIntent.generated.h"

// ============================================================================
// 텍스처 용도 — 임포트 후 자동 설정을 결정
// ============================================================================
UENUM(BlueprintType)
enum class EHktTextureUsage : uint8
{
	ParticleSprite,    // VFX 파티클 단일 스프라이트
	Flipbook4x4,       // 4x4 SubUV 시퀀스 (16프레임)
	Flipbook8x8,       // 8x8 SubUV 시퀀스 (64프레임)
	Noise,             // 타일 가능한 노이즈 텍스처
	Gradient,          // 그라디언트 램프
	ItemIcon,          // 아이템 아이콘 (2D UI)
	MaterialBase,      // BaseColor / Albedo 텍스처
	MaterialNormal,    // Normal Map
	MaterialMask,      // Mask (Roughness/Metallic/AO packed)
};

// ============================================================================
// FHktTextureIntent — 생성 요청의 의미 기술
// ============================================================================
USTRUCT(BlueprintType)
struct HKTTEXTUREGENERATOR_API FHktTextureIntent
{
	GENERATED_BODY()

	// --- 핵심 ---

	/** 텍스처 용도. 임포트 후 Compression/LODGroup/SRGB 등 자동 결정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent|Core")
	EHktTextureUsage Usage = EHktTextureUsage::ParticleSprite;

	/** 이미지 생성 프롬프트 (SD/DALL-E 등) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent|Core")
	FString Prompt;

	/** 네거티브 프롬프트 (생성 시 제외할 요소) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent|Core")
	FString NegativePrompt;

	// --- 출력 설정 ---

	/** 출력 해상도 (128/256/512/1024). 0이면 Usage 기본값 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent|Output", meta = (ClampMin = "0", ClampMax = "2048"))
	int32 Resolution = 256;

	/** 투명 배경 (알파 채널) 필요 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent|Output")
	bool bAlphaChannel = true;

	/** 타일링 가능 여부 (노이즈/머티리얼 텍스처) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent|Output")
	bool bTileable = false;

	// --- 스타일 힌트 ---

	/** 추가 스타일 키워드 (예: "painterly", "stylized", "pixel") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intent|Style")
	TArray<FString> StyleKeywords;

	// --- 유틸리티 ---

	/** Intent에서 결정적 캐시 키 생성. 같은 Intent면 같은 키 → 캐시 히트 */
	FString GetAssetKey() const
	{
		// Usage_Hash 형태. Prompt + Usage + Resolution 조합
		const uint32 Hash = GetTypeHash(Prompt) ^ GetTypeHash(static_cast<uint8>(Usage)) ^ GetTypeHash(Resolution);
		const TCHAR* UsageStr = nullptr;
		switch (Usage)
		{
		case EHktTextureUsage::ParticleSprite: UsageStr = TEXT("Sprite"); break;
		case EHktTextureUsage::Flipbook4x4:    UsageStr = TEXT("Flip4x4"); break;
		case EHktTextureUsage::Flipbook8x8:    UsageStr = TEXT("Flip8x8"); break;
		case EHktTextureUsage::Noise:           UsageStr = TEXT("Noise"); break;
		case EHktTextureUsage::Gradient:        UsageStr = TEXT("Gradient"); break;
		case EHktTextureUsage::ItemIcon:        UsageStr = TEXT("Icon"); break;
		case EHktTextureUsage::MaterialBase:    UsageStr = TEXT("Base"); break;
		case EHktTextureUsage::MaterialNormal:  UsageStr = TEXT("Normal"); break;
		case EHktTextureUsage::MaterialMask:    UsageStr = TEXT("Mask"); break;
		default:                                UsageStr = TEXT("Unknown"); break;
		}
		return FString::Printf(TEXT("T_%s_%08X"), UsageStr, Hash);
	}

	/** Usage에 따른 기본 해상도 반환 */
	int32 GetEffectiveResolution() const
	{
		if (Resolution > 0) return Resolution;
		switch (Usage)
		{
		case EHktTextureUsage::ItemIcon:        return 256;
		case EHktTextureUsage::Flipbook8x8:     return 1024;
		case EHktTextureUsage::MaterialBase:
		case EHktTextureUsage::MaterialNormal:
		case EHktTextureUsage::MaterialMask:    return 1024;
		default:                                return 256;
		}
	}

	/** JSON 직렬화 */
	FString ToJson() const;

	/** JSON 역직렬화 */
	static bool FromJson(const FString& JsonStr, FHktTextureIntent& OutIntent);
};

// ============================================================================
// FHktTextureRequest — 이름 + Intent (배치 요청용)
// ============================================================================
USTRUCT(BlueprintType)
struct HKTTEXTUREGENERATOR_API FHktTextureRequest
{
	GENERATED_BODY()

	/** 요청 식별 이름 (예: 에미터 이름, 머티리얼 슬롯 이름) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	FString Name;

	/** 텍스처 생성 의도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	FHktTextureIntent Intent;
};

// ============================================================================
// FHktTextureResult — 생성 결과
// ============================================================================
USTRUCT(BlueprintType)
struct HKTTEXTUREGENERATOR_API FHktTextureResult
{
	GENERATED_BODY()

	/** 요청 이름 (FHktTextureRequest::Name 대응) */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FString Name;

	/** 생성된 텍스처 (nullptr이면 실패) */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TObjectPtr<UTexture2D> Texture = nullptr;

	/** 에셋 경로 */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FString AssetPath;

	/** 실패 시 에러 메시지 */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FString Error;

	bool IsSuccess() const { return Texture != nullptr; }
};
