#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "AssetRegistry/AssetData.h"
#include "HktMcpEditorSubsystem.generated.h"

// 에셋 정보 구조체
USTRUCT(BlueprintType)
struct FHktAssetInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MCP")
	FString AssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "MCP")
	FString AssetName;

	UPROPERTY(BlueprintReadOnly, Category = "MCP")
	FString AssetClass;

	UPROPERTY(BlueprintReadOnly, Category = "MCP")
	FString PackagePath;
};

// 액터 정보 구조체
USTRUCT(BlueprintType)
struct FHktActorInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MCP")
	FString ActorName;

	UPROPERTY(BlueprintReadOnly, Category = "MCP")
	FString ActorLabel;

	UPROPERTY(BlueprintReadOnly, Category = "MCP")
	FString ActorClass;

	UPROPERTY(BlueprintReadOnly, Category = "MCP")
	FVector Location;

	UPROPERTY(BlueprintReadOnly, Category = "MCP")
	FRotator Rotation;

	UPROPERTY(BlueprintReadOnly, Category = "MCP")
	FVector Scale;

	UPROPERTY(BlueprintReadOnly, Category = "MCP")
	FString ActorGuid;
};

// 클래스 속성 정보
USTRUCT(BlueprintType)
struct FHktPropertyInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MCP")
	FString PropertyName;

	UPROPERTY(BlueprintReadOnly, Category = "MCP")
	FString PropertyType;

	UPROPERTY(BlueprintReadOnly, Category = "MCP")
	FString PropertyValue;

	UPROPERTY(BlueprintReadOnly, Category = "MCP")
	bool bIsEditable;
};

/**
 * 에디터 전용 서브시스템
 * Python MCP 서버에서 unreal 모듈을 통해 호출 가능한 함수들을 제공
 */
UCLASS()
class HKTMCPBRIDGEEDITOR_API UHktMcpEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	// UEditorSubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ==================== Asset Tools ====================
	
	// 에셋 목록 조회
	UFUNCTION(BlueprintCallable, Category = "MCP|Assets")
	TArray<FHktAssetInfo> ListAssets(const FString& Path, const FString& ClassFilter = TEXT(""));

	// 에셋 상세 정보 조회
	UFUNCTION(BlueprintCallable, Category = "MCP|Assets")
	FString GetAssetDetails(const FString& AssetPath);

	// 에셋 검색
	UFUNCTION(BlueprintCallable, Category = "MCP|Assets")
	TArray<FHktAssetInfo> SearchAssets(const FString& SearchQuery, const FString& ClassFilter = TEXT(""));

	// DataAsset 생성
	UFUNCTION(BlueprintCallable, Category = "MCP|Assets")
	bool CreateDataAsset(const FString& AssetPath, const FString& ParentClassName);

	// 에셋 속성 수정
	UFUNCTION(BlueprintCallable, Category = "MCP|Assets")
	bool ModifyAssetProperty(const FString& AssetPath, const FString& PropertyName, const FString& NewValue);

	// 에셋 삭제
	UFUNCTION(BlueprintCallable, Category = "MCP|Assets")
	bool DeleteAsset(const FString& AssetPath);

	// 에셋 복제
	UFUNCTION(BlueprintCallable, Category = "MCP|Assets")
	bool DuplicateAsset(const FString& SourcePath, const FString& DestinationPath);

	// ==================== Level Tools ====================

	// 현재 레벨의 액터 목록
	UFUNCTION(BlueprintCallable, Category = "MCP|Level")
	TArray<FHktActorInfo> ListActors(const FString& ClassFilter = TEXT(""));

	// 레벨에 액터 스폰
	UFUNCTION(BlueprintCallable, Category = "MCP|Level")
	FString SpawnActor(const FString& BlueprintPath, FVector Location, FRotator Rotation = FRotator::ZeroRotator, const FString& ActorLabel = TEXT(""));

	// 클래스명으로 기본 액터 스폰
	UFUNCTION(BlueprintCallable, Category = "MCP|Level")
	FString SpawnActorByClass(const FString& ClassName, FVector Location, FRotator Rotation = FRotator::ZeroRotator);

	// 액터 Transform 수정
	UFUNCTION(BlueprintCallable, Category = "MCP|Level")
	bool ModifyActorTransform(const FString& ActorName, FVector NewLocation, FRotator NewRotation, FVector NewScale);

	// 액터 속성 수정
	UFUNCTION(BlueprintCallable, Category = "MCP|Level")
	bool ModifyActorProperty(const FString& ActorName, const FString& PropertyName, const FString& NewValue);

	// 액터 삭제
	UFUNCTION(BlueprintCallable, Category = "MCP|Level")
	bool DeleteActor(const FString& ActorName);

	// 액터 선택
	UFUNCTION(BlueprintCallable, Category = "MCP|Level")
	bool SelectActor(const FString& ActorName);

	// 선택된 액터 목록
	UFUNCTION(BlueprintCallable, Category = "MCP|Level")
	TArray<FHktActorInfo> GetSelectedActors();

	// ==================== Query Tools ====================

	// 클래스 검색
	UFUNCTION(BlueprintCallable, Category = "MCP|Query")
	TArray<FString> SearchClasses(const FString& SearchQuery, bool bBlueprintOnly = false);

	// 클래스 속성 조회
	UFUNCTION(BlueprintCallable, Category = "MCP|Query")
	TArray<FHktPropertyInfo> GetClassProperties(const FString& ClassName);

	// 프로젝트 구조 조회
	UFUNCTION(BlueprintCallable, Category = "MCP|Query")
	FString GetProjectStructure(const FString& RootPath = TEXT("/Game"));

	// 현재 레벨 정보
	UFUNCTION(BlueprintCallable, Category = "MCP|Query")
	FString GetCurrentLevelInfo();

	// ==================== Editor Control ====================

	// 레벨 열기
	UFUNCTION(BlueprintCallable, Category = "MCP|Editor")
	bool OpenLevel(const FString& LevelPath);

	// 현재 레벨 저장
	UFUNCTION(BlueprintCallable, Category = "MCP|Editor")
	bool SaveCurrentLevel();

	// 새 레벨 생성
	UFUNCTION(BlueprintCallable, Category = "MCP|Editor")
	bool CreateNewLevel(const FString& LevelPath);

	// PIE 시작
	UFUNCTION(BlueprintCallable, Category = "MCP|Editor")
	bool StartPIE();

	// PIE 중지
	UFUNCTION(BlueprintCallable, Category = "MCP|Editor")
	bool StopPIE();

	// PIE 상태 확인
	UFUNCTION(BlueprintPure, Category = "MCP|Editor")
	bool IsPIERunning() const;

	// 에디터 콘솔 커맨드 실행
	UFUNCTION(BlueprintCallable, Category = "MCP|Editor")
	void ExecuteEditorCommand(const FString& Command);

	// ==================== Utility ====================

	// 현재 뷰포트 카메라 위치 가져오기
	UFUNCTION(BlueprintCallable, Category = "MCP|Utility")
	bool GetViewportCameraTransform(FVector& OutLocation, FRotator& OutRotation);

	// 뷰포트 카메라 이동
	UFUNCTION(BlueprintCallable, Category = "MCP|Utility")
	bool SetViewportCameraTransform(FVector Location, FRotator Rotation);

	// 에디터 알림 표시
	UFUNCTION(BlueprintCallable, Category = "MCP|Utility")
	void ShowNotification(const FString& Message, float Duration = 3.0f);

private:
	// 헬퍼 함수들
	AActor* FindActorByName(const FString& ActorName);
	UWorld* GetEditorWorld();
};

