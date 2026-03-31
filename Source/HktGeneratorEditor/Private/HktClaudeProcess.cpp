// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktClaudeProcess.h"
#include "HktGeneratorEditorModule.h"
#include "HktGeneratorEditorSettings.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"

// ==================== FReadRunnable ====================

FHktClaudeProcess::FReadRunnable::FReadRunnable(FHktClaudeProcess* InOwner, void* InReadPipe)
	: Owner(InOwner)
	, ReadPipe(InReadPipe)
	, bStopping(false)
{
}

uint32 FHktClaudeProcess::FReadRunnable::Run()
{
	while (!bStopping)
	{
		FString Output = FPlatformProcess::ReadPipe(ReadPipe);
		if (Output.IsEmpty())
		{
			// 파이프가 닫혔거나 프로세스 종료
			if (!FPlatformProcess::IsProcRunning(Owner->ProcessHandle))
			{
				break;
			}
			FPlatformProcess::Sleep(0.01f);
			continue;
		}

		// 줄 단위로 분리
		Owner->LineBuffer += Output;

		int32 NewlineIdx;
		while (Owner->LineBuffer.FindChar(TEXT('\n'), NewlineIdx))
		{
			FString Line = Owner->LineBuffer.Left(NewlineIdx).TrimEnd();
			Owner->LineBuffer.RightChopInline(NewlineIdx + 1);

			if (!Line.IsEmpty())
			{
				Owner->EnqueueLine(Line);
			}
		}
	}

	// 남은 버퍼 처리
	FString Remaining = Owner->LineBuffer.TrimStartAndEnd();
	if (!Remaining.IsEmpty())
	{
		Owner->EnqueueLine(Remaining);
	}
	Owner->LineBuffer.Empty();

	Owner->bReadThreadDone.Store(true);
	return 0;
}

void FHktClaudeProcess::FReadRunnable::Stop()
{
	bStopping.Store(true);
}

// ==================== FHktClaudeProcess ====================

FHktClaudeProcess::FHktClaudeProcess()
	: bReadThreadDone(false)
	, bCancelled(false)
	, bStarted(false)
{
}

FHktClaudeProcess::~FHktClaudeProcess()
{
	Cancel();
	Cleanup();
}

FString FHktClaudeProcess::FindClaudeCLI()
{
	// 0) Project Settings 확인
	const UHktGeneratorEditorSettings* Settings = UHktGeneratorEditorSettings::Get();
	if (Settings && !Settings->ClaudeCLIPath.IsEmpty())
	{
		if (FPaths::FileExists(Settings->ClaudeCLIPath))
		{
			UE_LOG(LogHktGenEditor, Log, TEXT("Found claude CLI via settings: %s"), *Settings->ClaudeCLIPath);
			return Settings->ClaudeCLIPath;
		}
		UE_LOG(LogHktGenEditor, Warning, TEXT("Settings CLI path not found: %s"), *Settings->ClaudeCLIPath);
	}

	return AutoDetectClaudeCLI();
}

FString FHktClaudeProcess::AutoDetectClaudeCLI()
{
	// 1) 환경변수 확인
	FString EnvCLI = FPlatformMisc::GetEnvironmentVariable(TEXT("HKT_CLAUDE_CLI"));
	if (!EnvCLI.IsEmpty() && FPaths::FileExists(EnvCLI))
	{
		UE_LOG(LogHktGenEditor, Log, TEXT("Found claude CLI via HKT_CLAUDE_CLI: %s"), *EnvCLI);
		return EnvCLI;
	}

	// 2) 알려진 경로들을 직접 확인
	TArray<FString> SearchPaths;

#if PLATFORM_WINDOWS
	FString UserProfile = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
	FString AppData = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
	FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));

	// 공식 Claude 설치 경로
	SearchPaths.Add(FPaths::Combine(UserProfile, TEXT(".claude"), TEXT("local"), TEXT("claude.exe")));
	SearchPaths.Add(FPaths::Combine(LocalAppData, TEXT(".claude"), TEXT("local"), TEXT("claude.exe")));
	SearchPaths.Add(FPaths::Combine(LocalAppData, TEXT("Programs"), TEXT("claude"), TEXT("claude.exe")));
	// npm global
	SearchPaths.Add(FPaths::Combine(AppData, TEXT("npm"), TEXT("claude.cmd")));
	// scoop
	SearchPaths.Add(FPaths::Combine(UserProfile, TEXT("scoop"), TEXT("shims"), TEXT("claude.cmd")));
	SearchPaths.Add(FPaths::Combine(UserProfile, TEXT("scoop"), TEXT("shims"), TEXT("claude.exe")));
	// Program Files
	SearchPaths.Add(TEXT("C:\\Program Files\\Claude\\claude.exe"));
