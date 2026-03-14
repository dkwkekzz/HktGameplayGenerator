// Copyright Hkt Studios, Inc. All Rights Reserved.
// Anim Generator Handler — "Anim.*" Tag miss 처리

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HktGeneratorRouter.h"
#include "HktAnimGeneratorHandler.generated.h"

/**
 * UHktAnimGeneratorHandler
 *
 * "Anim.*" 태그에 대한 IHktGeneratorHandler 구현.
 * FHktAnimIntent로 파싱 → MCP Agent에게 애니메이션 생성 위임.
 */
UCLASS()
class HKTANIMGENERATOR_API UHktAnimGeneratorHandler : public UObject, public IHktGeneratorHandler
{
	GENERATED_BODY()

public:
	virtual bool CanHandle(const FGameplayTag& Tag) const override;
	virtual FSoftObjectPath HandleTagMiss(const FGameplayTag& Tag) override;
};
