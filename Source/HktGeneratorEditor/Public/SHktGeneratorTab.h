// Copyright Hkt Studios, Inc. All Rights Reserved.
// 개별 Generator 탭 위젯 — Intent 편집, 진행상황, 결과, 피드백 UI

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Containers/Ticker.h"
#include "HktGeneratorPromptTypes.h"

class FHktClaudeProcess;
class SMultiLineEditableTextBox;
class SScrollBox;
class SVerticalBox;

/**
 * SHktGeneratorTab
 *
 * 하나의 Generator 탭. 모든 Generator 종류에 동일한 레이아웃을 사용.
 * - Intent Editor: JSON 텍스트 에디터 + Load from Step
 * - Progress: Phase 리스트 + 실시간 CLI 로그
 * - Results: 생성 에셋 경로 목록
 * - Feedback: Accept / Reject / Refine + 피드백 텍스트
 */
class SHktGeneratorTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHktGeneratorTab)
		: _GeneratorInfo()
		, _StepsDataPath()
		, _ProjectId()
	{}
		SLATE_ARGUMENT(FHktGeneratorTypeInfo, GeneratorInfo)
		SLATE_ARGUMENT(FString, StepsDataPath)
		SLATE_ARGUMENT(FString, ProjectId)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 프로젝트 변경 시 호출 */
	void SetProject(const FString& InStepsDataPath, const FString& InProjectId);

private:
	// ── Data ──
	FHktGeneratorTypeInfo GeneratorInfo;
	FString StepsDataPath;
	FString ProjectId;

	TSharedPtr<FHktClaudeProcess> ClaudeProcess;
	TArray<TSharedPtr<FHktGenerationPhase>> Phases;
	TArray<FString> LogLines;
	TArray<FString> GeneratedAssets;

	FString PreviousResult;   // Refine 시 이전 결과 보관
	int32 IterationCount = 0; // Refine 반복 횟수

	// ── Widgets ──
	TSharedPtr<SMultiLineEditableTextBox> IntentEditor;
	TSharedPtr<SListView<TSharedPtr<FHktGenerationPhase>>> PhaseListView;
	TSharedPtr<SScrollBox> LogScrollBox;
	TSharedPtr<SListView<TSharedPtr<FString>>> ResultListView;
	TSharedPtr<SMultiLineEditableTextBox> FeedbackEditor;
	TSharedPtr<SVerticalBox> ProgressSection;
	TSharedPtr<SVerticalBox> ResultSection;
	TSharedPtr<SVerticalBox> FeedbackSection;

	TArray<TSharedPtr<FString>> ResultListItems;

	// ── UI Builders ──
	TSharedRef<SWidget> BuildIntentSection();
	TSharedRef<SWidget> BuildProgressSection();
	TSharedRef<SWidget> BuildResultSection();
	TSharedRef<SWidget> BuildFeedbackSection();

	TSharedRef<ITableRow> OnGeneratePhaseRow(
		TSharedPtr<FHktGenerationPhase> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	TSharedRef<ITableRow> OnGenerateResultRow(
		TSharedPtr<FString> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	// ── Actions ──
	void OnLoadFromStep();
	void OnGenerate();
	void OnCancel();
	void OnAccept();
	void OnReject();
	void OnRefine();

	// ── CLI Event Handling ──
	void OnClaudeOutput(const FString& JsonLine);
	void OnClaudeComplete(int32 ExitCode);
	void OnClaudeError(const FString& ErrorMsg);

	FHktClaudeEvent ParseClaudeEvent(const FString& JsonLine) const;
	void UpdatePhaseFromToolUse(const FString& ToolName);
	void ExtractGeneratedAssets(const FString& ResultJson);

	// ── Helpers ──
	FString LoadSkillMd() const;
	FString BuildPrompt(const FString& IntentJson, const FString& Feedback = TEXT("")) const;
	FString FindPluginDir() const;
	void AddLogLine(const FString& Line);
	void SetSectionVisibility(bool bShowProgress, bool bShowResult, bool bShowFeedback);

	/** Tick 델리게이트 핸들 */
	FTSTicker::FDelegateHandle TickHandle;
	bool OnTick(float DeltaTime);
};
