#include "HktMcpEditorSubsystem.h"
#include "HktMcpBridgeEditorModule.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "LevelEditor.h"
#include "LevelEditorSubsystem.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "FileHelpers.h"
#include "Selection.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "SLevelViewport.h"
#include "EditorViewportClient.h"
#include "IPythonScriptPlugin.h"
#include "UObject/SavePackage.h"
#include "Factories/DataAssetFactory.h"

void UHktMcpEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogHktMcpEditor, Log, TEXT("HktMcpEditorSubsystem Initialized - Ready for Python MCP Server"));
}

void UHktMcpEditorSubsystem::Deinitialize()
{
	UE_LOG(LogHktMcpEditor, Log, TEXT("HktMcpEditorSubsystem Deinitialized"));
	Super::Deinitialize();
}

// ==================== Asset Tools ====================

TArray<FHktAssetInfo> UHktMcpEditorSubsystem::ListAssets(const FString& Path, const FString& ClassFilter)
{
	TArray<FHktAssetInfo> Result;
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByPath(FName(*Path), AssetDataList, true);

	for (const FAssetData& AssetData : AssetDataList)
	{
		if (!ClassFilter.IsEmpty() && !AssetData.AssetClassPath.GetAssetName().ToString().Contains(ClassFilter))
		{
			continue;
		}

		FHktAssetInfo Info;
		Info.AssetPath = AssetData.GetObjectPathString();
		Info.AssetName = AssetData.AssetName.ToString();
		Info.AssetClass = AssetData.AssetClassPath.GetAssetName().ToString();
		Info.PackagePath = AssetData.PackagePath.ToString();
		Result.Add(Info);
	}

	return Result;
}

FString UHktMcpEditorSubsystem::GetAssetDetails(const FString& AssetPath)
{
	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		return TEXT("{\"error\": \"Asset not found\"}");
	}

	TSharedRef<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(TEXT("name"), Asset->GetName());
	JsonObject->SetStringField(TEXT("class"), Asset->GetClass()->GetName());
	JsonObject->SetStringField(TEXT("path"), AssetPath);
	JsonObject->SetStringField(TEXT("package"), Asset->GetOutermost()->GetName());

	// 클래스별 추가 정보
	if (UBlueprint* BP = Cast<UBlueprint>(Asset))
	{
		JsonObject->SetStringField(TEXT("parent_class"), BP->ParentClass ? BP->ParentClass->GetName() : TEXT("None"));
		JsonObject->SetStringField(TEXT("blueprint_type"), UEnum::GetValueAsString(BP->BlueprintType));
	}

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(JsonObject, Writer);
	return Output;
}

TArray<FHktAssetInfo> UHktMcpEditorSubsystem::SearchAssets(const FString& SearchQuery, const FString& ClassFilter)
{
	TArray<FHktAssetInfo> Result;
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(FName("/Game"));

	if (!ClassFilter.IsEmpty())
	{
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), FName(*ClassFilter)));
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		if (AssetData.AssetName.ToString().Contains(SearchQuery))
		{
			FHktAssetInfo Info;
			Info.AssetPath = AssetData.GetObjectPathString();
			Info.AssetName = AssetData.AssetName.ToString();
			Info.AssetClass = AssetData.AssetClassPath.GetAssetName().ToString();
			Info.PackagePath = AssetData.PackagePath.ToString();
			Result.Add(Info);
		}
	}

	return Result;
}

