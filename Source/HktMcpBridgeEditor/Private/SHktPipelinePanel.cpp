#include "SHktPipelinePanel.h"
#include "HktMcpBridgeEditorModule.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "EditorAssetLibrary.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "HktPipelinePanel"

// ==================== Phase Display Names ====================

static const TMap<FString, FString> PhaseDisplayNames = {
	{TEXT("design"), TEXT("Design")},
	{TEXT("task_planning"), TEXT("Task Planning")},
	{TEXT("story_building"), TEXT("Story Building")},
	{TEXT("asset_discovery"), TEXT("Asset Discovery")},
	{TEXT("verification"), TEXT("Verification")},
};

static const TArray<FString> PhaseOrder = {
	TEXT("design"),
	TEXT("task_planning"),
	TEXT("story_building"),
	TEXT("asset_discovery"),
	TEXT("verification"),
};

// ==================== Status Colors ====================

static FSlateColor StatusToColor(const FString& Status)
{
	if (Status == TEXT("completed")) return FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f));
	if (Status == TEXT("in_progress")) return FSlateColor(FLinearColor(0.3f, 0.6f, 1.0f));
	if (Status == TEXT("failed")) return FSlateColor(FLinearColor(1.0f, 0.3f, 0.2f));
	if (Status == TEXT("blocked")) return FSlateColor(FLinearColor(1.0f, 0.6f, 0.0f));
	if (Status == TEXT("review")) return FSlateColor(FLinearColor(0.8f, 0.6f, 1.0f));
	return FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)); // pending
}

static FString StatusToSymbol(const FString& Status)
{
	if (Status == TEXT("completed")) return TEXT("[OK]");
	if (Status == TEXT("in_progress")) return TEXT("[..]");
	if (Status == TEXT("failed")) return TEXT("[X]");
	if (Status == TEXT("blocked")) return TEXT("[!]");
	if (Status == TEXT("review")) return TEXT("[?]");
	return TEXT("[ ]"); // pending
}

// ==================== Construct ====================

void SHktPipelinePanel::Construct(const FArguments& InArgs)
{
	PipelineDataPath = FindPipelineDataPath();

	ChildSlot
	[
		SNew(SVerticalBox)

		// Header
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PanelTitle", "Pipeline Monitor"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Refresh", "Refresh"))
				.OnClicked_Lambda([this]() { OnRefreshClicked(); return FReply::Handled(); })
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f)
		[
			SNew(SSeparator)
		]

		// Content area (rebuilt on refresh/selection)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(8.0f)
		[
			SAssignNew(ContentBox, SVerticalBox)
		]
	];

	// Initial load
	OnRefreshClicked();
}

// ==================== Pipeline Data Path ====================

FString SHktPipelinePanel::FindPipelineDataPath() const
{
	// Search for .pipeline_data directory in likely locations
	TArray<FString> SearchPaths;

	// Relative to plugin/project
	FString PluginDir = FPaths::GetPath(FPaths::GetPath(
		FModuleManager::Get().GetModuleFilename("HktMcpBridgeEditor")));

	// McpServer/.pipeline_data (most likely - same level as McpServer)
	FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	// Look for McpServer directory near the project
	SearchPaths.Add(FPaths::Combine(ProjectRoot, TEXT(".."), TEXT("McpServer"), TEXT(".pipeline_data")));
	SearchPaths.Add(FPaths::Combine(ProjectRoot, TEXT("McpServer"), TEXT(".pipeline_data")));
	SearchPaths.Add(FPaths::Combine(ProjectRoot, TEXT(".pipeline_data")));

	// Also check the plugin's own directory structure
	SearchPaths.Add(FPaths::Combine(ProjectRoot, TEXT("Plugins"), TEXT("HktGameplayGenerator"),
		TEXT("McpServer"), TEXT(".pipeline_data")));

	for (const FString& Path : SearchPaths)
	{
		FString Normalized = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeDirectoryName(Normalized);
		if (FPaths::DirectoryExists(Normalized))
		{
			UE_LOG(LogHktMcpEditor, Log, TEXT("Pipeline data found at: %s"), *Normalized);
			return Normalized;
		}
	}

	// Default fallback
	FString DefaultPath = FPaths::Combine(ProjectRoot, TEXT(".pipeline_data"));
	UE_LOG(LogHktMcpEditor, Warning, TEXT("Pipeline data not found, using default: %s"), *DefaultPath);
	return DefaultPath;
}

