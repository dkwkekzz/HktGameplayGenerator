// Copyright Hkt Studios, Inc. All Rights Reserved.
// Item Generator Handler — "Entity.Item.*" Tag miss 처리

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HktGeneratorRouter.h"
#include "HktItemGeneratorHandler.generated.h"

/**
 * UHktItemGeneratorHandler
 *
 * "Entity.Item.*" 태그에 대한 IHktGeneratorHandler 구현.
 * FHktItemIntent로 파싱 → MCP Agent에게 메시+아이콘 생성 위임.
 * 에셋 존재 시 UHktActorVisualDataAsset 생성 후 DataAsset 경로 반환.
 */
UCLASS()
class HKTITEMGENERATOR_API UHktItemGeneratorHandler : public UObject, public IHktGeneratorHandler
{
	GENERATED_BODY()

public:
	virtual bool CanHandle(const FGameplayTag& Tag) const override;
	virtual FSoftObjectPath HandleTagMiss(const FGameplayTag& Tag) override;

private:
	/** 태그에 대응하는 UHktActorVisualDataAsset 생성 (아이템 Blueprint 하드 참조) */
	FSoftObjectPath CreateItemDataAsset(const FGameplayTag& Tag, const FSoftObjectPath& BlueprintPath, const FString& OutputDir);
};
