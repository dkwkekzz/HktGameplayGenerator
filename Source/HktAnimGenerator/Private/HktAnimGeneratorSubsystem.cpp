// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktAnimGeneratorSubsystem.h"
#include "HktAnimGeneratorSettings.h"
#include "HktAnimGeneratorHandler.h"
#include "HktGeneratorRouter.h"
#include "HktAssetSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/Skeleton.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Factories/AnimBlueprintFactory.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_Root.h"
#include "AnimationStateMachineGraph.h"
#include "AnimStateNode.h"
#include "AnimStateEntryNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_TwoWayBlend.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimationStateGraph.h"
#include "AnimationTransitionGraph.h"
#include "PackageTools.h"
#include "UObject/SavePackage.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/SkeletalMeshSocket.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktAnimGeneratorSubsystem, Log, All);

// ============================================================================
// JSON 헬퍼
// ============================================================================

FString UHktAnimGeneratorSubsystem::MakeErrorJson(const FString& Error)
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), false);
	Obj->SetStringField(TEXT("error"), Error);
	FString Out;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Obj, W);
	return Out;
}

FString UHktAnimGeneratorSubsystem::MakeSuccessJson(const TSharedRef<FJsonObject>& Data)
{
	Data->SetBoolField(TEXT("success"), true);
	FString Out;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Data, W);
	return Out;
}

static TSharedPtr<FJsonObject> ParseJson(const FString& JsonStr)
{
	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	FJsonSerializer::Deserialize(Reader, Obj);
	return Obj;
}

static void SaveAssetPackage(UObject* Asset)
{
	if (!Asset) return;
	UPackage* Pkg = Asset->GetOutermost();
	Pkg->MarkPackageDirty();
	FString PackageFileName = FPackageName::LongPackageNameToFilename(
		Pkg->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Pkg, Asset, *PackageFileName, SaveArgs);
}

// ============================================================================
// 초기화 / 해제
// ============================================================================

void UHktAnimGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UHktGeneratorRouter* Router = GEditor->GetEditorSubsystem<UHktGeneratorRouter>())
	{
		AnimHandler = NewObject<UHktAnimGeneratorHandler>(this);
		Router->RegisterHandler(TScriptInterface<IHktGeneratorHandler>(AnimHandler));
		UE_LOG(LogHktAnimGeneratorSubsystem, Log, TEXT("AnimGeneratorHandler registered with Router"));
	}

	UE_LOG(LogHktAnimGeneratorSubsystem, Log, TEXT("AnimGeneratorSubsystem initialized"));
}

void UHktAnimGeneratorSubsystem::Deinitialize()
{
	PendingRequests.Empty();
	Super::Deinitialize();
}

// ============================================================================
// ABP 헬퍼
// ============================================================================

UAnimBlueprint* UHktAnimGeneratorSubsystem::LoadAnimBlueprint(const FString& AssetPath) const
{
	UObject* Obj = StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *AssetPath);
	return Cast<UAnimBlueprint>(Obj);
}

UEdGraph* UHktAnimGeneratorSubsystem::FindAnimGraph(UAnimBlueprint* ABP) const
{
	if (!ABP) return nullptr;
	for (UEdGraph* Graph : ABP->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == FName(TEXT("AnimGraph")))
		{
			return Graph;
		}
	}
	return nullptr;
}

UAnimGraphNode_StateMachine* UHktAnimGeneratorSubsystem::FindStateMachineNode(
	UAnimBlueprint* ABP, const FString& MachineName) const
{
	UEdGraph* AnimGraph = FindAnimGraph(ABP);
	if (!AnimGraph) return nullptr;

	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (UAnimGraphNode_StateMachine* SM = Cast<UAnimGraphNode_StateMachine>(Node))
		{
			if (SM->EditorStateMachineGraph &&
				SM->EditorStateMachineGraph->GetName().Contains(MachineName))
			{
				return SM;
			}
			if (SM->GetNodeTitle(ENodeTitleType::EditableTitle).ToString().Contains(MachineName))
			{
				return SM;
			}
		}
	}
	return nullptr;
}

UAnimStateNode* UHktAnimGeneratorSubsystem::FindStateNode(
	UAnimationStateMachineGraph* SMGraph, const FString& StateName) const
{
	if (!SMGraph) return nullptr;
	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
		{
			if (StateNode->GetStateName().Equals(StateName))
			{
				return StateNode;
			}
		}
	}
	return nullptr;
}

// ============================================================================
// 기존 생성 API
// ============================================================================

FSoftObjectPath UHktAnimGeneratorSubsystem::RequestAnimGeneration(const FHktAnimIntent& Intent)
{
	FSoftObjectPath ConventionPath = ResolveConventionPath(Intent);

	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FAssetData AssetData = ARM.Get().GetAssetByObjectPath(ConventionPath);
	if (AssetData.IsValid())
	{
		UE_LOG(LogHktAnimGeneratorSubsystem, Log, TEXT("Animation already exists: %s"), *ConventionPath.ToString());
		return ConventionPath;
	}

	FHktAnimGenerationRequest Request;
	Request.Intent = Intent;
	Request.ConventionPath = ConventionPath;
	Request.GenerationPrompt = BuildPrompt(Intent);
	Request.ExpectedAssetType = DetermineExpectedAssetType(Intent);
	PendingRequests.Add(Request);

	UE_LOG(LogHktAnimGeneratorSubsystem, Log, TEXT("Animation generation pending: %s (%s)"), *ConventionPath.ToString(), *Request.ExpectedAssetType);
	return ConventionPath;
}

UObject* UHktAnimGeneratorSubsystem::ImportAnimFromFile(const FString& FilePath, const FString& DestinationPath)
{
	// TODO: Phase 2 — FBX animation -> UE5 AnimSequence import
	UE_LOG(LogHktAnimGeneratorSubsystem, Warning, TEXT("ImportAnimFromFile: Not yet implemented (Phase 2). File: %s"), *FilePath);
	return nullptr;
}

// ============================================================================
// 기존 MCP JSON API
// ============================================================================

FString UHktAnimGeneratorSubsystem::McpRequestAnimation(const FString& JsonIntent)
{
	TSharedPtr<FJsonObject> JsonObj = ParseJson(JsonIntent);
	if (!JsonObj.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FHktAnimIntent Intent;
	Intent.Layer = JsonObj->GetStringField(TEXT("layer"));
	Intent.Type = JsonObj->GetStringField(TEXT("type"));
	Intent.Name = JsonObj->GetStringField(TEXT("name"));

	FString SkeletonStr;
	if (JsonObj->TryGetStringField(TEXT("skeletonType"), SkeletonStr))
	{
		if (SkeletonStr == TEXT("Quadruped")) Intent.SkeletonType = EHktSkeletonType::Quadruped;
		else if (SkeletonStr == TEXT("Custom")) Intent.SkeletonType = EHktSkeletonType::Custom;
	}

	const TArray<TSharedPtr<FJsonValue>>* Keywords;
	if (JsonObj->TryGetArrayField(TEXT("styleKeywords"), Keywords))
	{
		for (const auto& Val : *Keywords)
		{
			Intent.StyleKeywords.Add(Val->AsString());
		}
	}

	FString TagStr = FString::Printf(TEXT("Anim.%s.%s.%s"), *Intent.Layer, *Intent.Type, *Intent.Name);
	Intent.AnimTag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);

	FSoftObjectPath Path = RequestAnimGeneration(Intent);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("layer"), Intent.Layer);
	Result->SetStringField(TEXT("type"), Intent.Type);
	Result->SetStringField(TEXT("name"), Intent.Name);
	Result->SetStringField(TEXT("conventionPath"), Path.ToString());
	Result->SetStringField(TEXT("prompt"), BuildPrompt(Intent));
	Result->SetStringField(TEXT("expectedAssetType"), DetermineExpectedAssetType(Intent));
	Result->SetBoolField(TEXT("pending"), true);

	return MakeSuccessJson(Result);
}

FString UHktAnimGeneratorSubsystem::McpImportAnimation(const FString& FilePath, const FString& JsonIntent)
{
	UObject* Imported = ImportAnimFromFile(FilePath);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	if (Imported)
	{
		Result->SetStringField(TEXT("assetPath"), Imported->GetPathName());
		return MakeSuccessJson(Result);
	}
	else
	{
		return MakeErrorJson(TEXT("Import not yet implemented (Phase 2)"));
	}
}

FString UHktAnimGeneratorSubsystem::McpGetPendingRequests()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Items;

	for (const FHktAnimGenerationRequest& Req : PendingRequests)
	{
		if (Req.bCompleted) continue;
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("layer"), Req.Intent.Layer);
		Item->SetStringField(TEXT("type"), Req.Intent.Type);
		Item->SetStringField(TEXT("name"), Req.Intent.Name);
		Item->SetStringField(TEXT("conventionPath"), Req.ConventionPath.ToString());
		Item->SetStringField(TEXT("prompt"), Req.GenerationPrompt);
		Item->SetStringField(TEXT("expectedAssetType"), Req.ExpectedAssetType);
		Items.Add(MakeShared<FJsonValueObject>(Item));
	}

	Root->SetArrayField(TEXT("pending"), Items);
	Root->SetNumberField(TEXT("count"), Items.Num());
	return MakeSuccessJson(Root);
}

