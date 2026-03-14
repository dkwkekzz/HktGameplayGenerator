// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktAnimGeneratorFunctionLibrary.h"
#include "HktAnimGeneratorSubsystem.h"
#include "Editor.h"

FString UHktAnimGeneratorFunctionLibrary::McpRequestAnimation(const FString& JsonIntent)
{
	if (UHktAnimGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktAnimGeneratorSubsystem>())
	{
		return Sub->McpRequestAnimation(JsonIntent);
	}
	return TEXT("{\"error\": \"AnimGeneratorSubsystem not available\"}");
}

FString UHktAnimGeneratorFunctionLibrary::McpImportAnimation(const FString& FilePath, const FString& JsonIntent)
{
	if (UHktAnimGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktAnimGeneratorSubsystem>())
	{
		return Sub->McpImportAnimation(FilePath, JsonIntent);
	}
	return TEXT("{\"error\": \"AnimGeneratorSubsystem not available\"}");
}

FString UHktAnimGeneratorFunctionLibrary::McpGetPendingAnimRequests()
{
	if (UHktAnimGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktAnimGeneratorSubsystem>())
	{
		return Sub->McpGetPendingRequests();
	}
	return TEXT("{\"error\": \"AnimGeneratorSubsystem not available\"}");
}

FString UHktAnimGeneratorFunctionLibrary::McpListGeneratedAnimations(const FString& Directory)
{
	if (UHktAnimGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktAnimGeneratorSubsystem>())
	{
		return Sub->McpListGeneratedAnimations(Directory);
	}
	return TEXT("{\"error\": \"AnimGeneratorSubsystem not available\"}");
}