#else
	// Linux/Mac
	FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
	SearchPaths.Add(FPaths::Combine(Home, TEXT(".claude"), TEXT("local"), TEXT("claude")));
	SearchPaths.Add(TEXT("/usr/local/bin/claude"));
	SearchPaths.Add(TEXT("/usr/bin/claude"));
	SearchPaths.Add(FPaths::Combine(Home, TEXT(".npm-global"), TEXT("bin"), TEXT("claude")));
	SearchPaths.Add(FPaths::Combine(Home, TEXT(".local"), TEXT("bin"), TEXT("claude")));
#endif

	for (const FString& Path : SearchPaths)
	{
		if (FPaths::FileExists(Path))
		{
			UE_LOG(LogHktGenEditor, Log, TEXT("Found claude CLI at: %s"), *Path);
			return Path;
		}
	}

	// 3) PATH에서 검색 (where/which)
#if PLATFORM_WINDOWS
	FString WhichExe = TEXT("C:\\Windows\\System32\\where.exe");
	FString WhichArgs = TEXT("claude");
#else
	FString WhichExe = TEXT("/usr/bin/which");
	FString WhichArgs = TEXT("claude");
#endif

	FString WhichOutput;
	int32 WhichReturnCode = -1;
	if (FPaths::FileExists(WhichExe))
	{
		FPlatformProcess::ExecProcess(*WhichExe, *WhichArgs, &WhichReturnCode, &WhichOutput, nullptr);
		if (WhichReturnCode == 0)
		{
			// 첫 번째 줄만 사용 (where는 여러 줄 반환 가능)
			FString FoundPath = WhichOutput.TrimStartAndEnd();
			int32 NewlineIdx;
			if (FoundPath.FindChar(TEXT('\n'), NewlineIdx))
			{
				FoundPath.LeftInline(NewlineIdx);
				FoundPath.TrimEndInline();
			}
			if (!FoundPath.IsEmpty() && FPaths::FileExists(FoundPath))
			{
				UE_LOG(LogHktGenEditor, Log, TEXT("Found claude CLI via PATH: %s"), *FoundPath);
				return FoundPath;
			}
		}
	}

	// 4) Fallback
	UE_LOG(LogHktGenEditor, Warning, TEXT("Claude CLI not found. Set HKT_CLAUDE_CLI environment variable or install Claude Code CLI."));
	return TEXT("claude");
}