FString UHktAnimGeneratorSubsystem::McpListGeneratedAnimations(const FString& Directory)
{
	FString SearchDir = Directory.IsEmpty() ? ResolveOutputDir(TEXT("")) : Directory;

	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*SearchDir));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UAnimMontage::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UBlendSpace::StaticClass()->GetClassPathName());

	TArray<FAssetData> Assets;
	ARM.Get().GetAssets(Filter, Assets);

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Items;

	for (const FAssetData& Asset : Assets)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		Item->SetStringField(TEXT("path"), Asset.GetSoftObjectPath().ToString());
		Item->SetStringField(TEXT("class"), Asset.AssetClassPath.ToString());
		Items.Add(MakeShared<FJsonValueObject>(Item));
	}

	Root->SetArrayField(TEXT("animations"), Items);
	Root->SetNumberField(TEXT("count"), Items.Num());
	return MakeSuccessJson(Root);
}

// ============================================================================
// Animation Blueprint API
// ============================================================================

FString UHktAnimGeneratorSubsystem::McpCreateAnimBlueprint(const FString& JsonConfig)
{
	TSharedPtr<FJsonObject> Cfg = ParseJson(JsonConfig);
	if (!Cfg.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FString Name = Cfg->GetStringField(TEXT("name"));
	FString PackagePath = Cfg->GetStringField(TEXT("packagePath"));
	FString SkeletonPath = Cfg->GetStringField(TEXT("skeletonPath"));

	if (Name.IsEmpty() || SkeletonPath.IsEmpty())
		return MakeErrorJson(TEXT("name and skeletonPath are required"));

	if (PackagePath.IsEmpty())
		PackagePath = ResolveOutputDir(TEXT("")) + TEXT("/AnimBP");

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
		return MakeErrorJson(FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));

	UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
	Factory->TargetSkeleton = Skeleton;

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	UObject* Created = AssetTools.CreateAsset(Name, PackagePath, UAnimBlueprint::StaticClass(), Factory);

	UAnimBlueprint* ABP = Cast<UAnimBlueprint>(Created);
	if (!ABP)
		return MakeErrorJson(TEXT("Failed to create AnimBlueprint"));

	SaveAssetPackage(ABP);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("assetPath"), ABP->GetPathName());
	Result->SetStringField(TEXT("name"), Name);
	Result->SetStringField(TEXT("skeleton"), SkeletonPath);
	return MakeSuccessJson(Result);
}

FString UHktAnimGeneratorSubsystem::McpGetAnimBlueprintInfo(const FString& AssetPath)
{
	UAnimBlueprint* ABP = LoadAnimBlueprint(AssetPath);
	if (!ABP)
		return MakeErrorJson(FString::Printf(TEXT("AnimBlueprint not found: %s"), *AssetPath));

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("assetPath"), ABP->GetPathName());
	Result->SetStringField(TEXT("skeleton"), ABP->TargetSkeleton ? ABP->TargetSkeleton->GetPathName() : TEXT("None"));

	// AnimGraph 노드 목록
	UEdGraph* AnimGraph = FindAnimGraph(ABP);
	TArray<TSharedPtr<FJsonValue>> NodeList;
	if (AnimGraph)
	{
		for (UEdGraphNode* Node : AnimGraph->Nodes)
		{
			TSharedRef<FJsonObject> NObj = MakeShared<FJsonObject>();
			NObj->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
			NObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
			NObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
			NObj->SetNumberField(TEXT("posX"), Node->NodePosX);
			NObj->SetNumberField(TEXT("posY"), Node->NodePosY);

			// State Machine 세부 정보
			if (UAnimGraphNode_StateMachine* SM = Cast<UAnimGraphNode_StateMachine>(Node))
			{
				if (SM->EditorStateMachineGraph)
				{
					TArray<TSharedPtr<FJsonValue>> States;
					for (UEdGraphNode* SMNode : SM->EditorStateMachineGraph->Nodes)
					{
						if (UAnimStateNode* StateN = Cast<UAnimStateNode>(SMNode))
						{
							TSharedRef<FJsonObject> SObj = MakeShared<FJsonObject>();
							SObj->SetStringField(TEXT("stateName"), StateN->GetStateName());
							SObj->SetStringField(TEXT("nodeId"), StateN->NodeGuid.ToString());
							States.Add(MakeShared<FJsonValueObject>(SObj));
						}
					}
					NObj->SetArrayField(TEXT("states"), States);
				}
			}

			// 핀 목록
			TArray<TSharedPtr<FJsonValue>> Pins;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				TSharedRef<FJsonObject> PObj = MakeShared<FJsonObject>();
				PObj->SetStringField(TEXT("pinName"), Pin->PinName.ToString());
				PObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
				PObj->SetBoolField(TEXT("connected"), Pin->LinkedTo.Num() > 0);
				Pins.Add(MakeShared<FJsonValueObject>(PObj));
			}
			NObj->SetArrayField(TEXT("pins"), Pins);

			NodeList.Add(MakeShared<FJsonValueObject>(NObj));
		}
	}
	Result->SetArrayField(TEXT("animGraphNodes"), NodeList);

	// 변수/파라미터 목록
	TArray<TSharedPtr<FJsonValue>> Params;
	for (const FBPVariableDescription& Var : ABP->NewVariables)
	{
		TSharedRef<FJsonObject> VObj = MakeShared<FJsonObject>();
		VObj->SetStringField(TEXT("name"), Var.VarName.ToString());
		VObj->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());
		Params.Add(MakeShared<FJsonValueObject>(VObj));
	}
	Result->SetArrayField(TEXT("parameters"), Params);

	return MakeSuccessJson(Result);
}

FString UHktAnimGeneratorSubsystem::McpCompileAnimBlueprint(const FString& AssetPath)
{
	UAnimBlueprint* ABP = LoadAnimBlueprint(AssetPath);
	if (!ABP)
		return MakeErrorJson(FString::Printf(TEXT("AnimBlueprint not found: %s"), *AssetPath));

	FKismetEditorUtilities::CompileBlueprint(ABP);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("assetPath"), AssetPath);
	Result->SetBoolField(TEXT("compiled"), true);
	Result->SetBoolField(TEXT("hasErrors"), ABP->Status == BS_Error);

	if (ABP->Status == BS_Error)
	{
		TArray<TSharedPtr<FJsonValue>> Errors;
		for (const UEdGraph* Graph : ABP->UbergraphPages)
		{
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node->bHasCompilerMessage && Node->ErrorType <= EMessageSeverity::Error)
				{
					Errors.Add(MakeShared<FJsonValueString>(Node->ErrorMsg));
				}
			}
		}
		if (Errors.Num() == 0)
		{
			Errors.Add(MakeShared<FJsonValueString>(TEXT("Compilation failed (no detailed message available)")));
		}
		Result->SetArrayField(TEXT("errors"), Errors);
	}

	SaveAssetPackage(ABP);
	return MakeSuccessJson(Result);
}

// ============================================================================
// State Machine API
// ============================================================================

FString UHktAnimGeneratorSubsystem::McpAddStateMachine(const FString& JsonConfig)
{
	TSharedPtr<FJsonObject> Cfg = ParseJson(JsonConfig);
	if (!Cfg.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FString AbpPath = Cfg->GetStringField(TEXT("abpPath"));
	FString MachineName = Cfg->GetStringField(TEXT("machineName"));

	UAnimBlueprint* ABP = LoadAnimBlueprint(AbpPath);
	if (!ABP) return MakeErrorJson(FString::Printf(TEXT("ABP not found: %s"), *AbpPath));

	UEdGraph* AnimGraph = FindAnimGraph(ABP);
	if (!AnimGraph) return MakeErrorJson(TEXT("AnimGraph not found in ABP"));

	// StateMachine 노드 생성
	UAnimGraphNode_StateMachine* SMNode = NewObject<UAnimGraphNode_StateMachine>(AnimGraph);
	SMNode->CreateNewGuid();
	SMNode->PostPlacedNewNode();
	AnimGraph->AddNode(SMNode, /*bFromUI=*/false, /*bSelectNewNode=*/false);

	// 이름 설정
	if (SMNode->EditorStateMachineGraph)
	{
		SMNode->EditorStateMachineGraph->Rename(*MachineName);
	}

	// 위치 설정
	int32 PosX = Cfg->HasField(TEXT("posX")) ? (int32)Cfg->GetNumberField(TEXT("posX")) : -300;
	int32 PosY = Cfg->HasField(TEXT("posY")) ? (int32)Cfg->GetNumberField(TEXT("posY")) : 0;
	SMNode->NodePosX = PosX;
	SMNode->NodePosY = PosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(ABP);
	SaveAssetPackage(ABP);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("nodeId"), SMNode->NodeGuid.ToString());
	Result->SetStringField(TEXT("machineName"), MachineName);
	Result->SetStringField(TEXT("abpPath"), AbpPath);
	return MakeSuccessJson(Result);
}

