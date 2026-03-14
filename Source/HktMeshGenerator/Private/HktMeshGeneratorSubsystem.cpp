// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktMeshGeneratorSubsystem.h"
#include "HktMeshGeneratorSettings.h"
#include "HktMeshGeneratorHandler.h"
#include "HktGeneratorRouter.h"
#include "HktAssetSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktMeshGeneratorSubsystem, Log, All);

void UHktMeshGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Handler 생성 및 Router 등록
	if (UHktGeneratorRouter* Router = GEditor->GetEditorSubsystem<UHktGeneratorRouter>())
	{
		MeshHandler = NewObject<UHktMeshGeneratorHandler>(this);
		Router->RegisterHandler(TScriptInterface<IHktGeneratorHandler>(MeshHandler));
		UE_LOG(LogHktMeshGeneratorSubsystem, Log, TEXT("MeshGeneratorHandler registered with Router"));
	}

	UE_LOG(LogHktMeshGeneratorSubsystem, Log, TEXT("MeshGeneratorSubsystem initialized"));
}

void UHktMeshGeneratorSubsystem::Deinitialize()
{
	PendingRequests.Empty();
	Super::Deinitialize();
}

FSoftObjectPath UHktMeshGeneratorSubsystem::RequestMeshGeneration(const FHktCharacterIntent& Intent)
{
	FSoftObjectPath ConventionPath = ResolveConventionPath(Intent);

	// 이미 에셋이 존재하면 바로 반환
	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FAssetData AssetData = ARM.Get().GetAssetByObjectPath(ConventionPath);
	if (AssetData.IsValid())
	{
		UE_LOG(LogHktMeshGeneratorSubsystem, Log, TEXT("Mesh already exists: %s"), *ConventionPath.ToString());
		return ConventionPath;
	}

	// 펜딩 요청 등록
	FHktMeshGenerationRequest Request;
	Request.Intent = Intent;
	Request.ConventionPath = ConventionPath;
	Request.GenerationPrompt = BuildPrompt(Intent);
	PendingRequests.Add(Request);

	UE_LOG(LogHktMeshGeneratorSubsystem, Log, TEXT("Mesh generation pending: %s"), *ConventionPath.ToString());
	return ConventionPath;
}

UObject* UHktMeshGeneratorSubsystem::ImportMeshFromFile(const FString& FilePath, const FString& DestinationPath)
{
	// TODO: Phase 2 — FBX/OBJ → UE5 에셋 임포트
	// UAutomatedAssetImportData + AssetTools 사용
	UE_LOG(LogHktMeshGeneratorSubsystem, Warning, TEXT("ImportMeshFromFile: Not yet implemented (Phase 2). File: %s"), *FilePath);
	return nullptr;
}

FString UHktMeshGeneratorSubsystem::McpRequestCharacterMesh(const FString& JsonIntent)
{
	// JSON → FHktCharacterIntent 파싱
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonIntent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		return TEXT("{\"error\": \"Invalid JSON\"}");
	}

	FHktCharacterIntent Intent;
	Intent.Name = JsonObj->GetStringField(TEXT("name"));

	FString SkeletonStr = JsonObj->GetStringField(TEXT("skeletonType"));
	if (SkeletonStr == TEXT("Quadruped")) Intent.SkeletonType = EHktSkeletonType::Quadruped;
	else if (SkeletonStr == TEXT("Custom")) Intent.SkeletonType = EHktSkeletonType::Custom;
	else Intent.SkeletonType = EHktSkeletonType::Humanoid;

	const TArray<TSharedPtr<FJsonValue>>* Keywords;
	if (JsonObj->TryGetArrayField(TEXT("styleKeywords"), Keywords))
	{
		for (const auto& Val : *Keywords)
		{
			Intent.StyleKeywords.Add(Val->AsString());
		}
	}

	// 태그 생성
	FString TagStr = FString::Printf(TEXT("Entity.Character.%s"), *Intent.Name);
	Intent.EntityTag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);

	FSoftObjectPath Path = RequestMeshGeneration(Intent);

	// 결과 JSON
	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), Intent.Name);
	Result->SetStringField(TEXT("conventionPath"), Path.ToString());
	Result->SetStringField(TEXT("prompt"), BuildPrompt(Intent));
	Result->SetBoolField(TEXT("pending"), true);

	FString ResultStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
	FJsonSerializer::Serialize(Result, Writer);
	return ResultStr;
}

