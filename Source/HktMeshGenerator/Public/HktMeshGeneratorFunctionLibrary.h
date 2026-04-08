// Copyright Hkt Studios, Inc. All Rights Reserved.
// MCP/LLM 호출용 Mesh Generator API

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HktMeshGeneratorFunctionLibrary.generated.h"

/**
 * UHktMeshGeneratorFunctionLibrary
 *
 * MCP Agent가 호출하는 메시 생성 함수 모음.
 *
 * === MCP 워크플로우 ===
 * 1. McpRequestCharacterMesh(intent) → Convention Path + 생성 프롬프트
 * 2. Agent가 외부 도구로 3D 메시 생성 (Meshy, Rodin 등)
 * 3. McpImportMesh(filePath, intent) → UE5 에셋 임포트
 * 4. McpGetSkeletonPool() → 사용 가능한 Base Skeleton 조회
 */
UCLASS()
class HKTMESHGENERATOR_API UHktMeshGeneratorFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 캐릭터 메시 생성 요청 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MeshGenerator|MCP")
	static FString McpRequestCharacterMesh(const FString& JsonIntent);

	/** 외부 파일 임포트 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MeshGenerator|MCP")
	static FString McpImportMesh(const FString& FilePath, const FString& JsonIntent);

	/** 펜딩 요청 목록 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MeshGenerator|MCP")
	static FString McpGetPendingMeshRequests();

	/** 생성된 메시 목록 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MeshGenerator|MCP")
	static FString McpListGeneratedMeshes(const FString& Directory = TEXT(""));

	/** 스켈레톤 풀 정보 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MeshGenerator|MCP")
	static FString McpGetSkeletonPool();

	// =========================================================================
	// Shape Generator (Layer 3 — MCP 노출)
	// =========================================================================

	/** Shape StaticMesh 에셋 생성. JSON 파라미터로 타입/크기 지정. 해시 캐싱. */
	UFUNCTION(BlueprintCallable, Category = "HKT|MeshGenerator|MCP")
	static FString McpCreateShape(const FString& JsonParams, const FString& OutputDir = TEXT(""));

	/** Shape 카탈로그 조회 (기존 에셋 + 캐시 목록) */
	UFUNCTION(BlueprintCallable, Category = "HKT|MeshGenerator|MCP")
	static FString McpListShapes();

	/** 이름으로 Shape 에셋 경로 검색 */
	UFUNCTION(BlueprintCallable, Category = "HKT|MeshGenerator|MCP")
	static FString McpFindShape(const FString& ShapeName);
};
