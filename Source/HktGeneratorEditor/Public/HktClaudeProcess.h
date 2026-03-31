// Copyright Hkt Studios, Inc. All Rights Reserved.
// Claude Code CLI subprocess 래퍼 — stream-json 출력을 비동기로 파싱

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformProcess.h"
#include "Containers/Queue.h"

DECLARE_DELEGATE_OneParam(FOnClaudeOutput, const FString& /* JsonLine */);
DECLARE_DELEGATE_OneParam(FOnClaudeComplete, int32 /* ExitCode */);
DECLARE_DELEGATE_OneParam(FOnClaudeError, const FString& /* ErrorMsg */);

/**
 * FHktClaudeProcess
 *
 * claude CLI를 subprocess로 실행하고, stream-json 출력을 줄 단위로
 * GameThread에 디스패치하는 비동기 래퍼.
 *
 * Usage:
 *   auto Proc = MakeShared<FHktClaudeProcess>();
 *   Proc->OnOutput.BindLambda([](const FString& Line){ ... });
 *   Proc->OnComplete.BindLambda([](int32 Code){ ... });
 *   Proc->Start(Prompt, SystemPrompt);
 *   // 매 프레임 Tick() 호출 필요 (FTickerDelegate 등)
 */
class HKTGENERATOREDITOR_API FHktClaudeProcess : public TSharedFromThis<FHktClaudeProcess>
{
public:
	FHktClaudeProcess();
	~FHktClaudeProcess();

	/**
	 * claude CLI 경로를 탐색.
	 * 순서: Settings → 환경변수 → 알려진 경로 → PATH
	 */
	static FString FindClaudeCLI();

	/**
	 * Settings를 무시하고 자동 탐색만 수행.
	 * Connection Manager에서 auto-detect 결과 표시용.
	 */
	static FString AutoDetectClaudeCLI();

	/**
	 * CLI 프로세스를 시작.
	 * @param Prompt        사용자 프롬프트 (Intent JSON 등)
	 * @param SystemPrompt  시스템 프롬프트 (SKILL.md 내용)
	 * @param WorkingDir    작업 디렉토리 (비어있으면 프로젝트 루트)
	 * @return 시작 성공 여부
	 */
	bool Start(const FString& Prompt, const FString& SystemPrompt, const FString& WorkingDir = TEXT(""));

	/** 실행 중인 프로세스를 취소 */
	void Cancel();

	/**
	 * GameThread에서 매 프레임 호출.
	 * 버퍼된 출력을 OnOutput 델리게이트로 디스패치.
	 */
	void Tick();

	/** 프로세스가 실행 중인지 확인 */
	bool IsRunning() const;

	// Delegates
	FOnClaudeOutput OnOutput;
	FOnClaudeComplete OnComplete;
	FOnClaudeError OnError;

private:
	/** stdout 읽기 스레드 */
	class FReadRunnable : public FRunnable
	{
	public:
		FReadRunnable(FHktClaudeProcess* InOwner, void* InReadPipe);
		virtual uint32 Run() override;
		virtual void Stop() override;

	private:
		FHktClaudeProcess* Owner;
		void* ReadPipe;
		TAtomic<bool> bStopping;
	};

	FProcHandle ProcessHandle;
	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;

	TUniquePtr<FReadRunnable> ReadRunnable;
	TUniquePtr<FRunnableThread> ReadThread;

	/** 파싱된 JSON 라인 큐 — ReadThread → GameThread */
	TQueue<FString, EQueueMode::Mpsc> PendingLines;

	/** 완료 플래그 — ReadThread가 설정 */
	TAtomic<bool> bReadThreadDone;
	TAtomic<bool> bCancelled;
	TAtomic<bool> bStarted;

	/** 미완성 줄 버퍼 (ReadThread 내부) */
	FString LineBuffer;

	/** 줄 파싱 후 큐에 추가 (ReadThread에서 호출) */
	void EnqueueLine(const FString& Line);

	/** 파이프/스레드 정리 */
	void Cleanup();
};
