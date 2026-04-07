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

// ==================== Generation Request/Response ====================

/** 생성 요청 상태 */
UENUM(BlueprintType)
enum class EHktGenerationStatus : uint8
{
	Pending,      // 큐에 대기 중
	InProgress,   // 외부 에이전트가 처리 중
	Completed,    // 완료
	Failed,       // 실패
	Cancelled     // 취소
};

/** 외부 에이전트 접속 정보 */
USTRUCT(BlueprintType)
struct FHktAgentInfo
{
	GENERATED_BODY()

	/** 에이전트 고유 ID (클라이언트가 생성) */
	UPROPERTY(BlueprintReadWrite, Category = "MCP|Agent")
	FString AgentId;

	/**
	 * 에이전트 프로바이더 타입.
	 * 예: "claude-cli", "anthropic-api", "openai-api", "custom"
	 * UI와 요청 라우팅에서 참조.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "MCP|Agent")
	FString Provider;

	/** 사람이 읽을 수 있는 에이전트 이름 (예: "Claude Code 1.0.3") */
	UPROPERTY(BlueprintReadWrite, Category = "MCP|Agent")
	FString DisplayName;

	/** 에이전트 버전 문자열 */
	UPROPERTY(BlueprintReadWrite, Category = "MCP|Agent")
	FString Version;

	/** 에이전트가 지원하는 기능 목록 (예: "generation", "nl-convert", "sub-agent") */
	UPROPERTY(BlueprintReadWrite, Category = "MCP|Agent")
	TArray<FString> Capabilities;
};

/** 생성 요청 — 외부 에이전트에 전달 */
USTRUCT(BlueprintType)
struct FHktGenerationRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MCP|Generation")
	FString RequestId;          // 고유 요청 ID

	UPROPERTY(BlueprintReadWrite, Category = "MCP|Generation")
	FString SkillName;          // "vfx-gen", "char-gen" 등

	UPROPERTY(BlueprintReadWrite, Category = "MCP|Generation")
	FString StepType;           // "vfx_generation" 등

	UPROPERTY(BlueprintReadWrite, Category = "MCP|Generation")
	FString ProjectId;

	UPROPERTY(BlueprintReadWrite, Category = "MCP|Generation")
	FString IntentJson;         // Intent JSON 전체

	UPROPERTY(BlueprintReadWrite, Category = "MCP|Generation")
	FString SystemPrompt;       // SKILL.md 내용

	UPROPERTY(BlueprintReadWrite, Category = "MCP|Generation")
	FString Feedback;           // Refine 시 피드백

	UPROPERTY(BlueprintReadWrite, Category = "MCP|Generation")
	FString PreviousResult;     // Refine 시 이전 결과

	UPROPERTY(BlueprintReadWrite, Category = "MCP|Generation")
	EHktGenerationStatus Status = EHktGenerationStatus::Pending;
};

