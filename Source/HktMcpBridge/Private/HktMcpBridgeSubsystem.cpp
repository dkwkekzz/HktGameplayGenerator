#include "HktMcpBridgeSubsystem.h"
#include "HktMcpBridgeModule.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

void UHktMcpBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogHktMcp, Log, TEXT("HktMcpBridgeSubsystem Initialized"));
}

void UHktMcpBridgeSubsystem::Deinitialize()
{
	StopServer();
	Super::Deinitialize();
	UE_LOG(LogHktMcp, Log, TEXT("HktMcpBridgeSubsystem Deinitialized"));
}

bool UHktMcpBridgeSubsystem::StartServer(int32 Port)
{
	if (bServerRunning)
	{
		UE_LOG(LogHktMcp, Warning, TEXT("WebSocket server already running on port %d"), ServerPort);
		return false;
	}

	ServerPort = Port;
	
	// TODO: 실제 WebSocket 서버 구현
	// UE5의 WebSocket 모듈은 클라이언트만 지원하므로
	// 서버 기능은 별도 구현 필요 (libwebsocket 또는 TCP 서버 + WS 프로토콜)
	// 현재는 placeholder로 둠
	
	bServerRunning = true;
	UE_LOG(LogHktMcp, Log, TEXT("WebSocket server started on port %d"), ServerPort);
	
	OnConnectionChanged.Broadcast(true);
	return true;
}

void UHktMcpBridgeSubsystem::StopServer()
{
	if (!bServerRunning)
	{
		return;
	}

	// 모든 클라이언트 연결 종료
	ConnectedClients.Empty();
	bServerRunning = false;
	
	UE_LOG(LogHktMcp, Log, TEXT("WebSocket server stopped"));
	OnConnectionChanged.Broadcast(false);
}

void UHktMcpBridgeSubsystem::SendRpcResponse(const FMcpRpcResponse& Response)
{
	FString JsonResponse = SerializeRpcResponse(Response);
	BroadcastMessage(JsonResponse);
}

void UHktMcpBridgeSubsystem::BroadcastMessage(const FString& Message)
{
	// TODO: 모든 연결된 클라이언트에 메시지 전송
	UE_LOG(LogHktMcp, Verbose, TEXT("Broadcasting: %s"), *Message);
}

void UHktMcpBridgeSubsystem::ProcessMessage(const FString& ClientId, const FString& Message)
{
	UE_LOG(LogHktMcp, Log, TEXT("Received from %s: %s"), *ClientId, *Message);
	
	FMcpRpcRequest Request = ParseRpcRequest(Message);
	
	if (Request.Id.IsEmpty())
	{
		UE_LOG(LogHktMcp, Warning, TEXT("Invalid RPC request received"));
		return;
	}

	// 내장 핸들러 먼저 확인
	if (Request.Method == TEXT("get_game_state"))
	{
		HandleGetGameState(Request);
		return;
	}
	else if (Request.Method == TEXT("execute_command"))
	{
		HandleExecuteCommand(Request);
		return;
	}
	else if (Request.Method == TEXT("get_actor_list"))
	{
		HandleGetActorList(Request);
		return;
	}

	// 외부 핸들러에게 브로드캐스트
	OnRpcRequestReceived.Broadcast(Request);
}

FMcpRpcRequest UHktMcpBridgeSubsystem::ParseRpcRequest(const FString& JsonString)
{
	FMcpRpcRequest Request;
	
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return Request;
	}

	Request.Id = JsonObject->GetStringField(TEXT("id"));
	Request.Method = JsonObject->GetStringField(TEXT("method"));
	
	const TSharedPtr<FJsonObject>* ParamsObject;
	if (JsonObject->TryGetObjectField(TEXT("params"), ParamsObject))
	{
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Request.Params);
		FJsonSerializer::Serialize(ParamsObject->ToSharedRef(), Writer);
	}

	return Request;
}

FString UHktMcpBridgeSubsystem::SerializeRpcResponse(const FMcpRpcResponse& Response)
{
	TSharedRef<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	
	JsonObject->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	JsonObject->SetStringField(TEXT("id"), Response.Id);
	
	if (Response.bSuccess)
	{
		// result를 JSON 객체로 파싱
		TSharedPtr<FJsonObject> ResultObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response.Result);
		if (FJsonSerializer::Deserialize(Reader, ResultObject) && ResultObject.IsValid())
		{
			JsonObject->SetObjectField(TEXT("result"), ResultObject);
		}
		else
		{
			JsonObject->SetStringField(TEXT("result"), Response.Result);
		}
	}
	else
	{
		TSharedRef<FJsonObject> ErrorObject = MakeShareable(new FJsonObject);
		ErrorObject->SetNumberField(TEXT("code"), -32000);
		ErrorObject->SetStringField(TEXT("message"), Response.Error);
		JsonObject->SetObjectField(TEXT("error"), ErrorObject);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject, Writer);
	
	return OutputString;
}

