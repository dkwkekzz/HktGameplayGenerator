// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktClaudeProcess.h"
#include "HktGeneratorEditorModule.h"
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
	// 1. PATH에서 claude 탐색
	FString ClaudePath = FPlatformProcess::ExecutablePath();
	// 'which claude' 대신 알려진 경로들을 직접 확인
	TArray<FString> SearchPaths;

#if PLATFORM_WINDOWS
	// Windows: npm global, AppData
	FString AppData = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
	SearchPaths.Add(FPaths::Combine(AppData, TEXT("npm"), TEXT("claude.cmd")));
	SearchPaths.Add(FPaths::Combine(AppData, TEXT(".."), TEXT("Local"), TEXT(".claude"), TEXT("local"), TEXT("claude.exe")));
	// fnm / nvm 등
	FString UserProfile = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
	SearchPaths.Add(FPaths::Combine(UserProfile, TEXT(".claude"), TEXT("local"), TEXT("claude.exe")));
#else
	// Linux/Mac
	SearchPaths.Add(TEXT("/usr/local/bin/claude"));
	SearchPaths.Add(TEXT("/usr/bin/claude"));
	FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
	SearchPaths.Add(FPaths::Combine(Home, TEXT(".claude"), TEXT("local"), TEXT("claude")));
	SearchPaths.Add(FPaths::Combine(Home, TEXT(".npm-global"), TEXT("bin"), TEXT("claude")));
	// nvm node path
	SearchPaths.Add(FPaths::Combine(Home, TEXT(".nvm"), TEXT("versions"), TEXT("node")));
#endif

	for (const FString& Path : SearchPaths)
	{
		if (FPaths::FileExists(Path))
		{
			UE_LOG(LogHktGenEditor, Log, TEXT("Found claude CLI at: %s"), *Path);
			return Path;
		}
	}

	// Fallback: 단순히 "claude" — PATH에 있기를 기대
	UE_LOG(LogHktGenEditor, Warning, TEXT("Claude CLI not found in known paths, falling back to 'claude'"));
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
		WritePipe,// PipeWriteChild (child의 stdout이 여기로)
		ReadPipe  // PipeReadChild
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
