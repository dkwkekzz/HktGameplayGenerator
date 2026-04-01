// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "SHktAgentConnectionPanel.h"
#include "HktGeneratorEditorSettings.h"
#include "HktClaudeProcess.h"
#include "HktGeneratorEditorModule.h"
#include "HktMcpBridgeSubsystem.h"
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
#include "Engine/GameEngine.h"
#include "Engine/GameInstance.h"

#define LOCTEXT_NAMESPACE "HktAgentConnectionPanel"

SHktAgentConnectionPanel::FOnSettingsChanged SHktAgentConnectionPanel::OnSettingsChanged;

// ==================== Open (static) ====================

void SHktAgentConnectionPanel::Open()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "AI Agent Connection Manager"))
		.ClientSize(FVector2D(560, 420))
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
		? FString::Printf(TEXT("Running (port 9876, %d client(s))"), McpClientCount)
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
		.Padding(0, 0, 0, 2)
		[
			MakeStatusRow(
				LOCTEXT("McpStatus", "Status:"),
				FText::FromString(StatusText),
				StatusColor)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("McpNote", "MCP 서버는 PIE(Play In Editor) 실행 시 자동으로 시작됩니다."))
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
	TSharedPtr<SWindow> Window = OwnerWindow.Pin();
	if (Window.IsValid())
	{
		Window->RequestDestroyWindow();
	}
}

void SHktAgentConnectionPanel::RefreshMcpStatus()
{
	bMcpServerRunning = false;
	McpClientCount = 0;

	// PIE 실행 중인 GameInstance에서 서브시스템 확인
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World())
			{
				UGameInstance* GI = Context.World()->GetGameInstance();
				if (GI)
				{
					UHktMcpBridgeSubsystem* McpSub = GI->GetSubsystem<UHktMcpBridgeSubsystem>();
					if (McpSub)
					{
						bMcpServerRunning = McpSub->IsServerRunning();
						McpClientCount = McpSub->GetConnectedClientCount();
						break;
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
