// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktMapStoryRegistry.h"

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

	// TODO: Call HktStoryGeneratorSubsystem to compile and register this story
	// The actual integration depends on HktStoryGenerator being loaded as a module
	// and exposing a public API for runtime story registration.
	//
	// Expected call:
	// if (UHktStoryGeneratorSubsystem* StorySub = ...)
	//     StorySub->McpBuildStory(StoryJsonForTag);
}

void UHktMapStoryRegistry::UnloadStory(const FGameplayTag& StoryTag)
{
	UE_LOG(LogHktStoryRegistry, Log, TEXT("Unloading story: %s"), *StoryTag.ToString());

	// TODO: Call HktStoryGenerator to unregister this story from the VM
}
