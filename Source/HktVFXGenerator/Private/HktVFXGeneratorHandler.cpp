// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVFXGeneratorHandler.h"
#include "HktVFXAutoResolver.h"
#include "HktVFXGeneratorSubsystem.h"
#include "HktVFXGeneratorSettings.h"
#include "HktVFXNiagaraConfig.h"
#include "HktVFXIntent.h"
#include "HktAssetSubsystem.h"
#include "NiagaraSystem.h"
#include "DataAssets/HktVFXVisualDataAsset.h"
#include "Editor.h"
#include "UObject/SavePackage.h"
#include "PackageTools.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktVFXGeneratorHandler, Log, All);

bool UHktVFXGeneratorHandler::CanHandle(const FGameplayTag& Tag) const
{
	return Tag.IsValid() && Tag.ToString().StartsWith(TEXT("VFX."));
}

FSoftObjectPath UHktVFXGeneratorHandler::HandleTagMiss(const FGameplayTag& Tag)
{
	FHktVFXIntent Intent;
	if (!FHktVFXAutoResolver::ParseTagToIntent(Tag, Intent))
	{
		UE_LOG(LogHktVFXGeneratorHandler, Warning, TEXT("Failed to parse VFX tag: %s"), *Tag.ToString());
		return FSoftObjectPath();
	}

	// VFXGeneratorSubsystem으로 빌드
	UHktVFXGeneratorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UHktVFXGeneratorSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogHktVFXGeneratorHandler, Error, TEXT("VFXGeneratorSubsystem not available"));
		return FSoftObjectPath();
	}

	// Convention Path에서 출력 경로와 에셋 이름 결정
	FSoftObjectPath ConventionPath = UHktAssetSubsystem::ResolveConventionPath(Tag);
	FString OutputDir;
	FString SystemName;

	if (ConventionPath.IsValid())
	{
		// Package 경로만 추출 (.AssetName 제외)
		FString PackageName = ConventionPath.GetLongPackageName();
		int32 LastSlash;
		if (PackageName.FindLastChar('/', LastSlash))
		{
			OutputDir = PackageName.Left(LastSlash);
			SystemName = PackageName.Mid(LastSlash + 1);
		}
	}

	if (SystemName.IsEmpty())
	{
		FString TagStr = Tag.ToString();
		SystemName = TagStr;
		SystemName.ReplaceInline(TEXT("."), TEXT("_"));
	}

	// Builder가 NS_ 접두사를 자동으로 붙이므로 SystemName에서 제거해서 전달
	FString CleanName = SystemName;
	if (CleanName.StartsWith(TEXT("NS_")))
	{
		CleanName = CleanName.Mid(3);
	}
	FHktVFXNiagaraConfig Config = BuildDefaultConfig(Intent, CleanName);

	// Niagara 시스템 빌드
	UNiagaraSystem* System = Subsystem->BuildNiagaraFromConfig(Config, OutputDir);
	if (!System)
	{
		UE_LOG(LogHktVFXGeneratorHandler, Warning, TEXT("Failed to build Niagara for tag: %s"), *Tag.ToString());
		return FSoftObjectPath();
	}

	// TagDataAsset 생성 — UHktVFXVisualDataAsset로 NiagaraSystem 참조 등록
	FString ActualAssetName = FString::Printf(TEXT("NS_%s"), *CleanName);
	FSoftObjectPath NiagaraPath(FString::Printf(TEXT("%s/%s.%s"), *OutputDir, *ActualAssetName, *ActualAssetName));

	FSoftObjectPath DataAssetPath = CreateVFXDataAsset(Tag, NiagaraPath, OutputDir);
	if (!DataAssetPath.IsValid())
	{
		UE_LOG(LogHktVFXGeneratorHandler, Warning, TEXT("Failed to create VFX DataAsset for tag: %s, falling back to NiagaraSystem path"), *Tag.ToString());
		return NiagaraPath;
	}

	UE_LOG(LogHktVFXGeneratorHandler, Log, TEXT("VFX generated: %s → DataAsset=%s, Niagara=%s"), *Tag.ToString(), *DataAssetPath.ToString(), *NiagaraPath.ToString());
	return DataAssetPath;
}

