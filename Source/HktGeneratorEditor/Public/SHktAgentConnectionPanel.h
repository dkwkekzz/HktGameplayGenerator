// Copyright Hkt Studios, Inc. All Rights Reserved.
// AI Agent 연결 관리 패널 — Claude CLI, Steps Data, MCP 상태 관리

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class STextBlock;

/**
 * SHktAgentConnectionPanel
 *
 * AI Agent 연결 관리 모달 윈도우.
 * Claude CLI 경로, Steps Data 경로 설정 및 MCP 서버 상태 확인.
 */
class SHktAgentConnectionPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHktAgentConnectionPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 모달 윈도우를 열어서 패널 표시 */
	static void Open();

	/** 설정 변경 시 호출되는 델리게이트 */
	DECLARE_DELEGATE(FOnSettingsChanged);
	static FOnSettingsChanged OnSettingsChanged;

private:
	// ── Widgets ──
	TSharedPtr<SEditableTextBox> CLIPathEditor;
	TSharedPtr<SEditableTextBox> StepsPathEditor;

	// ── Dynamic status widgets ──
	TSharedPtr<STextBlock> McpStatusText;
	TSharedPtr<STextBlock> AgentStatusText;

	// ── Status ──
	FString DetectedCLIPath;
	FString DetectedStepsPath;
	bool bMcpServerRunning = false;
	int32 McpClientCount = 0;
	int32 McpPort = 9876;
	bool bAgentVerified = false;
	bool bAgentVerifying = false;
	FString AgentVersionString;
	bool bExternalAgentConnected = false;
	FString ConnectedAgentProvider;
	FString ConnectedAgentDisplayName;

	// ── UI Builders ──
	TSharedRef<SWidget> BuildCLISection();
	TSharedRef<SWidget> BuildStepsSection();
	TSharedRef<SWidget> BuildMcpSection();
	TSharedRef<SWidget> BuildAgentSection();
	TSharedRef<SWidget> BuildButtons();

	// ── Actions ──
	void OnBrowseCLI();
	void OnBrowseSteps();
	void OnAutoDetectCLI();
	void OnAutoDetectSteps();
	void OnReconnectMcp();
	void OnVerifyAgent();
	void OnSave();
	void OnClose();
	void RefreshMcpStatus();
	void RefreshAgentStatus();

	// ── 상태 표시 헬퍼 ──
	TSharedRef<SWidget> MakeStatusRow(const FText& Label, const FText& Value, const FLinearColor& Color);

	/** 부모 윈도우 참조 (닫기용) */
	TWeakPtr<SWindow> OwnerWindow;

	/** Agent 검증 델리게이트 핸들 (leak 방지) */
	FDelegateHandle AgentVerifiedHandle;
};