FString UHktAnimGeneratorSubsystem::McpAddState(const FString& JsonConfig)
{
	TSharedPtr<FJsonObject> Cfg = ParseJson(JsonConfig);
	if (!Cfg.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FString AbpPath = Cfg->GetStringField(TEXT("abpPath"));
	FString MachineName = Cfg->GetStringField(TEXT("machineName"));
	FString StateName = Cfg->GetStringField(TEXT("stateName"));
	FString AnimAssetPath;
	Cfg->TryGetStringField(TEXT("animAssetPath"), AnimAssetPath);

	UAnimBlueprint* ABP = LoadAnimBlueprint(AbpPath);
	if (!ABP) return MakeErrorJson(TEXT("ABP not found"));

	UAnimGraphNode_StateMachine* SMNode = FindStateMachineNode(ABP, MachineName);
	if (!SMNode || !SMNode->EditorStateMachineGraph)
		return MakeErrorJson(FString::Printf(TEXT("StateMachine '%s' not found"), *MachineName));

	UAnimationStateMachineGraph* SMGraph = SMNode->EditorStateMachineGraph;

	// State 노드 생성
	UAnimStateNode* StateNode = NewObject<UAnimStateNode>(SMGraph);
	StateNode->CreateNewGuid();
	StateNode->PostPlacedNewNode();
	SMGraph->AddNode(StateNode, false, false);

	// 이름 설정
	StateNode->Rename(*StateName);

	// 위치 설정
	int32 PosX = Cfg->HasField(TEXT("posX")) ? (int32)Cfg->GetNumberField(TEXT("posX")) : 200;
	int32 PosY = Cfg->HasField(TEXT("posY")) ? (int32)Cfg->GetNumberField(TEXT("posY")) : 0;
	StateNode->NodePosX = PosX;
	StateNode->NodePosY = PosY;

	// Entry 노드와 연결 (첫 번째 State인 경우)
	bool bIsFirstState = true;
	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (UAnimStateNode* Existing = Cast<UAnimStateNode>(Node))
		{
			if (Existing != StateNode)
			{
				bIsFirstState = false;
				break;
			}
		}
	}

	if (bIsFirstState && SMGraph->EntryNode)
	{
		// Entry -> State 연결
		UEdGraphPin* EntryOut = SMGraph->EntryNode->GetOutputPin();
		UEdGraphPin* StateIn = StateNode->GetInputPin();
		if (EntryOut && StateIn)
		{
			EntryOut->MakeLinkTo(StateIn);
		}
	}

	// 애니메이션 에셋이 지정된 경우 State 내부에 SequencePlayer 추가
	if (!AnimAssetPath.IsEmpty())
	{
		SetStateAnimationInternal(ABP, StateNode, AnimAssetPath);
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(ABP);
	SaveAssetPackage(ABP);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("nodeId"), StateNode->NodeGuid.ToString());
	Result->SetStringField(TEXT("stateName"), StateName);
	Result->SetBoolField(TEXT("isEntryState"), bIsFirstState);
	if (!AnimAssetPath.IsEmpty())
		Result->SetStringField(TEXT("animAsset"), AnimAssetPath);
	return MakeSuccessJson(Result);
}

FString UHktAnimGeneratorSubsystem::McpAddTransition(const FString& JsonConfig)
{
	TSharedPtr<FJsonObject> Cfg = ParseJson(JsonConfig);
	if (!Cfg.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FString AbpPath = Cfg->GetStringField(TEXT("abpPath"));
	FString MachineName = Cfg->GetStringField(TEXT("machineName"));
	FString FromState = Cfg->GetStringField(TEXT("fromState"));
	FString ToState = Cfg->GetStringField(TEXT("toState"));

	UAnimBlueprint* ABP = LoadAnimBlueprint(AbpPath);
	if (!ABP) return MakeErrorJson(TEXT("ABP not found"));

	UAnimGraphNode_StateMachine* SMNode = FindStateMachineNode(ABP, MachineName);
	if (!SMNode || !SMNode->EditorStateMachineGraph)
		return MakeErrorJson(TEXT("StateMachine not found"));

	UAnimationStateMachineGraph* SMGraph = SMNode->EditorStateMachineGraph;

	UAnimStateNode* SrcNode = FindStateNode(SMGraph, FromState);
	UAnimStateNode* DstNode = FindStateNode(SMGraph, ToState);
	if (!SrcNode) return MakeErrorJson(FString::Printf(TEXT("Source state '%s' not found"), *FromState));
	if (!DstNode) return MakeErrorJson(FString::Printf(TEXT("Target state '%s' not found"), *ToState));

	// Transition 노드 생성
	UAnimStateTransitionNode* TransNode = NewObject<UAnimStateTransitionNode>(SMGraph);
	TransNode->CreateNewGuid();
	TransNode->PostPlacedNewNode();
	SMGraph->AddNode(TransNode, false, false);

	// 위치
	TransNode->NodePosX = (SrcNode->NodePosX + DstNode->NodePosX) / 2;
	TransNode->NodePosY = (SrcNode->NodePosY + DstNode->NodePosY) / 2;

	// Source -> Transition -> Target 연결
	UEdGraphPin* SrcOut = SrcNode->GetOutputPin();
	UEdGraphPin* TransIn = TransNode->GetInputPin();
	UEdGraphPin* TransOut = TransNode->GetOutputPin();
	UEdGraphPin* DstIn = DstNode->GetInputPin();

	if (SrcOut && TransIn) SrcOut->MakeLinkTo(TransIn);
	if (TransOut && DstIn) TransOut->MakeLinkTo(DstIn);

	// 트랜지션 룰 설정
	const TSharedPtr<FJsonObject>* Rule;
	if (Cfg->TryGetObjectField(TEXT("transitionRule"), Rule))
	{
		FString RuleType = (*Rule)->GetStringField(TEXT("type"));

		if (RuleType == TEXT("automatic") || RuleType == TEXT("timeRemaining"))
		{
			TransNode->bAutomaticRuleBasedOnSequencePlayerInState = true;
			double CrossfadeDuration = 0.2;
			(*Rule)->TryGetNumberField(TEXT("crossfadeDuration"), CrossfadeDuration);
			TransNode->CrossfadeDuration = CrossfadeDuration;
		}
		// boolParam 등 더 복잡한 룰은 Transition Graph 내부 노드 조작 필요
		// 기본적으로 간단한 속성만 설정
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(ABP);
	SaveAssetPackage(ABP);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("nodeId"), TransNode->NodeGuid.ToString());
	Result->SetStringField(TEXT("from"), FromState);
	Result->SetStringField(TEXT("to"), ToState);
	return MakeSuccessJson(Result);
}

FString UHktAnimGeneratorSubsystem::McpConnectStateMachineToOutput(const FString& JsonConfig)
{
	TSharedPtr<FJsonObject> Cfg = ParseJson(JsonConfig);
	if (!Cfg.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FString AbpPath = Cfg->GetStringField(TEXT("abpPath"));
	FString MachineName = Cfg->GetStringField(TEXT("machineName"));

	UAnimBlueprint* ABP = LoadAnimBlueprint(AbpPath);
	if (!ABP) return MakeErrorJson(TEXT("ABP not found"));

	UEdGraph* AnimGraph = FindAnimGraph(ABP);
	if (!AnimGraph) return MakeErrorJson(TEXT("AnimGraph not found"));

	UAnimGraphNode_StateMachine* SMNode = FindStateMachineNode(ABP, MachineName);
	if (!SMNode) return MakeErrorJson(TEXT("StateMachine not found"));

	// OutputPose (Root) 노드 찾기
	UAnimGraphNode_Root* RootNode = nullptr;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		RootNode = Cast<UAnimGraphNode_Root>(Node);
		if (RootNode) break;
	}
	if (!RootNode) return MakeErrorJson(TEXT("AnimGraph Root node not found"));

	// SM 출력 핀 -> Root 입력 핀 연결
	UEdGraphPin* SMOutPin = nullptr;
	for (UEdGraphPin* Pin : SMNode->Pins)
	{
		if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
		{
			SMOutPin = Pin;
			break;
		}
	}

	UEdGraphPin* RootInPin = nullptr;
	for (UEdGraphPin* Pin : RootNode->Pins)
	{
		if (Pin->Direction == EGPD_Input)
		{
			RootInPin = Pin;
			break;
		}
	}

	if (!SMOutPin || !RootInPin)
		return MakeErrorJson(TEXT("Could not find compatible pins for connection"));

	// 기존 연결 제거
	RootInPin->BreakAllPinLinks();
	SMOutPin->MakeLinkTo(RootInPin);

	FBlueprintEditorUtils::MarkBlueprintAsModified(ABP);
	SaveAssetPackage(ABP);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("connected"), TEXT("StateMachine -> OutputPose"));
	return MakeSuccessJson(Result);
}