FString UHktMeshGeneratorSubsystem::McpImportMesh(const FString& FilePath, const FString& JsonIntent)
{
	UObject* Imported = ImportMeshFromFile(FilePath);

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

FString UHktMeshGeneratorSubsystem::McpGetPendingRequests()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Items;

	for (const FHktMeshGenerationRequest& Req : PendingRequests)
	{
		if (Req.bCompleted) continue;

		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("name"), Req.Intent.Name);
		Item->SetStringField(TEXT("conventionPath"), Req.ConventionPath.ToString());
		Item->SetStringField(TEXT("prompt"), Req.GenerationPrompt);

		FString SkeletonStr;
		switch (Req.Intent.SkeletonType)
		{
		case EHktSkeletonType::Quadruped: SkeletonStr = TEXT("Quadruped"); break;
		case EHktSkeletonType::Custom: SkeletonStr = TEXT("Custom"); break;
		default: SkeletonStr = TEXT("Humanoid"); break;
		}
		Item->SetStringField(TEXT("skeletonType"), SkeletonStr);

		Items.Add(MakeShared<FJsonValueObject>(Item));
	}

	Root->SetArrayField(TEXT("pending"), Items);
	Root->SetNumberField(TEXT("count"), Items.Num());

	FString ResultStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
	FJsonSerializer::Serialize(Root, Writer);
	return ResultStr;
}

FString UHktMeshGeneratorSubsystem::McpListGeneratedMeshes(const FString& Directory)
{
	FString SearchDir = Directory.IsEmpty() ? ResolveOutputDir(TEXT("")) : Directory;

	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*SearchDir));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());

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

	Root->SetArrayField(TEXT("meshes"), Items);
	Root->SetNumberField(TEXT("count"), Items.Num());

	FString ResultStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
	FJsonSerializer::Serialize(Root, Writer);
	return ResultStr;
}

FString UHktMeshGeneratorSubsystem::McpGetSkeletonPool()
{
	const UHktMeshGeneratorSettings* Settings = UHktMeshGeneratorSettings::Get();

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Skeletons;

	auto AddSkeleton = [&](const FString& Type, const FSoftObjectPath& Path)
	{
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("type"), Type);
		Entry->SetStringField(TEXT("path"), Path.ToString());
		Entry->SetBoolField(TEXT("available"), !Path.ToString().IsEmpty());
		Skeletons.Add(MakeShared<FJsonValueObject>(Entry));
	};

	if (Settings)
	{
		AddSkeleton(TEXT("Humanoid"), Settings->HumanoidBaseSkeleton);
		AddSkeleton(TEXT("Quadruped"), Settings->QuadrupedBaseSkeleton);
	}

	Root->SetArrayField(TEXT("skeletons"), Skeletons);

	FString ResultStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
	FJsonSerializer::Serialize(Root, Writer);
	return ResultStr;
}

FString UHktMeshGeneratorSubsystem::ResolveOutputDir(const FString& OutputDir) const
{
	if (!OutputDir.IsEmpty()) return OutputDir;
	const UHktMeshGeneratorSettings* Settings = UHktMeshGeneratorSettings::Get();
	return Settings ? Settings->DefaultOutputDirectory : TEXT("/Game/Generated/Characters");
}

FString UHktMeshGeneratorSubsystem::BuildPrompt(const FHktCharacterIntent& Intent) const
{
	const UHktMeshGeneratorSettings* Settings = UHktMeshGeneratorSettings::Get();

	FString Prompt = FString::Printf(TEXT("Create a 3D character model: %s"), *Intent.Name);

	switch (Intent.SkeletonType)
	{
	case EHktSkeletonType::Humanoid:
		Prompt += TEXT(", humanoid biped");
		break;
	case EHktSkeletonType::Quadruped:
		Prompt += TEXT(", quadruped animal");
		break;
	case EHktSkeletonType::Custom:
		Prompt += TEXT(", custom creature");
		break;
	}

	if (Intent.StyleKeywords.Num() > 0)
	{
		Prompt += TEXT(", ") + FString::Join(Intent.StyleKeywords, TEXT(", "));
	}

	if (Settings && !Settings->CharacterStylePromptSuffix.IsEmpty())
	{
		Prompt += TEXT(", ") + Settings->CharacterStylePromptSuffix;
	}

	return Prompt;
}

FSoftObjectPath UHktMeshGeneratorSubsystem::ResolveConventionPath(const FHktCharacterIntent& Intent) const
{
	FString TagStr = Intent.EntityTag.IsValid() ? Intent.EntityTag.ToString() : FString::Printf(TEXT("Entity.Character.%s"), *Intent.Name);
	FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);

	// Settings 기반 Convention Path 해결 (사용자 설정 반영)
	FSoftObjectPath SettingsPath = UHktAssetSubsystem::ResolveConventionPath(Tag);
	if (SettingsPath.IsValid())
	{
		return SettingsPath;
	}

	// Fallback: Settings 규칙이 없는 경우 기본 경로
	FString OutputDir = ResolveOutputDir(TEXT(""));
	FString AssetPath;

	if (TagStr.StartsWith(TEXT("Entity.Character.")))
	{
		AssetPath = FString::Printf(TEXT("%s/%s/BP_%s"), *OutputDir, *Intent.Name, *Intent.Name);
	}
	else
	{
		AssetPath = FString::Printf(TEXT("/Game/Generated/Entities/%s/BP_%s"), *Intent.Name, *Intent.Name);
	}

	FString AssetName;
	int32 LastSlash;
	if (AssetPath.FindLastChar('/', LastSlash))
	{
		AssetName = AssetPath.Mid(LastSlash + 1);
	}
	else
	{
		AssetName = AssetPath;
	}

	return FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName));
}
