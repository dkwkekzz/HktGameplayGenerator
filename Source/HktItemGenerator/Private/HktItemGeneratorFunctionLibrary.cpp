// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktItemGeneratorFunctionLibrary.h"
#include "HktItemGeneratorSubsystem.h"
#include "Editor.h"

FString UHktItemGeneratorFunctionLibrary::McpRequestItem(const FString& JsonIntent)
{
	if (UHktItemGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktItemGeneratorSubsystem>())
	{
		return Sub->McpRequestItem(JsonIntent);
	}
	return TEXT("{\"error\": \"ItemGeneratorSubsystem not available\"}");
}

FString UHktItemGeneratorFunctionLibrary::McpImportItemMesh(const FString& FilePath, const FString& JsonIntent)
{
	if (UHktItemGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktItemGeneratorSubsystem>())
	{
		return Sub->McpImportItemMesh(FilePath, JsonIntent);
	}
	return TEXT("{\"error\": \"ItemGeneratorSubsystem not available\"}");
}

FString UHktItemGeneratorFunctionLibrary::McpGetPendingItemRequests()
{
	if (UHktItemGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktItemGeneratorSubsystem>())
	{
		return Sub->McpGetPendingRequests();
	}
	return TEXT("{\"error\": \"ItemGeneratorSubsystem not available\"}");
}

FString UHktItemGeneratorFunctionLibrary::McpListGeneratedItems(const FString& Directory)
{
	if (UHktItemGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktItemGeneratorSubsystem>())
	{
		return Sub->McpListGeneratedItems(Directory);
	}
	return TEXT("{\"error\": \"ItemGeneratorSubsystem not available\"}");
}

FString UHktItemGeneratorFunctionLibrary::McpGetSocketMappings()
{
	if (UHktItemGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktItemGeneratorSubsystem>())
	{
		return Sub->McpGetSocketMappings();
	}
	return TEXT("{\"error\": \"ItemGeneratorSubsystem not available\"}");
}
