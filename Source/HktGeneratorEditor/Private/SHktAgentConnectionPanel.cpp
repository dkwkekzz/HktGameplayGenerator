// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "SHktAgentConnectionPanel.h"
#include "HktGeneratorEditorSettings.h"
#include "HktClaudeProcess.h"
#include "HktGeneratorEditorModule.h"
#include "HktMcpEditorSubsystem.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "DesktopPlatformModule.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "HktAgentConnectionPanel"

SHktAgentConnectionPanel::FOnSettingsChanged SHktAgentConnectionPanel::OnSettingsChanged;

// ==================== Open (static) ====================

void SHktAgentConnectionPanel::Open()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "AI Agent Connection Manager"))
		.ClientSize(FVector2D(560, 560))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.SizingRule(ESizingRule::FixedSize);

	TSharedRef<SHktAgentConnectionPanel> Panel = SNew(SHktAgentConnectionPanel);
	Panel->OwnerWindow = Window;

	Window->SetContent(Panel);
	FSlateApplication::Get().AddModalWindow(Window, FSlateApplication::Get().GetActiveTopLevelWindow());
}

// ==================== Construct ====================

void SHktAgentConnectionPanel::Construct(const FArguments& InArgs)
{
	// 현재 설정 로드
	const UHktGeneratorEditorSettings* Settings = UHktGeneratorEditorSettings::Get();
	DetectedCLIPath = Settings->ClaudeCLIPath;
	DetectedStepsPath = Settings->StepsDataDirectory;

	// 자동 탐색 결과도 표시용으로 확인
	if (DetectedCLIPath.IsEmpty())
	{
		DetectedCLIPath = FHktClaudeProcess::AutoDetectClaudeCLI();
	}

	RefreshMcpStatus();
	RefreshAgentStatus();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(12)
		[
			SNew(SVerticalBox)

			// 제목
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PanelTitle", "AI Agent Connection Manager"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PanelDesc", "Claude CLI, Steps Data 경로 및 MCP 서버 연결 상태를 관리합니다."))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
				.AutoWrapText(true)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 8, 0, 8)
			[
				SNew(SSeparator)
			]

			// Claude CLI
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildCLISection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 8, 0, 8)
			[
				SNew(SSeparator)
			]

			// Steps Data
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildStepsSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 8, 0, 8)
			[
				SNew(SSeparator)
			]

			// MCP Server
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildMcpSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 8, 0, 8)
			[
				SNew(SSeparator)
			]

			// AI Agent Connection
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildAgentSection()
			]

			// 스프링
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 8, 0, 0)
			[
				SNew(SSeparator)
			]

			// 하단 버튼
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 8, 0, 0)
			[
				BuildButtons()
			]
		]
	];
}

// ==================== UI Builders ====================

TSharedRef<SWidget> SHktAgentConnectionPanel::BuildCLISection()
{
	const UHktGeneratorEditorSettings* Settings = UHktGeneratorEditorSettings::Get();
	FString AutoDetected = FHktClaudeProcess::AutoDetectClaudeCLI();
	bool bAutoFound = !AutoDetected.IsEmpty() && AutoDetected != TEXT("claude");

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CLIHeader", "Claude Code CLI"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		// 자동 탐색 상태
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			MakeStatusRow(
				LOCTEXT("CLIAutoDetect", "Auto-detect:"),
				FText::FromString(bAutoFound ? AutoDetected : TEXT("Not found")),
				bAutoFound ? FLinearColor(0.2f, 0.8f, 0.2f) : FLinearColor(1.0f, 0.4f, 0.2f))
		]

		// 경로 입력
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 2)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CLIPathLabel", "Custom Path (비어있으면 자동 탐색):"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0, 0, 4, 0)
			[
				SAssignNew(CLIPathEditor, SEditableTextBox)
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.Text(FText::FromString(Settings->ClaudeCLIPath))
				.HintText(LOCTEXT("CLIPathHint", "Auto-detect"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Browse", "..."))
				.ToolTipText(LOCTEXT("BrowseCLI", "Browse for claude CLI executable"))
				.OnClicked_Lambda([this]() { OnBrowseCLI(); return FReply::Handled(); })
			]
		];
}

