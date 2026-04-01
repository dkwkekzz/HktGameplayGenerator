// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "SHktGeneratorTab.h"
#include "HktGeneratorEditorModule.h"
#include "HktClaudeProcess.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SExpandableArea.h"

#define LOCTEXT_NAMESPACE "HktGeneratorTab"

// ==================== Phase → Tool Mapping ====================

static const TMap<FString, FString> ToolToPhase = {
	// VFX phases
	{ TEXT("generate_texture"),         TEXT("Material Prep") },
	{ TEXT("create_particle_material"), TEXT("Material Prep") },
	{ TEXT("build_vfx_system"),         TEXT("Niagara Build") },
	{ TEXT("assign_vfx_material"),      TEXT("Niagara Build") },
	{ TEXT("preview_vfx"),              TEXT("Preview & Refine") },
	{ TEXT("update_vfx_emitter"),       TEXT("Preview & Refine") },
	// Map phases
	{ TEXT("validate_map"),             TEXT("Validation") },
	{ TEXT("generate_terrain_preview"), TEXT("Preview") },
	// Story phases
	{ TEXT("build_story"),              TEXT("Story Build") },
	// Generic step phases
	{ TEXT("step_begin"),               TEXT("Initialization") },
	{ TEXT("step_save_output"),         TEXT("Save Output") },
	{ TEXT("step_fail"),                TEXT("Error") },
};

static FSlateColor PhaseStatusColor(EHktPhaseStatus Status)
{
	switch (Status)
	{
	case EHktPhaseStatus::Completed:  return FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f));
	case EHktPhaseStatus::InProgress: return FSlateColor(FLinearColor(0.3f, 0.6f, 1.0f));
	case EHktPhaseStatus::Failed:     return FSlateColor(FLinearColor(1.0f, 0.3f, 0.2f));
	default:                          return FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f));
	}
}

// ==================== Construct ====================

void SHktGeneratorTab::Construct(const FArguments& InArgs)
{
	GeneratorInfo = InArgs._GeneratorInfo;
	StepsDataPath = InArgs._StepsDataPath;
	ProjectId = InArgs._ProjectId;

	ChildSlot
	[
		SNew(SScrollBox)

		+ SScrollBox::Slot()
		.Padding(8.0f)
		[
			BuildIntentSection()
		]

		+ SScrollBox::Slot()
		.Padding(8.0f, 0.0f)
		[
			SNew(SSeparator)
		]

		+ SScrollBox::Slot()
		.Padding(8.0f)
		[
			BuildProgressSection()
		]

		+ SScrollBox::Slot()
		.Padding(8.0f, 0.0f)
		[
			SNew(SSeparator)
		]

		+ SScrollBox::Slot()
		.Padding(8.0f)
		[
			BuildResultSection()
		]

		+ SScrollBox::Slot()
		.Padding(8.0f, 0.0f)
		[
			SNew(SSeparator)
		]

		+ SScrollBox::Slot()
		.Padding(8.0f)
		[
			BuildFeedbackSection()
		]
	];

	// 초기 상태: Progress/Result/Feedback 숨김
	SetSectionVisibility(false, false, false);
}

// ==================== SetProject ====================

void SHktGeneratorTab::SetProject(const FString& InStepsDataPath, const FString& InProjectId)
{
	StepsDataPath = InStepsDataPath;
	ProjectId = InProjectId;
}

// ==================== UI Builders ====================

