#include "HktMcpBridgeModule.h"
#include "WebSocketsModule.h"

DEFINE_LOG_CATEGORY(LogHktMcp);

#define LOCTEXT_NAMESPACE "FHktMcpBridgeModule"

void FHktMcpBridgeModule::StartupModule()
{
	// WebSockets 모듈 로드 확인
	if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
	{
		FModuleManager::Get().LoadModule("WebSockets");
	}

	UE_LOG(LogHktMcp, Log, TEXT("HktMcpBridge Module Started"));
}

void FHktMcpBridgeModule::ShutdownModule()
{
	UE_LOG(LogHktMcp, Log, TEXT("HktMcpBridge Module Shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHktMcpBridgeModule, HktMcpBridge)