FString UHktAnimGeneratorSubsystem::McpSetStateAnimation(const FString& JsonConfig)
{
	TSharedPtr<FJsonObject> Cfg = ParseJson(JsonConfig);
	if (!Cfg.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FString AbpPath = Cfg->GetStringField(TEXT("abpPath"));
	FString MachineName = Cfg->GetStringField(TEXT("machineName"));
	FString StateName = Cfg->GetStringField(TEXT("stateName"));
	FString AnimAssetPath = Cfg->GetStringField(TEXT("animAssetPath"));

	UAnimBlueprint* ABP = LoadAnimBlueprint(AbpPath);
	if (!ABP) return MakeErrorJson(TEXT("ABP not found"));

	UAnimGraphNode_StateMachine* SMNode = FindStateMachineNode(ABP, MachineName);
	if (!SMNode || !SMNode->EditorStateMachineGraph)
		return MakeErrorJson(TEXT("StateMachine not found"));

	UAnimStateNode* StateNode = FindStateNode(SMNode->EditorStateMachineGraph, StateName);
	if (!StateNode) return MakeErrorJson(FString::Printf(TEXT("State '%s' not found"), *StateName));

	if (!SetStateAnimationInternal(ABP, StateNode, AnimAssetPath))
		return MakeErrorJson(FString::Printf(TEXT("Failed to set animation for state '%s'"), *StateName));

	FBlueprintEditorUtils::MarkBlueprintAsModified(ABP);
	SaveAssetPackage(ABP);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("stateName"), StateName);
	Result->SetStringField(TEXT("animAsset"), AnimAssetPath);
	return MakeSuccessJson(Result);
}

// ============================================================================
// AnimGraph Node API
// ============================================================================

FString UHktAnimGeneratorSubsystem::McpAddAnimGraphNode(const FString& JsonConfig)
{
	TSharedPtr<FJsonObject> Cfg = ParseJson(JsonConfig);
	if (!Cfg.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FString AbpPath = Cfg->GetStringField(TEXT("abpPath"));
	FString NodeType = Cfg->GetStringField(TEXT("nodeType"));
	int32 PosX = Cfg->HasField(TEXT("posX")) ? (int32)Cfg->GetNumberField(TEXT("posX")) : 0;
	int32 PosY = Cfg->HasField(TEXT("posY")) ? (int32)Cfg->GetNumberField(TEXT("posY")) : 0;

	UAnimBlueprint* ABP = LoadAnimBlueprint(AbpPath);
	if (!ABP) return MakeErrorJson(TEXT("ABP not found"));

	UEdGraph* AnimGraph = FindAnimGraph(ABP);
	if (!AnimGraph) return MakeErrorJson(TEXT("AnimGraph not found"));

	UEdGraphNode* NewNode = nullptr;

	if (NodeType == TEXT("SequencePlayer"))
	{
		auto* N = NewObject<UAnimGraphNode_SequencePlayer>(AnimGraph);
		N->CreateNewGuid();
		N->PostPlacedNewNode();

		FString AnimPath;
		if (Cfg->TryGetStringField(TEXT("animAssetPath"), AnimPath))
		{
			UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, *AnimPath);
			if (Seq)
			{
				N->Node.SetSequence(Seq);
			}
		}
		NewNode = N;
	}
	else if (NodeType == TEXT("BlendListByBool"))
	{
		auto* N = NewObject<UAnimGraphNode_BlendListByBool>(AnimGraph);
		N->CreateNewGuid();
		N->PostPlacedNewNode();
		NewNode = N;
	}
	else if (NodeType == TEXT("TwoWayBlend"))
	{
		auto* N = NewObject<UAnimGraphNode_TwoWayBlend>(AnimGraph);
		N->CreateNewGuid();
		N->PostPlacedNewNode();
		NewNode = N;
	}
	else if (NodeType == TEXT("LayeredBoneBlend"))
	{
		auto* N = NewObject<UAnimGraphNode_LayeredBoneBlend>(AnimGraph);
		N->CreateNewGuid();
		N->PostPlacedNewNode();
		NewNode = N;
	}
	else if (NodeType == TEXT("SaveCachedPose"))
	{
		auto* N = NewObject<UAnimGraphNode_SaveCachedPose>(AnimGraph);
		N->CreateNewGuid();
		N->PostPlacedNewNode();

		FString CacheName;
		if (Cfg->TryGetStringField(TEXT("cacheName"), CacheName))
		{
			N->CacheName = CacheName;
		}
		NewNode = N;
	}
	else if (NodeType == TEXT("UseCachedPose"))
	{
		auto* N = NewObject<UAnimGraphNode_UseCachedPose>(AnimGraph);
		N->CreateNewGuid();
		N->PostPlacedNewNode();
		NewNode = N;
	}
	else
	{
		return MakeErrorJson(FString::Printf(
			TEXT("Unknown nodeType '%s'. Supported: SequencePlayer, BlendListByBool, TwoWayBlend, LayeredBoneBlend, SaveCachedPose, UseCachedPose"),
			*NodeType));
	}

	if (NewNode)
	{
		AnimGraph->AddNode(NewNode, false, false);
		NewNode->NodePosX = PosX;
		NewNode->NodePosY = PosY;

		FBlueprintEditorUtils::MarkBlueprintAsModified(ABP);
		SaveAssetPackage(ABP);

		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
		Result->SetStringField(TEXT("nodeType"), NodeType);
		Result->SetNumberField(TEXT("posX"), PosX);
		Result->SetNumberField(TEXT("posY"), PosY);
		return MakeSuccessJson(Result);
	}

	return MakeErrorJson(TEXT("Failed to create node"));
}

FString UHktAnimGeneratorSubsystem::McpConnectAnimNodes(const FString& JsonConfig)
{
	TSharedPtr<FJsonObject> Cfg = ParseJson(JsonConfig);
	if (!Cfg.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FString AbpPath = Cfg->GetStringField(TEXT("abpPath"));
	FString SrcNodeId = Cfg->GetStringField(TEXT("sourceNodeId"));
	FString SrcPinName = Cfg->GetStringField(TEXT("sourcePinName"));
	FString TgtNodeId = Cfg->GetStringField(TEXT("targetNodeId"));
	FString TgtPinName = Cfg->GetStringField(TEXT("targetPinName"));

	UAnimBlueprint* ABP = LoadAnimBlueprint(AbpPath);
	if (!ABP) return MakeErrorJson(TEXT("ABP not found"));

	UEdGraph* AnimGraph = FindAnimGraph(ABP);
	if (!AnimGraph) return MakeErrorJson(TEXT("AnimGraph not found"));

	FGuid SrcGuid, TgtGuid;
	FGuid::Parse(SrcNodeId, SrcGuid);
	FGuid::Parse(TgtNodeId, TgtGuid);

	UEdGraphNode* SrcNode = nullptr;
	UEdGraphNode* TgtNode = nullptr;

	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (Node->NodeGuid == SrcGuid) SrcNode = Node;
		if (Node->NodeGuid == TgtGuid) TgtNode = Node;
	}

	if (!SrcNode) return MakeErrorJson(TEXT("Source node not found"));
	if (!TgtNode) return MakeErrorJson(TEXT("Target node not found"));

	UEdGraphPin* SrcPin = SrcNode->FindPin(FName(*SrcPinName));
	UEdGraphPin* TgtPin = TgtNode->FindPin(FName(*TgtPinName));

	if (!SrcPin) return MakeErrorJson(FString::Printf(TEXT("Source pin '%s' not found"), *SrcPinName));
	if (!TgtPin) return MakeErrorJson(FString::Printf(TEXT("Target pin '%s' not found"), *TgtPinName));

	SrcPin->MakeLinkTo(TgtPin);
	bool bConnected = SrcPin->LinkedTo.Contains(TgtPin);

	FBlueprintEditorUtils::MarkBlueprintAsModified(ABP);
	SaveAssetPackage(ABP);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("connected"), bConnected);
	Result->SetStringField(TEXT("source"), SrcNodeId + TEXT(".") + SrcPinName);
	Result->SetStringField(TEXT("target"), TgtNodeId + TEXT(".") + TgtPinName);
	return MakeSuccessJson(Result);
}