TSharedRef<SWidget> SHktGeneratorTab::BuildIntentSection()
{
	FString HelpText = GetIntentHelpText(GeneratorInfo.Type);
	FString ExampleJson = GetIntentExample(GeneratorInfo.Type);

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("IntentHeader", "{0} Intent"), FText::FromString(GeneratorInfo.DisplayName)))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 8)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("IntentHelpDual",
				"Mode A: Describe in natural language and generate directly.\n"
				"Mode B: Load intent JSON from a pipeline step, edit, and generate."))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			.AutoWrapText(true)
		]

		// ══════════════════════════════════════════════════════
		// Mode A: 자연어 입력
		// ══════════════════════════════════════════════════════
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(8)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 4)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ModeAHeader", "Natural Language"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.MaxHeight(80.0f)
				.Padding(0, 0, 0, 4)
				[
					SNew(SBox)
					.MinDesiredHeight(40.0f)
					[
						SAssignNew(NaturalLanguageEditor, SMultiLineEditableTextBox)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						.HintText(FText::Format(
							LOCTEXT("NLHint", "{0} (e.g., \"{1}\")"),
							FText::FromString(GeneratorInfo.DisplayName),
							FText::FromString(GetNLPlaceholder(GeneratorInfo.Type))))
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 4, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("ConvertNL", "Convert to Intent"))
						.ToolTipText(LOCTEXT("ConvertNLTip", "Convert to intent JSON, then review before generating"))
						.OnClicked_Lambda([this]() { OnConvertNL(); return FReply::Handled(); })
						.IsEnabled_Lambda([this]()
						{
							return (!NLProcess.IsValid() || !NLProcess->IsRunning())
								&& (!ClaudeProcess.IsValid() || !ClaudeProcess->IsRunning());
						})
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4, 0, 0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("NLGenerate", "Generate Directly"))
						.ToolTipText(LOCTEXT("NLGenerateTip", "Convert to intent JSON and immediately start generation"))
						.OnClicked_Lambda([this]() { OnConvertAndGenerate(); return FReply::Handled(); })
						.IsEnabled_Lambda([this]()
						{
							return (!NLProcess.IsValid() || !NLProcess->IsRunning())
								&& (!ClaudeProcess.IsValid() || !ClaudeProcess->IsRunning());
						})
						.ButtonColorAndOpacity(FLinearColor(0.3f, 0.5f, 0.9f, 1.0f))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(8, 0, 0, 0)
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							if (NLProcess.IsValid() && NLProcess->IsRunning())
							{
								return LOCTEXT("NLConverting", "Converting...");
							}
							return FText::GetEmpty();
						})
						.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.6f, 1.0f)))
					]
				]
			]
		]

		// ══════════════════════════════════════════════════════
		// Mode B: Pipeline / JSON 직접 편집
		// ══════════════════════════════════════════════════════
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 4)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(8)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 4)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ModeBHeader", "Intent JSON"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					]

					// 필드 가이드 (접기/펼치기)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SExpandableArea)
						.AreaTitle(LOCTEXT("IntentGuideTitle", "Intent Guide"))
						.InitiallyCollapsed(true)
						.MaxHeight(300.0f)
						.BodyContent()
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(4, 4, 4, 8)
							[
								SNew(STextBlock)
								.Text(FText::FromString(HelpText))
								.Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
								.AutoWrapText(true)
							]

							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(4, 0, 4, 4)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ExampleLabel", "-- Example --"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
								.ColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.7f, 1.0f)))
							]

							+ SVerticalBox::Slot()
							.AutoHeight()
							.MaxHeight(250.0f)
							.Padding(4, 0, 4, 4)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
								.Padding(6)
								[
									SNew(STextBlock)
									.Text(FText::FromString(ExampleJson))
									.Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
								]
							]

							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(4, 0, 4, 4)
							[
								SNew(SButton)
								.Text(LOCTEXT("LoadExample", "Load Example"))
								.ToolTipText(LOCTEXT("LoadExampleTip", "Load this example JSON into the editor"))
								.OnClicked_Lambda([this, ExampleJson]()
								{
									IntentEditor->SetText(FText::FromString(ExampleJson));
									return FReply::Handled();
								})
							]
						]
					]
				]

				// JSON 에디터
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.MaxHeight(300.0f)
				.Padding(0, 0, 0, 8)
				[
					SNew(SBox)
					.MinDesiredHeight(150.0f)
					[
						SAssignNew(IntentEditor, SMultiLineEditableTextBox)
						.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
						.HintText(LOCTEXT("IntentEditorHint", "Enter JSON here, use 'Load from Step', or convert from natural language above"))
						.Text(FText::GetEmpty())
					]
				]

				// 하단 버튼
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 4, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("LoadFromStep", "Load from Step"))
						.ToolTipText(LOCTEXT("LoadFromStepTip", "Load input data from the previous pipeline step's output"))
						.OnClicked_Lambda([this]() { OnLoadFromStep(); return FReply::Handled(); })
						.IsEnabled_Lambda([this]()
						{
							return !ProjectId.IsEmpty();
						})
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4, 0, 4, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("Generate", "Generate"))
						.ToolTipText(LOCTEXT("GenerateTip", "Generate from intent JSON"))
						.OnClicked_Lambda([this]() { OnGenerate(); return FReply::Handled(); })
						.IsEnabled_Lambda([this]() { return !ClaudeProcess.IsValid() || !ClaudeProcess->IsRunning(); })
						.ButtonColorAndOpacity(FLinearColor(0.2f, 0.6f, 0.2f, 1.0f))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4, 0, 0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("Cancel", "Cancel"))
						.OnClicked_Lambda([this]() { OnCancel(); return FReply::Handled(); })
						.IsEnabled_Lambda([this]() { return ClaudeProcess.IsValid() && ClaudeProcess->IsRunning(); })
						.ButtonColorAndOpacity(FLinearColor(0.8f, 0.2f, 0.2f, 1.0f))
					]
				]
			]
		];
}