bool UHktMcpEditorSubsystem::CreateDataAsset(const FString& AssetPath, const FString& ParentClassName)
{
	// 클래스 검색
	UClass* ParentClass = FindObject<UClass>(nullptr, *ParentClassName);
	if (!ParentClass)
	{
		ParentClass = LoadObject<UClass>(nullptr, *ParentClassName);
	}
	if (!ParentClass)
	{
		UE_LOG(LogHktMcpEditor, Error, TEXT("CreateDataAsset: Class not found: %s"), *ParentClassName);
		return false;
	}

	if (!ParentClass->IsChildOf(UDataAsset::StaticClass()))
	{
		UE_LOG(LogHktMcpEditor, Error, TEXT("CreateDataAsset: %s is not a DataAsset subclass"), *ParentClassName);
		return false;
	}

	// 패키지 경로와 에셋 이름 분리
	FString PackagePath, AssetName;
	AssetPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (AssetName.IsEmpty())
	{
		UE_LOG(LogHktMcpEditor, Error, TEXT("CreateDataAsset: Invalid asset path: %s"), *AssetPath);
		return false;
	}

	// DataAssetFactory 사용
	UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
	Factory->DataAssetClass = ParentClass;

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UDataAsset::StaticClass(), Factory);
	if (!NewAsset)
	{
		UE_LOG(LogHktMcpEditor, Error, TEXT("CreateDataAsset: Failed to create asset: %s"), *AssetPath);
		return false;
	}

	// 저장
	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName(), false);
	UE_LOG(LogHktMcpEditor, Log, TEXT("CreateDataAsset: Created %s (%s)"), *AssetPath, *ParentClassName);
	return true;
}

bool UHktMcpEditorSubsystem::ModifyAssetProperty(const FString& AssetPath, const FString& PropertyName, const FString& NewValue)
{
	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("Asset not found: %s"), *AssetPath);
		return false;
	}

	FProperty* Property = Asset->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("Property not found: %s"), *PropertyName);
		return false;
	}

	void* PropertyValue = Property->ContainerPtrToValuePtr<void>(Asset);
	const TCHAR* Ret = Property->ImportText_Direct(*NewValue, PropertyValue, Asset, EPropertyPortFlags::PPF_None);
	if (Ret == nullptr)
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("Failed to set property value"));
		return false;
	}

	Asset->MarkPackageDirty();
	return true;
}

bool UHktMcpEditorSubsystem::DeleteAsset(const FString& AssetPath)
{
	return UEditorAssetLibrary::DeleteAsset(AssetPath);
}

bool UHktMcpEditorSubsystem::DuplicateAsset(const FString& SourcePath, const FString& DestinationPath)
{
	return UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath) != nullptr;
}

// ==================== Level Tools ====================

TArray<FHktActorInfo> UHktMcpEditorSubsystem::ListActors(const FString& ClassFilter)
{
	TArray<FHktActorInfo> Result;
	
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return Result;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		
		if (!ClassFilter.IsEmpty() && !Actor->GetClass()->GetName().Contains(ClassFilter))
		{
			continue;
		}

		FHktActorInfo Info;
		Info.ActorName = Actor->GetName();
		Info.ActorLabel = Actor->GetActorLabel();
		Info.ActorClass = Actor->GetClass()->GetName();
		Info.Location = Actor->GetActorLocation();
		Info.Rotation = Actor->GetActorRotation();
		Info.Scale = Actor->GetActorScale3D();
		Info.ActorGuid = Actor->GetActorGuid().ToString();
		Result.Add(Info);
	}

	return Result;
}

FString UHktMcpEditorSubsystem::SpawnActor(const FString& BlueprintPath, FVector Location, FRotator Rotation, const FString& ActorLabel)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return TEXT("");
	}

	UObject* Asset = UEditorAssetLibrary::LoadAsset(BlueprintPath);
	UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
	if (!Blueprint)
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("Blueprint not found: %s"), *BlueprintPath);
		return TEXT("");
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Spawn Actor")));

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, Location, Rotation, SpawnParams);
	if (NewActor)
	{
		if (!ActorLabel.IsEmpty())
		{
			NewActor->SetActorLabel(ActorLabel);
		}
		return NewActor->GetName();
	}

	return TEXT("");
}

FString UHktMcpEditorSubsystem::SpawnActorByClass(const FString& ClassName, FVector Location, FRotator Rotation)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return TEXT("");
	}

	UClass* ActorClass = FindObject<UClass>(nullptr, *ClassName);
	if (!ActorClass || !ActorClass->IsChildOf(AActor::StaticClass()))
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("Actor class not found: %s"), *ClassName);
		return TEXT("");
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Spawn Actor")));

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
	if (NewActor)
	{
		return NewActor->GetName();
	}

	return TEXT("");
}