TSharedRef<SWidget> SHktAgentConnectionPanel::BuildStepsSection()
{
	const UHktGeneratorEditorSettings* Settings = UHktGeneratorEditorSettings::Get();

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("StepsHeader", "Steps Data Directory"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 2)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("StepsPathLabel", "Custom Path (비어있으면 자동 탐색):"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0, 0, 4, 0)
			[
				SAssignNew(StepsPathEditor, SEditableTextBox)
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.Text(FText::FromString(Settings->StepsDataDirectory))
				.HintText(LOCTEXT("StepsPathHint", "Auto-detect"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("BrowseDir", "..."))
				.ToolTipText(LOCTEXT("BrowseSteps", "Browse for steps data directory"))
				.OnClicked_Lambda([this]() { OnBrowseSteps(); return FReply::Handled(); })
			]
		];
}

TSharedRef<SWidget> SHktAgentConnectionPanel::BuildMcpSection()
{
	FLinearColor StatusColor = bMcpServerRunning
		? FLinearColor(0.2f, 0.8f, 0.2f)
		: FLinearColor(1.0f, 0.4f, 0.2f);

	FString StatusText = bMcpServerRunning
		? FString::Printf(TEXT("Running (port %d, %d client(s))"), McpPort, McpClientCount)
		: TEXT("Not running");

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("McpHeader", "MCP Bridge Server"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 6, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("McpStatus", "Status:"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(McpStatusText, STextBlock)
				.Text(FText::FromString(StatusText))
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.ColorAndOpacity(FSlateColor(StatusColor))
				.AutoWrapText(true)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("McpReconnect", "Reconnect"))
				.ToolTipText(LOCTEXT("McpReconnectTip", "MCP Bridge 서버를 재시작합니다"))
				.OnClicked_Lambda([this]() { OnReconnectMcp(); return FReply::Handled(); })
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("McpNote", "MCP 서버는 에디터 시작 시 자동으로 시작됩니다. 꺼진 경우 Reconnect로 재시작하세요."))
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			.AutoWrapText(true)
		];
}

TSharedRef<SWidget> SHktAgentConnectionPanel::BuildAgentSection()
{
	// CLI 검증 상태
	FLinearColor CLIColor;
	FString CLIText;
	if (bAgentVerifying)
	{
		CLIColor = FLinearColor(0.8f, 0.8f, 0.2f);
		CLIText = TEXT("Verifying...");
	}
	else if (bAgentVerified)
	{
		CLIColor = FLinearColor(0.2f, 0.8f, 0.2f);
		CLIText = FString::Printf(TEXT("CLI found (%s)"), *AgentVersionString);
	}
	else
	{
		CLIColor = FLinearColor(1.0f, 0.4f, 0.2f);
		CLIText = AgentVersionString.IsEmpty() ? TEXT("CLI not found") : AgentVersionString;
	}

	// 외부 에이전트 연결 상태
	FLinearColor AgentColor;
	FString AgentText;
	if (bExternalAgentConnected)
	{
		AgentColor = FLinearColor(0.2f, 0.8f, 0.2f);
		if (!ConnectedAgentDisplayName.IsEmpty())
		{
			AgentText = FString::Printf(TEXT("Connected: %s [%s]"), *ConnectedAgentDisplayName, *ConnectedAgentProvider);
		}
		else
		{
			AgentText = TEXT("Connected");
		}
	}
	else
	{
		AgentColor = FLinearColor(1.0f, 0.4f, 0.2f);
		AgentText = TEXT("Not connected — 터미널에서 AI Agent를 실행해주세요");
	}

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AgentHeader", "AI Agent (External Claude CLI)"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		// 외부 에이전트 연결 상태
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SAssignNew(AgentStatusText, STextBlock)
			.Text(FText::FromString(AgentText))
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
			.ColorAndOpacity(FSlateColor(AgentColor))
			.AutoWrapText(true)
		]

		// CLI 검증 상태
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 6, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CLIVerify", "CLI:"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(CLIText))
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.ColorAndOpacity(FSlateColor(CLIColor))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("AgentVerify", "Test CLI"))
				.ToolTipText(LOCTEXT("AgentVerifyTip", "Claude CLI --version 을 실행하여 설치 여부를 확인합니다"))
				.OnClicked_Lambda([this]() { OnVerifyAgent(); return FReply::Handled(); })
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AgentNote",
				"AI Agent를 터미널에서 실행하면, MCP ConnectAgent → Heartbeat로 연결됩니다.\n"
				"Claude CLI 외에 Anthropic Console API, OpenAI API 등 다른 프로바이더도 동일한 방식으로 연결 가능합니다."))
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			.AutoWrapText(true)
		];
}