TSharedRef<SWidget> SHktGeneratorTab::BuildProgressSection()
{
	return SAssignNew(ProgressSection, SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ProgressHeader", "Progress"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		// Phase 리스트
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(120.0f)
		.Padding(0, 0, 0, 4)
		[
			SAssignNew(PhaseListView, SListView<TSharedPtr<FHktGenerationPhase>>)
			.ListItemsSource(&Phases)
			.OnGenerateRow(this, &SHktGeneratorTab::OnGeneratePhaseRow)
			.SelectionMode(ESelectionMode::None)
		]

		// 실시간 로그
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.MaxHeight(200.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(4)
			[
				SAssignNew(LogScrollBox, SScrollBox)
			]
		];
}

TSharedRef<SWidget> SHktGeneratorTab::BuildResultSection()
{
	return SAssignNew(ResultSection, SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ResultHeader", "Results"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(150.0f)
		[
			SAssignNew(ResultListView, SListView<TSharedPtr<FString>>)
			.ListItemsSource(&ResultListItems)
			.OnGenerateRow(this, &SHktGeneratorTab::OnGenerateResultRow)
			.SelectionMode(ESelectionMode::None)
		];
}

TSharedRef<SWidget> SHktGeneratorTab::BuildFeedbackSection()
{
	return SAssignNew(FeedbackSection, SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("FeedbackHeader", "Feedback"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(12, 0, 0, 0)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() {
					return FText::Format(LOCTEXT("IterFmt", "Iteration: {0}"), FText::AsNumber(IterationCount));
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			]
		]

		// 피드백 텍스트
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(80.0f)
		.Padding(0, 0, 0, 8)
		[
			SAssignNew(FeedbackEditor, SMultiLineEditableTextBox)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.HintText(LOCTEXT("FeedbackHint", "Enter feedback for refinement..."))
		]

		// 액션 버튼
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Accept", "Accept"))
				.ToolTipText(LOCTEXT("AcceptTip", "Accept this result and save output"))
				.OnClicked_Lambda([this]() { OnAccept(); return FReply::Handled(); })
				.ButtonColorAndOpacity(FLinearColor(0.2f, 0.6f, 0.2f, 1.0f))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4, 0, 4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Refine", "Refine"))
				.ToolTipText(LOCTEXT("RefineTip", "Re-generate with feedback"))
				.OnClicked_Lambda([this]() { OnRefine(); return FReply::Handled(); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Reject", "Reject"))
				.ToolTipText(LOCTEXT("RejectTip", "Discard and go back to intent editing"))
				.OnClicked_Lambda([this]() { OnReject(); return FReply::Handled(); })
				.ButtonColorAndOpacity(FLinearColor(0.8f, 0.2f, 0.2f, 1.0f))
			]
		];
}

// ==================== Phase List Row ====================

TSharedRef<ITableRow> SHktGeneratorTab::OnGeneratePhaseRow(
	TSharedPtr<FHktGenerationPhase> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	FString StatusSym;
	switch (Item->Status)
	{
	case EHktPhaseStatus::Completed:  StatusSym = TEXT("[OK]"); break;
	case EHktPhaseStatus::InProgress: StatusSym = TEXT("[..]"); break;
	case EHktPhaseStatus::Failed:     StatusSym = TEXT("[X]");  break;
	default:                          StatusSym = TEXT("[ ]");  break;
	}

	return SNew(STableRow<TSharedPtr<FHktGenerationPhase>>, OwnerTable)
		.Padding(FMargin(2, 1))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 6, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(StatusSym))
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.ColorAndOpacity(PhaseStatusColor(Item->Status))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->PhaseName))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Description))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			]
		];
}

// ==================== Result List Row ====================

TSharedRef<ITableRow> SHktGeneratorTab::OnGenerateResultRow(
	TSharedPtr<FString> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		.Padding(FMargin(2, 1))
		[
			SNew(STextBlock)
			.Text(FText::FromString(*Item))
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
		];
}

// ==================== Actions ====================

