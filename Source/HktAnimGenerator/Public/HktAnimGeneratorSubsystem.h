// Copyright Hkt Studios, Inc. All Rights Reserved.
// Anim Generator EditorSubsystem — Animation 생성 + Blueprint/StateMachine/Montage API

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "HktAnimGeneratorTypes.h"
#include "HktAnimGeneratorSubsystem.generated.h"

class UAnimBlueprint;
class UAnimationStateMachineGraph;
class UAnimGraphNode_StateMachine;
class UAnimStateNode;

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
 * ABP 생성, State Machine 구축, Montage/BlendSpace 관리까지 확장.
 */
UCLASS()
class HKTANIMGENERATOR_API UHktAnimGeneratorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// =========================================================================
	// 기존 생성 API
	// =========================================================================

	/** Intent에서 애니메이션 생성 요청. Convention Path 반환. */
	FSoftObjectPath RequestAnimGeneration(const FHktAnimIntent& Intent);

	/** 외부 FBX 애니메이션 파일을 UE5 에셋으로 임포트 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator")
	UObject* ImportAnimFromFile(const FString& FilePath, const FString& DestinationPath = TEXT(""));

	// =========================================================================
	// 기존 MCP JSON API
	// =========================================================================

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator")
	FString McpRequestAnimation(const FString& JsonIntent);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator")
	FString McpImportAnimation(const FString& FilePath, const FString& JsonIntent);

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator")
	FString McpGetPendingRequests();

	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator")
	FString McpListGeneratedAnimations(const FString& Directory = TEXT(""));

	// =========================================================================
	// Animation Blueprint API
	// =========================================================================

	/**
	 * AnimBlueprint 생성.
	 * JsonConfig: { "name": "ABP_Hero", "packagePath": "/Game/Generated/AnimBP",
	 *               "skeletonPath": "/Game/Characters/Skeleton.Skeleton" }
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|ABP")
	FString McpCreateAnimBlueprint(const FString& JsonConfig);

	/**
	 * AnimBlueprint 정보 조회 — 그래프, 노드, 상태머신 구조.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|ABP")
	FString McpGetAnimBlueprintInfo(const FString& AssetPath);

	/**
	 * AnimBlueprint 컴파일.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|ABP")
	FString McpCompileAnimBlueprint(const FString& AssetPath);

	// =========================================================================
	// State Machine API
	// =========================================================================

	/**
	 * AnimBlueprint의 AnimGraph에 State Machine 추가.
	 * JsonConfig: { "abpPath": "...", "machineName": "Locomotion" }
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|StateMachine")
	FString McpAddStateMachine(const FString& JsonConfig);

	/**
	 * State Machine에 State 추가.
	 * JsonConfig: { "abpPath": "...", "machineName": "...", "stateName": "Idle",
	 *               "animAssetPath": "/Game/.../Anim_Idle" (optional) }
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|StateMachine")
	FString McpAddState(const FString& JsonConfig);

	/**
	 * State 간 Transition 추가.
	 * JsonConfig: { "abpPath": "...", "machineName": "...",
	 *               "fromState": "Idle", "toState": "Run",
	 *               "transitionRule": { "type": "boolParam", "paramName": "bIsRunning" } }
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|StateMachine")
	FString McpAddTransition(const FString& JsonConfig);

	/**
	 * State Machine 노드를 AnimGraph의 OutputPose에 연결.
	 * JsonConfig: { "abpPath": "...", "machineName": "..." }
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|StateMachine")
	FString McpConnectStateMachineToOutput(const FString& JsonConfig);

	/**
	 * State의 애니메이션 에셋 설정/변경.
	 * JsonConfig: { "abpPath": "...", "machineName": "...",
	 *               "stateName": "Idle", "animAssetPath": "/Game/.../Anim_Idle" }
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|StateMachine")
	FString McpSetStateAnimation(const FString& JsonConfig);

	// =========================================================================
	// AnimGraph Node API
	// =========================================================================

	/**
	 * AnimGraph에 노드 추가 (BlendByBool, TwoWayBlend, LayeredBlend, CachePose 등).
	 * JsonConfig: { "abpPath": "...", "nodeType": "BlendListByBool",
	 *               "posX": 0, "posY": 0, "config": { ... } }
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|AnimGraph")
	FString McpAddAnimGraphNode(const FString& JsonConfig);

	/**
	 * AnimGraph 노드 간 핀 연결.
	 * JsonConfig: { "abpPath": "...",
	 *               "sourceNodeId": "...", "sourcePinName": "Pose",
	 *               "targetNodeId": "...", "targetPinName": "Pose" }
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|AnimGraph")
	FString McpConnectAnimNodes(const FString& JsonConfig);

	/**
	 * ABP에 Bool/Float/Int 파라미터 추가.
	 * JsonConfig: { "abpPath": "...", "paramName": "bIsRunning",
	 *               "paramType": "bool", "defaultValue": "false" }
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|AnimGraph")
	FString McpAddAnimParameter(const FString& JsonConfig);

	// =========================================================================
	// Montage API
	// =========================================================================

	/**
	 * AnimSequence에서 Montage 생성.
	 * JsonConfig: { "name": "Montage_Attack", "packagePath": "/Game/Generated/Montages",
	 *               "animSequencePath": "/Game/.../Anim_Attack",
	 *               "slotName": "DefaultSlot" (optional),
	 *               "sections": [ { "name": "Windup", "startTime": 0.0 }, ... ] (optional) }
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|Montage")
	FString McpCreateMontage(const FString& JsonConfig);

	/**
	 * Montage에 Section 추가.
	 * JsonConfig: { "montagePath": "...", "sectionName": "Recovery", "startTime": 0.5 }
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|Montage")
	FString McpAddMontageSection(const FString& JsonConfig);

	/**
	 * Montage의 슬롯 이름 변경.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|Montage")
	FString McpSetMontageSlot(const FString& AssetPath, const FString& SlotName);

	/**
	 * Montage Section 간 링크 설정.
	 * JsonConfig: { "montagePath": "...", "fromSection": "Windup", "toSection": "Impact" }
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|Montage")
	FString McpLinkMontageSections(const FString& JsonConfig);

	// =========================================================================
	// Blend Space API
	// =========================================================================

	/**
	 * BlendSpace 생성.
	 * JsonConfig: { "name": "BS_Locomotion", "packagePath": "/Game/Generated/BlendSpaces",
	 *               "skeletonPath": "...",
	 *               "axisX": { "name": "Speed", "min": 0, "max": 600 },
	 *               "axisY": { "name": "Direction", "min": -180, "max": 180 } (optional, 없으면 1D),
	 *               "samples": [ { "animPath": "...", "x": 0, "y": 0 }, ... ] (optional) }
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|BlendSpace")
	FString McpCreateBlendSpace(const FString& JsonConfig);

	/**
	 * BlendSpace에 샘플 포인트 추가.
	 * JsonConfig: { "blendSpacePath": "...", "animPath": "...", "x": 100.0, "y": 0.0 }
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|BlendSpace")
	FString McpAddBlendSpaceSample(const FString& JsonConfig);

	/**
	 * BlendSpace 축 설정 변경.
	 * JsonConfig: { "blendSpacePath": "...",
	 *               "axis": "X" or "Y",
	 *               "name": "Speed", "min": 0, "max": 600 }
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|BlendSpace")
	FString McpSetBlendSpaceAxis(const FString& JsonConfig);

	// =========================================================================
	// Skeleton API
	// =========================================================================

	/**
	 * 스켈레톤 정보 조회 (본 계층, 소켓, 가상 본).
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|Skeleton")
	FString McpGetSkeletonInfo(const FString& SkeletonPath);

	/**
	 * 스켈레톤에 소켓 추가.
	 * JsonConfig: { "skeletonPath": "...", "boneName": "hand_r",
	 *               "socketName": "WeaponSocket",
	 *               "relativeLocation": { "x": 0, "y": 0, "z": 0 },
	 *               "relativeRotation": { "pitch": 0, "yaw": 0, "roll": 0 },
	 *               "relativeScale": { "x": 1, "y": 1, "z": 1 } }
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|Skeleton")
	FString McpAddSocket(const FString& JsonConfig);

	/**
	 * 스켈레톤에 Virtual Bone 추가.
	 * JsonConfig: { "skeletonPath": "...", "sourceBone": "hand_r",
	 *               "targetBone": "hand_l", "virtualBoneName": "VB_hands_center" }
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|Skeleton")
	FString McpAddVirtualBone(const FString& JsonConfig);

	// =========================================================================
	// Prompt Guide API
	// =========================================================================

	/**
	 * AI Agent를 위한 Animation API 사용 가이드.
	 * ABP 생성, State Machine, Montage, BlendSpace 워크플로우 및 JSON 스키마 포함.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|AnimGenerator|Guide")
	FString McpGetAnimApiGuide();

private:
	FString ResolveOutputDir(const FString& OutputDir) const;
	FString BuildPrompt(const FHktAnimIntent& Intent) const;
	FSoftObjectPath ResolveConventionPath(const FHktAnimIntent& Intent) const;
	static FString DetermineExpectedAssetType(const FHktAnimIntent& Intent);

	// ABP 헬퍼
	UAnimBlueprint* LoadAnimBlueprint(const FString& AssetPath) const;
	UEdGraph* FindAnimGraph(UAnimBlueprint* ABP) const;
	UAnimGraphNode_StateMachine* FindStateMachineNode(UAnimBlueprint* ABP, const FString& MachineName) const;
	UAnimStateNode* FindStateNode(UAnimationStateMachineGraph* SMGraph, const FString& StateName) const;
	bool SetStateAnimationInternal(UAnimBlueprint* ABP, UAnimStateNode* StateNode, const FString& AnimAssetPath);

	static FString MakeErrorJson(const FString& Error);
	static FString MakeSuccessJson(const TSharedRef<FJsonObject>& Data);

	TArray<FHktAnimGenerationRequest> PendingRequests;

	UPROPERTY()
	TObjectPtr<class UHktAnimGeneratorHandler> AnimHandler;
};