FString UHktAnimGeneratorSubsystem::McpAddAnimParameter(const FString& JsonConfig)
{
	TSharedPtr<FJsonObject> Cfg = ParseJson(JsonConfig);
	if (!Cfg.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FString AbpPath = Cfg->GetStringField(TEXT("abpPath"));
	FString ParamName = Cfg->GetStringField(TEXT("paramName"));
	FString ParamType = Cfg->GetStringField(TEXT("paramType"));
	FString DefaultValue;
	Cfg->TryGetStringField(TEXT("defaultValue"), DefaultValue);

	UAnimBlueprint* ABP = LoadAnimBlueprint(AbpPath);
	if (!ABP) return MakeErrorJson(TEXT("ABP not found"));

	FEdGraphPinType PinType;
	if (ParamType == TEXT("bool"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (ParamType == TEXT("float"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	}
	else if (ParamType == TEXT("int"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else
	{
		return MakeErrorJson(FString::Printf(TEXT("Unknown paramType '%s'. Supported: bool, float, int"), *ParamType));
	}

	bool bAdded = FBlueprintEditorUtils::AddMemberVariable(ABP, FName(*ParamName), PinType);
	if (!bAdded)
		return MakeErrorJson(FString::Printf(TEXT("Failed to add parameter '%s' (may already exist)"), *ParamName));

	// 기본값 설정
	if (!DefaultValue.IsEmpty())
	{
		for (FBPVariableDescription& Var : ABP->NewVariables)
		{
			if (Var.VarName == FName(*ParamName))
			{
				Var.DefaultValue = DefaultValue;
				break;
			}
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(ABP);
	SaveAssetPackage(ABP);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("paramName"), ParamName);
	Result->SetStringField(TEXT("paramType"), ParamType);
	if (!DefaultValue.IsEmpty()) Result->SetStringField(TEXT("defaultValue"), DefaultValue);
	return MakeSuccessJson(Result);
}

// ============================================================================
// Montage API
// ============================================================================

FString UHktAnimGeneratorSubsystem::McpCreateMontage(const FString& JsonConfig)
{
	TSharedPtr<FJsonObject> Cfg = ParseJson(JsonConfig);
	if (!Cfg.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FString Name = Cfg->GetStringField(TEXT("name"));
	FString PackagePath = Cfg->GetStringField(TEXT("packagePath"));

	if (Name.IsEmpty())
		return MakeErrorJson(TEXT("name is required"));

	if (PackagePath.IsEmpty())
		PackagePath = ResolveOutputDir(TEXT("")) + TEXT("/Montages");

	// 다중 AnimSequence 지원: "animSequences" 배열 또는 단일 "animSequencePath"
	struct FAnimEntry
	{
		FString SectionName;
		FString Path;
		UAnimSequence* Seq = nullptr;
	};
	TArray<FAnimEntry> AnimEntries;

	const TArray<TSharedPtr<FJsonValue>>* AnimSequencesArr;
	if (Cfg->TryGetArrayField(TEXT("animSequences"), AnimSequencesArr))
	{
		// 다중 모드: [{ "name": "Jab", "path": "/Game/.../Anim" }, ...]
		for (const auto& Val : *AnimSequencesArr)
		{
			TSharedPtr<FJsonObject> Obj = Val->AsObject();
			if (!Obj.IsValid()) continue;

			FAnimEntry Entry;
			Entry.SectionName = Obj->GetStringField(TEXT("name"));
			Entry.Path = Obj->GetStringField(TEXT("path"));
			Entry.Seq = LoadObject<UAnimSequence>(nullptr, *Entry.Path);
			if (!Entry.Seq)
			{
				UE_LOG(LogHktAnimGeneratorSubsystem, Warning, TEXT("AnimSequence not found: %s"), *Entry.Path);
				continue;
			}
			AnimEntries.Add(Entry);
		}
	}
	else
	{
		// 기존 단일 모드: "animSequencePath"
		FString AnimSequencePath = Cfg->GetStringField(TEXT("animSequencePath"));
		if (AnimSequencePath.IsEmpty())
			return MakeErrorJson(TEXT("animSequencePath or animSequences is required"));

		FAnimEntry Entry;
		Entry.Path = AnimSequencePath;
		Entry.Seq = LoadObject<UAnimSequence>(nullptr, *AnimSequencePath);
		if (!Entry.Seq)
			return MakeErrorJson(FString::Printf(TEXT("AnimSequence not found: %s"), *AnimSequencePath));
		AnimEntries.Add(Entry);
	}

	if (AnimEntries.Num() == 0)
		return MakeErrorJson(TEXT("No valid AnimSequences found"));

	// 패키지 생성
	FString FullPath = PackagePath / Name;
	UPackage* Pkg = CreatePackage(*FullPath);
	if (!Pkg) return MakeErrorJson(TEXT("Failed to create package"));

	UAnimMontage* Montage = NewObject<UAnimMontage>(Pkg, FName(*Name), RF_Public | RF_Standalone);
	Montage->SetSkeleton(AnimEntries[0].Seq->GetSkeleton());

	// SlotAnimTrack 설정
	if (Montage->SlotAnimTracks.Num() == 0)
	{
		FSlotAnimationTrack NewTrack;
		NewTrack.SlotName = FName(TEXT("DefaultSlot"));
		Montage->SlotAnimTracks.Add(NewTrack);
	}

	FString SlotName;
	if (Cfg->TryGetStringField(TEXT("slotName"), SlotName))
	{
		Montage->SlotAnimTracks[0].SlotName = FName(*SlotName);
	}

	// AnimSegments 순차 배치 + Section 자동 생성
	float CurrentPos = 0.0f;
	TSharedRef<FJsonObject> ResultSections = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> SectionArray;

	for (int32 i = 0; i < AnimEntries.Num(); ++i)
	{
		const FAnimEntry& Entry = AnimEntries[i];
		float PlayLength = Entry.Seq->GetPlayLength();

		// AnimSegment 추가
		FAnimSegment Segment;
		Segment.SetAnimReference(Entry.Seq);
		Segment.AnimStartTime = 0.0f;
		Segment.AnimEndTime = PlayLength;
		Segment.AnimPlayRate = 1.0f;
		Segment.StartPos = CurrentPos;
		Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Add(Segment);

		// Section 자동 생성 (다중 모드에서 이름이 있으면)
		if (!Entry.SectionName.IsEmpty())
		{
			int32 Idx = Montage->AddAnimCompositeSection(FName(*Entry.SectionName), CurrentPos);
			UE_LOG(LogHktAnimGeneratorSubsystem, Log, TEXT("Montage section: %s at %.3f (idx=%d)"), *Entry.SectionName, CurrentPos, Idx);

			TSharedRef<FJsonObject> SecInfo = MakeShared<FJsonObject>();
			SecInfo->SetStringField(TEXT("name"), Entry.SectionName);
			SecInfo->SetNumberField(TEXT("startTime"), CurrentPos);
			SecInfo->SetNumberField(TEXT("duration"), PlayLength);
			SectionArray.Add(MakeShared<FJsonValueObject>(SecInfo));
		}

		CurrentPos += PlayLength;
	}

	// 전체 길이 설정
	Montage->UpdateLinkableElements();

	// 수동 Section 추가 (기존 호환)
	const TArray<TSharedPtr<FJsonValue>>* Sections;
	if (Cfg->TryGetArrayField(TEXT("sections"), Sections))
	{
		for (const auto& SecVal : *Sections)
		{
			TSharedPtr<FJsonObject> SecObj = SecVal->AsObject();
			if (!SecObj.IsValid()) continue;

			FString SectionName = SecObj->GetStringField(TEXT("name"));
			double StartTime = SecObj->GetNumberField(TEXT("startTime"));

			int32 Idx = Montage->AddAnimCompositeSection(FName(*SectionName), StartTime);
			UE_LOG(LogHktAnimGeneratorSubsystem, Log, TEXT("Added montage section: %s at %f (idx=%d)"), *SectionName, StartTime, Idx);
		}
	}

	SaveAssetPackage(Montage);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("assetPath"), Montage->GetPathName());
	Result->SetStringField(TEXT("name"), Name);
	Result->SetNumberField(TEXT("duration"), Montage->GetPlayLength());
	Result->SetNumberField(TEXT("segmentCount"), AnimEntries.Num());
	if (SectionArray.Num() > 0)
	{
		Result->SetArrayField(TEXT("sections"), SectionArray);
	}
	return MakeSuccessJson(Result);
}

FString UHktAnimGeneratorSubsystem::McpAddMontageSection(const FString& JsonConfig)
{
	TSharedPtr<FJsonObject> Cfg = ParseJson(JsonConfig);
	if (!Cfg.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FString MontagePath = Cfg->GetStringField(TEXT("montagePath"));
	FString SectionName = Cfg->GetStringField(TEXT("sectionName"));
	double StartTime = Cfg->GetNumberField(TEXT("startTime"));

	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
	if (!Montage) return MakeErrorJson(FString::Printf(TEXT("Montage not found: %s"), *MontagePath));

	int32 Idx = Montage->AddAnimCompositeSection(FName(*SectionName), StartTime);
	SaveAssetPackage(Montage);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("sectionName"), SectionName);
	Result->SetNumberField(TEXT("startTime"), StartTime);
	Result->SetNumberField(TEXT("sectionIndex"), Idx);
	return MakeSuccessJson(Result);
}

FString UHktAnimGeneratorSubsystem::McpSetMontageSlot(const FString& AssetPath, const FString& SlotName)
{
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *AssetPath);
	if (!Montage) return MakeErrorJson(FString::Printf(TEXT("Montage not found: %s"), *AssetPath));

	if (Montage->SlotAnimTracks.Num() > 0)
	{
		Montage->SlotAnimTracks[0].SlotName = FName(*SlotName);
	}
	SaveAssetPackage(Montage);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("slotName"), SlotName);
	return MakeSuccessJson(Result);
}

FString UHktAnimGeneratorSubsystem::McpLinkMontageSections(const FString& JsonConfig)
{
	TSharedPtr<FJsonObject> Cfg = ParseJson(JsonConfig);
	if (!Cfg.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FString MontagePath = Cfg->GetStringField(TEXT("montagePath"));
	FString FromSection = Cfg->GetStringField(TEXT("fromSection"));
	FString ToSection = Cfg->GetStringField(TEXT("toSection"));

	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
	if (!Montage) return MakeErrorJson(TEXT("Montage not found"));

	int32 FromIdx = Montage->GetSectionIndex(FName(*FromSection));
	int32 ToIdx = Montage->GetSectionIndex(FName(*ToSection));

	if (FromIdx == INDEX_NONE) return MakeErrorJson(FString::Printf(TEXT("Section '%s' not found"), *FromSection));
	if (ToIdx == INDEX_NONE) return MakeErrorJson(FString::Printf(TEXT("Section '%s' not found"), *ToSection));

	// CompositeSections 배열에서 직접 NextSectionName 설정
	for (FCompositeSection& Section : Montage->CompositeSections)
	{
		if (Section.SectionName == FName(*FromSection))
		{
			Section.NextSectionName = FName(*ToSection);
			break;
		}
	}
	SaveAssetPackage(Montage);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("from"), FromSection);
	Result->SetStringField(TEXT("to"), ToSection);
	return MakeSuccessJson(Result);
}

// ============================================================================
// BlendSpace API
// ============================================================================

FString UHktAnimGeneratorSubsystem::McpCreateBlendSpace(const FString& JsonConfig)
{
	TSharedPtr<FJsonObject> Cfg = ParseJson(JsonConfig);
	if (!Cfg.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FString Name = Cfg->GetStringField(TEXT("name"));
	FString PackagePath = Cfg->GetStringField(TEXT("packagePath"));
	FString SkeletonPath = Cfg->GetStringField(TEXT("skeletonPath"));

	if (Name.IsEmpty() || SkeletonPath.IsEmpty())
		return MakeErrorJson(TEXT("name and skeletonPath are required"));

	if (PackagePath.IsEmpty())
		PackagePath = ResolveOutputDir(TEXT("")) + TEXT("/BlendSpaces");

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
		return MakeErrorJson(FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));

	// Y축이 있으면 2D, 없으면 1D
	bool bIs2D = Cfg->HasField(TEXT("axisY"));

	FString FullPath = PackagePath / Name;
	UPackage* Pkg = CreatePackage(*FullPath);
	if (!Pkg) return MakeErrorJson(TEXT("Failed to create package"));

	UBlendSpace* BS = nullptr;
	if (bIs2D)
	{
		BS = NewObject<UBlendSpace>(Pkg, FName(*Name), RF_Public | RF_Standalone);
	}
	else
	{
		BS = NewObject<UBlendSpace1D>(Pkg, FName(*Name), RF_Public | RF_Standalone);
	}
	BS->SetSkeleton(Skeleton);

	// X축 설정
	const TSharedPtr<FJsonObject>* AxisX;
	if (Cfg->TryGetObjectField(TEXT("axisX"), AxisX))
	{
		FString AxisName;
		(*AxisX)->TryGetStringField(TEXT("name"), AxisName);
		double Min = 0, Max = 100;
		(*AxisX)->TryGetNumberField(TEXT("min"), Min);
		(*AxisX)->TryGetNumberField(TEXT("max"), Max);

		FBlendParameter& Param0 = const_cast<FBlendParameter&>(BS->GetBlendParameter(0));
		Param0.DisplayName = AxisName;
		Param0.Min = Min;
		Param0.Max = Max;
	}

	// Y축 설정 (2D only)
	if (bIs2D)
	{
		const TSharedPtr<FJsonObject>* AxisY;
		if (Cfg->TryGetObjectField(TEXT("axisY"), AxisY))
		{
			FString AxisName;
			(*AxisY)->TryGetStringField(TEXT("name"), AxisName);
			double Min = -180, Max = 180;
			(*AxisY)->TryGetNumberField(TEXT("min"), Min);
			(*AxisY)->TryGetNumberField(TEXT("max"), Max);

			FBlendParameter& Param1 = const_cast<FBlendParameter&>(BS->GetBlendParameter(1));
			Param1.DisplayName = AxisName;
			Param1.Min = Min;
			Param1.Max = Max;
		}
	}

	// 초기 샘플 추가
	const TArray<TSharedPtr<FJsonValue>>* Samples;
	if (Cfg->TryGetArrayField(TEXT("samples"), Samples))
	{
		for (const auto& SVal : *Samples)
		{
			TSharedPtr<FJsonObject> SObj = SVal->AsObject();
			if (!SObj.IsValid()) continue;

			FString AnimPath = SObj->GetStringField(TEXT("animPath"));
			double X = SObj->GetNumberField(TEXT("x"));
			double Y = 0;
			SObj->TryGetNumberField(TEXT("y"), Y);

			UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, *AnimPath);
			if (Seq)
			{
				BS->AddSample(Seq, FVector(X, Y, 0));
			}
		}
	}

	SaveAssetPackage(BS);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("assetPath"), BS->GetPathName());
	Result->SetStringField(TEXT("name"), Name);
	Result->SetBoolField(TEXT("is2D"), bIs2D);
	return MakeSuccessJson(Result);
}

FString UHktAnimGeneratorSubsystem::McpAddBlendSpaceSample(const FString& JsonConfig)
{
	TSharedPtr<FJsonObject> Cfg = ParseJson(JsonConfig);
	if (!Cfg.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FString BSPath = Cfg->GetStringField(TEXT("blendSpacePath"));
	FString AnimPath = Cfg->GetStringField(TEXT("animPath"));
	double X = Cfg->GetNumberField(TEXT("x"));
	double Y = 0;
	Cfg->TryGetNumberField(TEXT("y"), Y);

	UBlendSpace* BS = LoadObject<UBlendSpace>(nullptr, *BSPath);
	if (!BS) return MakeErrorJson(TEXT("BlendSpace not found"));

	UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, *AnimPath);
	if (!Seq) return MakeErrorJson(TEXT("AnimSequence not found"));

	BS->AddSample(Seq, FVector(X, Y, 0));
	SaveAssetPackage(BS);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("animPath"), AnimPath);
	Result->SetNumberField(TEXT("x"), X);
	Result->SetNumberField(TEXT("y"), Y);
	return MakeSuccessJson(Result);
}

FString UHktAnimGeneratorSubsystem::McpSetBlendSpaceAxis(const FString& JsonConfig)
{
	TSharedPtr<FJsonObject> Cfg = ParseJson(JsonConfig);
	if (!Cfg.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FString BSPath = Cfg->GetStringField(TEXT("blendSpacePath"));
	FString Axis = Cfg->GetStringField(TEXT("axis"));
	FString AxisName;
	Cfg->TryGetStringField(TEXT("name"), AxisName);
	double Min = 0, Max = 100;
	Cfg->TryGetNumberField(TEXT("min"), Min);
	Cfg->TryGetNumberField(TEXT("max"), Max);

	UBlendSpace* BS = LoadObject<UBlendSpace>(nullptr, *BSPath);
	if (!BS) return MakeErrorJson(TEXT("BlendSpace not found"));

	int32 AxisIdx = (Axis == TEXT("Y") || Axis == TEXT("y")) ? 1 : 0;

	FBlendParameter& Param = const_cast<FBlendParameter&>(BS->GetBlendParameter(AxisIdx));
	if (!AxisName.IsEmpty()) Param.DisplayName = AxisName;
	Param.Min = Min;
	Param.Max = Max;
	SaveAssetPackage(BS);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("axis"), Axis);
	Result->SetStringField(TEXT("name"), AxisName);
	Result->SetNumberField(TEXT("min"), Min);
	Result->SetNumberField(TEXT("max"), Max);
	return MakeSuccessJson(Result);
}

// ============================================================================
// Skeleton API
// ============================================================================

FString UHktAnimGeneratorSubsystem::McpGetSkeletonInfo(const FString& SkeletonPath)
{
	USkeleton* Skel = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skel) return MakeErrorJson(FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("assetPath"), Skel->GetPathName());

	const FReferenceSkeleton& RefSkel = Skel->GetReferenceSkeleton();

	// 본 목록
	TArray<TSharedPtr<FJsonValue>> Bones;
	for (int32 i = 0; i < RefSkel.GetNum(); ++i)
	{
		TSharedRef<FJsonObject> BObj = MakeShared<FJsonObject>();
		BObj->SetStringField(TEXT("name"), RefSkel.GetBoneName(i).ToString());
		BObj->SetNumberField(TEXT("index"), i);
		BObj->SetNumberField(TEXT("parentIndex"), RefSkel.GetParentIndex(i));
		if (RefSkel.GetParentIndex(i) >= 0)
		{
			BObj->SetStringField(TEXT("parentName"), RefSkel.GetBoneName(RefSkel.GetParentIndex(i)).ToString());
		}
		Bones.Add(MakeShared<FJsonValueObject>(BObj));
	}
	Result->SetArrayField(TEXT("bones"), Bones);
	Result->SetNumberField(TEXT("boneCount"), RefSkel.GetNum());

	// 소켓 목록
	TArray<TSharedPtr<FJsonValue>> Sockets;
	for (USkeletalMeshSocket* Socket : Skel->Sockets)
	{
		TSharedRef<FJsonObject> SObj = MakeShared<FJsonObject>();
		SObj->SetStringField(TEXT("socketName"), Socket->SocketName.ToString());
		SObj->SetStringField(TEXT("boneName"), Socket->BoneName.ToString());

		TSharedRef<FJsonObject> Loc = MakeShared<FJsonObject>();
		Loc->SetNumberField(TEXT("x"), Socket->RelativeLocation.X);
		Loc->SetNumberField(TEXT("y"), Socket->RelativeLocation.Y);
		Loc->SetNumberField(TEXT("z"), Socket->RelativeLocation.Z);
		SObj->SetObjectField(TEXT("relativeLocation"), Loc);

		TSharedRef<FJsonObject> Rot = MakeShared<FJsonObject>();
		Rot->SetNumberField(TEXT("pitch"), Socket->RelativeRotation.Pitch);
		Rot->SetNumberField(TEXT("yaw"), Socket->RelativeRotation.Yaw);
		Rot->SetNumberField(TEXT("roll"), Socket->RelativeRotation.Roll);
		SObj->SetObjectField(TEXT("relativeRotation"), Rot);

		Sockets.Add(MakeShared<FJsonValueObject>(SObj));
	}
	Result->SetArrayField(TEXT("sockets"), Sockets);

	// Virtual Bone 목록
	TArray<TSharedPtr<FJsonValue>> VBones;
	for (const FVirtualBone& VB : Skel->GetVirtualBones())
	{
		TSharedRef<FJsonObject> VObj = MakeShared<FJsonObject>();
		VObj->SetStringField(TEXT("virtualBoneName"), VB.VirtualBoneName.ToString());
		VObj->SetStringField(TEXT("sourceBone"), VB.SourceBoneName.ToString());
		VObj->SetStringField(TEXT("targetBone"), VB.TargetBoneName.ToString());
		VBones.Add(MakeShared<FJsonValueObject>(VObj));
	}
	Result->SetArrayField(TEXT("virtualBones"), VBones);

	return MakeSuccessJson(Result);
}

FString UHktAnimGeneratorSubsystem::McpAddSocket(const FString& JsonConfig)
{
	TSharedPtr<FJsonObject> Cfg = ParseJson(JsonConfig);
	if (!Cfg.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FString SkeletonPath = Cfg->GetStringField(TEXT("skeletonPath"));
	FString BoneName = Cfg->GetStringField(TEXT("boneName"));
	FString SocketName = Cfg->GetStringField(TEXT("socketName"));

	USkeleton* Skel = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skel) return MakeErrorJson(TEXT("Skeleton not found"));

	// 이미 존재하는지 확인
	if (Skel->FindSocket(FName(*SocketName)))
		return MakeErrorJson(FString::Printf(TEXT("Socket '%s' already exists"), *SocketName));

	USkeletalMeshSocket* NewSocket = NewObject<USkeletalMeshSocket>(Skel);
	NewSocket->SocketName = FName(*SocketName);
	NewSocket->BoneName = FName(*BoneName);

	const TSharedPtr<FJsonObject>* LocObj;
	if (Cfg->TryGetObjectField(TEXT("relativeLocation"), LocObj))
	{
		NewSocket->RelativeLocation.X = (*LocObj)->GetNumberField(TEXT("x"));
		NewSocket->RelativeLocation.Y = (*LocObj)->GetNumberField(TEXT("y"));
		NewSocket->RelativeLocation.Z = (*LocObj)->GetNumberField(TEXT("z"));
	}

	const TSharedPtr<FJsonObject>* RotObj;
	if (Cfg->TryGetObjectField(TEXT("relativeRotation"), RotObj))
	{
		NewSocket->RelativeRotation.Pitch = (*RotObj)->GetNumberField(TEXT("pitch"));
		NewSocket->RelativeRotation.Yaw = (*RotObj)->GetNumberField(TEXT("yaw"));
		NewSocket->RelativeRotation.Roll = (*RotObj)->GetNumberField(TEXT("roll"));
	}

	const TSharedPtr<FJsonObject>* ScaleObj;
	if (Cfg->TryGetObjectField(TEXT("relativeScale"), ScaleObj))
	{
		NewSocket->RelativeScale.X = (*ScaleObj)->GetNumberField(TEXT("x"));
		NewSocket->RelativeScale.Y = (*ScaleObj)->GetNumberField(TEXT("y"));
		NewSocket->RelativeScale.Z = (*ScaleObj)->GetNumberField(TEXT("z"));
	}

	Skel->Sockets.Add(NewSocket);
	SaveAssetPackage(Skel);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("socketName"), SocketName);
	Result->SetStringField(TEXT("boneName"), BoneName);
	return MakeSuccessJson(Result);
}