void SHktGeneratorTab::OnLoadFromStep()
{
	if (StepsDataPath.IsEmpty() || ProjectId.IsEmpty())
	{
		AddLogLine(TEXT("[Error] No project selected. Select a project first."));
		return;
	}

	// Generator에 따라 다른 이전 스텝의 output을 로드
	// VFX/Character/Item → asset_discovery output
	// Map/Story → concept_design output
	// Texture → 독립 스텝이 아니므로 concept_design 전체
	FString SourceStep;
	if (GeneratorInfo.Type == EHktGeneratorType::Map
		|| GeneratorInfo.Type == EHktGeneratorType::Story
		|| GeneratorInfo.Type == EHktGeneratorType::Texture)
	{
		SourceStep = TEXT("concept_design");
	}
	else
	{
		SourceStep = TEXT("asset_discovery");
	}

	FString OutputPath = FPaths::Combine(StepsDataPath, ProjectId, SourceStep, TEXT("output.json"));

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *OutputPath))
	{
		AddLogLine(FString::Printf(TEXT("[Error] Could not load %s"), *OutputPath));
		return;
	}

	// Map은 concept_design 전체가 필요 (terrain_spec + stories)
	// Texture는 별도 InputKey가 없으므로 전체 로드
	// 나머지는 InputKey로 서브셋만 추출
	bool bLoadFull = (GeneratorInfo.Type == EHktGeneratorType::Map
		|| GeneratorInfo.Type == EHktGeneratorType::Texture);

	if (bLoadFull)
	{
		// Pretty-print 시도
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		TSharedPtr<FJsonObject> Root;
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			FString PrettyJson;
			TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
				TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&PrettyJson);
			FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
			IntentEditor->SetText(FText::FromString(PrettyJson));
		}
		else
		{
			IntentEditor->SetText(FText::FromString(JsonString));
		}
		AddLogLine(FString::Printf(TEXT("[Info] Loaded full %s output"), *SourceStep));
	}
	else
	{
		// InputKey에 해당하는 데이터만 추출하여 표시
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		TSharedPtr<FJsonObject> Root;
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			const TSharedPtr<FJsonObject>* SubObj = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* SubArr = nullptr;

			FString PrettyJson;
			if (Root->TryGetObjectField(GeneratorInfo.InputKey, SubObj))
			{
				TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
					TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&PrettyJson);
				FJsonSerializer::Serialize(SubObj->ToSharedRef(), Writer);
			}
			else if (Root->TryGetArrayField(GeneratorInfo.InputKey, SubArr))
			{
				TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
					TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&PrettyJson);
				TSharedRef<FJsonObject> Wrapper = MakeShared<FJsonObject>();
				Wrapper->SetArrayField(GeneratorInfo.InputKey, *SubArr);
				FJsonSerializer::Serialize(Wrapper, Writer);
			}
			else
			{
				PrettyJson = JsonString;
			}

			IntentEditor->SetText(FText::FromString(PrettyJson));
			AddLogLine(FString::Printf(TEXT("[Info] Loaded %s data from %s output"), *GeneratorInfo.InputKey, *SourceStep));
		}
		else
		{
			IntentEditor->SetText(FText::FromString(JsonString));
			AddLogLine(TEXT("[Warning] Could not parse JSON, loaded raw content"));
		}
	}
}

void SHktGeneratorTab::OnGenerate()
{
	if (ClaudeProcess.IsValid() && ClaudeProcess->IsRunning())
	{
		return;
	}

	// 초기화
	Phases.Empty();
	LogLines.Empty();
	GeneratedAssets.Empty();
	ResultListItems.Empty();
	PreviousResult.Empty();
	IterationCount++;

	if (LogScrollBox.IsValid())
	{
		LogScrollBox->ClearChildren();
	}

	SetSectionVisibility(true, false, false);

	FString IntentJson = IntentEditor->GetText().ToString();
	if (IntentJson.TrimStartAndEnd().IsEmpty())
	{
		AddLogLine(TEXT("[Error] Intent is empty. Enter JSON directly, use 'Load from Step', or use 'Load Example' in the Intent Guide."));
		return;
	}

	FString SystemPrompt = LoadSkillMd();
	FString Prompt = BuildPrompt(IntentJson);

	if (SystemPrompt.IsEmpty())
	{
		AddLogLine(FString::Printf(TEXT("[Error] Could not load SKILL.md for %s"), *GeneratorInfo.SkillName));
		return;
	}

	AddLogLine(FString::Printf(TEXT("[Start] %s generation (iteration %d)"), *GeneratorInfo.DisplayName, IterationCount));

	// 기존 Tick 핸들이 있으면 해제
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	// CLI 프로세스 시작
	ClaudeProcess = MakeShared<FHktClaudeProcess>();
	ClaudeProcess->OnOutput.BindSP(this, &SHktGeneratorTab::OnClaudeOutput);
	ClaudeProcess->OnComplete.BindSP(this, &SHktGeneratorTab::OnClaudeComplete);
	ClaudeProcess->OnError.BindSP(this, &SHktGeneratorTab::OnClaudeError);

	FString WorkingDir = FindPluginDir();
	if (!ClaudeProcess->Start(Prompt, SystemPrompt, WorkingDir))
	{
		AddLogLine(TEXT("[Error] Failed to start claude CLI process"));
		ClaudeProcess.Reset();
		return;
	}

	// Tick 등록
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateSP(this, &SHktGeneratorTab::OnTick), 0.05f);
}

void SHktGeneratorTab::OnCancel()
{
	if (ClaudeProcess.IsValid())
	{
		ClaudeProcess->Cancel();
		AddLogLine(TEXT("[Cancelled] Generation cancelled by user"));
	}
}

