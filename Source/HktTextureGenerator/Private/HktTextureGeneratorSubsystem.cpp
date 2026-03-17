// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktTextureGeneratorSubsystem.h"
#include "HktTextureGeneratorSettings.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktTextureGenerator, Log, All);

// ============================================================================
// 라이프사이클
// ============================================================================

void UHktTextureGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RebuildCache();
	UE_LOG(LogHktTextureGenerator, Log, TEXT("TextureGenerator initialized. Cached %d textures."), TextureCache.Num());
}

void UHktTextureGeneratorSubsystem::Deinitialize()
{
	TextureCache.Empty();
	Super::Deinitialize();
}

// ============================================================================
// 생성 API
// ============================================================================

UTexture2D* UHktTextureGeneratorSubsystem::GenerateTexture(const FHktTextureIntent& Intent, const FString& OutputDir)
{
	const FString AssetKey = Intent.GetAssetKey();

	// 1. 캐시 확인
	if (UTexture2D* Cached = FindCachedTexture(AssetKey))
	{
		UE_LOG(LogHktTextureGenerator, Log, TEXT("Cache hit: %s"), *AssetKey);
		return Cached;
	}

	// 2. Convention Path에 이미 에셋이 있는지 확인
	const FString AssetPath = ResolveAssetPath(Intent, OutputDir);
	if (UTexture2D* Existing = LoadObject<UTexture2D>(nullptr, *AssetPath))
	{
		TextureCache.Add(AssetKey, FSoftObjectPath(Existing));
		UE_LOG(LogHktTextureGenerator, Log, TEXT("Found existing asset: %s"), *AssetPath);
		return Existing;
	}

	// 3. 이미지 파일이 있는지 확인 (MCP/외부 도구가 미리 생성한 경우)
	//    Convention: {ProjectSavedDir}/TextureGenerator/{AssetKey}.png
	const FString ImageDir = FPaths::ProjectSavedDir() / TEXT("TextureGenerator");
	const FString ImagePath = ImageDir / (AssetKey + TEXT(".png"));

	if (FPaths::FileExists(ImagePath))
	{
		UE_LOG(LogHktTextureGenerator, Log, TEXT("Found image file, importing: %s"), *ImagePath);
		return ImportTextureInternal(ImagePath, AssetPath, Intent);
	}

	// 4. 이미지가 없음 — 생성 불가. 요청 정보를 로그에 남김.
	UE_LOG(LogHktTextureGenerator, Warning,
		TEXT("Texture miss: %s — 이미지 파일을 '%s'에 생성하거나 McpImportTexture()를 호출하세요."),
		*AssetKey, *ImagePath);

	return nullptr;
}

TArray<FHktTextureResult> UHktTextureGeneratorSubsystem::GenerateBatch(
	const TArray<FHktTextureRequest>& Requests, const FString& OutputDir)
{
	TArray<FHktTextureResult> Results;
	Results.Reserve(Requests.Num());

	for (const FHktTextureRequest& Req : Requests)
	{
		FHktTextureResult Result;
		Result.Name = Req.Name;

		UTexture2D* Texture = GenerateTexture(Req.Intent, OutputDir);
		if (Texture)
		{
			Result.Texture = Texture;
			Result.AssetPath = Texture->GetPathName();
		}
		else
		{
			Result.Error = FString::Printf(TEXT("Texture generation failed for '%s'"), *Req.Name);
		}

		Results.Add(MoveTemp(Result));
	}

	return Results;
}

// ============================================================================
// 이미지 파일 임포트
// ============================================================================

UTexture2D* UHktTextureGeneratorSubsystem::ImportTextureFromFile(
	const FString& ImageFilePath, const FHktTextureIntent& Intent, const FString& OutputDir)
{
	if (!FPaths::FileExists(ImageFilePath))
	{
		UE_LOG(LogHktTextureGenerator, Error, TEXT("Image file not found: %s"), *ImageFilePath);
		return nullptr;
	}

	const FString AssetPath = ResolveAssetPath(Intent, OutputDir);
	return ImportTextureInternal(ImageFilePath, AssetPath, Intent);
}

