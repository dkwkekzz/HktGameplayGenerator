// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktMeshGeneratorFunctionLibrary.h"
#include "HktMeshGeneratorSubsystem.h"
#include "Editor.h"

FString UHktMeshGeneratorFunctionLibrary::McpRequestCharacterMesh(const FString& JsonIntent)
{
	if (UHktMeshGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktMeshGeneratorSubsystem>())
	{
		return Sub->McpRequestCharacterMesh(JsonIntent);
	}
	return TEXT("{\"error\": \"MeshGeneratorSubsystem not available\"}");
}

FString UHktMeshGeneratorFunctionLibrary::McpImportMesh(const FString& FilePath, const FString& JsonIntent)
{
	if (UHktMeshGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktMeshGeneratorSubsystem>())
	{
		return Sub->McpImportMesh(FilePath, JsonIntent);
	}
	return TEXT("{\"error\": \"MeshGeneratorSubsystem not available\"}");
}

FString UHktMeshGeneratorFunctionLibrary::McpGetPendingMeshRequests()
{
	if (UHktMeshGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktMeshGeneratorSubsystem>())
	{
		return Sub->McpGetPendingRequests();
	}
	return TEXT("{\"error\": \"MeshGeneratorSubsystem not available\"}");
}

FString UHktMeshGeneratorFunctionLibrary::McpListGeneratedMeshes(const FString& Directory)
{
	if (UHktMeshGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktMeshGeneratorSubsystem>())
	{
		return Sub->McpListGeneratedMeshes(Directory);
	}
	return TEXT("{\"error\": \"MeshGeneratorSubsystem not available\"}");
}

FString UHktMeshGeneratorFunctionLibrary::McpGetSkeletonPool()
{
	if (UHktMeshGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktMeshGeneratorSubsystem>())
	{
		return Sub->McpGetSkeletonPool();
	}
	return TEXT("{\"error\": \"MeshGeneratorSubsystem not available\"}");
}
