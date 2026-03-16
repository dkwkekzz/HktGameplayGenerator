// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktItemGeneratorSubsystem.h"
#include "HktItemGeneratorSettings.h"
#include "HktItemGeneratorHandler.h"
#include "HktGeneratorRouter.h"
#include "HktAssetSubsystem.h"
#include "HktTextureGeneratorSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktItemGeneratorSubsystem, Log, All);

void UHktItemGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Handler 생성 및 Router 등록
	if (UHktGeneratorRouter* Router = GEditor->GetEditorSubsystem<UHktGeneratorRouter>())
	{
		ItemHandler = NewObject<UHktItemGeneratorHandler>(this);
		Router->RegisterHandler(TScriptInterface<IHktGeneratorHandler>(ItemHandler));
		UE_LOG(LogHktItemGeneratorSubsystem, Log, TEXT("ItemGeneratorHandler registered with Router"));
	}

	UE_LOG(LogHktItemGeneratorSubsystem, Log, TEXT("ItemGeneratorSubsystem initialized"));
}

void UHktItemGeneratorSubsystem::Deinitialize()
{
	PendingRequests.Empty();
	Super::Deinitialize();
}

FSoftObjectPath UHktItemGeneratorSubsystem::RequestItemGeneration(const FHktItemIntent& Intent)
{
	FSoftObjectPath ConventionPath = ResolveConventionPath(Intent);

	// 이미 에셋이 존재하면 바로 반환
	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FAssetData AssetData = ARM.Get().GetAssetByObjectPath(ConventionPath);
	if (AssetData.IsValid())
	{
		UE_LOG(LogHktItemGeneratorSubsystem, Log, TEXT("Item already exists: %s"), *ConventionPath.ToString());
		return ConventionPath;
	}

	// 소켓 정보
	FName Socket;
	FString Method;
	ResolveSocketInfo(Intent.Category, Socket, Method);

	// 펜딩 요청 등록
	FHktItemGenerationRequest Request;
	Request.Intent = Intent;
	Request.ConventionPath = ConventionPath;
	Request.MeshPrompt = BuildMeshPrompt(Intent);
	Request.IconPrompt = BuildIconPrompt(Intent);
	Request.AttachSocket = Socket;
	Request.AttachMethod = Method;
	PendingRequests.Add(Request);

	// 아이콘 텍스처 자동 요청 (TextureGenerator에 위임)
	GenerateItemIcon(Intent);

	UE_LOG(LogHktItemGeneratorSubsystem, Log, TEXT("Item generation pending: %s"), *ConventionPath.ToString());
	return ConventionPath;
}

UTexture2D* UHktItemGeneratorSubsystem::GenerateItemIcon(const FHktItemIntent& Intent)
{
	UHktTextureGeneratorSubsystem* TexSub = GEditor->GetEditorSubsystem<UHktTextureGeneratorSubsystem>();
	if (!TexSub) return nullptr;

	FHktTextureIntent TexIntent;
	TexIntent.Usage = EHktTextureUsage::ItemIcon;
	TexIntent.Prompt = BuildIconPrompt(Intent);
	TexIntent.Resolution = 256;
	TexIntent.bAlphaChannel = true;

	const UHktItemGeneratorSettings* Settings = UHktItemGeneratorSettings::Get();
	FString OutputDir = Settings ? Settings->IconOutputDirectory : TEXT("/Game/Generated/Textures/Icons");

	return TexSub->GenerateTexture(TexIntent, OutputDir);
}

UObject* UHktItemGeneratorSubsystem::ImportItemMesh(const FString& FilePath, const FString& DestinationPath)
{
	// TODO: Phase 2 — FBX/OBJ → UE5 StaticMesh 임포트
	UE_LOG(LogHktItemGeneratorSubsystem, Warning, TEXT("ImportItemMesh: Not yet implemented (Phase 2). File: %s"), *FilePath);
	return nullptr;
}