bool UHktMcpEditorSubsystem::ModifyActorTransform(const FString& ActorName, FVector NewLocation, FRotator NewRotation, FVector NewScale)
{
	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return false;
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Modify Actor Transform")));
	Actor->Modify();
	Actor->SetActorLocation(NewLocation);
	Actor->SetActorRotation(NewRotation);
	Actor->SetActorScale3D(NewScale);
	return true;
}

bool UHktMcpEditorSubsystem::ModifyActorProperty(const FString& ActorName, const FString& PropertyName, const FString& NewValue)
{
	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return false;
	}

	FProperty* Property = Actor->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		UE_LOG(LogHktMcpEditor, Warning, TEXT("Property not found: %s"), *PropertyName);
		return false;
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Modify Actor Property")));
	Actor->Modify();

	void* PropertyValue = Property->ContainerPtrToValuePtr<void>(Actor);
	const TCHAR* Ret = Property->ImportText_Direct(*NewValue, PropertyValue, Actor, EPropertyPortFlags::PPF_None);
	return Ret != nullptr;
}

bool UHktMcpEditorSubsystem::DeleteActor(const FString& ActorName)
{
	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return false;
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP Delete Actor")));
	return Actor->Destroy();
}

bool UHktMcpEditorSubsystem::SelectActor(const FString& ActorName)
{
	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return false;
	}

	GEditor->SelectNone(true, true);
	GEditor->SelectActor(Actor, true, true, true);
	return true;
}

TArray<FHktActorInfo> UHktMcpEditorSubsystem::GetSelectedActors()
{
	TArray<FHktActorInfo> Result;
	
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		if (Actor)
		{
			FHktActorInfo Info;
			Info.ActorName = Actor->GetName();
			Info.ActorLabel = Actor->GetActorLabel();
			Info.ActorClass = Actor->GetClass()->GetName();
			Info.Location = Actor->GetActorLocation();
			Info.Rotation = Actor->GetActorRotation();
			Info.Scale = Actor->GetActorScale3D();
			Info.ActorGuid = Actor->GetActorGuid().ToString();
			Result.Add(Info);
		}
	}

	return Result;
}

// ==================== Query Tools ====================

TArray<FString> UHktMcpEditorSubsystem::SearchClasses(const FString& SearchQuery, bool bBlueprintOnly)
{
	TArray<FString> Result;

	if (bBlueprintOnly)
	{
		// 블루프린트 클래스만 검색
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.PackagePaths.Add(FName("/Game"));
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());

		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssets(Filter, AssetDataList);

		for (const FAssetData& AssetData : AssetDataList)
		{
			if (AssetData.AssetName.ToString().Contains(SearchQuery))
			{
				Result.Add(AssetData.GetObjectPathString());
			}
		}
	}
	else
	{
		// 네이티브 클래스 검색
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (Class->GetName().Contains(SearchQuery))
			{
				Result.Add(Class->GetPathName());
			}
		}
	}

	return Result;
}

TArray<FHktPropertyInfo> UHktMcpEditorSubsystem::GetClassProperties(const FString& ClassName)
{
	TArray<FHktPropertyInfo> Result;

	UClass* Class = FindObject<UClass>(nullptr, *ClassName);
	if (!Class)
	{
		return Result;
	}

	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		FProperty* Property = *It;
		
		FHktPropertyInfo Info;
		Info.PropertyName = Property->GetName();
		Info.PropertyType = Property->GetCPPType();
		Info.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
		
		Result.Add(Info);
	}

	return Result;
}

