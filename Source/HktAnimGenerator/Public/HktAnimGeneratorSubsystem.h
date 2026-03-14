// Copyright Hkt Studios, Inc. All Rights Reserved.
// Anim Generator EditorSubsystem — Animation 생성 API

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "HktAnimGeneratorTypes.h"
#include "HktAnimGeneratorSubsystem.generated.h"

/**
 * 애니메이션 생성 요청 상태
 */
USTRUCT(BlueprintType)
struct HKTANIMGENERATOR_API FHktAnimGenerationRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	FHktAnimIntent Intent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	FSoftObjectPath ConventionPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	bool bCompleted = false;

	/** MCP Agent용 생성 프롬프트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	FString GenerationPrompt;

	/** 예상 에셋 타입 (AnimSequence, AnimMontage, BlendSpace) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	FString ExpectedAssetType;
};

/**
 * UHktAnimGeneratorSubsystem
 *
 * Animation 생성 EditorSubsystem.
 * MCP Agent가 외부 도구 (Mixamo, Motion Diffusion 등) 로 애니메이션 생성 후 Import.
 */
UCLASS()
class HKTANIMGENERATOR_API UHktAnimGeneratorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// =========================================================================
	// 생성 API
	// =========================================================================

	/** Intent에서 애니메이션 생성 요청. Convention Path 반환. */
	FSoftObjectPath RequestAnimGeneration(const FHktAnimIntent& Intent);

	/** 외부 FBX 애니메이션 파일을 UE5 에셋으로 임포트 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator")
	UObject* ImportAnimFromFile(const FString& FilePath, const FString& DestinationPath = TEXT(""));

	// =========================================================================
	// MCP JSON API
	// =========================================================================

	/** 애니메이션 생성 요청 (JSON) */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator")
	FString McpRequestAnimation(const FString& JsonIntent);

	/** 외부 FBX 임포트 (JSON) */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator")
	FString McpImportAnimation(const FString& FilePath, const FString& JsonIntent);

	/** 펜딩 생성 요청 목록 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator")
	FString McpGetPendingRequests();

	/** 생성된 애니메이션 목록 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator")
	FString McpListGeneratedAnimations(const FString& Directory = TEXT(""));

private:
	FString ResolveOutputDir(const FString& OutputDir) const;
	FString BuildPrompt(const FHktAnimIntent& Intent) const;
	FSoftObjectPath ResolveConventionPath(const FHktAnimIntent& Intent) const;
	static FString DetermineExpectedAssetType(const FHktAnimIntent& Intent);

	TArray<FHktAnimGenerationRequest> PendingRequests;

	UPROPERTY()
	TObjectPtr<class UHktAnimGeneratorHandler> AnimHandler;
};
