// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktAnimGeneratorSubsystem.h"
#include "HktAnimGeneratorSettings.h"
#include "HktAnimGeneratorHandler.h"
#include "HktGeneratorRouter.h"
#include "HktAssetSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/BlendSpace.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktAnimGeneratorSubsystem, Log, All);

void UHktAnimGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Handler 생성 및 Router 등록
	if (UHktGeneratorRouter* Router = GEditor->GetEditorSubsystem<UHktGeneratorRouter>())
	{
		AnimHandler = NewObject<UHktAnimGeneratorHandler>(this);
		Router->RegisterHandler(TScriptInterface<IHktGeneratorHandler>(AnimHandler));
		UE_LOG(LogHktAnimGeneratorSubsystem, Log, TEXT("AnimGeneratorHandler registered with Router"));
	}

	UE_LOG(LogHktAnimGeneratorSubsystem, Log, TEXT("AnimGeneratorSubsystem initialized"));
}

void UHktAnimGeneratorSubsystem::Deinitialize()
{
	PendingRequests.Empty();
	Super::Deinitialize();
}

FSoftObjectPath UHktAnimGeneratorSubsystem::RequestAnimGeneration(const FHktAnimIntent& Intent)
{
	FSoftObjectPath ConventionPath = ResolveConventionPath(Intent);

	// 이미 에셋이 존재하면 바로 반환
	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FAssetData AssetData = ARM.Get().GetAssetByObjectPath(ConventionPath);
	if (AssetData.IsValid())
	{
		UE_LOG(LogHktAnimGeneratorSubsystem, Log, TEXT("Animation already exists: %s"), *ConventionPath.ToString());
		return ConventionPath;
	}

	// 펜딩 요청 등록
	FHktAnimGenerationRequest Request;
	Request.Intent = Intent;
	Request.ConventionPath = ConventionPath;
	Request.GenerationPrompt = BuildPrompt(Intent);
	Request.ExpectedAssetType = DetermineExpectedAssetType(Intent);
	PendingRequests.Add(Request);

	UE_LOG(LogHktAnimGeneratorSubsystem, Log, TEXT("Animation generation pending: %s (%s)"), *ConventionPath.ToString(), *Request.ExpectedAssetType);
	return ConventionPath;
}

UObject* UHktAnimGeneratorSubsystem::ImportAnimFromFile(const FString& FilePath, const FString& DestinationPath)
{
	// TODO: Phase 2 — FBX animation → UE5 AnimSequence 임포트
	UE_LOG(LogHktAnimGeneratorSubsystem, Warning, TEXT("ImportAnimFromFile: Not yet implemented (Phase 2). File: %s"), *FilePath);
	return nullptr;
}

FString UHktAnimGeneratorSubsystem::McpRequestAnimation(const FString& JsonIntent)
{
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonIntent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		return TEXT("{\"error\": \"Invalid JSON\"}");
	}

	FHktAnimIntent Intent;
	Intent.Layer = JsonObj->GetStringField(TEXT("layer"));
	Intent.Type = JsonObj->GetStringField(TEXT("type"));
	Intent.Name = JsonObj->GetStringField(TEXT("name"));

	FString SkeletonStr;
	if (JsonObj->TryGetStringField(TEXT("skeletonType"), SkeletonStr))
	{
		if (SkeletonStr == TEXT("Quadruped")) Intent.SkeletonType = EHktSkeletonType::Quadruped;
		else if (SkeletonStr == TEXT("Custom")) Intent.SkeletonType = EHktSkeletonType::Custom;
	}

	const TArray<TSharedPtr<FJsonValue>>* Keywords;
	if (JsonObj->TryGetArrayField(TEXT("styleKeywords"), Keywords))
	{
		for (const auto& Val : *Keywords)
		{
			Intent.StyleKeywords.Add(Val->AsString());
		}
	}

	// 태그 생성
	FString TagStr = FString::Printf(TEXT("Anim.%s.%s.%s"), *Intent.Layer, *Intent.Type, *Intent.Name);
	Intent.AnimTag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);

	FSoftObjectPath Path = RequestAnimGeneration(Intent);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("layer"), Intent.Layer);
	Result->SetStringField(TEXT("type"), Intent.Type);
	Result->SetStringField(TEXT("name"), Intent.Name);
	Result->SetStringField(TEXT("conventionPath"), Path.ToString());
	Result->SetStringField(TEXT("prompt"), BuildPrompt(Intent));
	Result->SetStringField(TEXT("expectedAssetType"), DetermineExpectedAssetType(Intent));
	Result->SetBoolField(TEXT("pending"), true);

	FString ResultStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
	FJsonSerializer::Serialize(Result, Writer);
	return ResultStr;
}

FString UHktAnimGeneratorSubsystem::McpImportAnimation(const FString& FilePath, const FString& JsonIntent)
{
	UObject* Imported = ImportAnimFromFile(FilePath);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	if (Imported)
	{
		Result->SetStringField(TEXT("assetPath"), Imported->GetPathName());
		Result->SetBoolField(TEXT("success"), true);
	}
	else
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), TEXT("Import not yet implemented (Phase 2)"));
		Result->SetStringField(TEXT("filePath"), FilePath);
	}

	FString ResultStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
	FJsonSerializer::Serialize(Result, Writer);
	return ResultStr;
}

