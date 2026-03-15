// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktStoryGeneratorSubsystem.h"
#include "HktStoryJsonCompiler.h"
#include "HktAssetSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktStoryGeneratorSubsystem, Log, All);

void UHktStoryGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogHktStoryGeneratorSubsystem, Log, TEXT("StoryGeneratorSubsystem initialized"));
}

void UHktStoryGeneratorSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

FString UHktStoryGeneratorSubsystem::McpBuildStory(const FString& JsonStory)
{
	FHktStoryCompileResult CompileResult = FHktStoryJsonCompiler::CompileAndRegister(JsonStory);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), CompileResult.bSuccess);
	Result->SetStringField(TEXT("storyTag"), CompileResult.StoryTag);

	if (CompileResult.bSuccess)
	{
		GeneratedStoryTags.AddUnique(CompileResult.StoryTag);

		// 참조 태그 목록
		TArray<TSharedPtr<FJsonValue>> RefTags;
		for (const FGameplayTag& Tag : CompileResult.ReferencedTags)
		{
			RefTags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
		}
		Result->SetArrayField(TEXT("referencedTags"), RefTags);

		// 에셋 없는 태그 확인
		TArray<FGameplayTag> MissingTags = FindMissingAssetTags(CompileResult.ReferencedTags);
		TArray<TSharedPtr<FJsonValue>> MissingArr;
		for (const FGameplayTag& Tag : MissingTags)
		{
			MissingArr.Add(MakeShared<FJsonValueString>(Tag.ToString()));
		}
		Result->SetArrayField(TEXT("missingAssetTags"), MissingArr);
		Result->SetNumberField(TEXT("missingCount"), MissingTags.Num());
	}

	// 에러
	if (CompileResult.Errors.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Errors;
		for (const FString& Err : CompileResult.Errors)
		{
			Errors.Add(MakeShared<FJsonValueString>(Err));
		}
		Result->SetArrayField(TEXT("errors"), Errors);
	}

	// 경고
	if (CompileResult.Warnings.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Warnings;
		for (const FString& Warn : CompileResult.Warnings)
		{
			Warnings.Add(MakeShared<FJsonValueString>(Warn));
		}
		Result->SetArrayField(TEXT("warnings"), Warnings);
	}

	FString ResultStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
	FJsonSerializer::Serialize(Result, Writer);
	return ResultStr;
}

FString UHktStoryGeneratorSubsystem::McpValidateStory(const FString& JsonStory)
{
	FHktStoryCompileResult ValidateResult = FHktStoryJsonCompiler::Validate(JsonStory);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("valid"), ValidateResult.bSuccess);
	Result->SetStringField(TEXT("storyTag"), ValidateResult.StoryTag);

	if (ValidateResult.Errors.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Errors;
		for (const FString& Err : ValidateResult.Errors)
		{
			Errors.Add(MakeShared<FJsonValueString>(Err));
		}
		Result->SetArrayField(TEXT("errors"), Errors);
	}

	// 참조 태그
	TArray<TSharedPtr<FJsonValue>> RefTags;
	for (const FGameplayTag& Tag : ValidateResult.ReferencedTags)
	{
		RefTags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
	}
	Result->SetArrayField(TEXT("referencedTags"), RefTags);

	FString ResultStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
	FJsonSerializer::Serialize(Result, Writer);
	return ResultStr;
}