// ==================== Pipeline Discovery & Loading ====================

void SHktPipelinePanel::DiscoverPipelines()
{
	PipelineIds.Empty();

	FString PipelinesDir = FPaths::Combine(PipelineDataPath, TEXT("pipelines"));
	if (!FPaths::DirectoryExists(PipelinesDir))
	{
		return;
	}

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(PipelinesDir, TEXT("*.json")), true, false);

	for (const FString& File : Files)
	{
		if (!File.EndsWith(TEXT(".bak")))
		{
			FString Id = FPaths::GetBaseFilename(File);
			PipelineIds.Add(Id);
		}
	}

	PipelineIds.Sort();
}

bool SHktPipelinePanel::LoadPipeline(const FString& PipelineId)
{
	FString FilePath = FPaths::Combine(PipelineDataPath, TEXT("pipelines"),
		PipelineId + TEXT(".json"));

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("Failed to load pipeline file: %s"), *FilePath);
		return false;
	}

	return ParsePipelineJson(JsonString);
}

bool SHktPipelinePanel::ParsePipelineJson(const FString& JsonString)
{
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	CurrentPipeline = FPipelineSummary();
	CurrentPipeline.Id = Root->GetStringField(TEXT("id"));
	CurrentPipeline.Name = Root->GetStringField(TEXT("name"));
	CurrentPipeline.Description = Root->GetStringField(TEXT("description"));
	CurrentPipeline.CurrentPhase = Root->GetStringField(TEXT("current_phase"));

	// Parse phases
	const TSharedPtr<FJsonObject>* PhasesObj = nullptr;
	if (Root->TryGetObjectField(TEXT("phases"), PhasesObj))
	{
		for (const FString& PhaseName : PhaseOrder)
		{
			const TSharedPtr<FJsonObject>* PhaseObj = nullptr;
			if ((*PhasesObj)->TryGetObjectField(PhaseName, PhaseObj))
			{
				FPipelinePhaseEntry Entry;
				Entry.Phase = PhaseName;
				Entry.Status = (*PhaseObj)->GetStringField(TEXT("status"));
				CurrentPipeline.Phases.Add(Entry);
			}
		}
	}

	// Parse tasks
	const TArray<TSharedPtr<FJsonValue>>* TasksArray = nullptr;
	if (Root->TryGetArrayField(TEXT("tasks"), TasksArray))
	{
		for (const TSharedPtr<FJsonValue>& TaskVal : *TasksArray)
		{
			const TSharedPtr<FJsonObject>& TaskObj = TaskVal->AsObject();
			if (!TaskObj.IsValid()) continue;

			FPipelineTaskEntry Task;
			Task.Id = TaskObj->GetStringField(TEXT("id"));
			Task.Title = TaskObj->GetStringField(TEXT("title"));
			Task.Category = TaskObj->GetStringField(TEXT("category"));
			Task.Status = TaskObj->GetStringField(TEXT("status"));
			Task.Phase = TaskObj->GetStringField(TEXT("phase"));
			TaskObj->TryGetStringField(TEXT("mcp_tool_hint"), Task.McpToolHint);
			TaskObj->TryGetStringField(TEXT("error"), Task.Error);

			// Parse tags
			const TArray<TSharedPtr<FJsonValue>>* TagsArray = nullptr;
			if (TaskObj->TryGetArrayField(TEXT("tags"), TagsArray))
			{
				for (const auto& TagVal : *TagsArray)
				{
					Task.Tags.Add(TagVal->AsString());
				}
			}

			// Extract asset path from result if present
			const TSharedPtr<FJsonObject>* ResultObj = nullptr;
			if (TaskObj->TryGetObjectField(TEXT("result"), ResultObj) && ResultObj)
			{
				// Look for common asset path fields
				FString AssetPath;
				if ((*ResultObj)->TryGetStringField(TEXT("asset_path"), AssetPath) ||
					(*ResultObj)->TryGetStringField(TEXT("assetPath"), AssetPath) ||
					(*ResultObj)->TryGetStringField(TEXT("path"), AssetPath))
				{
					Task.ResultAssetPath = AssetPath;
				}
			}

			CurrentPipeline.Tasks.Add(Task);
		}
	}

	// Count tasks per phase
	for (FPipelinePhaseEntry& PhaseEntry : CurrentPipeline.Phases)
	{
		for (const FPipelineTaskEntry& Task : CurrentPipeline.Tasks)
		{
			if (Task.Phase == PhaseEntry.Phase)
			{
				PhaseEntry.TotalTasks++;
				if (Task.Status == TEXT("completed"))
				{
					PhaseEntry.CompletedTasks++;
				}
			}
		}
	}

	// Parse checkpoints
	const TArray<TSharedPtr<FJsonValue>>* CpArray = nullptr;
	if (Root->TryGetArrayField(TEXT("checkpoints"), CpArray))
	{
		for (const TSharedPtr<FJsonValue>& CpVal : *CpArray)
		{
			const TSharedPtr<FJsonObject>& CpObj = CpVal->AsObject();
			if (!CpObj.IsValid()) continue;

			FPipelineCheckpointEntry Cp;
			Cp.Id = CpObj->GetStringField(TEXT("id"));
			Cp.Phase = CpObj->GetStringField(TEXT("phase"));
			Cp.Title = CpObj->GetStringField(TEXT("title"));

			FString Action;
			Cp.bPending = !CpObj->TryGetStringField(TEXT("action"), Action) || Action.IsEmpty();

			CurrentPipeline.Checkpoints.Add(Cp);
		}
	}

	return true;
}