FSoftObjectPath UHktVFXGeneratorHandler::CreateVFXDataAsset(const FGameplayTag& Tag, const FSoftObjectPath& NiagaraSystemPath, const FString& OutputDir)
{
	// 에셋 이름: DA_VFX_{TagPath}
	FString TagStr = Tag.ToString();
	FString SafeTagStr = TagStr;
	SafeTagStr.ReplaceInline(TEXT("."), TEXT("_"));
	FString AssetName = FString::Printf(TEXT("DA_VFX_%s"), *SafeTagStr);

	FString PackagePath = OutputDir.IsEmpty() ? TEXT("/Game/Generated/VFX") : OutputDir;
	FString FullPackagePath = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);

	UPackage* Package = CreatePackage(*FullPackagePath);
	if (!Package)
	{
		UE_LOG(LogHktVFXGeneratorHandler, Error, TEXT("Failed to create package: %s"), *FullPackagePath);
		return FSoftObjectPath();
	}

	UHktVFXVisualDataAsset* DataAsset = NewObject<UHktVFXVisualDataAsset>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!DataAsset)
	{
		UE_LOG(LogHktVFXGeneratorHandler, Error, TEXT("Failed to create VFX DataAsset: %s"), *AssetName);
		return FSoftObjectPath();
	}

	// IdentifierTag 설정
	DataAsset->IdentifierTag = Tag;

	// NiagaraSystem 하드 참조 설정 (런타임에서 DataAsset 비동기 로드 시 함께 로드됨)
	DataAsset->NiagaraSystem = Cast<UNiagaraSystem>(NiagaraSystemPath.TryLoad());

	// 패키지 저장
	DataAsset->MarkPackageDirty();

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	FString PackageFilename = FPackageName::LongPackageNameToFilename(FullPackagePath, FPackageName::GetAssetPackageExtension());
	bool bSaveResult = UPackage::SavePackage(Package, DataAsset, *PackageFilename, SaveArgs);
	if (!bSaveResult)
	{
		UE_LOG(LogHktVFXGeneratorHandler, Error, TEXT("Failed to save VFX DataAsset package: %s"), *FullPackagePath);
		return FSoftObjectPath();
	}

	FSoftObjectPath ResultPath(FString::Printf(TEXT("%s.%s"), *FullPackagePath, *AssetName));
	UE_LOG(LogHktVFXGeneratorHandler, Log, TEXT("Created VFX DataAsset: %s → Niagara=%s"), *ResultPath.ToString(), *NiagaraSystemPath.ToString());
	return ResultPath;
}

FHktVFXNiagaraConfig UHktVFXGeneratorHandler::BuildDefaultConfig(const FHktVFXIntent& Intent, const FString& SystemName)
{
	FHktVFXNiagaraConfig Config;
	Config.SystemName = SystemName;

	// 기본 에미터 추가
	FHktVFXEmitterConfig& Emitter = Config.Emitters.AddDefaulted_GetRef();
	Emitter.Name = TEXT("Main");

	// Spawn 설정 (Intent.Intensity 기반)
	Emitter.Spawn.Mode = TEXT("burst");
	Emitter.Spawn.BurstCount = FMath::Lerp(10, 100, Intent.Intensity);

	// Init 설정
	Emitter.Init.LifetimeMin = Intent.Duration * 0.5f;
	Emitter.Init.LifetimeMax = Intent.Duration;
	Emitter.Init.SizeMin = FMath::Lerp(5.f, 20.f, Intent.Intensity);
	Emitter.Init.SizeMax = FMath::Lerp(10.f, 50.f, Intent.Intensity);

	float SpeedMin = FMath::Lerp(50.f, 200.f, Intent.Intensity);
	float SpeedMax = FMath::Lerp(100.f, 500.f, Intent.Intensity);
	Emitter.Init.VelocityMin = FVector(-SpeedMin, -SpeedMin, SpeedMin * 0.5f);
	Emitter.Init.VelocityMax = FVector(SpeedMax, SpeedMax, SpeedMax);

	// Element에 따른 색상 매핑 (Init.Color + Update ColorOverLife)
	FLinearColor ColorStart;
	FLinearColor ColorEnd;
	switch (Intent.Element)
	{
	case EHktVFXElement::Fire:
		ColorStart = FLinearColor(1.f, 0.3f, 0.0f, 1.f);
		ColorEnd = FLinearColor(1.f, 0.8f, 0.1f, 0.f);
		break;
	case EHktVFXElement::Ice:
		ColorStart = FLinearColor(0.5f, 0.8f, 1.0f, 1.f);
		ColorEnd = FLinearColor(0.8f, 0.95f, 1.0f, 0.f);
		break;
	case EHktVFXElement::Lightning:
		ColorStart = FLinearColor(0.7f, 0.7f, 1.0f, 1.f);
		ColorEnd = FLinearColor(1.0f, 1.0f, 1.0f, 0.f);
		break;
	case EHktVFXElement::Dark:
		ColorStart = FLinearColor(0.1f, 0.0f, 0.2f, 1.f);
		ColorEnd = FLinearColor(0.3f, 0.0f, 0.5f, 0.f);
		break;
	case EHktVFXElement::Holy:
		ColorStart = FLinearColor(1.0f, 0.9f, 0.6f, 1.f);
		ColorEnd = FLinearColor(1.0f, 1.0f, 0.8f, 0.f);
		break;
	case EHktVFXElement::Poison:
		ColorStart = FLinearColor(0.2f, 0.8f, 0.1f, 1.f);
		ColorEnd = FLinearColor(0.4f, 1.0f, 0.2f, 0.f);
		break;
	default:
		ColorStart = FLinearColor(0.8f, 0.8f, 0.8f, 1.f);
		ColorEnd = FLinearColor(1.0f, 1.0f, 1.0f, 0.f);
		break;
	}
	Emitter.Init.Color = ColorStart;
	Emitter.Update.bUseColorOverLife = true;
	Emitter.Update.ColorEnd = ColorEnd;

	// Update 설정
	float GravityScale = (Intent.EventType == EHktVFXEventType::Explosion) ? 0.5f : 0.0f;
	Emitter.Update.Gravity = FVector(0.f, 0.f, -980.f * GravityScale);
	Emitter.Update.Drag = 0.5f;

	// Render 설정
	Emitter.Render.RendererType = TEXT("sprite");

	// EventType에 따른 Radius 반영 (SpawnShape)
	if (Intent.Radius > 0.f)
	{
		Emitter.Init.ShapeLocation.Shape = TEXT("sphere");
		Emitter.Init.ShapeLocation.SphereRadius = Intent.Radius;
	}

	return Config;
}
