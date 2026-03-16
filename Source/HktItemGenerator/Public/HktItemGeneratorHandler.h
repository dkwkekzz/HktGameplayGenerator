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
 */
UCLASS()
class HKTITEMGENERATOR_API UHktItemGeneratorHandler : public UObject, public IHktGeneratorHandler
{
	GENERATED_BODY()

public:
	virtual bool CanHandle(const FGameplayTag& Tag) const override;
	virtual FSoftObjectPath HandleTagMiss(const FGameplayTag& Tag) override;
};