FString UHktAnimGeneratorSubsystem::McpAddVirtualBone(const FString& JsonConfig)
{
	TSharedPtr<FJsonObject> Cfg = ParseJson(JsonConfig);
	if (!Cfg.IsValid()) return MakeErrorJson(TEXT("Invalid JSON"));

	FString SkeletonPath = Cfg->GetStringField(TEXT("skeletonPath"));
	FString SourceBone = Cfg->GetStringField(TEXT("sourceBone"));
	FString TargetBone = Cfg->GetStringField(TEXT("targetBone"));
	FString VBoneName = Cfg->GetStringField(TEXT("virtualBoneName"));

	USkeleton* Skel = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skel) return MakeErrorJson(TEXT("Skeleton not found"));

	FName OutName;
	bool bAdded = Skel->AddNewVirtualBone(FName(*SourceBone), FName(*TargetBone), OutName);
	if (!bAdded)
		return MakeErrorJson(TEXT("Failed to add virtual bone (check bone names)"));

	// 이름 변경
	if (!VBoneName.IsEmpty())
	{
		Skel->RenameVirtualBone(OutName, FName(*VBoneName));
		OutName = FName(*VBoneName);
	}

	SaveAssetPackage(Skel);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("virtualBoneName"), OutName.ToString());
	Result->SetStringField(TEXT("sourceBone"), SourceBone);
	Result->SetStringField(TEXT("targetBone"), TargetBone);
	return MakeSuccessJson(Result);
}

