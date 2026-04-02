// Copyright Hkt Studios, Inc. All Rights Reserved.
// 개별 Generator 탭 위젯 — Intent 편집, 진행상황, 결과, 피드백 UI
// 외부 Claude CLI 에이전트와 MCP 서버를 통해 생성 요청/응답 처리

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "HktGeneratorPromptTypes.h"

struct FHktGenerationEvent;
class SMultiLineEditableTextBox;
class SScrollBox;
class SVerticalBox;

/**
 * SHktGeneratorTab
 *
 * 하나의 Generator 탭. 모든 Generator 종류에 동일한 레이아웃을 사용.
 * - Intent Editor: JSON 텍스트 에디터 + Load from Step
 * - Progress: Phase 리스트 + 실시간 로그
 * - Results: 생성 에셋 경로 목록
 * - Feedback: Accept / Reject / Refine + 피드백 텍스트
 *
 * 생성은 MCP를 통해 외부 Claude CLI 에이전트에 위임.
 * 사용자가 직접 CLI를 띄우고, 패널은 요청 큐잉 + 이벤트 수신만 담당.
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
	virtual ~SHktGeneratorTab();

	/** 프로젝트 변경 시 호출 */
	void SetProject(const FString& InStepsDataPath, const FString& InProjectId);

private:
	// ── Data ──
	FHktGeneratorTypeInfo GeneratorInfo;
	FString StepsDataPath;
	FString ProjectId;

	TArray<TSharedPtr<FHktGenerationPhase>> Phases;
	TArray<FString> LogLines;
	TArray<FString> GeneratedAssets;

	FString PreviousResult;     // Refine 시 이전 결과 보관
	int32 IterationCount = 0;   // Refine 반복 횟수

	// ── MCP Generation ──
	FString CurrentRequestId;   // 현재 활성 생성 요청 ID
	FString NLRequestId;        // NL 변환 요청 ID
	FString NLResultBuffer;     // 변환 결과 누적
	bool bAutoGenerateAfterConvert = false;
	FDelegateHandle GenerationEventHandle;  // OnGenerationEvent 구독 핸들

	// ── SD WebUI Status (Texture tab only) ──
	TSharedPtr<SVerticalBox> SDStatusSection;
	TSharedPtr<STextBlock> SDStatusText;

	// ── Widgets ──
	TSharedPtr<SMultiLineEditableTextBox> NaturalLanguageEditor;
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
	TSharedRef<SWidget> BuildSDStatusSection();
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

	// ── NL → Intent (via MCP) ──
	void OnConvertNL();
	void OnConvertAndGenerate();
	FString BuildNLConversionPrompt(const FString& NaturalLanguage) const;

	// ── Actions ──
	void OnLoadFromStep();
	void OnGenerate();
	void OnCancel();
	void OnAccept();
	void OnReject();
	void OnRefine();

	// ── MCP Event Handling ──
	void OnGenerationEventReceived(const FHktGenerationEvent& Event);
	void SubscribeToEvents();
	void UnsubscribeFromEvents();

	void UpdatePhaseFromToolUse(const FString& ToolName);
	void ExtractGeneratedAssets(const FString& ResultJson);

	// ── SD WebUI ──
	void OnCheckSDServer();
	void OnLaunchSDServer();
	void OnSDServerStatusChanged(bool bAlive, const FString& StatusMessage);
	void SubscribeSDStatus();
	void UnsubscribeSDStatus();
	bool IsTextureTab() const;

	// ── Helpers ──
	FString LoadSkillMd() const;
	FString BuildPrompt(const FString& IntentJson, const FString& Feedback = TEXT("")) const;
	FString FindPluginDir() const;
	void AddLogLine(const FString& Line);
	void SetSectionVisibility(bool bShowProgress, bool bShowResult, bool bShowFeedback);

	/** Generation 요청 중인지 */
	bool IsGenerating() const { return !CurrentRequestId.IsEmpty(); }
	bool IsNLConverting() const { return !NLRequestId.IsEmpty(); }
};