void SHktGeneratorTab::OnAccept()
{
	AddLogLine(TEXT("[Accept] Result accepted. Saving output..."));

	// 결과를 step output으로 저장
	if (!StepsDataPath.IsEmpty() && !ProjectId.IsEmpty() && !PreviousResult.IsEmpty())
	{
		FString OutputDir = FPaths::Combine(StepsDataPath, ProjectId, GeneratorInfo.StepType);
		IFileManager::Get().MakeDirectory(*OutputDir, true);

		FString OutputPath = FPaths::Combine(OutputDir, TEXT("output.json"));
		FFileHelper::SaveStringToFile(PreviousResult, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		AddLogLine(FString::Printf(TEXT("[Saved] Output saved to %s"), *OutputPath));
	}

	SetSectionVisibility(false, true, false);
}

void SHktGeneratorTab::OnReject()
{
	AddLogLine(TEXT("[Reject] Result rejected. Returning to intent editing."));
	SetSectionVisibility(false, false, false);
	PreviousResult.Empty();
}

void SHktGeneratorTab::OnRefine()
{
	FString Feedback = FeedbackEditor->GetText().ToString();
	if (Feedback.IsEmpty())
	{
		AddLogLine(TEXT("[Warning] Please enter feedback before refining."));
		return;
	}

	// 이전 결과 + 피드백으로 재실행
	Phases.Empty();
	LogLines.Empty();
	GeneratedAssets.Empty();
	ResultListItems.Empty();

	if (LogScrollBox.IsValid())
	{
		LogScrollBox->ClearChildren();
	}

	SetSectionVisibility(true, false, false);

	FString IntentJson = IntentEditor->GetText().ToString();
	FString SystemPrompt = LoadSkillMd();
	FString Prompt = BuildPrompt(IntentJson, Feedback);

	IterationCount++;
	AddLogLine(FString::Printf(TEXT("[Refine] %s generation (iteration %d) with feedback"), *GeneratorInfo.DisplayName, IterationCount));

	// 기존 Tick 핸들이 있으면 해제
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	ClaudeProcess = MakeShared<FHktClaudeProcess>();
	ClaudeProcess->OnOutput.BindSP(this, &SHktGeneratorTab::OnClaudeOutput);
	ClaudeProcess->OnComplete.BindSP(this, &SHktGeneratorTab::OnClaudeComplete);
	ClaudeProcess->OnError.BindSP(this, &SHktGeneratorTab::OnClaudeError);

	FString WorkingDir = FindPluginDir();
	if (!ClaudeProcess->Start(Prompt, SystemPrompt, WorkingDir))
	{
		AddLogLine(TEXT("[Error] Failed to start claude CLI process for refinement"));
		ClaudeProcess.Reset();
		return;
	}

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateSP(this, &SHktGeneratorTab::OnTick), 0.05f);

	// 피드백 에디터 초기화
	FeedbackEditor->SetText(FText::GetEmpty());
}

// ==================== CLI Event Handling ====================

void SHktGeneratorTab::OnClaudeOutput(const FString& JsonLine)
{
	FHktClaudeEvent Event = ParseClaudeEvent(JsonLine);

	if (Event.Type == TEXT("assistant"))
	{
		AddLogLine(FString::Printf(TEXT("[AI] %s"), *Event.Content));
	}
	else if (Event.Type == TEXT("tool_use"))
	{
		AddLogLine(FString::Printf(TEXT("[Tool] %s"), *Event.ToolName));
		UpdatePhaseFromToolUse(Event.ToolName);
	}
	else if (Event.Type == TEXT("tool_result"))
	{
		// 도구 결과는 간략히 표시
		FString TruncatedContent = Event.Content.Left(200);
		if (Event.Content.Len() > 200) TruncatedContent += TEXT("...");
		AddLogLine(FString::Printf(TEXT("[Result] %s"), *TruncatedContent));
	}
	else if (Event.Type == TEXT("result"))
	{
		PreviousResult = Event.Content;
		ExtractGeneratedAssets(Event.Content);
	}
	else
	{
		// 알 수 없는 타입 — 그래도 로그에 표시
		AddLogLine(FString::Printf(TEXT("[%s] %s"), *Event.Type, *Event.Content.Left(200)));
	}
}

void SHktGeneratorTab::OnClaudeComplete(int32 ExitCode)
{
	// Tick 해제
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	if (ExitCode == 0)
	{
		AddLogLine(TEXT("[Done] Generation completed successfully."));
		SetSectionVisibility(true, true, true);
	}
	else
	{
		AddLogLine(FString::Printf(TEXT("[Error] Generation ended with exit code %d"), ExitCode));
		SetSectionVisibility(true, false, false);
	}
}

void SHktGeneratorTab::OnClaudeError(const FString& ErrorMsg)
{
	AddLogLine(FString::Printf(TEXT("[Error] %s"), *ErrorMsg));
}