UTexture2D* UHktTextureGeneratorSubsystem::ImportTextureInternal(
	const FString& ImageFilePath, const FString& AssetPath, const FHktTextureIntent& Intent)
{
	// 에셋 경로 분해
	FString PackagePath, AssetName;
	AssetPath.Split(TEXT("."), &PackagePath, &AssetName);
	if (AssetName.IsEmpty())
	{
		// "/Game/Generated/Textures/Sprite/T_Sprite_ABCD1234" 형태
		int32 LastSlash;
		if (AssetPath.FindLastChar('/', LastSlash))
		{
			PackagePath = AssetPath.Left(LastSlash);
			AssetName = AssetPath.Mid(LastSlash + 1);
		}
	}

	// TextureFactory로 임포트
	UTextureFactory* Factory = NewObject<UTextureFactory>();
	Factory->AddToRoot(); // GC 방지

	// Usage에 따른 임포트 설정
	Factory->NoAlpha = !Intent.bAlphaChannel;
	Factory->bUseHashAsGuid = true;

	// 이미지 파일 로드
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *ImageFilePath))
	{
		UE_LOG(LogHktTextureGenerator, Error, TEXT("Failed to load image file: %s"), *ImageFilePath);
		Factory->RemoveFromRoot();
		return nullptr;
	}

	// 패키지 생성
	UPackage* Package = CreatePackage(*AssetPath);
	Package->FullyLoad();

	// 임포트 실행
	const uint8* BufferPtr = FileData.GetData();
	const uint8* BufferEnd = FileData.GetData() + FileData.Num();
	UObject* ImportedObj = Factory->FactoryCreateBinary(
		UTexture2D::StaticClass(),
		Package,
		FName(*AssetName),
		RF_Public | RF_Standalone,
		nullptr,
		*FPaths::GetExtension(ImageFilePath),
		BufferPtr,
		BufferEnd,
		GWarn);

	Factory->RemoveFromRoot();

	UTexture2D* Texture = Cast<UTexture2D>(ImportedObj);
	if (!Texture)
	{
		UE_LOG(LogHktTextureGenerator, Error, TEXT("Failed to import texture from: %s"), *ImageFilePath);
		return nullptr;
	}

	// Usage별 텍스처 설정 적용
	ConfigureTextureSettings(Texture, Intent);

	// 소스 파일 정보 기록
	Texture->AssetImportData->Update(ImageFilePath);

	// 저장
	Texture->MarkPackageDirty();
	FString PackageFileName = FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, Texture, *PackageFileName, SaveArgs);

	// 캐시 등록
	const FString AssetKey = Intent.GetAssetKey();
	TextureCache.Add(AssetKey, FSoftObjectPath(Texture));

	UE_LOG(LogHktTextureGenerator, Log, TEXT("Imported texture: %s → %s"), *ImageFilePath, *AssetPath);

	// 이벤트 발행
	FHktTextureResult Result;
	Result.Name = AssetKey;
	Result.Texture = Texture;
	Result.AssetPath = AssetPath;
	OnTextureGenerated.Broadcast(Result);

	return Texture;
}

// ============================================================================
// MCP JSON API
// ============================================================================

namespace
{
	FString MakeJsonSuccess(const FString& AssetPath, const FString& AssetKey)
	{
		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("success"), true);
		Writer->WriteValue(TEXT("assetPath"), AssetPath);
		Writer->WriteValue(TEXT("assetKey"), AssetKey);
		Writer->WriteObjectEnd();
		Writer->Close();
		return Output;
	}

	FString MakeJsonError(const FString& Error)
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
}

