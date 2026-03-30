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

	// =========================================================================
	// Phase 2: 머티리얼 동적 생성
	// =========================================================================

	/**
	 * 파티클용 MaterialInstance 동적 생성.
	 * 마스터 머티리얼 기반으로 텍스처 바인딩 + Emissive 세기를 설정한 MI를 저장.
	 * @return 생성된 MI 에셋 경로. 실패 시 빈 문자열.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator")
	FString CreateParticleMaterial(
		const FString& MaterialName,
		const FString& TexturePath,
		const FString& BlendMode,
		float EmissiveIntensity,
		const FString& OutputDir = TEXT(""));

	/**
	 * 기존 Niagara 시스템의 에미터에 머티리얼 할당.
	 * @return 성공 여부.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator")
	bool AssignMaterialToEmitter(
		const FString& NiagaraSystemPath,
		const FString& EmitterName,
		const FString& MaterialPath);

	// =========================================================================
	// Phase 4: 프리뷰 / 튜닝
	// =========================================================================

	/**
	 * VFX를 뷰포트에 스폰 + 스크린샷 캡처.
	 * @return 스크린샷 파일 경로. 실패 시 빈 문자열.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator")
	FString PreviewVFX(
		const FString& NiagaraSystemPath,
		float Duration = 2.f,
		const FString& ScreenshotPath = TEXT(""));

	/**
	 * 기존 Niagara 시스템의 에미터 파라미터를 부분 업데이트.
	 * 전체 리빌드 없이 Spawn/Init/Update/Render 파라미터를 변경.
	 * @return 성공 여부.
	 */
	UFUNCTION(BlueprintCallable, Category = "HKT|VFXGenerator")
	bool UpdateEmitterParameters(
		const FString& NiagaraSystemPath,
		const FString& EmitterName,
		const FString& JsonOverrides);

private:
	FString ResolveOutputDir(const FString& OutputDir) const;

	FHktVFXNiagaraBuilder Builder;

	UPROPERTY()
	TObjectPtr<class UHktVFXGeneratorHandler> VFXHandler;
};
