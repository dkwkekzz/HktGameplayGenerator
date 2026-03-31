// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktGeneratorEditorModule.h"
#include "SHktGeneratorPromptPanel.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

DEFINE_LOG_CATEGORY(LogHktGenEditor);

const FName FHktGeneratorEditorModule::GeneratorPromptTabName(TEXT("HktGeneratorPrompt"));

#define LOCTEXT_NAMESPACE "FHktGeneratorEditorModule"

void FHktGeneratorEditorModule::StartupModule()
{
	RegisterGeneratorPromptTab();
	RegisterConsoleCommands();
	UE_LOG(LogHktGenEditor, Log, TEXT("HktGeneratorEditor Module Started"));
}

void FHktGeneratorEditorModule::ShutdownModule()
{
	UnregisterConsoleCommands();
	UnregisterGeneratorPromptTab();
	UE_LOG(LogHktGenEditor, Log, TEXT("HktGeneratorEditor Module Shutdown"));
}

// ==================== Generator Prompt Tab ====================

void FHktGeneratorEditorModule::RegisterGeneratorPromptTab()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		GeneratorPromptTabName,
		FOnSpawnTab::CreateRaw(this, &FHktGeneratorEditorModule::SpawnGeneratorPromptTab))
		.SetDisplayName(LOCTEXT("GenPromptTitle", "Generator Prompt"))
		.SetTooltipText(LOCTEXT("GenPromptTooltip", "AI-powered asset generation with feedback loop"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FHktGeneratorEditorModule::UnregisterGeneratorPromptTab()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GeneratorPromptTabName);
}

TSharedRef<SDockTab> FHktGeneratorEditorModule::SpawnGeneratorPromptTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(NomadTab)
		.Label(LOCTEXT("GenPromptLabel", "Generator Prompt"))
		[
			SNew(SHktGeneratorPromptPanel)
		];
}

// ==================== Console Commands ====================

void FHktGeneratorEditorModule::RegisterConsoleCommands()
{
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("HktGen.Prompt"),
		TEXT("Open Generator Prompt panel"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FHktGeneratorEditorModule::GeneratorPromptTabName);
		}),
		ECVF_Default
	));
}

void FHktGeneratorEditorModule::UnregisterConsoleCommands()
{
	for (IConsoleObject* Command : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Command);
	}
	ConsoleCommands.Empty();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHktGeneratorEditorModule, HktGeneratorEditor)