FString UHktMcpEditorSubsystem::GetProjectStructure(const FString& RootPath)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FString> SubPaths;
	AssetRegistry.GetSubPaths(RootPath, SubPaths, false);

	TSharedRef<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(TEXT("root"), RootPath);

	TArray<TSharedPtr<FJsonValue>> PathArray;
	for (const FString& SubPath : SubPaths)
	{
		PathArray.Add(MakeShareable(new FJsonValueString(SubPath)));
	}
	JsonObject->SetArrayField(TEXT("folders"), PathArray);

	// 에셋 수
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByPath(FName(*RootPath), AssetDataList, false);
	JsonObject->SetNumberField(TEXT("asset_count"), AssetDataList.Num());

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(JsonObject, Writer);
	return Output;
}

FString UHktMcpEditorSubsystem::GetCurrentLevelInfo()
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return TEXT("{\"error\": \"No world\"}");
	}

	TSharedRef<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(TEXT("world_name"), World->GetName());
	JsonObject->SetStringField(TEXT("map_name"), World->GetMapName());
	
	// 레벨 정보
	if (World->PersistentLevel)
	{
		JsonObject->SetStringField(TEXT("level_name"), World->PersistentLevel->GetName());
	}

	// 액터 수
	int32 ActorCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		ActorCount++;
	}
	JsonObject->SetNumberField(TEXT("actor_count"), ActorCount);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(JsonObject, Writer);
	return Output;
}

// ==================== Editor Control ====================

bool UHktMcpEditorSubsystem::OpenLevel(const FString& LevelPath)
{
	return FEditorFileUtils::LoadMap(LevelPath, false, true);
}

bool UHktMcpEditorSubsystem::SaveCurrentLevel()
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return false;
	}

	return FEditorFileUtils::SaveCurrentLevel();
}

bool UHktMcpEditorSubsystem::CreateNewLevel(const FString& LevelPath)
{
	UWorld* NewWorld = GEditor->NewMap();
	if (NewWorld)
	{
		return FEditorFileUtils::SaveLevel(NewWorld->PersistentLevel, LevelPath);
	}
	return false;
}

bool UHktMcpEditorSubsystem::StartPIE()
{
	if (GUnrealEd && !GUnrealEd->IsPlayingSessionInEditor())
	{
		FRequestPlaySessionParams Params;
		Params.WorldType = EPlaySessionWorldType::PlayInEditor;
		GUnrealEd->RequestPlaySession(Params);
		return true;
	}
	return false;
}

bool UHktMcpEditorSubsystem::StopPIE()
{
	if (GUnrealEd && GUnrealEd->IsPlayingSessionInEditor())
	{
		GUnrealEd->RequestEndPlayMap();
		return true;
	}
	return false;
}

bool UHktMcpEditorSubsystem::IsPIERunning() const
{
	return GUnrealEd && GUnrealEd->IsPlayingSessionInEditor();
}

void UHktMcpEditorSubsystem::ExecuteEditorCommand(const FString& Command)
{
	GEngine->Exec(GetEditorWorld(), *Command);
}

// ==================== Utility ====================

bool UHktMcpEditorSubsystem::GetViewportCameraTransform(FVector& OutLocation, FRotator& OutRotation)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<SLevelViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveLevelViewport();
	
	if (ActiveLevelViewport.IsValid())
	{
		FEditorViewportClient& ViewportClient = ActiveLevelViewport->GetLevelViewportClient();
		OutLocation = ViewportClient.GetViewLocation();
		OutRotation = ViewportClient.GetViewRotation();
		return true;
	}
	return false;
}

bool UHktMcpEditorSubsystem::SetViewportCameraTransform(FVector Location, FRotator Rotation)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<SLevelViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveLevelViewport();
	
	if (ActiveLevelViewport.IsValid())
	{
		FEditorViewportClient& ViewportClient = ActiveLevelViewport->GetLevelViewportClient();
		ViewportClient.SetViewLocation(Location);
		ViewportClient.SetViewRotation(Rotation);
		return true;
	}
	return false;
}

void UHktMcpEditorSubsystem::ShowNotification(const FString& Message, float Duration)
{
	FNotificationInfo Info(FText::FromString(Message));
	Info.ExpireDuration = Duration;
	Info.bUseSuccessFailIcons = false;
	FSlateNotificationManager::Get().AddNotification(Info);
}

// ==================== Python Script Executor ====================

