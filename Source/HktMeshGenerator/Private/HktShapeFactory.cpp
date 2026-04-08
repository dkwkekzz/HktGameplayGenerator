// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktShapeFactory.h"
#include "HktShapeGenerator.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Engine/StaticMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/SecureHash.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktShapeFactory, Log, All);

FString UHktShapeFactory::GetDefaultOutputDir()
{
	return TEXT("/Game/Generated/VFX/Shapes");
}

FString UHktShapeFactory::ComputeParamsHash(const FString& JsonParams)
{
	return FMD5::HashAnsiString(*JsonParams);
}

FString UHktShapeFactory::CreateShapeAsset(const FString& JsonParams, const FString& OutputDir)
{
	// JSON 파싱
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonParams);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogHktShapeFactory, Error, TEXT("CreateShapeAsset: Invalid JSON"));
		return FString();
	}

	// 이름, 해시
	FString Name = JsonObj->HasField(TEXT("name"))
		? JsonObj->GetStringField(TEXT("name"))
		: TEXT("Shape");
	FString Hash = ComputeParamsHash(JsonParams);

	// 캐시 확인
	for (const auto& Entry : Catalog)
	{
		if (Entry.ParamsHash == Hash)
		{
			UE_LOG(LogHktShapeFactory, Log, TEXT("Cache hit: %s → %s"), *Name, *Entry.AssetPath);
			return Entry.AssetPath;
		}
	}

	// MeshDescription 빌드 (Register는 Commit 내부에서 수행)
	FMeshDescription MeshDesc;
	if (!FHktShapeGenerator::BuildFromJson(JsonObj, MeshDesc))
	{
		UE_LOG(LogHktShapeFactory, Error, TEXT("CreateShapeAsset: BuildFromJson failed for '%s'"), *Name);
		return FString();
	}

	// 에셋 경로
	FString Dir = OutputDir.IsEmpty() ? GetDefaultOutputDir() : OutputDir;
	FString AssetName = FString::Printf(TEXT("SM_%s"), *Name);
	FString PackagePath = Dir / AssetName;

	// StaticMesh 에셋 생성
	UStaticMesh* Mesh = SaveMeshAsset(MeshDesc, PackagePath, AssetName);
	if (!Mesh)
	{
		UE_LOG(LogHktShapeFactory, Error, TEXT("CreateShapeAsset: SaveMeshAsset failed for '%s'"), *AssetName);
		return FString();
	}

	FString AssetPath = Mesh->GetPathName();

	// 카탈로그 등록
	FHktShapeCatalogEntry Entry;
	Entry.Name = Name;
	Entry.ShapeType = FHktShapeGenerator::ParseShapeType(
		JsonObj->HasField(TEXT("shapeType")) ? JsonObj->GetStringField(TEXT("shapeType")) : TEXT("Disc"));
	Entry.AssetPath = AssetPath;
	Entry.ParamsHash = Hash;
	Entry.ParamsJson = JsonParams;
	Catalog.Add(Entry);

	UE_LOG(LogHktShapeFactory, Log, TEXT("Created shape asset: %s → %s"), *Name, *AssetPath);
	return AssetPath;
}

UStaticMesh* UHktShapeFactory::SaveMeshAsset(FMeshDescription& MeshDesc, const FString& PackagePath, const FString& AssetName)
{
	// 패키지 생성
	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE_LOG(LogHktShapeFactory, Error, TEXT("Failed to create package: %s"), *PackagePath);
		return nullptr;
	}
	Package->FullyLoad();

	// StaticMesh 생성
	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, FName(*AssetName), RF_Public | RF_Standalone);

	// --- StaticMesh에 MeshDescription 커밋 ---
	// NOTE: UE5 엔진 버전에 따라 BuildFromMeshDescriptions API가 다를 수 있음.
	// 컴파일 에러 시 아래 블록을 대체:
	//   StaticMesh->CreateMeshDescription(0, MoveTemp(MeshDesc));
	//   StaticMesh->CommitMeshDescription(0);
	//   StaticMesh->Build(false);
	TArray<const FMeshDescription*> MeshDescs;
	MeshDescs.Add(&MeshDesc);

	UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
	BuildParams.bMarkPackageDirty = true;
	BuildParams.bBuildSimpleCollision = false;       // 파티클용 — collision 불필요
	BuildParams.bCommitMeshDescription = true;
	StaticMesh->BuildFromMeshDescriptions(MeshDescs, BuildParams);

	// 머티리얼 슬롯 (기본 1개)
	if (StaticMesh->GetStaticMaterials().Num() == 0)
	{
		StaticMesh->GetStaticMaterials().Add(FStaticMaterial());
	}

	// 저장
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(StaticMesh);

	FString PackageFileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, StaticMesh, *PackageFileName, SaveArgs);

	return StaticMesh;
}

FString UHktShapeFactory::GetCatalogJson() const
{
	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteArrayStart(TEXT("shapes"));

	for (const auto& Entry : Catalog)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("name"), Entry.Name);
		Writer->WriteValue(TEXT("assetPath"), Entry.AssetPath);
		Writer->WriteValue(TEXT("paramsHash"), Entry.ParamsHash);
		Writer->WriteObjectEnd();
	}

	// AssetRegistry에서 기존 에셋도 검색
	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*GetDefaultOutputDir()));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());

	TArray<FAssetData> Assets;
	ARM.Get().GetAssets(Filter, Assets);

	for (const auto& Asset : Assets)
	{
		// 카탈로그에 이미 있으면 스킵
		bool bAlreadyInCatalog = false;
		for (const auto& Entry : Catalog)
		{
			if (Entry.AssetPath.Contains(Asset.AssetName.ToString()))
			{
				bAlreadyInCatalog = true;
				break;
			}
		}
		if (bAlreadyInCatalog) continue;

		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("name"), Asset.AssetName.ToString());
		Writer->WriteValue(TEXT("assetPath"), Asset.GetSoftObjectPath().ToString());
		Writer->WriteValue(TEXT("paramsHash"), TEXT(""));
		Writer->WriteObjectEnd();
	}

	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();
	Writer->Close();
	return Output;
}

FString UHktShapeFactory::FindShapeByName(const FString& ShapeName) const
{
	for (const auto& Entry : Catalog)
	{
		if (Entry.Name.Equals(ShapeName, ESearchCase::IgnoreCase) ||
			Entry.AssetPath.Contains(ShapeName))
		{
			return Entry.AssetPath;
		}
	}
	return FString();
}