FString UHktTextureGeneratorSubsystem::McpGenerateTexture(const FString& JsonIntent, const FString& OutputDir)
{
	FHktTextureIntent Intent;
	if (!FHktTextureIntent::FromJson(JsonIntent, Intent))
	{
		return MakeJsonError(TEXT("Invalid JSON intent"));
	}

	UTexture2D* Texture = GenerateTexture(Intent, OutputDir);
	if (!Texture)
	{
		// 캐시 미스 — pending 정보 반환
		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("success"), false);
		Writer->WriteValue(TEXT("pending"), true);
		Writer->WriteValue(TEXT("assetKey"), Intent.GetAssetKey());

		// MCP Agent가 이미지를 생성할 수 있도록 완성된 프롬프트 제공
		const auto* Settings = UHktTextureGeneratorSettings::Get();
		FString FullPrompt = Intent.Prompt;

		// Usage별 프롬프트 접미사 추가
		switch (Intent.Usage)
		{
		case EHktTextureUsage::ParticleSprite:
			FullPrompt += Settings->ParticleSpritePromptSuffix;
			break;
		case EHktTextureUsage::Flipbook4x4:
			FullPrompt += FString::Printf(TEXT(", 4x4 grid%s"), *Settings->FlipbookPromptSuffix);
			break;
		case EHktTextureUsage::Flipbook8x8:
			FullPrompt += FString::Printf(TEXT(", 8x8 grid%s"), *Settings->FlipbookPromptSuffix);
			break;
		case EHktTextureUsage::ItemIcon:
			FullPrompt += Settings->ItemIconPromptSuffix;
			break;
		case EHktTextureUsage::Noise:
		case EHktTextureUsage::Gradient:
			FullPrompt += Settings->NoisePromptSuffix;
			break;
		case EHktTextureUsage::MaterialBase:
		case EHktTextureUsage::MaterialNormal:
		case EHktTextureUsage::MaterialMask:
			FullPrompt += Settings->MaterialPromptSuffix;
			break;
		}

		FString FullNegative = Settings->DefaultNegativePrompt;
		if (!Intent.NegativePrompt.IsEmpty())
		{
			FullNegative += TEXT(", ") + Intent.NegativePrompt;
		}

		Writer->WriteValue(TEXT("prompt"), FullPrompt);
		Writer->WriteValue(TEXT("negativePrompt"), FullNegative);
		Writer->WriteValue(TEXT("resolution"), Intent.GetEffectiveResolution());
		Writer->WriteValue(TEXT("alphaChannel"), Intent.bAlphaChannel);

		// 이미지를 저장해야 할 경로
		const FString ImageDir = FPaths::ProjectSavedDir() / TEXT("TextureGenerator");
		const FString ImagePath = ImageDir / (Intent.GetAssetKey() + TEXT(".png"));
		Writer->WriteValue(TEXT("imagePath"), ImagePath);

		// SD WebUI 서버 설정 (MCP Agent가 자동 실행에 사용)
		if (!Settings->SDWebUIBatchFilePath.IsEmpty())
		{
			Writer->WriteValue(TEXT("sdWebUIBatchFilePath"), Settings->SDWebUIBatchFilePath);
		}
		Writer->WriteValue(TEXT("sdWebUIServerURL"), Settings->SDWebUIServerURL);

		Writer->WriteObjectEnd();
		Writer->Close();
		return Output;
	}

	return MakeJsonSuccess(Texture->GetPathName(), Intent.GetAssetKey());
}

FString UHktTextureGeneratorSubsystem::McpImportTexture(
	const FString& ImageFilePath, const FString& JsonIntent, const FString& OutputDir)
{
	FHktTextureIntent Intent;
	if (!FHktTextureIntent::FromJson(JsonIntent, Intent))
	{
		return MakeJsonError(TEXT("Invalid JSON intent"));
	}

	UTexture2D* Texture = ImportTextureFromFile(ImageFilePath, Intent, OutputDir);
	if (!Texture)
	{
		return MakeJsonError(FString::Printf(TEXT("Failed to import texture from: %s"), *ImageFilePath));
	}

	return MakeJsonSuccess(Texture->GetPathName(), Intent.GetAssetKey());
}