/** Helper to escape a string for embedding inside a Python triple-quoted string */
static FString EscapePythonTripleQuote(const FString& Input)
{
	// Replace backslashes first, then triple-quotes
	FString Out = Input.Replace(TEXT("\\"), TEXT("\\\\"));
	Out = Out.Replace(TEXT("\"\"\""), TEXT("\\\"\\\"\\\""));
	return Out;
}

FString UHktMcpEditorSubsystem::ExecutePythonScript(const FString& ScriptCode, float TimeoutSeconds)
{
	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin)
	{
		return TEXT("{\"success\":false,\"output\":\"\",\"error\":\"Python plugin not available\"}");
	}

	// Wrap user script to capture stdout/stderr and report results via unreal.log()
	// The wrapper stores results in a known format that we parse from LogOutput
	FString EscapedCode = EscapePythonTripleQuote(ScriptCode);

	FString WrappedScript = FString::Printf(TEXT(
		"import sys as _sys, io as _io, json as _json, traceback as _tb\n"
		"_hkt_stdout = _io.StringIO()\n"
		"_hkt_stderr = _io.StringIO()\n"
		"_hkt_old_stdout, _hkt_old_stderr = _sys.stdout, _sys.stderr\n"
		"_sys.stdout, _sys.stderr = _hkt_stdout, _hkt_stderr\n"
		"_hkt_success = True\n"
		"_hkt_error = ''\n"
		"try:\n"
		"    exec(compile(\"\"\"%s\"\"\", '<mcp_script>', 'exec'))\n"
		"except Exception as _e:\n"
		"    _hkt_success = False\n"
		"    _hkt_error = _tb.format_exc()\n"
		"finally:\n"
		"    _sys.stdout, _sys.stderr = _hkt_old_stdout, _hkt_old_stderr\n"
		"_hkt_out = _hkt_stdout.getvalue()\n"
		"_hkt_err_out = _hkt_stderr.getvalue()\n"
		"if _hkt_err_out and not _hkt_error:\n"
		"    _hkt_error = _hkt_err_out\n"
		"import unreal as _ue\n"
		"_ue.log('__HKT_PYRESULT__' + _json.dumps({'success': _hkt_success, 'output': _hkt_out, 'error': _hkt_error}))\n"
	), *EscapedCode);

	FPythonCommandEx PythonCommand;
	PythonCommand.Command = WrappedScript;
	PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
	PythonCommand.FileExecutionScope = EPythonFileExecutionScope::Public;

	PythonPlugin->ExecPythonCommandEx(PythonCommand);

	// Search LogOutput for our result marker
	FString ResultJson;
	const FString Marker = TEXT("__HKT_PYRESULT__");
	for (const FPythonLogOutputEntry& Entry : PythonCommand.LogOutput)
	{
		int32 MarkerIdx = Entry.Output.Find(Marker);
		if (MarkerIdx != INDEX_NONE)
		{
			ResultJson = Entry.Output.Mid(MarkerIdx + Marker.Len());
			break;
		}
	}

	if (!ResultJson.IsEmpty())
	{
		return ResultJson;
	}

	// Fallback: collect all log output
	FString AllOutput;
	for (const FPythonLogOutputEntry& Entry : PythonCommand.LogOutput)
	{
		if (!AllOutput.IsEmpty())
		{
			AllOutput += TEXT("\n");
		}
		AllOutput += Entry.Output;
	}

	TSharedRef<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), !AllOutput.Contains(TEXT("Error")));
	Result->SetStringField(TEXT("output"), AllOutput);
	Result->SetStringField(TEXT("error"), TEXT(""));

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Result, Writer);
	return Output;
}

// ==================== Helper Functions ====================

AActor* UHktMcpEditorSubsystem::FindActorByName(const FString& ActorName)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName)
		{
			return Actor;
		}
	}

	return nullptr;
}

UWorld* UHktMcpEditorSubsystem::GetEditorWorld()
{
	if (GEditor)
	{
		return GEditor->GetEditorWorldContext().World();
	}
	return nullptr;
}
