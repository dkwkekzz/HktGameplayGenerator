#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

/**
 * Step Viewer Panel - Dockable Slate tab for tracking modular step progress.
 *
 * Reads step data from .hkt_steps/ directory and displays:
 * - Project selector
 * - Step progress overview (7 independent steps)
 * - Input/output JSON preview per step
 * - Asset browse buttons for completed generation steps
 *
 * Each step is independent and can be run by a different agent:
 *   concept_design → map_generation / story_generation → asset_discovery
 *   → character_generation / item_generation / vfx_generation
 */

// Lightweight struct for step UI display (parsed from manifest JSON)
struct FStepEntry
{
	FString StepType;
	FString DisplayName;
	FString Status;
	FString AgentId;
	FString StartedAt;
	FString CompletedAt;
	FString Error;
	FString OutputFilePath;
	FString InputFilePath;
	bool bHasOutput = false;
	bool bHasInput = false;
};

struct FProjectSummary
{
	FString ProjectId;
	FString ProjectName;
	FString Concept;
	FString CreatedAt;
	FString UpdatedAt;
	TArray<FStepEntry> Steps;
	int32 CompletedCount = 0;
	int32 TotalCount = 0;
};

class SHktPipelinePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHktPipelinePanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// Data
	FString StepsDataPath;
	TArray<FString> ProjectIds;
	FString SelectedProjectId;
	FProjectSummary CurrentProject;
	TArray<TSharedPtr<FStepEntry>> StepListItems;

	// Widgets
	TSharedPtr<SListView<TSharedPtr<FStepEntry>>> StepListView;

	// Project file discovery & loading
	void DiscoverProjects();
	bool LoadProject(const FString& ProjectId);
	bool ParseManifestJson(const FString& JsonString);
	FString FindStepsDataPath() const;

	// UI builders
	TSharedRef<SWidget> BuildProjectSelector();
	TSharedRef<SWidget> BuildStepOverview();
	TSharedRef<SWidget> BuildStepList();

	// Step list view
	TSharedRef<ITableRow> OnGenerateStepRow(
		TSharedPtr<FStepEntry> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	// Actions
	void OnRefreshClicked();
	void OnProjectSelected(const FString& ProjectId);
	void OnBrowseStepOutput(const FString& FilePath);

	// Helpers
	FSlateColor GetStatusColor(const FString& Status) const;
	FText GetStepDisplayName(const FString& StepType) const;

	// Rebuild entire panel content
	TSharedPtr<SVerticalBox> ContentBox;
	void RebuildContent();
};