TSharedRef<SWidget> SHktAgentConnectionPanel::BuildButtons()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 4, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("Save", "Save"))
			.ToolTipText(LOCTEXT("SaveTip", "Save settings and close"))
			.OnClicked_Lambda([this]() { OnSave(); return FReply::Handled(); })
			.ButtonColorAndOpacity(FLinearColor(0.2f, 0.6f, 0.2f, 1.0f))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("Cancel", "Cancel"))
			.OnClicked_Lambda([this]() { OnClose(); return FReply::Handled(); })
		];
}

TSharedRef<SWidget> SHktAgentConnectionPanel::MakeStatusRow(
	const FText& Label, const FText& Value, const FLinearColor& Color)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 6, 0)
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(Value)
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
			.ColorAndOpacity(FSlateColor(Color))
			.AutoWrapText(true)
		];
}

// ==================== Actions ====================

void SHktAgentConnectionPanel::OnBrowseCLI()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return;
	}

	TArray<FString> OutFiles;
	bool bOpened = DesktopPlatform->OpenFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()),
		TEXT("Select Claude CLI"),
		FPaths::GetPath(CLIPathEditor->GetText().ToString()),
		TEXT(""),
#if PLATFORM_WINDOWS
		TEXT("Executable (*.exe;*.cmd;*.bat)|*.exe;*.cmd;*.bat|All Files (*.*)|*.*"),
#else
		TEXT("All Files (*)|*"),
#endif
		EFileDialogFlags::None,
		OutFiles);

	if (bOpened && OutFiles.Num() > 0)
	{
		CLIPathEditor->SetText(FText::FromString(OutFiles[0]));
	}
}

void SHktAgentConnectionPanel::OnBrowseSteps()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return;
	}

	FString OutDirectory;
	bool bOpened = DesktopPlatform->OpenDirectoryDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()),
		TEXT("Select Steps Data Directory"),
		StepsPathEditor->GetText().ToString(),
		OutDirectory);

	if (bOpened)
	{
		StepsPathEditor->SetText(FText::FromString(OutDirectory));
	}
}

void SHktAgentConnectionPanel::OnAutoDetectCLI()
{
	FString Found = FHktClaudeProcess::AutoDetectClaudeCLI();
	if (Found != TEXT("claude"))
	{
		CLIPathEditor->SetText(FText::FromString(Found));
	}
}

void SHktAgentConnectionPanel::OnAutoDetectSteps()
{
	// 기본 검색 로직은 SHktGeneratorPromptPanel::FindStepsDataPath에 있음
	// 여기서는 단순히 비워서 자동 탐색 활성화
	StepsPathEditor->SetText(FText::GetEmpty());
}

void SHktAgentConnectionPanel::OnSave()
{
	UHktGeneratorEditorSettings* Settings = UHktGeneratorEditorSettings::GetMutable();
	Settings->ClaudeCLIPath = CLIPathEditor->GetText().ToString().TrimStartAndEnd();
	Settings->StepsDataDirectory = StepsPathEditor->GetText().ToString().TrimStartAndEnd();
	Settings->SaveConfig();

	UE_LOG(LogHktGenEditor, Log, TEXT("Agent connection settings saved. CLI: '%s', Steps: '%s'"),
		*Settings->ClaudeCLIPath, *Settings->StepsDataDirectory);

	OnSettingsChanged.ExecuteIfBound();
	OnClose();
}

void SHktAgentConnectionPanel::OnClose()
{
	// 델리게이트 정리
	if (AgentVerifiedHandle.IsValid() && GEditor)
	{
		UHktMcpEditorSubsystem* McpSub = GEditor->GetEditorSubsystem<UHktMcpEditorSubsystem>();
		if (McpSub)
		{
			McpSub->OnAgentVerified.Remove(AgentVerifiedHandle);
		}
		AgentVerifiedHandle.Reset();
	}

	TSharedPtr<SWindow> Window = OwnerWindow.Pin();
	if (Window.IsValid())
	{
		Window->RequestDestroyWindow();
	}
}

