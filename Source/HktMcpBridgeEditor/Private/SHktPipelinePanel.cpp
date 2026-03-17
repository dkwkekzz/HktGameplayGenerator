#include "SHktPipelinePanel.h"
#include "HktMcpBridgeEditorModule.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/AppStyle.h"
#include "DesktopPlatformModule.h"

#define LOCTEXT_NAMESPACE "HktStepViewer"

// ==================== Step Display Names ====================

static const TMap<FString, FString> StepDisplayNames = {
	{TEXT("concept_design"),        TEXT("Concept Design")},
	{TEXT("map_generation"),        TEXT("Map Generation")},
	{TEXT("story_generation"),      TEXT("Story Generation")},
	{TEXT("asset_discovery"),       TEXT("Asset Discovery")},
	{TEXT("character_generation"),  TEXT("Character Generation")},
	{TEXT("item_generation"),       TEXT("Item Generation")},
	{TEXT("vfx_generation"),        TEXT("VFX Generation")},
};

static const TArray<FString> StepOrder = {
	TEXT("concept_design"),
	TEXT("map_generation"),
	TEXT("story_generation"),
	TEXT("asset_discovery"),
	TEXT("character_generation"),
	TEXT("item_generation"),
	TEXT("vfx_generation"),
};

// ==================== Status Colors ====================

static FSlateColor StatusToColor(const FString& Status)
{
	if (Status == TEXT("completed"))   return FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f));
	if (Status == TEXT("in_progress")) return FSlateColor(FLinearColor(0.3f, 0.6f, 1.0f));
	if (Status == TEXT("failed"))      return FSlateColor(FLinearColor(1.0f, 0.3f, 0.2f));
	return FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)); // not_started
}

static FString StatusToSymbol(const FString& Status)
{
	if (Status == TEXT("completed"))   return TEXT("[OK]");
	if (Status == TEXT("in_progress")) return TEXT("[..]");
	if (Status == TEXT("failed"))      return TEXT("[X]");
	return TEXT("[ ]"); // not_started
}

// ==================== Construct ====================

void SHktPipelinePanel::Construct(const FArguments& InArgs)
{
	StepsDataPath = FindStepsDataPath();

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
				.Text(LOCTEXT("PanelTitle", "Step Viewer"))
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

// ==================== Steps Data Path ====================

FString SHktPipelinePanel::FindStepsDataPath() const
{
	TArray<FString> SearchPaths;
	FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	SearchPaths.Add(FPaths::Combine(ProjectRoot, TEXT(".."), TEXT("McpServer"), TEXT(".hkt_steps")));
	SearchPaths.Add(FPaths::Combine(ProjectRoot, TEXT("McpServer"), TEXT(".hkt_steps")));
	SearchPaths.Add(FPaths::Combine(ProjectRoot, TEXT(".hkt_steps")));
	SearchPaths.Add(FPaths::Combine(ProjectRoot, TEXT("Plugins"), TEXT("HktGameplayGenerator"),
		TEXT("McpServer"), TEXT(".hkt_steps")));

	for (const FString& Path : SearchPaths)
	{
		FString Normalized = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeDirectoryName(Normalized);
		if (FPaths::DirectoryExists(Normalized))
		{
			UE_LOG(LogHktMcpEditor, Log, TEXT("Steps data found at: %s"), *Normalized);
			return Normalized;
		}
	}

	FString DefaultPath = FPaths::Combine(ProjectRoot, TEXT(".hkt_steps"));
	UE_LOG(LogHktMcpEditor, Warning, TEXT("Steps data not found, using default: %s"), *DefaultPath);
	return DefaultPath;
}

// ==================== Project Discovery & Loading ====================

void SHktPipelinePanel::DiscoverProjects()
{
	ProjectIds.Empty();

	if (!FPaths::DirectoryExists(StepsDataPath))
	{
		return;
	}

	TArray<FString> Directories;
	IFileManager::Get().FindFiles(Directories, *FPaths::Combine(StepsDataPath, TEXT("*")), false, true);

	for (const FString& Dir : Directories)
	{
		FString ManifestPath = FPaths::Combine(StepsDataPath, Dir, TEXT("manifest.json"));
		if (FPaths::FileExists(ManifestPath))
		{
			ProjectIds.Add(Dir);
		}
	}

	ProjectIds.Sort();
}

bool SHktPipelinePanel::LoadProject(const FString& ProjectId)
{
	FString FilePath = FPaths::Combine(StepsDataPath, ProjectId, TEXT("manifest.json"));

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("Failed to load manifest: %s"), *FilePath);
		return false;
	}

	return ParseManifestJson(JsonString);
}

