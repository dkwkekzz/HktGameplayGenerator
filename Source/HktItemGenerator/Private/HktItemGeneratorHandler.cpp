// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktItemGeneratorHandler.h"
#include "HktItemGeneratorSubsystem.h"
#include "HktAnimGeneratorTypes.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktItemGeneratorHandler, Log, All);

bool UHktItemGeneratorHandler::CanHandle(const FGameplayTag& Tag) const
{
	return Tag.IsValid() && Tag.ToString().StartsWith(TEXT("Entity.Item."));
}

FSoftObjectPath UHktItemGeneratorHandler::HandleTagMiss(const FGameplayTag& Tag)
{
	FHktItemIntent Intent;
	if (!FHktItemIntent::FromTag(Tag, Intent))
	{
		UE_LOG(LogHktItemGeneratorHandler, Warning, TEXT("Failed to parse Entity.Item tag: %s"), *Tag.ToString());
		return FSoftObjectPath();
	}

	UHktItemGeneratorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UHktItemGeneratorSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogHktItemGeneratorHandler, Error, TEXT("ItemGeneratorSubsystem not available"));
		return FSoftObjectPath();
	}

	FSoftObjectPath ConventionPath = Subsystem->RequestItemGeneration(Intent);
	UE_LOG(LogHktItemGeneratorHandler, Log, TEXT("Item generation requested: %s → %s"), *Tag.ToString(), *ConventionPath.ToString());
	return ConventionPath;
}
