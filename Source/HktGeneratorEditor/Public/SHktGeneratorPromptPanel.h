// Copyright Hkt Studios, Inc. All Rights Reserved.
// Generator Prompt 메인 패널 — Generator별 탭 바 + 프로젝트 선택

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "HktGeneratorPromptTypes.h"

class SHktGeneratorTab;
class SWidgetSwitcher;
class SHorizontalBox;

/**
 * SHktGeneratorPromptPanel
 *
 * 에디터 NomadTab에 표시되는 메인 패널.
 * 상단에 프로젝트 선택 + Generator 탭 바,
 * 하단에 활성 탭의 SHktGeneratorTab을 표시.
 */
class SHktGeneratorPromptPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHktGeneratorPromptPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// ── Data ──
	FString StepsDataPath;
	TArray<FString> ProjectIds;
	FString SelectedProjectId;
	int32 ActiveTabIndex = 0;

	// ── Widgets ──
	TSharedPtr<SWidgetSwitcher> TabSwitcher;
	TArray<TSharedPtr<SHktGeneratorTab>> GeneratorTabs;
	TSharedPtr<SHorizontalBox> ProjectSelectorBox;

	// ── Status ──
	FString DetectedClaudeCLI;

	// ── Methods ──
	FString FindStepsDataPath() const;
	void DiscoverProjects();
	void OnProjectSelected(const FString& ProjectId);
	void OnTabSelected(int32 Index);
	void OnRefreshClicked();

	TSharedRef<SWidget> BuildHeader();
	TSharedRef<SWidget> BuildStatusBar();
	TSharedRef<SWidget> BuildProjectSelector();
	TSharedRef<SWidget> BuildTabBar();
	void RebuildProjectSelector();
	void RefreshStatus();
};