FString UHktStoryGeneratorSubsystem::McpAnalyzeDependencies(const FString& JsonStory)
{
	FHktStoryCompileResult ValidateResult = FHktStoryJsonCompiler::Validate(JsonStory);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!ValidateResult.bSuccess)
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), TEXT("Story validation failed"));

		if (ValidateResult.Errors.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Errors;
			for (const FString& Err : ValidateResult.Errors)
			{
				Errors.Add(MakeShared<FJsonValueString>(Err));
			}
			Result->SetArrayField(TEXT("errors"), Errors);
		}

		FString ResultStr;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
		FJsonSerializer::Serialize(Result, Writer);
		return ResultStr;
	}

	TArray<FGameplayTag> MissingTags = FindMissingAssetTags(ValidateResult.ReferencedTags);

	// 카테고리별 분류
	TArray<TSharedPtr<FJsonValue>> VFXDeps, EntityDeps, AnimDeps, ItemDeps, OtherDeps;

	for (const FGameplayTag& Tag : MissingTags)
	{
		FString TagStr = Tag.ToString();
		TSharedRef<FJsonObject> Dep = MakeShared<FJsonObject>();
		Dep->SetStringField(TEXT("tag"), TagStr);

		if (TagStr.StartsWith(TEXT("VFX.")))
		{
			Dep->SetStringField(TEXT("generator"), TEXT("VFXGenerator"));
			VFXDeps.Add(MakeShared<FJsonValueObject>(Dep));
		}
		else if (TagStr.StartsWith(TEXT("Entity.Item.")))
		{
			Dep->SetStringField(TEXT("generator"), TEXT("ItemGenerator"));
			ItemDeps.Add(MakeShared<FJsonValueObject>(Dep));
		}
		else if (TagStr.StartsWith(TEXT("Entity.")))
		{
			Dep->SetStringField(TEXT("generator"), TEXT("MeshGenerator"));
			EntityDeps.Add(MakeShared<FJsonValueObject>(Dep));
		}
		else if (TagStr.StartsWith(TEXT("Anim.")))
		{
			Dep->SetStringField(TEXT("generator"), TEXT("AnimGenerator"));
			AnimDeps.Add(MakeShared<FJsonValueObject>(Dep));
		}
		else
		{
			Dep->SetStringField(TEXT("generator"), TEXT("manual"));
			OtherDeps.Add(MakeShared<FJsonValueObject>(Dep));
		}
	}

	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("storyTag"), ValidateResult.StoryTag);
	Result->SetNumberField(TEXT("totalDependencies"), ValidateResult.ReferencedTags.Num());
	Result->SetNumberField(TEXT("missingDependencies"), MissingTags.Num());

	TSharedRef<FJsonObject> ByCategory = MakeShared<FJsonObject>();
	if (VFXDeps.Num() > 0) ByCategory->SetArrayField(TEXT("vfx"), VFXDeps);
	if (EntityDeps.Num() > 0) ByCategory->SetArrayField(TEXT("entity"), EntityDeps);
	if (AnimDeps.Num() > 0) ByCategory->SetArrayField(TEXT("anim"), AnimDeps);
	if (ItemDeps.Num() > 0) ByCategory->SetArrayField(TEXT("item"), ItemDeps);
	if (OtherDeps.Num() > 0) ByCategory->SetArrayField(TEXT("other"), OtherDeps);
	Result->SetObjectField(TEXT("missingByCategory"), ByCategory);

	FString ResultStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
	FJsonSerializer::Serialize(Result, Writer);
	return ResultStr;
}

FString UHktStoryGeneratorSubsystem::McpGetStorySchema()
{
	return FHktStoryJsonCompiler::GetStorySchema();
}

FString UHktStoryGeneratorSubsystem::McpGetStoryExamples()
{
	return FHktStoryJsonCompiler::GetStoryExamples();
}

FString UHktStoryGeneratorSubsystem::McpListGeneratedStories()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Items;

	for (const FString& Tag : GeneratedStoryTags)
	{
		Items.Add(MakeShared<FJsonValueString>(Tag));
	}

	Root->SetArrayField(TEXT("stories"), Items);
	Root->SetNumberField(TEXT("count"), Items.Num());

	FString ResultStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultStr);
	FJsonSerializer::Serialize(Root, Writer);
	return ResultStr;
}

TArray<FGameplayTag> UHktStoryGeneratorSubsystem::FindMissingAssetTags(const TArray<FGameplayTag>& ReferencedTags)
{
	TArray<FGameplayTag> Missing;

	// Generator가 처리할 수 있는 태그 prefix만 체크
	static const TArray<FString> GeneratorPrefixes = { TEXT("VFX."), TEXT("Entity."), TEXT("Anim.") };

	for (const FGameplayTag& Tag : ReferencedTags)
	{
		FString TagStr = Tag.ToString();

		bool bIsGeneratable = false;
		for (const FString& Prefix : GeneratorPrefixes)
		{
			if (TagStr.StartsWith(Prefix))
			{
				bIsGeneratable = true;
				break;
			}
		}

		if (!bIsGeneratable) continue;

		// Convention path로 AssetRegistry 조회
		FSoftObjectPath ConventionPath = UHktAssetSubsystem::ResolveConventionPath(Tag);
		if (ConventionPath.IsValid())
		{
			FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			FAssetData AssetData = ARM.Get().GetAssetByObjectPath(ConventionPath);
			if (AssetData.IsValid())
			{
				continue; // 에셋이 이미 존재 — missing 아님
			}
		}

		Missing.Add(Tag);
	}

	return Missing;
}
