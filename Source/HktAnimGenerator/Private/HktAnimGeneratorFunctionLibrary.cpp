// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktAnimGeneratorFunctionLibrary.h"
#include "HktAnimGeneratorSubsystem.h"
#include "Editor.h"

#define GET_SUBSYSTEM_OR_ERROR() \
	UHktAnimGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktAnimGeneratorSubsystem>(); \
	if (!Sub) return TEXT("{\"success\":false,\"error\":\"AnimGeneratorSubsystem not available\"}");

// ===== 기존 API =====

FString UHktAnimGeneratorFunctionLibrary::McpRequestAnimation(const FString& JsonIntent)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpRequestAnimation(JsonIntent);
}

FString UHktAnimGeneratorFunctionLibrary::McpImportAnimation(const FString& FilePath, const FString& JsonIntent)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpImportAnimation(FilePath, JsonIntent);
}

FString UHktAnimGeneratorFunctionLibrary::McpGetPendingAnimRequests()
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpGetPendingRequests();
}

FString UHktAnimGeneratorFunctionLibrary::McpListGeneratedAnimations(const FString& Directory)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpListGeneratedAnimations(Directory);
}

// ===== Animation Blueprint API =====

FString UHktAnimGeneratorFunctionLibrary::McpCreateAnimBlueprint(const FString& JsonConfig)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpCreateAnimBlueprint(JsonConfig);
}

FString UHktAnimGeneratorFunctionLibrary::McpGetAnimBlueprintInfo(const FString& AssetPath)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpGetAnimBlueprintInfo(AssetPath);
}

FString UHktAnimGeneratorFunctionLibrary::McpCompileAnimBlueprint(const FString& AssetPath)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpCompileAnimBlueprint(AssetPath);
}

// ===== State Machine API =====

FString UHktAnimGeneratorFunctionLibrary::McpAddStateMachine(const FString& JsonConfig)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpAddStateMachine(JsonConfig);
}

FString UHktAnimGeneratorFunctionLibrary::McpAddState(const FString& JsonConfig)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpAddState(JsonConfig);
}

FString UHktAnimGeneratorFunctionLibrary::McpAddTransition(const FString& JsonConfig)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpAddTransition(JsonConfig);
}

FString UHktAnimGeneratorFunctionLibrary::McpConnectStateMachineToOutput(const FString& JsonConfig)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpConnectStateMachineToOutput(JsonConfig);
}

FString UHktAnimGeneratorFunctionLibrary::McpSetStateAnimation(const FString& JsonConfig)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpSetStateAnimation(JsonConfig);
}

// ===== AnimGraph Node API =====

FString UHktAnimGeneratorFunctionLibrary::McpAddAnimGraphNode(const FString& JsonConfig)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpAddAnimGraphNode(JsonConfig);
}

FString UHktAnimGeneratorFunctionLibrary::McpConnectAnimNodes(const FString& JsonConfig)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpConnectAnimNodes(JsonConfig);
}

FString UHktAnimGeneratorFunctionLibrary::McpAddAnimParameter(const FString& JsonConfig)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpAddAnimParameter(JsonConfig);
}

// ===== Montage API =====

FString UHktAnimGeneratorFunctionLibrary::McpCreateMontage(const FString& JsonConfig)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpCreateMontage(JsonConfig);
}

FString UHktAnimGeneratorFunctionLibrary::McpAddMontageSection(const FString& JsonConfig)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpAddMontageSection(JsonConfig);
}

FString UHktAnimGeneratorFunctionLibrary::McpSetMontageSlot(const FString& AssetPath, const FString& SlotName)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpSetMontageSlot(AssetPath, SlotName);
}

FString UHktAnimGeneratorFunctionLibrary::McpLinkMontageSections(const FString& JsonConfig)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpLinkMontageSections(JsonConfig);
}

// ===== BlendSpace API =====

FString UHktAnimGeneratorFunctionLibrary::McpCreateBlendSpace(const FString& JsonConfig)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpCreateBlendSpace(JsonConfig);
}

FString UHktAnimGeneratorFunctionLibrary::McpAddBlendSpaceSample(const FString& JsonConfig)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpAddBlendSpaceSample(JsonConfig);
}

FString UHktAnimGeneratorFunctionLibrary::McpSetBlendSpaceAxis(const FString& JsonConfig)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpSetBlendSpaceAxis(JsonConfig);
}

// ===== Skeleton API =====

FString UHktAnimGeneratorFunctionLibrary::McpGetSkeletonInfo(const FString& SkeletonPath)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpGetSkeletonInfo(SkeletonPath);
}

FString UHktAnimGeneratorFunctionLibrary::McpAddSocket(const FString& JsonConfig)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpAddSocket(JsonConfig);
}

FString UHktAnimGeneratorFunctionLibrary::McpAddVirtualBone(const FString& JsonConfig)
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpAddVirtualBone(JsonConfig);
}

// ===== Guide API =====

FString UHktAnimGeneratorFunctionLibrary::McpGetAnimApiGuide()
{
	GET_SUBSYSTEM_OR_ERROR();
	return Sub->McpGetAnimApiGuide();
}

#undef GET_SUBSYSTEM_OR_ERROR
