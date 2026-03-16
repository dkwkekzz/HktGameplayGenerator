#include "HktMcpBridgeEditorModule.h"
#include "HktMcpEditorSubsystem.h"
#include "HktMcpBridgeModule.h"
#include "SHktPipelinePanel.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

DEFINE_LOG_CATEGORY(LogHktMcpEditor);

const FName FHktMcpBridgeEditorModule::PipelineTabName(TEXT("HktPipelineMonitor"));

#define LOCTEXT_NAMESPACE "FHktMcpBridgeEditorModule"

void FHktMcpBridgeEditorModule::StartupModule()
{
	RegisterConsoleCommands();
	RegisterPipelineTab();
	UE_LOG(LogHktMcpEditor, Log, TEXT("HktMcpBridgeEditor Module Started"));
}

void FHktMcpBridgeEditorModule::ShutdownModule()
{
	UnregisterConsoleCommands();
	UnregisterPipelineTab();
	UE_LOG(LogHktMcpEditor, Log, TEXT("HktMcpBridgeEditor Module Shutdown"));
}

// ==================== Pipeline Tab ====================

void FHktMcpBridgeEditorModule::RegisterPipelineTab()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		PipelineTabName,
		FOnSpawnTab::CreateRaw(this, &FHktMcpBridgeEditorModule::SpawnPipelineTab))
		.SetDisplayName(LOCTEXT("PipelineTabTitle", "Pipeline Monitor"))
		.SetTooltipText(LOCTEXT("PipelineTabTooltip", "Track automation pipeline progress, tasks, and checkpoints"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
}

void FHktMcpBridgeEditorModule::UnregisterPipelineTab()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PipelineTabName);
}

TSharedRef<SDockTab> FHktMcpBridgeEditorModule::SpawnPipelineTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(NomadTab)
		.Label(LOCTEXT("PipelineTabLabel", "Pipeline Monitor"))
		[
			SNew(SHktPipelinePanel)
		];
}

// ==================== Console Commands ====================

void FHktMcpBridgeEditorModule::RegisterConsoleCommands()
{
	// MCP 상태 확인 커맨드
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("HktMcp.Status"),
		TEXT("Show MCP Bridge status"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			UHktMcpEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UHktMcpEditorSubsystem>();
			if (Subsystem)
			{
				UE_LOG(LogHktMcpEditor, Log, TEXT("MCP Editor Subsystem is active"));
			}
			else
			{
				UE_LOG(LogHktMcpEditor, Warning, TEXT("MCP Editor Subsystem not found"));
			}
		}),
		ECVF_Default
	));

	// 에셋 목록 조회 커맨드
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("HktMcp.ListAssets"),
		TEXT("List assets in specified path. Usage: HktMcp.ListAssets /Game/Path"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			UHktMcpEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UHktMcpEditorSubsystem>();
			if (Subsystem && Args.Num() > 0)
			{
				TArray<FHktAssetInfo> Assets = Subsystem->ListAssets(Args[0], TEXT(""));
				UE_LOG(LogHktMcpEditor, Log, TEXT("Found %d assets in %s"), Assets.Num(), *Args[0]);
				for (const FHktAssetInfo& Asset : Assets)
				{
					UE_LOG(LogHktMcpEditor, Log, TEXT("  - %s (%s)"), *Asset.AssetName, *Asset.AssetClass);
				}
			}
		}),
		ECVF_Default
	));

	// 파이프라인 모니터 탭 열기
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("HktMcp.Pipeline"),
		TEXT("Open Pipeline Monitor tab"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FHktMcpBridgeEditorModule::PipelineTabName);
		}),
		ECVF_Default
	));
}

void FHktMcpBridgeEditorModule::UnregisterConsoleCommands()
{
	for (IConsoleObject* Command : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Command);
	}
	ConsoleCommands.Empty();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHktMcpBridgeEditorModule, HktMcpBridgeEditor)
