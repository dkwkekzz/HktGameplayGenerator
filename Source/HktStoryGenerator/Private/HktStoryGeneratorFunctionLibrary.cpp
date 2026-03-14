// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktStoryGeneratorFunctionLibrary.h"
#include "HktStoryGeneratorSubsystem.h"
#include "Editor.h"

FString UHktStoryGeneratorFunctionLibrary::McpBuildStory(const FString& JsonStory)
{
	if (UHktStoryGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktStoryGeneratorSubsystem>())
	{
		return Sub->McpBuildStory(JsonStory);
	}
	return TEXT("{\"error\": \"StoryGeneratorSubsystem not available\"}");
}

FString UHktStoryGeneratorFunctionLibrary::McpValidateStory(const FString& JsonStory)
{
	if (UHktStoryGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktStoryGeneratorSubsystem>())
	{
		return Sub->McpValidateStory(JsonStory);
	}
	return TEXT("{\"error\": \"StoryGeneratorSubsystem not available\"}");
}

FString UHktStoryGeneratorFunctionLibrary::McpAnalyzeDependencies(const FString& JsonStory)
{
	if (UHktStoryGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktStoryGeneratorSubsystem>())
	{
		return Sub->McpAnalyzeDependencies(JsonStory);
	}
	return TEXT("{\"error\": \"StoryGeneratorSubsystem not available\"}");
}

FString UHktStoryGeneratorFunctionLibrary::McpGetStorySchema()
{
	if (UHktStoryGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktStoryGeneratorSubsystem>())
	{
		return Sub->McpGetStorySchema();
	}
	return TEXT("{\"error\": \"StoryGeneratorSubsystem not available\"}");
}

FString UHktStoryGeneratorFunctionLibrary::McpGetStoryExamples()
{
	if (UHktStoryGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktStoryGeneratorSubsystem>())
	{
		return Sub->McpGetStoryExamples();
	}
	return TEXT("{\"error\": \"StoryGeneratorSubsystem not available\"}");
}

FString UHktStoryGeneratorFunctionLibrary::McpListGeneratedStories()
{
	if (UHktStoryGeneratorSubsystem* Sub = GEditor->GetEditorSubsystem<UHktStoryGeneratorSubsystem>())
	{
		return Sub->McpListGeneratedStories();
	}
	return TEXT("{\"error\": \"StoryGeneratorSubsystem not available\"}");
}
