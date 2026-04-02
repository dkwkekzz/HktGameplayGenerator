// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "SHktGeneratorPromptPanel.h"
#include "SHktGeneratorTab.h"
#include "SHktAgentConnectionPanel.h"
#include "HktClaudeProcess.h"
#include "HktMcpEditorSubsystem.h"
#include "HktGeneratorEditorModule.h"
#include "HktTextureGeneratorSubsystem.h"
#include "HktTextureGeneratorSettings.h"
#include "Editor.h"
#include "HktGeneratorEditorSettings.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "HktGeneratorPromptPanel"

// ==================== Construct ====================

void SHktGeneratorPromptPanel::Construct(const FArguments& InArgs)
{
	StepsDataPath = FindStepsDataPath();
	DiscoverProjects();

	if (ProjectIds.Num() > 0)
	{
		SelectedProjectId = ProjectIds.Last();
	}

	// Generator 탭들 생성
	TabSwitcher = SNew(SWidgetSwitcher);

	const TArray<FHktGeneratorTypeInfo>& TypeInfos = GetGeneratorTypeInfos();
	for (const FHktGeneratorTypeInfo& Info : TypeInfos)
	{
		TSharedPtr<SHktGeneratorTab> Tab;
		TabSwitcher->AddSlot()
		[
			SAssignNew(Tab, SHktGeneratorTab)
			.GeneratorInfo(Info)
			.StepsDataPath(StepsDataPath)
			.ProjectId(SelectedProjectId)
		];
		GeneratorTabs.Add(Tab);
	}

	// 상태 초기화
	RefreshStatus();

	ChildSlot
	[
		SNew(SVerticalBox)

		// Header
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f)
		[
			BuildHeader()
		]

		// Status bar
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 4.0f)
		[
			BuildStatusBar()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f)
		[
			SNew(SSeparator)
		]

		// Project selector
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 4.0f, 8.0f, 4.0f)
		[
			BuildProjectSelector()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f)
		[
			SNew(SSeparator)
		]

		// Tab bar
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 4.0f)
		[
			BuildTabBar()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f)
		[
			SNew(SSeparator)
		]

		// Active tab content
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(0.0f)
		[
			TabSwitcher.ToSharedRef()
		]
	];

	// 초기 프로젝트 버튼 표시
	RebuildProjectSelector();

	// 초기 탭 선택
	OnTabSelected(0);

	// Agent Connection Manager에서 설정 변경 시 Refresh
	SHktAgentConnectionPanel::OnSettingsChanged.BindSP(this, &SHktGeneratorPromptPanel::OnRefreshClicked);
}

// ==================== Steps Data Path ====================

FString SHktGeneratorPromptPanel::FindStepsDataPath() const
{
	// 0) Project Settings 확인
	const UHktGeneratorEditorSettings* Settings = UHktGeneratorEditorSettings::Get();
	if (Settings && !Settings->StepsDataDirectory.IsEmpty())
	{
		FString SettingsPath = FPaths::ConvertRelativePathToFull(Settings->StepsDataDirectory);
		FPaths::NormalizeDirectoryName(SettingsPath);
		if (FPaths::DirectoryExists(SettingsPath))
		{
			UE_LOG(LogHktGenEditor, Log, TEXT("Steps data found via settings: %s"), *SettingsPath);
			return SettingsPath;
		}
		UE_LOG(LogHktGenEditor, Warning, TEXT("Settings steps path not found: %s"), *SettingsPath);
	}

	// 1) HKT_STEPS_DIR 환경변수 확인
	FString EnvSteps = FPlatformMisc::GetEnvironmentVariable(TEXT("HKT_STEPS_DIR"));
	if (!EnvSteps.IsEmpty())
	{
		FString Normalized = FPaths::ConvertRelativePathToFull(EnvSteps);
		FPaths::NormalizeDirectoryName(Normalized);
		if (FPaths::DirectoryExists(Normalized))
		{
			UE_LOG(LogHktGenEditor, Log, TEXT("Steps data found via HKT_STEPS_DIR: %s"), *Normalized);
			return Normalized;
		}
		UE_LOG(LogHktGenEditor, Warning, TEXT("HKT_STEPS_DIR set but path not found: %s"), *Normalized);
	}

	// 2) 알려진 경로들 검색
	TArray<FString> SearchPaths;
	FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	// 플러그인 McpServer 하위
	SearchPaths.Add(FPaths::Combine(ProjectRoot, TEXT("Plugins"), TEXT("HktGameplayGenerator"),
		TEXT("McpServer"), TEXT(".hkt_steps")));
	SearchPaths.Add(FPaths::Combine(ProjectRoot, TEXT("Plugins"), TEXT("HktGameplayGenerator"),
		TEXT(".hkt_steps")));
	// 프로젝트 루트
	SearchPaths.Add(FPaths::Combine(ProjectRoot, TEXT(".hkt_steps")));
	// 프로젝트 상위 (모노레포 구조)
	SearchPaths.Add(FPaths::Combine(ProjectRoot, TEXT(".."), TEXT("McpServer"), TEXT(".hkt_steps")));
	SearchPaths.Add(FPaths::Combine(ProjectRoot, TEXT("McpServer"), TEXT(".hkt_steps")));

	for (const FString& Path : SearchPaths)
	{
		FString Normalized = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeDirectoryName(Normalized);
		if (FPaths::DirectoryExists(Normalized))
		{
			UE_LOG(LogHktGenEditor, Log, TEXT("Steps data found at: %s"), *Normalized);
			return Normalized;
		}
	}

	// 3) 기본 경로 사용 (디렉토리 자동 생성)
	FString DefaultPath = FPaths::Combine(ProjectRoot, TEXT(".hkt_steps"));
	IFileManager::Get().MakeDirectory(*DefaultPath, true);
	UE_LOG(LogHktGenEditor, Log, TEXT("Steps data directory created at: %s"), *DefaultPath);
	return DefaultPath;
}