bool SHktPipelinePanel::ParseManifestJson(const FString& JsonString)
{
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	CurrentProject = FProjectSummary();
	CurrentProject.ProjectId = Root->GetStringField(TEXT("project_id"));
	CurrentProject.ProjectName = Root->GetStringField(TEXT("project_name"));
	Root->TryGetStringField(TEXT("concept"), CurrentProject.Concept);
	Root->TryGetStringField(TEXT("created_at"), CurrentProject.CreatedAt);
	Root->TryGetStringField(TEXT("updated_at"), CurrentProject.UpdatedAt);

	// Parse steps
	const TSharedPtr<FJsonObject>* StepsObj = nullptr;
	if (Root->TryGetObjectField(TEXT("steps"), StepsObj))
	{
		CurrentProject.TotalCount = 0;
		CurrentProject.CompletedCount = 0;

		for (const FString& StepType : StepOrder)
		{
			const TSharedPtr<FJsonObject>* StepObj = nullptr;
			if ((*StepsObj)->TryGetObjectField(StepType, StepObj))
			{
				FStepEntry Entry;
				Entry.StepType = StepType;

				if (const FString* Name = StepDisplayNames.Find(StepType))
				{
					Entry.DisplayName = *Name;
				}
				else
				{
					Entry.DisplayName = StepType;
				}

				(*StepObj)->TryGetStringField(TEXT("status"), Entry.Status);
				(*StepObj)->TryGetStringField(TEXT("agent_id"), Entry.AgentId);
				(*StepObj)->TryGetStringField(TEXT("started_at"), Entry.StartedAt);
				(*StepObj)->TryGetStringField(TEXT("completed_at"), Entry.CompletedAt);
				(*StepObj)->TryGetStringField(TEXT("error"), Entry.Error);

				// Resolve file paths
				FString StepDir = FPaths::Combine(StepsDataPath, SelectedProjectId, StepType);
				Entry.OutputFilePath = FPaths::Combine(StepDir, TEXT("output.json"));
				Entry.InputFilePath = FPaths::Combine(StepDir, TEXT("input.json"));
				Entry.bHasOutput = FPaths::FileExists(Entry.OutputFilePath);
				Entry.bHasInput = FPaths::FileExists(Entry.InputFilePath);

				CurrentProject.Steps.Add(Entry);
				CurrentProject.TotalCount++;
				if (Entry.Status == TEXT("completed"))
				{
					CurrentProject.CompletedCount++;
				}
			}
		}
	}

	return true;
}

// ==================== UI Building ====================

void SHktPipelinePanel::RebuildContent()
{
	ContentBox->ClearChildren();

	if (ProjectIds.Num() == 0)
	{
		ContentBox->AddSlot()
			.AutoHeight()
			.Padding(0, 20)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoProjects", "No projects found.\n\nUse step_create_project via MCP to start a new project."))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			];
		return;
	}

	// Project selector
	ContentBox->AddSlot()
		.AutoHeight()
		.Padding(0, 0, 0, 8)
		[
			BuildProjectSelector()
		];

	if (SelectedProjectId.IsEmpty())
	{
		return;
	}

	// Project name & concept
	ContentBox->AddSlot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(FText::FromString(CurrentProject.ProjectName))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
		];

	if (!CurrentProject.Concept.IsEmpty())
	{
		FString TruncatedConcept = CurrentProject.Concept;
		if (TruncatedConcept.Len() > 200)
		{
			TruncatedConcept = TruncatedConcept.Left(200) + TEXT("...");
		}
		ContentBox->AddSlot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TruncatedConcept))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
				.AutoWrapText(true)
			];
	}

	// Step overview (progress bar)
	ContentBox->AddSlot()
		.AutoHeight()
		.Padding(0, 0, 0, 12)
		[
			BuildStepOverview()
		];

	ContentBox->AddSlot()
		.AutoHeight()
		[
			SNew(SSeparator)
		];

	// Step list header
	ContentBox->AddSlot()
		.AutoHeight()
		.Padding(0, 8, 0, 4)
		[
			SNew(STextBlock)
			.Text(FText::Format(
				LOCTEXT("StepListHeader", "Steps ({0}/{1} completed)"),
				FText::AsNumber(CurrentProject.CompletedCount),
				FText::AsNumber(CurrentProject.TotalCount)))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		];

	// Step list
	ContentBox->AddSlot()
		.FillHeight(1.0f)
		[
			BuildStepList()
		];
}

TSharedRef<SWidget> SHktPipelinePanel::BuildProjectSelector()
{
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);

	Box->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 8, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Project", "Project:"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		];

	for (const FString& Id : ProjectIds)
	{
		bool bSelected = (Id == SelectedProjectId);
		Box->AddSlot()
			.AutoWidth()
			.Padding(2, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(Id))
				.OnClicked_Lambda([this, Id]() {
					OnProjectSelected(Id);
					return FReply::Handled();
				})
				.ButtonColorAndOpacity(bSelected
					? FLinearColor(0.2f, 0.4f, 0.8f, 1.0f)
					: FLinearColor(0.15f, 0.15f, 0.15f, 1.0f))
			];
	}

	return Box;
}