FString UHktAnimGeneratorSubsystem::McpGetPendingRequests()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Items;

	for (const FHktAnimGenerationRequest& Req : PendingRequests)
	{
		if (Req.bCompleted) continue;

		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("layer"), Req.Intent.Layer);
		Item->SetStringField(TEXT("type"), Req.Intent.Type);
		Item->SetStringField(TEXT("name"), Req.Intent.Name);
		Item->SetStringField(TEXT("conventionPath"), Req.ConventionPath.ToString());
		Item->SetStringField(TEXT("prompt"), Req.GenerationPrompt);
		Item->SetStringField(TEXT("expectedAssetType"), Req.ExpectedAssetType);
		Items.Add(MakeShared<FJsonValueObject>(Item));
	}

	Root->SetArrayField(TEXT("pending"), Items);
	Root->SetNumberField(TEXT("count"), Items.Num());

	FString ResultStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
	FJsonSerializer::Serialize(Root, Writer);
	return ResultStr;
}

FString UHktAnimGeneratorSubsystem::McpListGeneratedAnimations(const FString& Directory)
{
	FString SearchDir = Directory.IsEmpty() ? ResolveOutputDir(TEXT("")) : Directory;

	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*SearchDir));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UAnimMontage::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UBlendSpace::StaticClass()->GetClassPathName());

	TArray<FAssetData> Assets;
	ARM.Get().GetAssets(Filter, Assets);

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Items;

	for (const FAssetData& Asset : Assets)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		Item->SetStringField(TEXT("path"), Asset.GetSoftObjectPath().ToString());
		Item->SetStringField(TEXT("class"), Asset.AssetClassPath.ToString());
		Items.Add(MakeShared<FJsonValueObject>(Item));
	}

	Root->SetArrayField(TEXT("animations"), Items);
	Root->SetNumberField(TEXT("count"), Items.Num());

	FString ResultStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
	FJsonSerializer::Serialize(Root, Writer);
	return ResultStr;
}

FString UHktAnimGeneratorSubsystem::ResolveOutputDir(const FString& OutputDir) const
{
	if (!OutputDir.IsEmpty()) return OutputDir;
	const UHktAnimGeneratorSettings* Settings = UHktAnimGeneratorSettings::Get();
	return Settings ? Settings->DefaultOutputDirectory : TEXT("/Game/Generated/Animations");
}

FString UHktAnimGeneratorSubsystem::BuildPrompt(const FHktAnimIntent& Intent) const
{
	const UHktAnimGeneratorSettings* Settings = UHktAnimGeneratorSettings::Get();

	FString Prompt = FString::Printf(TEXT("Create animation: %s %s %s"), *Intent.Layer, *Intent.Type, *Intent.Name);

	switch (Intent.SkeletonType)
	{
	case EHktSkeletonType::Humanoid:
		Prompt += TEXT(", humanoid character");
		break;
	case EHktSkeletonType::Quadruped:
		Prompt += TEXT(", quadruped creature");
		break;
	case EHktSkeletonType::Custom:
		Prompt += TEXT(", custom rig");
		break;
	}

	// Layer별 힌트
	if (Intent.Layer == TEXT("FullBody"))
	{
		Prompt += TEXT(", full body animation");
	}
	else if (Intent.Layer == TEXT("UpperBody"))
	{
		Prompt += TEXT(", upper body only, lower body idle");
	}
	else if (Intent.Layer == TEXT("Montage"))
	{
		Prompt += TEXT(", one-shot montage, clear start/end poses");
	}

	// Type별 힌트
	if (Intent.Type == TEXT("Locomotion"))
	{
		Prompt += TEXT(", loopable, smooth cycle");
	}
	else if (Intent.Type == TEXT("Combat"))
	{
		Prompt += TEXT(", impactful, clear anticipation and follow-through");
	}

	if (Intent.StyleKeywords.Num() > 0)
	{
		Prompt += TEXT(", ") + FString::Join(Intent.StyleKeywords, TEXT(", "));
	}

	if (Settings && !Settings->AnimStylePromptSuffix.IsEmpty())
	{
		Prompt += TEXT(", ") + Settings->AnimStylePromptSuffix;
	}

	return Prompt;
}

FSoftObjectPath UHktAnimGeneratorSubsystem::ResolveConventionPath(const FHktAnimIntent& Intent) const
{
	// Settings 기반 Convention Path 해결 (사용자 설정 반영)
	if (Intent.AnimTag.IsValid())
	{
		FSoftObjectPath SettingsPath = UHktAssetSubsystem::ResolveConventionPath(Intent.AnimTag);
		if (SettingsPath.IsValid())
		{
			return SettingsPath;
		}
	}

	// Fallback: Settings 규칙이 없는 경우 기본 경로
	FString OutputDir = ResolveOutputDir(TEXT(""));
	FString TagPath = FString::Printf(TEXT("Anim_%s_%s_%s"), *Intent.Layer, *Intent.Type, *Intent.Name);
	FString AssetPath = FString::Printf(TEXT("%s/%s"), *OutputDir, *TagPath);

	return FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *AssetPath, *TagPath));
}

FString UHktAnimGeneratorSubsystem::DetermineExpectedAssetType(const FHktAnimIntent& Intent)
{
	if (Intent.Type == TEXT("Locomotion"))
	{
		const UHktAnimGeneratorSettings* Settings = UHktAnimGeneratorSettings::Get();
		if (Settings && Settings->bGenerateBlendSpaceForLocomotion)
		{
			return TEXT("BlendSpace");
		}
	}

	if (Intent.Layer == TEXT("Montage") || Intent.Layer == TEXT("UpperBody"))
	{
		return TEXT("AnimMontage");
	}

	return TEXT("AnimSequence");
}
