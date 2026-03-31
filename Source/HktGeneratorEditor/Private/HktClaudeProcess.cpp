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
	const int32 BufferSize = 4096;
	char Buffer[4096];

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

	FString PromptFilePath = TempDir / TEXT("prompt.txt");
	FString SystemPromptFilePath = TempDir / TEXT("system_prompt.txt");

	FFileHelper::SaveStringToFile(Prompt, *PromptFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	FFileHelper::SaveStringToFile(SystemPrompt, *SystemPromptFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	// 커맨드라인 조립
	// claude --print --output-format stream-json --system-prompt "$(cat file)" -p "$(cat file)"
	// 파일 기반으로 전달하기 위해 stdin 대신 파일 경로 사용
	FString Args = FString::Printf(
		TEXT("--print --output-format stream-json --system-prompt-file \"%s\" --prompt-file \"%s\""),
		*SystemPromptFilePath,
		*PromptFilePath
	);

	FString ActualWorkingDir = WorkingDir;
	if (ActualWorkingDir.IsEmpty())
	{
		// 프로젝트 루트의 상위 (McpServer 등이 있는 플러그인 디렉토리)
		ActualWorkingDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	}

	UE_LOG(LogHktGenEditor, Log, TEXT("Starting claude CLI: %s %s"), *ClaudeCLI, *Args);
	UE_LOG(LogHktGenEditor, Log, TEXT("Working directory: %s"), *ActualWorkingDir);

	// 파이프 생성
	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

	ProcessHandle = FPlatformProcess::CreateProc(
		*ClaudeCLI,
		*Args,
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