// ==================== UI Building ====================

void SHktPipelinePanel::RebuildContent()
{
	ContentBox->ClearChildren();

	if (PipelineIds.Num() == 0)
	{
		ContentBox->AddSlot()
			.AutoHeight()
			.Padding(0, 20)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoPipelines", "No pipelines found.\n\nUse pipeline_create via MCP to start a new pipeline."))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			];
		return;
	}

	// Pipeline selector
	ContentBox->AddSlot()
		.AutoHeight()
		.Padding(0, 0, 0, 8)
		[
			BuildPipelineSelector()
		];

	if (SelectedPipelineId.IsEmpty())
	{
		return;
	}

	// Pipeline name & description
	ContentBox->AddSlot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(FText::FromString(CurrentPipeline.Name))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
		];

	if (!CurrentPipeline.Description.IsEmpty())
	{
		ContentBox->AddSlot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				SNew(STextBlock)
				.Text(FText::FromString(CurrentPipeline.Description))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
			];
	}

	// Checkpoint alert (if pending)
	ContentBox->AddSlot()
		.AutoHeight()
		[
			BuildCheckpointAlert()
		];

	// Phase progress
	ContentBox->AddSlot()
		.AutoHeight()
		.Padding(0, 0, 0, 12)
		[
			BuildPhaseProgress()
		];

	ContentBox->AddSlot()
		.AutoHeight()
		[
			SNew(SSeparator)
		];

	// Current phase label
	FString PhaseDisplay = CurrentPipeline.CurrentPhase;
	if (const FString* Display = PhaseDisplayNames.Find(CurrentPipeline.CurrentPhase))
	{
		PhaseDisplay = *Display;
	}

	ContentBox->AddSlot()
		.AutoHeight()
		.Padding(0, 8, 0, 4)
		[
			SNew(STextBlock)
			.Text(FText::Format(
				LOCTEXT("CurrentPhaseFmt", "Tasks - {0}"),
				FText::FromString(PhaseDisplay)))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		];

	// Task list
	ContentBox->AddSlot()
		.FillHeight(1.0f)
		[
			BuildTaskList()
		];
}

