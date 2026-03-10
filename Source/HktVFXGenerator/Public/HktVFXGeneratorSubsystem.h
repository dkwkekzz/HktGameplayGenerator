// Copyright Hkt Studios, Inc. All Rights Reserved.
// VFX Generator Editor Subsystem — Config→Niagara 빌드 API 표면

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "HktVFXNiagaraConfig.h"
#include "HktVFXNiagaraBuilder.h"
#include "HktVFXGeneratorSubsystem.generated.h"

class UNiagaraSystem;

/**
 * UHktVFXGeneratorSubsystem
 *
 * 에디터 서브시스템으로 VFX Config→Niagara 빌드 기능 제공.
 * 설정은 UHktVFXGeneratorSettings (Project Settings > Plugins > HKT VFX Generator).
 */
UCLASS()
class HKTVFXGENERATOR_API UHktVFXGeneratorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Config 구조체로 Niagara 시스템 빌드. OutputDir 비어있으면 Settings 기본값 사용. */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator")
	UNiagaraSystem* BuildNiagaraFromConfig(
		const FHktVFXNiagaraConfig& Config,
		const FString& OutputDir = TEXT(""));

	/** JSON 문자열로 Niagara 시스템 빌드. */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator")
	UNiagaraSystem* BuildNiagaraFromJson(
		const FString& JsonStr,
		const FString& OutputDir = TEXT(""));

	/** 프리셋 폭발 이펙트 빌드 (테스트용). */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator")
	UNiagaraSystem* BuildPresetExplosion(
		const FString& Name,
		FLinearColor Color = FLinearColor(1.f, 0.5f, 0.1f, 1.f),
		float Intensity = 0.5f,
		const FString& OutputDir = TEXT(""));

	/** Config JSON 스키마 반환 (LLM 학습용). */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator")
	FString GetConfigSchemaJson() const;

	/** 템플릿 에미터의 실제 RapidIterationParameter 이름 덤프 (디버그용). */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator")
	FString DumpTemplateParameters(const FString& RendererType = TEXT("sprite"));

	/** 모든 템플릿 에미터의 RapidIterationParameter 덤프 (Phase 2 문서화용). */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator")
	FString DumpAllTemplateParameters();

private:
	FString ResolveOutputDir(const FString& OutputDir) const;

	FHktVFXNiagaraBuilder Builder;
};
