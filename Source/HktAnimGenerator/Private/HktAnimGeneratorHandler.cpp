// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktAnimGeneratorHandler.h"
#include "HktAnimGeneratorSubsystem.h"
#include "HktAnimGeneratorTypes.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktAnimGeneratorHandler, Log, All);

bool UHktAnimGeneratorHandler::CanHandle(const FGameplayTag& Tag) const
{
	return Tag.IsValid() && Tag.ToString().StartsWith(TEXT("Anim."));
}

FSoftObjectPath UHktAnimGeneratorHandler::HandleTagMiss(const FGameplayTag& Tag)
{
	FHktAnimIntent Intent;
	if (!FHktAnimIntent::FromTag(Tag, Intent))
	{
		UE_LOG(LogHktAnimGeneratorHandler, Warning, TEXT("Failed to parse Anim tag: %s"), *Tag.ToString());
		return FSoftObjectPath();
	}

	UHktAnimGeneratorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UHktAnimGeneratorSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogHktAnimGeneratorHandler, Error, TEXT("AnimGeneratorSubsystem not available"));
		return FSoftObjectPath();
	}

	FSoftObjectPath ConventionPath = Subsystem->RequestAnimGeneration(Intent);
	UE_LOG(LogHktAnimGeneratorHandler, Log, TEXT("Anim generation requested: %s → %s"), *Tag.ToString(), *ConventionPath.ToString());
	return ConventionPath;
}
