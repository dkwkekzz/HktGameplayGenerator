#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

/**
 * Pipeline Monitor Panel - Dockable Slate tab for tracking pipeline progress.
 *
 * Reads pipeline JSON files from .pipeline_data/ directory and displays:
 * - Pipeline selector
 * - Phase progress indicators
 * - Task list with status colors
 * - "Browse Asset" buttons for completed tasks with asset results
 * - Pending checkpoint alerts
 */

// Lightweight structs for UI display (parsed from JSON)
struct FPipelineTaskEntry
{
	FString Id;
	FString Title;
	FString Category;
	FString Status;
	FString Phase;
	FString McpToolHint;
	FString Error;
	TArray<FString> Tags;
	// Result asset path (extracted from result JSON if present)
	FString ResultAssetPath;
};

struct FPipelinePhaseEntry
{
	FString Phase;
	FString Status;
	int32 CompletedTasks = 0;
	int32 TotalTasks = 0;
};

struct FPipelineCheckpointEntry
{
	FString Id;
	FString Phase;
	FString Title;
	bool bPending = false;
};

struct FPipelineSummary
{
	FString Id;
	FString Name;
	FString Description;
	FString CurrentPhase;
	TArray<FPipelinePhaseEntry> Phases;
	TArray<FPipelineTaskEntry> Tasks;
	TArray<FPipelineCheckpointEntry> Checkpoints;
};

class SHktPipelinePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHktPipelinePanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// Data
	FString PipelineDataPath;
	TArray<FString> PipelineIds;
	FString SelectedPipelineId;
	FPipelineSummary CurrentPipeline;
	TArray<TSharedPtr<FPipelineTaskEntry>> TaskListItems;

	// Widgets
	TSharedPtr<SListView<TSharedPtr<FPipelineTaskEntry>>> TaskListView;

	// Pipeline file discovery & loading
	void DiscoverPipelines();
	bool LoadPipeline(const FString& PipelineId);
	bool ParsePipelineJson(const FString& JsonString);
	FString FindPipelineDataPath() const;

	// UI builders
	TSharedRef<SWidget> BuildPipelineSelector();
	TSharedRef<SWidget> BuildPhaseProgress();
	TSharedRef<SWidget> BuildCheckpointAlert();
	TSharedRef<SWidget> BuildTaskList();

	// Task list view
	TSharedRef<ITableRow> OnGenerateTaskRow(
		TSharedPtr<FPipelineTaskEntry> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	// Actions
	void OnRefreshClicked();
	void OnPipelineSelected(const FString& PipelineId);
	void OnBrowseAsset(const FString& AssetPath);

	// Helpers
	FSlateColor GetStatusColor(const FString& Status) const;
	FText GetPhaseDisplayName(const FString& Phase) const;

	// Rebuild entire panel content
	TSharedPtr<SVerticalBox> ContentBox;
	void RebuildContent();
};