// ============================================================================
// Prompt Guide API
// ============================================================================

FString UHktAnimGeneratorSubsystem::McpGetAnimApiGuide()
{
	FString Guide = TEXT(R"JSON({
  "animationApiGuide": {
    "overview": "HktAnimGenerator MCP API - Animation Blueprint, State Machine, Montage, BlendSpace, Skeleton 관리",
    "workflows": {
      "createAnimBlueprintWithStateMachine": {
        "description": "ABP 생성 후 State Machine으로 Locomotion 설정하는 전체 워크플로우",
        "steps": [
          "1. create_anim_blueprint - ABP 생성 (skeleton 지정)",
          "2. add_anim_parameter - 필요한 파라미터 추가 (bIsRunning, Speed 등)",
          "3. add_state_machine - AnimGraph에 State Machine 추가",
          "4. add_state - 각 상태 추가 (Idle, Walk, Run 등 + 애니메이션 지정)",
          "5. add_transition - 상태 간 전환 규칙 추가",
          "6. connect_state_machine_to_output - SM을 OutputPose에 연결",
          "7. compile_anim_blueprint - 컴파일"
        ]
      },
      "createMontage": {
        "description": "AnimSequence에서 Montage 생성 및 섹션 설정",
        "steps": [
          "1. create_montage - AnimSequence 기반 Montage 생성",
          "2. add_montage_section - 섹션 추가 (Windup, Impact, Recovery 등)",
          "3. link_montage_sections - 섹션 간 링크 설정",
          "4. set_montage_slot - 슬롯 이름 설정"
        ]
      },
      "createBlendSpace": {
        "description": "Locomotion용 BlendSpace 생성",
        "steps": [
          "1. create_blend_space - BS 생성 (1D/2D, 축 설정)",
          "2. add_blend_space_sample - 샘플 포인트 추가"
        ]
      }
    },
    "tools": {
      "abp": [
        {"name": "create_anim_blueprint", "params": {"name": "string", "packagePath": "string?", "skeletonPath": "string"}},
        {"name": "get_anim_blueprint_info", "params": {"assetPath": "string"}},
        {"name": "compile_anim_blueprint", "params": {"assetPath": "string"}}
      ],
      "stateMachine": [
        {"name": "add_state_machine", "params": {"abpPath": "string", "machineName": "string", "posX?": "int", "posY?": "int"}},
        {"name": "add_state", "params": {"abpPath": "string", "machineName": "string", "stateName": "string", "animAssetPath?": "string", "posX?": "int", "posY?": "int"}},
        {"name": "add_transition", "params": {"abpPath": "string", "machineName": "string", "fromState": "string", "toState": "string", "transitionRule?": {"type": "automatic|timeRemaining|boolParam", "crossfadeDuration?": "float", "paramName?": "string"}}},
        {"name": "connect_state_machine_to_output", "params": {"abpPath": "string", "machineName": "string"}},
        {"name": "set_state_animation", "params": {"abpPath": "string", "machineName": "string", "stateName": "string", "animAssetPath": "string"}}
      ],
      "animGraph": [
        {"name": "add_anim_graph_node", "params": {"abpPath": "string", "nodeType": "SequencePlayer|BlendListByBool|TwoWayBlend|LayeredBoneBlend|SaveCachedPose|UseCachedPose", "posX?": "int", "posY?": "int", "animAssetPath?": "string", "cacheName?": "string"}},
        {"name": "connect_anim_nodes", "params": {"abpPath": "string", "sourceNodeId": "guid", "sourcePinName": "string", "targetNodeId": "guid", "targetPinName": "string"}},
        {"name": "add_anim_parameter", "params": {"abpPath": "string", "paramName": "string", "paramType": "bool|float|int", "defaultValue?": "string"}}
      ],
      "montage": [
        {"name": "create_montage", "params": {"name": "string", "packagePath?": "string", "animSequencePath": "string", "slotName?": "string", "sections?": [{"name": "string", "startTime": "float"}]}},
        {"name": "add_montage_section", "params": {"montagePath": "string", "sectionName": "string", "startTime": "float"}},
        {"name": "set_montage_slot", "params": {"assetPath": "string", "slotName": "string"}},
        {"name": "link_montage_sections", "params": {"montagePath": "string", "fromSection": "string", "toSection": "string"}}
      ],
      "blendSpace": [
        {"name": "create_blend_space", "params": {"name": "string", "packagePath?": "string", "skeletonPath": "string", "axisX": {"name": "string", "min": "float", "max": "float"}, "axisY?": {"name": "string", "min": "float", "max": "float"}, "samples?": [{"animPath": "string", "x": "float", "y?": "float"}]}},
        {"name": "add_blend_space_sample", "params": {"blendSpacePath": "string", "animPath": "string", "x": "float", "y?": "float"}},
        {"name": "set_blend_space_axis", "params": {"blendSpacePath": "string", "axis": "X|Y", "name": "string", "min": "float", "max": "float"}}
      ],
      "skeleton": [
        {"name": "get_skeleton_info", "params": {"skeletonPath": "string"}},
        {"name": "add_socket", "params": {"skeletonPath": "string", "boneName": "string", "socketName": "string", "relativeLocation?": {"x":"float","y":"float","z":"float"}, "relativeRotation?": {"pitch":"float","yaw":"float","roll":"float"}, "relativeScale?": {"x":"float","y":"float","z":"float"}}},
        {"name": "add_virtual_bone", "params": {"skeletonPath": "string", "sourceBone": "string", "targetBone": "string", "virtualBoneName": "string"}}
      ]
    }
  }
})JSON");
	return Guide;
}

