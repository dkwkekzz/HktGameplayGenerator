// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktTextureGeneratorFunctionLibrary.h"
#include "HktTextureGeneratorSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktTextureMcp, Log, All);

namespace HktTexture
{
	UHktTextureGeneratorSubsystem* GetTexSubsystem()
	{
		return GEditor ? GEditor->GetEditorSubsystem<UHktTextureGeneratorSubsystem>() : nullptr;
	}

	FString MakeError(const FString& Msg)
	{
		return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\"}"), *Msg);
	}
}

FString UHktTextureGeneratorFunctionLibrary::McpGenerateTexture(
	const FString& JsonIntent, const FString& OutputDir)
{
	UHktTextureGeneratorSubsystem* Sub = HktTexture::GetTexSubsystem();
	if (!Sub) return HktTexture::MakeError(TEXT("TextureGenerator subsystem not available"));
	return Sub->McpGenerateTexture(JsonIntent, OutputDir);
}

FString UHktTextureGeneratorFunctionLibrary::McpImportTexture(
	const FString& ImageFilePath, const FString& JsonIntent, const FString& OutputDir)
{
	UHktTextureGeneratorSubsystem* Sub = HktTexture::GetTexSubsystem();
	if (!Sub) return HktTexture::MakeError(TEXT("TextureGenerator subsystem not available"));
	return Sub->McpImportTexture(ImageFilePath, JsonIntent, OutputDir);
}

FString UHktTextureGeneratorFunctionLibrary::McpGetPendingRequests(const FString& JsonRequests)
{
	UHktTextureGeneratorSubsystem* Sub = HktTexture::GetTexSubsystem();
	if (!Sub) return HktTexture::MakeError(TEXT("TextureGenerator subsystem not available"));
	return Sub->McpGetPendingRequests(JsonRequests);
}

FString UHktTextureGeneratorFunctionLibrary::McpCheckSDServerStatus()
{
	UHktTextureGeneratorSubsystem* Sub = HktTexture::GetTexSubsystem();
	if (!Sub) return HktTexture::MakeError(TEXT("TextureGenerator subsystem not available"));
	return Sub->McpCheckSDServerStatus();
}

FString UHktTextureGeneratorFunctionLibrary::McpListGeneratedTextures(const FString& Directory)
{
	UHktTextureGeneratorSubsystem* Sub = HktTexture::GetTexSubsystem();
	if (!Sub) return HktTexture::MakeError(TEXT("TextureGenerator subsystem not available"));
	return Sub->McpListGeneratedTextures(Directory);
}