FString UHktMcpBridgeSubsystem::GetGameStateJson() const
{
	TSharedRef<FJsonObject> StateObject = MakeShareable(new FJsonObject);
	
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		StateObject->SetStringField(TEXT("error"), TEXT("No game instance"));
		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(StateObject, Writer);
		return Output;
	}

	UWorld* World = GameInstance->GetWorld();
	if (World)
	{
		StateObject->SetStringField(TEXT("world_name"), World->GetName());
		StateObject->SetStringField(TEXT("map_name"), World->GetMapName());
		StateObject->SetBoolField(TEXT("is_playing"), World->IsPlayInEditor() || World->IsGameWorld());
		StateObject->SetNumberField(TEXT("time_seconds"), World->GetTimeSeconds());
		
		// 플레이어 정보
		APlayerController* PC = World->GetFirstPlayerController();
		if (PC && PC->GetPawn())
		{
			TSharedRef<FJsonObject> PlayerObject = MakeShareable(new FJsonObject);
			FVector Location = PC->GetPawn()->GetActorLocation();
			PlayerObject->SetNumberField(TEXT("x"), Location.X);
			PlayerObject->SetNumberField(TEXT("y"), Location.Y);
			PlayerObject->SetNumberField(TEXT("z"), Location.Z);
			StateObject->SetObjectField(TEXT("player_location"), PlayerObject);
		}

		// 액터 수
		int32 ActorCount = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			ActorCount++;
		}
		StateObject->SetNumberField(TEXT("actor_count"), ActorCount);
	}

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(StateObject, Writer);
	return Output;
}

FString UHktMcpBridgeSubsystem::ExecuteConsoleCommand(const FString& Command)
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return TEXT("{\"error\": \"No game instance\"}");
	}

	UWorld* World = GameInstance->GetWorld();
	if (!World)
	{
		return TEXT("{\"error\": \"No world\"}");
	}

	// 콘솔 커맨드 실행
	GEngine->Exec(World, *Command);
	
	TSharedRef<FJsonObject> ResultObject = MakeShareable(new FJsonObject);
	ResultObject->SetBoolField(TEXT("success"), true);
	ResultObject->SetStringField(TEXT("command"), Command);
	
	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(ResultObject, Writer);
	return Output;
}

void UHktMcpBridgeSubsystem::HandleGetGameState(const FMcpRpcRequest& Request)
{
	FMcpRpcResponse Response;
	Response.Id = Request.Id;
	Response.bSuccess = true;
	Response.Result = GetGameStateJson();
	SendRpcResponse(Response);
}

void UHktMcpBridgeSubsystem::HandleExecuteCommand(const FMcpRpcRequest& Request)
{
	// params에서 command 추출
	TSharedPtr<FJsonObject> ParamsObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Request.Params);
	
	FMcpRpcResponse Response;
	Response.Id = Request.Id;
	
	if (FJsonSerializer::Deserialize(Reader, ParamsObject) && ParamsObject.IsValid())
	{
		FString Command = ParamsObject->GetStringField(TEXT("command"));
		Response.bSuccess = true;
		Response.Result = ExecuteConsoleCommand(Command);
	}
	else
	{
		Response.bSuccess = false;
		Response.Error = TEXT("Invalid params");
	}
	
	SendRpcResponse(Response);
}

void UHktMcpBridgeSubsystem::HandleGetActorList(const FMcpRpcRequest& Request)
{
	FMcpRpcResponse Response;
	Response.Id = Request.Id;
	
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	
	if (!World)
	{
		Response.bSuccess = false;
		Response.Error = TEXT("No world available");
		SendRpcResponse(Response);
		return;
	}

	TArray<TSharedPtr<FJsonValue>> ActorArray;
	
	// params에서 class_filter 추출
	FString ClassFilter;
	TSharedPtr<FJsonObject> ParamsObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Request.Params);
	if (FJsonSerializer::Deserialize(Reader, ParamsObject) && ParamsObject.IsValid())
	{
		ClassFilter = ParamsObject->GetStringField(TEXT("class_filter"));
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		
		// 클래스 필터 적용
		if (!ClassFilter.IsEmpty())
		{
			if (!Actor->GetClass()->GetName().Contains(ClassFilter))
			{
				continue;
			}
		}

		TSharedRef<FJsonObject> ActorObject = MakeShareable(new FJsonObject);
		ActorObject->SetStringField(TEXT("name"), Actor->GetName());
		ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
		ActorObject->SetStringField(TEXT("label"), Actor->GetActorLabel());
		
		FVector Location = Actor->GetActorLocation();
		TSharedRef<FJsonObject> LocationObject = MakeShareable(new FJsonObject);
		LocationObject->SetNumberField(TEXT("x"), Location.X);
		LocationObject->SetNumberField(TEXT("y"), Location.Y);
		LocationObject->SetNumberField(TEXT("z"), Location.Z);
		ActorObject->SetObjectField(TEXT("location"), LocationObject);
		
		FRotator Rotation = Actor->GetActorRotation();
		TSharedRef<FJsonObject> RotationObject = MakeShareable(new FJsonObject);
		RotationObject->SetNumberField(TEXT("pitch"), Rotation.Pitch);
		RotationObject->SetNumberField(TEXT("yaw"), Rotation.Yaw);
		RotationObject->SetNumberField(TEXT("roll"), Rotation.Roll);
		ActorObject->SetObjectField(TEXT("rotation"), RotationObject);
		
		ActorArray.Add(MakeShareable(new FJsonValueObject(ActorObject)));
	}

	TSharedRef<FJsonObject> ResultObject = MakeShareable(new FJsonObject);
	ResultObject->SetArrayField(TEXT("actors"), ActorArray);
	ResultObject->SetNumberField(TEXT("count"), ActorArray.Num());
	
	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(ResultObject, Writer);
	
	Response.bSuccess = true;
	Response.Result = Output;
	SendRpcResponse(Response);
}

bool UHktMcpBridgeSubsystem::Tick(float DeltaTime)
{
	// 서버 메시지 폴링 처리
	return true;
}

