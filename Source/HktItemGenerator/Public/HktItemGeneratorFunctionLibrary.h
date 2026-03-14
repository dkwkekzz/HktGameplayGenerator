// Copyright Hkt Studios, Inc. All Rights Reserved.
// MCP/LLM 호출용 Item Generator API

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HktItemGeneratorFunctionLibrary.generated.h"

/**
 * UHktItemGeneratorFunctionLibrary
 *
 * MCP Agent가 호출하는 아이템 생성 함수 모음.
 *
 * === MCP 워크플로우 ===
 * 1. McpRequestItem(intent) → Convention Path + 메시/아이콘 프롬프트
 * 2. Agent가 외부 도구로 3D 메시 생성 + 아이콘 이미지 생성
 * 3. McpImportItemMesh(filePath, intent) → UE5 StaticMesh 임포트
 * 4. 아이콘은 HktTextureGenerator.McpImportTexture()로 임포트
 * 5. McpGetSocketMappings() → 장착 소켓 정보
 */
UCLASS()
class HKTITEMGENERATOR_API UHktItemGeneratorFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 아이템 생성 요청 */
	UFUNCTION(BlueprintCallable, Category = "HKT|ItemGenerator|MCP")
	static FString McpRequestItem(const FString& JsonIntent);

	/** 외부 메시 임포트 */
	UFUNCTION(BlueprintCallable, Category = "HKT|ItemGenerator|MCP")
	static FString McpImportItemMesh(const FString& FilePath, const FString& JsonIntent);

	/** 펜딩 요청 목록 */
	UFUNCTION(BlueprintCallable, Category = "HKT|ItemGenerator|MCP")
	static FString McpGetPendingItemRequests();

	/** 생성된 아이템 목록 */
	UFUNCTION(BlueprintCallable, Category = "HKT|ItemGenerator|MCP")
	static FString McpListGeneratedItems(const FString& Directory = TEXT(""));

	/** 소켓 매핑 정보 */
	UFUNCTION(BlueprintCallable, Category = "HKT|ItemGenerator|MCP")
	static FString McpGetSocketMappings();
};
