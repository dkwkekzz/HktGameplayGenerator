// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktMeshGeneratorFunctionLibrary.h"
#include "HktMeshGeneratorSubsystem.h"
#include "HktShapeFactory.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	UHktShapeFactory* GetShapeFactory()
	{
		static TWeakObjectPtr<UHktShapeFactory> Singleton;
		if (!Singleton.IsValid())
		{
			Singleton = NewObject<UHktShapeFactory>(GetTransientPackage());
			Singleton->AddToRoot();
		}
		return Singleton.Get();
	}
}

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

// ============================================================================
// Shape Generator MCP
// ============================================================================

FString UHktMeshGeneratorFunctionLibrary::McpCreateShape(const FString& JsonParams, const FString& OutputDir)
{
	UHktShapeFactory* Factory = GetShapeFactory();
	if (!Factory)
	{
		return TEXT("{\"success\":false,\"error\":\"ShapeFactory not available\"}");
	}

	FString AssetPath = Factory->CreateShapeAsset(JsonParams, OutputDir);
	if (AssetPath.IsEmpty())
	{
		return TEXT("{\"success\":false,\"error\":\"Failed to create shape asset\"}");
	}

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("success"), true);
	Writer->WriteValue(TEXT("assetPath"), AssetPath);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Output;
}

FString UHktMeshGeneratorFunctionLibrary::McpListShapes()
{
	UHktShapeFactory* Factory = GetShapeFactory();
	if (!Factory)
	{
		return TEXT("{\"shapes\":[]}");
	}
	return Factory->GetCatalogJson();
}

FString UHktMeshGeneratorFunctionLibrary::McpFindShape(const FString& ShapeName)
{
	UHktShapeFactory* Factory = GetShapeFactory();
	if (!Factory)
	{
		return TEXT("{\"success\":false,\"error\":\"ShapeFactory not available\"}");
	}

	FString Path = Factory->FindShapeByName(ShapeName);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("success"), !Path.IsEmpty());
	if (!Path.IsEmpty())
	{
		Writer->WriteValue(TEXT("assetPath"), Path);
	}
	Writer->WriteObjectEnd();
	Writer->Close();
	return Output;
}
