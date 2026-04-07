// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktMeshGeneratorHandler.h"
#include "HktMeshGeneratorSubsystem.h"
#include "HktAnimGeneratorTypes.h"
#include "DataAssets/HktActorVisualDataAsset.h"
#include "Editor.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktMeshGeneratorHandler, Log, All);

bool UHktMeshGeneratorHandler::CanHandle(const FGameplayTag& Tag) const
{
	return Tag.IsValid() && Tag.ToString().StartsWith(TEXT("Entity."));
}

FSoftObjectPath UHktMeshGeneratorHandler::HandleTagMiss(const FGameplayTag& Tag)
{
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

	// 펜딩 요청 등록 + Convention Path 반환
	FSoftObjectPath ConventionPath = Subsystem->RequestMeshGeneration(Intent);
	UE_LOG(LogHktMeshGeneratorHandler, Log, TEXT("Mesh generation requested: %s → %s"), *Tag.ToString(), *ConventionPath.ToString());
	return ConventionPath;
}

FSoftObjectPath UHktMeshGeneratorHandler::CreateActorVisualDataAsset(const FGameplayTag& Tag, const FSoftObjectPath& ActorClassPath, const FString& OutputDir)
{
	// 에셋 이름: DA_Actor_{TagPath}
	FString TagStr = Tag.ToString();
	FString SafeTagStr = TagStr;
	SafeTagStr.ReplaceInline(TEXT("."), TEXT("_"));
	FString AssetName = FString::Printf(TEXT("DA_Actor_%s"), *SafeTagStr);

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

	// IdentifierTag 설정
	DataAsset->IdentifierTag = Tag;

	// ActorClass 참조 설정 (BP 또는 NativeClass)
	if (ActorClassPath.IsValid())
	{
		DataAsset->ActorClass = Cast<UClass>(ActorClassPath.TryLoad());
	}

	// 패키지 저장
	DataAsset->MarkPackageDirty();

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	FString PackageFilename = FPackageName::LongPackageNameToFilename(FullPackagePath, FPackageName::GetAssetPackageExtension());
	bool bSaveResult = UPackage::SavePackage(Package, DataAsset, *PackageFilename, SaveArgs);
	if (!bSaveResult)
	{
		UE_LOG(LogHktMeshGeneratorHandler, Error, TEXT("Failed to save Actor DataAsset package: %s"), *FullPackagePath);
		return FSoftObjectPath();
	}

	FSoftObjectPath ResultPath(FString::Printf(TEXT("%s.%s"), *FullPackagePath, *AssetName));
	UE_LOG(LogHktMeshGeneratorHandler, Log, TEXT("Created Actor DataAsset: %s → ActorClass=%s"), *ResultPath.ToString(), *ActorClassPath.ToString());
	return ResultPath;
}