void SHktGeneratorPromptPanel::DiscoverProjects()
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

// ==================== Status ====================

void SHktGeneratorPromptPanel::RefreshStatus()
{
	DetectedClaudeCLI = FHktClaudeProcess::FindClaudeCLI();
}

// ==================== UI Builders ====================

TSharedRef<SWidget> SHktGeneratorPromptPanel::BuildStatusBar()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		.Padding(6)
		[
			SNew(SHorizontalBox)

			// 상태 정보 (왼쪽)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SVerticalBox)

				// Claude CLI
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 1)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 6, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("StatusAgent", "AI Agent:"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text_Lambda([]()
						{
							if (GEditor)
							{
								UHktMcpEditorSubsystem* McpSub = GEditor->GetEditorSubsystem<UHktMcpEditorSubsystem>();
								if (McpSub && McpSub->IsExternalAgentConnected())
								{
									FHktAgentInfo Info = McpSub->GetConnectedAgentInfo();
									if (!Info.DisplayName.IsEmpty())
									{
										return FText::FromString(FString::Printf(TEXT("%s [%s]"), *Info.DisplayName, *Info.Provider));
									}
									return FText::FromString(TEXT("Connected"));
								}
							}
							return FText::FromString(TEXT("Not connected"));
						})
						.Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
						.ColorAndOpacity_Lambda([]() -> FSlateColor
						{
							if (GEditor)
							{
								UHktMcpEditorSubsystem* McpSub = GEditor->GetEditorSubsystem<UHktMcpEditorSubsystem>();
								if (McpSub && McpSub->IsExternalAgentConnected())
								{
									return FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f));
								}
							}
							return FSlateColor(FLinearColor(1.0f, 0.4f, 0.2f));
						})
						.AutoWrapText(true)
					]
				]

				// Steps Data
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 1)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 6, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("StatusSteps", "Steps Data:"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							return FText::FromString(StepsDataPath);
						})
						.Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
						.ColorAndOpacity_Lambda([this]() -> FSlateColor
						{
							bool bFound = FPaths::DirectoryExists(StepsDataPath);
							return FSlateColor(bFound
								? FLinearColor(0.2f, 0.8f, 0.2f)
								: FLinearColor(1.0f, 0.8f, 0.2f));
						})
						.AutoWrapText(true)
					]
				]

				// Projects
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 1)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 6, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("StatusProjects", "Projects:"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							return FText::FromString(FString::Printf(TEXT("%d project(s)"), ProjectIds.Num()));
						})
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						.ColorAndOpacity_Lambda([this]() -> FSlateColor
						{
							return FSlateColor(ProjectIds.Num() > 0
								? FLinearColor(0.2f, 0.8f, 0.2f)
								: FLinearColor(0.5f, 0.5f, 0.5f));
						})
					]
				]

				// SD WebUI
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 1)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 6, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("StatusSD", "SD WebUI:"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text_Lambda([]()
						{
							if (GEditor)
							{
								auto* TexSub = GEditor->GetEditorSubsystem<UHktTextureGeneratorSubsystem>();
								if (TexSub)
								{
									if (TexSub->IsSDServerAlive())
									{
										return FText::FromString(FString::Printf(TEXT("Connected (%s)"),
											*UHktTextureGeneratorSettings::Get()->SDWebUIServerURL));
									}
									if (TexSub->IsSDServerLaunching())
									{
										return FText::FromString(TexSub->GetSDServerStatusMessage());
									}
								}
							}
							return FText::FromString(TEXT("Not connected"));
						})
						.Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
						.ColorAndOpacity_Lambda([]() -> FSlateColor
						{
							if (GEditor)
							{
								auto* TexSub = GEditor->GetEditorSubsystem<UHktTextureGeneratorSubsystem>();
								if (TexSub)
								{
									if (TexSub->IsSDServerAlive())
										return FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f));
									if (TexSub->IsSDServerLaunching())
										return FSlateColor(FLinearColor(0.9f, 0.7f, 0.1f));
								}
							}
							return FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f));
						})
						.AutoWrapText(true)
					]
				]
			]

			// 편집 버튼 (오른쪽)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("EditConnection", "Edit"))
				.ToolTipText(LOCTEXT("EditConnectionTip", "Open AI Agent Connection Manager"))
				.OnClicked_Lambda([this]()
				{
					SHktAgentConnectionPanel::Open();
					return FReply::Handled();
				})
			]
		];
}