/** 생성 진행 이벤트 — 외부 에이전트가 보내는 진행 업데이트 */
USTRUCT(BlueprintType)
struct FHktGenerationEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MCP|Generation")
	FString RequestId;

	UPROPERTY(BlueprintReadWrite, Category = "MCP|Generation")
	FString EventType;          // "assistant", "tool_use", "tool_result", "result", "error", "complete"

	UPROPERTY(BlueprintReadWrite, Category = "MCP|Generation")
	FString Content;

	UPROPERTY(BlueprintReadWrite, Category = "MCP|Generation")
	FString ToolName;

	UPROPERTY(BlueprintReadWrite, Category = "MCP|Generation")
	FString ToolInput;

	UPROPERTY(BlueprintReadWrite, Category = "MCP|Generation")
	int32 ExitCode = 0;
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

	// ==================== Generation Request Queue ====================

	/**
	 * 생성 요청을 큐에 추가. 외부 에이전트가 PollGenerationRequest로 가져감.
	 * @return RequestId
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Generation")
	FString SubmitGenerationRequest(const FHktGenerationRequest& Request);

	/**
	 * 외부 에이전트가 호출 — Pending 요청을 하나 가져감.
	 * 없으면 빈 RequestId 반환.
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Generation")
	FHktGenerationRequest PollGenerationRequest();

	/**
	 * 외부 에이전트가 진행 이벤트를 보냄.
	 * 해당 RequestId를 구독 중인 UI에 브로드캐스트.
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Generation")
	void SendGenerationEvent(const FHktGenerationEvent& Event);

	/** 요청 취소 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Generation")
	void CancelGenerationRequest(const FString& RequestId);

	/** 특정 요청의 현재 상태 조회 */
	UFUNCTION(BlueprintPure, Category = "MCP|Generation")
	EHktGenerationStatus GetGenerationStatus(const FString& RequestId) const;

	/** 생성 이벤트 수신 델리게이트 — UI가 구독 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnGenerationEvent, const FHktGenerationEvent&);
	FOnGenerationEvent OnGenerationEvent;

	// ==================== External Agent Connection ====================

	/**
	 * 외부 에이전트가 연결 시 호출.
	 * 에이전트 정보를 등록하고, UI에 연결 상태를 표시.
	 * 동일 AgentId로 재연결 시 정보만 갱신.
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Agent")
	void ConnectAgent(const FHktAgentInfo& AgentInfo);

	/**
	 * 외부 에이전트가 주기적으로 호출 (권장: 3~5초 간격).
	 * 연결 유지 + 생성 요청 Pending 여부 반환.
	 * @return Pending 요청이 있으면 true
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Agent")
	bool Heartbeat(const FString& AgentId);

	/**
	 * 외부 에이전트가 정상 종료 시 호출.
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Agent")
	void DisconnectAgent(const FString& AgentId);

	/** 외부 에이전트가 연결되어 있는지 (Heartbeat 기반) */
	UFUNCTION(BlueprintPure, Category = "MCP|Agent")
	bool IsExternalAgentConnected() const;

	/** 현재 연결된 에이전트 정보 (없으면 빈 구조체) */
	UFUNCTION(BlueprintPure, Category = "MCP|Agent")
	FHktAgentInfo GetConnectedAgentInfo() const;

	/** 연결된 에이전트 수 */
	UFUNCTION(BlueprintPure, Category = "MCP|Agent")
	int32 GetConnectedAgentCount() const;

	/** 에이전트 연결/해제 시 브로드캐스트 (bConnected, AgentInfo) */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAgentConnectionChanged, bool, const FHktAgentInfo&);
	FOnAgentConnectionChanged OnAgentConnectionChanged;

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

	/** DataAsset 생성 + 속성 일괄 설정. 아무 UDataAsset 서브클래스에 사용 가능.
	 *  PropertiesJson: {"PropertyName": "Value", ...} 형식. 모든 ModifyAssetProperty 지원 타입 사용 가능. */
	UFUNCTION(BlueprintCallable, Category = "MCP|Assets")
	FString CreateDataAssetWithProperties(const FString& AssetPath, const FString& ParentClassName, const FString& PropertiesJson);

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

	// ==================== Python Script Executor ====================

	/** Execute Python script and capture stdout/stderr output */
	UFUNCTION(BlueprintCallable, Category = "MCP|Python")
	FString ExecutePythonScript(const FString& ScriptCode, float TimeoutSeconds = 30.0f);

	// ==================== MCP Bridge Server (Editor-level) ====================

	/** MCP Bridge 서버 시작. 에디터 시작 시 자동 호출됨. */
	UFUNCTION(BlueprintCallable, Category = "MCP|Server")
	bool StartMcpServer(int32 Port = 0);

	/** MCP Bridge 서버 중지 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Server")
	void StopMcpServer();

	/** MCP Bridge 서버 재연결 (Stop → Start) */
	UFUNCTION(BlueprintCallable, Category = "MCP|Server")
	bool ReconnectMcpServer();

	/** MCP Bridge 서버 실행 중인지 확인 */
	UFUNCTION(BlueprintPure, Category = "MCP|Server")
	bool IsMcpServerRunning() const { return bMcpServerRunning; }

	/** 연결된 MCP 클라이언트 수 */
	UFUNCTION(BlueprintPure, Category = "MCP|Server")
	int32 GetMcpClientCount() const { return McpClientCount; }

	/** MCP 서버 포트 */
	UFUNCTION(BlueprintPure, Category = "MCP|Server")
	int32 GetMcpServerPort() const { return McpServerPort; }

	// ==================== Agent Connection Verification ====================

	/** Claude CLI가 유효한지 비동기 검증. 완료 시 OnAgentVerified 브로드캐스트.
	 *  @param CLIPathOverride  비어있으면 자동 탐색, 지정하면 해당 경로 사용 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Agent")
	void VerifyAgentConnection(const FString& CLIPathOverride = TEXT(""));

	/** 마지막 검증 결과 */
	UFUNCTION(BlueprintPure, Category = "MCP|Agent")
	bool IsAgentVerified() const { return bAgentVerified; }

	/** 마지막 검증된 CLI 버전 */
	UFUNCTION(BlueprintPure, Category = "MCP|Agent")
	FString GetAgentVersion() const { return AgentVersionString; }

	/** 마지막 검증된 CLI 경로 */
	UFUNCTION(BlueprintPure, Category = "MCP|Agent")
	FString GetAgentCLIPath() const { return AgentCLIPath; }

	/** Agent 검증 진행 중인지 */
	UFUNCTION(BlueprintPure, Category = "MCP|Agent")
	bool IsAgentVerifying() const { return bAgentVerifying; }

	/** 에이전트 검증 완료 델리게이트 (bSuccess, VersionOrError) */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAgentVerified, bool, const FString&);
	FOnAgentVerified OnAgentVerified;

	/** MCP 서버 상태 변경 델리게이트 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMcpServerStateChanged, bool);
	FOnMcpServerStateChanged OnMcpServerStateChanged;

private:
	// 헬퍼 함수들
	AActor* FindActorByName(const FString& ActorName);
	UWorld* GetEditorWorld();

	// ── MCP Server State ──
	bool bMcpServerRunning = false;
	int32 McpServerPort = 9876;
	int32 McpClientCount = 0;  // TODO: 실제 WebSocket 서버 구현 시 클라이언트 연결 추적으로 교체

	// ── Agent Verification State ──
	bool bAgentVerified = false;
	bool bAgentVerifying = false;
	FString AgentVersionString;
	FString AgentCLIPath;

	// ── Generation Request Queue ──
	TMap<FString, FHktGenerationRequest> GenerationRequests;  // RequestId → Request
	TArray<FString> PendingRequestQueue;                       // Pending RequestId 순서
	int32 NextRequestId = 1;

	// ── External Agent Connection ──
	struct FAgentConnection
	{
		FHktAgentInfo Info;
		double LastHeartbeat = 0.0;
	};
	TMap<FString, FAgentConnection> ConnectedAgents;           // AgentId → Connection
	static constexpr double AgentTimeoutSeconds = 15.0;        // Heartbeat 타임아웃

	/** 타임아웃된 에이전트 정리 (Heartbeat/Poll 호출 시 함께 실행) */
	void CleanupTimedOutAgents();
};