FHktClaudeEvent SHktGeneratorTab::ParseClaudeEvent(const FString& JsonLine) const
{
	FHktClaudeEvent Event;

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonLine);
	TSharedPtr<FJsonObject> JsonObj;
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		// JSON 파싱 실패 — 일반 텍스트로 취급
		Event.Type = TEXT("text");
		Event.Content = JsonLine;
		return Event;
	}

	JsonObj->TryGetStringField(TEXT("type"), Event.Type);

	// assistant 메시지에서 텍스트 추출
	if (Event.Type == TEXT("assistant"))
	{
		const TArray<TSharedPtr<FJsonValue>>* ContentArr;
		if (JsonObj->TryGetArrayField(TEXT("content"), ContentArr))
		{
			for (const auto& Val : *ContentArr)
			{
				const TSharedPtr<FJsonObject>* ContentObj;
				if (Val->TryGetObject(ContentObj))
				{
					FString ContentType;
					(*ContentObj)->TryGetStringField(TEXT("type"), ContentType);
					if (ContentType == TEXT("text"))
					{
						FString Text;
						(*ContentObj)->TryGetStringField(TEXT("text"), Text);
						if (!Event.Content.IsEmpty()) Event.Content += TEXT("\n");
						Event.Content += Text;
					}
				}
			}
		}
	}
	else if (Event.Type == TEXT("tool_use"))
	{
		JsonObj->TryGetStringField(TEXT("name"), Event.ToolName);
		const TSharedPtr<FJsonObject>* InputObj;
		if (JsonObj->TryGetObjectField(TEXT("input"), InputObj))
		{
			FString InputStr;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&InputStr);
			FJsonSerializer::Serialize(InputObj->ToSharedRef(), Writer);
			Event.ToolInput = InputStr;
		}
	}
	else if (Event.Type == TEXT("tool_result"))
	{
		JsonObj->TryGetStringField(TEXT("content"), Event.Content);
	}
	else if (Event.Type == TEXT("result"))
	{
		JsonObj->TryGetStringField(TEXT("result"), Event.Content);
		if (Event.Content.IsEmpty())
		{
			// result 필드가 없으면 전체 JSON을 Content로
			Event.Content = JsonLine;
		}
	}

	return Event;
}

void SHktGeneratorTab::UpdatePhaseFromToolUse(const FString& ToolName)
{
	const FString* PhaseName = ToolToPhase.Find(ToolName);
	if (!PhaseName)
	{
		return;
	}

	// 기존 Phase 중 이름이 같은 것을 찾아 InProgress로
	bool bFound = false;
	for (auto& Phase : Phases)
	{
		if (Phase->PhaseName == *PhaseName)
		{
			Phase->Status = EHktPhaseStatus::InProgress;
			Phase->Description = ToolName;
			bFound = true;
		}
		else if (Phase->Status == EHktPhaseStatus::InProgress && Phase->PhaseName != *PhaseName)
		{
			// 이전 Phase 완료 처리
			Phase->Status = EHktPhaseStatus::Completed;
		}
	}

	if (!bFound)
	{
		auto NewPhase = MakeShared<FHktGenerationPhase>();
		NewPhase->PhaseName = *PhaseName;
		NewPhase->Description = ToolName;
		NewPhase->Status = EHktPhaseStatus::InProgress;
		Phases.Add(NewPhase);
	}

	if (PhaseListView.IsValid())
	{
		PhaseListView->RequestListRefresh();
	}
}

void SHktGeneratorTab::ExtractGeneratedAssets(const FString& ResultJson)
{
	ResultListItems.Empty();

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResultJson);
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* AssetsArr;
	if (Root->TryGetArrayField(TEXT("generated_assets"), AssetsArr))
	{
		for (const auto& Val : *AssetsArr)
		{
			const TSharedPtr<FJsonObject>* AssetObj;
			if (Val->TryGetObject(AssetObj))
			{
				FString AssetTag, Path;
				(*AssetObj)->TryGetStringField(TEXT("tag"), AssetTag);
				(*AssetObj)->TryGetStringField(TEXT("asset_path"), Path);
				ResultListItems.Add(MakeShared<FString>(FString::Printf(TEXT("%s → %s"), *AssetTag, *Path)));
			}
		}
	}

	if (ResultListView.IsValid())
	{
		ResultListView->RequestListRefresh();
	}
}

// ==================== Helpers ====================

FString SHktGeneratorTab::LoadSkillMd() const
{
	FString PluginDir = FindPluginDir();
	FString SkillPath = FPaths::Combine(PluginDir, TEXT(".claude"), TEXT("skills"),
		GeneratorInfo.SkillName, TEXT("SKILL.md"));

	FString Content;
	if (FFileHelper::LoadFileToString(Content, *SkillPath))
	{
		return Content;
	}

	UE_LOG(LogHktGenEditor, Warning, TEXT("SKILL.md not found at: %s"), *SkillPath);
	return FString();
}

