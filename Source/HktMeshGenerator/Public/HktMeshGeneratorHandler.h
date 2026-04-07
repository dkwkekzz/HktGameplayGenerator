// Copyright Hkt Studios, Inc. All Rights Reserved.
// Mesh Generator Handler — "Entity.*" Tag miss 처리

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HktGeneratorRouter.h"
#include "HktMeshGeneratorHandler.generated.h"

/**
 * UHktMeshGeneratorHandler
 *
 * "Entity.*" 태그에 대한 IHktGeneratorHandler 구현.
 * FHktCharacterIntent로 파싱 → MCP Agent에게 메시 생성 위임.
 * 메시 임포트 후 UHktActorVisualDataAsset 생성하여 TagDataAsset 시스템과 연결.
 */
UCLASS()
class HKTMESHGENERATOR_API UHktMeshGeneratorHandler : public UObject, public IHktGeneratorHandler
{
	GENERATED_BODY()

public:
	virtual bool CanHandle(const FGameplayTag& Tag) const override;
	virtual FSoftObjectPath HandleTagMiss(const FGameplayTag& Tag) override;

	/** 임포트된 메시/BP에 대해 UHktActorVisualDataAsset 생성 및 저장 */
	FSoftObjectPath CreateActorVisualDataAsset(const FGameplayTag& Tag, const FSoftObjectPath& ActorClassPath, const FString& OutputDir);
};