FString UHktItemGeneratorSubsystem::McpRequestItem(const FString& JsonIntent)
{
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonIntent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		return TEXT("{\"error\": \"Invalid JSON\"}");
	}

	FHktItemIntent Intent;
	Intent.Category = JsonObj->GetStringField(TEXT("category"));
	Intent.SubType = JsonObj->GetStringField(TEXT("subType"));
	JsonObj->TryGetStringField(TEXT("element"), Intent.Element);

	double Rarity;
	if (JsonObj->TryGetNumberField(TEXT("rarity"), Rarity))
	{
		Intent.Rarity = static_cast<float>(Rarity);
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
	FString TagStr = Intent.Element.IsEmpty()
		? FString::Printf(TEXT("Entity.Item.%s.%s"), *Intent.Category, *Intent.SubType)
		: FString::Printf(TEXT("Entity.Item.%s.%s.%s"), *Intent.Category, *Intent.SubType, *Intent.Element);
	Intent.ItemTag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);

	FSoftObjectPath Path = RequestItemGeneration(Intent);

	// 소켓 정보
	FName Socket;
	FString Method;
	ResolveSocketInfo(Intent.Category, Socket, Method);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("category"), Intent.Category);
	Result->SetStringField(TEXT("subType"), Intent.SubType);
	Result->SetStringField(TEXT("element"), Intent.Element);
	Result->SetNumberField(TEXT("rarity"), Intent.Rarity);
	Result->SetStringField(TEXT("conventionPath"), Path.ToString());
	Result->SetStringField(TEXT("meshPrompt"), BuildMeshPrompt(Intent));
	Result->SetStringField(TEXT("iconPrompt"), BuildIconPrompt(Intent));
	Result->SetStringField(TEXT("attachSocket"), Socket.ToString());
	Result->SetStringField(TEXT("attachMethod"), Method);
	Result->SetBoolField(TEXT("pending"), true);

	FString ResultStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
	FJsonSerializer::Serialize(Result, Writer);
	return ResultStr;
}

FString UHktItemGeneratorSubsystem::McpImportItemMesh(const FString& FilePath, const FString& JsonIntent)
{
	UObject* Imported = ImportItemMesh(FilePath);

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

FString UHktItemGeneratorSubsystem::McpGetPendingRequests()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Items;

	for (const FHktItemGenerationRequest& Req : PendingRequests)
	{
		if (Req.bCompleted) continue;

		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("category"), Req.Intent.Category);
		Item->SetStringField(TEXT("subType"), Req.Intent.SubType);
		Item->SetStringField(TEXT("element"), Req.Intent.Element);
		Item->SetNumberField(TEXT("rarity"), Req.Intent.Rarity);
		Item->SetStringField(TEXT("conventionPath"), Req.ConventionPath.ToString());
		Item->SetStringField(TEXT("meshPrompt"), Req.MeshPrompt);
		Item->SetStringField(TEXT("iconPrompt"), Req.IconPrompt);
		Item->SetStringField(TEXT("attachSocket"), Req.AttachSocket.ToString());
		Item->SetStringField(TEXT("attachMethod"), Req.AttachMethod);
		Items.Add(MakeShared<FJsonValueObject>(Item));
	}

	Root->SetArrayField(TEXT("pending"), Items);
	Root->SetNumberField(TEXT("count"), Items.Num());

	FString ResultStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
	FJsonSerializer::Serialize(Root, Writer);
	return ResultStr;
}

FString UHktItemGeneratorSubsystem::McpListGeneratedItems(const FString& Directory)
{
	FString SearchDir = Directory.IsEmpty() ? ResolveOutputDir(TEXT("")) : Directory;

	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*SearchDir));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());

	TArray<FAssetData> Assets;
	ARM.Get().GetAssets(Filter, Assets);

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ItemsList;

	for (const FAssetData& Asset : Assets)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		Item->SetStringField(TEXT("path"), Asset.GetSoftObjectPath().ToString());
		ItemsList.Add(MakeShared<FJsonValueObject>(Item));
	}

	Root->SetArrayField(TEXT("items"), ItemsList);
	Root->SetNumberField(TEXT("count"), ItemsList.Num());

	FString ResultStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
	FJsonSerializer::Serialize(Root, Writer);
	return ResultStr;
}

FString UHktItemGeneratorSubsystem::McpGetSocketMappings()
{
	const UHktItemGeneratorSettings* Settings = UHktItemGeneratorSettings::Get();
	if (!Settings) return TEXT("{\"error\": \"Settings not available\"}");

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Mappings;

	for (const FHktItemSocketMapping& Mapping : Settings->SocketMappings)
	{
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("category"), Mapping.Category);
		Entry->SetStringField(TEXT("socket"), Mapping.SocketName.ToString());
		Entry->SetStringField(TEXT("method"), Mapping.AttachMethod);
		Mappings.Add(MakeShared<FJsonValueObject>(Entry));
	}

	Root->SetArrayField(TEXT("socketMappings"), Mappings);

	FString ResultStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
	FJsonSerializer::Serialize(Root, Writer);
	return ResultStr;
}

