// Copyright Hkt Studios, Inc. All Rights Reserved.
// Mesh Generator EditorSubsystem — Entity/Character 메시 생성 API

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "HktAnimGeneratorTypes.h"
#include "HktMeshGeneratorSubsystem.generated.h"

/**
 * 메시 생성 요청 상태
 */
USTRUCT(BlueprintType)
struct HKTMESHGENERATOR_API FHktMeshGenerationRequest
{
	GENERATED_BODY()

	/** 캐릭터 의도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	FHktCharacterIntent Intent;

	/** Convention Path (생성 예정 위치) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	FSoftObjectPath ConventionPath;

	/** 생성 완료 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	bool bCompleted = false;

	/** MCP Agent용 프롬프트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	FString GenerationPrompt;
};

/**
 * UHktMeshGeneratorSubsystem
 *
 * Entity/Character 메시 생성 EditorSubsystem.
 * MCP Agent가 외부 3D 생성 도구(Meshy, Rodin 등)로 메시 생성 후 Import.
 */
UCLASS()
class HKTMESHGENERATOR_API UHktMeshGeneratorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// =========================================================================
	// 생성 API
	// =========================================================================

	/** Intent에서 메시 생성 요청. Convention Path 반환. */
	FSoftObjectPath RequestMeshGeneration(const FHktCharacterIntent& Intent);

	/** 외부 FBX/OBJ 파일을 UE5 에셋으로 임포트 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MeshGenerator")
	UObject* ImportMeshFromFile(const FString& FilePath, const FString& DestinationPath = TEXT(""));

	// =========================================================================
	// MCP JSON API
	// =========================================================================

	/** 캐릭터 메시 생성 요청 (JSON) — Intent 파싱 + 프롬프트 반환 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MeshGenerator")
	FString McpRequestCharacterMesh(const FString& JsonIntent);

	/** 외부 파일 임포트 (JSON) — FBX/OBJ → UE5 에셋 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MeshGenerator")
	FString McpImportMesh(const FString& FilePath, const FString& JsonIntent);

	/** 펜딩 생성 요청 목록 조회 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MeshGenerator")
	FString McpGetPendingRequests();

	/** 생성된 메시 에셋 목록 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MeshGenerator")
	FString McpListGeneratedMeshes(const FString& Directory = TEXT(""));

	/** 스켈레톤 풀 정보 반환 (사용 가능한 Base Skeleton 목록) */
	UFUNCTION(BlueprintCallable, Category = "HKT|MeshGenerator")
	FString McpGetSkeletonPool();

private:
	FString ResolveOutputDir(const FString& OutputDir) const;
	FString BuildPrompt(const FHktCharacterIntent& Intent) const;
	FSoftObjectPath ResolveConventionPath(const FHktCharacterIntent& Intent) const;

	TArray<FHktMeshGenerationRequest> PendingRequests;

	UPROPERTY()
	TObjectPtr<class UHktMeshGeneratorHandler> MeshHandler;
};
