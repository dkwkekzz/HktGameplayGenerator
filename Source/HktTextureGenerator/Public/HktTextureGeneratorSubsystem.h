// Copyright Hkt Studios, Inc. All Rights Reserved.
// 텍스처 생성 EditorSubsystem — Intent→UTexture2D 파이프라인

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "HktTextureIntent.h"
#include "HktTextureGeneratorSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTextureGenerated, const FHktTextureResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSDServerStatusChanged, bool, bAlive, const FString&, StatusMessage);

/**
 * UHktTextureGeneratorSubsystem
 *
 * 공유 텍스처 생성 서브시스템. 다른 Generator 모듈에서 이 서브시스템을 통해 텍스처 생성.
 *
 * 생성 흐름:
 *   1. 캐시 확인 (AssetKey → 기존 에셋)
 *   2. Convention Path 확인 (/Game/Generated/Textures/{Usage}/{AssetKey})
 *   3. Miss → 이미지 파일 생성 (MCP/외부 API/로컬 생성)
 *   4. 이미지 파일 → UTexture2D .uasset 임포트
 *   5. Usage별 텍스처 설정 자동 적용
 *   6. 캐시 등록
 *
 * 설정: Project Settings > HktGameplay > HktTextureGenerator
 */
UCLASS()
class HKTTEXTUREGENERATOR_API UHktTextureGeneratorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// =========================================================================
	// 생성 API
	// =========================================================================

	/**
	 * Intent로 텍스처 생성 (동기).
	 * 캐시 히트 시 즉시 반환. Miss 시 이미지 파일이 있어야 임포트 가능.
	 * @param Intent 텍스처 생성 의도
	 * @param OutputDir 출력 디렉토리 (빈 문자열이면 Settings 기본값)
	 * @return 생성/로드된 UTexture2D. 실패 시 nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator")
	UTexture2D* GenerateTexture(const FHktTextureIntent& Intent, const FString& OutputDir = TEXT(""));

	/**
	 * 배치 생성 — 여러 텍스처를 한번에 처리.
	 * @param Requests 요청 배열
	 * @param OutputDir 출력 디렉토리
	 * @return 결과 배열
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator")
	TArray<FHktTextureResult> GenerateBatch(const TArray<FHktTextureRequest>& Requests, const FString& OutputDir = TEXT(""));

	// =========================================================================
	// 이미지 파일 임포트 API (MCP/외부 도구가 이미지 생성 후 호출)
	// =========================================================================

	/**
	 * 외부에서 생성된 이미지 파일을 UTexture2D로 임포트.
	 * @param ImageFilePath 소스 이미지 파일 경로 (png/jpg/exr)
	 * @param Intent 텍스처 설정을 결정하기 위한 Intent
	 * @param OutputDir 출력 디렉토리 (빈 문자열이면 Settings 기본값)
	 * @return 임포트된 UTexture2D. 실패 시 nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator")
	UTexture2D* ImportTextureFromFile(const FString& ImageFilePath, const FHktTextureIntent& Intent, const FString& OutputDir = TEXT(""));

	// =========================================================================
	// MCP 호출용 JSON API
	// =========================================================================

	/** JSON Intent로 텍스처 생성. 결과를 JSON으로 반환. */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator")
	FString McpGenerateTexture(const FString& JsonIntent, const FString& OutputDir = TEXT(""));

	/** JSON Intent + 이미지 파일로 텍스처 임포트. 결과를 JSON으로 반환. */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator")
	FString McpImportTexture(const FString& ImageFilePath, const FString& JsonIntent, const FString& OutputDir = TEXT(""));

	/** 배치 생성을 위한 미처리 요청 반환 (캐시 miss 목록). MCP Agent가 이미지 생성 후 McpImportTexture 호출. */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator")
	FString McpGetPendingRequests(const FString& JsonRequests);

	// =========================================================================
	// SD WebUI 서버 관리
	// =========================================================================

	/** SD WebUI 서버가 응답 중인지 비동기 확인. 결과는 OnSDServerStatusChanged로 전달. */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator")
	void CheckSDServerStatus();

	/** SD WebUI 배치 파일을 실행하여 서버를 시작. 결과는 OnSDServerStatusChanged로 전달. */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator")
	void LaunchSDServer();

	/** SD WebUI 서버가 마지막 체크 시 응답했는지 (캐시된 상태) */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator")
	bool IsSDServerAlive() const { return bSDServerAlive; }

	/** SD WebUI 서버 상태 마지막 메시지 */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator")
	FString GetSDServerStatusMessage() const { return SDServerStatusMessage; }

	/** SD WebUI 서버가 현재 시작 중인지 */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator")
	bool IsSDServerLaunching() const { return bSDServerLaunching; }

	/** SD WebUI 서버 상태 변경 이벤트 */
	UPROPERTY(BlueprintAssignable, Category = "HKT|TextureGenerator")
	FOnSDServerStatusChanged OnSDServerStatusChanged;

	/** MCP용: SD 서버 상태를 JSON으로 반환 */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator")
	FString McpCheckSDServerStatus();

	// =========================================================================
	// 캐시 / 조회
	// =========================================================================

	/** 캐시에서 텍스처 조회 (AssetKey 기반) */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator")
	UTexture2D* FindCachedTexture(const FString& AssetKey) const;

	/** 생성된 텍스처 목록 조회 */
	UFUNCTION(BlueprintCallable, Category = "HKT|TextureGenerator")
	FString McpListGeneratedTextures(const FString& Directory = TEXT(""));

	// =========================================================================
	// 이벤트
	// =========================================================================

	UPROPERTY(BlueprintAssignable, Category = "HKT|TextureGenerator")
	FOnTextureGenerated OnTextureGenerated;

private:
	/** 출력 경로 결정 */
	FString ResolveOutputDir(const FString& OutputDir) const;

	/** Intent + OutputDir → 전체 에셋 경로 */
	FString ResolveAssetPath(const FHktTextureIntent& Intent, const FString& OutputDir) const;

	/** Usage에 따른 하위 폴더 이름 */
	static FString GetUsageSubDir(EHktTextureUsage Usage);

	/** 임포트된 텍스처에 Usage별 최적 설정 적용 */
	static void ConfigureTextureSettings(UTexture2D* Texture, const FHktTextureIntent& Intent);

	/** 내부 임포트 실행 */
	UTexture2D* ImportTextureInternal(const FString& ImageFilePath, const FString& AssetPath, const FHktTextureIntent& Intent);

	/** 캐시 재구축 (Asset Registry 스캔) */
	void RebuildCache();

	/** AssetKey → SoftObjectPath 캐시 */
	TMap<FString, FSoftObjectPath> TextureCache;

	// ── SD WebUI 상태 ──
	bool bSDServerAlive = false;
	bool bSDServerLaunching = false;
	FString SDServerStatusMessage;
	FProcHandle SDServerProcessHandle;
	FTimerHandle SDServerPollTimerHandle;
	int32 SDServerPollCount = 0;
	static constexpr int32 SDServerMaxPollCount = 60; // 최대 3분 (3초 간격)
};