FString UHktItemGeneratorSubsystem::ResolveOutputDir(const FString& OutputDir) const
{
	if (!OutputDir.IsEmpty()) return OutputDir;
	const UHktItemGeneratorSettings* Settings = UHktItemGeneratorSettings::Get();
	return Settings ? Settings->DefaultOutputDirectory : TEXT("/Game/Generated/Items");
}

FString UHktItemGeneratorSubsystem::BuildMeshPrompt(const FHktItemIntent& Intent) const
{
	const UHktItemGeneratorSettings* Settings = UHktItemGeneratorSettings::Get();

	FString Prompt = FString::Printf(TEXT("Create a 3D %s: %s"), *Intent.Category.ToLower(), *Intent.SubType);

	if (!Intent.Element.IsEmpty())
	{
		Prompt += FString::Printf(TEXT(", %s elemental theme"), *Intent.Element.ToLower());
	}

	// Rarity에 따른 스타일 힌트
	if (Intent.Rarity > 0.8f)
	{
		Prompt += TEXT(", legendary quality, glowing effects, ornate details");
	}
	else if (Intent.Rarity > 0.5f)
	{
		Prompt += TEXT(", rare quality, refined details, subtle glow");
	}
	else if (Intent.Rarity > 0.2f)
	{
		Prompt += TEXT(", uncommon quality, clean design");
	}
	else
	{
		Prompt += TEXT(", common quality, simple design");
	}

	if (Intent.StyleKeywords.Num() > 0)
	{
		Prompt += TEXT(", ") + FString::Join(Intent.StyleKeywords, TEXT(", "));
	}

	if (Settings && !Settings->ItemMeshPromptSuffix.IsEmpty())
	{
		Prompt += TEXT(", ") + Settings->ItemMeshPromptSuffix;
	}

	return Prompt;
}

FString UHktItemGeneratorSubsystem::BuildIconPrompt(const FHktItemIntent& Intent) const
{
	const UHktItemGeneratorSettings* Settings = UHktItemGeneratorSettings::Get();

	FString Prompt = FString::Printf(TEXT("%s %s icon"), *Intent.SubType, *Intent.Category.ToLower());

	if (!Intent.Element.IsEmpty())
	{
		Prompt += FString::Printf(TEXT(", %s element"), *Intent.Element.ToLower());
	}

	if (Intent.Rarity > 0.8f)
	{
		Prompt += TEXT(", legendary golden border");
	}
	else if (Intent.Rarity > 0.5f)
	{
		Prompt += TEXT(", rare purple border");
	}

	if (Settings && !Settings->ItemIconPromptSuffix.IsEmpty())
	{
		Prompt += TEXT(", ") + Settings->ItemIconPromptSuffix;
	}

	return Prompt;
}

FSoftObjectPath UHktItemGeneratorSubsystem::ResolveConventionPath(const FHktItemIntent& Intent) const
{
	// Settings 기반 Convention Path 해결 (사용자 설정 반영)
	if (Intent.ItemTag.IsValid())
	{
		FSoftObjectPath SettingsPath = UHktAssetSubsystem::ResolveConventionPath(Intent.ItemTag);
		if (SettingsPath.IsValid())
		{
			return SettingsPath;
		}
	}

	// Fallback: Settings 규칙이 없는 경우 기본 경로
	FString OutputDir = ResolveOutputDir(TEXT(""));
	FString AssetName = FString::Printf(TEXT("SM_%s"), *Intent.SubType);
	FString AssetPath = FString::Printf(TEXT("%s/%s/%s"), *OutputDir, *Intent.Category, *AssetName);

	return FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName));
}

void UHktItemGeneratorSubsystem::ResolveSocketInfo(const FString& Category, FName& OutSocket, FString& OutMethod) const
{
	const UHktItemGeneratorSettings* Settings = UHktItemGeneratorSettings::Get();
	if (Settings)
	{
		for (const FHktItemSocketMapping& Mapping : Settings->SocketMappings)
		{
			if (Mapping.Category.Equals(Category, ESearchCase::IgnoreCase))
			{
				OutSocket = Mapping.SocketName;
				OutMethod = Mapping.AttachMethod;
				return;
			}
		}
	}

	// 기본값
	OutSocket = FName("hand_r");
	OutMethod = TEXT("socket");
}
