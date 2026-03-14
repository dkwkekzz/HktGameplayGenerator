#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IWebSocket.h"
#include "HktMcpBridgeSubsystem.generated.h"

// JSON-RPC 요청 구조체
USTRUCT(BlueprintType)
struct FMcpRpcRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MCP")
	FString Id;

	UPROPERTY(BlueprintReadWrite, Category = "MCP")
	FString Method;

	UPROPERTY(BlueprintReadWrite, Category = "MCP")
	FString Params;  // JSON string
};

// JSON-RPC 응답 구조체
USTRUCT(BlueprintType)
struct FMcpRpcResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MCP")
	FString Id;

	UPROPERTY(BlueprintReadWrite, Category = "MCP")
	bool bSuccess;

	UPROPERTY(BlueprintReadWrite, Category = "MCP")
	FString Result;  // JSON string

	UPROPERTY(BlueprintReadWrite, Category = "MCP")
	FString Error;
};

// 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMcpRpcRequest, const FMcpRpcRequest&, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMcpConnectionChanged, bool, bConnected);

/**
 * 런타임 WebSocket 서버 역할을 수행하는 서브시스템
 * MCP Server(Python)가 WebSocket Client로 접속하여 게임 상태 조회/제어
 */
UCLASS()
class HKTMCPBRIDGE_API UHktMcpBridgeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	// WebSocket 서버 시작
	UFUNCTION(BlueprintCallable, Category = "MCP|Runtime")
	bool StartServer(int32 Port = 9876);

	// WebSocket 서버 중지
	UFUNCTION(BlueprintCallable, Category = "MCP|Runtime")
	void StopServer();

	// 서버 상태 확인
	UFUNCTION(BlueprintPure, Category = "MCP|Runtime")
	bool IsServerRunning() const { return bServerRunning; }

	// 연결된 클라이언트 수
	UFUNCTION(BlueprintPure, Category = "MCP|Runtime")
	int32 GetConnectedClientCount() const { return ConnectedClients.Num(); }

	// RPC 응답 전송
	UFUNCTION(BlueprintCallable, Category = "MCP|Runtime")
	void SendRpcResponse(const FMcpRpcResponse& Response);

	// 모든 클라이언트에 브로드캐스트
	UFUNCTION(BlueprintCallable, Category = "MCP|Runtime")
	void BroadcastMessage(const FString& Message);

	// 게임 상태 정보 반환
	UFUNCTION(BlueprintCallable, Category = "MCP|Runtime")
	FString GetGameStateJson() const;

	// 콘솔 커맨드 실행
	UFUNCTION(BlueprintCallable, Category = "MCP|Runtime")
	FString ExecuteConsoleCommand(const FString& Command);

	// 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "MCP|Runtime")
	FOnMcpRpcRequest OnRpcRequestReceived;

	UPROPERTY(BlueprintAssignable, Category = "MCP|Runtime")
	FOnMcpConnectionChanged OnConnectionChanged;

private:
	// 메시지 처리
	void ProcessMessage(const FString& ClientId, const FString& Message);
	FMcpRpcRequest ParseRpcRequest(const FString& JsonString);
	FString SerializeRpcResponse(const FMcpRpcResponse& Response);

	// 내장 RPC 핸들러
	void HandleGetGameState(const FMcpRpcRequest& Request);
	void HandleExecuteCommand(const FMcpRpcRequest& Request);
	void HandleGetActorList(const FMcpRpcRequest& Request);

	// WebSocket 서버 관련
	TSharedPtr<class INetworkingWebSocket> ServerSocket;
	TMap<FString, TSharedPtr<class INetworkingWebSocket>> ConnectedClients;
	bool bServerRunning = false;
	int32 ServerPort = 9876;

	// Tick 핸들러
	FDelegateHandle TickDelegateHandle;
	bool Tick(float DeltaTime);
};

