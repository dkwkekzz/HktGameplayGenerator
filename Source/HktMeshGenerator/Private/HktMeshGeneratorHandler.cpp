// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktMeshGeneratorHandler.h"
#include "HktMeshGeneratorSubsystem.h"
#include "HktAnimGeneratorTypes.h"
#include "HktAssetSubsystem.h"
#include "DataAssets/HktActorVisualDataAsset.h"
#include "Editor.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktMeshGeneratorHandler, Log, All);

bool UHktMeshGeneratorHandler::CanHandle(const FGameplayTag& Tag) const
{
	return Tag.IsValid() && Tag.ToString().StartsWith(TEXT("Entity."));
}

FSoftObjectPath UHktMeshGeneratorHandler::HandleTagMiss(const FGameplayTag& Tag)
{
	// 1. 이미 DataAsset이 존재하는지 확인
	FString SafeTagStr = Tag.ToString();
	SafeTagStr.ReplaceInline(TEXT("."), TEXT("_"));
	FString DAAssetName = FString::Printf(TEXT("DA_Entity_%s"), *SafeTagStr);

	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = ARM.Get();

	// DataAsset 검색 (Generated 폴더 내)
	FARFilter DAFilter;
	DAFilter.ClassPaths.Add(UHktActorVisualDataAsset::StaticClass()->GetClassPathName());
	DAFilter.bRecursiveClasses = true;

	TArray<FAssetData> DAAssets;
	AssetRegistry.GetAssets(DAFilter, DAAssets);
	for (const FAssetData& DA : DAAssets)
	{
		FString TagValue;
		if (DA.GetTagValue(FName("IdentifierTag"), TagValue))
		{
			FGameplayTag FoundTag = FGameplayTag::RequestGameplayTag(FName(*TagValue), false);
			if (FoundTag == Tag)
			{
				UE_LOG(LogHktMeshGeneratorHandler, Log, TEXT("DataAsset already exists for tag: %s"), *Tag.ToString());
				return DA.ToSoftObjectPath();
			}
		}
	}

	// 2. Intent 파싱
	FHktCharacterIntent Intent;
	if (!FHktCharacterIntent::FromTag(Tag, Intent))
	{
		UE_LOG(LogHktMeshGeneratorHandler, Warning, TEXT("Failed to parse Entity tag: %s"), *Tag.ToString());
		return FSoftObjectPath();
	}

	UHktMeshGeneratorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UHktMeshGeneratorSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogHktMeshGeneratorHandler, Error, TEXT("MeshGeneratorSubsystem not available"));
		return FSoftObjectPath();
	}

	// 3. Convention Path 해결 + 에셋 존재 확인
	FSoftObjectPath ConventionPath = Subsystem->RequestMeshGeneration(Intent);

	// 4. Blueprint가 이미 존재하면 DataAsset 생성
	FAssetData BPAssetData = AssetRegistry.GetAssetByObjectPath(ConventionPath);
	if (BPAssetData.IsValid())
	{
		// Convention Path에서 출력 디렉토리 추출
		FString PackageName = ConventionPath.GetLongPackageName();
		FString OutputDir;
		int32 LastSlash;
		if (PackageName.FindLastChar('/', LastSlash))
		{
			OutputDir = PackageName.Left(LastSlash);
		}

		FSoftObjectPath DataAssetPath = CreateActorDataAsset(Tag, ConventionPath, OutputDir);
		if (DataAssetPath.IsValid())
		{
			return DataAssetPath;
		}
	}

	// 5. Blueprint 미존재 — 펜딩 요청만 등록 (MCP Agent가 이후 생성)
	UE_LOG(LogHktMeshGeneratorHandler, Log, TEXT("Mesh generation pending: %s"), *Tag.ToString());
	return FSoftObjectPath();
}

FSoftObjectPath UHktMeshGeneratorHandler::CreateActorDataAsset(const FGameplayTag& Tag, const FSoftObjectPath& BlueprintPath, const FString& OutputDir)
{
	FString SafeTagStr = Tag.ToString();
	SafeTagStr.ReplaceInline(TEXT("."), TEXT("_"));
	FString AssetName = FString::Printf(TEXT("DA_Entity_%s"), *SafeTagStr);

	FString PackagePath = OutputDir.IsEmpty() ? TEXT("/Game/Generated/Characters") : OutputDir;
	FString FullPackagePath = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);

	UPackage* Package = CreatePackage(*FullPackagePath);
	if (!Package)
	{
		UE_LOG(LogHktMeshGeneratorHandler, Error, TEXT("Failed to create package: %s"), *FullPackagePath);
		return FSoftObjectPath();
	}

	UHktActorVisualDataAsset* DataAsset = NewObject<UHktActorVisualDataAsset>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!DataAsset)
	{
		UE_LOG(LogHktMeshGeneratorHandler, Error, TEXT("Failed to create Actor DataAsset: %s"), *AssetName);
		return FSoftObjectPath();
	}

	DataAsset->IdentifierTag = Tag;

	// Blueprint 하드 참조 설정
	UObject* LoadedBP = BlueprintPath.TryLoad();
	if (LoadedBP)
	{
		UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(LoadedBP);
		if (!BPGC)
		{
			UBlueprint* BP = Cast<UBlueprint>(LoadedBP);
			if (BP)
			{
				BPGC = Cast<UBlueprintGeneratedClass>(BP->GeneratedClass);
			}
		}
		if (BPGC)
		{
			DataAsset->ActorClass = BPGC;
		}
	}

	if (!DataAsset->ActorClass)
	{
		UE_LOG(LogHktMeshGeneratorHandler, Warning, TEXT("Could not resolve ActorClass from: %s"), *BlueprintPath.ToString());
	}

	// 패키지 저장
	DataAsset->MarkPackageDirty();

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	FString PackageFilename = FPackageName::LongPackageNameToFilename(FullPackagePath, FPackageName::GetAssetPackageExtension());
	FSavePackageResultStruct SaveResult = UPackage::SavePackage(Package, DataAsset, *PackageFilename, SaveArgs);
	if (!SaveResult.IsSuccessful())
	{
		UE_LOG(LogHktMeshGeneratorHandler, Error, TEXT("Failed to save Actor DataAsset package: %s"), *FullPackagePath);
		return FSoftObjectPath();
	}

	FSoftObjectPath ResultPath(FString::Printf(TEXT("%s.%s"), *FullPackagePath, *AssetName));
	UE_LOG(LogHktMeshGeneratorHandler, Log, TEXT("Created Actor DataAsset: %s → BP=%s"), *ResultPath.ToString(), *BlueprintPath.ToString());
	return ResultPath;
}
