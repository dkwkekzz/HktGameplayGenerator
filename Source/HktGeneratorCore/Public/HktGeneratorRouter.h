// Copyright Hkt Studios, Inc. All Rights Reserved.
// GeneratorRouter — Tag miss를 적절한 Generator로 라우팅

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "GameplayTagContainer.h"
#include "HktGeneratorRouter.generated.h"

class UHktAssetSubsystem;

/**
 * Generator 라우터 인터페이스.
 * 특정 Tag prefix를 처리할 수 있는 Generator가 구현.
 */
UINTERFACE(MinimalAPI)
class UHktGeneratorHandler : public UInterface
{
	GENERATED_BODY()
};

class HKTGENERATORCORE_API IHktGeneratorHandler
{
	GENERATED_BODY()

public:
	/** 이 Generator가 해당 Tag를 처리할 수 있는지 */
	virtual bool CanHandle(const FGameplayTag& Tag) const = 0;

	/**
	 * Tag에 대한 에셋을 생성하고 경로를 반환.
	 * 생성 불가능하면 빈 FSoftObjectPath 반환.
	 */
	virtual FSoftObjectPath HandleTagMiss(const FGameplayTag& Tag) = 0;
};

/**
 * UHktGeneratorRouter
 *
 * EditorSubsystem. 초기화 시 AssetSubsystem의 OnTagMiss에 바인딩.
 * Tag prefix에 따라 등록된 Generator에 라우팅:
 *   "VFX.*"        → VFX Generator
 *   "Entity.*"     → Mesh/Character Generator
 *   "Anim.*"       → Animation Generator
 *   "Entity.Item.*" → Item Generator
 */
UCLASS()
class HKTGENERATORCORE_API UHktGeneratorRouter : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Generator 핸들러 등록 */
	void RegisterHandler(TScriptInterface<IHktGeneratorHandler> Handler);

	/** Generator 핸들러 제거 */
	void UnregisterHandler(TScriptInterface<IHktGeneratorHandler> Handler);

private:
	/** AssetSubsystem OnTagMiss 콜백 */
	FSoftObjectPath OnTagMiss(const FGameplayTag& Tag);

	/** PIE 시작 시 AssetSubsystem에 OnTagMiss 바인딩 */
	void OnPIEStarted(bool bIsSimulating);

	/** PIE 종료 시 정리 */
	void OnPIEEnded(bool bIsSimulating);

	TArray<TScriptInterface<IHktGeneratorHandler>> Handlers;

	FDelegateHandle PIEStartedHandle;
	FDelegateHandle PIEEndedHandle;
};