// ============================================================================
// SetStateAnimationInternal — State 내부에 SequencePlayer 추가
// ============================================================================

bool UHktAnimGeneratorSubsystem::SetStateAnimationInternal(
	UAnimBlueprint* ABP, UAnimStateNode* StateNode, const FString& AnimAssetPath)
{
	if (!StateNode || !ABP) return false;

	UAnimSequenceBase* AnimAsset = LoadObject<UAnimSequenceBase>(nullptr, *AnimAssetPath);
	if (!AnimAsset)
	{
		UE_LOG(LogHktAnimGeneratorSubsystem, Warning, TEXT("Animation asset not found: %s"), *AnimAssetPath);
		return false;
	}

	// State 내부 BoundGraph 가져오기
	UEdGraph* StateGraph = StateNode->GetBoundGraph();
	if (!StateGraph)
	{
		UE_LOG(LogHktAnimGeneratorSubsystem, Warning, TEXT("State bound graph not found for: %s"), *StateNode->GetStateName());
		return false;
	}

	// 기존 SequencePlayer 노드가 있으면 제거
	TArray<UAnimGraphNode_SequencePlayer*> ExistingPlayers;
	StateGraph->GetNodesOfClass(ExistingPlayers);
	for (UAnimGraphNode_SequencePlayer* Existing : ExistingPlayers)
	{
		StateGraph->RemoveNode(Existing);
	}

	// 새 SequencePlayer 노드 생성
	UAnimGraphNode_SequencePlayer* SeqPlayer = NewObject<UAnimGraphNode_SequencePlayer>(StateGraph);
	SeqPlayer->CreateNewGuid();
	SeqPlayer->PostPlacedNewNode();
	StateGraph->AddNode(SeqPlayer, false, false);
	SeqPlayer->NodePosX = -200;
	SeqPlayer->NodePosY = 0;

	// 애니메이션 설정
	if (UAnimSequence* AnimSeq = Cast<UAnimSequence>(AnimAsset))
	{
		SeqPlayer->Node.SetSequence(AnimSeq);
	}

	// SequencePlayer 출력 -> State Result 노드 연결
	// State Graph의 Result 노드 찾기
	for (UEdGraphNode* Node : StateGraph->Nodes)
	{
		// AnimationStateGraph의 Result 노드에 연결
		if (Node->GetClass()->GetName().Contains(TEXT("Result")))
		{
			UEdGraphPin* ResultIn = nullptr;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->Direction == EGPD_Input)
				{
					ResultIn = Pin;
					break;
				}
			}

			UEdGraphPin* SeqOut = nullptr;
			for (UEdGraphPin* Pin : SeqPlayer->Pins)
			{
				if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
				{
					SeqOut = Pin;
					break;
				}
			}

			if (ResultIn && SeqOut)
			{
				ResultIn->BreakAllPinLinks();
				SeqOut->MakeLinkTo(ResultIn);
			}
			break;
		}
	}

	return true;
}

// ============================================================================
// Private 헬퍼
// ============================================================================

FString UHktAnimGeneratorSubsystem::ResolveOutputDir(const FString& OutputDir) const
{
	if (!OutputDir.IsEmpty()) return OutputDir;
	const UHktAnimGeneratorSettings* Settings = UHktAnimGeneratorSettings::Get();
	return Settings ? Settings->DefaultOutputDirectory : TEXT("/Game/Generated/Animations");
}

FString UHktAnimGeneratorSubsystem::BuildPrompt(const FHktAnimIntent& Intent) const
{
	const UHktAnimGeneratorSettings* Settings = UHktAnimGeneratorSettings::Get();

	FString Prompt = FString::Printf(TEXT("Create animation: %s %s %s"), *Intent.Layer, *Intent.Type, *Intent.Name);

	switch (Intent.SkeletonType)
	{
	case EHktSkeletonType::Humanoid:  Prompt += TEXT(", humanoid character"); break;
	case EHktSkeletonType::Quadruped: Prompt += TEXT(", quadruped creature"); break;
	case EHktSkeletonType::Custom:    Prompt += TEXT(", custom rig"); break;
	}

	if (Intent.Layer == TEXT("FullBody"))       Prompt += TEXT(", full body animation");
	else if (Intent.Layer == TEXT("UpperBody")) Prompt += TEXT(", upper body only, lower body idle");
	else if (Intent.Layer == TEXT("Montage"))   Prompt += TEXT(", one-shot montage, clear start/end poses");

	if (Intent.Type == TEXT("Locomotion"))      Prompt += TEXT(", loopable, smooth cycle");
	else if (Intent.Type == TEXT("Combat"))     Prompt += TEXT(", impactful, clear anticipation and follow-through");

	if (Intent.StyleKeywords.Num() > 0)
		Prompt += TEXT(", ") + FString::Join(Intent.StyleKeywords, TEXT(", "));

	if (Settings && !Settings->AnimStylePromptSuffix.IsEmpty())
		Prompt += TEXT(", ") + Settings->AnimStylePromptSuffix;

	return Prompt;
}

FSoftObjectPath UHktAnimGeneratorSubsystem::ResolveConventionPath(const FHktAnimIntent& Intent) const
{
	if (Intent.AnimTag.IsValid())
	{
		FSoftObjectPath SettingsPath = UHktAssetSubsystem::ResolveConventionPath(Intent.AnimTag);
		if (SettingsPath.IsValid()) return SettingsPath;
	}

	FString OutputDir = ResolveOutputDir(TEXT(""));
	FString TagPath = FString::Printf(TEXT("Anim_%s_%s_%s"), *Intent.Layer, *Intent.Type, *Intent.Name);
	FString AssetPath = FString::Printf(TEXT("%s/%s"), *OutputDir, *TagPath);
	return FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *AssetPath, *TagPath));
}

FString UHktAnimGeneratorSubsystem::DetermineExpectedAssetType(const FHktAnimIntent& Intent)
{
	if (Intent.Type == TEXT("Locomotion"))
	{
		const UHktAnimGeneratorSettings* Settings = UHktAnimGeneratorSettings::Get();
		if (Settings && Settings->bGenerateBlendSpaceForLocomotion)
			return TEXT("BlendSpace");
	}

	if (Intent.Layer == TEXT("Montage") || Intent.Layer == TEXT("UpperBody"))
		return TEXT("AnimMontage");

	return TEXT("AnimSequence");
}