bool FHktClaudeProcess::Start(const FString& Prompt, const FString& SystemPrompt, const FString& WorkingDir)
{
	if (bStarted.Load())
	{
		UE_LOG(LogHktGenEditor, Warning, TEXT("Claude process already started"));
		return false;
	}

	FString ClaudeCLI = FindClaudeCLI();

	// 프롬프트를 임시 파일로 저장 (커맨드라인 길이 제한 회피)
	FString TempDir = FPaths::ProjectSavedDir() / TEXT("HktGenerator");
	IFileManager::Get().MakeDirectory(*TempDir, true);

	// 시스템 프롬프트 + 유저 프롬프트를 하나의 파일로 합침
	// claude CLI에 파일 기반 플래그가 없으므로, stdin 파이프로 전달
	// 시스템 프롬프트는 프롬프트 상단에 ## System Instructions로 구분
	FString CombinedPromptPath = FPaths::ConvertRelativePathToFull(TempDir / TEXT("prompt.txt"));
	FString CombinedContent = TEXT("## System Instructions\n\n")
		+ SystemPrompt
		+ TEXT("\n\n---\n\n## User Request\n\n")
		+ Prompt;
	FFileHelper::SaveStringToFile(CombinedContent, *CombinedPromptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	FString ActualWorkingDir = WorkingDir;
	if (ActualWorkingDir.IsEmpty())
	{
		ActualWorkingDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	}

	// 임시 쉘 스크립트를 생성하여 파일 내용을 stdin으로 파이프
	// 이 방식은 쉘 이스케이프 문제를 완전히 회피
	FString ScriptPath;
	FString ShellExe;
	FString ShellArgs;

#if PLATFORM_WINDOWS
	ScriptPath = FPaths::ConvertRelativePathToFull(TempDir / TEXT("run_claude.bat"));
	FString ScriptContent = FString::Printf(
		TEXT("@echo off\r\ntype \"%s\" | \"%s\" --print --output-format stream-json\r\n"),
		*CombinedPromptPath, *ClaudeCLI);
	FFileHelper::SaveStringToFile(ScriptContent, *ScriptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	ShellExe = TEXT("cmd.exe");
	ShellArgs = FString::Printf(TEXT("/c \"\"%s\"\""), *ScriptPath);
#else
	ScriptPath = FPaths::ConvertRelativePathToFull(TempDir / TEXT("run_claude.sh"));
	FString ScriptContent = FString::Printf(
		TEXT("#!/bin/sh\nexec cat \"%s\" | \"%s\" --print --output-format stream-json\n"),
		*CombinedPromptPath, *ClaudeCLI);
	FFileHelper::SaveStringToFile(ScriptContent, *ScriptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	// 실행 권한 부여
	FPlatformProcess::ExecProcess(TEXT("/bin/chmod"), *FString::Printf(TEXT("+x \"%s\""), *ScriptPath), nullptr, nullptr, nullptr);
	ShellExe = TEXT("/bin/sh");
	ShellArgs = FString::Printf(TEXT("\"%s\""), *ScriptPath);
#endif

	UE_LOG(LogHktGenEditor, Log, TEXT("Starting claude via shell script: %s"), *ScriptPath);
	UE_LOG(LogHktGenEditor, Log, TEXT("Working directory: %s"), *ActualWorkingDir);

	// 파이프 생성
	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

	ProcessHandle = FPlatformProcess::CreateProc(
		*ShellExe,
		*ShellArgs,
		false,    // bLaunchDetached
		true,     // bLaunchHidden
		true,     // bLaunchReallyHidden
		nullptr,  // OutProcessID
		0,        // PriorityModifier
		*ActualWorkingDir,
		WritePipe // PipeWriteChild — child stdout이 여기로 쓰임, ReadPipe로 읽음
	);

	if (!ProcessHandle.IsValid())
	{
		UE_LOG(LogHktGenEditor, Error, TEXT("Failed to start claude CLI process"));
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		ReadPipe = nullptr;
		WritePipe = nullptr;
		return false;
	}

	bStarted.Store(true);
	bCancelled.Store(false);
	bReadThreadDone.Store(false);

	// stdout 읽기 스레드 시작
	ReadRunnable = MakeUnique<FReadRunnable>(this, ReadPipe);
	ReadThread = TUniquePtr<FRunnableThread>(FRunnableThread::Create(
		ReadRunnable.Get(),
		TEXT("HktClaudeReadThread"),
		0,
		TPri_BelowNormal
	));

	UE_LOG(LogHktGenEditor, Log, TEXT("Claude CLI process started successfully"));
	return true;
}

void FHktClaudeProcess::Cancel()
{
	if (!bStarted.Load() || bCancelled.Load())
	{
		return;
	}

	bCancelled.Store(true);

	if (ReadRunnable)
	{
		ReadRunnable->Stop();
	}

	if (ProcessHandle.IsValid() && FPlatformProcess::IsProcRunning(ProcessHandle))
	{
		FPlatformProcess::TerminateProc(ProcessHandle, true);
		UE_LOG(LogHktGenEditor, Log, TEXT("Claude CLI process cancelled"));
	}
}

void FHktClaudeProcess::Tick()
{
	if (!bStarted.Load())
	{
		return;
	}

	// 큐에서 라인을 소비하여 OnOutput 호출
	FString Line;
	while (PendingLines.Dequeue(Line))
	{
		OnOutput.ExecuteIfBound(Line);
	}

	// 프로세스 완료 체크
	if (bReadThreadDone.Load())
	{
		// 스레드 정리
		if (ReadThread)
		{
			ReadThread->WaitForCompletion();
			ReadThread.Reset();
		}
		ReadRunnable.Reset();

		int32 ExitCode = -1;
		if (ProcessHandle.IsValid())
		{
			FPlatformProcess::GetProcReturnCode(ProcessHandle, &ExitCode);
		}

		Cleanup();
		bStarted.Store(false);

		if (bCancelled.Load())
		{
			OnError.ExecuteIfBound(TEXT("Process cancelled by user"));
		}
		else if (ExitCode != 0)
		{
			OnError.ExecuteIfBound(FString::Printf(TEXT("Claude CLI exited with code %d"), ExitCode));
		}

		OnComplete.ExecuteIfBound(ExitCode);
	}
}

bool FHktClaudeProcess::IsRunning() const
{
	return bStarted.Load() && !bReadThreadDone.Load();
}

void FHktClaudeProcess::EnqueueLine(const FString& Line)
{
	PendingLines.Enqueue(Line);
}

void FHktClaudeProcess::Cleanup()
{
	if (ProcessHandle.IsValid())
	{
		FPlatformProcess::CloseProc(ProcessHandle);
		ProcessHandle.Reset();
	}

	if (ReadPipe || WritePipe)
	{
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		ReadPipe = nullptr;
		WritePipe = nullptr;
	}
}
