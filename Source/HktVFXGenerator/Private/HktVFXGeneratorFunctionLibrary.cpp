// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVFXGeneratorFunctionLibrary.h"
#include "HktVFXGeneratorSubsystem.h"
#include "HktVFXGeneratorSettings.h"
#include "NiagaraSystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktVFXMcp, Log, All);

namespace
{
	// JSON 응답 헬퍼
	FString MakeSuccessResponse(const FString& AssetPath)
	{
		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("success"), true);
		Writer->WriteValue(TEXT("assetPath"), AssetPath);
		Writer->WriteObjectEnd();
		Writer->Close();
		return Output;
	}

	FString MakeErrorResponse(const FString& Error)
	{
		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("success"), false);
		Writer->WriteValue(TEXT("error"), Error);
		Writer->WriteObjectEnd();
		Writer->Close();
		return Output;
	}

	UHktVFXGeneratorSubsystem* GetSubsystem()
	{
		return GEditor ? GEditor->GetEditorSubsystem<UHktVFXGeneratorSubsystem>() : nullptr;
	}
}

FString UHktVFXGeneratorFunctionLibrary::McpBuildNiagaraSystem(
	const FString& JsonConfig,
	const FString& OutputDir)
{
	UHktVFXGeneratorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return MakeErrorResponse(TEXT("VFXGenerator subsystem not available"));
	}

	UNiagaraSystem* Result = Subsystem->BuildNiagaraFromJson(JsonConfig, OutputDir);
	if (!Result)
	{
		return MakeErrorResponse(TEXT("Failed to build Niagara system from JSON config"));
	}

	UE_LOG(LogHktVFXMcp, Log, TEXT("MCP: Built Niagara system: %s"), *Result->GetPathName());
	return MakeSuccessResponse(Result->GetPathName());
}

FString UHktVFXGeneratorFunctionLibrary::McpBuildPresetExplosion(
	const FString& Name,
	float R, float G, float B,
	float Intensity,
	const FString& OutputDir)
{
	UHktVFXGeneratorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return MakeErrorResponse(TEXT("VFXGenerator subsystem not available"));
	}

	FLinearColor Color(R, G, B, 1.0f);
	UNiagaraSystem* Result = Subsystem->BuildPresetExplosion(Name, Color, Intensity, OutputDir);
	if (!Result)
	{
		return MakeErrorResponse(TEXT("Failed to build preset explosion"));
	}

	UE_LOG(LogHktVFXMcp, Log, TEXT("MCP: Built preset explosion: %s"), *Result->GetPathName());
	return MakeSuccessResponse(Result->GetPathName());
}

FString UHktVFXGeneratorFunctionLibrary::McpGetVFXConfigSchema()
{
	UHktVFXGeneratorSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return FHktVFXNiagaraConfig::GetSchemaJson();
	}

	return Subsystem->GetConfigSchemaJson();
}

FString UHktVFXGeneratorFunctionLibrary::McpListGeneratedVFX(const FString& Directory)
{
	const FString ResolvedDir = Directory.IsEmpty()
		? UHktVFXGeneratorSettings::Get()->DefaultOutputDirectory
		: Directory;

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByPath(FName(*ResolvedDir), Assets, true);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteArrayStart(TEXT("assets"));

	for (const FAssetData& Asset : Assets)
	{
		if (Asset.AssetClassPath.GetAssetName() == TEXT("NiagaraSystem"))
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("name"), Asset.AssetName.ToString());
			Writer->WriteValue(TEXT("path"), Asset.GetObjectPathString());
			Writer->WriteObjectEnd();
		}
	}

	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();
	Writer->Close();

	return Output;
}