FString SHktGeneratorTab::BuildPrompt(const FString& IntentJson, const FString& Feedback) const
{
	FString Prompt;

	Prompt += FString::Printf(TEXT("Project: %s\n"), *ProjectId);
	Prompt += FString::Printf(TEXT("Step: %s\n"), *GeneratorInfo.StepType);
	Prompt += TEXT("\n## Intent\n\n");
	Prompt += IntentJson;

	if (!Feedback.IsEmpty() && !PreviousResult.IsEmpty())
	{
		Prompt += TEXT("\n\n## Previous Result\n\n");
		Prompt += PreviousResult;
		Prompt += TEXT("\n\n## Feedback\n\n");
		Prompt += Feedback;
		Prompt += TEXT("\n\nPlease refine the generation based on the feedback above.");
	}

	return Prompt;
}

FString SHktGeneratorTab::FindPluginDir() const
{
	// 플러그인 디렉토리 탐색: Plugins/HktGameplayGenerator/
	FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	TArray<FString> SearchPaths;
	SearchPaths.Add(FPaths::Combine(ProjectDir, TEXT("Plugins"), TEXT("HktGameplayGenerator")));
	SearchPaths.Add(FPaths::Combine(ProjectDir, TEXT(".."), TEXT("HktGameplayGenerator")));
	SearchPaths.Add(ProjectDir);

	for (const FString& Path : SearchPaths)
	{
		FString Normalized = FPaths::ConvertRelativePathToFull(Path);
		FString SkillCheck = FPaths::Combine(Normalized, TEXT(".claude"), TEXT("skills"));
		if (FPaths::DirectoryExists(SkillCheck))
		{
			return Normalized;
		}
	}

	return ProjectDir;
}

void SHktGeneratorTab::AddLogLine(const FString& Line)
{
	LogLines.Add(Line);

	if (LogScrollBox.IsValid())
	{
		LogScrollBox->AddSlot()
			.Padding(2, 1)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Line))
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
				.AutoWrapText(true)
			];

		LogScrollBox->ScrollToEnd();
	}
}

void SHktGeneratorTab::SetSectionVisibility(bool bShowProgress, bool bShowResult, bool bShowFeedback)
{
	if (ProgressSection.IsValid())
	{
		ProgressSection->SetVisibility(bShowProgress ? EVisibility::Visible : EVisibility::Collapsed);
	}
	if (ResultSection.IsValid())
	{
		ResultSection->SetVisibility(bShowResult ? EVisibility::Visible : EVisibility::Collapsed);
	}
	if (FeedbackSection.IsValid())
	{
		FeedbackSection->SetVisibility(bShowFeedback ? EVisibility::Visible : EVisibility::Collapsed);
	}
}

bool SHktGeneratorTab::OnTick(float DeltaTime)
{
	if (ClaudeProcess.IsValid())
	{
		ClaudeProcess->Tick();

		if (!ClaudeProcess->IsRunning())
		{
			return false; // Tick 해제
		}
	}
	return true; // 계속 Tick
}

// ==================== NL → Intent Conversion ====================

FString SHktGeneratorTab::BuildNLConversionPrompt(const FString& NaturalLanguage) const
{
	FString HelpText = GetIntentHelpText(GeneratorInfo.Type);
	FString ExampleJson = GetIntentExample(GeneratorInfo.Type);

	FString Prompt;
	Prompt += TEXT("You are a JSON generator. Convert the user's natural language description into a valid intent JSON.\n\n");
	Prompt += TEXT("## Generator Type\n\n");
	Prompt += GeneratorInfo.DisplayName;
	Prompt += TEXT("\n\n## Intent Schema\n\n");
	Prompt += HelpText;
	Prompt += TEXT("\n\n## Example Output\n\n");
	Prompt += ExampleJson;
	Prompt += TEXT("\n\n## Rules\n\n");
	Prompt += TEXT("- Output ONLY the JSON. No explanations, no markdown fences, no comments.\n");
	Prompt += TEXT("- Follow the schema exactly. Fill all required fields.\n");
	Prompt += TEXT("- Infer reasonable values for fields the user didn't mention.\n");
	Prompt += TEXT("- Use the example as a structural reference.\n");
	Prompt += TEXT("- Tag naming convention: VFX → VFX.{Event}.{Element}, Character → Entity.Character.{Name}, ");
	Prompt += TEXT("Item → Entity.Item.{Category}.{SubType}, Story → Story.Quest.{Name}\n");
	Prompt += TEXT("- For VFX: choose appropriate emitter roles (core, spark, trail, smoke, debris, ring, shockwave) ");
	Prompt += TEXT("based on the description. Set color_palette RGB values > 1.0 for HDR emissive effects.\n");
	Prompt += TEXT("\n## User Description\n\n");
	Prompt += NaturalLanguage;

	return Prompt;
}

