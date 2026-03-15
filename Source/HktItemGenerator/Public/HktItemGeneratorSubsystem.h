// Copyright Hkt Studios, Inc. All Rights Reserved.
// Item Generator EditorSubsystem — Entity.Item 에셋 생성 API

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "HktAnimGeneratorTypes.h"
#include "HktTextureIntent.h"
#include "HktItemGeneratorSubsystem.generated.h"

/**
 * 아이템 생성 요청 상태
 */
USTRUCT(BlueprintType)
struct HKTITEMGENERATOR_API FHktItemGenerationRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	FHktItemIntent Intent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	FSoftObjectPath ConventionPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	bool bCompleted = false;

	/** MCP Agent용 메시 생성 프롬프트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	FString MeshPrompt;

	/** MCP Agent용 아이콘 생성 프롬프트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	FString IconPrompt;

	/** 장착 소켓 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	FName AttachSocket;

	/** 장착 방식 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	FString AttachMethod;
};

/**
 * UHktItemGeneratorSubsystem
 *
 * Entity.Item 에셋 생성 EditorSubsystem.
 * 아이템 = StaticMesh + ItemIcon(Texture) + Material Instance
 * ItemIcon은 HktTextureGenerator에 위임.
 */
UCLASS()
class HKTITEMGENERATOR_API UHktItemGeneratorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// =========================================================================
	// 생성 API
	// =========================================================================

	/** Intent에서 아이템 생성 요청. Convention Path 반환. */
	FSoftObjectPath RequestItemGeneration(const FHktItemIntent& Intent);

	/** 아이콘 텍스처 생성 (HktTextureGenerator 위임) */
	UFUNCTION(BlueprintCallable, Category = "HKT|ItemGenerator")
	UTexture2D* GenerateItemIcon(const FHktItemIntent& Intent);

	/** 외부 FBX/OBJ 메시 임포트 */
	UFUNCTION(BlueprintCallable, Category = "HKT|ItemGenerator")
	UObject* ImportItemMesh(const FString& FilePath, const FString& DestinationPath = TEXT(""));

	// =========================================================================
	// MCP JSON API
	// =========================================================================

	/** 아이템 생성 요청 (JSON) — Intent 파싱 + 메시/아이콘 프롬프트 반환 */
	UFUNCTION(BlueprintCallable, Category = "HKT|ItemGenerator")
	FString McpRequestItem(const FString& JsonIntent);

	/** 외부 메시 임포트 (JSON) */
	UFUNCTION(BlueprintCallable, Category = "HKT|ItemGenerator")
	FString McpImportItemMesh(const FString& FilePath, const FString& JsonIntent);

	/** 펜딩 생성 요청 목록 */
	UFUNCTION(BlueprintCallable, Category = "HKT|ItemGenerator")
	FString McpGetPendingRequests();

	/** 생성된 아이템 목록 */
	UFUNCTION(BlueprintCallable, Category = "HKT|ItemGenerator")
	FString McpListGeneratedItems(const FString& Directory = TEXT(""));

	/** 소켓 매핑 정보 (장착 위치 가이드) */
	UFUNCTION(BlueprintCallable, Category = "HKT|ItemGenerator")
	FString McpGetSocketMappings();

private:
	FString ResolveOutputDir(const FString& OutputDir) const;
	FString BuildMeshPrompt(const FHktItemIntent& Intent) const;
	FString BuildIconPrompt(const FHktItemIntent& Intent) const;
	FSoftObjectPath ResolveConventionPath(const FHktItemIntent& Intent) const;
	void ResolveSocketInfo(const FString& Category, FName& OutSocket, FString& OutMethod) const;

	TArray<FHktItemGenerationRequest> PendingRequests;

	UPROPERTY()
	TObjectPtr<class UHktItemGeneratorHandler> ItemHandler;
};