FString UHktTextureGeneratorSubsystem::McpGetPendingRequests(const FString& JsonRequests)
{
	// JSON 배열 파싱
	TSharedPtr<FJsonValue> RootVal;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonRequests);
	if (!FJsonSerializer::Deserialize(Reader, RootVal) || !RootVal.IsValid())
	{
		return MakeJsonError(TEXT("Invalid JSON array"));
	}

	const TArray<TSharedPtr<FJsonValue>>* RequestArray = nullptr;
	if (!RootVal->TryGetArray(RequestArray))
	{
		return MakeJsonError(TEXT("Expected JSON array"));
	}

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteArrayStart(TEXT("pending"));

	for (const auto& ReqVal : *RequestArray)
	{
		const TSharedPtr<FJsonObject>* ReqObj = nullptr;
		if (!ReqVal->TryGetObject(ReqObj)) continue;

		FString Name;
		(*ReqObj)->TryGetStringField(TEXT("name"), Name);

		FString IntentJson;
		const TSharedPtr<FJsonObject>* IntentObj = nullptr;
		if ((*ReqObj)->TryGetObjectField(TEXT("intent"), IntentObj))
		{
			TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> IntentWriter =
				TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&IntentJson);
			FJsonSerializer::Serialize((*IntentObj).ToSharedRef(), IntentWriter);
		}

		FHktTextureIntent Intent;
		if (!FHktTextureIntent::FromJson(IntentJson, Intent)) continue;

		const FString AssetKey = Intent.GetAssetKey();

		// 캐시에 있으면 pending이 아님
		if (FindCachedTexture(AssetKey) != nullptr) continue;

		// pending 항목 추가
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("name"), Name);
		Writer->WriteValue(TEXT("assetKey"), AssetKey);
		Writer->WriteValue(TEXT("prompt"), Intent.Prompt);
		Writer->WriteValue(TEXT("resolution"), Intent.GetEffectiveResolution());

		const FString ImageDir = FPaths::ProjectSavedDir() / TEXT("TextureGenerator");
		Writer->WriteValue(TEXT("imagePath"), ImageDir / (AssetKey + TEXT(".png")));
		Writer->WriteObjectEnd();
	}

	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();
	Writer->Close();
	return Output;
}

// ============================================================================
// 캐시 / 조회
// ============================================================================

UTexture2D* UHktTextureGeneratorSubsystem::FindCachedTexture(const FString& AssetKey) const
{
	const FSoftObjectPath* Path = TextureCache.Find(AssetKey);
	if (!Path) return nullptr;

	return Cast<UTexture2D>(Path->ResolveObject());
}