void SHktGeneratorTab::OnConvertAndGenerate()
{
	bAutoGenerateAfterConvert = true;
	OnConvertNL();
}

void SHktGeneratorTab::OnConvertNL()
{
	if (NLProcess.IsValid() && NLProcess->IsRunning())
	{
		return;
	}

	FString NaturalLanguage = NaturalLanguageEditor->GetText().ToString().TrimStartAndEnd();
	if (NaturalLanguage.IsEmpty())
	{
		AddLogLine(TEXT("[Error] Please enter a description in natural language."));
		bAutoGenerateAfterConvert = false;
		return;
	}

	AddLogLine(FString::Printf(TEXT("[NL] Converting: \"%s\"%s"),
		*NaturalLanguage.Left(100),
		bAutoGenerateAfterConvert ? TEXT(" (will auto-generate)") : TEXT("")));

	NLResultBuffer.Empty();

	NLProcess = MakeShared<FHktClaudeProcess>();
	NLProcess->OnOutput.BindSP(this, &SHktGeneratorTab::OnNLOutput);
	NLProcess->OnComplete.BindSP(this, &SHktGeneratorTab::OnNLComplete);
	NLProcess->OnError.BindLambda([this](const FString& Err)
	{
		AddLogLine(FString::Printf(TEXT("[NL Error] %s"), *Err));
	});

	FString Prompt = BuildNLConversionPrompt(NaturalLanguage);
	FString SystemPrompt = TEXT("You output only valid JSON. No markdown, no explanation.");
	FString WorkingDir = FindPluginDir();

	if (!NLProcess->Start(Prompt, SystemPrompt, WorkingDir))
	{
		AddLogLine(TEXT("[NL Error] Failed to start Claude CLI for conversion."));
		NLProcess.Reset();
		return;
	}

	// NL Tick 등록
	if (NLTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(NLTickHandle);
		NLTickHandle.Reset();
	}
	NLTickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateSP(this, &SHktGeneratorTab::OnNLTick), 0.05f);
}

void SHktGeneratorTab::OnNLOutput(const FString& JsonLine)
{
	FHktClaudeEvent Event = ParseClaudeEvent(JsonLine);

	if (Event.Type == TEXT("result"))
	{
		NLResultBuffer = Event.Content;
	}
	else if (Event.Type == TEXT("assistant"))
	{
		// 스트리밍 텍스트가 JSON일 수 있으므로 누적
		if (!Event.Content.IsEmpty())
		{
			NLResultBuffer += Event.Content;
		}
	}
}

void SHktGeneratorTab::OnNLComplete(int32 ExitCode)
{
	// NL Tick 해제
	if (NLTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(NLTickHandle);
		NLTickHandle.Reset();
	}

	if (ExitCode != 0 || NLResultBuffer.IsEmpty())
	{
		AddLogLine(FString::Printf(TEXT("[NL Error] Conversion failed (exit code: %d)"), ExitCode));
		NLProcess.Reset();
		bAutoGenerateAfterConvert = false;
		return;
	}

	// JSON 추출 — markdown 코드 블록 제거
	FString Result = NLResultBuffer.TrimStartAndEnd();
	if (Result.StartsWith(TEXT("```")))
	{
		// 첫 줄(```json 등)과 마지막 줄(```) 제거
		int32 FirstNewline;
		if (Result.FindChar(TEXT('\n'), FirstNewline))
		{
			Result.RightChopInline(FirstNewline + 1);
		}
		if (Result.EndsWith(TEXT("```")))
		{
			Result.LeftChopInline(3);
			Result.TrimEndInline();
		}
	}

	// JSON 검증
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Result);
	TSharedPtr<FJsonValue> Parsed;
	if (FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid())
	{
		IntentEditor->SetText(FText::FromString(Result));
		AddLogLine(TEXT("[NL] Conversion successful. Intent JSON has been populated."));
	}
	else
	{
		// JSON이 아닌 경우에도 일단 표시 (사용자가 수정 가능)
		IntentEditor->SetText(FText::FromString(Result));
		AddLogLine(TEXT("[NL Warning] Output may not be valid JSON. Please review and fix if needed."));
	}

	NLProcess.Reset();

	// Convert → Generate 연결
	if (bAutoGenerateAfterConvert)
	{
		bAutoGenerateAfterConvert = false;
		AddLogLine(TEXT("[NL] Auto-generating from converted intent..."));
		OnGenerate();
	}
}

bool SHktGeneratorTab::OnNLTick(float DeltaTime)
{
	if (NLProcess.IsValid())
	{
		NLProcess->Tick();
		if (!NLProcess->IsRunning())
		{
			return false;
		}
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