TSharedRef<SWidget> SHktPipelinePanel::BuildPipelineSelector()
{
	// Simple text buttons for each pipeline
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);

	Box->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 8, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Pipeline", "Pipeline:"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		];

	for (const FString& Id : PipelineIds)
	{
		bool bSelected = (Id == SelectedPipelineId);
		Box->AddSlot()
			.AutoWidth()
			.Padding(2, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(Id))
				.OnClicked_Lambda([this, Id]() {
					OnPipelineSelected(Id);
					return FReply::Handled();
				})
				.ButtonColorAndOpacity(bSelected
					? FLinearColor(0.2f, 0.4f, 0.8f, 1.0f)
					: FLinearColor(0.15f, 0.15f, 0.15f, 1.0f))
			];
	}

	return Box;
}

TSharedRef<SWidget> SHktPipelinePanel::BuildPhaseProgress()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	for (const FPipelinePhaseEntry& Phase : CurrentPipeline.Phases)
	{
		FString DisplayName = Phase.Phase;
		if (const FString* Name = PhaseDisplayNames.Find(Phase.Phase))
		{
			DisplayName = *Name;
		}

		bool bIsCurrent = (Phase.Phase == CurrentPipeline.CurrentPhase);
		FLinearColor PhaseColor = StatusToColor(Phase.Status).GetSpecifiedColor();

		// Progress fraction
		float Progress = (Phase.TotalTasks > 0)
			? static_cast<float>(Phase.CompletedTasks) / Phase.TotalTasks
			: (Phase.Status == TEXT("completed") ? 1.0f : 0.0f);

		FString TaskCountStr = (Phase.TotalTasks > 0)
			? FString::Printf(TEXT("%d/%d"), Phase.CompletedTasks, Phase.TotalTasks)
			: TEXT("-");

		Box->AddSlot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SHorizontalBox)

				// Current phase indicator
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 4, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(bIsCurrent ? TEXT(">") : TEXT(" ")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.8f, 1.0f)))
				]

				// Phase name
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(SBox)
					.WidthOverride(120.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(DisplayName))
						.Font(bIsCurrent
							? FCoreStyle::GetDefaultFontStyle("Bold", 10)
							: FCoreStyle::GetDefaultFontStyle("Regular", 10))
					]
				]

				// Progress bar
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(SBox)
					.HeightOverride(14.0f)
					[
						SNew(SProgressBar)
						.Percent(Progress)
						.FillColorAndOpacity(PhaseColor)
					]
				]

				// Task count
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TaskCountStr))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
				]
			];
	}

	return Box;
}

TSharedRef<SWidget> SHktPipelinePanel::BuildCheckpointAlert()
{
	// Find pending checkpoints
	bool bHasPending = false;
	FString PendingTitle;
	for (const FPipelineCheckpointEntry& Cp : CurrentPipeline.Checkpoints)
	{
		if (Cp.bPending)
		{
			bHasPending = true;
			PendingTitle = Cp.Title;
			break;
		}
	}

	if (!bHasPending)
	{
		return SNew(SBox); // empty
	}

	return SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(0.8f, 0.6f, 0.0f, 0.3f))
		.Padding(8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CheckpointIcon", "[!]"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.8f, 0.0f)))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CheckpointPending", "Checkpoint Pending Review"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.8f, 0.0f)))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::Format(
						LOCTEXT("CheckpointDetailFmt", "{0} - Reply approve/revise/reject in the conversation"),
						FText::FromString(PendingTitle)))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)))
				]
			]
		];
}

TSharedRef<SWidget> SHktPipelinePanel::BuildTaskList()
{
	// Build filtered list items (current phase tasks)
	TaskListItems.Empty();
	for (FPipelineTaskEntry& Task : CurrentPipeline.Tasks)
	{
		if (Task.Phase == CurrentPipeline.CurrentPhase)
		{
			TaskListItems.Add(MakeShared<FPipelineTaskEntry>(Task));
		}
	}

	// Sort: in_progress first, then pending, then completed, then failed
	TaskListItems.Sort([](const TSharedPtr<FPipelineTaskEntry>& A, const TSharedPtr<FPipelineTaskEntry>& B)
	{
		auto Priority = [](const FString& S) -> int32
		{
			if (S == TEXT("in_progress")) return 0;
			if (S == TEXT("blocked")) return 1;
			if (S == TEXT("review")) return 2;
			if (S == TEXT("pending")) return 3;
			if (S == TEXT("failed")) return 4;
			if (S == TEXT("completed")) return 5;
			return 6;
		};
		return Priority(A->Status) < Priority(B->Status);
	});

	return SAssignNew(TaskListView, SListView<TSharedPtr<FPipelineTaskEntry>>)
		.ListItemsSource(&TaskListItems)
		.OnGenerateRow(this, &SHktPipelinePanel::OnGenerateTaskRow)
		.SelectionMode(ESelectionMode::None);
}

