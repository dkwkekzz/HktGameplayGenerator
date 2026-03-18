// Copyright Hkt Studios, Inc. All Rights Reserved.
// MCP/LLM 호출용 Animation Generator API

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HktAnimGeneratorFunctionLibrary.generated.h"

/**
 * UHktAnimGeneratorFunctionLibrary
 *
 * MCP Agent가 호출하는 애니메이션 생성 함수 모음.
 *
 * === 카테고리 ===
 * - 기존: request/import/list 워크플로우
 * - ABP: AnimBlueprint 생성, 정보 조회, 컴파일
 * - StateMachine: State Machine 추가, State/Transition 관리
 * - AnimGraph: 노드 추가, 연결, 파라미터
 * - Montage: Montage 생성, Section, Slot
 * - BlendSpace: BlendSpace 생성, 샘플, 축 설정
 * - Skeleton: 스켈레톤 정보, 소켓, 가상 본
 * - Guide: AI Agent용 API 가이드
 */
UCLASS()
class HKTANIMGENERATOR_API UHktAnimGeneratorFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ===== 기존 API =====

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpRequestAnimation(const FString& JsonIntent);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpImportAnimation(const FString& FilePath, const FString& JsonIntent);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpGetPendingAnimRequests();

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpListGeneratedAnimations(const FString& Directory = TEXT(""));

	// ===== Animation Blueprint API =====

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpCreateAnimBlueprint(const FString& JsonConfig);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpGetAnimBlueprintInfo(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpCompileAnimBlueprint(const FString& AssetPath);

	// ===== State Machine API =====

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpAddStateMachine(const FString& JsonConfig);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpAddState(const FString& JsonConfig);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpAddTransition(const FString& JsonConfig);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpConnectStateMachineToOutput(const FString& JsonConfig);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpSetStateAnimation(const FString& JsonConfig);

	// ===== AnimGraph Node API =====

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpAddAnimGraphNode(const FString& JsonConfig);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpConnectAnimNodes(const FString& JsonConfig);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpAddAnimParameter(const FString& JsonConfig);

	// ===== Montage API =====

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpCreateMontage(const FString& JsonConfig);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpAddMontageSection(const FString& JsonConfig);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpSetMontageSlot(const FString& AssetPath, const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpLinkMontageSections(const FString& JsonConfig);

	// ===== BlendSpace API =====

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpCreateBlendSpace(const FString& JsonConfig);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpAddBlendSpaceSample(const FString& JsonConfig);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpSetBlendSpaceAxis(const FString& JsonConfig);

	// ===== Skeleton API =====

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpGetSkeletonInfo(const FString& SkeletonPath);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpAddSocket(const FString& JsonConfig);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpAddVirtualBone(const FString& JsonConfig);

	// ===== Guide API =====

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|MCP")
	static FString McpGetAnimApiGuide();
};
