// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "SHktGeneratorPromptPanel.h"
#include "SHktGeneratorTab.h"
#include "HktGeneratorEditorModule.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
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

		// Project selector
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 4.0f)
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

	// 초기 탭 선택
	OnTabSelected(0);
}

// ==================== Steps Data Path ====================

FString SHktGeneratorPromptPanel::FindStepsDataPath() const
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
			UE_LOG(LogHktGenEditor, Log, TEXT("Steps data found at: %s"), *Normalized);
			return Normalized;
		}
	}

	FString DefaultPath = FPaths::Combine(ProjectRoot, TEXT(".hkt_steps"));
	UE_LOG(LogHktGenEditor, Warning, TEXT("Steps data not found, using default: %s"), *DefaultPath);
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

// ==================== UI Builders ====================

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

	// 프로젝트 버튼들은 RebuildProjectSelector에서 동적 추가
	// Construct 후에 호출됨
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