TSharedRef<SWidget> SHktPipelinePanel::BuildStepOverview()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	float OverallProgress = (CurrentProject.TotalCount > 0)
		? static_cast<float>(CurrentProject.CompletedCount) / CurrentProject.TotalCount
		: 0.0f;

	// Overall progress bar
	Box->AddSlot()
		.AutoHeight()
		.Padding(0, 0, 0, 8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.HeightOverride(16.0f)
				[
					SNew(SProgressBar)
					.Percent(OverallProgress)
					.FillColorAndOpacity(FLinearColor(0.2f, 0.6f, 0.9f))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8, 0, 0, 0)
			[
				SNew(STextBlock)
				.Text(FText::Format(
					LOCTEXT("ProgressFmt", "{0}%"),
					FText::AsNumber(FMath::RoundToInt(OverallProgress * 100.f))))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]
		];

	// Mini step indicators (horizontal dots/boxes)
	TSharedRef<SHorizontalBox> StepDots = SNew(SHorizontalBox);
	for (const FStepEntry& Step : CurrentProject.Steps)
	{
		FSlateColor Color = StatusToColor(Step.Status);
		StepDots->AddSlot()
			.FillWidth(1.0f)
			.Padding(1, 0)
			[
				SNew(SBox)
				.HeightOverride(4.0f)
				[
					SNew(SBorder)
					.BorderBackgroundColor(Color.GetSpecifiedColor())
				]
			];
	}

	Box->AddSlot()
		.AutoHeight()
		[
			StepDots
		];

	return Box;
}

TSharedRef<SWidget> SHktPipelinePanel::BuildStepList()
{
	StepListItems.Empty();
	for (FStepEntry& Step : CurrentProject.Steps)
	{
		StepListItems.Add(MakeShared<FStepEntry>(Step));
	}

	return SAssignNew(StepListView, SListView<TSharedPtr<FStepEntry>>)
		.ListItemsSource(&StepListItems)
		.OnGenerateRow(this, &SHktPipelinePanel::OnGenerateStepRow)
		.SelectionMode(ESelectionMode::None);
}

TSharedRef<ITableRow> SHktPipelinePanel::OnGenerateStepRow(
	TSharedPtr<FStepEntry> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	FString StatusSym = StatusToSymbol(Item->Status);
	FSlateColor Color = StatusToColor(Item->Status);

	// Subtitle: agent + timing info
	FString SubLine;
	if (!Item->AgentId.IsEmpty())
	{
		SubLine += FString::Printf(TEXT("Agent: %s"), *Item->AgentId);
	}
	if (!Item->CompletedAt.IsEmpty())
	{
		if (!SubLine.IsEmpty()) SubLine += TEXT(" | ");
		SubLine += FString::Printf(TEXT("Done: %s"), *Item->CompletedAt.Left(19));
	}
	else if (!Item->StartedAt.IsEmpty())
	{
		if (!SubLine.IsEmpty()) SubLine += TEXT(" | ");
		SubLine += FString::Printf(TEXT("Started: %s"), *Item->StartedAt.Left(19));
	}

	// I/O indicators
	FString IoLine;
	if (Item->bHasInput) IoLine += TEXT("[IN]");
	if (Item->bHasOutput) IoLine += TEXT(" [OUT]");

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

		// Step name + subtitle
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->DisplayName))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(SubLine.IsEmpty() ? Item->StepType : SubLine))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			]
		]

		// I/O indicators
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(IoLine))
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.7f, 0.4f)))
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

	// Browse output button (only if output exists)
	if (Item->bHasOutput)
	{
		FString OutputPath = Item->OutputFilePath;
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(LOCTEXT("ViewOutput", "Output"))
				.ToolTipText(FText::FromString(OutputPath))
				.OnClicked_Lambda([this, OutputPath]() {
					OnBrowseStepOutput(OutputPath);
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

	return SNew(STableRow<TSharedPtr<FStepEntry>>, OwnerTable)
		.Padding(FMargin(4, 3))
		[
			Row
		];
}

// ==================== Actions ====================

void SHktPipelinePanel::OnRefreshClicked()
{
	StepsDataPath = FindStepsDataPath();
	DiscoverProjects();

	if (!SelectedProjectId.IsEmpty() && ProjectIds.Contains(SelectedProjectId))
	{
		LoadProject(SelectedProjectId);
	}
	else if (ProjectIds.Num() > 0)
	{
		SelectedProjectId = ProjectIds.Last();
		LoadProject(SelectedProjectId);
	}
	else
	{
		SelectedProjectId.Empty();
		CurrentProject = FProjectSummary();
	}

	RebuildContent();
}

void SHktPipelinePanel::OnProjectSelected(const FString& ProjectId)
{
	SelectedProjectId = ProjectId;
	if (LoadProject(ProjectId))
	{
		RebuildContent();
	}
}

void SHktPipelinePanel::OnBrowseStepOutput(const FString& FilePath)
{
	// Open the JSON file in the system's default editor
	FPlatformProcess::LaunchFileInDefaultExternalApplication(*FilePath);
}

FSlateColor SHktPipelinePanel::GetStatusColor(const FString& Status) const
{
	return StatusToColor(Status);
}

FText SHktPipelinePanel::GetStepDisplayName(const FString& StepType) const
{
	if (const FString* Name = StepDisplayNames.Find(StepType))
	{
		return FText::FromString(*Name);
	}
	return FText::FromString(StepType);
}

#undef LOCTEXT_NAMESPACE