TSharedRef<SWidget> SHktGeneratorPromptPanel::BuildHeader()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PanelTitle", "Generator Prompt"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("Refresh", "Refresh"))
			.OnClicked_Lambda([this]() { OnRefreshClicked(); return FReply::Handled(); })
		];
}

TSharedRef<SWidget> SHktGeneratorPromptPanel::BuildProjectSelector()
{
	// "Project:" 라벨만 배치. 프로젝트 버튼은 RebuildProjectSelector()에서 동적 추가
	return SAssignNew(ProjectSelectorBox, SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 8, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Project", "Project:"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		];
}

TSharedRef<SWidget> SHktGeneratorPromptPanel::BuildTabBar()
{
	TSharedRef<SHorizontalBox> TabBar = SNew(SHorizontalBox);

	const TArray<FHktGeneratorTypeInfo>& TypeInfos = GetGeneratorTypeInfos();
	for (int32 i = 0; i < TypeInfos.Num(); i++)
	{
		int32 Index = i;
		TabBar->AddSlot()
			.AutoWidth()
			.Padding(0, 0, 2, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TypeInfos[i].DisplayName))
				.OnClicked_Lambda([this, Index]() {
					OnTabSelected(Index);
					return FReply::Handled();
				})
				.ButtonColorAndOpacity_Lambda([this, Index]() -> FLinearColor {
					return (Index == ActiveTabIndex)
						? FLinearColor(0.2f, 0.4f, 0.8f, 1.0f)
						: FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);
				})
			];
	}

	return TabBar;
}

void SHktGeneratorPromptPanel::RebuildProjectSelector()
{
	if (!ProjectSelectorBox.IsValid())
	{
		return;
	}

	// 기존 프로젝트 버튼 제거 (첫 번째 "Project:" 라벨 슬롯은 유지)
	while (ProjectSelectorBox->NumSlots() > 1)
	{
		ProjectSelectorBox->RemoveSlot(ProjectSelectorBox->GetSlot(1).GetWidget());
	}

	if (ProjectIds.Num() == 0)
	{
		ProjectSelectorBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoProjects", "No projects found"))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			];
		return;
	}

	for (const FString& Id : ProjectIds)
	{
		bool bSelected = (Id == SelectedProjectId);
		ProjectSelectorBox->AddSlot()
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
}

// ==================== Actions ====================

void SHktGeneratorPromptPanel::OnProjectSelected(const FString& ProjectId)
{
	SelectedProjectId = ProjectId;

	for (auto& Tab : GeneratorTabs)
	{
		if (Tab.IsValid())
		{
			Tab->SetProject(StepsDataPath, SelectedProjectId);
		}
	}

	RebuildProjectSelector();
}

void SHktGeneratorPromptPanel::OnTabSelected(int32 Index)
{
	ActiveTabIndex = Index;
	if (TabSwitcher.IsValid())
	{
		TabSwitcher->SetActiveWidgetIndex(Index);
	}
}

void SHktGeneratorPromptPanel::OnRefreshClicked()
{
	RefreshStatus();
	StepsDataPath = FindStepsDataPath();
	DiscoverProjects();

	if (!SelectedProjectId.IsEmpty() && ProjectIds.Contains(SelectedProjectId))
	{
		// 유지
	}
	else if (ProjectIds.Num() > 0)
	{
		SelectedProjectId = ProjectIds.Last();
	}
	else
	{
		SelectedProjectId.Empty();
	}

	for (auto& Tab : GeneratorTabs)
	{
		if (Tab.IsValid())
		{
			Tab->SetProject(StepsDataPath, SelectedProjectId);
		}
	}

	RebuildProjectSelector();
}

#undef LOCTEXT_NAMESPACE
