// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "SHktGeneratorTab.h"
#include "HktGeneratorEditorModule.h"
#include "HktMcpEditorSubsystem.h"
#include "HktTextureGeneratorSubsystem.h"
#include "HktTextureGeneratorSettings.h"
#include "Editor.h"
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
#include "ISettingsModule.h"

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

	SubscribeToEvents();
	SubscribeSDStatus();

	ChildSlot
	[
		SNew(SScrollBox)

		// SD WebUI Status (Texture tab only)
		+ SScrollBox::Slot()
		.Padding(8.0f, 8.0f, 8.0f, 0.0f)
		[
			BuildSDStatusSection()
		]

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

SHktGeneratorTab::~SHktGeneratorTab()
{
	UnsubscribeFromEvents();
	UnsubscribeSDStatus();

	// 진행 중인 요청 ID를 클리어하여, 지연된 이벤트가 도착해도 무시되도록 함
	CurrentRequestId.Empty();
	NLRequestId.Empty();
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
							return !IsNLConverting() && !IsGenerating();
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
							return !IsNLConverting() && !IsGenerating();
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
							if (IsNLConverting())
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
					SNew(STextBlock)
					.Text(LOCTEXT("ModeBHeader", "Intent JSON"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]

				// 필드 가이드 (접기/펼치기) — 독립 슬롯
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 4)
				[
					SNew(SExpandableArea)
					.AreaTitle(LOCTEXT("IntentGuideTitle", "Intent Guide"))
					.InitiallyCollapsed(true)
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
						.IsEnabled_Lambda([this]() { return !IsGenerating() && !IsNLConverting(); })
						.ButtonColorAndOpacity(FLinearColor(0.2f, 0.6f, 0.2f, 1.0f))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4, 0, 0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("Cancel", "Cancel"))
						.OnClicked_Lambda([this]() { OnCancel(); return FReply::Handled(); })
						.IsEnabled_Lambda([this]() { return IsGenerating() || IsNLConverting(); })
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
	if (IsGenerating())
	{
		return;
	}

	// 외부 에이전트 연결 확인
	UHktMcpEditorSubsystem* McpSub = GEditor ? GEditor->GetEditorSubsystem<UHktMcpEditorSubsystem>() : nullptr;
	if (!McpSub)
	{
		AddLogLine(TEXT("[Error] MCP Editor Subsystem not available."));
		return;
	}

	if (!McpSub->IsExternalAgentConnected())
	{
		AddLogLine(TEXT("[Error] 외부 AI Agent가 연결되어 있지 않습니다. Claude CLI를 먼저 실행해주세요."));
		AddLogLine(TEXT("[Info] 터미널에서 claude 명령어를 실행하고, MCP 서버에 연결되면 다시 시도하세요."));
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

	if (!IntentEditor.IsValid())
	{
		return;
	}

	FString IntentJson = IntentEditor->GetText().ToString();
	if (IntentJson.TrimStartAndEnd().IsEmpty())
	{
		AddLogLine(TEXT("[Error] Intent is empty. Enter JSON directly, use 'Load from Step', or use 'Load Example' in the Intent Guide."));
		return;
	}

	FString SystemPrompt = LoadSkillMd();
	if (SystemPrompt.IsEmpty())
	{
		AddLogLine(FString::Printf(TEXT("[Error] Could not load SKILL.md for %s"), *GeneratorInfo.SkillName));
		return;
	}

	AddLogLine(FString::Printf(TEXT("[Start] %s generation (iteration %d) — requesting via MCP"),
		*GeneratorInfo.DisplayName, IterationCount));

	// MCP를 통해 생성 요청 큐잉
	FHktGenerationRequest Request;
	Request.SkillName = GeneratorInfo.SkillName;
	Request.StepType = GeneratorInfo.StepType;
	Request.ProjectId = ProjectId;
	Request.IntentJson = IntentJson;
	Request.SystemPrompt = SystemPrompt;

	CurrentRequestId = McpSub->SubmitGenerationRequest(Request);
	AddLogLine(FString::Printf(TEXT("[Queued] Request ID: %s — 외부 Agent가 처리를 시작할 때까지 대기 중..."), *CurrentRequestId));
}

void SHktGeneratorTab::OnCancel()
{
	UHktMcpEditorSubsystem* McpSub = GEditor ? GEditor->GetEditorSubsystem<UHktMcpEditorSubsystem>() : nullptr;

	if (!CurrentRequestId.IsEmpty() && McpSub)
	{
		McpSub->CancelGenerationRequest(CurrentRequestId);
		CurrentRequestId.Empty();
		AddLogLine(TEXT("[Cancelled] Generation cancelled by user"));
	}

	if (!NLRequestId.IsEmpty() && McpSub)
	{
		McpSub->CancelGenerationRequest(NLRequestId);
		NLRequestId.Empty();
		NLResultBuffer.Empty();
		bAutoGenerateAfterConvert = false;
		AddLogLine(TEXT("[Cancelled] NL conversion cancelled by user"));
	}

	SetSectionVisibility(false, false, false);
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
	if (IsGenerating())
	{
		return;
	}

	if (!FeedbackEditor.IsValid() || !IntentEditor.IsValid())
	{
		return;
	}

	FString Feedback = FeedbackEditor->GetText().ToString();
	if (Feedback.IsEmpty())
	{
		AddLogLine(TEXT("[Warning] Please enter feedback before refining."));
		return;
	}

	UHktMcpEditorSubsystem* McpSub = GEditor ? GEditor->GetEditorSubsystem<UHktMcpEditorSubsystem>() : nullptr;
	if (!McpSub || !McpSub->IsExternalAgentConnected())
	{
		AddLogLine(TEXT("[Error] 외부 AI Agent가 연결되어 있지 않습니다."));
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

	IterationCount++;
	AddLogLine(FString::Printf(TEXT("[Refine] %s generation (iteration %d) with feedback — requesting via MCP"),
		*GeneratorInfo.DisplayName, IterationCount));

	FHktGenerationRequest Request;
	Request.SkillName = GeneratorInfo.SkillName;
	Request.StepType = GeneratorInfo.StepType;
	Request.ProjectId = ProjectId;
	Request.IntentJson = IntentJson;
	Request.SystemPrompt = SystemPrompt;
	Request.Feedback = Feedback;
	Request.PreviousResult = PreviousResult;

	CurrentRequestId = McpSub->SubmitGenerationRequest(Request);
	AddLogLine(FString::Printf(TEXT("[Queued] Refine request ID: %s"), *CurrentRequestId));

	// 피드백 에디터 초기화
	FeedbackEditor->SetText(FText::GetEmpty());
}

// ==================== MCP Event Handling ====================

void SHktGeneratorTab::SubscribeToEvents()
{
	if (GEditor)
	{
		UHktMcpEditorSubsystem* McpSub = GEditor->GetEditorSubsystem<UHktMcpEditorSubsystem>();
		if (McpSub)
		{
			GenerationEventHandle = McpSub->OnGenerationEvent.AddSP(
				this, &SHktGeneratorTab::OnGenerationEventReceived);
		}
	}
}

void SHktGeneratorTab::UnsubscribeFromEvents()
{
	if (GEditor)
	{
		UHktMcpEditorSubsystem* McpSub = GEditor->GetEditorSubsystem<UHktMcpEditorSubsystem>();
		if (McpSub && GenerationEventHandle.IsValid())
		{
			McpSub->OnGenerationEvent.Remove(GenerationEventHandle);
			GenerationEventHandle.Reset();
		}
	}
}

void SHktGeneratorTab::OnGenerationEventReceived(const FHktGenerationEvent& Event)
{
	// NL 변환 이벤트 처리
	if (!NLRequestId.IsEmpty() && Event.RequestId == NLRequestId)
	{
		if (Event.EventType == TEXT("assistant"))
		{
			if (!Event.Content.IsEmpty())
			{
				NLResultBuffer += Event.Content;
			}
		}
		else if (Event.EventType == TEXT("result"))
		{
			NLResultBuffer = Event.Content;
		}
		else if (Event.EventType == TEXT("complete"))
		{
			FString RequestId = NLRequestId;
			NLRequestId.Empty();

			if (Event.ExitCode == 0 && !NLResultBuffer.IsEmpty())
			{
				// JSON 추출 — markdown 코드 블록 제거
				FString Result = NLResultBuffer.TrimStartAndEnd();
				if (Result.StartsWith(TEXT("```")))
				{
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

				if (!IntentEditor.IsValid())
				{
					NLResultBuffer.Empty();
					bAutoGenerateAfterConvert = false;
					return;
				}

				IntentEditor->SetText(FText::FromString(Result));
				AddLogLine(TEXT("[NL] Conversion successful. Intent JSON has been populated."));

				if (bAutoGenerateAfterConvert)
				{
					bAutoGenerateAfterConvert = false;
					AddLogLine(TEXT("[NL] Auto-generating from converted intent..."));
					OnGenerate();
				}
			}
			else
			{
				AddLogLine(FString::Printf(TEXT("[NL Error] Conversion failed (exit code: %d)"), Event.ExitCode));
				bAutoGenerateAfterConvert = false;
			}
			NLResultBuffer.Empty();
		}
		else if (Event.EventType == TEXT("error"))
		{
			AddLogLine(FString::Printf(TEXT("[NL Error] %s"), *Event.Content));
			NLRequestId.Empty();
			NLResultBuffer.Empty();
			bAutoGenerateAfterConvert = false;
		}
		return;
	}

	// 생성 이벤트 처리 — CurrentRequestId와 매치되는 이벤트만
	if (CurrentRequestId.IsEmpty() || Event.RequestId != CurrentRequestId)
	{
		return;
	}

	if (Event.EventType == TEXT("assistant"))
	{
		AddLogLine(FString::Printf(TEXT("[AI] %s"), *Event.Content));
	}
	else if (Event.EventType == TEXT("tool_use"))
	{
		AddLogLine(FString::Printf(TEXT("[Tool] %s"), *Event.ToolName));
		UpdatePhaseFromToolUse(Event.ToolName);
	}
	else if (Event.EventType == TEXT("tool_result"))
	{
		FString TruncatedContent = Event.Content.Left(200);
		if (Event.Content.Len() > 200) TruncatedContent += TEXT("...");
		AddLogLine(FString::Printf(TEXT("[Result] %s"), *TruncatedContent));
	}
	else if (Event.EventType == TEXT("result"))
	{
		PreviousResult = Event.Content;
		ExtractGeneratedAssets(Event.Content);
	}
	else if (Event.EventType == TEXT("complete"))
	{
		FString RequestId = CurrentRequestId;
		CurrentRequestId.Empty();

		if (Event.ExitCode == 0)
		{
			AddLogLine(TEXT("[Done] Generation completed successfully."));
			SetSectionVisibility(true, true, true);
		}
		else
		{
			AddLogLine(FString::Printf(TEXT("[Error] Generation ended with exit code %d"), Event.ExitCode));
			SetSectionVisibility(true, false, false);
		}
	}
	else if (Event.EventType == TEXT("error"))
	{
		AddLogLine(FString::Printf(TEXT("[Error] %s"), *Event.Content));
	}
	else
	{
		AddLogLine(FString::Printf(TEXT("[%s] %s"), *Event.EventType, *Event.Content.Left(200)));
	}
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

// ==================== SD WebUI Status ====================

bool SHktGeneratorTab::IsTextureTab() const
{
	return GeneratorInfo.Type == EHktGeneratorType::Texture;
}

TSharedRef<SWidget> SHktGeneratorTab::BuildSDStatusSection()
{
	const auto* Settings = UHktTextureGeneratorSettings::Get();
	const bool bHasBatchFile = !Settings->SDWebUIBatchFilePath.IsEmpty();
	const FString ServerURL = Settings->SDWebUIServerURL;

	return SAssignNew(SDStatusSection, SVerticalBox)
		.Visibility(IsTextureTab() ? EVisibility::Visible : EVisibility::Collapsed)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(8)
			[
				SNew(SVerticalBox)

				// 헤더
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 4)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0, 0, 8, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SDHeader", "SD WebUI Server"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					]

					// 상태 인디케이터 (동그라미)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0, 0, 6, 0)
					[
						SNew(SBox)
						.WidthOverride(10.0f)
						.HeightOverride(10.0f)
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
							.BorderBackgroundColor_Lambda([this]() -> FSlateColor
							{
								if (GEditor)
								{
									auto* TexSub = GEditor->GetEditorSubsystem<UHktTextureGeneratorSubsystem>();
									if (TexSub)
									{
										if (TexSub->IsSDServerAlive())
											return FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f)); // Green
										if (TexSub->IsSDServerLaunching())
											return FSlateColor(FLinearColor(0.9f, 0.7f, 0.1f)); // Yellow
									}
								}
								return FSlateColor(FLinearColor(0.8f, 0.2f, 0.2f)); // Red
							})
						]
					]

					// 상태 텍스트
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SAssignNew(SDStatusText, STextBlock)
						.Text_Lambda([this]() -> FText
						{
							if (GEditor)
							{
								auto* TexSub = GEditor->GetEditorSubsystem<UHktTextureGeneratorSubsystem>();
								if (TexSub)
								{
									const FString& Msg = TexSub->GetSDServerStatusMessage();
									if (!Msg.IsEmpty())
										return FText::FromString(Msg);

									return TexSub->IsSDServerAlive()
										? LOCTEXT("SDConnected", "Connected")
										: LOCTEXT("SDNotChecked", "Not checked");
								}
							}
							return LOCTEXT("SDUnavailable", "Subsystem not available");
						})
						.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
						.ColorAndOpacity_Lambda([this]() -> FSlateColor
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
							return FSlateColor(FLinearColor(1.0f, 0.4f, 0.2f));
						})
						.AutoWrapText(true)
					]
				]

				// 상세 정보 행: URL + Batch File
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 2, 0, 4)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 6, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SDURLLabel", "URL:"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 12, 0)
					[
						SNew(STextBlock)
						.Text(FText::FromString(ServerURL.IsEmpty() ? TEXT("(not set)") : ServerURL))
						.Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 6, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SDBatchLabel", "Launcher:"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(bHasBatchFile ? FPaths::GetCleanFilename(Settings->SDWebUIBatchFilePath) : TEXT("(not set)")))
						.Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
						.ColorAndOpacity(FSlateColor(bHasBatchFile
							? FLinearColor(0.6f, 0.6f, 0.6f)
							: FLinearColor(0.8f, 0.5f, 0.2f)))
						.ToolTipText(FText::FromString(bHasBatchFile
							? Settings->SDWebUIBatchFilePath
							: TEXT("Project Settings > HktGameplay > HktTextureGenerator > SD WebUI Batch File Path")))
					]
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
						.Text(LOCTEXT("SDCheck", "Check Connection"))
						.ToolTipText(LOCTEXT("SDCheckTip", "SD WebUI 서버에 핑을 보내 연결 상태를 확인합니다"))
						.OnClicked_Lambda([this]() { OnCheckSDServer(); return FReply::Handled(); })
						.IsEnabled_Lambda([this]()
						{
							if (!GEditor) return false;
							auto* TexSub = GEditor->GetEditorSubsystem<UHktTextureGeneratorSubsystem>();
							return TexSub && !TexSub->IsSDServerLaunching();
						})
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4, 0, 4, 0)
					[
						SNew(SButton)
						.Text_Lambda([this]() -> FText
						{
							if (GEditor)
							{
								auto* TexSub = GEditor->GetEditorSubsystem<UHktTextureGeneratorSubsystem>();
								if (TexSub && TexSub->IsSDServerLaunching())
									return LOCTEXT("SDLaunching", "Launching...");
								if (TexSub && TexSub->IsSDServerAlive())
									return LOCTEXT("SDRunning", "Server Running");
							}
							return LOCTEXT("SDLaunch", "Launch Server");
						})
						.ToolTipText_Lambda([this]() -> FText
						{
							const auto* Settings = UHktTextureGeneratorSettings::Get();
							if (Settings->SDWebUIBatchFilePath.IsEmpty())
								return LOCTEXT("SDLaunchTipNoBat", "Batch file not configured. Set it in Project Settings > HktGameplay > HktTextureGenerator");
							return FText::Format(
								LOCTEXT("SDLaunchTipFmt", "Launch: {0}"),
								FText::FromString(Settings->SDWebUIBatchFilePath));
						})
						.OnClicked_Lambda([this]() { OnLaunchSDServer(); return FReply::Handled(); })
						.IsEnabled_Lambda([this]()
						{
							if (!GEditor) return false;
							auto* TexSub = GEditor->GetEditorSubsystem<UHktTextureGeneratorSubsystem>();
							if (!TexSub) return false;
							if (TexSub->IsSDServerAlive() || TexSub->IsSDServerLaunching()) return false;
							return !UHktTextureGeneratorSettings::Get()->SDWebUIBatchFilePath.IsEmpty();
						})
						.ButtonColorAndOpacity_Lambda([this]() -> FLinearColor
						{
							if (GEditor)
							{
								auto* TexSub = GEditor->GetEditorSubsystem<UHktTextureGeneratorSubsystem>();
								if (TexSub && TexSub->IsSDServerAlive())
									return FLinearColor(0.15f, 0.15f, 0.15f, 0.5f);
							}
							return FLinearColor(0.3f, 0.5f, 0.9f, 1.0f);
						})
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("SDSettings", "Settings"))
						.ToolTipText(LOCTEXT("SDSettingsTip", "Open Project Settings > HktGameplay > HktTextureGenerator"))
						.OnClicked_Lambda([]()
						{
							FModuleManager::LoadModuleChecked<ISettingsModule>("Settings")
								.ShowViewer("Project", "HktGameplay", "HktTextureGenerator");
							return FReply::Handled();
						})
					]
				]
			]
		];
}

