// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktMeshGeneratorHandler.h"
#include "HktMeshGeneratorSubsystem.h"
#include "HktAnimGeneratorTypes.h"
#include "Editor.h"

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
