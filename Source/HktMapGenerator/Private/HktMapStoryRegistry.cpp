// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktMapStoryRegistry.h"
#include "HktStoryGeneratorSubsystem.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktStoryRegistry, Log, All);

void UHktMapStoryRegistry::RegisterGlobalStories(const TArray<FHktMapStoryRef>& Stories)
{
	for (const auto& StoryRef : Stories)
	{
		if (StoryRef.StoryTag.IsValid())
		{
			GlobalStoryTags.Add(StoryRef.StoryTag);
			if (StoryRef.bAutoLoad)
			{
				LoadStory(StoryRef.StoryTag);
			}
		}
	}

	UE_LOG(LogHktStoryRegistry, Log, TEXT("Registered %d global stories"), GlobalStoryTags.Num());
}

void UHktMapStoryRegistry::OnRegionActivated(const FString& RegionName, const TArray<FHktMapStoryRef>& Stories)
{
	TArray<FGameplayTag>& Tags = RegionStoryTags.FindOrAdd(RegionName);
	Tags.Empty();

	for (const auto& StoryRef : Stories)
	{
		if (StoryRef.StoryTag.IsValid())
		{
			Tags.Add(StoryRef.StoryTag);
			if (StoryRef.bAutoLoad)
			{
				LoadStory(StoryRef.StoryTag);
			}
		}
	}

	UE_LOG(LogHktStoryRegistry, Log, TEXT("Region '%s' activated — %d stories loaded"), *RegionName, Tags.Num());
}

void UHktMapStoryRegistry::OnRegionDeactivated(const FString& RegionName)
{
	if (TArray<FGameplayTag>* Tags = RegionStoryTags.Find(RegionName))
	{
		for (const auto& Tag : *Tags)
		{
			// Don't unload if it's also a global story
			if (!GlobalStoryTags.Contains(Tag))
			{
				UnloadStory(Tag);
			}
		}
		UE_LOG(LogHktStoryRegistry, Log, TEXT("Region '%s' deactivated — stories unloaded"), *RegionName);
		RegionStoryTags.Remove(RegionName);
	}
}

void UHktMapStoryRegistry::Clear()
{
	// Unload all region stories
	for (auto& Pair : RegionStoryTags)
	{
		for (const auto& Tag : Pair.Value)
		{
			if (!GlobalStoryTags.Contains(Tag))
			{
				UnloadStory(Tag);
			}
		}
	}
	RegionStoryTags.Empty();

	// Unload global stories
	for (const auto& Tag : GlobalStoryTags)
	{
		UnloadStory(Tag);
	}
	GlobalStoryTags.Empty();
	LoadedStorySet.Empty();

	UE_LOG(LogHktStoryRegistry, Log, TEXT("Story registry cleared"));
}

TArray<FGameplayTag> UHktMapStoryRegistry::GetLoadedStoryTags() const
{
	TArray<FGameplayTag> All;
	All.Append(GlobalStoryTags);
	for (const auto& Pair : RegionStoryTags)
	{
		All.Append(Pair.Value);
	}
	return All;
}

void UHktMapStoryRegistry::LoadStory(const FGameplayTag& StoryTag)
{
	UE_LOG(LogHktStoryRegistry, Log, TEXT("Loading story: %s"), *StoryTag.ToString());

	// Avoid duplicate loads
	if (LoadedStorySet.Contains(StoryTag))
	{
		UE_LOG(LogHktStoryRegistry, Verbose, TEXT("Story '%s' already loaded, skipping"), *StoryTag.ToString());
		return;
	}

	UHktStoryGeneratorSubsystem* StorySub = nullptr;
	if (GEditor)
	{
		StorySub = GEditor->GetEditorSubsystem<UHktStoryGeneratorSubsystem>();
	}

	if (!StorySub)
	{
		UE_LOG(LogHktStoryRegistry, Warning, TEXT("Story '%s': HktStoryGeneratorSubsystem not available"),
			*StoryTag.ToString());
		return;
	}

	// Build a minimal story JSON that references the story tag.
	// The StoryGenerator will compile and register it to the VM.
	FString StoryJson = FString::Printf(
		TEXT("{\"storyTag\":\"%s\",\"tags\":{},\"steps\":[]}"),
		*StoryTag.ToString());

	FString Result = StorySub->McpBuildStory(StoryJson);

	// Check result for success
	if (Result.Contains(TEXT("\"success\":true")) || Result.Contains(TEXT("\"success\": true")))
	{
		LoadedStorySet.Add(StoryTag);
		UE_LOG(LogHktStoryRegistry, Log, TEXT("Story '%s' loaded successfully"), *StoryTag.ToString());
	}
	else
	{
		UE_LOG(LogHktStoryRegistry, Warning, TEXT("Story '%s' load failed: %s"),
			*StoryTag.ToString(), *Result);
	}
}

void UHktMapStoryRegistry::UnloadStory(const FGameplayTag& StoryTag)
{
	UE_LOG(LogHktStoryRegistry, Log, TEXT("Unloading story: %s"), *StoryTag.ToString());

	LoadedStorySet.Remove(StoryTag);

	// Story VM unregistration will be handled when HktStoryGenerator
	// exposes an explicit unload API. For now we track state locally
	// so that reloads on region re-entry work correctly.
}