void SHktGeneratorTab::OnCheckSDServer()
{
	if (!GEditor) return;
	auto* TexSub = GEditor->GetEditorSubsystem<UHktTextureGeneratorSubsystem>();
	if (TexSub)
	{
		AddLogLine(TEXT("[SD] Checking SD WebUI server connection..."));
		TexSub->CheckSDServerStatus();
	}
}

void SHktGeneratorTab::OnLaunchSDServer()
{
	if (!GEditor) return;
	auto* TexSub = GEditor->GetEditorSubsystem<UHktTextureGeneratorSubsystem>();
	if (TexSub)
	{
		AddLogLine(TEXT("[SD] Launching SD WebUI server..."));
		TexSub->LaunchSDServer();
	}
}

void SHktGeneratorTab::OnSDServerStatusChanged(bool bAlive, const FString& StatusMessage)
{
	if (bAlive)
	{
		AddLogLine(FString::Printf(TEXT("[SD] Server connected: %s"), *StatusMessage));
	}
	else if (StatusMessage.Contains(TEXT("Launching")))
	{
		// 폴링 중에는 로그 안 남김 (너무 빈번)
	}
	else
	{
		AddLogLine(FString::Printf(TEXT("[SD] %s"), *StatusMessage));
	}
}

void SHktGeneratorTab::SubscribeSDStatus()
{
	if (!IsTextureTab() || !GEditor) return;

	auto* TexSub = GEditor->GetEditorSubsystem<UHktTextureGeneratorSubsystem>();
	if (TexSub)
	{
		// 초기 상태 체크 (Slate Lambda가 매 프레임 상태를 반영)
		TexSub->CheckSDServerStatus();
	}
}

