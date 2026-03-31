// Copyright Hkt Studios, Inc. All Rights Reserved.
// Generator Editor 설정 — Project Settings > HktGameplay > HktGeneratorEditor

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HktGeneratorEditorSettings.generated.h"

/**
 * UHktGeneratorEditorSettings
 *
 * AI Agent 연결 설정.
 * Project Settings > HktGameplay > HktGeneratorEditor에서 편집 가능.
 * Agent Connection Manager 패널에서도 직접 수정 가능.
 */
UCLASS(config = EditorPerProjectUserSettings, DefaultConfig, DisplayName = "Hkt Generator Editor")
class HKTGENERATOREDITOR_API UHktGeneratorEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UHktGeneratorEditorSettings();

	// ============================================================================
	// Claude CLI
	// ============================================================================

	/** Claude Code CLI 실행 파일 경로. 비어있으면 자동 탐색. */
	UPROPERTY(Config, EditAnywhere, Category = "Claude CLI",
		meta = (DisplayName = "Claude CLI Path",
			ToolTip = "Claude Code CLI 경로. 비어있으면 자동 탐색합니다. (예: C:/Users/user/.claude/local/claude.exe)"))
	FString ClaudeCLIPath;

	// ============================================================================
	// Steps Data
	// ============================================================================

	/** .hkt_steps 디렉토리 경로. 비어있으면 자동 탐색. */
	UPROPERTY(Config, EditAnywhere, Category = "Steps Data",
		meta = (DisplayName = "Steps Data Directory",
			ToolTip = "스텝 데이터(.hkt_steps) 경로. 비어있으면 자동 탐색합니다."))
	FString StepsDataDirectory;

	// ============================================================================
	// MCP Server
	// ============================================================================

	/** MCP Bridge WebSocket 서버 포트 */
	UPROPERTY(Config, EditAnywhere, Category = "MCP Server",
		meta = (DisplayName = "MCP Server Port", ClampMin = "1024", ClampMax = "65535"))
	int32 McpServerPort = 9876;

	// ============================================================================
	// UDeveloperSettings
	// ============================================================================

	virtual FName GetContainerName() const override { return FName("Project"); }
	virtual FName GetCategoryName() const override { return FName("HktGameplay"); }
	virtual FName GetSectionName() const override { return FName("HktGeneratorEditor"); }

	/** 싱글턴 접근 (읽기용) */
	static const UHktGeneratorEditorSettings* Get()
	{
		return GetDefault<UHktGeneratorEditorSettings>();
	}

	/** 싱글턴 접근 (수정용) */
	static UHktGeneratorEditorSettings* GetMutable()
	{
		return GetMutableDefault<UHktGeneratorEditorSettings>();
	}
};