TSharedRef<ITableRow> SHktPipelinePanel::OnGenerateTaskRow(
	TSharedPtr<FPipelineTaskEntry> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	FString StatusSym = StatusToSymbol(Item->Status);
	FSlateColor Color = StatusToColor(Item->Status);

	// Tags display
	FString TagsStr;
	for (const FString& Tag : Item->Tags)
	{
		if (!TagsStr.IsEmpty()) TagsStr += TEXT(", ");
		TagsStr += Tag;
	}

	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		// Status symbol
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 6, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(StatusSym))
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
			.ColorAndOpacity(Color)
		]

		// Title + category
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Title))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(
					FString::Printf(TEXT("%s%s%s"),
						*Item->Category,
						Item->McpToolHint.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" | %s"), *Item->McpToolHint),
						TagsStr.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" | %s"), *TagsStr))))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			]
		]

		// Status text
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(8, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->Status))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.ColorAndOpacity(Color)
		];

	// Browse asset button (only if we have an asset path)
	if (!Item->ResultAssetPath.IsEmpty())
	{
		FString AssetPath = Item->ResultAssetPath;
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(LOCTEXT("Browse", "Browse"))
				.ToolTipText(FText::FromString(AssetPath))
				.OnClicked_Lambda([this, AssetPath]() {
					OnBrowseAsset(AssetPath);
					return FReply::Handled();
				})
			];
	}

	// Error indicator
	if (!Item->Error.IsEmpty())
	{
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4, 0, 0, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ErrorIcon", "[ERR]"))
				.ToolTipText(FText::FromString(Item->Error))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.3f, 0.2f)))
			];
	}

	return SNew(STableRow<TSharedPtr<FPipelineTaskEntry>>, OwnerTable)
		.Padding(FMargin(4, 3))
		[
			Row
		];
}

// ==================== Actions ====================

void SHktPipelinePanel::OnRefreshClicked()
{
	PipelineDataPath = FindPipelineDataPath();
	DiscoverPipelines();

	if (!SelectedPipelineId.IsEmpty() && PipelineIds.Contains(SelectedPipelineId))
	{
		LoadPipeline(SelectedPipelineId);
	}
	else if (PipelineIds.Num() > 0)
	{
		SelectedPipelineId = PipelineIds.Last();
		LoadPipeline(SelectedPipelineId);
	}
	else
	{
		SelectedPipelineId.Empty();
		CurrentPipeline = FPipelineSummary();
	}

	RebuildContent();
}

void SHktPipelinePanel::OnPipelineSelected(const FString& PipelineId)
{
	SelectedPipelineId = PipelineId;
	if (LoadPipeline(PipelineId))
	{
		RebuildContent();
	}
}

void SHktPipelinePanel::OnBrowseAsset(const FString& AssetPath)
{
	// Try to sync Content Browser to the asset
	if (UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath))
	{
		FContentBrowserModule& ContentBrowserModule =
			FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<UObject*> Assets;
		Assets.Add(Asset);
		ContentBrowserModule.Get().SyncBrowserToAssets(Assets);
	}
	else
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("Asset not found: %s"), *AssetPath);
	}
}

FSlateColor SHktPipelinePanel::GetStatusColor(const FString& Status) const
{
	return StatusToColor(Status);
}

FText SHktPipelinePanel::GetPhaseDisplayName(const FString& Phase) const
{
	if (const FString* Name = PhaseDisplayNames.Find(Phase))
	{
		return FText::FromString(*Name);
	}
	return FText::FromString(Phase);
}

#undef LOCTEXT_NAMESPACE