void SHktGeneratorTab::UnsubscribeSDStatus()
{
	// Slate Lambda 바인딩 방식이므로 별도 해제 불필요
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
	if (IsNLConverting())
	{
		return;
	}

	UHktMcpEditorSubsystem* McpSub = GEditor ? GEditor->GetEditorSubsystem<UHktMcpEditorSubsystem>() : nullptr;
	if (!McpSub || !McpSub->IsExternalAgentConnected())
	{
		AddLogLine(TEXT("[NL Error] 외부 AI Agent가 연결되어 있지 않습니다."));
		bAutoGenerateAfterConvert = false;
		return;
	}

	if (!NaturalLanguageEditor.IsValid())
	{
		bAutoGenerateAfterConvert = false;
		return;
	}

	FString NaturalLanguage = NaturalLanguageEditor->GetText().ToString().TrimStartAndEnd();
	if (NaturalLanguage.IsEmpty())
	{
		AddLogLine(TEXT("[Error] Please enter a description in natural language."));
		bAutoGenerateAfterConvert = false;
		return;
	}

	SetSectionVisibility(true, false, false);

	AddLogLine(FString::Printf(TEXT("[NL] Converting: \"%s\"%s"),
		*NaturalLanguage.Left(100),
		bAutoGenerateAfterConvert ? TEXT(" (will auto-generate)") : TEXT("")));

	NLResultBuffer.Empty();

	// NL 변환 요청을 MCP를 통해 전송
	FHktGenerationRequest Request;
	Request.SkillName = TEXT("nl-convert");
	Request.StepType = GeneratorInfo.StepType;
	Request.ProjectId = ProjectId;
	Request.IntentJson = BuildNLConversionPrompt(NaturalLanguage);
	Request.SystemPrompt = TEXT("You output only valid JSON. No markdown, no explanation.");

	NLRequestId = McpSub->SubmitGenerationRequest(Request);
	AddLogLine(FString::Printf(TEXT("[NL] Conversion request queued: %s"), *NLRequestId));
}

#undef LOCTEXT_NAMESPACE
