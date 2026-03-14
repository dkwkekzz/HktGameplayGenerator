// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktGeneratorRouter.h"
#include "HktAssetSubsystem.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktGeneratorRouter, Log, All);

void UHktGeneratorRouter::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// PIE 시작/종료 시 AssetSubsystem OnTagMiss에 바인딩/해제
	PIEStartedHandle = FEditorDelegates::PostPIEStarted.AddUObject(this, &UHktGeneratorRouter::OnPIEStarted);
	PIEEndedHandle = FEditorDelegates::PrePIEEnded.AddUObject(this, &UHktGeneratorRouter::OnPIEEnded);

	UE_LOG(LogHktGeneratorRouter, Log, TEXT("GeneratorRouter initialized with %d handlers"), Handlers.Num());
}

void UHktGeneratorRouter::Deinitialize()
{
	FEditorDelegates::PostPIEStarted.Remove(PIEStartedHandle);
	FEditorDelegates::PrePIEEnded.Remove(PIEEndedHandle);

	Handlers.Empty();
	Super::Deinitialize();
}

void UHktGeneratorRouter::OnPIEStarted(bool bIsSimulating)
{
	// PIE World의 AssetSubsystem을 찾아 OnTagMiss 바인딩
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE && Context.World())
		{
			if (UGameInstance* GI = Context.World()->GetGameInstance())
			{
				if (UHktAssetSubsystem* AssetSub = GI->GetSubsystem<UHktAssetSubsystem>())
				{
					AssetSub->OnTagMiss.BindUObject(this, &UHktGeneratorRouter::OnTagMiss);
					UE_LOG(LogHktGeneratorRouter, Log, TEXT("OnTagMiss bound to AssetSubsystem (PIE)"));
				}
			}
		}
	}
}

void UHktGeneratorRouter::OnPIEEnded(bool bIsSimulating)
{
	// AssetSubsystem이 소멸되므로 명시적 해제 불필요, 로그만 출력
	UE_LOG(LogHktGeneratorRouter, Log, TEXT("PIE ended — OnTagMiss binding released"));
}

void UHktGeneratorRouter::RegisterHandler(TScriptInterface<IHktGeneratorHandler> Handler)
{
	if (Handler.GetInterface())
	{
		Handlers.AddUnique(Handler);
		UE_LOG(LogHktGeneratorRouter, Log, TEXT("Handler registered. Total: %d"), Handlers.Num());
	}
}

void UHktGeneratorRouter::UnregisterHandler(TScriptInterface<IHktGeneratorHandler> Handler)
{
	Handlers.Remove(Handler);
}

FSoftObjectPath UHktGeneratorRouter::OnTagMiss(const FGameplayTag& Tag)
{
	UE_LOG(LogHktGeneratorRouter, Log, TEXT("Tag miss: %s — routing to %d handlers"), *Tag.ToString(), Handlers.Num());

	for (const TScriptInterface<IHktGeneratorHandler>& Handler : Handlers)
	{
		if (Handler.GetInterface() && Handler->CanHandle(Tag))
		{
			FSoftObjectPath Result = Handler->HandleTagMiss(Tag);
			if (Result.IsValid())
			{
				UE_LOG(LogHktGeneratorRouter, Log, TEXT("Tag miss handled: %s → %s"), *Tag.ToString(), *Result.ToString());
				return Result;
			}
		}
	}

	UE_LOG(LogHktGeneratorRouter, Warning, TEXT("Tag miss unhandled: %s"), *Tag.ToString());
	return FSoftObjectPath();
}