void SHktAgentConnectionPanel::OnReconnectMcp()
{
	if (!GEditor)
	{
		return;
	}

	UHktMcpEditorSubsystem* McpSub = GEditor->GetEditorSubsystem<UHktMcpEditorSubsystem>();
	if (McpSub)
	{
		McpSub->ReconnectMcpServer();
		RefreshMcpStatus();

		// UI 업데이트
		if (McpStatusText.IsValid())
		{
			FLinearColor StatusColor = bMcpServerRunning
				? FLinearColor(0.2f, 0.8f, 0.2f)
				: FLinearColor(1.0f, 0.4f, 0.2f);
			FString StatusText = bMcpServerRunning
				? FString::Printf(TEXT("Running (port %d, %d client(s))"), McpPort, McpClientCount)
				: TEXT("Not running");
			McpStatusText->SetText(FText::FromString(StatusText));
			McpStatusText->SetColorAndOpacity(FSlateColor(StatusColor));
		}
	}
}

void SHktAgentConnectionPanel::OnVerifyAgent()
{
	if (!GEditor)
	{
		return;
	}

	UHktMcpEditorSubsystem* McpSub = GEditor->GetEditorSubsystem<UHktMcpEditorSubsystem>();
	if (!McpSub)
	{
		return;
	}

	// 검증 시작 상태 즉시 반영
	bAgentVerifying = true;
	bAgentVerified = false;
	if (AgentStatusText.IsValid())
	{
		AgentStatusText->SetText(LOCTEXT("AgentVerifying", "Verifying..."));
		AgentStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.2f)));
	}

	// 이전 핸들 제거 후 새로 등록 (leak 방지)
	if (AgentVerifiedHandle.IsValid())
	{
		McpSub->OnAgentVerified.Remove(AgentVerifiedHandle);
		AgentVerifiedHandle.Reset();
	}

	TWeakPtr<SHktAgentConnectionPanel> WeakPanel = SharedThis(this);
	AgentVerifiedHandle = McpSub->OnAgentVerified.AddLambda([WeakPanel](bool bSuccess, const FString& VersionOrError)
	{
		// GameThread에서 실행되므로 안전하게 UI 업데이트
		TSharedPtr<SHktAgentConnectionPanel> PinnedThis = WeakPanel.Pin();
		if (!PinnedThis.IsValid())
		{
			return;
		}

		PinnedThis->bAgentVerifying = false;
		PinnedThis->bAgentVerified = bSuccess;
		PinnedThis->AgentVersionString = bSuccess ? VersionOrError : VersionOrError;

		if (PinnedThis->AgentStatusText.IsValid())
		{
			FLinearColor Color = bSuccess
				? FLinearColor(0.2f, 0.8f, 0.2f)
				: FLinearColor(1.0f, 0.4f, 0.2f);
			FString Text = bSuccess
				? FString::Printf(TEXT("Connected (%s)"), *VersionOrError)
				: VersionOrError;
			PinnedThis->AgentStatusText->SetText(FText::FromString(Text));
			PinnedThis->AgentStatusText->SetColorAndOpacity(FSlateColor(Color));
		}
	});

	McpSub->VerifyAgentConnection();
}

void SHktAgentConnectionPanel::RefreshMcpStatus()
{
	bMcpServerRunning = false;
	McpClientCount = 0;
	McpPort = 9876;

	// 에디터 서브시스템에서 MCP 서버 상태 확인
	if (GEditor)
	{
		UHktMcpEditorSubsystem* McpSub = GEditor->GetEditorSubsystem<UHktMcpEditorSubsystem>();
		if (McpSub)
		{
			bMcpServerRunning = McpSub->IsMcpServerRunning();
			McpClientCount = McpSub->GetMcpClientCount();
			McpPort = McpSub->GetMcpServerPort();
		}
	}
}

void SHktAgentConnectionPanel::RefreshAgentStatus()
{
	bAgentVerified = false;
	bAgentVerifying = false;
	AgentVersionString.Empty();
	bExternalAgentConnected = false;
	ConnectedAgentProvider.Empty();
	ConnectedAgentDisplayName.Empty();

	if (GEditor)
	{
		UHktMcpEditorSubsystem* McpSub = GEditor->GetEditorSubsystem<UHktMcpEditorSubsystem>();
		if (McpSub)
		{
			bAgentVerified = McpSub->IsAgentVerified();
			bAgentVerifying = McpSub->IsAgentVerifying();
			AgentVersionString = McpSub->GetAgentVersion();
			bExternalAgentConnected = McpSub->IsExternalAgentConnected();

			if (bExternalAgentConnected)
			{
				FHktAgentInfo AgentInfo = McpSub->GetConnectedAgentInfo();
				ConnectedAgentProvider = AgentInfo.Provider;
				ConnectedAgentDisplayName = AgentInfo.DisplayName;
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