FString UHktTextureGeneratorSubsystem::McpListGeneratedTextures(const FString& Directory)
{
	const FString ResolvedDir = Directory.IsEmpty()
		? UHktTextureGeneratorSettings::Get()->DefaultOutputDirectory
		: Directory;

	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AR = ARM.Get();

	TArray<FAssetData> Assets;
	AR.GetAssetsByPath(FName(*ResolvedDir), Assets, true);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteArrayStart(TEXT("textures"));

	for (const FAssetData& Asset : Assets)
	{
		if (Asset.AssetClassPath.GetAssetName() == TEXT("Texture2D"))
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

void UHktTextureGeneratorSubsystem::RebuildCache()
{
	TextureCache.Empty();

	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AR = ARM.Get();

	const FString OutputDir = UHktTextureGeneratorSettings::Get()->DefaultOutputDirectory;
	TArray<FAssetData> Assets;
	AR.GetAssetsByPath(FName(*OutputDir), Assets, true);

	for (const FAssetData& Asset : Assets)
	{
		if (Asset.AssetClassPath.GetAssetName() == TEXT("Texture2D"))
		{
			// 에셋 이름이 AssetKey 형식(T_{Usage}_{Hash})이면 캐시에 등록
			const FString AssetName = Asset.AssetName.ToString();
			if (AssetName.StartsWith(TEXT("T_")))
			{
				TextureCache.Add(AssetName, Asset.GetSoftObjectPath());
			}
		}
	}
}

// ============================================================================
// 경로 해결
// ============================================================================

FString UHktTextureGeneratorSubsystem::ResolveOutputDir(const FString& OutputDir) const
{
	if (!OutputDir.IsEmpty()) return OutputDir;
	return UHktTextureGeneratorSettings::Get()->DefaultOutputDirectory;
}

FString UHktTextureGeneratorSubsystem::ResolveAssetPath(const FHktTextureIntent& Intent, const FString& OutputDir) const
{
	const FString Dir = ResolveOutputDir(OutputDir);
	const FString SubDir = GetUsageSubDir(Intent.Usage);
	const FString AssetKey = Intent.GetAssetKey();
	return FString::Printf(TEXT("%s/%s/%s"), *Dir, *SubDir, *AssetKey);
}

FString UHktTextureGeneratorSubsystem::GetUsageSubDir(EHktTextureUsage Usage)
{
	switch (Usage)
	{
	case EHktTextureUsage::ParticleSprite:
	case EHktTextureUsage::Flipbook4x4:
	case EHktTextureUsage::Flipbook8x8: return TEXT("VFX");
	case EHktTextureUsage::Noise:        return TEXT("Noise");
	case EHktTextureUsage::Gradient:     return TEXT("Gradient");
	case EHktTextureUsage::ItemIcon:     return TEXT("Icons");
	case EHktTextureUsage::MaterialBase:
	case EHktTextureUsage::MaterialNormal:
	case EHktTextureUsage::MaterialMask: return TEXT("Materials");
	default:                             return TEXT("Misc");
	}
}

// ============================================================================
// 텍스처 설정
// ============================================================================

void UHktTextureGeneratorSubsystem::ConfigureTextureSettings(UTexture2D* Texture, const FHktTextureIntent& Intent)
{
	if (!Texture) return;

	switch (Intent.Usage)
	{
	case EHktTextureUsage::ParticleSprite:
	case EHktTextureUsage::Flipbook4x4:
	case EHktTextureUsage::Flipbook8x8:
		Texture->CompressionSettings = TC_EditorIcon;
		Texture->SRGB = true;
		Texture->LODGroup = TEXTUREGROUP_Effects;
		Texture->MipGenSettings = TMGS_FromTextureGroup;
		if (!Intent.bAlphaChannel)
		{
			Texture->CompressionSettings = TC_Default;
		}
		break;

	case EHktTextureUsage::ItemIcon:
		Texture->CompressionSettings = TC_EditorIcon;
		Texture->SRGB = true;
		Texture->LODGroup = TEXTUREGROUP_UI;
		Texture->NeverStream = true;
		Texture->MipGenSettings = TMGS_NoMipmaps;
		break;

	case EHktTextureUsage::MaterialBase:
		Texture->CompressionSettings = TC_Default;
		Texture->SRGB = true;
		Texture->LODGroup = TEXTUREGROUP_World;
		break;

	case EHktTextureUsage::MaterialNormal:
		Texture->CompressionSettings = TC_Normalmap;
		Texture->SRGB = false;
		Texture->LODGroup = TEXTUREGROUP_WorldNormalMap;
		break;

	case EHktTextureUsage::MaterialMask:
		Texture->CompressionSettings = TC_Masks;
		Texture->SRGB = false;
		Texture->LODGroup = TEXTUREGROUP_World;
		break;

	case EHktTextureUsage::Noise:
	case EHktTextureUsage::Gradient:
		Texture->CompressionSettings = TC_Grayscale;
		Texture->SRGB = false;
		Texture->LODGroup = TEXTUREGROUP_Effects;
		if (Intent.bTileable)
		{
			Texture->AddressX = TA_Wrap;
			Texture->AddressY = TA_Wrap;
		}
		break;
	}

	Texture->UpdateResource();
}
